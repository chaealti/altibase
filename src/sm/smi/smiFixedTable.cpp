/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: smiFixedTable.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <idl.h>
#include <iduMemMgr.h>
#include <smiFixedTable.h>
#include <smuHash.h>
#include <smnfModule.h>
#include <smErrorCode.h>
#include <smiMisc.h>

smuHashBase        smiFixedTable::mTableHeaderHash;

// PROJ-2083 DUAL Table
static UChar      *gConstRecord      = NULL;

UInt smiFixedTable::genHashValueFunc(void *aKey)
{
    UInt   sValue = 0;
    SChar *sName;
    // ����� �����͸� �̿��Ͽ�, �̸� ������ ���
    sName = (SChar *)(*((void **)aKey));

    while( (*sName++) != 0)
    {
        sValue ^= (UInt)(*sName);
    }
    return sValue;
}

SInt smiFixedTable::compareFunc(void* aLhs, void* aRhs)
{

    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    return idlOS::strcmp((SChar *)(*(void **)aLhs), (SChar *)*((void **)(aRhs)) );

}


/*
 * QP���� ǥ���� Column�� ũ��� Offset�� ����Ѵ�.
 * ���� �� ����� QP�� ���� ����Ǿ�� �ϳ�,
 * A4�� Ư���� Meta ���̵� �����ؾ� �ϱ� ������ SM���� ����Ѵ�.
 * BUGBUG : ���� QP�� ����ϴ� ��İ� �������� �����ؾ� �Ѵ�.
 */
void   smiFixedTable::initColumnInformation(iduFixedTableDesc *aDesc)
{
    UInt                  sOffset;
    UInt                  sColCount;
    iduFixedTableColDesc *sColDesc;

    for (sColDesc = aDesc->mColDesc,
             sOffset = SMP_SLOT_HEADER_SIZE,
             sColCount = 0;
         sColDesc->mName != NULL;
         sColDesc++, sColCount++)
    {
        sColDesc->mTableName = aDesc->mTableName;
        sColDesc->mColOffset = sOffset;
        switch(IDU_FT_GET_TYPE(sColDesc->mDataType))
        {
            case IDU_FT_TYPE_VARCHAR:
            case IDU_FT_TYPE_CHAR:
                sColDesc->mColSize = idlOS::align8(sColDesc->mLength + 2);
                sOffset += sColDesc->mColSize;
                break;
            case IDU_FT_TYPE_UBIGINT:
            case IDU_FT_TYPE_USMALLINT:
            case IDU_FT_TYPE_UINTEGER:
            case IDU_FT_TYPE_BIGINT:
            case IDU_FT_TYPE_SMALLINT:
            case IDU_FT_TYPE_INTEGER:
            case IDU_FT_TYPE_DOUBLE:
                sColDesc->mColSize = idlOS::align8(sColDesc->mLength);
                sOffset += sColDesc->mColSize;
                break;
            default:
                IDE_CALLBACK_FATAL("Unknown Datatype");
        }
    }

    /* ------------------------------------------------
     *  Record�� Set�� ���������� ���� �� �ֱ� ������
     *  �� ���ڵ峢�� �����ͷ� ����Ǿ� �ִ�.
     *  �׷���, ���ڵ��� ó�� 8����Ʈ��
     *  next record pointer�� ����Ѵ�.
     * ----------------------------------------------*/

    aDesc->mSlotSize = (sOffset + idlOS::align8( ID_SIZEOF(void *)));
    aDesc->mColCount = sColCount;
}


// Fixed Table ����� �ʱ�ȭ �Ѵ�.
// initializeStatic()�� ���� �� �ѹ� ȣ���.
void   smiFixedTable::initFixedTableHeader(smiFixedTableHeader *aHeader,
                                           iduFixedTableDesc   *aDesc)
{
    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aDesc != NULL );
    
    // Slot Header
    SM_INIT_SCN( &(aHeader->mSlotHeader.mCreateSCN) );
    SM_SET_SCN_FREE_ROW( &(aHeader->mSlotHeader.mLimitSCN) );
    SMP_SLOT_SET_OFFSET( &(aHeader->mSlotHeader), 0 );

    // To Fix BUG-18186
    //   Fixed Table�� Cursor open���� Table Header��
    //   mIndexes�ʵ带 �ʱ�ȭ���� ���� ä�� ����
    idlOS::memset( &aHeader->mHeader, 0, ID_SIZEOF(aHeader->mHeader));

    // Table Header Init
    aHeader->mHeader.mFlag = SMI_TABLE_REPLICATION_DISABLE |
                             SMI_TABLE_LOCK_ESCALATION_DISABLE |
                             SMI_TABLE_FIXED;

    /* PROJ-2162 */
    aHeader->mHeader.mIsConsistent = ID_TRUE;

    // PROJ-2083 Dual Table
    aHeader->mHeader.mFlag &= ~SMI_TABLE_DUAL_MASK;

    // PROJ-1071 Parallel query
    // Default parallel degree is 1
    aHeader->mHeader.mParallelDegree = 1;

    if( idlOS::strcmp( aDesc->mTableName, "X$DUAL" ) == 0 )
    {
        aHeader->mHeader.mFlag |= SMI_TABLE_DUAL_TRUE;
    }
    else
    {
        aHeader->mHeader.mFlag |= SMI_TABLE_DUAL_FALSE;
    }

    aHeader->mHeader.mRuntimeInfo = NULL;

    /* BUG-42928 No Partition Lock */
    aHeader->mHeader.mReplacementLock = NULL;

    // Descriptor Assign
    aHeader->mDesc = aDesc;

}

// ��ϵ� ��� Fixed Table�� ���� ��ü�� �����ϰ�, Hash�� �����Ѵ�.

