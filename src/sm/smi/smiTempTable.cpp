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
 * $Id $
 **********************************************************************/

#include <sdt.h>
#include <smiMain.h>
#include <smiTrans.h>
#include <smiMisc.h>
#include <sgmManager.h>
#include <smiTempTable.h>
#include <smiStatement.h>
#include <smuProperty.h>
#include <sdtTempRow.h>
#include <sdtWASortMap.h>
#include <sdtWAExtentMgr.h>
#include <smiHashTempTable.h>
#include <smiSortTempTable.h>

iduMemPool smiTempTable::mTempTableHdrPool;

smiTempTableStats   smiTempTable::mGlobalStats;
smiTempTableIOStats smiTempTable::mSortIOStats;
smiTempTableIOStats smiTempTable::mHashIOStats;
smiTempTableStats * smiTempTable::mTempTableStatsWatchArray = NULL;
UInt                smiTempTable::mStatIdx = 0;
SChar smiTempTable::mOprName[][ SMI_TT_STR_SIZE ] = {
            "NONE",
            "CREATE",
            "DROP",
            "SORT",
            "OPENCURSOR",
            "OPEN_HASHSCANCURSOR",
            "OPEN_FULLSCANCURSOR",
            "OPEN_UPDATECURSOR",
            "RESTARTCURSOR",
            "FETCH",
            "FETCHHASHSCAN",
            "FETCHFULLSCAN",
            "RID_FETCH",
            "STORECURSOR",
            "RESTORECURSOR",
            "CLEAR",
            "CLEARHITFLAG",
            "INSERT",
            "UPDATE",
            "SETHITFLAG",
            "RESETKEYCOLUMN" };
SChar smiTempTable::mTTStateName[][ SMI_TT_STR_SIZE ] = {
            "INIT",
            "SORT_INSERTNSORT",
            "SORT_INSERTONLY",
            "SORT_EXTRACTNSORT",
            "SORT_MERGE",
            "SORT_MAKETREE",
            "SORT_INMEMORYSCAN",
            "SORT_MERGESCAN",
            "SORT_INDEXSCAN",
            "SORT_SCAN",
            "HASH_INSERT",
            "HASH_FETCH_FULLSCAN",
            "HASH_FETCH_HASHSCAN",
            "HASH_FETCH_UPDATE"};

/**************************************************************************
 * Description : ���� ������ TempTable�� ���� �ֿ� �޸𸮸� �ʱ�ȭ��
 **************************************************************************/
