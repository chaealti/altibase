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
 * Copyright 1999-2001, ATIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: smiMisc.cpp 90936 2021-06-02 06:02:46Z emlee $
 **********************************************************************/

#include <smErrorCode.h>

#include <smu.h>
#include <smm.h>
#include <svm.h>
#include <sdd.h>
#include <smp.h>
#include <sdp.h>
#include <smc.h>
#include <sdc.h>
#include <sml.h>
#include <smx.h>
#include <sma.h>
#include <sds.h>
#include <smi.h>

#include <sctTableSpaceMgr.h>
#include <sgmManager.h>
#include <scpfModule.h>
#include <sdpscFT.h>

extern smiGlobalCallBackList gSmiGlobalCallBackList;

/* PROJ-1915 */
extern const UInt            smVersionID;

// BUG-14113
// isolation level�� ���� table lock mode
static const smiTableLockMode smiTableLockModeOnIsolationLevel[3][6] =
{
    {
        // SMI_ISOLATION_CONSISTENT (==0x00000000)
        SMI_TABLE_NLOCK,    // SMI_TABLE_NLOCK
        SMI_TABLE_LOCK_S,   // SMI_TABLE_LOCK_S
        SMI_TABLE_LOCK_X,   // SMI_TABLE_LOCK_X
        SMI_TABLE_LOCK_IS,  // SMI_TABLE_LOCK_IS
        SMI_TABLE_LOCK_IX,  // SMI_TABLE_LOCK_IX
        SMI_TABLE_LOCK_SIX  // SMI_TABLE_LOCK_SIX
    },
    {
        // SMI_ISOLATION_REPEATABLE (==0x00000001)
        SMI_TABLE_NLOCK,    // SMI_TABLE_NLOCK
        SMI_TABLE_LOCK_S,   // SMI_TABLE_LOCK_S
        SMI_TABLE_LOCK_X,   // SMI_TABLE_LOCK_X
        SMI_TABLE_LOCK_IX,  // SMI_TABLE_LOCK_IS
        SMI_TABLE_LOCK_IX,  // SMI_TABLE_LOCK_IX
        SMI_TABLE_LOCK_SIX  // SMI_TABLE_LOCK_SIX
    },
    {
        // SMI_ISOLATION_NO_PHANTOM (0x00000002)
        SMI_TABLE_NLOCK,    // SMI_TABLE_NOLOCK
        SMI_TABLE_LOCK_S,   // SMI_TABLE_LOCK_S
        SMI_TABLE_LOCK_X,   // SMI_TABLE_LOCK_X
        SMI_TABLE_LOCK_S,   // SMI_TABLE_LOCK_IS
        SMI_TABLE_LOCK_X,   // SMI_TABLE_LOCK_IX
        SMI_TABLE_LOCK_SIX  // SMI_TABLE_LOCK_SIX
    }
};


// For A4 : Table Space Type�� ���� ���� �߰�
UInt smiGetPageSize( smiTableSpaceType aTSType )
{

    if ( (aTSType == SMI_MEMORY_SYSTEM_DICTIONARY) ||
         (aTSType == SMI_MEMORY_SYSTEM_DATA) ||
         (aTSType == SMI_MEMORY_USER_DATA) )
    {
        return (UInt)SM_PAGE_SIZE;
    }
    else
    {
        return (UInt)SD_PAGE_SIZE;
    }

}

/* ���� ��ũ DB�� �� ũ�⸦ ���Ѵ�. */
ULong smiGetDiskDBFullSize()
{
    ULong               sDiskDBFullSize = 0; 
    sctTableSpaceNode*  sCurrSpaceNode;
    sctTableSpaceNode*  sNextSpaceNode;
    smiTableSpaceAttr   sTableSpaceAttr;
    sddDataFileNode*    sFileNode;
    smiDataFileAttr     sDataFileAttr;
    UInt                i = 0; 

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {    
       sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        /* Disk tablespace�� �������� ũ�⸸ ���ϴ� ���� ��ǥ�̹Ƿ�
         * undo tablespace�� ��ǥ���� �����ϵ��� �Ѵ�. */
        if ( ( sCurrSpaceNode->mType == SMI_DISK_SYSTEM_DATA ) || 
             ( sCurrSpaceNode->mType == SMI_DISK_USER_DATA ) )
        {    
            if ( SMI_TBS_IS_DROPPED(sCurrSpaceNode->mState) )
            {    
                sCurrSpaceNode = sNextSpaceNode;
                continue;
            }    

            sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode*)sCurrSpaceNode,
                                              &sTableSpaceAttr );
            
            for( i = 0 ; i < ((sddTableSpaceNode*)sCurrSpaceNode)->mNewFileID ; i++ )
            {   
                sFileNode = ((sddTableSpaceNode*)sCurrSpaceNode)->mFileNodeArr[i];
                
                if ( sFileNode == NULL )
                {   
                    continue;
                }
                
                if ( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
                {   
                    continue;
                }
                
                sddDataFile::getDataFileAttr(sFileNode, &sDataFileAttr);

                if ( sDataFileAttr.mMaxSize > 0 )
                {
                    sDiskDBFullSize += sDataFileAttr.mMaxSize;
                }
                else
                {
                    /* auto extend�� ���� ���� ��� mMaxSize�� 0 �� ��찡 �ִ�.
                     * �� ��쿡�� ���� ũ�⸦ MaxSize ������� ����ϵ��� �Ѵ�. */
                    sDiskDBFullSize += sDataFileAttr.mCurrSize;
                }        
            }
        }
        sCurrSpaceNode = sNextSpaceNode;
    }
    return sDiskDBFullSize;
}

UInt smiGetBufferPoolSize( )
{
    return sdbBufferMgr::getPageCount();
}

SChar * smiGetDBName( )
{
    return smmDatabase::getDBName();
}

// PROJ-1579 NCHAR
SChar * smiGetDBCharSet( )
{
    return smmDatabase::getDBCharSet();
}

// PROJ-1579 NCHAR
SChar * smiGetNationalCharSet( )
{
    return smmDatabase::getNationalCharSet();
}
    
const void* smiGetCatalogTable( void )
{
    return (void*)( (UChar*)smmManager::m_catTableHeader - SMP_SLOT_HEADER_SIZE );
}

// For A4 : Table Type�� ���� ���� �߰�
UInt smiGetVariableColumnSize( UInt aTableType )
{
    UInt sTableType = aTableType & SMI_TABLE_TYPE_MASK;

    IDE_ASSERT( ( sTableType == SMI_TABLE_MEMORY ) ||
                ( sTableType == SMI_TABLE_VOLATILE ) );

    return idlOS::align8(ID_SIZEOF(smVCDesc));
}

UInt smiGetVariableColumnSize4DiskIndex()
{
    return idlOS::align8(ID_SIZEOF(sdcVarColHdr));
}

UInt smiGetVCDescInModeSize()
{
    return idlOS::align8(ID_SIZEOF(smVCDescInMode));
}


// For A4 : Table Type�� ���� ���� �߰�
UInt smiGetRowHeaderSize( UInt aTableType )
{

    UInt sTableType = aTableType & SMI_TABLE_TYPE_MASK;

    if ( ( sTableType == SMI_TABLE_MEMORY   ) ||
         ( sTableType == SMI_TABLE_VOLATILE ) )
    {
        return SMP_SLOT_HEADER_SIZE;
    }
    else
    {
        IDE_ASSERT(0);
    }
}

// For A4 : Table Type�� ���� ���� �߰� ����. table handle�� ���ؼ��� ����
//          !! ���� Disk Row�� ���� ���Ǿ�� �ȵ� !!
smSCN smiGetRowSCN( const void * aRow )
{
    smSCN sSCN;
    smTID sDummyTID;

    SMX_GET_SCN_AND_TID( ((smpSlotHeader*)aRow)->mCreateSCN, sSCN, sDummyTID );

    return sSCN;
}