IDE_RC smiFixedTable::initializeStatic( SChar * aNameType )
{
    iduFixedTableDesc *sDesc;
    UInt               sColCount;
    SChar             *sTableName; // sys_tables_
    UInt               sTableNameLen;
    UInt               i;

    IDE_ASSERT( aNameType != NULL );
    
    if( idlOS::strncmp( aNameType, "X$", 2 ) == 0 )
    {
        iduFixedTable::setCallback(NULL, buildRecord, checkKeyRange );

        IDE_TEST( smuHash::initialize(&mTableHeaderHash,
                                      1,
                                      16,    /* aBucketCount */
                                      ID_SIZEOF(void *), /* aKeyLength */
                                      ID_FALSE,          /* aUseLatch */
                                      genHashValueFunc,
                                      compareFunc) != IDE_SUCCESS );
    }

    for (sDesc = iduFixedTable::getTableDescHeader(), sColCount = 0;
         sDesc != NULL;
         sDesc = sDesc->mNext, sColCount++)
    {
        /* TC/FIT/Limit/sm/smi/smiFixedTable_initializeStatic_malloc1.sql */
        IDU_FIT_POINT_RAISE( "smiFixedTable::initializeStatic::malloc1",
                              insufficient_memory );

        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMI,
                                   40, //  BUGBUG, DEFINED CONSTANT
                                   (void **)&sTableName) != IDE_SUCCESS,
                       insufficient_memory );

        sTableNameLen = idlOS::strlen( sDesc->mTableName );
        for( i = 0; i < sTableNameLen; i++ )
        {
            // PRJ-1678 : multi-byte character�� �� fixed table�� ������...
            sTableName[i] = idlOS::idlOS_toupper( sDesc->mTableName[i] );
        }
        sTableName[i] = '\0';

        if( idlOS::strncmp( aNameType, sTableName, 2 ) == 0 )
        {
            smiFixedTableHeader *sHeader;
            smiFixedTableNode   *sNode;

            /* TC/FIT/Limit/sm/smi/smiFixedTable_initializeStatic_malloc2.sql */
            IDU_FIT_POINT_RAISE( "smiFixedTable::initializeStatic::malloc2",
                                  insufficient_memory );

            // smcTable ��ü �Ҵ�
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMI,
                                       ID_SIZEOF(smiFixedTableHeader),
                                       (void **)&sHeader) != IDE_SUCCESS,
                           insufficient_memory );

            initFixedTableHeader(sHeader, sDesc);

            initColumnInformation(sDesc);

            /* TC/FIT/Limit/sm/smi/smiFixedTable_initializeStatic_malloc3.sql */
            IDU_FIT_POINT_RAISE( "smiFixedTable::initializeStatic::malloc3",
                                  insufficient_memory );

            // Node ��ü �Ҵ�
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMI,
                                       ID_SIZEOF(smiFixedTableNode),
                                       (void **)&sNode) != IDE_SUCCESS,
                           insufficient_memory );

            sDesc->mTableName = sNode->mName   = sTableName;

            for( i = 0; i < sDesc->mColCount; i++ )
            {
                sDesc->mColDesc[i].mTableName = sTableName;
            }

            sNode->mHeader = sHeader;

            IDE_TEST(smuHash::insertNode(&mTableHeaderHash,
                                         (void *)&(sNode->mName),
                                         (void *)sNode) != IDE_SUCCESS);

        }
        else
        {
            IDE_TEST( iduMemMgr::free(sTableName) != IDE_SUCCESS);

        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiFixedTable::destroyStatic()
{
    smiFixedTableNode   *sNode;

    IDE_TEST( smuHash::open(&mTableHeaderHash) != IDE_SUCCESS );

    IDE_TEST(smuHash::cutNode(&mTableHeaderHash,
                              (void **)&sNode) != IDE_SUCCESS);

    while( sNode != NULL)
    {
        // free fixed table name memory
        IDE_TEST( iduMemMgr::free(sNode->mName ) != IDE_SUCCESS);
        // free fixed table header slot
        IDE_TEST(iduMemMgr::free(sNode->mHeader) != IDE_SUCCESS);
        // free hash node
        IDE_TEST(iduMemMgr::free(sNode) != IDE_SUCCESS);
        IDE_TEST(smuHash::cutNode(&mTableHeaderHash,
                                      (void **)&sNode) != IDE_SUCCESS);
    }//while

    IDE_TEST( smuHash::close(&mTableHeaderHash) != IDE_SUCCESS );
    IDE_TEST( smuHash::destroy(&mTableHeaderHash) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*
IDE_RC smiFixedTable::initializeTables(smiStartupPhase aPhase)
{
    iduFixedTableDesc *sDesc;

    for (sDesc = iduFixedTable::getTableDescHeader();
         sDesc != NULL;
         sDesc = sDesc->mNext)
    {
        if (sDesc->mEnablePhase == (iduStartupPhase)aPhase)
        {
            // �ʱ�ȭ�� ���� ����� �� �ֵ��� �Ѵ�.
            // mHeader �ʱ�ȭ?..
        }
    }

    return IDE_SUCCESS;
}
*/
IDE_RC smiFixedTable::findTable(SChar *aName, void **aHeader)
{
    smiFixedTableNode *sNode;

    IDE_ASSERT( aName != NULL );
    IDE_ASSERT( aHeader != NULL );
    
    IDE_TEST(smuHash::findNode(&mTableHeaderHash,
                               (void *)(&aName),
                               (void **)&sNode) != IDE_SUCCESS);

    if (sNode == NULL)
    {
        *aHeader = NULL;
    }
    else
    {
        *aHeader = sNode->mHeader;
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

extern smiStartupPhase smiGetStartupPhase();

idBool smiFixedTable::canUse(void *aHeader)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );

    sHeader = (smiFixedTableHeader *)aHeader;

    if (smiGetStartupPhase() < (smiStartupPhase)sHeader->mDesc->mEnablePhase)
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}


UInt   smiFixedTable::getSlotSize(void *aHeader)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );

    sHeader = (smiFixedTableHeader *)((SChar *)aHeader -  SMP_SLOT_HEADER_SIZE);

    return sHeader->mDesc->mSlotSize;
}


iduFixedTableBuildFunc smiFixedTable::getBuildFunc(void *aHeader)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );

    sHeader = (smiFixedTableHeader *)((SChar *)aHeader -  SMP_SLOT_HEADER_SIZE);

    return sHeader->mDesc->mBuildFunc;
}

