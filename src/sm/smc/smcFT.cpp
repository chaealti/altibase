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
 * $Id: smcFT.cpp 19860 2007-02-07 02:09:39Z kimmkeun $
 *
 * Description
 *
 *   BUG-20805
 *   Memory Table Records�� DUMP�ϱ� ���� �Լ�
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smErrorCode.h>
#include <smpManager.h>
#include <svpManager.h>
#include <svpVarPageList.h>
#include <smcDef.h>
#include <smcFT.h>
#include <sgmManager.h>
#include <smiFixedTable.h>
#include <sdp.h>
#include <sdc.h>
#include <smxTransMgr.h>
#include <smcReq.h>
#include <smiMisc.h>

/***********************************************************************
 * Description
 *
 *   D$MEM_TABLE_RECORD
 *   : MEMORY TABLE�� Record ������ ���
 *
 *
 **********************************************************************/

extern smiGlobalCallBackList gSmiGlobalCallBackList;

//------------------------------------------------------
// D$MEM_TABLE_RECORD Dump Table�� Column Description
//------------------------------------------------------

static iduFixedTableColDesc gDumpMemTableRecordColDesc[]=
{
    {
        (SChar*)"PAGE_ID",
        offsetof(smcDumpMemTableRow, mPageID ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mPageID ),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"OFFSET",
        offsetof(smcDumpMemTableRow, mOffset ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mOffset ),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CREATE_TID",
        offsetof(smcDumpMemTableRow, mCreateTID ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mCreateTID ),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CREATE_SCN",
        offsetof(smcDumpMemTableRow, mCreateSCN ),
        16,
        IDU_FT_TYPE_CHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"LIMIT_TID",
        offsetof(smcDumpMemTableRow, mLimitTID ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mLimitTID ),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"LIMIT_SCN",
        offsetof(smcDumpMemTableRow, mLimitSCN ),
        16,
        IDU_FT_TYPE_CHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NEXT_OID",
        offsetof(smcDumpMemTableRow, mNext ),
        16,
        IDU_FT_TYPE_CHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"FLAG",
        offsetof(smcDumpMemTableRow, mFlag ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mFlag ),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"USED_FLAG",
        offsetof(smcDumpMemTableRow, mUsedFlag ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mUsedFlag ),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"DROP_FLAG",
        offsetof(smcDumpMemTableRow, mDropFlag ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mDropFlag ),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SKIP_REFINE_FLAG",
        offsetof(smcDumpMemTableRow, mSkipRefineFlag ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mSkipRefineFlag ),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NTH_SLOT",
        offsetof(smcDumpMemTableRow, mNthSlot ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mNthSlot ),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NTH_COLUMN",
        offsetof(smcDumpMemTableRow, mNthColumn ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mNthColumn ),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"VALUE24B",
        offsetof(smcDumpMemTableRow, mValue ),
        IDU_FT_SIZEOF(smcDumpMemTableRow, mValue ),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

//------------------------------------------------------
// D$MEM_TABLE_RECORD Dump Table�� Table Description
//------------------------------------------------------

iduFixedTableDesc  gDumpMemTableRecordTableDesc =
{
    (SChar *)"D$MEM_TABLE_RECORD",
    smcFT::buildRecordMemTableRecord,
    gDumpMemTableRecordColDesc,
    IDU_STARTUP_SERVICE,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

//------------------------------------------------------
// D$MEM_TABLE_RECORD Dump Table�� ���ڵ� Build
//------------------------------------------------------

IDE_RC smcFT::buildRecordMemTableRecord( idvSQL              * /*aStatistics*/,
                                         void                * aHeader,
                                         void                * aDumpObj,
                                         iduFixedTableMemory * aMemory )
{
    smcTableHeader      * sTblHdr = NULL;
    scPageID              sCurPageID;
    scPageID              sLstPageID;
    SChar               * sPagePtr;
    smpSlotHeader       * sSlotHeader;
    smcDumpMemTableRow    sDumpRecord;
    smpPageListEntry    * sFixedPageList;
    idBool                sLocked = ID_FALSE;
    SChar               * sRowPtr;
    SChar               * sFence;
    UInt                  i;
    UInt                  sSlotSeq;
    SChar                 sStrCreateSCN[ SM_SCN_STRING_LENGTH + 1];
    SChar                 sStrLimitSCN[ SM_SCN_STRING_LENGTH + 1];
    SChar                 sNext[ SM_SCN_STRING_LENGTH + 1 ];

    //------------------------------------------
    // Parameter Validation
    //------------------------------------------

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    IDE_TEST_RAISE( aDumpObj == NULL, ERR_EMPTY_OBJECT );

    sTblHdr = (smcTableHeader *)( (smpSlotHeader*)aDumpObj + 1);

    IDE_TEST_RAISE( sTblHdr->mType != SMC_TABLE_NORMAL &&
                    sTblHdr->mType != SMC_TABLE_CATALOG,
                    ERR_INVALID_DUMP_OBJECT );

    IDE_TEST_RAISE( ( SMI_TABLE_TYPE_IS_MEMORY( sTblHdr ) == ID_FALSE ) &&
                    ( SMI_TABLE_TYPE_IS_META( sTblHdr ) == ID_FALSE ),
                    ERR_INVALID_DUMP_OBJECT );

    //------------------------------------------
    // Get Table Info
    //------------------------------------------
    sFixedPageList = (smpPageListEntry*)&(sTblHdr->mFixed);
    sCurPageID     = smpManager::getFirstAllocPageID(sFixedPageList);
    sLstPageID     = smpManager::getLastAllocPageID(sFixedPageList);

    //------------------------------------------
    // Get Table Records
    //------------------------------------------
    while( 1 )
    {
        sSlotSeq = 0;
        IDE_TEST( smmManager::holdPageSLatch(sTblHdr->mSpaceID,
                                             sCurPageID)
                  != IDE_SUCCESS );
        sLocked = ID_TRUE;

        IDE_ASSERT( smmManager::getPersPagePtr( sTblHdr->mSpaceID,
                                                sCurPageID,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );

        sRowPtr  = sPagePtr + SMP_PERS_PAGE_BODY_OFFSET;
        sFence   = sRowPtr + sFixedPageList->mSlotCount * sFixedPageList->mSlotSize;

        for( ; sRowPtr < sFence; sRowPtr += sFixedPageList->mSlotSize )
        {
            sSlotHeader = (smpSlotHeader *)sRowPtr;

            sDumpRecord.mPageID    = sCurPageID;
            sDumpRecord.mOffset    = SMP_SLOT_GET_OFFSET( sSlotHeader );
            sDumpRecord.mCreateTID = SMP_GET_TID( sSlotHeader->mCreateSCN );
            sDumpRecord.mLimitTID  = SMP_GET_TID( sSlotHeader->mLimitSCN );

            idlOS::memset( sStrCreateSCN, 0x00, SM_SCN_STRING_LENGTH + 1 );
            idlOS::sprintf( (SChar*)sStrCreateSCN, 
                            "%"ID_XINT64_FMT, 
                            SM_SCN_TO_LONG( sSlotHeader->mCreateSCN ) );
            sDumpRecord.mCreateSCN = sStrCreateSCN;

            idlOS::memset( sStrLimitSCN, 0x00, SM_SCN_STRING_LENGTH + 1 );
            idlOS::sprintf( (SChar*)sStrLimitSCN, 
                            "%"ID_XINT64_FMT, 
                            SM_SCN_TO_LONG( sSlotHeader->mLimitSCN ) );
            sDumpRecord.mLimitSCN = sStrLimitSCN;

            idlOS::memset( sNext, 0x00, SM_SCN_STRING_LENGTH + 1 );
            idlOS::snprintf( (SChar*)sNext, SM_SCN_STRING_LENGTH,
                             "%"ID_XINT64_FMT, SMP_SLOT_GET_NEXT_OID(sSlotHeader) );
            sDumpRecord.mNext = sNext;

            /* BUG-31062�� Slot Header Flag ó�� ��� ����*/
            sDumpRecord.mFlag = SMP_SLOT_GET_FLAGS( sSlotHeader );

            sDumpRecord.mUsedFlag       = SMP_SLOT_IS_USED( sSlotHeader ) ? 'U':'F';
            sDumpRecord.mDropFlag       = SMP_SLOT_IS_DROP( sSlotHeader ) ? 'D':'F';
            sDumpRecord.mSkipRefineFlag = SMP_SLOT_IS_SKIP_REFINE( sSlotHeader ) ? 'S':'F';

            sDumpRecord.mNthSlot = sSlotSeq++;

            for( i = 0; i < sTblHdr->mColumnCount; i++ )
            {
                idlOS::memset( sDumpRecord.mValue,
                               0x00,
                               SM_DUMP_VALUE_LENGTH );

                if( ( sDumpRecord.mFlag & SMP_SLOT_USED_MASK )
                    == SMP_SLOT_USED_TRUE )
                {
                    IDE_TEST( makeMemColValue24B(
                                  smcTable::getColumn(sTblHdr, i),
                                  sRowPtr,
                                  sDumpRecord.mValue ) != IDE_SUCCESS );
                }

                sDumpRecord.mNthColumn = i;

                IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                    aMemory,
                                                    (void *)&sDumpRecord )
                         != IDE_SUCCESS);
            }
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(sTblHdr->mSpaceID,
                                               sCurPageID)
                  != IDE_SUCCESS );

        if( sCurPageID == sLstPageID )
        {
            break;
        }

        sCurPageID = smpManager::getNextAllocPageID(sTblHdr->mSpaceID,
                                                    sFixedPageList,
                                                    sCurPageID);
    }

    //------------------------------------------
    // Finalize
    //------------------------------------------

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_DUMP_OBJECT);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_INVALID_DUMP_OBJECT));
    }
    IDE_EXCEPTION(ERR_EMPTY_OBJECT);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_DUMP_EMPTY_OBJECT));
    }

    IDE_EXCEPTION_END;

    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch( sTblHdr->mSpaceID,
                                                  sCurPageID )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