// For A4 : Index Module ���� ���� ����
IDE_RC smiFindIndexType( SChar * aIndexName,
                         UInt *  aIndexType )
{

    UInt i;

    for( i = 0 ; i < SMI_INDEXTYPE_COUNT ; i++)
    {
        if ( gSmnAllIndex[i] != NULL )
        {
            if ( idlOS::strcmp( gSmnAllIndex[i]->mName, aIndexName ) == 0 )
            {
                *aIndexType = gSmnAllIndex[i]->mTypeID;
                break;
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    IDE_TEST( i == SMI_INDEXTYPE_COUNT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( smERR_ABORT_smnNotFoundByIndexName ) );

    return IDE_FAILURE;

}

// BUG-17449
idBool smiCanMakeIndex( UInt    aTableType,
                        UInt    aIndexType )
{
    void   * sIndexModule;
    idBool   sResult;

    sIndexModule = smnManager::getIndexModule(aTableType, aIndexType);

    if ( sIndexModule == NULL )
    {
        sResult = ID_FALSE;
    }
    else
    {
        sResult = ID_TRUE;
    }

    return sResult;
}

const SChar* smiGetIndexName( UInt aIndexType )
{

    IDE_TEST( aIndexType >= SMI_INDEXTYPE_COUNT );

    IDE_TEST( gSmnAllIndex[aIndexType] == NULL );

    return gSmnAllIndex[aIndexType]->mName;

    IDE_EXCEPTION_END;

    return NULL;

}

idBool smiGetIndexUnique( const void * aIndex )
{

    idBool sResult;

    if ( ( (((const smnIndexHeader*)aIndex)->mFlag & SMI_INDEX_UNIQUE_MASK)
           == SMI_INDEX_UNIQUE_ENABLE ) ||
         ( (((const smnIndexHeader*)aIndex)->mFlag & SMI_INDEX_LOCAL_UNIQUE_MASK)
           == SMI_INDEX_LOCAL_UNIQUE_ENABLE ) )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

// For A4 : Table Type�� ���� ���� �߰�
UInt smiGetDefaultIndexType( void )
{

    return (UInt) SMI_BUILTIN_B_TREE_INDEXTYPE_ID; // 1 : BTREE

}

idBool smiCanUseUniqueIndex( UInt aIndexType )
{
    idBool sResult;

    if ( gSmnAllIndex[aIndexType] != NULL )
    {
        if ( ( gSmnAllIndex[aIndexType]->mFlag & SMN_INDEX_UNIQUE_MASK )
             == SMN_INDEX_UNIQUE_ENABLE )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

idBool smiCanUseCompositeIndex( UInt aIndexType )
{
    idBool sResult;

    if ( gSmnAllIndex[aIndexType] != NULL )
    {
        if ( (gSmnAllIndex[aIndexType]->mFlag & SMN_INDEX_COMPOSITE_MASK)
             == SMN_INDEX_COMPOSITE_ENABLE )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

// PROJ-1502 PARTITIONED DISK TABLE
// ��� �� ������ �ε��� Ÿ������ üũ�Ѵ�.
idBool smiGreaterLessValidIndexType( UInt aIndexType )
{
    idBool sResult;

    if ( gSmnAllIndex[aIndexType] != NULL )
    {
        if ( (gSmnAllIndex[aIndexType]->mFlag & SMN_INDEX_NUMERIC_COMPARE_MASK)
             == SMN_INDEX_NUMERIC_COMPARE_ENABLE )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

// PROJ-1704 MVCC Renewal
// AGING ������ �ε��� Ÿ������ �˻��Ѵ�.
idBool smiIsAgableIndex( const void * aIndex )
{
    idBool sResult;
    UInt   sIndexType;

    sIndexType = smiGetIndexType( aIndex );
    
    if ( gSmnAllIndex[sIndexType] != NULL )
    {
        if ( (gSmnAllIndex[sIndexType]->mFlag & SMN_INDEX_AGING_MASK)
             == SMN_INDEX_AGING_ENABLE )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

const void * smiGetTable( smOID aTableOID )
{
    void * sTableHeader;

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       aTableOID,
                                       (void**)&sTableHeader )
                == IDE_SUCCESS );

    return sTableHeader;
}

smOID smiGetTableId( const void * aTable )
{
    return SMI_MISC_TABLE_HEADER(aTable)->mSelfOID;
}


UInt smiGetTableColumnCount( const void * aTable )
{

    return SMI_MISC_TABLE_HEADER(aTable)->mColumnCount;

}

UInt smiGetTableColumnSize( const void * aTable )
{

    return SMI_MISC_TABLE_HEADER(aTable)->mColumnSize;

}

// BUG-28321 drop tablespace ���� ���� �� ������ ���ᰡ �߻��մϴ�.
// qp�� Meta�� sm ���� index�� ������ ���� �ٸ� �� �ֽ��ϴ�.
// ó�� Meta������ �� �ܿ��� �� �������̽��� ����ϸ� �ȵ˴ϴ�.
const void * smiGetTableIndexes( const void * aTable,
                                 const UInt   aIdx )
{
    return smcTable::getTableIndex( (void*)SMI_MISC_TABLE_HEADER(aTable),aIdx );
}

const void * smiGetTableIndexByID( const void * aTable,
                                   const UInt   aIndexId )
{
    // BUG-28321 drop tablespace ���� ���� �� ������ ���ᰡ �߻��մϴ�.
    // Index handle �� ��ȯ�� �� Index�� ID�� �������� ��ȯ �ϵ��� ����
    return smcTable::getTableIndexByID( (void*)SMI_MISC_TABLE_HEADER(aTable),
                                        aIndexId );
}

// Primary Key Index�� Handle�� �������� �������̽� �Լ�
const void* smiGetTablePrimaryKeyIndex( const void * aTable )
{
    return smcTable::getTableIndex( (void*)SMI_MISC_TABLE_HEADER(aTable),
                                    0 ); // 0��° Index�� Primary Index�̴�.
}

IDE_RC smiGetTableColumns( const void        * aTable,
                           const UInt          aIndex,
                           const smiColumn  ** aColumn )
{
    *aColumn = (smiColumn *)smcTable::getColumn(SMI_MISC_TABLE_HEADER(aTable), aIndex);

    return IDE_SUCCESS;
}

idBool smiIsConsistentIndices( const void * aTable )
{
    UInt                 i                = 0;
    UInt                 sIndexCount      = 0;
    idBool               sIsConsistentIdx = ID_TRUE;
    smnIndexHeader     * sIndexCursor     = NULL;

    sIndexCount = smcTable::getIndexCount( (void*)SMI_MISC_TABLE_HEADER( aTable ) );

    for ( i = 0 ; i < sIndexCount ; i++ )
    {
        sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex( 
                                               (void*)SMI_MISC_TABLE_HEADER( aTable ), i );
        if ( smnManager::getIsConsistentOfIndexHeader( (void*)sIndexCursor ) == ID_FALSE )
        {
            sIsConsistentIdx = ID_FALSE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }
    return sIsConsistentIdx;
}

UInt smiGetTableIndexCount( const void * aTable )
{
    return smcTable::getIndexCount( (void*)SMI_MISC_TABLE_HEADER(aTable) );
}

const void* smiGetTableInfo( const void * aTable )
{
    const smVCDesc* sColumnVCDesc;
    const smVCPieceHeader *sVCPieceHeader;
    const void* sInfoPtr = NULL;

    sColumnVCDesc = &(SMI_MISC_TABLE_HEADER(aTable)->mInfo);

    if ( sColumnVCDesc->length != 0 )
    {
        IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sColumnVCDesc->fstPieceOID,
                                           (void**)&sVCPieceHeader )
                    == IDE_SUCCESS );

        sInfoPtr = (void*)(sVCPieceHeader + 1);
    }
    else
    {
        /* nothing to do */
    }

    return sInfoPtr;
}

IDE_RC smiGetTableTempInfo( const void    * aTable,
                            void         ** aRuntimeInfo )
{
    *aRuntimeInfo = SMI_MISC_TABLE_HEADER(aTable)->mRuntimeInfo;
    return IDE_SUCCESS;
}

void smiSetTableTempInfo( const void  * aTable,
                          void        * aTempInfo )
{
    ((smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable))->mRuntimeInfo = aTempInfo;
}

void smiSetTableReplacementLock( const void * aDstTable,
                                 const void * aSrcTable )
{
    ((smcTableHeader *)SMI_MISC_TABLE_HEADER(aDstTable))->mReplacementLock =
    ((smcTableHeader *)SMI_MISC_TABLE_HEADER(aSrcTable))->mLock;
}

void smiInitTableReplacementLock( const void * aTable )
{
    ((smcTableHeader *)SMI_MISC_TABLE_HEADER(aTable))->mReplacementLock = NULL;
}

IDE_RC smiGetTableNullRow( const void * aTable,
                           void      ** aRow,
                           scGRID     * aRowGRID )
{
    UInt            sTableType;
    smcTableHeader *sTableHeader;

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);
    sTableType   = SMI_GET_TABLE_TYPE( sTableHeader );

    if ( (sTableType == SMI_TABLE_MEMORY) ||
         (sTableType == SMI_TABLE_META) ||
         (sTableType == SMI_TABLE_VOLATILE) )
    {
        if ( sTableHeader->mNullOID != SM_NULL_OID )
        {
            IDE_ERROR( smmManager::getOIDPtr( sTableHeader->mSpaceID,
                                              sTableHeader->mNullOID, 
                                              aRow )
                        == IDE_SUCCESS );

            SC_MAKE_GRID( *aRowGRID,
                          smcTable::getTableSpaceID(sTableHeader),
                          SM_MAKE_PID(sTableHeader->mNullOID),
                          SM_MAKE_OFFSET(sTableHeader->mNullOID) );
        }
        else
        {
            SC_MAKE_NULL_GRID( *aRowGRID );
        }
    }
    else if ( sTableType == SMI_TABLE_FIXED )
    {
        /* ------------------------------------------------
         * Fixed Table�� ��� Null Row�� smiFixedTableHeader�� mNullRow�� �����Ѵ�.
         * BUG-11268
         * ----------------------------------------------*/

        IDE_ASSERT( sTableType == SMI_TABLE_FIXED );

        *aRow = ((smiFixedTableHeader *)aTable)->mNullRow;
        SC_MAKE_NULL_GRID( *aRowGRID );

        IDE_DASSERT( *aRow != NULL );
    }
    else 
    {
        // sTableType == SMI_TABLE_DISK
        IDE_ASSERT ( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt smiGetTableFlag(const void * aTable)
{

    return ((smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable))->mFlag;
}

// PROJ-1665
UInt smiGetTableLoggingMode(const void * aTable)
{
    UInt sLoggingMode;

    sLoggingMode = (((smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable))->mFlag) & SMI_TABLE_LOGGING_MASK;

    return sLoggingMode;
}

UInt smiGetTableParallelDegree(const void * aTable)
{
    UInt sParallelDegree;

    sParallelDegree = ((smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable))->mParallelDegree;

    return sParallelDegree;
}

// FOR A4 : table hadle�� ���� DISK ���̺������� ��ȯ��.
idBool smiIsDiskTable(const void * aTable)
{
    smcTableHeader * sHeader;
     
    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);

    return ( SMI_TABLE_TYPE_IS_DISK( sHeader ) == ID_TRUE ) ? ID_TRUE : ID_FALSE;
}

// FOR A4 : table hadle�� ���� Memory ���̺������� ��ȯ��.
idBool smiIsMemTable(const void * aTable)
{
    smcTableHeader * sHeader;
    
    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);

    return ( (SMI_TABLE_TYPE_IS_MEMORY( sHeader ) == ID_TRUE) ||
             (SMI_TABLE_TYPE_IS_META( sHeader )   == ID_TRUE) )
           ? ID_TRUE : ID_FALSE;
}

idBool smiIsUserMemTable( const void * aTable )
{
    smcTableHeader * sHeader;

    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);

    return ( ( SMI_TABLE_TYPE_IS_MEMORY( sHeader ) == ID_TRUE ) ? ID_TRUE : ID_FALSE ); 
}

// table hadle�� ���� Volatile ���̺������� ��ȯ��.
idBool smiIsVolTable(const void * aTable)
{
    smcTableHeader * sHeader;

    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);

    return ( SMI_TABLE_TYPE_IS_VOLATILE( sHeader ) == ID_TRUE ) ? ID_TRUE : ID_FALSE;

}
IDE_RC smiGetTableBlockCount(const void * aTable, ULong * aBlockCnt )
{
    smcTableHeader *sTblHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);

    IDE_TEST( smcTable::getTablePageCount(sTblHeader, aBlockCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*
IDE_RC smiGetTableExtentCount(const void * aTable, UInt * aBlockCnt );
{
    return IDE_SUCCESS;
}
*/

UInt smiGetIndexId( const void * aIndex )
{
    return ((const smnIndexHeader*)aIndex)->mId;
}

UInt smiGetIndexType( const void * aIndex )
{
    if ( aIndex == NULL )
    {
        return SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID;
    }
    else
    {
        return ((const smnIndexHeader*)aIndex)->mType;
    }
}

idBool smiGetIndexRange( const void * aIndex )
{
    if ( ((const smnIndexHeader*)aIndex)->mModule != NULL )
    {
        return (((const smnIndexHeader*)aIndex)->mModule->mFlag&SMN_RANGE_MASK)
               == SMN_RANGE_ENABLE ? ID_TRUE : ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    return ID_FALSE;
}

idBool smiGetIndexDimension( const void * aIndex )
{
    if ( ((const smnIndexHeader*)aIndex)->mModule != NULL )
    {
        return (((const smnIndexHeader*)aIndex)->mModule->mFlag
                &SMN_DIMENSION_MASK) == SMN_DIMENSION_ENABLE ?
          ID_TRUE : ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    return ID_FALSE;
}

const UInt* smiGetIndexColumns( const void * aIndex )
{
    return ((const smnIndexHeader*)aIndex)->mColumns;
}

const UInt* smiGetIndexColumnFlags( const void * aIndex )
{
    return ((const smnIndexHeader*)aIndex)->mColumnFlags;
}

UInt smiGetIndexColumnCount( const void * aIndex )
{
    return ((const smnIndexHeader*)aIndex)->mColumnCount;
}

idBool smiGetIndexBuiltIn( const void * aIndex )
{
    UInt sType;
    sType = smiGetIndexType( aIndex );

    IDE_DASSERT( gSmnAllIndex[sType] != NULL );
    return ( ( gSmnAllIndex[sType]->mBuiltIn == SMN_BUILTIN_INDEX ) ?
             ID_TRUE : ID_FALSE );
}

UInt smiEstimateMaxKeySize( UInt        aColumnCount,
                            smiColumn * aColumns,
                            UInt *      aMaxLengths )
{
    SInt          i;
    UInt          sFixed    = 0;
    UInt          sVariable = 0;
    UInt          sColSize;
    smiColumn *   sColumn;

    sColumn = aColumns;

    for( i = 1 ; i < (SInt)aColumnCount ; i++ )
    {
        if ( aColumns[i].offset > sColumn->offset )
        {
            sColumn = & aColumns[i];
        }
        else
        {
            // Nothing to do
        }
    }

    if ( SMI_IS_VARIABLE_COLUMN(sColumn->flag) || 
         SMI_IS_VARIABLE_LARGE_COLUMN(sColumn->flag) )
    {
        sColSize = idlOS::align8(ID_SIZEOF(sdcVarColHdr));
    }
    else
    {
        sColSize = sColumn->size;
    }

    sFixed = idlOS::align8( sColumn->offset +
                            sColSize );

    for(i = 0; i < (SInt)aColumnCount; i++)
    {

        if ( SMI_IS_VARIABLE_COLUMN(aColumns[i].flag) || 
             SMI_IS_VARIABLE_LARGE_COLUMN(aColumns[i].flag) )
        {
            sVariable += idlOS::align8(aMaxLengths[i]);
        }
        else
        {
            // Nothing to do
        }
    }

    return sFixed + idlOS::align8(sVariable);
}


UInt smiGetIndexKeySizeLimit( UInt        aTableType,
                              UInt        aIndexType )
{
    smnIndexModule * sIdxModule;

    sIdxModule = (smnIndexModule*)
        smnManager::getIndexModule(aTableType,
                                   aIndexType);
    
    return sIdxModule->mMaxKeySize;
}




// For A4 : Variable Column Data retrieval
/***********************************************************************
 * FUNCTION DESCRIPTION : smiVarAccess::smiGetVarColumn()              *
 * ------------------------------------------------------------------- *
 * �� �Լ��� fix record ������ ����Ǿ� �ִ� variable column header��  *
 * �̿��Ͽ� ���� column�� ���� �������� �Լ��̴�.                      *
 * �޸� ���̺��� ��쿡�� �����Ͱ� �ϳ��� �޸� ����� ����Ǿ�   *
 * �����Ƿ� �ܼ��� OID�� �����ͷ� ��ȯ�Ͽ� ��ȯ�ϸ� �ȴ�.              *
 * ��ũ ���̺��� ��쿡�� �����Ͱ� ��ũ�� �����Ƿ� �޸�(����)��  *
 * �÷����ϸ� ���� �� �������� �Ѵ� �����ʹ� ���� ����� ������      *
 * �����ϹǷ� �̸� �ϳ��� ��ġ�� �κ��� �ʿ��ϴ�.                      *
 * ��ũ ���̺��� ���� �÷��� ���� ������ ũ�� �� ������ ���� ��      *
 * �ִµ�,                                                             *
 *    1. QP���� �̸� ����� fix record�� variable header�� RID��       *
 *       �̿��Ͽ� ���� �÷��� �����ϴ� ��.                             *
 *    2. QP�� ������ FIlter�� �̿��Ͽ� �������� ���� �÷��� �����ϴ�   *
 *       ��.                                                           *
 *    3. QP�� ������ Key Range�� �̿��Ͽ� index node�� �ִ� ����       *
 *       �÷��� �����ϴ� ��                                            *
 *    4. Insert Ȥ�� delete key �ÿ� key ���� �����ϴ� ��.             *
 * �� �ִ�.                                                            *
 * ���⼭ index�� �����ϴ� variable key column�� �׻� �ش� node �ȿ�   *
 * ���� �����ϹǷ� ���� fix/unfix�� �� �ʿ䰡 ����. ���� Ư�� ���ۿ� *
 * �ٽ� ������ �ʿ䰡 ����. �� ��쿡 �ش��ϴ� ���� 3,4���̴�.         *
 * 1,2 ���� ��쿡�� �� �Լ��� ȣ���� �� aColumn�� value�� ������      *
 * ��ġ(������)�� �����ؼ�, �� �Լ����� �ش� ���� �÷��� ���� ������   *
 * �� �ֵ��� �Ѵ�.                                                     *
 *                                                                   *
 * ���ܷ�, 4�� ��쿡�� �̹� Data page�� insert�� Row�� �����ͷ�       *
 * key�� ��ġ�� ã�µ�, �̶� Row�� �����÷��� ���� ��쿡 �����ؼ�     *
 * ���� ���۰� �������� �ʴ´�. �̸� ���� insert cursor�� close�ÿ�  *
 * row�� after image�� ������ ��, �� row�� ��� �����÷��� row�� �ٸ�  *
 * �������� �ִ� �͵��� ��� fix�� �Ŀ� index key insert�� �����ϴ�    *
 * ������� �Ѵ�.                                                      *
 *                                                                   *
 * smiColumn->value�� ���� NULL�� �ƴ� ��쿡 �����÷� ���� �����ϴµ�,*
 * ���� ��ġ�� ���� �ι� �̻� �������� �ʱ� ���ؼ�, value�� ó����     *
 * �����÷��� ��ġ(RID)�� �����ϰ� �� ���Ŀ� ���� �����Ѵ�.
 ***********************************************************************/
const void* smiGetVarColumn( const void       * aRow,
                             const smiColumn  * aColumn,
                             UInt             * aLength )
{
    SChar    * sRet = NULL;
        
    sRet = sgmManager::getVarColumnDirect( (SChar*)aRow,
                                           aColumn,
                                           aLength );
    
    //BUG-48746: variable ���� ���� ���濡 ���� �е� ����
    if ( *aLength > aColumn->size )
    {
        *aLength = aColumn->size;
    }

    return sRet;
}

/* Description: Variable Column�� �о ��� ���ۿ� ������ ���� ���
 *              ����Ÿ�� ���ۿ� ������ �ش�.
 *
 *              1. Disk Table�� �ִ� Var Column
 *              2. Memory Table�� �����鼭 Row�� �������� Variable
 *                 Column Piece�� ���ļ� ����� ���
 *
 *              �׷��� QP���� ���� ���۸� �ָ鼭 ���� ����Ÿ�� ������
 *              �д� ��찡 �ִٰ� �Ѵ�. �ܼ��� Row Pointer�� �شٸ�
 *              ������ ���� ������ ���ۿ� ������ ��� ����� ũ��. �� ������
 *              �ذ��ϱ� ���ؼ� Buffer������ ù 8byte������ ǥ�ø� �صд�.
 *
 *              1. Memory: Variable Column�� ù Piece�� ������
 *              2. Disk : Variable Column�� SDRID
 *
 *              sdRID�� ULong�̰� Memory Pointer�� 32��Ʈ�϶���
 *              4����Ʈ������ ū���� �������� �ؼ� 8����Ʈ�� �Ѵ�.
 *
 */
UInt smiGetVarColumnBufHeadSize( const smiColumn * aColumn )
{
    IDE_ASSERT( (aColumn->flag & SMI_COLUMN_STORAGE_MASK)
                == SMI_COLUMN_STORAGE_MEMORY );

    return ID_SIZEOF(ULong);
}

// For A4 : Table Type�� ���� ���� �߰�
UInt smiGetVarColumnLength( const void*       aRow,
                            const smiColumn * aColumn )
{
    IDE_ASSERT( (aColumn->flag & SMI_COLUMN_STORAGE_MASK)
                == SMI_COLUMN_STORAGE_MEMORY );

    return smcRecord::getVarColumnLen( aColumn, (const SChar*)aRow );

}

/* FOR A4 : Cursor ���� �Լ��� */
smiRange * smiGetDefaultKeyRange( )
{
    return (smiRange*)smiTableCursor::getDefaultKeyRange();
}

smiCallBack * smiGetDefaultFilter( )
{
    return (smiCallBack*)smiTableCursor::getDefaultFilter();
}


/* -----------------------
   For Global Transaction
   ----------------------- */
IDE_RC smiXaRecover( SInt           *a_slotID,
                     /* BUG-18981 */
                     ID_XID       *a_xid,
                     timeval        *a_preparedTime,
                     smiCommitState *a_state )
{

    return smxTransMgr::recover( a_slotID,
                                 a_xid,
                                 a_preparedTime,
                                 (smxCommitState *)a_state );

}

/* -----------------------
   For Global Transaction
   ----------------------- */
/***********************************************************************
 * Description : checkpoint�����带 ���� checkpoint�� �����Ѵ�.
 *               ����� �Է¹��� aStart�� True�ΰ�� Turn off ������
 *               Flusher���� ����� �����Ѵ�.

 *                                                                       
 * aStatistics   - [IN] None                                             
 * a_pTrans      - [IN] Transaction Pointer                              
 * aStart        - [IN] Turn Off������ Flusher���� ������ �ϴ��� ���� 
 ***********************************************************************/
IDE_RC smiCheckPoint( idvSQL   * aStatistics,
                      idBool     aStart )
{
    if ( aStart == ID_TRUE )
    {
        IDE_TEST( sdsFlushMgr::turnOnAllflushers() != IDE_SUCCESS );
        IDE_TEST( sdbFlushMgr::turnOnAllflushers() != IDE_SUCCESS );
    }
    else
    {
      // Nothing To do
    }
  
    IDE_TEST( gSmrChkptThread.resumeAndWait( aStatistics )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC _smiSetAger( idBool aValue )
{

    static idBool sValue = ID_TRUE;

    if ( aValue != sValue )
    {
        if ( aValue == ID_TRUE )
        {
            IDE_TEST( smaLogicalAger::unblock() != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( smaLogicalAger::block() != IDE_SUCCESS );
        }
        sValue = aValue;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

#define MSG_BUFFER_SIZE (128 * 1024)

void smiGetTxLockInfo( smiTrans *aTrans, smTID *aOwnerList, UInt *aOwnerCount )
{
    smxTrans *sTrans;

    //for fix Bug PR-5827
    *aOwnerCount=0;
    sTrans = (smxTrans*)aTrans->getTrans();
    if ( sTrans != NULL )
    {
        smlLockMgr::getTxLockInfo( sTrans->mSlotN, aOwnerList, aOwnerCount );
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : [PROJ-1362] Table�� Type���� LOB Column Piece��
 *               ũ�⸦ ��ȯ�Ѵ�.
 *               Disk�� ��� idBool�� NULL�� ���θ� ��ȯ�Ѵ�.
 *               �̸� �����Ϸ��� mtdClob, mtdBlob��
 *               mtdStoredValue2MtdValu() �� ���� ó���ؾ� �Ѵ�.
 *
 *    aTableType  - [IN] LOB Column�� Table�� Type
 **********************************************************************/
UInt smiGetLobColumnSize(UInt aTableType)
{
    UInt sTableType = aTableType & SMI_TABLE_TYPE_MASK;
    
    if ( sTableType == SMI_TABLE_DISK )
    {
        return idlOS::align8(ID_SIZEOF(sdcLobDesc));
    }
    else
    {
        return idlOS::align8(ID_SIZEOF(smcLobDesc));
    }
    
}

/***********************************************************************
 * Description : [PROJ-1362] LOB Column�� Null ( length == 0 )����
 *               ������ ��ȯ�Ѵ�.
 *
 *    aRow       - [IN] Fetch�ؼ� ���� LOB Column Data
 *                      Memory�� ��� LOB Descriptor�� ����ְ�
 *                      Disk�� ��� Lob Column�� ũ�Ⱑ ����ִ�.
 *    aColumn    - [IN] LOB Column ����
 **********************************************************************/
idBool smiIsNullLobColumn( const void*       aRow,
                           const smiColumn * aColumn )
{
    SChar       * sRow;
    sdcLobDesc    sDLobDesc;
    smcLobDesc  * sMLobDesc;
    idBool        sIsNullLob;

    sRow = (SChar*)aRow + aColumn->offset;

    if ((aColumn->flag & SMI_COLUMN_STORAGE_MASK) == SMI_COLUMN_STORAGE_DISK)
    {
        idlOS::memcpy( &sDLobDesc, sRow, ID_SIZEOF(sdcLobDesc) );

        if ( (sDLobDesc.mLobDescFlag & SDC_LOB_DESC_NULL_MASK) ==
             SDC_LOB_DESC_NULL_TRUE )
        {
            sIsNullLob = ID_TRUE;
        }
        else
        {
            sIsNullLob = ID_FALSE;
        }
    }
    else
    {
        sMLobDesc = (smcLobDesc*)sRow;
        if ( (sMLobDesc->flag & SM_VCDESC_NULL_LOB_MASK) ==
             SM_VCDESC_NULL_LOB_TRUE )
        {
            sIsNullLob = ID_TRUE;
        }
        else
        {
            sIsNullLob = ID_FALSE;
        }
    }

    return sIsNullLob;
}

//  For A4 : TableSpace type���� Maximum fixed row size�� ��ȯ�Ѵ�.
//  slot header ����.
UInt smiGetMaxFixedRowSize( smiTableSpaceType aTblSpaceType )
{
    if ( (aTblSpaceType == SMI_MEMORY_SYSTEM_DICTIONARY) ||
         (aTblSpaceType == SMI_MEMORY_SYSTEM_DATA) ||
         (aTblSpaceType == SMI_MEMORY_USER_DATA) )
    {
        return SMP_MAX_FIXED_ROW_SIZE;
    }
    else
    {
        IDE_ASSERT(0);
    }
}

/*
    SMI Layer�� Tablespace Lock Mode�� Exclusive Lock���� ���� Ȯ��

    [IN] aLockMode - SMI Layer�� Lock Mode
 */
idBool isExclusiveTBSLock( smiTBSLockMode aLockMode )
{
    idBool sIsExclusive ;


    switch ( aLockMode )
    {
        case SMI_TBS_LOCK_EXCLUSIVE :
            sIsExclusive = ID_TRUE;
            break;

        case SMI_TBS_LOCK_SHARED :
            sIsExclusive = ID_FALSE;
            break;
        default:
            // ���� �ΰ��� ���� �ϳ����� �Ѵ�.
            IDE_ASSERT(0);
    }

    return sIsExclusive;
}

/* ���̺� �����̽��� ���� Lock�� ȹ���ϰ� Validation�� �����Ѵ�.

   [IN]  aStmt         : Statement�� void* ��
   [IN]  scSpaceID     : Lock�� ȹ���� Tablespace��  ID
   [IN]  aTBSLockMode  : Tablespace�� Lock Mode
   [IN]  aTBSLvType    : Tablespace ���� Validation�ɼ�
   [IN]  aLockWaitMicroSec : ��ݿ�û�� Wait �ð�
 */
IDE_RC smiValidateAndLockTBS( smiStatement        * aStmt,
                              scSpaceID             aSpaceID,
                              smiTBSLockMode        aLockMode,
                              smiTBSLockValidType   aTBSLvType,
                              ULong                 aLockWaitMicroSec )
{
    smiTrans *sTrans;
    idBool    sIsExclusiveLock;

    IDE_ASSERT( aStmt != NULL );
    IDE_ASSERT( aLockMode  != SMI_TBS_LOCK_NONE );
    IDE_ASSERT( aTBSLvType != SMI_TBSLV_NONE );

    sTrans = aStmt->getTrans();

    sIsExclusiveLock = isExclusiveTBSLock( aLockMode );

    IDE_TEST_RAISE( sctTableSpaceMgr::lockAndValidateTBS(
                                    (void*)(sTrans->getTrans()),
                                    aSpaceID,
                                    sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ),
                                    ID_FALSE,    /* Is Intent */
                                    sIsExclusiveLock,
                                    aLockWaitMicroSec )
                    != IDE_SUCCESS, ERR_TBS_LOCK_VALIDATE );

    /* BUG-18279: Drop Table Space�ÿ� ������ Table�� ���߸���
     *            Drop�� ����˴ϴ�.
     *
     * Tn: Transaction n, TBS = Tablespace
     * 1. TBS�� Drop�� �����ϴ� Transaction�� T1�� ViewSCN�� ����.
     * 2. DDL(Create Table, Drop, Alter)�� T2�� �����Ѵ�. �׸��� Commit�Ѵ�.
     * 3. T1�� Drop Tablespace�� �����ϴµ� T2�� ������ ����� ���� ���Ѵ�.
     *    �ֳ��ϸ� T2�� commit�ϱ����� ViewSCN�� ���⶧���̴�.
     *
     * �� �����ذ��� ���ؼ� Tablespace�� DDLCommitSCN�� �ΰ� Table�� ���� DDL����
     * �ڽ��� Commit SCN�� �����ϵ��� �Ͽ���. �׸��� Tablespace�� ���ؼ�
     * Drop�� �����Ҷ����� �ڽ��� ViewSCN�� Tablespace�� DDLCommitSCN����
     * ū���� �˻��ϰ� ������ Statement Rebuild Error�� ����.
     */
    if ( sIsExclusiveLock == ID_TRUE )
    {
        IDE_TEST( sctTableSpaceMgr::canDropByViewSCN( aSpaceID,
                                                      aStmt->getSCN() )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TBS_LOCK_VALIDATE );
    {
        /* Tablespace�� �߰ߵ��� ���� ���
           Tablespace�� Drop�Ǿ��ٰ�
           ���� �̸����� �ٽ� ������ ����� �� �ִ�.

           �� ��� Rebuild Error�� �÷��� QP���� Validation�� �ٽ�
           �����Ͽ� ���ο� Tablespace ID�� �ٽ� ����ǵ��� �����Ѵ�.
        */

        if ( ( ideGetErrorCode() == smERR_ABORT_NotFoundTableSpaceNode ) ||
             ( ideGetErrorCode() == smERR_ABORT_NotFoundTableSpaceNodeByName ) )
        {
            IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTBSModified ) );
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
   PRJ-1548 User Memory Tablespace

   ���̺� ���� ��ȿ�� �˻� �� ��� ȹ��

   ���̺�� ���õ� ��� ���̺����̽�(����Ÿ, �ε���)�� ���Ͽ�
   ����� ������ ���� ȹ���Ѵ�.

   fix BUG-17121
   �ݵ��, ���̺����̽� -> ���̺� -> Index,Lob ���̺����̽�
   ������ ����� ȹ���Ѵ�.

   ���������� ��Ÿ ���̺�� �ý��� ���̺����̽��� ����� ȹ������ �ʴ´�.

   A. ���̺� IX �Ǵ� X ����� ȹ���ϱ� ���ؼ���
      ���̺����̽� IX ����� ���� ȹ���ؾ���.

   B. ���̺� IS �Ǵ� S ����� ȹ���ϱ� ���ؼ���
      ���̺����̽� IS ����� ���� ȹ���ؾ���.
 

   BUG-28752 lock table ... in row share mode ������ ������ �ʽ��ϴ�.

   implicit/explicit lock�� �����Ͽ� �̴ϴ�.
   implicit is lock�� statement end�� Ǯ���ֱ� �����Դϴ�. */


IDE_RC smiValidateAndLockObjects( smiTrans           * aTrans,
                                  const void         * aTable,
                                  smSCN                aSCN,
                                  smiTBSLockValidType  aTBSLvType,
                                  smiTableLockMode     aLockMode,
                                  ULong                aLockWaitMicroSec,
                                  idBool               aIsExplicitLock )
{
    scSpaceID             sSpaceID;
    smcTableHeader      * sTable;
    idBool                sLocked;
    smlLockNode         * sCurLockNodePtr = NULL;
    smlLockSlot         * sCurLockSlotPtr = NULL;
    smiTableLockMode      sTableLockMode;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );

    // Lock Table
    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    sSpaceID = smcTable::getTableSpaceID( (void*)sTable );

    IDU_FIT_POINT_RAISE( "smiValidateAndLockObjects::ERR_TBS_LOCK_VALIDATE", ERR_TBS_LOCK_VALIDATE ); 

    // ���̺��� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
    IDE_TEST_RAISE( sctTableSpaceMgr::lockAndValidateTBS(
                                     (void*)((smxTrans*)aTrans->getTrans()), /* smxTrans* */
                                     sSpaceID,          /* LOCK ȹ���� TBSID */
                                     sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ),/* TBS Lock Validation Type */
                                     ID_TRUE,           /* intent lock  ���� */
                                     smlLockMgr::isNotISorS((smlLockMode)aLockMode),
                                     aLockWaitMicroSec ) != IDE_SUCCESS,
                    ERR_TBS_LOCK_VALIDATE );

    // PRJ-1476
    IDE_TEST_CONT( SMI_TABLE_TYPE_IS_META( sTable ) == ID_TRUE,
                   skip_lock_meta_table);

    // BUG-14113
    // Isolation Level�� �´� table lock mode�� ����
    sTableLockMode = smiTableLockModeOnIsolationLevel[(UInt)aTrans->getIsolationLevel()]
                                                     [(UInt)aLockMode];

    /* BUG-32237 [sm_transaction] Free lock node when dropping table.
     * DropTablePending ���� �����ص� freeLockNode�� �����մϴ�. */
    /* �̹� Drop�� ���, rebuild �ϸ� ���� ����.
     * QP���� ���ſ� ����ص� TableOID�� ���� �Ѿƿ� �� �ֱ� ������
     * ���� ��Ȳ. */
    IDE_TEST( smiValidateObjects( aTable, aSCN ) != IDE_SUCCESS );

    // ���̺� ���Ͽ� ����� ȹ���Ѵ�.
    IDE_TEST( smlLockMgr::lockTable( ((smxTrans*)aTrans->getTrans())->mSlotN,
                                     (smlLockItem *)SMC_TABLE_LOCK( sTable ),
                                     (smlLockMode)sTableLockMode,
                                     aLockWaitMicroSec,
                                     NULL,
                                     &sLocked,
                                     &sCurLockNodePtr,
                                     &sCurLockSlotPtr,
                                     aIsExplicitLock ) != IDE_SUCCESS );

    IDE_TEST( sLocked == ID_FALSE );

    IDU_FIT_POINT("smiValidateAndLockObjects::_FT_");

    IDE_TEST( smiValidateObjects( aTable, aSCN ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT(skip_lock_meta_table);

    // ���̺��� Index, Lob �÷� ���� ���̺����̽��鿡 ���Ͽ�
    // INTENTION ����� ȹ���Ѵ�.
    IDE_TEST( sctTableSpaceMgr::lockAndValidateRelTBSs(
                                     (void*)((smxTrans*)aTrans->getTrans()), /* smxTrans* */
                                     (void*)sTable,                          /* smcTableHeader */
                                     sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ),/* TBS Lock Validation Type */
                                     ID_TRUE,                                /* intent lock  ���� */
                                     smlLockMgr::isNotISorS((smlLockMode)aLockMode),
                                     aLockWaitMicroSec ) != IDE_SUCCESS );

    return IDE_SUCCESS;


    IDE_EXCEPTION( ERR_TBS_LOCK_VALIDATE );
    {
        if ( (ideGetErrorCode() == smERR_ABORT_NotFoundTableSpaceNode ) ||
             (ideGetErrorCode() == smERR_ABORT_NotFoundTableSpaceNodeByName ) )
        {
            IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTBSModified ) );
        }
    }

    IDE_EXCEPTION_END;

    if ( sCurLockSlotPtr != NULL )
    {
        (void)smlLockMgr::unlockTable( ((smxTrans*)aTrans->getTrans())->mSlotN,
                                       sCurLockNodePtr,
                                       sCurLockSlotPtr );
    }

    if ( ideGetErrorCode() == smERR_REBUILD_smiTableModified )
    {
        SMX_INC_SESSION_STATISTIC( aTrans->mTrans,
                                   IDV_STAT_INDEX_STMT_REBUILD_COUNT,
                                   1 /* Increase Value */ );
    }

    return IDE_FAILURE;
}


/*
   ���̺� ���� ��ȿ�� �˻�
*/
IDE_RC smiValidateObjects( const void         * aTable,
                           smSCN                aCachedSCN )
{
    smcTableHeader      * sTable;
    smSCN sSCN;
    smTID sDummyTID;

    IDE_DASSERT( aTable != NULL );

    SMX_GET_SCN_AND_TID( ((smpSlotHeader*)aTable)->mCreateSCN, sSCN, sDummyTID );
    
    IDU_FIT_POINT_RAISE( "smiValidateObjects::ERR_TABLE_MODIFIED", ERR_TABLE_MODIFIED );

    IDE_TEST_RAISE ( SM_SCN_IS_DELETED(sSCN), ERR_TABLE_MODIFIED );

    sTable = (smcTableHeader*)( (UChar*)aTable + SMP_SLOT_HEADER_SIZE );
    IDE_TEST_RAISE( smcTable::isDropedTable( sTable ) == ID_TRUE,
                    ERR_TABLE_MODIFIED );

    /* PROJ-2268 Table�� SCN�� cached SCN�� ���� �ʴٸ� ��Ȱ��� Slot�̴�. */
    IDE_TEST_RAISE( SM_SCN_IS_EQ( &sSCN, &aCachedSCN ) != ID_TRUE,
                    ERR_TABLE_MODIFIED );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TABLE_MODIFIED );
    {
        IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTableModified ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2268 Reuse Catalog Table Slot
 * Plan�� ��ϵǾ� �ִ� Slot�� ����Ǿ����� Ȯ���Ѵ�. */
IDE_RC smiValidatePlanHandle( const void * aHandle,
                              smSCN        aCachedSCN )
{
    /* slot�� ��Ȱ����� �ʴ´ٸ� validation �� �ʿ䰡 ����. */
    if ( smuProperty::getCatalogSlotReusable() == 1 )
    {
        IDE_TEST( smiValidateObjects( aHandle, aCachedSCN )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smiGetGRIDFromRowPtr( const void   * aRowPtr,
                             scGRID       * aRowGRID )
{
    smpPersPageHeader   * sPageHdrPtr;
    void                * sTableHeader;

    IDE_DASSERT( aRowPtr != NULL );
    IDE_DASSERT( aRowGRID != NULL );

    SC_MAKE_NULL_GRID( *aRowGRID );

    sPageHdrPtr = SMP_GET_PERS_PAGE_HEADER( aRowPtr );

    IDE_TEST( smcTable::getTableHeaderFromOID( sPageHdrPtr->mTableOID,
                                               &sTableHeader )
              != IDE_SUCCESS );

    SC_MAKE_GRID( *aRowGRID,
                  smcTable::getTableSpaceID( sTableHeader ),
                  SMP_SLOT_GET_PID( aRowPtr ),
                  SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowPtr ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smiFetchRowFromGRID( idvSQL                      * aStatistics,
                            smiStatement                * aStatement,
                            UInt                          aTableType,
                            scGRID                        aRowGRID,
                            const smiFetchColumnList    * aFetchColumnList,
                            void                        * aTableHdr,
                            void                        * aDestRowBuf )
{
    UChar                * sSlotPtr;
    idBool                 sIsSuccess;
    smxTrans             * sTrans;
    sdSID                  sTSSlotSID;
    sdpPageType            sPageType;
    smSCN                  sSCN;
    idBool                 sIsRowDeleted;
    smSCN                  sInfiniteSCN;
    idBool                 sIsPageLatchReleased = ID_TRUE;
    const smcTableHeader * sTableHeader;
    idBool                 sSkipFetch = ID_FALSE;   /* BUG-43844 */

    SM_MAX_SCN( &sInfiniteSCN );
    SM_MAX_SCN( &sSCN );

    sTrans = (smxTrans*)aStatement->getTrans()->getTrans();
    sTSSlotSID = smxTrans::getTSSlotSID(sTrans);
    sTableHeader = SMI_MISC_TABLE_HEADER( aTableHdr );

    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aRowGRID) );
    IDE_ASSERT( aTableType == SMI_TABLE_DISK );

    /* BUG-43801 : disk�� ��� slotnum���� ȣ�� */ 
    IDE_TEST_RAISE( SC_GRID_IS_NOT_WITH_SLOTNUM( aRowGRID ), error_invalid_grid );

    IDE_TEST( sdbBufferMgr::getPageByGRID( aStatistics,
                                           aRowGRID,
                                           SDB_S_LATCH,
                                           SDB_WAIT_NORMAL,
                                           NULL, /* aMtx */
                                           (UChar**)&sSlotPtr,
                                           &sIsSuccess,
                                           &sSkipFetch )
              != IDE_SUCCESS );
    sIsPageLatchReleased = ID_FALSE;

    /* BUG-43844 : �̹� free�� ����(unused �÷��� üũ��)�� �����Ͽ� fetch��
     * �õ� �� ��� ��ŵ */
    if ( sSkipFetch == ID_TRUE )
    {
        IDE_CONT( SKIP_FETCH );
    }
    else
    {
        /* do nothing */    
    }

    sPageType = sdpPhyPage::getPageType( sdpPhyPage::getHdr(sSlotPtr) );
    IDE_ASSERT(sPageType == SDP_PAGE_DATA);

    sSCN         = aStatement->getSCN();
    
    /* ���� statement�� ���� cursor�� INSERT�� row�� ���� ���ϴ� ����������
     * Transaction���κ��� infiniteSCN�� ���������� �Ǿ� �־����� (BUG-33889)
     * closeCursor()���� infiniteSCN�� ���� ��Ű���� �����Ǿ���.(BUG-32963)
     * BUG-43801 ���� ������ �߻��ϹǷ� statement���� infiniteSCN�� ���������� ������.*/
    sInfiniteSCN = aStatement->getInfiniteSCN();

    IDU_FIT_POINT( "smiFetchRowFromGRID::fetch" );

    IDE_TEST( sdcRow::fetch( aStatistics,
                             NULL, /* aMtx */
                             NULL, /* aSP */
                             sTrans,
                             SC_MAKE_SPACE(aRowGRID),
                             sSlotPtr,
                             ID_TRUE, /* aIsPersSlot */
                             SDB_SINGLE_PAGE_READ,
                             aFetchColumnList,
                             SMI_FETCH_VERSION_CONSISTENT,
                             sTSSlotSID,
                             &sSCN,
                             &sInfiniteSCN,
                             NULL, /* aIndexInfo4Fetch */
                             NULL, /* aLobInfo4Fetch */
                             sTableHeader->mRowTemplate,
                             (UChar*)aDestRowBuf,
                             &sIsRowDeleted,
                             &sIsPageLatchReleased )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsRowDeleted == ID_TRUE, error_deletedrow );

    IDE_EXCEPTION_CONT( SKIP_FETCH );

    /* BUG-23319
     * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
    /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
     * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
     * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
     * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
     * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
    if ( sIsPageLatchReleased == ID_FALSE )
    {
        sIsPageLatchReleased = ID_TRUE;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             sSlotPtr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    /* BUG-33889  [sm_transaction] The fetching by RID uses incorrect
     * ViewSCN. */
    IDE_EXCEPTION( error_deletedrow );
    {
        ideLog::log( IDE_SM_0, 
                     "InternalError[%s:%"ID_INT32_FMT"]\n"
                     "GRID     : <%"ID_UINT32_FMT",%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n"
                     "TSSRID   : %"ID_UINT64_FMT"\n"
                     "SCN      : 0X%"ID_UINT64_FMT", 0X%"ID_XINT64_FMT"\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__,
                     SC_MAKE_SPACE( aRowGRID ),
                     SC_MAKE_PID( aRowGRID ),
                     SC_MAKE_SLOTNUM( aRowGRID ),
                     sTSSlotSID,
                     SM_SCN_TO_LONG(sSCN),
                     SM_SCN_TO_LONG(sInfiniteSCN) );
        ideLog::logCallStack( IDE_SM_0 );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, 
                                  "Disk Fetch by RID" ) );
        
    }
    IDE_EXCEPTION( error_invalid_grid )
    {
        ideLog::log( IDE_SM_0,
                     "InternalError[%s:%d]\n"
                     "GRID     : <%"ID_UINT32_FMT",%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n"
                     "TSSRID   : %"ID_UINT64_FMT"\n"
                     "SCN      : 0X%"ID_UINT64_FMT", 0X%"ID_XINT64_FMT"\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__,
                     SC_MAKE_SPACE( aRowGRID ),
                     SC_MAKE_PID( aRowGRID ),
                     SC_MAKE_OFFSET( aRowGRID ),
                     sTSSlotSID,
                     SM_SCN_TO_LONG(sSCN),
                     SM_SCN_TO_LONG(sInfiniteSCN) );
        ideLog::logCallStack( IDE_SM_0 );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG,
                                  "Disk Fetch by Invalid GRID " ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               sSlotPtr )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/*
  FOR A4 :
      TableSpace���� Size�� �˾Ƴ��� ���� id ���ڸ� �߰�
*/

// smiAPI function
// callback for gSmiGlobalBackList.XXXEmergencyFunc.
void smiSetEmergency(idBool aFlag)
{
    if ( aFlag == ID_TRUE )
    {
        gSmiGlobalCallBackList.setEmergencyFunc(SMI_EMERGENCY_DB_SET);
    }
    else
    {
        gSmiGlobalCallBackList.clrEmergencyFunc(SMI_EMERGENCY_DB_CLR);
    }

}

void smiSwitchDDL(UInt aFlag)
{
    gSmiGlobalCallBackList.switchDDLFunc(aFlag);
}

UInt smiGetCurrTime()
{
    return gSmiGlobalCallBackList.getCurrTimeFunc();
}

/* ------------------------------------------------
 * for replication RPI function
 * ----------------------------------------------*/
void smiSetCallbackFunction(
                smGetMinSN                   aGetMinSNFunc,
                smIsReplCompleteBeforeCommit aIsReplCompleteBeforeCommitFunc,
                smIsReplCompleteAfterCommit  aIsReplCompleteAfterCommitFunc,
                smCopyToRPLogBuf             aCopyToRPLogBufFunc,
                smSendXLog                   aSendXLogFunc,
                smIsReplWaitGlobalTxAfterPrepare aIsReplWaitGlobalTxAfterPrepare )
{
    smrRecoveryMgr::setCallbackFunction( aGetMinSNFunc,
                                         aIsReplCompleteBeforeCommitFunc,
                                         aIsReplCompleteAfterCommitFunc,
                                         aCopyToRPLogBufFunc,
                                         aSendXLogFunc,
                                         aIsReplWaitGlobalTxAfterPrepare );
}

void smiGetLstDeleteLogFileNo( UInt *aFileNo )
{
    smrRecoveryMgr::getLstDeleteLogFileNo( aFileNo );
}

smSN smiGetReplRecoverySN()
{
    return SM_MAKE_SN( smrRecoveryMgr::getReplRecoveryLSN() );
}

IDE_RC smiSetReplRecoverySN( smSN aReplRecoverySN )
{
    smLSN sLSN;
    SM_MAKE_LSN( sLSN, aReplRecoverySN );
    return smrRecoveryMgr::setReplRecoveryLSN( sLSN );
}

IDE_RC smiGetFirstNeedLFN( smSN         aMinSN,
                           const UInt * aFirstFileNo,
                           const UInt * aEndFileNo,
                           UInt       * aNeedFirstFileNo )
{
    smLSN   sMinLSN;
    SM_MAKE_LSN( sMinLSN, aMinSN );
    return smrLogMgr::getFirstNeedLFN( sMinLSN,
                                       *aFirstFileNo,
                                       *aEndFileNo,
                                       aNeedFirstFileNo );
}

// SM�� �����ϴ� �ý����� ��������� �ݿ��Ѵ�.
// �� �Լ������� SM ������ �� ��⿡�� �ý��ۿ� �ݿ��Ǿ�� �ϴ�
// ��������� v$sysstat���� �ݿ��ϵ��� �ϸ� �ȴ�.
void smiApplyStatisticsForSystem()
{
    /*
     * TASK-2356 ��ǰ�����м�
     *
     * ALTIBASE WAIT INTERFACE
     * WaitEvent ���ð� ������� ����
     *
     * System Thread���� ���ð� ���������
     * Session/Statement Level�� �ƴ�
     * System Level ��������� �ֱ������� �����ȴ�.
     *
     * ( Call By mmtSessionManager::checkSessionTimeout() )
     *
     * Buffer Manager���� ������ ���ð� ��������� ��������
     * ������, �Ϲ����� ���� ���� ��������� �����Ѵ�.
     *
     */

    if ( smiGetStartupPhase() > SMI_STARTUP_CONTROL )
    {
        // Buffer Manager�� ��� ������ �ݿ��Ѵ�.
        sdbBufferMgr::applyStatisticsForSystem();

        gSmrChkptThread.applyStatisticsForSystem();
    }
}

/***********************************************************************
 * BUG-35392
 * Description : Fast Unlock Log Alloc Mutex ����� ����������� ����
 *
 **********************************************************************/
idBool smiIsFastUnlockLogAllocMutex()
{
    return smuProperty::isFastUnlockLogAllocMutex();
}

/***********************************************************************

 * Description : ���������� Log�� ����ϱ� ���� "����/�����" LSN���� �����Ѵ�.
                 smiGetLastValidLSN �� ���� �۾��� 
 * -----------------------------------------------------
 *  LSN    | 100 | 101 | 102   | 103 | 104   | 105 | 106 |
 *  Status | ok  | ok  | dummy | ok  | dummy | ok  | ok  |
 * ------------- A ------------------------------- B --
 *
 * dummy �α׸� ������ �ִ� LSN (B) : ���� ��
 * aUncompletedLSN (A) : ���������� valid �� �α׸� ������ ����
 *  FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� A�� ��ȯ�� 
 * aSN  - [OUT] output parameter
 **********************************************************************/
IDE_RC smiGetLastValidGSN( smSN *aSN )
{
   /*
    * PROJ-1923
    */
    smLSN sLSN;
    SM_LSN_MAX(sLSN);

    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� LSN */
        (void)smrLogMgr::getUncompletedLstWriteLSN( &sLSN );
    }
    else
    {
        /* ���������� ����� �α� ���ڵ��� LSN */
        smrLogMgr::getLstWriteLSN( &sLSN );
    }

    *aSN = SM_MAKE_SN( sLSN );

    return IDE_SUCCESS;
}


/***********************************************************************
 * Description : ���������� Log�� ����ϱ� ���� "����/�����" LSN���� �����Ѵ�.
 * ----------------------------------------------------
 *  LSN    | 100 | 101 | 102   | 103 | 104   | 105 | 106 |
 *  Status | ok  | ok  | dummy | ok  | dummy | ok  | ok  |
 * ------------- A ------------------------------- B --
 *
 * dummy �α׸� ������ �ִ� LSN (B) : ���� ��
 * aUncompletedLSN (A) : ���������� valid �� �α׸� ������ ����
 *  FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� A�� ��ȯ�� 
 * aLSN  - [OUT] output parameter
 **********************************************************************/
IDE_RC smiGetLastValidLSN( smLSN *aLSN )
{
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� LSN */
        (void)smrLogMgr::getUncompletedLstWriteLSN( aLSN );
    }
    else
    {
        /* ���������� ����� �α� ���ڵ��� LSN */
        smrLogMgr::getLstWriteLSN( aLSN );
    }

    return IDE_SUCCESS;
}


/***********************************************************************
 * BUG-43426
 *
 *   LSN 106���� ������ �������� �� �Լ��� ȣ��Ǿ���,
 *   FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ���
 *   106�������� dummy �αװ� ����αװ� �ɶ����� WAIT�Ͽ��ٰ�
 *   106�� �����ϰ� �ȴ�.
 *
 * -----------------------------------------------------
 *  LSN    | 100 | 101 | 102   | 103 | 104   | 105 | 106 |
 *  Status | ok  | ok  | dummy | ok  | dummy | ok  | ok  |
 * ------------- A ------------------------------- B --
 *
 * dummy �α׸� ������ �ִ� LSN (B) : ���� ��
 * aUncompletedLSN (A) : ���������� valid �� �α׸� ������ ����
 * (B) �� (A) �� �������°��� ��� ��. 
 **********************************************************************/
IDE_RC smiWaitAndGetLastValidGSN( smSN *aSN )
{
    smLSN           sLstWriteLSN;
    smLSN           sUncompletedLstWriteLSN;
    UInt            sIntervalUSec  = smuProperty::getUCLSNChkThrInterval();
    PDL_Time_Value  sTimeOut;

    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        smrLogMgr::getLstWriteLSN( &sLstWriteLSN );
        smrLogMgr::getUncompletedLstWriteLSN( &sUncompletedLstWriteLSN );

        while ( smrCompareLSN::isGT( &sLstWriteLSN, &sUncompletedLstWriteLSN ) )
        {
            /* UCLSNChk �������
               smuProperty::getUCLSNChkThrInterval() �ֱ��
               MinUncompletedSN ���� �����Ѵ�.
               ���⼭�� �̰��� �����ϰ� WAIT�Ѵ�. */
            sTimeOut.set( 0, sIntervalUSec );
            idlOS::sleep( sTimeOut );

            smrLogMgr::getUncompletedLstWriteLSN( &sUncompletedLstWriteLSN );
        }
    }
    else
    {
        smrLogMgr::getLstWriteLSN( &sLstWriteLSN );
    }

    *aSN = SM_MAKE_SN( sLstWriteLSN );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : ���������� Log�� ����ϱ� ���� ���� LSN���� �����Ѵ�.
 *               FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���̸� ������ �α� ���ڵ��� LSN
 *               (smiGetLastUsedLSN �� ���� �۾���)
 **********************************************************************/
IDE_RC smiGetLastUsedGSN( smSN *aSN )
{
    smLSN sLSN;
    SM_LSN_MAX(sLSN);
    
    smrLogMgr::getLstWriteLSN( &sLSN );
    *aSN = SM_MAKE_SN( sLSN );

    return IDE_SUCCESS;
}

#if 0
/***********************************************************************
 * Description : ���������� Log�� ����ϱ� ���� ���� LSN���� �����Ѵ�.
 *               FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���̸� ������ �α� ���ڵ��� LSN
 **********************************************************************/
IDE_RC smiGetLastUsedLSN( smLSN *aLSN )
{
    smrLogMgr::getLstWriteLSN( aLSN );

    return IDE_SUCCESS;
}
#endif

/***********************************************************************
 * Description : PROJ-1969
 *               ���������� ����� �α� ���ڵ��� "Last Offset"�� ��ȯ�Ѵ�.  
 **********************************************************************/
IDE_RC smiGetLstLSN( smLSN      * aEndLSN )
{
    smrLogMgr::getLstLSN( aEndLSN );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Transaction Table Size�� Return�Ѵ�.
 *
 **********************************************************************/
UInt smiGetTransTblSize()
{
    // BUG-27598 TRANSACTION_TABLE_SIZE �� SM �� RP ��
    // ���� �ٸ��� �ؼ��Ͽ� ����ȭ�� ������ ������ �� ����.
    return smxTransMgr::getCurTransCnt();
}

/***********************************************************************
 * Description :
 *
 **********************************************************************/
const SChar * smiGetArchiveDirPath()
{
    return smuProperty::getArchiveDirPath();
}

smiArchiveMode smiGetArchiveMode()
{
    return smrRecoveryMgr::getArchiveMode();
}

/***********************************************************************

 Description :

 SM�� �� tablespace�� ������ �����Ѵ�.

 ALTER SYSTEM VERIFY �� ���� ���� SQL�� ����ڰ� ó���Ѵٸ�
 QP���� ���� �Ľ���  �� �Լ��� ȣ���Ѵ�.

 ����� SMI_VERIFY_TBS �� ����������, ���Ŀ� ����� �߰��� �� �ִ�.

 ȣ�� ��) smiVerifySM(SMI_VERIFY_TBS)

 Implementation :

 wihle(��� tablespace)
     tablespace verify

**********************************************************************/
IDE_RC smiVerifySM( idvSQL  * aStatistics,
                    UInt      aFlag)
{

    sctTableSpaceNode  *sCurrSpaceNode;
    sctTableSpaceNode  *sNextSpaceNode;

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        if ( (sctTableSpaceMgr::isMemTableSpace( sCurrSpaceNode )  == ID_TRUE) ||
             (sctTableSpaceMgr::isVolatileTableSpace( sCurrSpaceNode ) == ID_TRUE) )
        {
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode )
                     == ID_TRUE );

        if ( aFlag & SMI_VERIFY_TBS )
        {
            // fix BUG-17377
            // ENABLE_RECOVERY_TEST = 1 �� ��쿡�� �����.
            if ( sctTableSpaceMgr::hasState( sCurrSpaceNode,
                                             SCT_SS_SKIP_IDENTIFY_DB )
                 == ID_FALSE )
            {
                // Verify ������.
                IDE_TEST( sdpTableSpace::verify( aStatistics,
                                                 sCurrSpaceNode->mID,
                                                 aFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                // DISCARDED/DROPPED �� TBS�� ���� Verify�� �������� ����.
            }
        }
        else
        {
            IDE_DASSERT(ID_FALSE);
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiUpdateIndexModule(void *aIndexModule)
{
    return smnManager::updateIndexModule((smnIndexModule *)aIndexModule);
}

/*
 * Enhancement BUG-15010
 * [����������] ���� Ʃ�׽� buffer pool�� initialize�ϴ� ����� ������ �ʿ��մϴ�.
 *
 * �� �������̽� ȣ��ÿ� ��� buffer pool�� �����ϴ� ��� �������� FREE LIST��
 * ��ȯ�ϹǷ�, ��� ȣ��� ��� 100% buffer miss�� �߻��� �� �ִ�.
 *
 * SYSDBA ���� ȹ���� �ʿ��ϴ�.
 *
 * iSQL> alter system flush buffer_pool;
 *
 * [ ���� ]
 *
 * aStatistics - �������
 *
 */
IDE_RC smiFlushBufferPool( idvSQL * aStatistics )
{
    IDE_ASSERT( aStatistics != NULL );

    ideLog::log( SM_TRC_LOG_LEVEL_INTERFACE,
                 SM_TRC_BUFFER_FLUSH_BUFFER_POOL );

    IDE_TEST( sdbBufferMgr::pageOutAll( aStatistics)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( SM_TRC_LOG_LEVEL_INTERFACE,
                 SM_TRC_BUFFER_FLUSH_BUFFER_POOL_FAILURE );

    return IDE_FAILURE;
}

/*
 * iSQL> alter system flush secondary_buffer;
 *
 * [ ���� ]
 *
 * aStatistics - �������
 *
 */
IDE_RC smiFlushSBuffer( idvSQL * aStatistics )
{
    IDE_ASSERT( aStatistics != NULL );

    ideLog::log( SM_TRC_LOG_LEVEL_INTERFACE,
                 SM_TRC_BUFFER_FLUSH_SECONDARY_BUFFER );

    IDE_TEST( sdsBufferMgr::pageOutAll( aStatistics )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( SM_TRC_LOG_LEVEL_INTERFACE,
                 SM_TRC_BUFFER_FLUSH_SECONDARY_BUFFER_FAILURE );

    return IDE_FAILURE;
}

/* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����

   Table/Index/Sequence��
   Create/Alter/Drop DDL�� ���� Query String�� �α��Ѵ�.
 */
IDE_RC smiWriteDDLStmtTextLog( idvSQL*          aStatistics,
                               smiStatement   * aStmt,
                               smiDDLStmtMeta * aDDLStmtMeta,
                               SChar *          aStmtText,
                               SInt             aStmtTextLen )
{
    IDE_DASSERT( aStmt != NULL );
    IDE_DASSERT( aDDLStmtMeta != NULL );
    IDE_DASSERT( aStmtText != NULL );
    IDE_DASSERT( aStmtTextLen > 0 );

    IDE_TEST( smrLogMgr::writeDDLStmtTextLog( aStatistics,
                                              aStmt->getTrans()->getTrans(),
                                              (smrDDLStmtMeta *)aDDLStmtMeta,
                                              aStmtText,
                                              aStmtTextLen )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : sync�� log file�� ù ��° �α� �� ���� ���� ���� �����Ѵ�.
 *
 * aLSN  - [OUT] output parameter
 **********************************************************************/
IDE_RC smiGetSyncedMinFirstLogSN( smSN   *aSN )
{
    smLSN sLSN;
    IDE_TEST( smrLogMgr::getSyncedMinFirstLogLSN( &sLSN ) != IDE_SUCCESS );
    *aSN = SM_MAKE_SN(sLSN); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  checkpoint ���� ������ ������ ���� ��ȣ�� ���� smSN���� ��ȭ�ؼ� �����Ѵ�.
 *                ���ŵ� ������ ������ ��ġ�� ã�� ���� �����̱� ������ offset�� MAX�� �����Ѵ�.
 * aLSN  - [OUT] output parameter
 **********************************************************************/
void  smiGetRemoveMaxLastLogSN( smSN   *aSN )
{
    UInt  sFileNo = 0;
    smLSN sLSN;
    
    SM_LSN_MAX( sLSN );
    
    smrRecoveryMgr::getLastRemovedFileNo( &sFileNo );
    if ( sFileNo != 0 )
    {
        sLSN.mFileNo = sFileNo;
    }

    *aSN = SM_MAKE_SN(sLSN); 
}

/***********************************************************************
 * Description :
 *
 **********************************************************************/
IDE_RC smiFlusherOnOff( idvSQL  * aStatistics,
                        UInt      aFlusherID,
                        idBool    aStart )
{
    IDE_TEST_RAISE( aFlusherID >= sdbFlushMgr::getFlusherCount(),
                    ERROR_INVALID_FLUSHER_ID );
   
    if ( aStart == ID_TRUE )
    {
        IDE_TEST( sdbFlushMgr::turnOnFlusher(aFlusherID)
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdbFlushMgr::turnOffFlusher(aStatistics, aFlusherID)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_FLUSHER_ID );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiInvalidFlusherID,
                                  aFlusherID,
                                  sdbFlushMgr::getFlusherCount()) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiSBufferFlusherOnOff( idvSQL *aStatistics,
                               UInt    aFlusherID,
                               idBool  aStart )
{ 
    IDE_TEST_RAISE( aFlusherID >= sdbFlushMgr::getFlusherCount(),
                    ERROR_INVALID_FLUSHER_ID );

    if ( aStart == ID_TRUE )
    {
        IDE_TEST( sdsFlushMgr::turnOnFlusher( aFlusherID )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdsFlushMgr::turnOffFlusher( aStatistics, aFlusherID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_FLUSHER_ID );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiInvalidFlusherID,
                                  aFlusherID,
                                  sdbFlushMgr::getFlusherCount() ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// Disk Segment�� �����ϱ� ���� �ּ� Extent ����
UInt smiGetMinExtPageCnt()
{
    return SDP_MIN_EXTENT_PAGE_CNT;
}

/* BUG-20727 */
IDE_RC smiExistPreparedTrans( idBool *aExistFlag )
{
    return smxTransMgr::existPreparedTrans( aExistFlag );
}

/**********************************************************************
 *
 * Description : smxDef.h���� define�� PSM Savepoint Name�� ����
 *
 * BUG-21800 [RP] Receiver���� PSM Savepoint�� �������� �ʾ� Explicit
 *                Savepoint�� ó�� �Ǿ����ϴ�
 **********************************************************************/
SChar * smiGetPsmSvpName()
{
    return (SChar*)SMX_PSM_SAVEPOINT_NAME;
}

/**********************************************************************
 *
 * Description : Ʈ����ǰ����ڿ� Active Ʈ����ǵ��� �ּ� Min ViewSCN��
 *               ���ϵ��� ��û��.
 *
 * BUG-23637 �ּ� ��ũ ViewSCN�� Ʈ����Ƿ������� Statement ������ ���ؾ���.
 *
 * aStatistics  - [IN] statistics
 *
 **********************************************************************/
IDE_RC smiRebuildMinViewSCN( idvSQL  * aStatistics )
{
    return smxTransMgr::rebuildMinViewSCN( aStatistics );
}

/* PROJ-1915 : Log ���� ������ ���� */
ULong smiGetLogFileSize()
{
    return smuProperty::getLogFileSize();
}

/* PROJ-1915 : smVersion üũ */
UInt smiGetSmVersionID()
{
    return smVersionID;
}


/*
 * BUG-27949 [5.3.3] add datafile ����. DDL_LOCK_TIMEOUT ��
 *           �����ϰ� �ֽ��ϴ�. (�ٷκ�������)
 *
 * SM,QP ���� ���������� �����ϴ� DDL_LOCK_TIMEOUT�� sm���� �����ؼ� ������.
 */
/* BUG-47577: Int �� �޾Ƽ� Long�� ������ �ؼ� overflow  �߻���.
 *            SInt -> SLong ����
 */
ULong smiGetDDLLockTimeOut(smiTrans * aTrans)
{
    SInt sDDLLockTimeoutProperty = smuProperty::getDDLLockTimeOut();
    ULong sDDLLockTimeout = 0;
    idvSQL * sStat = NULL;

    if ( aTrans != NULL )
    {
        IDE_ASSERT( aTrans->mTrans != NULL ); /* trans must be begun */
        sStat = ((smxTrans*)(aTrans->mTrans))->mStatistics;
        if (sStat != NULL )
        {
            IDE_ASSERT( sStat->mSess != NULL ); /* mSess is idvSession */
            if ( sStat->mSess->mSession != NULL ) /* mm session */
            {
                sDDLLockTimeoutProperty = gSmiGlobalCallBackList.getDDLLockTimeout(sStat->mSess->mSession);
            }
            else
            {
                /* internal session */
            }
        }
        else
        {
            /* internal transaction */
        }
    }
    else
    {
        /* aTrans is NULL */
    }

    if (sDDLLockTimeoutProperty == -1 )
    {
        sDDLLockTimeout = ID_ULONG_MAX;
    }
    else
    {
        sDDLLockTimeout = sDDLLockTimeoutProperty * 1000000;
    }

    return sDDLLockTimeout;
}

SInt smiGetDDLLockTimeOutProperty()
{
    return smuProperty::getDDLLockTimeOut();
}

IDE_RC smiWriteDummyLog()
{
    return smrLogMgr::writeDummyLog();
}


/*fix BUG-31514 While a dequeue rollback ,
  another dequeue statement which currenlty is waiting for enqueue event  might lost the  event  */
void   smiGetLastSystemSCN( smSCN *aLastSystemSCN )
{
    smSCN sSCN = smmDatabase::getLstSystemSCN();

    SM_GET_SCN(aLastSystemSCN, &sSCN);
}

/**********************************************************************
 * PROJ-2733 �л� Ʈ����� ���ռ�
 **********************************************************************/
IDE_RC smiSetLastSystemSCN( smSCN * aLastSystemSCN, smSCN * aNewLastSystemSCN )
{
   return smmDatabase::setLstSystemSCN( aLastSystemSCN, aNewLastSystemSCN );
}

/**********************************************************************
 * PROJ-2733 �л� Ʈ����� ���ռ�
 **********************************************************************/
void   smiInaccurateGetLastSystemSCN( smSCN *aLastSystemSCN )
{
    /* PROJ-2733 ��Ȯ�� ���ٴ� ���� �϶� ������ */
    smSCN sSCN = smmDatabase::inaccurateGetLstSystemSCN();

    SM_GET_SCN(aLastSystemSCN, &sSCN);
}

/* active transcation �� ���� ���� SN�� �����Ѵ�.
 * ���� NULL�� ��� ���� �α׿� ���� GSN�� �����Ѵ�.
 * log�� ��� ���̴� ��� GSN���� ����� �� �ֱ⶧���� ���� ���� smiGetLastValidGSN�� ȣ���Ѵ�.  */
smSN smiGetValidMinSNOfAllActiveTrans()
{
    smLSN sLSN;
    smSN  sMinSN = SM_SN_NULL;

    IDE_ASSERT( smiGetLastValidGSN( &sMinSN ) == IDE_SUCCESS );
    smxTransMgr::getMinLSNOfAllActiveTrans( &sLSN );
        
    if ( SM_MAKE_SN( sLSN ) < sMinSN )
    {
        sMinSN = SM_MAKE_SN( sLSN );
    }

    return sMinSN;
}

/***********************************************************************
 * Description : Retry Info�� �ʱ�ȭ�Ѵ�.

 *   aRetryInfo - [IN] �ʱ�ȭ �� Retry Info
 ***********************************************************************/
void smiInitDMLRetryInfo( smiDMLRetryInfo * aRetryInfo )
{
    IDE_DASSERT( aRetryInfo != NULL );

    if ( aRetryInfo != NULL )
    {
        aRetryInfo->mIsWithoutRetry  = ID_FALSE;
        aRetryInfo->mIsRowRetry      = ID_FALSE;
        aRetryInfo->mStmtRetryColLst = NULL;
        aRetryInfo->mStmtRetryValLst = NULL;
        aRetryInfo->mRowRetryColLst  = NULL;
        aRetryInfo->mRowRetryValLst  = NULL;
    }
}

// PROJ-2264
const void * smiGetCompressionColumn( const void      * aRow,
                                      const smiColumn * aColumn,
                                      idBool            aUseColumnOffset,
                                      UInt            * aLength )
{
    smOID    sCompressionRowOID;
    UInt     sColumnOffset;
    smcTableHeader *sTableHeader;

    void  * sRet;

    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT( aColumn != NULL );

    IDE_DASSERT( (aColumn->flag & SMI_COLUMN_COMPRESSION_MASK)
                 == SMI_COLUMN_COMPRESSION_TRUE );

    if ( aUseColumnOffset == ID_TRUE )
    {
        sColumnOffset = aColumn->offset;
    }
    else
    {
        sColumnOffset = 0;
    }

    idlOS::memcpy( &sCompressionRowOID,
                   (SChar*)aRow + sColumnOffset,
                   ID_SIZEOF(smOID) );

    if ( sCompressionRowOID != SM_NULL_OID )
    {
        sRet = smiGetCompressionColumnFromOID( &sCompressionRowOID,
                                               aColumn,
                                               aLength );
    }
    else
    {
        sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER( smiGetTable( aColumn->mDictionaryTableOID ) );

        sRet = smiGetCompressionColumnFromOID( &sTableHeader->mNullOID,
                                               aColumn,
                                               aLength );
    }
    return sRet;
}

// PROJ-2397
void * smiGetCompressionColumnFromOID( smOID           * aCompressionRowOID,
                                       const smiColumn * aColumn,
                                       UInt            * aLength )
{
    void      * sCompressionRowPtr  = NULL;
    void      * sRet                = NULL;
    smSCN       sCreateSCN          = SM_SCN_FREE_ROW;
    scSpaceID   sTableSpaceID       = 0;

    // PROJ-2429 Dictionary based data compress for on-disk DB
    if ( sctTableSpaceMgr::isDiskTableSpace( aColumn->colSpace ) == ID_TRUE )
    {
        sTableSpaceID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA;
    }
    else
    {
        IDE_ASSERT( sctTableSpaceMgr::isMemTableSpace( aColumn->colSpace ) == ID_TRUE );

        sTableSpaceID = aColumn->colSpace;
    }

    IDE_ASSERT( smmManager::getOIDPtr( sTableSpaceID,
                                       *aCompressionRowOID,
                                       &sCompressionRowPtr )
                == IDE_SUCCESS );

    /* BUG-41686 insert �� commit���� ���� row�� restart �� undo�� ������ ����
     * replication�� ����Ǿ� insert�� ���� Xlog�� ������ ���
     * compress�� �ɷ��ִ� column�� ���� smVCDesc�� �ʱ�ȭ �Ǿ� �������� ���� ���� �� ����.  */

    sCreateSCN = ((smpSlotHeader*)sCompressionRowPtr)->mCreateSCN;
    if ( SM_SCN_IS_FREE_ROW( sCreateSCN ) == ID_TRUE )
    {
        sRet     = NULL;
        *aLength = 0;
    }
    else
    {
        // __FORCE_COMPRESSION_COLUMN = 1
        // infinite scn assert test, hidden property all comp + natc
        // IDE_ASSERT( SM_SCN_IS_INFINITE(sCreateSCN) == ID_TRUE);

        if ( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            // BUG-38573 
            // FIXED type������ vcInOutBaseSize�� 0�� �ƴ� ����
            // Source Column�� VARIABLE�� Key Build Column�� �ǹ��Ѵ�.
            // VARIABLE type�� ���� value�� smVCDescInMode size ������ ��ġ�ϹǷ�
            // ���� value�� ����Ű�� ���ؼ��� offset�� �Ʒ��� ���� �����ؾ� �Ѵ�.
            if ( aColumn->vcInOutBaseSize != 0 )
            {
                // VARIABLE type�� Source Column Value�� ���Ϸ��� �� ��
                sRet = (void*)( (SChar*)sCompressionRowPtr + 
                                SMP_SLOT_HEADER_SIZE + 
                                ID_SIZEOF(smVCDescInMode) );
            }
            else
            {
                // FIXED type�� Source Column Value�� ���Ϸ��� �ϰų�,
                // ���� Column�� Value�� ���Ϸ��� �� ��
                sRet = (void*)( (SChar*)sCompressionRowPtr + 
                                SMP_SLOT_HEADER_SIZE );
            }
            *aLength = aColumn->size;
        }
        else if ( SMI_IS_VARIABLE_COLUMN(aColumn->flag) || 
                  SMI_IS_VARIABLE_LARGE_COLUMN(aColumn->flag) )
        {
            *aLength = 0;

            sRet = sgmManager::getCompressionVarColumn(
                                        (SChar*)sCompressionRowPtr,
                                        aColumn,
                                        aLength );
        }
        else
        {
            IDE_ASSERT(0);
        }
    }

    return sRet;
}

const void * smiGetCompressionColumnLight( const void * aRow,
                                           scSpaceID    aColSpace,
                                           UInt         aVcInOutBaseSize,
                                           UInt         aFlag,
                                           UInt       * aLength )
{
    smiColumn sColumn;

    sColumn.colSpace = aColSpace;
    sColumn.flag     = aFlag;
    sColumn.offset   = 0;
    
    sColumn.vcInOutBaseSize = aVcInOutBaseSize;
    sColumn.value    = NULL;
    
    return smiGetCompressionColumn( aRow, 
                                    &sColumn, 
                                    ID_TRUE, // aUseColumnOffset
                                    aLength );
    
}

// PROJ-2264
void smiGetSmiColumnFromMemTableOID( smOID        aTableOID,
                                     UInt         aIndex,
                                     smiColumn ** aSmiColumn )
{
    SChar          * sOIDPtr;
    smcTableHeader * sTableHeader;
    smOID            sColumnOID;
    smiColumn      * sSmiColumn = NULL;

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       aTableOID,
                                       (void**)&sOIDPtr )
                == IDE_SUCCESS );

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(sOIDPtr);

    sSmiColumn = smcTable::getColumnAndOID( sTableHeader,
                                            aIndex,
                                            &sColumnOID );
    IDE_ASSERT( sSmiColumn != NULL );

    *aSmiColumn = sSmiColumn;
}

// PROJ-2264
idBool smiIsDictionaryTable( void * aTableHandle )
{
    UInt   sTableFlag;
    idBool sRet;

    IDE_ASSERT( aTableHandle != NULL );

    sTableFlag = (SMI_MISC_TABLE_HEADER(aTableHandle))->mFlag;

    if ( (sTableFlag & SMI_TABLE_DICTIONARY_MASK) == SMI_TABLE_DICTIONARY_TRUE )
    {
        sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

// PROJ-2264
void * smiGetTableRuntimeInfoFromTableOID( smOID aTableOID )
{
    void           * sOIDPtr;
    smcTableHeader * sTableHeader;
    void           * sRuntimeInfo;

    IDE_ASSERT( aTableOID != SM_NULL_OID );

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       aTableOID,
                                       (void**)&sOIDPtr )
                == IDE_SUCCESS );

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(sOIDPtr);

    sRuntimeInfo = sTableHeader->mRuntimeInfo;

    return sRuntimeInfo;
}

// PROJ-2402
IDE_RC smiPrepareForParallel( smiTrans * aTrans,
                              UInt     * aParallelReadGroupID )
{
    IDE_ERROR( aTrans               != NULL );
    IDE_ERROR( aParallelReadGroupID != NULL );

    *aParallelReadGroupID = 0;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-2433
 * Description : memory btree index�� direct key �ּ�ũ�� (8).
 **********************************************************************/
UInt smiGetMemIndexKeyMinSize()
{
    return SMI_MEM_INDEX_MIN_KEY_SIZE;
}

/***********************************************************************
 * PROJ-2433
 * Description : memory btree index�� direct key �ִ�ũ��.
 *
 *               node���� direct key �Ӹ��ƴ϶�
 *               node���, pointer(child,row)�� ���ԵǹǷ�
 *               ������ nodeũ�� / 3 ���� direct key �ִ�ũ�⸦ �����Ѵ�
 *
 *               ( __MEM_BTREE_NODE_SIZE�� �� / 3 )
 **********************************************************************/
UInt smiGetMemIndexKeyMaxSize()
{
    return ( smuProperty::getMemBtreeNodeSize() / 3 );
}

/***********************************************************************
 * PROJ-2433
 * Description : property __FORCE_INDEX_DIRECTKEY �� ��ȯ.
 *               (�׽�Ʈ�� property)
 *              
 *               �� property�� �����Ǹ� ���Ļ����Ǵ�
 *               memory btree index�� ��� direct key index�� �����ȴ�
 **********************************************************************/
idBool smiIsForceIndexDirectKey()
{
    return smuProperty::isForceIndexDirectKey();
}

/***********************************************************************
 * BUG-41121, BUG-41541
 * Description : property __FORCE_INDEX_PERSISTENCE �� ��ȯ.
 *               (�׽�Ʈ�� property)
 *              
 *               �ش� property�� ���� 0�� ��� persistent index ����� ������ �ʴ´�.(�⺻��)
 *                                    1�� ��� persistent index�� ���õ� index��
 *                                            persistent index�� �����ȴ�.
 *                                    2�� ��� ���� �����Ǵ� ��� memory btree index��
 *                                            ��� persistent index�� �����ȴ�.
 **********************************************************************/
UInt smiForceIndexPersistenceMode()
{
    return smuProperty::forceIndexPersistenceMode();
}

/***********************************************************************
 * PROJ-2462 Result Cache
 *
 * Description: Table Header��mSequence.mCurSequence
 * ���� �о�´�. �̰��� Table�� Modify�Ǿ�������
 * �Ǵ��� �� ���ȴ�.
 **********************************************************************/
void smiGetTableModifyCount( const void   * aTable,
                             SLong        * aModifyCount )
{
    smcTableHeader * sTableHeader = NULL;

    IDE_DASSERT( aTable       != NULL );
    IDE_DASSERT( aModifyCount != NULL );

    sTableHeader = ( smcTableHeader * )SMI_MISC_TABLE_HEADER( aTable );

    *aModifyCount = idCore::acpAtomicGet64( &sTableHeader->mSequence.mCurSequence );
}

IDE_RC smiWriteXaStartReqLog( ID_XID * aXID,
                              smTID    aTID,
                              smLSN  * aLSN )

{
    return smrUpdate::writeXaStartReqLog( aXID,
                                          aTID,
                                          aLSN );
}

IDE_RC smiWriteXaPrepareReqLog( ID_XID * aXID,
                                smTID    aTID,
                                UInt     aGlobalTxId,
                                UChar  * aBranchTxInfo,
                                UInt     aBranchTxInfoSize,
                                smLSN  * aLSN )

{
    return smrUpdate::writeXaPrepareReqLog( aXID,
                                            aTID,
                                            aGlobalTxId,
                                            aBranchTxInfo,
                                            aBranchTxInfoSize,
                                            aLSN );
}

IDE_RC smiWriteXaEndLog( smTID    aTID,
                         UInt     aGlobalTxId )

{
    return smrUpdate::writeXaEndLog( aTID,
                                     aGlobalTxId );
}

void smiSetGlobalTxId ( smTID   aTID,
                        UInt    aGlobalTxId )
{
    smxTrans * sTrans = smxTransMgr::getTransByTID( aTID );

    if ( sTrans->mTransID != aTID )
    {
        return;
    }

    if ( sTrans->mStatus == SMX_TX_END )
    {
        return;
    } 

    /* BUG-48703 : ���ʰ��� �����Ѵ�. */
    if ( sTrans->mGlobalTxId == SM_INIT_GTX_ID )
    {
        sTrans->mGlobalTxId = aGlobalTxId;
    }
}

IDE_RC smiSet2PCCallbackFunction( smGetDtxMinLSN aGetDtxMinLSN,
                                  smManageDtxInfo aManageDtxInfo )
{
    smrRecoveryMgr::set2PCCallback( aGetDtxMinLSN, aManageDtxInfo );

    return IDE_SUCCESS;
}

void smiSetTransactionalDDLCallback( smiTransactionalDDLCallback * aTransactionalDDLCallback )
{
    smiTrans::setTransactionalDDLCallback( aTransactionalDDLCallback );
}

/* PROJ-2626 Memory UsedSize �� ���Ѵ�. */
void smiGetMemUsedSize( ULong * aMemSize )
{
    ULong        sAlloc  = 0;
    ULong        sFree   = 0;
    smmTBSNode * sCurTBS = NULL;
    UInt         i       = 0;

    sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurTBS != NULL )
    {
        if ( ( sctTableSpaceMgr::isMemTableSpace( sCurTBS ) == ID_TRUE ) &&
             ( sctTableSpaceMgr::hasState( sCurTBS->mHeader.mID,
                                           SCT_SS_SKIP_BUILD_FIXED_TABLE ) == ID_FALSE ) )
        {
            sAlloc += ( ( ULong )sCurTBS->mMemBase->mAllocPersPageCount );

            for ( i = 0 ; i < sCurTBS->mMemBase->mFreePageListCount ; i++ )
            {
                sFree += ( ULong )sCurTBS->mMemBase->mFreePageLists[i].mFreePageCount;
            }
        }
        else
        {
            /* Nothing to do */
        }

        sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getNextSpaceNode( sCurTBS->mHeader.mID );
    }

    *aMemSize = ( sAlloc - sFree ) * SM_PAGE_SIZE;
}

/* PROJ-2626 Disk Undo UsedSize �� ���Ѵ�. */
IDE_RC smiGetDiskUndoUsedSize( ULong * aSize )
{
    sdpscDumpSegHdrInfo   sDumpSegHdr;
    sdpSegMgmtOp        * sSegMgmtOp   = NULL;
    sdcTXSegEntry       * sEntry       = NULL;
    UInt                  sTotEntryCnt = 0;
    ULong                 sTotal       = 0;
    UInt                  sSegPID      = 0;
    UInt                  i            = 0;

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );

    IDE_ERROR( sSegMgmtOp != NULL );

    sTotEntryCnt = sdcTXSegMgr::getTotEntryCnt();

    for ( i = 0 ; i < sTotEntryCnt ; i++ )
    {
        sEntry  = sdcTXSegMgr::getEntryByIdx( i );
        sSegPID = sdcTXSegMgr::getTSSegPtr(sEntry)->getSegPID();

        // TSS Segment
        IDE_TEST( sdpscFT::dumpSegHdr( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       sSegPID,
                                       SDP_SEG_TYPE_TSS,
                                       sEntry,
                                       &sDumpSegHdr,
                                       NULL,
                                       NULL )
                  != IDE_SUCCESS );

        sTotal += sDumpSegHdr.mTxExtCnt;

        sSegPID = sdcTXSegMgr::getUDSegPtr(sEntry)->getSegPID();
        // Undo Segment
        IDE_TEST( sdpscFT::dumpSegHdr( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       sSegPID,
                                       SDP_SEG_TYPE_UNDO,
                                       sEntry,
                                       &sDumpSegHdr,
                                       NULL,
                                       NULL )
                  != IDE_SUCCESS );

        sTotal += sDumpSegHdr.mTxExtCnt + sDumpSegHdr.mUnExpiredExtCnt + sDumpSegHdr.mUnStealExtCnt;

        if ( sDumpSegHdr.mIsOnline == 'Y' )
        {
            sTotal += sDumpSegHdr.mExpiredExtCnt;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aSize = sTotal * smuProperty::getSysUndoTBSExtentSize();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* MM Callback�� SM�� ����ϴ� �Լ� */
IDE_RC smiSetSessionCallback( smiSessionCallback *aCallback )
{
    smxTransMgr::setSessionCallback( aCallback );

    return IDE_SUCCESS;
}

/******************************************************************************
 * PROJ-2733 �л� Ʈ����� ���ռ�
 *******************************************************************************/
void smiSetReplicaSCN()
{
    smxTransMgr::setGlobalConsistentSCNAsSystemSCN();
}