/* ------------------------------------------------
 *  �� �Լ��� CHAR Ÿ���� ��ȯ�ÿ�
 *  �Էµ� ��Ʈ���� ���̰� Null-Teminate��
 *  �ƴ� �� �ֱ� ������ �� ������ �˻縦
 *  ��� CHAR Ÿ���� ���̸�ŭ�� �˻��Ͽ�, ���ѷ�����
 *  �����ϴµ� �ִ�.
 * ----------------------------------------------*/
static UInt getCharLength(SChar *aChar, UInt aExpectedLength)
{
    UInt i;

    IDE_ASSERT( aChar != NULL );
    
    for (i = 0; i < aExpectedLength; i++)
    {
        if (aChar[i] == 0)
        {
            return i;
        }
    }
    return aExpectedLength;
}

SShort smiFixedTable::mSizeOfDataType[] =
{
    /* IDU_FT_TYPE_CHAR       (0x0000)*/ 0,
    /* IDU_FT_TYPE_BIGINT     (0x0001)*/  IDU_FT_SIZEOF_BIGINT,
    /* IDU_FT_TYPE_SMALLINT   (0x0002)*/  IDU_FT_SIZEOF_SMALLINT,
    /* IDU_FT_TYPE_INTEGER    (0x0003)*/  IDU_FT_SIZEOF_INTEGER,
    /* IDU_FT_TYPE_DOUBLE     (0x0004)*/  IDU_FT_SIZEOF_DOUBLE,
    /* IDU_FT_TYPE_UBIGINT    (0x0005)*/  IDU_FT_SIZEOF_BIGINT,
    /* IDU_FT_TYPE_USMALLINT  (0x0006)*/  IDU_FT_SIZEOF_SMALLINT,
    /* IDU_FT_TYPE_UINTEGER   (0x0007)*/  IDU_FT_SIZEOF_INTEGER
};

void smiFixedTable::buildOneColumn( iduFixedTableDataType          aDataType,
                                    UChar                        * aObj,
                                    void                         * aSourcePtr,
                                    UInt                           aSourceSize,
                                    void                         * aTargetPtr,
                                    iduFixedTableConvertCallback   aConvCallback )
{
    UInt    sStrLen;
    UChar * sColPos;
    UInt    sTargetSize; /* �ش� ����Ÿ Ÿ�Կ� �´� ����� �޸� ũ�� */

    switch (IDU_FT_GET_TYPE(aDataType))
    {
        case IDU_FT_TYPE_UBIGINT:
        case IDU_FT_TYPE_USMALLINT:
        case IDU_FT_TYPE_UINTEGER:
        case IDU_FT_TYPE_BIGINT:
        case IDU_FT_TYPE_SMALLINT:
        case IDU_FT_TYPE_INTEGER:
        case IDU_FT_TYPE_DOUBLE:
        {
            /*
             *  ����ڿ� ���� ��� Ÿ�� ���� ��� BIGINT(8)�� ���ǵǾ�����
             *  ���� ���� ��ȯ�ϴ� ������ Ÿ���� 8����Ʈ�� �ƴ� �� �ִ�.
             *  typedef enum ���� ��� �� ũ�⸦ �� �� ���⿡ �̷��� ũ����
             *  mismatch�� �߻��� �� �ִ�.
             *
             *  �׷� ������ ���ΰ��� ����Ÿ ũ�⸦ ���Ͽ�, �����ϰ�
             *  �ڵ������� ��ȯ�� ���ֵ��� �Ѵ�.
             *
             *   sObj                         aRowBuf
             *  +---------+                  +-----------------+
             *  |11223344 |sObject(4)   ===> |1122334455667788 | (8 byte)
             *  |---------|                  |-----------------|
             *  +---------+                  +-----------------+
             */

            sTargetSize = mSizeOfDataType[IDU_FT_GET_TYPE(aDataType)];
            if ( aConvCallback != NULL )
            {
                aConvCallback( aObj,
                               aSourcePtr,
                               (UChar *)aTargetPtr,
                               sTargetSize );
            }
            else
            {
                /*
                 * 4���� ���ڸ� �̿��ؼ� ��ȯ ���.
                 */
                if (sTargetSize == aSourceSize)
                {
                    idlOS::memcpy(aTargetPtr, aSourcePtr, sTargetSize);
                }
                else
                {
                    /*
                     *  Source�� ū ���� �߻��� �� ����.
                     *  ���� �ִٸ�, Fixed Table�� �߸� ������ ����.
                     *  ��ȯ ���� : 2->4, 2->8, 4->8 , �׿ܴ� ����.
                     */
                    IDE_ASSERT(sTargetSize >= aSourceSize);

                    if (sTargetSize == 4)
                    {
                        /* Short->Integer 2->4*/
                        UInt sIntBuf;

                        sIntBuf = (UInt)(*((UShort *)aSourcePtr));
                        idlOS::memcpy(aTargetPtr, &sIntBuf, sTargetSize);
                    }
                    else
                    {
                        /* sTargetSize == 8 */
                        if (aSourceSize == 2)
                        {
                            /* 2->8 */
                            ULong sLongBuf;

                            sLongBuf = (ULong)(*((UShort *)aSourcePtr));
                            idlOS::memcpy(aTargetPtr, &sLongBuf, sTargetSize);
                        }
                        else
                        {
                            /* 4->8*/
                            /* 2->8 */
                            ULong sLongBuf;

                            sLongBuf = (ULong)(*((UInt *)aSourcePtr));
                            idlOS::memcpy(aTargetPtr, &sLongBuf, sTargetSize);
                        }
                    }
                }
            }
        }
        break;

        case IDU_FT_TYPE_CHAR:
        {
            sColPos = ( UChar * )aTargetPtr;
            *(UShort *)sColPos = aSourceSize;
            sColPos += ID_SIZEOF(UShort);

            if ( aConvCallback != NULL )
            {
                // �ش� Object�� CHAR�� ��ȯ�϶�!
                sStrLen = aConvCallback( aObj,
                                         aSourcePtr,
                                         sColPos,
                                         aSourceSize );
            }
            else
            {

                sStrLen = getCharLength((SChar *)aSourcePtr, aSourceSize );

                idlOS::memcpy(sColPos, aSourcePtr, sStrLen);
            }
            // Fill ' '
            idlOS::memset(sColPos + sStrLen ,
                          ' ',
                          aSourceSize - sStrLen);

            break;
        }

        case IDU_FT_TYPE_VARCHAR:
        {
            sColPos = ( UChar * )aTargetPtr;

            if ( aConvCallback != NULL )
            {
                // �ش� Object�� CHAR�� ��ȯ�϶�!
                sStrLen = aConvCallback( aObj,
                                         aSourcePtr,
                                         (sColPos + ID_SIZEOF(UShort)),
                                         aSourceSize );
                *(UShort *)sColPos = sStrLen;
            }
            else
            {
                sStrLen = getCharLength((SChar *)aSourcePtr, aSourceSize );

                // Set String Size
                *(UShort *)sColPos = sStrLen;
                sColPos += ID_SIZEOF(UShort);

                // Fill Value with 0
                idlOS::memcpy(sColPos, aSourcePtr, sStrLen);
                idlOS::memset(sColPos + sStrLen ,
                              0,
                              aSourceSize - sStrLen);
            }

            break;
        }
        default:
            IDE_CALLBACK_FATAL("Unknown Datatype");
    }
}