IDE_RC smcFT::makeMemColValue24B( const smiColumn * aColumn,
                                  SChar           * aRowPtr,
                                  SChar           * aValue24B )
{
    smiKey2StringFunc     sKey2String;
    UChar                 sValueBuffer[SM_DUMP_VALUE_BUFFER_SIZE];
    UInt                  sValueLength;
    IDE_RC                sReturn;
    SChar               * sColumnPtr;
    SChar                 sDummyNullPtr[3]  = { 0 }; /* mtdCharType �� ���缭 3 ����Ʈ ��� */
    SChar               * sKeyPtr           = NULL;
    UInt                  sLength           = 0;

    IDE_TEST( gSmiGlobalCallBackList.findKey2String(
                  aColumn, 0, &sKey2String )
              != IDE_SUCCESS );

    sColumnPtr = aRowPtr + aColumn->offset;

    if( ( aColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
        != SMI_COLUMN_COMPRESSION_TRUE )
    {
        switch( aColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            case SMI_COLUMN_TYPE_FIXED:
                sKeyPtr = sColumnPtr;
                break;
            case SMI_COLUMN_TYPE_VARIABLE:
            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                sKeyPtr = sgmManager::getVarColumn( aRowPtr,
                                                    aColumn,
                                                    &sLength );
                /* PROJ-2419 
                 * Null column return �ϴ� ���
                 * �������� fixed ������ column ptr �� ��ȯ�޾�����
                 * �ٲ� ���������� fixed �� vcDesc �� ������� ����ų���� �����Ƿ�
                 * ���������� ��ü�Ѵ� */
                if ( sKeyPtr == NULL )
                {
                    sKeyPtr = sDummyNullPtr;
                }
                else
                {
                    /* Nothing to do */
                }

                break;
            default:
                break;
        }
    }
    else
    {
        // PROJ-2264
        sKeyPtr = (SChar*)smiGetCompressionColumn( aRowPtr,
                                                   aColumn,
                                                   ID_TRUE, //aUseColumnOffset
                                                   &sLength );
    }

    sValueLength = SM_DUMP_VALUE_BUFFER_SIZE;
    IDE_TEST(sKey2String( (smiColumn*)aColumn,
                          (void*) (sKeyPtr),
                          0, /* meaningless */
                          (UChar*) SM_DUMP_VALUE_DATE_FMT,
                          idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                          sValueBuffer,
                          &sValueLength,
                          &sReturn )
             != IDE_SUCCESS );

    idlOS::memcpy( aValue24B,
                   sValueBuffer,
                   ( sValueLength > SM_DUMP_VALUE_LENGTH ) ?
                   SM_DUMP_VALUE_LENGTH : sValueLength );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :PROJ-1407 Temporary Table
 *              Temp Table Info�� ���� Fixed Table�� Record�� �����Ѵ�.
 **********************************************************************/
IDE_RC
smcFT::buildRecordForTempTableInfo(idvSQL              * /*aStatistics*/,
                                   void                *aHeader,
                                   void                * /* aDumpObj */,
                                   iduFixedTableMemory *aMemory)
{
    smcTableHeader *sCatTblHdr  = (smcTableHeader*)SMC_CAT_TEMPTABLE;
    smcTableHeader *sTgtTblHeader;
    void           *sLockItem   = NULL;
    SChar          *sNxtPtr;
    SChar          *sCurPtr     = NULL;
    smpSlotHeader  *sTgtSlotPtr = NULL;
    UInt            sTableType;
    UInt            sState = 0;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    /* BUG-16351: X$Table_info�� Catalog Table�� ���� ������ ������
     * �ʽ��ϴ�. */
    IDE_TEST( buildEachRecordForTableInfo(
                  aHeader,
                  sCatTblHdr,
                  aMemory) != IDE_SUCCESS );

    while(1)
    {
        IDE_TEST( smcRecord::nextOIDall( sCatTblHdr, sCurPtr, &sNxtPtr )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr

        sTgtSlotPtr   = (smpSlotHeader *)sNxtPtr;
        sTgtTblHeader = (smcTableHeader *)( sTgtSlotPtr + 1 );

        /* To fix BUG-14681 */
        /* BUG-31673 - [SM] If select the X$TABLE_INFO in the process of
         *             creating a table, because of uninitialized
         *             the lockitem, segfault can occur.
         *
         * ���̺� ���� ���� ���̸� ��, mSCN�� Infinite�̸� ���̺� lock item��
         * ���� �ʱ�ȭ���� �ʾ��� ���� �־� ���� ����� ���ɼ��� �ִ�.
         * ���� ���������� �Ϸ�Ǿ� commit �� ��쿡 ���ؼ� lock�� �⵵��
         * �����Ѵ�. */
        if( SM_SCN_IS_INFINITE(sTgtSlotPtr->mCreateSCN) == ID_TRUE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        sLockItem = SMC_TABLE_LOCK( sTgtTblHeader );
        IDE_TEST( smLayerCallback::getMutexOfLockItem( sLockItem )->lock( NULL )
                  != IDE_SUCCESS );
        sState = 1;

        sTableType = sTgtTblHeader->mFlag & SMI_TABLE_TYPE_MASK;

        IDE_ERROR_MSG(( sTableType == SMI_TABLE_VOLATILE ),
                      "Invalid Table Type : %"ID_UINT32_FMT"\n",
                      sTableType );

        /* �ش����̺��� Drop���� �ʾҴٸ� */
        // BUG-30867 Discard �� Tablespace�� ���� Table�� Skip�Ǿ�� ��
        if( sctTableSpaceMgr::hasState( sTgtTblHeader->mSpaceID,
                                        SCT_SS_INVALID_DISK_TBS ) == ID_FALSE )
        {
            IDE_TEST( buildEachRecordForTableInfo(
                          aHeader,
                          sTgtTblHeader,
                          aMemory)
                      != IDE_SUCCESS );
        }

        sState = 0;
        IDE_TEST( smLayerCallback::getMutexOfLockItem( sLockItem )->unlock()
                  != IDE_SUCCESS );

        sCurPtr = sNxtPtr;
    }// while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        smLayerCallback::getMutexOfLockItem( sLockItem )->unlock();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aTargetTblHeader�� �ش��ϴ� Table Info�� ���� Fixed Table��
 *               Record�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smcFT::buildEachRecordForTableInfo(
    void                *aFixedTblHeader,
    smcTableHeader      *aTargetTblHeader,
    iduFixedTableMemory *aMemory)
{
    smcTableInfoPerfV sTableInfo;
    UInt              i;
    ULong             sSlotCnt=0;
    UInt              sTableType;
    sdpSegInfo        sSegInfo;
    sdpSegMgmtOp     *sSegMgmtOp;
    scPageID          sSegPID;
    smpSlotHeader    *sTargetSlotPtr = ((smpSlotHeader *)aTargetTblHeader - 1);

    /* BUG-45654
     * Droped slot �� ��� IDE_TEST_RAISE( SMP_SLOT_IS_DROP ) ����
     *   �б��ϴ� �� �����δ� INC-38866 ���̽��� ���� �� ����.
     * ���� ���� ���ο��� droped slot �� �ƴϴ��� Lock���� ��ȣ���� ������
     *   �Ʒ� �ڵ带 �����ϴ� ���� mRuntimeEntry �����Ͱ� NULL�� �� �� �ִ�.
     * ���� ���� �Լ����� Lock���� ��ȣ�Ѵ�.
     *   ���⼭ IDE_TEST_RAISE �� ����� ���� 
     *   ���� Lock���� ��ȣ���� ���� ��찡 �߻��ϸ�
     *   �ִ��� �Լ� ���ο��� ��ȣ�ϰ�
     *   TC�� ���� ã�� �� �ֵ���
     *   droped slot�� ���� record�� �������� �ʵ��� �Ѵ�.                    */
    IDE_TEST_CONT( SMP_SLOT_IS_DROP( sTargetSlotPtr ), TABLE_IS_DROPPED );

    // fix BUG-20118
    idlOS::memset(&sTableInfo, 0x00, sizeof(smcTableInfoPerfV));

    sTableType = SMI_GET_TABLE_TYPE( aTargetTblHeader );

    /* aTarget Table�� ���ؼ��� Lock�� ���� �ʴ´�. �ֳ��ϸ�
       �ѹ� ������ Table�� Table Header�� Server�� ����ɶ�
       ���� �������� �ʴ´�. */
    sTableInfo.mTableOID         = aTargetTblHeader->mSelfOID;
    sTableInfo.mSpaceID          = aTargetTblHeader->mSpaceID;
    sTableInfo.mType             = sTableType;
    sTableInfo.mIsConsistent     = aTargetTblHeader->mIsConsistent;

    // TASK-2398 Log Compression
    //           Table�� �α� ���� ���� ����
    if ( ( aTargetTblHeader->mFlag & SMI_TABLE_LOG_COMPRESS_MASK )
         == SMI_TABLE_LOG_COMPRESS_TRUE )
    {
        sTableInfo.mCompressedLogging = 1;
    }
    else
    {
        sTableInfo.mCompressedLogging = 0;
    }

    if ((sTableType == SMI_TABLE_META) ||
        (sTableType == SMI_TABLE_MEMORY) ||
        (sTableType == SMI_TABLE_REMOTE))
    {
        /* Meta , Memory Table. */
        sTableInfo.mMemSlotCnt  = aTargetTblHeader->mFixed.mMRDB.mSlotCount;
        sTableInfo.mMemSlotSize = aTargetTblHeader->mFixed.mMRDB.mSlotSize;

        // fix BUG-20118
        if (aTargetTblHeader->mFixed.mMRDB.mRuntimeEntry != NULL)
        {
            sTableInfo.mMemPageHead =
                smpManager::getFirstAllocPageID(&(aTargetTblHeader->mFixed.mMRDB));
            sTableInfo.mMemPageCnt  =
                smpManager::getAllocPageCount(&(aTargetTblHeader->mFixed.mMRDB));
            sTableInfo.mMemVarPageCnt =
                smpManager::getAllocPageCount(aTargetTblHeader->mVar.mMRDB);


            /* Table�� Record������ �����´�. */
            IDE_TEST( smcTable::getRecordCount( aTargetTblHeader, &(sSlotCnt))
                      != IDE_SUCCESS);

            sTableInfo.mFixedUsedMem = sSlotCnt * sTableInfo.mMemSlotSize;
            sTableInfo.mVarUsedMem   = 0;

            /* ���� memset���� �ʱ�ȭ �Ǿ��⶧���� , mMemVarpageCnt,
               mSlotCount�� �ʱ�ȭ�� ���⼭ �� �ʿ䰡 ����. */
            for(i= 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
            {
                sSlotCnt = smpVarPageList::getRecordCount( &(aTargetTblHeader->mVar.mMRDB[i]) );
                sTableInfo.mVarUsedMem +=
                    sSlotCnt * aTargetTblHeader->mVar.mMRDB[i].mSlotSize;
            }//for

            //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ �� Aging�� �и��� ������ �� ��ȭ��
            sTableInfo.mUniqueViolationCount =
                aTargetTblHeader->mFixed.mMRDB.mRuntimeEntry->mUniqueViolationCount;
            sTableInfo.mUpdateRetryCount =
                aTargetTblHeader->mFixed.mMRDB.mRuntimeEntry->mUpdateRetryCount;
            sTableInfo.mDeleteRetryCount =
                aTargetTblHeader->mFixed.mMRDB.mRuntimeEntry->mDeleteRetryCount;
        }
    }//if
    else if (sTableType == SMI_TABLE_VOLATILE)
    {
        sTableInfo.mMemSlotCnt  = aTargetTblHeader->mFixed.mVRDB.mSlotCount;
        sTableInfo.mMemSlotSize = aTargetTblHeader->mFixed.mVRDB.mSlotSize;

        // fix BUG-20118
        if (aTargetTblHeader->mFixed.mVRDB.mRuntimeEntry != NULL)
        {
            sTableInfo.mMemPageCnt  =
                svpManager::getAllocPageCount(&(aTargetTblHeader->mFixed.mVRDB));
            sTableInfo.mMemVarPageCnt =
                svpManager::getAllocPageCount(aTargetTblHeader->mVar.mVRDB);
            sTableInfo.mMemPageHead =
                svpManager::getFirstAllocPageID(&(aTargetTblHeader->mFixed.mVRDB));

            /* Table�� Record������ �����´�. */
            IDE_TEST( smcTable::getRecordCount( aTargetTblHeader, &(sSlotCnt))
                      != IDE_SUCCESS);

            sTableInfo.mFixedUsedMem = sSlotCnt * sTableInfo.mMemSlotSize;
            sTableInfo.mVarUsedMem   = 0;

            /* ���� memset���� �ʱ�ȭ �Ǿ��⶧���� , mMemVarpageCnt,
               mSlotCount�� �ʱ�ȭ�� ���⼭ �� �ʿ䰡 ����. */
            for(i= 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
            {
                sSlotCnt = svpVarPageList::getRecordCount( &(aTargetTblHeader->mVar.mVRDB[i]) );
                sTableInfo.mVarUsedMem +=
                    sSlotCnt * aTargetTblHeader->mVar.mVRDB[i].mSlotSize;
            }
            //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ �� Aging�� �и��� ������ �� ��ȭ��
            /* PROJ-1381 �̱��� ������� 0���� ������ (mStatementRebuildCount)
            sTableInfo.mStatementRebuildCount =
                aTargetTblHeader->mFixed.mVRDB.mRuntimeEntry->mStatementRebuildCount;
            */
            sTableInfo.mStatementRebuildCount = 0;
            sTableInfo.mUniqueViolationCount =
                aTargetTblHeader->mFixed.mVRDB.mRuntimeEntry->mUniqueViolationCount;
            sTableInfo.mUpdateRetryCount =
                aTargetTblHeader->mFixed.mVRDB.mRuntimeEntry->mUpdateRetryCount;
            sTableInfo.mDeleteRetryCount =
                aTargetTblHeader->mFixed.mVRDB.mRuntimeEntry->mDeleteRetryCount;
        }
    }
    else if ( sTableType == SMI_TABLE_DISK )
    {
        sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( aTargetTblHeader->mSpaceID );
        // codesonar::Null Pointer Dereference
        IDE_ERROR( sSegMgmtOp != NULL );

        sSegPID = sdpSegDescMgr::getSegPID( &(aTargetTblHeader->mFixed.mDRDB));
        IDE_TEST_CONT( sSegPID == SD_NULL_PID, TABLE_IS_DROPPED );

        /* BUGBUG: ���⼭ ���� �ؾ���. */
        IDE_TEST( sSegMgmtOp->mGetSegInfo(
                      NULL,
                      aTargetTblHeader->mSpaceID,
                      sSegPID,
                      aTargetTblHeader,
                      &sSegInfo )
                  != IDE_SUCCESS );

        IDE_TEST_CONT( sSegInfo.mSegHdrPID == SD_NULL_PID, TABLE_IS_DROPPED );

        /* Disk Table. */
        sTableInfo.mDiskTotalPageCnt = sSegInfo.mPageCntInExt * sSegInfo.mExtCnt;
        sTableInfo.mDiskPageCnt      = sSegInfo.mFmtPageCnt;
        sTableInfo.mFstExtRID        = sSegInfo.mFstExtRID;
        sTableInfo.mLstExtRID        = sSegInfo.mLstExtRID;
        //XXX MetaPID�� �ѷ��־�� ��.
        //sTableInfo.mTableMetaPage    = sSegInfo.mMetaPID;

        sTableInfo.mTableSegPID =
            sdpSegDescMgr::getSegPID( &(aTargetTblHeader->mFixed.mDRDB) );

        sTableInfo.mDiskPctFree =
            sdpSegDescMgr::getSegAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mPctFree;
        sTableInfo.mDiskPctUsed =
            sdpSegDescMgr::getSegAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mPctUsed;

        sTableInfo.mDiskInitTrans =
            sdpSegDescMgr::getSegAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mInitTrans;
        sTableInfo.mDiskMaxTrans =
            sdpSegDescMgr::getSegAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mMaxTrans;

        sTableInfo.mDiskInitExtents =
            sdpSegDescMgr::getSegStoAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mInitExtCnt;

        sTableInfo.mDiskNextExtents =
            sdpSegDescMgr::getSegStoAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mNextExtCnt;

        sTableInfo.mDiskMinExtents =
            sdpSegDescMgr::getSegStoAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mMinExtCnt;

        sTableInfo.mDiskMaxExtents =
            sdpSegDescMgr::getSegStoAttr(
                &(aTargetTblHeader->mFixed.mDRDB.mSegDesc))->mMaxExtCnt;

        //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ �� Aging�� �и��� ������ �� ��ȭ��
        sTableInfo.mStatementRebuildCount = 0;
        sTableInfo.mUniqueViolationCount =  0;
        sTableInfo.mUpdateRetryCount =      0;
        sTableInfo.mDeleteRetryCount =      0;
    }
    else
    {
        // Disk temp table�� �� �Լ��� ȣ��Ǹ� �ȵȴ�.
        IDE_ASSERT(0);
    }

    IDE_TEST(iduFixedTable::buildRecord(aFixedTblHeader,
                                        aMemory,
                                        (void *)&sTableInfo)
             != IDE_SUCCESS);

    IDE_EXCEPTION_CONT( TABLE_IS_DROPPED );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Sequence�� ���� Fixed Table�� Record�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smcFT::buildRecordForSEQInfo(idvSQL              * /*aStatistics*/,
                                    void        *aHeader,
                                    void        * /* aDumpObj */,
                                    iduFixedTableMemory *aMemory)
{
    smcTableHeader *sCatTblHdr;
    smcTableHeader *sHeader;
    smpSlotHeader  *sPtr;
    SChar          *sCurPtr;
    SChar          *sNxtPtr;
    smcSequence4PerfV sSeqInfo;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr = NULL;

    while(1)
    {
        IDE_TEST(smcRecord::nextOIDall(sCatTblHdr,sCurPtr,&sNxtPtr)
                  != IDE_SUCCESS);
        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr
        sPtr = (smpSlotHeader *)sNxtPtr;
        // To fix BUG-14681
        if( SM_SCN_IS_INFINITE(sPtr->mCreateSCN) == ID_TRUE )
        {
            /* BUG-14974: ���� Loop�߻�.*/
            sCurPtr = sNxtPtr;
            continue;
        }
        sHeader = (smcTableHeader *)( sPtr + 1 );

        // 1. -temp table�� skip- PROJ-2201 TempTable�� ���� ����
        // 2. drop�� table�� skip
        // 3. sequence�� �ƴ� object�� skip.
        if( (smcTable::isDropedTable( sHeader ) == ID_TRUE) ||
            (sHeader->mType !=  SMC_TABLE_SEQUENCE) )
        {
            sCurPtr = sNxtPtr;
            continue;
        }//if

        sSeqInfo.mSeqOID = sHeader->mSelfOID;
        idlOS::memcpy(&(sSeqInfo.mSequence), &(sHeader->mSequence),
                      ID_SIZEOF(smcSequenceInfo));

        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *)&sSeqInfo)
             != IDE_SUCCESS);


        sCurPtr = sNxtPtr;
    }// while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


static iduFixedTableColDesc gCatalogColDesc[]=
{
    {
        (SChar*)"TABLE_OID",
        offsetof(smcCatalogInfoPerfV,mTableOID),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mTableOID),
        IDU_FT_TYPE_UBIGINT | IDU_FT_COLUMN_INDEX ,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"COLUMN_CNT",
        offsetof(smcCatalogInfoPerfV,mColumnCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mColumnCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"COLUMN_VAR_SLOT_CNT",
        offsetof(smcCatalogInfoPerfV,mColumnVarSlotCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mColumnVarSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INDEX_CNT",
        offsetof(smcCatalogInfoPerfV,mIndexCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mIndexCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INDEX_VAR_SLOT_CNT",
        offsetof(smcCatalogInfoPerfV,mIndexVarSlotCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mIndexVarSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },



    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }

};


// X$CATALOG
iduFixedTableDesc  gCatalogDesc =
{
    (SChar *)"X$CATALOG",
    smcFT::buildRecordForCatalog,
    gCatalogColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_USE,
    NULL
};

// for catalog table column, indexes, performance view.
IDE_RC smcFT::buildRecordForCatalog(idvSQL              * /*aStatistics*/,
                                    void        *aHeader,
                                    void        * /* aDumpObj */,
                                    iduFixedTableMemory *aMemory)
{
    smcTableHeader        *sCatTblHdr;
    smcTableHeader        *sHeader;
    smpSlotHeader         *sPtr;
    smcCatalogInfoPerfV   sCatalogInfo;
    smVCPieceHeader       *sVCPieceHdr;
    SChar                 *sCurPtr;
    SChar                 *sNxtPtr;
    UInt                  i;
    void*                 sTrans;
    smOID                 sPieceOID;
    void                * sISavepoint = NULL;
    UInt                  sDummy = 0;
    void                * sIndexValues[1];
    UInt                  sState = 0;

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr = NULL;

    if ( aMemory->useExternalMemory() == ID_TRUE )
    {
        /* BUG-43006 FixedTable Indexing Filter */
        sTrans = ((smiFixedTableProperties *)aMemory->getContext())->mTrans;
    }
    else
    {
        /* BUG-32292 [sm-util] Self deadlock occur since fixed-table building
         * operation uses another transaction.
         * NestedTransaction�� ����ϸ� Self-deadlock ����� �ִ�.
         * ���� id Memory �������κ��� Iterator�� ��� Transaction�� ����. */
        sTrans = ((smiIterator*)aMemory->getContext())->trans;
    }

    while(1)
    {
        IDE_TEST(smcRecord::nextOIDall(sCatTblHdr,sCurPtr,&sNxtPtr)
                  != IDE_SUCCESS);
        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr
        sPtr = (smpSlotHeader *)sNxtPtr;
        // To fix BUG-14681
        if( SM_SCN_IS_INFINITE(sPtr->mCreateSCN) == ID_TRUE )
        {
            /* BUG-14974: ���� Loop�߻�.*/
            sCurPtr = sNxtPtr;
            continue;
        }
        sHeader = (smcTableHeader *)( sPtr + 1 );

        // temp table�� skip
        if( smcTable::isDropedTable(sHeader) == ID_TRUE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }//if

        /* BUG-43006 FixedTable Indexing Filter
         * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        /* BUG-48160 ���͸� �߰� */
        sIndexValues[0] = &sHeader->mSelfOID;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gCatalogColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }
        sState = 1;
        IDE_TEST( smLayerCallback::setImpSavepoint( sTrans,
                                                    &sISavepoint,
                                                    sDummy )
                  != IDE_SUCCESS );

        /* BUG-48160 lock ���� table �� �����ϰ� ����ϴ� ��� �߰�.
         * Property : __SKIP_LOCKED_TABLE_AT_FIXED_TABLE */
        if ( smLayerCallback::lockTableModeIS4FixedTable( sTrans,
                                                          SMC_TABLE_LOCK( sHeader ) )
             == IDE_SUCCESS )
        {
            sState = 2;

            //lock�� ������� table�� drop�� ��쿡�� skip;
            if(smcTable::isDropedTable(sHeader) == ID_TRUE)
            {
                sState = 1;
                IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans,
                                                                sISavepoint )
                          != IDE_SUCCESS );
                sState = 0;
                IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans,
                                                              sISavepoint )
                          != IDE_SUCCESS );
                sCurPtr = sNxtPtr;
                continue;
            }//if

            sCatalogInfo.mTableOID =  sHeader->mSelfOID;
            sCatalogInfo.mColumnCnt = sHeader->mColumnCount;
            sCatalogInfo.mIndexCnt =  smcTable::getIndexCount(sHeader);

            for( sCatalogInfo.mColumnVarSlotCnt = 0,sPieceOID = sHeader->mColumns.fstPieceOID;
                 sPieceOID != SM_NULL_OID; sCatalogInfo.mColumnVarSlotCnt++ )
            {
                IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                   sPieceOID,
                                                   (void**)&sVCPieceHdr )
                            == IDE_SUCCESS );

                sPieceOID =  sVCPieceHdr->nxtPieceOID;
            }//for

            for(i = 0,sCatalogInfo.mIndexVarSlotCnt = 0;
                i < SMC_MAX_INDEX_OID_CNT;
                i++)
            {
                if(sHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
                {
                    continue;
                }//if aHeader
                sCatalogInfo.mIndexVarSlotCnt++;
            }//for i

            IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                aMemory,
                                                (void *)&sCatalogInfo)
                     != IDE_SUCCESS);
            sState = 1;
            IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                      != IDE_SUCCESS );
        }
        sState = 0;
        IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                  != IDE_SUCCESS );
        sCurPtr = sNxtPtr;
    }// while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}