IDE_RC smiTempTable::initializeStatic()
{
    IDE_ASSERT( mTempTableHdrPool.initialize( 
                                IDU_MEM_SM_SMI,
                                (SChar*)"SMI_TEMPTABLE_HDR_POOL",
                                smuProperty::getIteratorMemoryParallelFactor(),
                                ID_SIZEOF( smiTempTableHeader ),
                                128, /* ElemCount */
                                IDU_AUTOFREE_CHUNK_LIMIT,          /* ChunkLimit */
                                ID_TRUE,                           /* UseMutex */
                                IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,   /* AlignByte */
                                ID_FALSE,                          /* ForcePooling */
                                ID_TRUE,                           /* GarbageCollection */
                                ID_TRUE,                           /* HWCacheLine */
                                IDU_MEMPOOL_TYPE_LEGACY )
                == IDE_SUCCESS );

    IDE_ASSERT( smiSortTempTable::initializeStatic() == IDE_SUCCESS );
    IDE_ASSERT( smiHashTempTable::initializeStatic() == IDE_SUCCESS );

    IDE_TEST( sdtWAExtentMgr::initializeStatic() != IDE_SUCCESS );

    IDE_ASSERT(iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                  smuProperty::getTempStatsWatchArraySize(),
                                  ID_SIZEOF( smiTempTableStats ) ,
                                  (void**)&mTempTableStatsWatchArray )
               == IDE_SUCCESS);

    idlOS::memset( &mGlobalStats, 0, ID_SIZEOF( smiTempTableStats ) );
    idlOS::memset( &mHashIOStats, 0, ID_SIZEOF( smiTempTableIOStats ) );
    idlOS::memset( &mSortIOStats, 0, ID_SIZEOF( smiTempTableIOStats ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 *   ���� ����� TempTable�� ���� �ֿ� �޴����� ����.
 **************************************************************************/
IDE_RC smiTempTable::destroyStatic()
{
    IDE_ASSERT( iduMemMgr::free( mTempTableStatsWatchArray ) == IDE_SUCCESS );
    IDE_ASSERT( sdtWAExtentMgr::destroyStatic() == IDE_SUCCESS );
    IDE_ASSERT( smiSortTempTable::destroyStatic() == IDE_SUCCESS );
    IDE_ASSERT( smiHashTempTable::destroyStatic() == IDE_SUCCESS );
    IDE_ASSERT( mTempTableHdrPool.destroy() == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***************************************************************************
 * Description : NullRow�� ��ȯ�Ѵ�.
 *
 * aTable         - [IN] ��� Table
 * aNullRowPtr    - [IN] NullRow�� ������ ����
 ***************************************************************************/
IDE_RC smiTempTable::getNullRow( void   * aTable,
                                 UChar ** aNullRowPtr )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    if( sHeader->mNullRow == NULL )
    {
        IDE_TEST( makeNullRow( sHeader ) != IDE_SUCCESS );
    }

    idlOS::memcpy( *aNullRowPtr,
                   sHeader->mNullRow,
                   sHeader->mRowSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : NullRow�� �����Ѵ�.
 *
 * aHeader    - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiTempTable::makeNullRow( smiTempTableHeader   * aHeader )
{
    UInt             sColumnIdx;
    smiTempColumn  * sTempColumn;
    SChar          * sValue;
    UInt             sLength;

    IDE_DASSERT( aHeader->mNullRow == NULL );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 aHeader->mRowSize,
                                 (void**)&aHeader->mNullRow )
              != IDE_SUCCESS );

    /* NullColumn ���� */
    for( sColumnIdx = 0 ; sColumnIdx < aHeader->mColumnCount ; sColumnIdx++ )
    {
        sTempColumn = &aHeader->mColumns[ sColumnIdx ];

        if( (sTempColumn->mColumn.flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED )
        {
            sValue = (SChar*)aHeader->mNullRow + sTempColumn->mColumn.offset;
        }
        else
        {
            sValue = sgmManager::getVarColumn((SChar*)aHeader->mNullRow, &(sTempColumn->mColumn), &sLength);
        }

        sTempColumn->mNull( &sTempColumn->mColumn,
                            sValue );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : ������� �ʱ�ȭ
 *
 * aHeader      - [IN] ��� Table
 * aStatement   - [IN] �������
 ***************************************************************************/
void  smiTempTable::initStatsPtr( smiTempTableHeader * aHeader,
                                  idvSQL             * aStatistics,
                                  smiStatement       * aStatement )
{
    /* �ڽ��� StatBuffer�� �̿��ϴٰ�, TempTabeWatchTime�� �Ѿ��
     * Array�� ����� */
    aHeader->mStatsBuffer.mSpaceID        = aHeader->mSpaceID;
    aHeader->mStatsBuffer.mTransID        = aStatement->getTrans()->getTransID();
    aHeader->mStatsBuffer.mTTState        = SMI_TTSTATE_INIT;
    aHeader->mStatsBuffer.mCreateTV       = smiGetCurrTime();
    aHeader->mStatsBuffer.mSQLText[0]     = '\0';
    aHeader->mStatsBuffer.mRuntimeMemSize = ID_SIZEOF( smiTempTableHeader );

    IDE_ASSERT( gSmiGlobalCallBackList.getSQLText( aStatistics,
                                                   aHeader->mStatsBuffer.mSQLText,
                                                   SMI_TT_SQLSTRING_SIZE )
                == IDE_SUCCESS );

    aHeader->mStatsPtr = &aHeader->mStatsBuffer;
}

/***************************************************************************
 * Description : WatchArray�� ��������� �����
 *
 * aHeader - [IN] ��� Table
 ***************************************************************************/
smiTempTableStats * smiTempTable::registWatchArray( smiTempTableStats * aStatsPtr )
{
    UInt                sAcquiredIdx;
    smiTempTableStats * sStats;

    sAcquiredIdx = (UInt)idCore::acpAtomicInc32( &mStatIdx ) 
                   % smuProperty::getTempStatsWatchArraySize();
    sStats       = &mTempTableStatsWatchArray[ sAcquiredIdx ];

    idlOS::memcpy( sStats, 
                   aStatsPtr, 
                   ID_SIZEOF( smiTempTableStats ) );
    return sStats;
}

/***************************************************************************
 * Description : �� TempTable�� ���ġ�� Global ��迡 ������.
 *
 * aHeader - [IN] ��� Table
 ***************************************************************************/
void smiTempTable::accumulateStats( smiTempTableHeader * aHeader )
{
    smiTempTableStats * sStats = aHeader->mStatsPtr;

    if (( aHeader->mTTFlag & SMI_TTFLAG_TYPE_MASK ) == SMI_TTFLAG_TYPE_SORT )
    {
        mSortIOStats.mReadCount      += sStats->mReadCount;
        mSortIOStats.mWriteCount     += sStats->mWriteCount;
        mSortIOStats.mWritePageCount += sStats->mWritePageCount;

        mSortIOStats.mIOPassNo        = IDL_MAX( mSortIOStats.mIOPassNo,
                                                 sStats->mIOPassNo );

        mSortIOStats.mEstimatedOptimalSize    = IDL_MAX( mSortIOStats.mEstimatedOptimalSize,
                                                         sStats->mEstimatedOptimalSize );
        mSortIOStats.mEstimatedSubOptimalSize = IDL_MAX( mSortIOStats.mEstimatedSubOptimalSize,
                                                         sStats->mEstimatedSubOptimalSize );
        mSortIOStats.mMaxUsedWorkAreaSize     = IDL_MAX( mSortIOStats.mMaxUsedWorkAreaSize,
                                                         sStats->mUsedWorkAreaSize );
    }
    else
    {
        IDE_DASSERT(( aHeader->mTTFlag & SMI_TTFLAG_TYPE_MASK ) == SMI_TTFLAG_TYPE_HASH );

        mHashIOStats.mReadCount      += sStats->mReadCount;
        mHashIOStats.mWriteCount     += sStats->mWriteCount;
        mHashIOStats.mWritePageCount += sStats->mWritePageCount;

        mHashIOStats.mIOPassNo        = IDL_MAX( mHashIOStats.mIOPassNo,
                                                 sStats->mIOPassNo );

        mHashIOStats.mEstimatedOptimalSize    = IDL_MAX( mHashIOStats.mEstimatedOptimalSize,
                                                         sStats->mEstimatedOptimalSize );
        mHashIOStats.mEstimatedSubOptimalSize = IDL_MAX( mHashIOStats.mEstimatedSubOptimalSize,
                                                         sStats->mEstimatedSubOptimalSize );
        mHashIOStats.mMaxUsedWorkAreaSize     = IDL_MAX( mHashIOStats.mMaxUsedWorkAreaSize,
                                                         sStats->mUsedWorkAreaSize );
    }

    mGlobalStats.mOverAllocCount += sStats->mOverAllocCount;
    mGlobalStats.mAllocWaitCount += sStats->mAllocWaitCount;
    mGlobalStats.mUsedWorkAreaSize += sStats->mUsedWorkAreaSize;
    mGlobalStats.mNormalAreaSize += sStats->mNormalAreaSize;
    mGlobalStats.mRecordCount    += sStats->mRecordCount;
    mGlobalStats.mRecordLength   += sStats->mRecordLength;
    mGlobalStats.mTime           += sStats->mDropTV - sStats->mCreateTV;
    mGlobalStats.mCount ++;
}


/***************************************************************************
 * Description : ����ó�� ���� �Լ�.
 * ����ġ ���� ������ ���, Header�� File�� Dump�Ѵ�.
 *
 * aHeader   - [IN] ��� Table
 ***************************************************************************/
void   smiTempTable::checkAndDump( smiTempTableHeader * aHeader )
{
    SChar   sErrorBuffer[256];

    IDE_PUSH();
    switch( ideGetErrorCode() )
    {
    case smERR_ABORT_NOT_ENOUGH_WORKAREA:
    case smERR_ABORT_NOT_ENOUGH_SPACE:
    case smERR_ABORT_NotEnoughFreeSpace:
    case smERR_ABORT_INVALID_SORTAREASIZE:
    case idERR_ABORT_Session_Disconnected:  /* SessionTimeOut�̴� �������� */
    case idERR_ABORT_Session_Closed:        /* ���� ���� */
    case idERR_ABORT_Query_Timeout:         /* �ð� �ʰ� */
    case idERR_ABORT_Query_Canceled:
    case idERR_ABORT_IDU_MEMORY_ALLOCATION: /* BUG-38636 : �޸��Ѱ赵 ���� */
    case idERR_ABORT_InsufficientMemory:    /* �޸� �Ѱ��Ȳ�� ���� */
        IDE_CONT( skip );
        break;
    default:
        ideLog::log(IDE_ERR_0,"checkAndDump %d\n",ideGetErrorCode() );
        /* �� ���� ���� __TEMPDUMP_ENABLE�� ���� Dump�� */
        if( smuProperty::getTempDumpEnable() == 1 )
        {
            dumpToFile( aHeader );
        }
        else
        {
            /* nothing to do */
        }
        /*__ERROR_VALIDATION_LEVEL�� ���� ������������*/
        IDE_ERROR( 0 );
        break;
    }

    IDE_EXCEPTION_END;

    IDE_POP();
    idlOS::snprintf( sErrorBuffer,
                     256,
                     "[FAILURE] ERR-%05X(error=%"ID_UINT32_FMT") %s",
                     E_ERROR_CODE( ideGetErrorCode() ),
                     ideGetSystemErrno(),
                     ideGetErrorMsg( ideGetErrorCode() ) );

    ideLog::log( IDE_ERR_0,
                 "%s\n",
                 sErrorBuffer );

    IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );

    IDE_EXCEPTION_CONT( skip );
}

/* ------------------------------------------------
 *  Fixed Table Define for X$TEMPTABLE_STATS 
 * ----------------------------------------------*/

static iduFixedTableColDesc  gTempTableStatsColDesc[]=
{
    {
        (SChar*)"SLOT_IDX",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mSlotIdx),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mSlotIdx),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SQL_TEXT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mSQLText),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mSQLText),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CREATE_TIME",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mCreateTime),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mCreateTime),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"DROP_TIME",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mDropTime),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mDropTime),
        IDU_FT_TYPE_CHAR | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CONSUME_TIME",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mConsumeTime),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mConsumeTime),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"TBS_ID",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf, mSpaceID),
        IDU_FT_SIZEOF(smiTempTableStats4Perf, mSpaceID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar*)"TRANSACTION_ID",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf, mTransID),
        IDU_FT_SIZEOF(smiTempTableStats4Perf, mTransID),
        IDU_FT_TYPE_UBIGINT,  // BUG-47379 unsigned int -> big int
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar*)"STATE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf, mTTState),
        IDU_FT_SIZEOF(smiTempTableStats4Perf, mTTState),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar*)"IO_PASS_NUMBER",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf, mIOPassNo),
        IDU_FT_SIZEOF(smiTempTableStats4Perf, mIOPassNo),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar*)"ESTIMATED_OPTIMAL_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf, mEstimatedOptimalSize),
        IDU_FT_SIZEOF(smiTempTableStats4Perf, mEstimatedOptimalSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar*)"ESTIMATED_SUB_OPTIMAL_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mEstimatedSubOptimalSize), 
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mEstimatedSubOptimalSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"READ_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mReadCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mReadCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"WRITE_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mWriteCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mWriteCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"WRITE_PAGE_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mWritePageCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mWritePageCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"OVER_ALLOC_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mOverAllocCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mOverAllocCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"ALLOC_WAIT_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mAllocWaitCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mAllocWaitCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"WORK_AREA_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mUsedWorkAreaSize),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mUsedWorkAreaSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MAX_WORK_AREA_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mMaxWorkAreaSize),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mMaxWorkAreaSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NORMAL_AREA_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mNormalAreaSize),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mNormalAreaSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"RUNTIME_MAP_SIZE",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mRuntimeMemSize),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mRuntimeMemSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"RECORD_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mRecordCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mRecordCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"RECORD_LENGTH",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mRecordLength),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mRecordLength),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MERGE_RUN_COUNT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mMergeRunCount),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mMergeRunCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"HEIGHT",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mHeight),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mHeight),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"LAST_OPERATION",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mLastOpr),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mLastOpr),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"EXTRA_STAT1",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mExtraStat1),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mExtraStat1),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"EXTRA_STAT2",
        IDU_FT_OFFSETOF(smiTempTableStats4Perf,mExtraStat2),
        IDU_FT_SIZEOF(smiTempTableStats4Perf,mExtraStat2),
        IDU_FT_TYPE_UBIGINT,
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