IDE_RC smiFixedTable::buildOneRecord(iduFixedTableDesc *aTabDesc,
                                     UChar             *aRowBuf,
                                     UChar             *aObj)
{
    iduFixedTableColDesc *sColDesc;
    UChar                *sObject;

    IDE_ASSERT( aTabDesc != NULL );
    IDE_ASSERT( aRowBuf != NULL );
    IDE_ASSERT( aObj != NULL );
    
    for (sColDesc         = aTabDesc->mColDesc;
         sColDesc->mName != NULL;
         sColDesc++ )
    {
        sObject = (UChar *)aObj + sColDesc->mOffset;

        if (IDU_FT_IS_POINTER(sColDesc->mDataType))
        {
            // de-reference if pointer type..
            sObject = (UChar *)(*((void **)sObject));
        }

        switch(IDU_FT_GET_TYPE(sColDesc->mDataType))
        {
            case IDU_FT_TYPE_UBIGINT:
            case IDU_FT_TYPE_USMALLINT:
            case IDU_FT_TYPE_UINTEGER:
            case IDU_FT_TYPE_BIGINT:
            case IDU_FT_TYPE_SMALLINT:
            case IDU_FT_TYPE_INTEGER:
            case IDU_FT_TYPE_DOUBLE:
            {
                ULong sNullBuf[4] = { 0, 0, 0, 0 };
                UInt  sSourceSize; /* ����ڰ� ������ �޸� ũ�� */
                void *sTargetPtr;
                void *sSourcePtr;

                /*
                 *  ����ڿ� ���� ��� Ÿ�� ���� ��� BIGINT(8)�� ���ǵǾ�����
                 *  ���� ���� ��ȯ�ϴ� ������ Ÿ���� 8����Ʈ�� �ƴ� �� �ִ�.
                 *  typedef enum ���� ��� �� ũ�⸦ �� �� ���⿡ �̷��� ũ����
                 *  mismatch�� �߻��� �� �ִ�.
                 *
                 *  �׷� ������ ���ΰ��� ����Ÿ ũ�⸦ ���Ͽ�, �����ϰ�
                 *  �ڵ������� ��ȯ�� ���ֵ��� �Ѵ�.
                 *
                 *   sObj                         aRowBuf
                 *  +---------+                  +-----------------+
                 *  |11223344 |sObject(4)   ===> |1122334455667788 | (8 byte)
                 *  |---------|                  |-----------------|
                 *  +---------+                  +-----------------+
                 */

                sSourceSize = sColDesc->mLength;
                sTargetPtr  = aRowBuf + sColDesc->mColOffset;
                if (sObject == NULL)
                {
                    // 32 byte�� 0���� ä���, memcpy�� ���� ����Ѵ�.
                    // ���������� 32byte ������ ������ Ÿ���� ���� ������
                    // �����ϴٰ� �Ǵܵȴ�.
                    IDE_DASSERT(ID_SIZEOF(sNullBuf) >= sColDesc->mLength);
                    sSourcePtr  = sNullBuf;
                }
                else
                {
                    sSourcePtr  = sObject;
                }

                buildOneColumn( sColDesc->mDataType,
                                aObj,
                                sSourcePtr,
                                sSourceSize,
                                sTargetPtr,
                                sColDesc->mConvCallback );
                break;
            }
            case IDU_FT_TYPE_CHAR:
            {
                UChar *sColPos;

                if (sColDesc->mConvCallback != NULL)
                {
                    sColPos = aRowBuf + sColDesc->mColOffset;
                }
                else
                {
                    if (sObject == NULL)
                    {
                        sObject = (UChar *)"";
                    }

                    sColPos = aRowBuf + sColDesc->mColOffset;
                }

                buildOneColumn( sColDesc->mDataType,
                                aObj,
                                sObject,
                                sColDesc->mLength,
                                (void *)sColPos,
                                sColDesc->mConvCallback );
                break;
            }

            case IDU_FT_TYPE_VARCHAR:
            {
                UChar *sColPos;

                if (sColDesc->mConvCallback != NULL)
                {
                    sColPos = aRowBuf + sColDesc->mColOffset;
                }
                else
                {
                    if (sObject == NULL)
                    {
                        sObject = (UChar *)"";
                    }

                    sColPos = aRowBuf + sColDesc->mColOffset;
                }

                buildOneColumn( sColDesc->mDataType,
                                aObj,
                                sObject,
                                sColDesc->mLength,
                                sColPos,
                                sColDesc->mConvCallback );
                break;
            }
            default:
                IDE_CALLBACK_FATAL("Unknown Datatype");
        }
    }

    return IDE_SUCCESS;
}