// PROJ-1407 Temporary Table
// X$TEMP_CATALOG
static iduFixedTableColDesc gTempCatalogColDesc[]=
{
    {
        (SChar*)"TABLE_OID",
        offsetof(smcCatalogInfoPerfV,mTableOID),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mTableOID),
        IDU_FT_TYPE_UBIGINT | IDU_FT_COLUMN_INDEX ,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"COLUMN_CNT",
        offsetof(smcCatalogInfoPerfV,mColumnCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mColumnCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"COLUMN_VAR_SLOT_CNT",
        offsetof(smcCatalogInfoPerfV,mColumnVarSlotCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mColumnVarSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INDEX_CNT",
        offsetof(smcCatalogInfoPerfV,mIndexCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mIndexCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INDEX_VAR_SLOT_CNT",
        offsetof(smcCatalogInfoPerfV,mIndexVarSlotCnt),
        IDU_FT_SIZEOF(smcCatalogInfoPerfV,mIndexVarSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },



    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }

};


// PROJ-1407 Temporary Table
// X$TEMP_CATALOG
iduFixedTableDesc  gTempCatalogDesc =
{
    (SChar *)"X$TEMP_CATALOG",
    smcFT::buildRecordForTempCatalog,
    gTempCatalogColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_USE,
    NULL
};

/***********************************************************************
 * Description :PROJ-1407 Temporary Table
 *              for catalog table column, indexes, performance view.
 **********************************************************************/
IDE_RC smcFT::buildRecordForTempCatalog(idvSQL              * /*aStatistics*/,
                                        void        *aHeader,
                                        void        * /* aDumpObj */,
                                        iduFixedTableMemory *aMemory)
{
    smcTableHeader        *sCatTblHdr = (smcTableHeader*)SMC_CAT_TEMPTABLE;
    smcTableHeader        *sHeader;
    smpSlotHeader         *sPtr;
    smcCatalogInfoPerfV   sCatalogInfo;
    smVCPieceHeader       *sVCPieceHdr;
    SChar                 *sCurPtr = NULL;
    SChar                 *sNxtPtr;
    UInt                  i;
    void*                 sTrans;
    smOID                 sPieceOID;
    void                * sISavepoint = NULL;
    UInt                  sDummy = 0;
    UInt                  sTableType;
    void                * sIndexValues[1];
    UInt                  sState = 0;

    if ( aMemory->useExternalMemory() == ID_TRUE )
    {
        /* BUG-43006 FixedTable Indexing Filter */
        sTrans = ((smiFixedTableProperties *)aMemory->getContext())->mTrans;
    }
    else
    {
        /* BUG-32292 [sm-util] Self deadlock occur since fixed-table building
         * operation uses another transaction.
         * NestedTransaction�� ����ϸ� Self-deadlock ����� �ִ�.
         * ���� id Memory �������κ��� Iterator�� ��� Transaction�� ����. */
        sTrans = ((smiIterator*)aMemory->getContext())->trans;
    }

    while(1)
    {
        IDE_TEST(smcRecord::nextOIDall(sCatTblHdr,sCurPtr,&sNxtPtr)
                  != IDE_SUCCESS);
        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr
        sPtr = (smpSlotHeader *)sNxtPtr;
        // To fix BUG-14681
        if( SM_SCN_IS_INFINITE(sPtr->mCreateSCN) == ID_TRUE )
        {
            // BUG-14974: ���� Loop�߻�.
            sCurPtr = sNxtPtr;
            continue;
        }
        sHeader = (smcTableHeader *)( sPtr + 1 );

        sTableType = sHeader->mFlag & SMI_TABLE_TYPE_MASK;

        IDE_ERROR_MSG(( sTableType == SMI_TABLE_VOLATILE ),
                      "Invalid Table Type : %"ID_UINT32_FMT"\n",
                      sTableType );

        /* BUG-43006 FixedTable Indexing Filter
         * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        /* BUG-48160 ���͸� �߰� */
        sIndexValues[0] = &sHeader->mSelfOID;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gTempCatalogColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        sState = 1;
        IDE_TEST( smLayerCallback::setImpSavepoint( sTrans,
                                                    &sISavepoint,
                                                    sDummy )
                  != IDE_SUCCESS );

        /* BUG-48160 lock ���� table �� �����ϰ� ����ϴ� ��� �߰�.
         * Property : __SKIP_LOCKED_TABLE_AT_FIXED_TABLE */
        if ( smLayerCallback::lockTableModeIS4FixedTable( sTrans,
                                                          SMC_TABLE_LOCK( sHeader ) )
             == IDE_SUCCESS )
        {
            sState = 2;
            sCatalogInfo.mTableOID  = sHeader->mSelfOID;
            sCatalogInfo.mColumnCnt = sHeader->mColumnCount;
            sCatalogInfo.mIndexCnt  = smcTable::getIndexCount(sHeader);

            for( sCatalogInfo.mColumnVarSlotCnt = 0,sPieceOID = sHeader->mColumns.fstPieceOID;
                 sPieceOID != SM_NULL_OID; sCatalogInfo.mColumnVarSlotCnt++ )
            {
                IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                   sPieceOID,
                                                   (void**)&sVCPieceHdr )
                            == IDE_SUCCESS );

                sPieceOID =  sVCPieceHdr->nxtPieceOID;
            }//for

            for(i = 0,sCatalogInfo.mIndexVarSlotCnt = 0;
                i < SMC_MAX_INDEX_OID_CNT;
                i++)
            {
                if(sHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
                {
                    continue;
                }//if aHeader
                sCatalogInfo.mIndexVarSlotCnt++;
            }//for i

            IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                aMemory,
                                                (void *)&sCatalogInfo)
                     != IDE_SUCCESS);

            sState = 1;
            IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                      != IDE_SUCCESS );
        }

        sState = 0;
        IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                  != IDE_SUCCESS );
        sCurPtr = sNxtPtr;
    }// while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}