// X$TEMPTABLE_STATS
iduFixedTableDesc  gTempTableStatsDesc=
{
    (SChar *)"X$TEMPTABLE_STATS",
    smiTempTable::buildTempTableStatsRecord,
    gTempTableStatsColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/***************************************************************************
 * Description : X$TEMPTABLE_STATS ����.
 *
 * aHeader   - [IN] PerfView�� Header
 * aMemory   - [IN] ������� ���� �޸�
 ***************************************************************************/
IDE_RC smiTempTable::buildTempTableStatsRecord( idvSQL              * /*aStatistics*/,
                                                void                * aHeader,
                                                void                * /*aDumpObj*/,
                                                iduFixedTableMemory * aMemory )
{
    smiTempTableStats      * sStats;
    smiTempTableStats4Perf   sPerf;
    UInt                     sSlotIdx;
    UInt                     i;
    void                   * sIndexValues[2];

    sSlotIdx = (UInt)idCore::acpAtomicGet32( &mStatIdx ) 
                 % smuProperty::getTempStatsWatchArraySize();

    for ( i = 0 ; i < smuProperty::getTempStatsWatchArraySize() ; i++ )
    {
        sSlotIdx = ( sSlotIdx + 1 ) 
                    % smuProperty::getTempStatsWatchArraySize();

        sStats = &mTempTableStatsWatchArray[ sSlotIdx ];

        if ( sStats->mCreateTV != 0 )
        {
            /* BUG-43006 FixedTable Indexing Filter
             * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
             * �κи� ������ Filtering �Ѵ�.
             * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
             * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
             * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
             * �� �־���Ѵ�.
             */
            smuUtility::getTimeString( sStats->mDropTV,
                                       SMI_TT_STR_SIZE,
                                       sPerf.mDropTime );
            sIndexValues[0] = &sPerf.mDropTime;
            sIndexValues[1] = &sStats->mSpaceID;
            if ( iduFixedTable::checkKeyRange( aMemory,
                                               gTempTableStatsColDesc,
                                               sIndexValues )
                 == ID_FALSE )
            {
                continue;
            }
            else
            {
                /* Nothing to do */
            }
            makeStatsPerf( sStats, &sPerf );
            sPerf.mSlotIdx    = sSlotIdx;

            IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                aMemory,
                                                (void *)&sPerf)
                     != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Fixed Table Define for X$TEMPTABLE_STATS 
 * ----------------------------------------------*/
static iduFixedTableColDesc  gTempInfoColDesc[]=
{
    {
        (SChar*)"NAME",
        IDU_FT_OFFSETOF(smiTempInfo4Perf,mName),
        IDU_FT_SIZEOF(smiTempInfo4Perf,mName),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"VALUE",
        IDU_FT_OFFSETOF(smiTempInfo4Perf,mValue),
        IDU_FT_SIZEOF(smiTempInfo4Perf,mValue),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"UNIT",
        IDU_FT_OFFSETOF(smiTempInfo4Perf,mUnit),
        IDU_FT_SIZEOF(smiTempInfo4Perf,mUnit),
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

// X$TEMPINFO
iduFixedTableDesc  gTempInfoDesc=
{
    (SChar *)"X$TEMPINFO",
    smiTempTable::buildTempInfoRecord,
    gTempInfoColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/***************************************************************************
 * Description : X$TEMPINFO ����.
 *
 * aHeader    - [IN] PerfView�� Header
 * aMemory    - [IN] ������� ���� �޸�
 ***************************************************************************/
IDE_RC smiTempTable::buildTempInfoRecord( idvSQL              * /*aStatistics*/,
                                          void                * aHeader,
                                          void                * /* aDumpObj */,
                                          iduFixedTableMemory * aMemory )
{
    smiTempInfo4Perf         sInfo;

    /* Property���� ��� */
    SMI_TT_SET_TEMPINFO_ULONG( "INIT_TOTAL_WA_SIZE", 
                               smuProperty::getInitTotalWASize(), "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL_WA_SIZE", 
                               smuProperty::getMaxTotalWASize(), "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "HASH_AREA_SIZE", 
                               smuProperty::getHashAreaSize(), "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "SORT_AREA_SIZE", 
                               smuProperty::getSortAreaSize(), "BYTES" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_SORT_PARTITION_SIZE", 
                              smuProperty::getTempSortPartitionSize(), 
                              "ROWCOUNT" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_ALLOC_TRY_COUNT", 
                              smuProperty::getTempAllocTryCount(), "INTEGER" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_ROW_SPLIT_THRESHOLD", 
                              smuProperty::getTempRowSplitThreshold(), 
                              "BYTES" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_SORT_GROUP_RATIO", 
                              smuProperty::getTempSortGroupRatio(), 
                              "PERCENT" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_HASH_GROUP_RATIO", 
                              smuProperty::getTempHashGroupRatio(), 
                              "PERCENT" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_SUBHASH_GROUP_RATIO", 
                              smuProperty::getTempSubHashGroupRatio(), 
                              "PERCENT" );
    SMI_TT_SET_TEMPINFO_UINT( "HASH_SLOT_PAGE_COUNT",
                              sdtHashModule::getHashSlotPageCount(), 
                              "PAGECOUNT" );
    SMI_TT_SET_TEMPINFO_UINT( "SUB_HASH_PAGE_COUNT", 
                              sdtHashModule::getSubHashPageCount(), 
                              "PAGECOUNT" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_SLEEP_INTERVAL", 
                              smuProperty::getTempSleepInterval(), 
                              "USEC" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_MAX_KEY_SIZE", 
                              smuProperty::getTempMaxKeySize(), 
                              "BYTES" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_STATS_WATCH_ARRAY_SIZE", 
                              smuProperty::getTempStatsWatchArraySize(), 
                              "INTEGER" );
    SMI_TT_SET_TEMPINFO_UINT( "TEMP_STATS_WATCH_TIME", 
                              smuProperty::getTempStatsWatchTime(), 
                              "SEC" );

    /* WA Extent�� ������ ��� */
    IDE_TEST( sdtWAExtentMgr::buildTempInfoRecord( aHeader,
                                                   aMemory )
              != IDE_SUCCESS );
    /* ���� ���� ��� */

    SMI_TT_SET_TEMPINFO_UINT( "NORMAL PAGE HASH MAP POOL SIZE",
                              sdtHashModule::getNPageHashMapPoolSize() +
                              sdtSortSegment::getNPageHashMapPoolSize(),
                              "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL STATS COUNT", 
                               mGlobalStats.mCount, 
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL CONSUME TIME", 
                               mGlobalStats.mTime, "SEC" );

    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL OVER ALLOC COUNT", 
                               mGlobalStats.mOverAllocCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL ALLOC WAIT COUNT", 
                               mGlobalStats.mAllocWaitCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL USED WORK AREA SIZE", 
                               mGlobalStats.mUsedWorkAreaSize,
                               "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL USED TEMP PAGE COUNT", 
                               mGlobalStats.mNormalAreaSize / SD_PAGE_SIZE,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "TOTAL INSERTED RECORD COUNT", 
                               mGlobalStats.mRecordCount,
                               "INTEGER" );

    /* Hash */
    SMI_TT_SET_TEMPINFO_ULONG( "HASH MAX WA SIZE",
                               sdtHashModule::getMaxWAExtentCount() * SDT_WAEXTENT_SIZE,
                               "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "HASH MAX USED WA SIZE",
                               mHashIOStats.mMaxUsedWorkAreaSize,
                               "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "HASH TEMP SEGMENT POOL SIZE",
                               sdtHashModule::getSegPoolSize(),
                               "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "HASH TOTAL READ COUNT", 
                               mHashIOStats.mReadCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "HASH TOTAL WRITE COUNT", 
                               mHashIOStats.mWriteCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "HASH TOTAL WRITE PAGE COUNT", 
                               mHashIOStats.mWritePageCount,
                               "INTEGER" );

    SMI_TT_SET_TEMPINFO_ULONG( "HASH MAX IO PASS NUMBER", 
                               mHashIOStats.mIOPassNo,
                               "INTEGER" );

    SMI_TT_SET_TEMPINFO_ULONG( "MAX ESTIMATED OPTIMAL HASH SIZE", 
                               mHashIOStats.mEstimatedOptimalSize, 
                               "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "MAX ESTIMATED SUB OPTIMAL HASH SIZE", 
                               mHashIOStats.mEstimatedSubOptimalSize, 
                               "BYTES" );

    /* Sort */
    SMI_TT_SET_TEMPINFO_ULONG( "SORT MAX WA SIZE",
                               sdtSortSegment::getWAExtentCount() * SDT_WAEXTENT_SIZE,
                               "BYTES" );
    
    SMI_TT_SET_TEMPINFO_ULONG( "SORT MAX USED WA SIZE",
                               mSortIOStats.mMaxUsedWorkAreaSize,
                               "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "SORT TEMP SEGMENT POOL SIZE",
                               sdtSortSegment::getSegPoolSize(),
                               "BYTES" );

    SMI_TT_SET_TEMPINFO_ULONG( "SORT TOTAL READ COUNT", 
                               mSortIOStats.mReadCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "SORT TOTAL WRITE COUNT", 
                               mSortIOStats.mWriteCount,
                               "INTEGER" );
    SMI_TT_SET_TEMPINFO_ULONG( "SORT TOTAL WRITE PAGE COUNT", 
                               mSortIOStats.mWritePageCount,
                               "INTEGER" );

    SMI_TT_SET_TEMPINFO_ULONG( "SORT MAX IO PASS NUMBER", 
                               mSortIOStats.mIOPassNo,
                               "INTEGER" );

    SMI_TT_SET_TEMPINFO_ULONG( "MAX ESTIMATED OPTIMAL SORT SIZE", 
                               mSortIOStats.mEstimatedOptimalSize, 
                               "BYTES" );
    SMI_TT_SET_TEMPINFO_ULONG( "MAX ESTIMATED ONEPASS SORT SIZE", 
                               mSortIOStats.mEstimatedSubOptimalSize, 
                               "BYTES" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Temp Table Segment���� ������ ���Ϸ� ���
 *
 * aHeader  - [IN] ��� TempTableHeader
 ***************************************************************************/
void smiTempTable::dumpToFile( smiTempTableHeader * aHeader )
{
    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0, dumpTempTableHeader, aHeader );

    switch( aHeader->mTTFlag & SMI_TTFLAG_TYPE_MASK )
    {
        case SMI_TTFLAG_TYPE_SORT:
            sdtSortSegment::exportSortSegmentToFile( (sdtSortSegHdr*)aHeader->mWASegment );
            break;
        case SMI_TTFLAG_TYPE_HASH:
            sdtHashModule::exportHashSegmentToFile( (sdtHashSegHdr*)aHeader->mWASegment );
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    return;
}

/**************************************************************************
 * Description : Temp Table Header ������ ���
 *
 * aTempHeader  - [IN] ��� TempTableHeader
 * aOutBuf      - [OUT] ������ ����� Buffer
 * aOutSize     - [OUT] Buffer�� ũ��
 ***************************************************************************/
void smiTempTable::dumpTempTableHeader( void  * aTableHeader,
                                        SChar  * aOutBuf, 
                                        UInt     aOutSize )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTableHeader; 
    UInt                 i;

    IDE_ERROR( aTableHeader != NULL );

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "DUMP TEMPTABLEHEADER:\n"
                         "mColumnCount    : %"ID_UINT32_FMT"\n"
                         "mWASegment      : 0x%"ID_xINT64_FMT"\n"
                         "mTTState        : %"ID_UINT32_FMT"\n"
                         "mTTFlag         : %"ID_UINT32_FMT"\n"
                         "mSpaceID        : %"ID_UINT32_FMT"\n"
                         "mWorkGroupRatio : %"ID_UINT64_FMT"\n"
                         "mHitSequence    : %"ID_UINT32_FMT"\n"
                         "mRowSize        : %"ID_UINT32_FMT"\n"
                         "mRowCount       : %"ID_UINT64_FMT"\n"
                         "mMaxRowPageCount: %"ID_UINT32_FMT"\n"
                         "mFetchGroupID   : %"ID_UINT32_FMT"\n"
                         "mSortGroupID    : %"ID_UINT32_FMT"\n"
                         "mMergeRunCount  : %"ID_UINT32_FMT"\n"
                         "mLeftBottomPos  : %"ID_UINT32_FMT"\n"
                         "mRootWPID       : %"ID_UINT32_FMT"\n"
                         "mHeight         : %"ID_UINT32_FMT"\n"
                         "mRowHeadNPID    : %"ID_UINT32_FMT"\n"
                         "DUMP ColumnList:\n"
                         "%4s %4s %4s %4s %4s %4s\n",
                         sHeader->mColumnCount,
                         sHeader->mWASegment,
                         sHeader->mTTState,
                         sHeader->mTTFlag,
                         sHeader->mSpaceID,
                         sHeader->mWorkGroupRatio,
                         sHeader->mHitSequence,
                         sHeader->mRowSize,
                         sHeader->mRowCount,
                         sHeader->mMaxRowPageCount,
                         sHeader->mFetchGroupID,
                         sHeader->mSortGroupID,
                         sHeader->mMergeRunCount,
                         sHeader->mLeftBottomPos,
                         sHeader->mRootWPID,
                         sHeader->mHeight,
                         sHeader->mRowHeadNPID,
                         "IDX",
                         "HDSZ",
                         "ID",
                         "FLAG",
                         "OFF",
                         "SIZE");

    for( i = 0 ; i < sHeader->mColumnCount ; i ++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%4"ID_UINT32_FMT" "
                             "%4"ID_UINT32_FMT" "
                             "%4"ID_UINT32_FMT" "
                             "%4"ID_UINT32_FMT" "
                             "%4"ID_UINT32_FMT" "
                             "%4"ID_UINT32_FMT"\n",
                             sHeader->mColumns[ i ].mIdx,
                             sHeader->mColumns[ i ].mStoringSize,
                             sHeader->mColumns[ i ].mColumn.id,
                             sHeader->mColumns[ i ].mColumn.flag,
                             sHeader->mColumns[ i ].mColumn.offset,
                             sHeader->mColumns[ i ].mColumn.size );
    }

    dumpTempStats( sHeader->mStatsPtr, aOutBuf, aOutSize );

    if(sHeader->mTempCursorList != NULL )
    {
        switch( sHeader->mTTFlag & SMI_TTFLAG_TYPE_MASK )
        {
            case SMI_TTFLAG_TYPE_SORT:
                smiSortTempTable::dumpTempCursor( (smiSortTempCursor*)sHeader->mTempCursorList,
                                                  aOutBuf,
                                                  aOutSize );
                break;
            case SMI_TTFLAG_TYPE_HASH:
                smiHashTempTable::dumpTempCursor( (smiHashTempCursor*)sHeader->mTempCursorList,
                                                  aOutBuf,
                                                  aOutSize );
                break;
            default:
                IDE_ASSERT(0);
                break;
        }
    }

    return;

    IDE_EXCEPTION_END;

    return;
}

/**************************************************************************
 * Description : Temp table ��� ������ ���
 *
 * aTempCursor  - [IN] ��� Cursor
 * aOutBuf      - [OUT] ������ ����� Buffer
 * aOutSize     - [OUT] Buffer�� ũ��
 ***************************************************************************/
void smiTempTable::dumpTempStats( smiTempTableStats * aTempStats,
                                  SChar             * aOutBuf,
                                  UInt                aOutSize )
{
    smiTempTableStats4Perf sPerf;

    if( aTempStats != NULL )
    {
        makeStatsPerf( aTempStats, &sPerf );
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "TEMP STATS:\n"
                             "SQLText        : %s\n"
                             "CreateTime     : %s\n"
                             "DropTime       : %s\n"
                             "ConsumeTime    : %"ID_UINT32_FMT"\n"
                             "SpaceID        : %"ID_UINT32_FMT"\n"
                             "TransID        : %"ID_UINT32_FMT"\n"
                             "LastOpr        : %s\n"
                             "TTState        : %s\n"
                             "IOPassNo       : %"ID_UINT32_FMT"\n"
                             "ReadCount      : %"ID_UINT64_FMT"\n"
                             "WriteCount     : %"ID_UINT64_FMT"\n"
                             "WritePage      : %"ID_UINT64_FMT"\n"
                             "OverAllocCount : %"ID_UINT64_FMT"\n"
                             "AllocWaitCount : %"ID_UINT64_FMT"\n"
                             "MaxWorkAreaSize: %"ID_UINT64_FMT"\n"
                             "WorkAreaSize   : %"ID_UINT64_FMT"\n"
                             "NormalAreaSize : %"ID_UINT64_FMT"\n"
                             "RecordCount    : %"ID_UINT64_FMT"\n"
                             "RecordLength   : %"ID_UINT32_FMT"\n"
                             "MergeRunCount  : %"ID_UINT32_FMT"\n"
                             "Height         : %"ID_UINT32_FMT"\n",
                             sPerf.mSQLText,
                             sPerf.mCreateTime,
                             sPerf.mDropTime,
                             sPerf.mConsumeTime,
                             sPerf.mSpaceID,
                             sPerf.mTransID,
                             sPerf.mLastOpr,
                             sPerf.mTTState,
                             sPerf.mIOPassNo,
                             sPerf.mReadCount,
                             sPerf.mWriteCount,
                             sPerf.mWritePageCount,
                             sPerf.mOverAllocCount,
                             sPerf.mAllocWaitCount,
                             sPerf.mMaxWorkAreaSize,
                             sPerf.mUsedWorkAreaSize,
                             sPerf.mNormalAreaSize,
                             sPerf.mRecordCount,
                             sPerf.mRecordLength,
                             sPerf.mMergeRunCount,
                             sPerf.mHeight );
    }
    return;
}

/**************************************************************************
 * Description : Temp table ��� ������ ����
 *
 * aTempStats   - [IN] ��� ���� ����
 * aPerf        - [OUT] ��� ���� ���纻
 ***************************************************************************/
void smiTempTable::makeStatsPerf( smiTempTableStats      * aTempStats,
                                  smiTempTableStats4Perf * aPerf )
{
    idlOS::memcpy( aPerf->mSQLText,
                   aTempStats->mSQLText,
                   SMI_TT_SQLSTRING_SIZE );
    smuUtility::getTimeString( aTempStats->mCreateTV,
                               SMI_TT_STR_SIZE,
                               aPerf->mCreateTime );
    smuUtility::getTimeString( aTempStats->mDropTV,
                               SMI_TT_STR_SIZE,
                               aPerf->mDropTime );
    if( aTempStats->mDropTV == 0 )
    {
        aPerf->mConsumeTime = smiGetCurrTime() - aTempStats->mCreateTV;
    }
    else
    {
        aPerf->mConsumeTime = aTempStats->mDropTV - aTempStats->mCreateTV;
    }
    aPerf->mSpaceID        = aTempStats->mSpaceID;
    aPerf->mTransID        = aTempStats->mTransID;
    idlOS::memcpy( aPerf->mLastOpr, 
                   mOprName[ aTempStats->mTTLastOpr ], 
                   SMI_TT_STR_SIZE );
    idlOS::memcpy( aPerf->mTTState, 
                   mTTStateName[ aTempStats->mTTState ], 
                   SMI_TT_STR_SIZE );

    aPerf->mEstimatedOptimalSize    = aTempStats->mEstimatedOptimalSize;
    aPerf->mEstimatedSubOptimalSize = aTempStats->mEstimatedSubOptimalSize;

    aPerf->mIOPassNo         = aTempStats->mIOPassNo;
    aPerf->mReadCount        = aTempStats->mReadCount;
    aPerf->mWriteCount       = aTempStats->mWriteCount;
    aPerf->mWritePageCount   = aTempStats->mWritePageCount;
    aPerf->mOverAllocCount   = aTempStats->mOverAllocCount;
    aPerf->mAllocWaitCount   = aTempStats->mAllocWaitCount;
    aPerf->mMaxWorkAreaSize  = aTempStats->mMaxWorkAreaSize;
    aPerf->mUsedWorkAreaSize = aTempStats->mUsedWorkAreaSize;
    aPerf->mNormalAreaSize   = aTempStats->mNormalAreaSize;
    aPerf->mRuntimeMemSize   = aTempStats->mRuntimeMemSize;
    aPerf->mRecordCount      = aTempStats->mRecordCount;
    aPerf->mRecordLength     = aTempStats->mRecordLength;
    aPerf->mMergeRunCount    = aTempStats->mMergeRunCount;
    aPerf->mHeight           = aTempStats->mHeight;
    aPerf->mExtraStat1       = aTempStats->mExtraStat1;
    aPerf->mExtraStat2       = aTempStats->mExtraStat2;
}

/***************************************************************************
 * Description : TempTable�� �����մϴ�.
 *
 * aTable   - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiTempTable::drop( void    * aTable )
{
    switch( ((smiTempTableHeader*)aTable)->mTTFlag & SMI_TTFLAG_TYPE_MASK )
    {
        case SMI_TTFLAG_TYPE_SORT:
            IDE_TEST( smiSortTempTable::drop(aTable) != IDE_SUCCESS );
            break;
        case SMI_TTFLAG_TYPE_HASH:
            IDE_TEST( smiHashTempTable::drop(aTable) != IDE_SUCCESS );
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : GRID�� Row�� �����ɴϴ�.
 *
 * aTable       - [IN] ��� Table
 * aGRID        - [IN] ��� Row
 * aDestRowBuf  - [OUT] ������ Row�� ������ ����
 ***************************************************************************/
IDE_RC smiTempTable::fetchFromGRID( void     * aTable,
                                    scGRID     aGRID,
                                    void     * aDestRowBuf )
{
    switch( ((smiTempTableHeader*)aTable)->mTTFlag & SMI_TTFLAG_TYPE_MASK )
    {
        case SMI_TTFLAG_TYPE_SORT:
            IDE_TEST( smiSortTempTable::fetchFromGRID( aTable,
                                                       aGRID,
                                                       aDestRowBuf ) != IDE_SUCCESS );
            break;
        case SMI_TTFLAG_TYPE_HASH:
            IDE_TEST( smiHashTempTable::fetchFromGRID( aTable,
                                                       aGRID,
                                                       aDestRowBuf ) != IDE_SUCCESS );
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