IDE_RC smiFixedTable::buildRecord( void                * aHeader,
                                   iduFixedTableMemory * aMemory,
                                   void                * aObj )
{
    smiFixedTableHeader  *sHeader;
    iduFixedTableDesc    *sTabDesc;
    UChar                *sRecord;
    idBool                sFilterResult;
    idBool                sLimitResult;
    smiFixedTableRecord  *sCurrRec;
    smiFixedTableRecord  *sBeforeRec;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );
    IDE_ERROR( aObj    != NULL );

    sHeader = (smiFixedTableHeader *)((SChar *)aHeader -  SMP_SLOT_HEADER_SIZE);
    sTabDesc = sHeader->mDesc;
    sFilterResult = ID_FALSE;
    sLimitResult  = ID_FALSE;

    if ( ( (((smcTableHeader*)aHeader)->mFlag & SMI_TABLE_DUAL_MASK)
         == SMI_TABLE_DUAL_TRUE ) &&
         ( aMemory->useExternalMemory() == ID_FALSE ) )
    {
        if( gConstRecord == NULL )
        {
            IDE_TEST(aMemory->allocateRecord(sTabDesc->mSlotSize, (void **)&gConstRecord)
                     != IDE_SUCCESS);

            // �ش� ���ڵ带 �޸𸮿� �����ؾ� �Ѵ�.
            (void)buildOneRecord(
                sTabDesc,
                gConstRecord + idlOS::align8(ID_SIZEOF(void *)), // ���� ���ڵ� ��ġ��
                (UChar *)aObj);
        }
        else
        {
            IDE_TEST( aMemory->initRecord((void **)&gConstRecord)
                      != IDE_SUCCESS );
        }

        sRecord = gConstRecord;
    }
    else
    {
        IDE_TEST(aMemory->allocateRecord(sTabDesc->mSlotSize, (void **)&sRecord)
                 != IDE_SUCCESS);
        
        // �ش� ���ڵ带 �޸𸮿� �����ؾ� �Ѵ�.
        (void)buildOneRecord(
            sTabDesc,
            sRecord + idlOS::align8(ID_SIZEOF(void *)), // ���� ���ڵ� ��ġ��
            (UChar *)aObj);
    }
    
    /*
     * 1. Filter ó��
     */
    IDE_DASSERT( aMemory->getContext() != NULL );

    /* BUG-42639 Monitoring query */
    if ( aMemory->useExternalMemory() == ID_FALSE )
    {
        IDE_TEST( smnfCheckFilter( aMemory->getContext(),
                                   sRecord,
                                   &sFilterResult )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( checkFilter( aMemory->getContext(),
                               sRecord,
                               &sFilterResult )
                  != IDE_SUCCESS );
    }

    if( sFilterResult == ID_TRUE )
    {
        /* BUG-42639 Monitoring query */
        if ( aMemory->useExternalMemory() == ID_FALSE )
        {
            /*
             * 2. Limit ó��
             */
            smnfCheckLimitAndMovePos( aMemory->getContext(),
                                      &sLimitResult,
                                      ID_TRUE ); // aIsMovePos
        }
        else
        {
            checkLimitAndMovePos( aMemory->getContext(),
                                  &sLimitResult,
                                  ID_TRUE ); // aIsMovePos
        }
    }
    
    if( (sFilterResult == ID_TRUE) && (sLimitResult == ID_TRUE) )
    {
        sCurrRec   = (smiFixedTableRecord *)aMemory->getCurrRecord();
        sBeforeRec = (smiFixedTableRecord *)aMemory->getBeforeRecord();

        if (sBeforeRec != NULL)
        {
            sBeforeRec->mNext = sCurrRec;
        }
    }
    else
    {
        /* BUG-42639 Monitoring query */
        if ( aMemory->useExternalMemory() == ID_FALSE )
        {
            if ( (((smcTableHeader*)aHeader)->mFlag & SMI_TABLE_DUAL_MASK)
                == SMI_TABLE_DUAL_FALSE )
            {
                aMemory->freeRecord(); // ��� �¿� ���Ե��� �ʴ� ���ڵ�� free�Ѵ�.
            }
            else
            {
                aMemory->resetRecord();
            }
        }
        else
        {
            aMemory->freeRecord(); // ��� �¿� ���Ե��� �ʴ� ���ڵ�� free�Ѵ�.
        }
    }
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

//  IDE_RC smiFixedTable::allocRecordBuffer(void  *aHeader,
//                                          UInt   aRecordCount,
//                                          void **aBuffer)
//  {
//  //      IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMI,
//  //                                 1,
//  //                                 getSlotSize(aHeader) *  aRecordCount,
//  //                                 aBuffer)
//  //               != IDE_SUCCESS);

//      return IDE_SUCCESS;
//      IDE_EXCEPTION_END;
//      return IDE_FAILURE;
//  }


//IDE_RC smiFixedTable::freeRecordBuffer(void */*aBuffer*/)
//{
//      IDE_TEST(iduMemMgr::free(aBuffer) != IDE_SUCCESS);

//    return IDE_SUCCESS;
//}


UInt smiFixedTable::getColumnCount(void *aHeader)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    return sHeader->mDesc->mColCount;
}