static iduFixedTableColDesc  gSeqInfoColDesc[]=
{

    {
        (SChar*)"SEQ_OID",
        offsetof(smcSequence4PerfV,mSeqOID),
        IDU_FT_SIZEOF(smcSequence4PerfV,mSeqOID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"CURRENT_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mCurSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mCurSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"START_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mStartSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mStartSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INCREMENT_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mIncSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mIncSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SYNC_INTERVAL",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mSyncInterval) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mSyncInterval),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MAX_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mMaxSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mMaxSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MIN_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mMinSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mMinSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"LAST_SYNC_SEQ",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mLstSyncSequence) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mLstSyncSequence),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"FLAG",
        offsetof(smcSequence4PerfV,mSequence) +offsetof(smcSequenceInfo,mFlag) ,
        IDU_FT_SIZEOF(smcSequenceInfo,mFlag),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$SEQ
iduFixedTableDesc  gSeqDesc=
{
    (SChar *)"X$SEQ",
    smcFT::buildRecordForSEQInfo,
    gSeqInfoColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

static iduFixedTableColDesc  gTableInfoColDesc[]=
{
    {
        (SChar*)"TABLESPACE_ID",
        offsetof(smcTableInfoPerfV,mSpaceID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mSpaceID),
        IDU_FT_TYPE_USMALLINT | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TABLE_TYPE",
        offsetof(smcTableInfoPerfV,mType),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mType),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TABLE_OID",
        offsetof(smcTableInfoPerfV,mTableOID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableOID),
        IDU_FT_TYPE_UBIGINT | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mMemPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_VAR_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mMemVarPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemVarPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"MEM_SLOT_CNT",
        offsetof(smcTableInfoPerfV,mMemSlotCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_SLOT_SIZE",
        offsetof(smcTableInfoPerfV,mMemSlotSize),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemSlotSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"FIXED_USED_MEM",
        offsetof(smcTableInfoPerfV,mFixedUsedMem),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mFixedUsedMem),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"VAR_USED_MEM",
        offsetof(smcTableInfoPerfV,mVarUsedMem),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mVarUsedMem),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"MEM_FIRST_PAGEID",
        offsetof(smcTableInfoPerfV,mMemPageHead),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemPageHead),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"DISK_TOTAL_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mDiskTotalPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskTotalPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"DISK_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mDiskPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SEG_PID",
        offsetof(smcTableInfoPerfV,mTableSegPID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableSegPID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"META_PAGE",
        offsetof(smcTableInfoPerfV,mTableMetaPage),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableMetaPage),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"FST_EXTRID",
        offsetof(smcTableInfoPerfV,mFstExtRID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mFstExtRID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"LST_EXTRID",
        offsetof(smcTableInfoPerfV,mLstExtRID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mLstExtRID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"PCTFREE",
        offsetof(smcTableInfoPerfV,mDiskPctFree),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPctFree),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"PCTUSED",
        offsetof(smcTableInfoPerfV,mDiskPctUsed),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPctUsed),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INIT_TRANS",
        offsetof(smcTableInfoPerfV,mDiskInitTrans),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskInitTrans),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL
    },

    {
        (SChar*)"MAX_TRANS",
        offsetof(smcTableInfoPerfV,mDiskMaxTrans),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMaxTrans),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL
    },

    // initextnts
    {
        (SChar*)"INITEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskInitExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskInitExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // nextextnts
    {
        (SChar*)"NEXTEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskNextExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskNextExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // minextnts
    {
        (SChar*)"MINEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskMinExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMinExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // maxextnts
    {
        (SChar*)"MAXEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskMaxExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMaxExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    //BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //Aging�� �и��� ������ �� ��ȭ��.
    //���̺��� ������ old version �� ����
    {
        (SChar*)"OLD_VERSION_COUNT",
        offsetof(smcTableInfoPerfV,mOldVersionCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mOldVersionCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    //�� ���̺��� DDL�� ���ට���� rebuild�� tx�� ����
    {
        (SChar*)"STATEMENT_REBUILD_COUNT",
        offsetof(smcTableInfoPerfV,mStatementRebuildCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mStatementRebuildCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    //�ߺ�Ű�� �Է��Ϸ� �Ϸ��ϴ� abort�� Ƚ��
    {
        (SChar*)"UNIQUE_VIOLATION_COUNT",
        offsetof(smcTableInfoPerfV,mUniqueViolationCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mUniqueViolationCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //update�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...
    //�� ���̺� ���� �̷��� ����� �߻� Ƚ���� ��.
    {
        (SChar*) "UPDATE_RETRY_COUNT",
        offsetof(smcTableInfoPerfV,mUpdateRetryCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mUpdateRetryCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //remove�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...
    //�� ���̺� ���� �̷��� ����� �߻� Ƚ���� ��.
    {
        (SChar*)"DELETE_RETRY_COUNT",
        offsetof(smcTableInfoPerfV,mDeleteRetryCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDeleteRetryCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"COMPRESSED_LOGGING",
        offsetof(smcTableInfoPerfV,mCompressedLogging),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mCompressedLogging),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"IS_CONSISTENT",
        offsetof(smcTableInfoPerfV,mIsConsistent),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mIsConsistent),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$TABLE_INFO
iduFixedTableDesc  gTableInfoTableDesc=
{
    (SChar *)"X$TABLE_INFO",
    smcFT::buildRecordForTableInfo,
    gTableInfoColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_USE,
    NULL
};

// PROJ-1407 Temporary Table
// X$TEMP_TABLE_INFO
static iduFixedTableColDesc gTempTableInfoColDesc[]=
{
    {
        (SChar*)"TABLESPACE_ID",
        offsetof(smcTableInfoPerfV,mSpaceID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mSpaceID),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TABLE_TYPE",
        offsetof(smcTableInfoPerfV,mType),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mType),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TABLE_OID",
        offsetof(smcTableInfoPerfV,mTableOID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableOID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mMemPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_VAR_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mMemVarPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemVarPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"MEM_SLOT_CNT",
        offsetof(smcTableInfoPerfV,mMemSlotCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemSlotCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MEM_SLOT_SIZE",
        offsetof(smcTableInfoPerfV,mMemSlotSize),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemSlotSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"FIXED_USED_MEM",
        offsetof(smcTableInfoPerfV,mFixedUsedMem),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mFixedUsedMem),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"VAR_USED_MEM",
        offsetof(smcTableInfoPerfV,mVarUsedMem),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mVarUsedMem),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"MEM_FIRST_PAGEID",
        offsetof(smcTableInfoPerfV,mMemPageHead),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mMemPageHead),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"DISK_TOTAL_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mDiskTotalPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskTotalPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"DISK_PAGE_CNT",
        offsetof(smcTableInfoPerfV,mDiskPageCnt),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPageCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SEG_PID",
        offsetof(smcTableInfoPerfV,mTableSegPID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableSegPID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"META_PAGE",
        offsetof(smcTableInfoPerfV,mTableMetaPage),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mTableMetaPage),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"FST_EXTRID",
        offsetof(smcTableInfoPerfV,mFstExtRID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mFstExtRID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"LST_EXTRID",
        offsetof(smcTableInfoPerfV,mLstExtRID),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mLstExtRID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"PCTFREE",
        offsetof(smcTableInfoPerfV,mDiskPctFree),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPctFree),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"PCTUSED",
        offsetof(smcTableInfoPerfV,mDiskPctUsed),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskPctUsed),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INIT_TRANS",
        offsetof(smcTableInfoPerfV,mDiskInitTrans),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskInitTrans),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL
    },

    {
        (SChar*)"MAX_TRANS",
        offsetof(smcTableInfoPerfV,mDiskMaxTrans),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMaxTrans),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL
    },

    // initextnts
    {
        (SChar*)"INITEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskInitExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskInitExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // nextextnts
    {
        (SChar*)"NEXTEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskNextExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskNextExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // minextnts
    {
        (SChar*)"MINEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskMinExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMinExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // maxextnts
    {
        (SChar*)"MAXEXTENTS",
        offsetof(smcTableInfoPerfV,mDiskMaxExtents),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDiskMaxExtents),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    //BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //Aging�� �и��� ������ �� ��ȭ��.
    //���̺��� ������ old version �� ����
    {
        (SChar*)"OLD_VERSION_COUNT",
        offsetof(smcTableInfoPerfV,mOldVersionCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mOldVersionCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    //�� ���̺��� DDL�� ���ට���� rebuild�� tx�� ����
    {
        (SChar*)"STATEMENT_REBUILD_COUNT",
        offsetof(smcTableInfoPerfV,mStatementRebuildCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mStatementRebuildCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    //�ߺ�Ű�� �Է��Ϸ� �Ϸ��ϴ� abort�� Ƚ��
    {
        (SChar*)"UNIQUE_VIOLATION_COUNT",
        offsetof(smcTableInfoPerfV,mUniqueViolationCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mUniqueViolationCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //update�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...
    //�� ���̺� ���� �̷��� ����� �߻� Ƚ���� ��.
    {
        (SChar*) "UPDATE_RETRY_COUNT",
        offsetof(smcTableInfoPerfV,mUpdateRetryCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mUpdateRetryCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //remove�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...
    //�� ���̺� ���� �̷��� ����� �߻� Ƚ���� ��.
    {
        (SChar*)"DELETE_RETRY_COUNT",
        offsetof(smcTableInfoPerfV,mDeleteRetryCount),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mDeleteRetryCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"COMPRESSED_LOGGING",
        offsetof(smcTableInfoPerfV,mCompressedLogging),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mCompressedLogging),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"IS_CONSISTENT",
        offsetof(smcTableInfoPerfV,mIsConsistent),
        IDU_FT_SIZEOF(smcTableInfoPerfV,mIsConsistent),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

/***********************************************************************
 * Description :Table Info�� ���� Fixed Table�� Record�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smcFT::buildRecordForTableInfo(idvSQL              * /*aStatistis*/,
                                      void                *aHeader,
                                      void                * /* aDumpObj */,
                                      iduFixedTableMemory *aMemory)
{
    smcTableHeader *sCatTblHdr;
    smcTableHeader *sTgtTblHeader;
    SChar          *sCurPtr;
    SChar          *sNxtPtr;
    smpSlotHeader  *sTgtSlotPtr = NULL;
    UInt            sTableType;
    void          * sIndexValues[3];
    void*           sTrans;
    void           *sISavepoint = NULL;
    UInt            sDummy      = 0;
    UInt            sTxStatus   = 0;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    if ( aMemory->useExternalMemory() == ID_TRUE )
    {
        /* BUG-43006 FixedTable Indexing Filter */
        sTrans = ((smiFixedTableProperties *)aMemory->getContext())->mTrans;
    }
    else
    {
        /* BUG-32292 [sm-util] Self deadlock occur since fixed-table building
         * operation uses another transaction.
         * NestedTransaction�� ����ϸ� Self-deadlock ����� �ִ�.
         * ���� id Memory �������κ��� Iterator�� ��� Transaction�� ����. */
        sTrans = ((smiIterator *)aMemory->getContext())->trans;
    }

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr = NULL;

    sTableType = SMI_GET_TABLE_TYPE( sCatTblHdr );

    /* BUG-43006 FixedTable Indexing Filter
     * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
     * �κи� ������ Filtering �Ѵ�.
     * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
     * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
     * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
     * �� �־���Ѵ�.
     */
    /* BUG-48160 ���͸� �׸� �߰� */
    sIndexValues[0] = &sCatTblHdr->mSpaceID;
    sIndexValues[1] = &sTableType;
    sIndexValues[2] = &sCatTblHdr->mSelfOID;
    if ( iduFixedTable::checkKeyRange( aMemory,
                                       gTableInfoColDesc,
                                       sIndexValues )
         == ID_TRUE )
    {
        /* BUG-16351: X$Table_info�� Catalog Table�� ���� ������ ������
         * �ʽ��ϴ�. */
        IDE_TEST( buildEachRecordForTableInfo(
                      aHeader,
                      sCatTblHdr,
                      aMemory)
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    while (1)
    {
        IDE_TEST( smcRecord::nextOIDall( sCatTblHdr, sCurPtr, &sNxtPtr )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr

        sTgtSlotPtr = (smpSlotHeader *)sNxtPtr;
        sTgtTblHeader = (smcTableHeader *)( sTgtSlotPtr + 1 );

        /* BUG-45654 Check slot SCN */
        if ( SM_SCN_IS_INFINITE( sTgtSlotPtr->mCreateSCN ) == ID_TRUE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        if ( smcTable::isDropedTable( sTgtTblHeader ) == ID_TRUE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }
        /* BUG-43006 FixedTable Indexing Filter
         * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        /* BUG-48160 ���͸� �߰� */
        sTableType = SMI_GET_TABLE_TYPE( sTgtTblHeader );
        sIndexValues[0] = &sTgtTblHeader->mSpaceID;
        sIndexValues[1] = &sTableType;
        sIndexValues[2] = &sTgtTblHeader->mSelfOID;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gTableInfoColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }
        /* BUG-45654 Transaction for lock */
        sTxStatus = 1;
        IDE_TEST( smLayerCallback::setImpSavepoint(  sTrans,
                                                    &sISavepoint,
                                                     sDummy )
                  != IDE_SUCCESS );

        IDU_FIT_POINT_RAISE( "BUG-45654@smcFT::buildRecordForTableInfo::skipTableLock",
                             TABLE_LOCK_NEXT );

        /* BUG-48160 lock ���� table �� �����ϰ� ����ϴ� ��� �߰�.
         * Property : __SKIP_LOCKED_TABLE_AT_FIXED_TABLE */
        if ( smLayerCallback::lockTableModeIS4FixedTable( sTrans,
                                                          sTgtTblHeader->mLock )
             == IDE_SUCCESS )
        {
            sTxStatus = 2;
#ifdef ALTIBASE_FIT_CHECK
            IDE_EXCEPTION_CONT( TABLE_LOCK_NEXT );
#endif

            /* To fix BUG-14681 */
            /* BUG-31673 - [SM] If select the X$TABLE_INFO in the process of
             *             creating a table, because of uninitialized
             *             the lockitem, segfault can occur.
             *
             * ���̺� ���� ���� ���̸� ��, mSCN�� Infinite�̸� ���̺� lock item��
             * ���� �ʱ�ȭ���� �ʾ��� ���� �־� ���� ����� ���ɼ��� �ִ�.
             * ���� ���������� �Ϸ�Ǿ� commit �� ��쿡 ���ؼ� lock�� �⵵��
             * �����Ѵ�. */
            /* BUG-45654 Lock ���� �� �ٽ� Ȯ�� */
            if( SM_SCN_IS_INFINITE(sTgtSlotPtr->mCreateSCN) == ID_FALSE )
            {
                /* Target Table�� Temp Table�� �ƴϰ� �ش����̺��� Drop����
                 * �ʾҴٸ� */
                // BUG-30867 Discard �� Tablespace�� ���� Table�� Skip�Ǿ�� ��
                if( (smcTable::isDropedTable(sTgtTblHeader) == ID_FALSE) &&
                    (sctTableSpaceMgr::hasState( sTgtTblHeader->mSpaceID,
                                                 SCT_SS_INVALID_DISK_TBS ) 
                     == ID_FALSE ))
                {
                    IDU_FIT_POINT( "BUG-42805@smcFT::buildRecordForTableInfo::afterMutexOfLockItem" );

                    IDE_TEST( buildEachRecordForTableInfo(
                                  aHeader,
                                  sTgtTblHeader,
                                  aMemory)
                              != IDE_SUCCESS );
                }
            }

            /* BUG-45654 Transaction for lock */
            sTxStatus = 1;
            IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                      != IDE_SUCCESS );
        }

        sTxStatus = 0;
        IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                  != IDE_SUCCESS );

        sCurPtr = sNxtPtr;
    }// while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sTxStatus )
    {
        case 2:
            IDE_ASSERT( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}


// X$TEMP_TABLE_INFO
iduFixedTableDesc  gTempTableInfoTableDesc=
{
    (SChar *)"X$TEMP_TABLE_INFO",
    smcFT::buildRecordForTempTableInfo,
    gTempTableInfoColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

static iduFixedTableColDesc  gTmsTableCacheColDesc[]=
{
    {
        (SChar*)"TBS_ID",
        offsetof(smcTmsCacheInfoPerfV,mSpaceID),
        IDU_FT_SIZEOF(smcTmsCacheInfoPerfV,mSpaceID),
        IDU_FT_TYPE_USMALLINT | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SEG_PID",
        offsetof(smcTmsCacheInfoPerfV,mSegPID),
        IDU_FT_SIZEOF(smcTmsCacheInfoPerfV,mSegPID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_POSVT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSPosVtPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSPosVtPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_RTBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSRtBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSRtBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_POSRT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSPosRtPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSPosRtPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_ITBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSItBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSItBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_POSIT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSPosItPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSPosItPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_LFBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSLfBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mSLfBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_ITHINT_RSFLAG",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSRsFlag),
        IDU_FT_SIZEOF(sdpHintPosInfo,mSRsFlag),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SLOT_ITHINT_STFLAG",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mSStFlag),
        IDU_FT_SIZEOF(sdpHintPosInfo, mSStFlag),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_POSVT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPPosVtPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPPosVtPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_RTBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPRtBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPRtBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_POSRT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPPosRtPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPPosRtPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_ITBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPItBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPItBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_POSIT_PAGEID",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPPosItPID ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPPosItPID ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_LFBMP_IDX",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPLfBMPIdx ),
        IDU_FT_SIZEOF( sdpHintPosInfo, mPLfBMPIdx ),
        IDU_FT_TYPE_USMALLINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_ITHINT_RSFLAG",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPRsFlag),
        IDU_FT_SIZEOF(sdpHintPosInfo,mPRsFlag),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"PAGE_ITHINT_STFLAG",
        offsetof( smcTmsCacheInfoPerfV, mHintPosInfo ) +
        offsetof( sdpHintPosInfo, mPStFlag),
        IDU_FT_SIZEOF(sdpHintPosInfo, mPStFlag),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$TMS_TABLE_CACHE;
iduFixedTableDesc gTmsTableCacheDesc=
{
    (SChar *)"X$TMS_TABLE_CACHE",
    smcFT::buildRecordForTmsTableCache,
    gTmsTableCacheColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_USE,
    NULL
};



IDE_RC smcFT::buildRecordForTmsTableCache(idvSQL              * /*aStatistics*/,
                                          void        *aHeader,
                                          void        * /* aDumpObj */,
                                          iduFixedTableMemory *aMemory)
{
    void                    * sTrans;
    smcTableHeader          * sCatTblHdr;
    smcTableHeader          * sHeader;
    smpSlotHeader           * sPtr;
    smcTmsCacheInfoPerfV      sCacheInfo;
    SChar                   * sCurPtr;
    SChar                   * sNxtPtr;
    sdpSegHandle            * sSegHandle;
    sdpSegMgmtOp            * sSegMgmtOp;
    void                    * sISavepoint = NULL;
    UInt                      sDummy = 0;
    void                    * sIndexValues[2];
    UInt                      sState = 0;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr = NULL;

    if ( aMemory->useExternalMemory() == ID_TRUE )
    {
        /* BUG-43006 FixedTable Indexing Filter */
        sTrans = ((smiFixedTableProperties *)aMemory->getContext())->mTrans;
    }
    else
    {
        /* BUG-32292 [sm-util] Self deadlock occur since fixed-table building
         * operation uses another transaction.
         * NestedTransaction�� ����ϸ� Self-deadlock ����� �ִ�.
         * ���� id Memory �������κ��� Iterator�� ��� Transaction�� ����. */
        sTrans = ((smiIterator*)aMemory->getContext())->trans;
    }

    while(1)
    {
        IDE_TEST(smcRecord::nextOIDall(sCatTblHdr,sCurPtr,&sNxtPtr)
                  != IDE_SUCCESS);
        if( sNxtPtr == NULL )
        {
            break;
        }//if sNxtPtr
        sPtr = (smpSlotHeader *)sNxtPtr;
        // To fix BUG-14681
        if( SM_SCN_IS_INFINITE(sPtr->mCreateSCN) == ID_TRUE )
        {
            /* BUG-14974: ���� Loop�߻�.*/
            sCurPtr = sNxtPtr;
            continue;
        }
        sHeader = (smcTableHeader *)( sPtr + 1 );

        if( ( SMI_TABLE_TYPE_IS_DISK( sHeader )  == ID_FALSE ) ||
            ( smcTable::isDropedTable( sHeader ) == ID_TRUE ) )
        {
            sCurPtr = sNxtPtr;
            continue;
        }//if

        /* BUG-43006 FixedTable Indexing Filter
         * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        sIndexValues[0] = &sHeader->mSpaceID;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gTmsTableCacheColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        sState = 1;
        IDE_TEST( smLayerCallback::setImpSavepoint( sTrans,
                                                    &sISavepoint,
                                                    sDummy )
                  != IDE_SUCCESS );

        /* BUG-48160 lock ���� table �� �����ϰ� ����ϴ� ��� �߰�.
         * Property : __SKIP_LOCKED_TABLE_AT_FIXED_TABLE */
        if ( smLayerCallback::lockTableModeIS4FixedTable( sTrans,
                                                          SMC_TABLE_LOCK( sHeader ) )
             == IDE_SUCCESS )
        {
            sState = 2;
            // lock�� ������� table�� drop�� ��쿡�� skip;
            // TBS Drop, Discarded, Offline  �� ��쿡�� skip;
            if( ( smcTable::isDropedTable(sHeader) ) ||
                ( sctTableSpaceMgr::hasState(sHeader->mSpaceID, 
                                             SCT_SS_SKIP_BUILD_FIXED_TABLE) ) )
            {
                sState = 1;
                IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans,
                                                                sISavepoint )
                          != IDE_SUCCESS );
                sState = 0;
                IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans,
                                                              sISavepoint )
                          != IDE_SUCCESS );
                sCurPtr = sNxtPtr;
                continue;
            }

            if ( sdpTableSpace::getSegMgmtType( sHeader->mSpaceID )
                 != SMI_SEGMENT_MGMT_TREELIST_TYPE )
            {
                sCurPtr = sNxtPtr;
                continue;
            }

            idlOS::memset( &sCacheInfo, 0, ID_SIZEOF( smcTmsCacheInfoPerfV ) );

            sCacheInfo.mSpaceID  = sHeader->mSpaceID;

            sCacheInfo.mSegPID   = sdpSegDescMgr::getSegPID( &(sHeader->mFixed.mDRDB) );

            sSegHandle =
                sdpSegDescMgr::getSegHandle( &(sHeader->mFixed.mDRDB) );

            sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( sHeader->mSpaceID );
            // codesonar::Null Pointer Dereference
            IDE_ERROR( sSegMgmtOp != NULL );

            sSegMgmtOp->mGetHintPosInfo( NULL,  // aStatistics
                                         sSegHandle->mCache,
                                         &(sCacheInfo.mHintPosInfo) );

            IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                aMemory,
                                                (void *)&sCacheInfo)
                     != IDE_SUCCESS);
            sState = 1;
            IDE_TEST( smLayerCallback::abortToImpSavepoint( sTrans,
                                                            sISavepoint )
                      != IDE_SUCCESS );
        }
        sState = 0;
        IDE_TEST( smLayerCallback::unsetImpSavepoint( sTrans,
                                                      sISavepoint )
                  != IDE_SUCCESS );
        sCurPtr = sNxtPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( smLayerCallback::abortToImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smLayerCallback::unsetImpSavepoint( sTrans, sISavepoint )
                        == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}
/* ---------------------------------------------------
 *  Fixed Table Define for X$MEM_LOB_STATISTICS
 * -------------------------------------------------*/

static iduFixedTableColDesc gMemLobStatisticsColDesc[] =
{
    {
        (SChar*)"OPEN_COUNT",
        offsetof(smcLobStatistics, mOpen),
        IDU_FT_SIZEOF(smcLobStatistics, mOpen),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"READ_COUNT",
        offsetof(smcLobStatistics, mRead),
        IDU_FT_SIZEOF(smcLobStatistics, mRead),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"WRITE_COUNT",
        offsetof(smcLobStatistics, mWrite),
        IDU_FT_SIZEOF(smcLobStatistics, mWrite),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"PREPARE_FOR_WRITE_COUNT",
        offsetof(smcLobStatistics, mPrepareForWrite),
        IDU_FT_SIZEOF(smcLobStatistics, mPrepareForWrite),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"FINISH_WRITE_COUNT",
        offsetof(smcLobStatistics, mFinishWrite),
        IDU_FT_SIZEOF(smcLobStatistics, mFinishWrite),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"GET_LOBINFO_COUNT",
        offsetof(smcLobStatistics, mGetLobInfo),
        IDU_FT_SIZEOF(smcLobStatistics, mGetLobInfo),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        (SChar*)"CLOSE_COUNT",
        offsetof(smcLobStatistics, mClose),
        IDU_FT_SIZEOF(smcLobStatistics, mClose),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

static IDE_RC buildRecordForMemLobStatistics(idvSQL              * /*aStatistics*/,
                                             void             *aHeader,
                                             void             * /* aDumpObj */,
                                             iduFixedTableMemory *aMemory)
{
    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                        aMemory,
                                        (void *) &smcLob::mMemLobStatistics)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

iduFixedTableDesc gMemLobStatisticsTableDesc =
{
    (SChar *)"X$MEM_LOB_STATISTICS",
    buildRecordForMemLobStatistics,
    gMemLobStatisticsColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};