SChar *smiFixedTable::getColumnName(void *aHeader, UInt aNum)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    return sHeader->mDesc->mColDesc[aNum].mName;
}

UInt smiFixedTable::getColumnOffset(void *aHeader, UInt aNum)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    return sHeader->mDesc->mColDesc[aNum].mColOffset;
}

UInt smiFixedTable::getColumnLength(void *aHeader, UInt aNum)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    return sHeader->mDesc->mColDesc[aNum].mLength;
}

iduFixedTableDataType smiFixedTable::getColumnType(void *aHeader, UInt aNum)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    return IDU_FT_GET_TYPE(sHeader->mDesc->mColDesc[aNum].mDataType);
}

void   smiFixedTable::setNullRow(void *aHeader, void *aNullRow)
{
    smiFixedTableHeader *sHeader;

    IDE_ASSERT( aHeader != NULL );
    
    sHeader = (smiFixedTableHeader *)aHeader;

    sHeader->mNullRow = aNullRow;
}

/*
SChar *smiFixedTable::getTableName(void *aHeader)
{
    smiFixedTableHeader *sHeader;

    sHeader = (smiFixedTableHeader *)aHeader;

    return sHeader->mDesc->mTableName;
}
*/

UInt smiFixedTable::convertSCN(void        * /*aBaseObj*/,
                                 void        *aMember,
                                 UChar       *aBuf,
                                 UInt         aBufSize)
{
    SChar  sBuf[30];
    UInt   sSize;
    ULong  sInfiniteMask;
    ULong  sSCNValue;
    smSCN *sOrgSCN;

    sOrgSCN = (smSCN *)aMember;
    sSCNValue = SM_SCN_TO_LONG( *sOrgSCN );

    if (SM_SCN_IS_NOT_INFINITE(*sOrgSCN))
    {
        sSize = idlOS::snprintf(sBuf, ID_SIZEOF(sBuf),
                                "%19"ID_UINT64_FMT,
                                sSCNValue);
    }
    /* BUG-16564:
     * SCN�� INFINITE�� ��� ���� 0x8000000000000000�� �� �ִ�.
     * �� ���� query processor ���忡���� BIGINT�� NULL ���� �ش��Ѵ�.
     * ����, fixed table���� SCN �� ����� ���� BIGINT Ÿ���� ����� ���
     * INFINITE�� NULL�� ó���Ǿ������ �ȴ�.
     * ��������� iSQL���� INFINITE�� SCN ���� SELECT�غ���,
     * ȭ�鿡�� �ƹ��� ���� ��µ��� �ʰ� �ȴ�.
     * NULL�� SCN�� INFINITE�� �����϶�� ����ڿ��� ������ ���� ������
     * �̴� ����� ģȭ������ �����Ƿ�,
     * SCN�� INFINITE�� ��� SCN �÷� ���� INFINITE(n) ���·� ����ϵ���
     * �ڵ带 �����Ѵ�.
     * �� ��, n�� SCN���� MSB 1bit(INFINITE bit)�� �� SCN ���̴�.
     *
     * PROJ-1381 Fetch Across Commits
     * SlotHeader Refactoring�� ���ؼ� infinite SCN��
     * [4bytes infinite SCN + 4bytes TID]�� �����ߴ�.
     * ���� 4Bytes ��ŭ shift�ؼ� ������ �����ϰ� infinite SCN�� ����Ѵ�.*/
    else
    {
        sInfiniteMask = SM_SCN_INFINITE;
        sSize = idlOS::snprintf(sBuf, ID_SIZEOF(sBuf),
                                "INFINITE(%19"ID_UINT64_FMT")",
                                (sSCNValue & (~sInfiniteMask))>>32);
    }

    if (aBufSize < sSize)
    {
        sSize = aBufSize;
    }

    idlOS::memset(aBuf, 0, aBufSize);
    idlOS::memcpy(aBuf, sBuf, sSize);

    return sSize;
}

UInt smiFixedTable::convertTIMESTAMP(void        * /*aBaseObj*/,
                                       void        *aMember,
                                       UChar       *aBuf,
                                       UInt         aBufSize)
{
    struct timeval *sTime;
    UInt            sSize;

    sTime = (struct timeval *)aMember;

    idlOS::memset(aBuf, 0, aBufSize);

    sSize = idlOS::snprintf((SChar *)aBuf, aBufSize,
                            "[%"ID_UINT64_FMT"][%"ID_UINT64_FMT"]",
                           (ULong)sTime->tv_sec,
                           (ULong)sTime->tv_usec);

    return sSize;
}

/* �ð���½� usec �պκ��� 0���� ä���� �׻� 6�ڸ��� ��� �Ѵ�. */
UInt smiFixedTable::convertAlignedTIMESTAMP( void       * /*aBaseObj*/,
                                             void       * aMember,
                                             UChar      * aBuf,
                                             UInt         aBufSize )
{
    struct timeval * sTime;
    UInt             sSize;

    sTime = (struct timeval *)aMember;

    idlOS::memset(aBuf, 0, aBufSize);

    sSize = idlOS::snprintf( (SChar *)aBuf, aBufSize,
                             "[%"ID_UINT64_FMT"][%06"ID_UINT64_FMT"]",
                             (ULong)sTime->tv_sec,
                             (ULong)sTime->tv_usec );

    return sSize;
}

UInt smiFixedTable::convertShardPinToString( void       * /*aBaseObj*/,
                                             void       * aMember,
                                             UChar      * aBuf,
                                             UInt         aBufSize )
{
    smiShardPin * sShardPin; 
    UInt          sSize;

    sShardPin = (smiShardPin*)aMember;
    
    sSize = idlOS::snprintf( (SChar *)aBuf, aBufSize,
                             SMI_SHARD_PIN_FORMAT_STR,
                             SMI_SHARD_PIN_FORMAT_ARG( *sShardPin ) );

    return sSize;
}

/* BUG-42639 Monitoring query */
IDE_RC smiFixedTable::build( idvSQL               * aStatistics,
                             void                 * aHeader,
                             void                 * aDumpObject,
                             iduFixedTableMemory  * aFixedMemory,
                             UChar               ** aRecRecord,
                             UChar               ** aTraversePtr )
{
    smcTableHeader         * sTable;
    iduFixedTableBuildFunc   sBuildFunc;
    smiFixedTableRecord    * sEndRecord;

    IDE_ERROR( aHeader      != NULL );
    IDE_ERROR( aFixedMemory != NULL );
    IDE_ERROR( aRecRecord   != NULL );
    IDE_ERROR( aTraversePtr != NULL );

    sTable = (smcTableHeader *)((UChar*)aHeader + SMP_SLOT_HEADER_SIZE);
    sBuildFunc = smiFixedTable::getBuildFunc( sTable );

    // PROJ-1618
    IDE_TEST( sBuildFunc( aStatistics,
                          sTable,
                          aDumpObject,
                          aFixedMemory )
              != IDE_SUCCESS );

    *aRecRecord   = aFixedMemory->getBeginRecord();
    *aTraversePtr = *aRecRecord;

    // Check End of Record
    sEndRecord = (smiFixedTableRecord *)aFixedMemory->getCurrRecord();

    if ( sEndRecord != NULL )
    {
        sEndRecord->mNext = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aStatistics != NULL )
    {
        IDV_SQL_ADD( aStatistics, mMemCursorSeqScan, 1 );
        IDV_SESS_ADD( aStatistics->mSess, IDV_STAT_INDEX_MEM_CURSOR_SEQ_SCAN, 1 );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-42639 Monitoring query */
IDE_RC smiFixedTable::checkFilter( void   * aProperty,
                                   void   * aRecord,
                                   idBool * aResult )
{
    smiFixedTableProperties * sProperty;

    IDE_ERROR( aProperty != NULL );
    IDE_ERROR( aRecord   != NULL );
    IDE_ERROR( aResult   != NULL );

    sProperty = ( smiFixedTableProperties * )aProperty;

    IDE_TEST( sProperty->mFilter->callback( aResult,
                                            getRecordPtr((UChar *)aRecord),
                                            NULL,
                                            0,
                                            SC_NULL_GRID,
                                            sProperty->mFilter->data )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-42639 Monitoring query */
void * smiFixedTable::getRecordPtr( UChar * aCurRec )
{
    return smnfGetRecordPtr( ( void * )aCurRec );
}

/* BUG-42639 Monitoring query */
void smiFixedTable::checkLimitAndMovePos( void   * aProperty,
                                          idBool * aLimitResult,
                                          idBool   aIsMovePos )
{
    smiFixedTableProperties * sProperty;
    idBool                    sFirstLimitResult;
    idBool                    sLastLimitResult;

    sProperty = ( smiFixedTableProperties * )aProperty;

    *aLimitResult = ID_FALSE;
    sFirstLimitResult = ID_FALSE;
    sLastLimitResult = ID_FALSE;

    if ( sProperty->mFirstReadRecordPos <= sProperty->mReadRecordPos )
    {
        sFirstLimitResult = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( sProperty->mFirstReadRecordPos + sProperty->mReadRecordCount ) >
         sProperty->mReadRecordPos )
    {
        sLastLimitResult = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sFirstLimitResult == ID_FALSE ) // �о�� �� ��ġ���� �տ� �ִ� ���.
    {
        /*
         *         first           last
         * ----------*---------------*---------->
         *      ^
         */
    }
    else
    {
        if ( sLastLimitResult == ID_TRUE ) // �о�� �� ���� �ȿ� �ִ� ���.
        {
            /*
             *         first           last
             * ----------*---------------*---------->
             *                 ^
             */
            *aLimitResult = ID_TRUE;
        }
        else // �о�� �� ��ġ���� �ڿ� �ִ� ���.
        {
            /*
             *         first           last
             * ----------*---------------*---------->
             *                                 ^
             */
        }
    }

    if ( aIsMovePos == ID_TRUE )
    {
        sProperty->mReadRecordPos++;
    }
    else
    {
        /* Nothing to do */
    }
}

/* BUG-42639 Monitoring query */
IDE_RC smiFixedTable::checkLastLimit( void   * aProperty,
                                      idBool * aIsLastLimitResult )
{
    smiFixedTableProperties * sProperty;

    IDE_ERROR( aProperty != NULL );
    IDE_ERROR( aIsLastLimitResult   != NULL );

    sProperty = ( smiFixedTableProperties * )aProperty;

    // Session Event�� �˻��Ѵ�.
    IDE_TEST( iduCheckSessionEvent( sProperty->mStatistics ) != IDE_SUCCESS );

    if ( ( sProperty->mFirstReadRecordPos + sProperty->mReadRecordCount ) >
           sProperty->mReadRecordPos )
    {
        *aIsLastLimitResult = ID_FALSE;
    }
    else
    {
        // �о�� �� ��ġ���� �ڿ� �ִ� ���.
        /*
         *         first           last
         * ----------*---------------*---------->
         *                                 ^
         */
        *aIsLastLimitResult = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-42639 Monitoring query */
idBool smiFixedTable::useTrans( void *aHeader )
{
    smiFixedTableHeader * sHeader;
    idBool                sIsUse;

    IDE_ASSERT( aHeader != NULL );

    sHeader = ( smiFixedTableHeader * )aHeader;

    if ( sHeader->mDesc->mUseTrans == IDU_FT_DESC_TRANS_NOT_USE )
    {
        sIsUse = ID_FALSE;
    }
    else
    {
        sIsUse = ID_TRUE;
    }

    return sIsUse;
}

/**
 *  BUG-43006 Fixed Table Indexing Filter
 *
 *  FixedTable�� ����� Key Range Filter�� �˻��ؼ�
 *  �ش� �÷��� �ش�Ǵ��� �ʵǴ��� �˻��Ѵ�.
 */
idBool smiFixedTable::checkKeyRange( iduFixedTableMemory   * aMemory,
                                     iduFixedTableColDesc  * aColDesc,
                                     void                 ** aObjects )
{
    idBool                    sResult;
    smiFixedTableProperties * sProperty;
    UChar                     sRowBuf[IDP_MAX_VALUE_LEN];
    UInt                      sOffset1;
    UInt                      sOffset2;
    UChar                   * sObject;
    ULong                     sNullBuf[4];
    void                    * sSourcePtr;
    void                    * sObj;
    iduFixedTableColDesc    * sColDesc;
    iduFixedTableColDesc    * sColIndexDesc;
    UInt                      sCount;
    smiRange                * sRange;

    sResult = ID_TRUE;

    IDE_TEST_CONT( aMemory->useExternalMemory() == ID_FALSE, normal_exit );

    sProperty = ( smiFixedTableProperties * )aMemory->getContext();

    IDE_TEST_CONT( sProperty->mKeyRange == smiGetDefaultKeyRange(), normal_exit );

    if ( ( sProperty->mMaxColumn->offset == 0 ) &&
         ( sProperty->mMaxColumn->size == 0 ) )
    {
        sResult = ID_FALSE;
        IDE_CONT( normal_exit );
    }
    else
    {
        /* Nothing to do */
    }

    for ( sColDesc = aColDesc, sCount = 0, sColIndexDesc = NULL;
          sColDesc->mName != NULL;
          sColDesc++ )
    {
        if ( IDU_FT_IS_COLUMN_INDEX( sColDesc->mDataType ) )
        {
            if ( sColDesc->mColOffset == sProperty->mMaxColumn->offset )
            {
                sColIndexDesc = sColDesc;
                break;
            }
            else
            {
                sCount++;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_TEST_CONT( sColIndexDesc == NULL, normal_exit );

    IDE_TEST_CONT( ( sColIndexDesc->mLength > IDP_MAX_VALUE_LEN ) ||
                   ( sColIndexDesc->mConvCallback != NULL ), normal_exit );

    sObj = aObjects[sCount];

    if ( IDU_FT_IS_POINTER( sColIndexDesc->mDataType ) )
    {
        // de-reference if pointer type..
        sObject = (UChar *)(*((void **)sObj));
    }
    else
    {
        sObject = (UChar *)sObj;
    }

    if ( ( sObject == NULL ) &&
         ( IDU_FT_GET_TYPE( sColIndexDesc->mDataType ) != IDU_FT_TYPE_VARCHAR ) &&
         ( IDU_FT_GET_TYPE( sColIndexDesc->mDataType ) != IDU_FT_TYPE_CHAR ) )
    {
        sNullBuf[0] = 0;
        sNullBuf[1] = 0;
        sNullBuf[2] = 0;
        sNullBuf[3] = 0;

        // 32 byte�� 0���� ä���, memcpy�� ���� ����Ѵ�.
        // ���������� 32byte ������ ������ Ÿ���� ���� ������
        // �����ϴٰ� �Ǵܵȴ�.
        IDE_DASSERT(ID_SIZEOF(sNullBuf) >= sColIndexDesc->mLength);
        sSourcePtr  = sNullBuf;
    }
    else
    {
        sSourcePtr = sObject;
    }

    buildOneColumn( sColIndexDesc->mDataType,
                    (UChar *)sObj,
                    sSourcePtr,
                    sColIndexDesc->mLength,
                    sRowBuf,
                    sColIndexDesc->mConvCallback );

    sOffset1 = sProperty->mMinColumn->offset;
    sOffset2 = sProperty->mMaxColumn->offset;
    /* Column �Ѱ��� ������� �ϱ⶧���� Column offset �� 0 ����
     * �ٲ㼭 �˻�������Ѵ�
     */
    sProperty->mMinColumn->offset = 0;
    sProperty->mMaxColumn->offset = 0;

    for ( sRange = sProperty->mKeyRange; sRange != NULL; sRange = sRange->next )
    {
        if ( sRange->minimum.callback( &sResult,
                                       sRowBuf,
                                       NULL,
                                       0,
                                       SC_NULL_GRID,
                                       sRange->minimum.data )
             != IDE_SUCCESS )
        {
            sResult = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sResult == ID_TRUE )
        {
            if ( sRange->maximum.callback( &sResult,
                                           sRowBuf,
                                           NULL,
                                           0,
                                           SC_NULL_GRID,
                                           sRange->maximum.data )
                 != IDE_SUCCESS )
            {
                sResult = ID_TRUE;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sResult == ID_TRUE )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sProperty->mMinColumn->offset = sOffset1;
    sProperty->mMaxColumn->offset = sOffset2;

    IDE_EXCEPTION_CONT( normal_exit );

    return sResult;
}

