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
 * $Id:
 **********************************************************************/

#ifndef O_SMU_PROPERTY_H
#define O_SMU_PROPERTY_H 1

#include <idp.h>
#include <iduLatch.h>
#include <smDef.h>

#define SMU_LOG_BUFFER_TYPE_MMAP       (0)
#define SMU_LOG_BUFFER_TYPE_MEMORY     (1)


#define SMU_THREAD_OFF                 (0)
#define SMU_THREAD_ON                  (1)

#define SMU_LOG_FILE_CREATE_WRITE      (0)
#define SMU_LOG_FILE_CREATE_FALLOCATE  (1)

/* BUG-27122 Restart Recovery�� UTrans�� �����ϴ� ��ũ �ε����� Verify ���
 *           (__SM_CHECK_DISK_INDEX_INTEGRITY = 2 ) �߰�
 *
 * DISABLED - check integrity ��� Off
 * LEVEL 1  - Restart Recovery �������� ��� ��ũ �ε��� Integrity Ȯ��
 * LEVEL 2  - Restart Recovery �������� UTrans�� �����ϴ� ��ũ �ε���
 *            Integrity Ȯ�� 
 * LEVEL 3  - __SM_VERIFY_DISK_INDEX_COUNT ��ŭ ����ڰ� ��������
 *            __SM_VERIFY_DISK_INDEXID ������Ƽ�� ������ �ε����� �����ϱ⵵
 *            �Ѵ�.
 */

#define SMU_MAX_VERIFY_DISK_INDEX_COUNT        (IDP_MAX_VERIFY_INDEX_COUNT)
#define SMU_CHECK_DSKINDEX_INTEGRITY_DISABLED  (0)
#define SMU_CHECK_DSKINDEX_INTEGRITY_LEVEL1    (1)
#define SMU_CHECK_DSKINDEX_INTEGRITY_LEVEL2    (2)
#define SMU_CHECK_DSKINDEX_INTEGRITY_LEVEL3    (3)

#define SMU_DBMS_STAT_METHOD_NONE   (-1)
#define SMU_DBMS_STAT_METHOD_MANUAL (0)
#define SMU_DBMS_STAT_METHOD_AUTO   (1)

#define SMU_IGNORE4EMERGENCY_COUNT     (IDP_MAX_VALUE_COUNT)

#define SMU_SECONDARY_BUFFER_DISABLE (0)
#define SMU_SECONDARY_BUFFER_ENABLE  (1)

/************************************************
 * PROJ-2201 Innovation in sorting and hashing(temp)
 * �ݺ����� Property����� ȸ���ϱ� ���� ���� 
 ************************************************/

#define SMU_PROPERTY_READONLY_DECLARE( aType, aValNFuncName )       \
    static aType m##aValNFuncName;                                  \
    static aType get##aValNFuncName()                               \
    {                                                               \
        return m##aValNFuncName;                                    \
    }

#define SMU_PROPERTY_WRITABLE_DECLARE( aType, aValNFuncName )       \
    static aType m##aValNFuncName;                                  \
    static IDE_RC callback##aValNFuncName( idvSQL * /*aStatistics*/,\
                                           SChar * /*aName*/,       \
                                           void  * /*aOldValue*/,   \
                                           void  * aNewValue,       \
                                           void  * /*aArg*/)        \
    {                                                               \
        m##aValNFuncName =  *((aType *)aNewValue);                  \
        return IDE_SUCCESS;                                         \
    }                                                               \
    static aType get##aValNFuncName()                               \
    {                                                               \
        return m##aValNFuncName;                                    \
    }

#define SMU_PROPERTY_WRITABLE_STR_DECLARE( aValNFuncName )          \
    static SChar *m##aValNFuncName;                                 \
    static IDE_RC callback##aValNFuncName( idvSQL * /*aStatistics*/,\
                                           SChar * /*aName*/,       \
                                           void  * /*aOldValue*/,   \
                                           void  * aNewValue,       \
                                           void  * /*aArg*/)        \
    {                                                               \
        m##aValNFuncName =  (SChar *)aNewValue;                     \
        return IDE_SUCCESS;                                         \
    }                                                               \
    static SChar *get##aValNFuncName()                              \
    {                                                               \
        return m##aValNFuncName;                                    \
    }


#define SMU_PROPERTY_WRITABLE_REGIST( aPropName, aValNFuncName ) \
    IDE_ASSERT( idp::read( aPropName,                            \
                           &m##aValNFuncName  )                  \
                == IDE_SUCCESS );                                \
    idp::setupAfterUpdateCallback( aPropName,                    \
                                   callback##aValNFuncName );


#define SMU_PROPERTY_READONLY_REGIST( aPropName, aValNFuncName ) \
    IDE_ASSERT( idp::read( aPropName,                            \
                           &m##aValNFuncName  )                  \
                == IDE_SUCCESS );

#define SMU_PROPERTY_READONLY_PTR_REGIST( aPropName, aValNFuncName ) \
    IDE_ASSERT( idp::readPtr( aPropName,                             \
                              (void**)&m##aValNFuncName  )           \
                == IDE_SUCCESS );


class smuProperty
{
private:
    // property�� load�Ǿ��°�
    // �ѹ� �ε�Ǹ� 2�� �̻� �� �ʿ� ����.
    static idBool mIsLoaded;

public:

    // sdd
    static UInt mOpenDataFileWaitInterval; // ���ð����� (property)
    static UInt mBackupDataFileEndWaitInterval; //���Ϲ����� ����
    static UInt mMaxOpenDataFileCount; // �ִ� ���°����� datafile ����
    static ULong mDataFileWriteUnitSize; // ���������� ������ ���� ���� ����
    static UInt mDWDirCount;
    static SChar* mDWDir[SM_MAX_DWDIR_COUNT];

    /* To Fix TASK-2688 DRDB�� Datafile�� �������� FD( File Descriptor )
     * ��� �����ؾ� ��
     *
     * �ϳ��� DRDB�� Data File�� Open�� �� �ִ� FD�� �ִ� ����
     * */
    static UInt mMaxOpenFDCount4File;

    // sdb
    //static UInt mValidateBuffer;
    static UInt mUseDWBuffer;

    static UInt mBulkIOPageCnt4DPInsert;
    static UInt mDPathBuffPageCnt;
    static UInt mDBFileMultiReadCnt;
    static UInt mSmallTblThreshold;
    static UInt mFlusherBusyConditionCheckInterval;
    static SLong mDPathBuffPageAllocRetryUSec;  // PROJ-2068
    static idBool mDPathInsertEnable;

    /* PROJ-2669 */
    static UInt mDelayedFlushListPct;
    static UInt mDelayedFlushProtectionTimeMsec;

    // To implement PRJ-1568
    static UInt mHotTouchCnt;
    static UInt mBufferVictimSearchInterval;
    static UInt mBufferVictimSearchPct;
    static UInt mHotListPct;
    static UInt mBufferHashBucketDensity;
    static UInt mBufferHashChainLatchDensity;
    static UInt mBufferHashPermute1;
    static UInt mBufferHashPermute2;
    static UInt mBufferLRUListCnt;
    static UInt mBufferFlushListCnt;
    static UInt mBufferPrepareListCnt;
    static UInt mBufferCheckPointListCnt;
    static UInt mBufferFlusherCnt;
    static UInt mBufferIOBufferSize;
    static ULong mBufferAreaSize;
    static ULong mBufferAreaChunkSize;
    //BUG-48042
    static UInt mBuffAreaCreateParDeg; 
    //static UInt mBufferPinningCount;
    //static UInt mBufferPinningHistoryCount;
    static UInt mDefaultFlusherWaitSec;
    static UInt mMaxFlusherWaitSec;
    static ULong mCheckpointFlushCount;
    static ULong mFastStartIoTarget;
    static UInt mFastStartLogFileTarget;
    static UInt mLowPreparePCT;
    static UInt mHighFlushPCT;
    static UInt mLowFlushPCT;
    static UInt mTouchTimeInterval;
    static UInt mCheckpointFlushMaxWaitSec;
    static UInt mCheckpointFlushMaxGap;
    static UInt mBlockAllTxTimeOut;
    //proj-1568 end


    // sdp
#if 0
    static ULong mTransWaitTime4TTS;
#endif
    static ULong mTransWaitTime;
    static UInt  mTmsIgnoreHintPID;
    static SInt  mTmsManualSlotNoInItBMP;
    static SInt  mTmsManualSlotNoInLfBMP;
    static UInt  mTmsCandidateLfBMPCnt;
    static UInt  mTmsCandidatePageCnt;
    static UInt  mTmsMaxSlotCntPerRtBMP;
    static UInt  mTmsMaxSlotCntPerItBMP;
    static UInt  mTmsMaxSlotCntPerExtDir;
    static UInt  mTmsDelayedAllocHintPageArr;
    static UInt  mTmsHintPageArrSize;

    static UInt  mPctFree;
    static UInt  mPctUsed;
    static UInt  mTableInitTrans;
    static UInt  mTableMaxTrans;
    static UInt  mIndexInitTrans;
    static UInt  mIndexMaxTrans;
    static UInt  mSegStoInitExtCnt;
    static UInt  mSegStoNextExtCnt;
    static UInt  mSegStoMinExtCnt;
    static UInt  mSegStoMaxExtCnt;

    // PROJ-1704 Disk MVCC Renewal
    static UInt  mTSSegExtDescCntPerExtDir;
    static UInt  mUDSegExtDescCntPerExtDir;
    static UInt  mTSSegSizeShrinkThreshold;
    static UInt  mUDSegSizeShrinkThreshold;
    static UInt  mRetryStealCount;

    static UInt  mCheckSumMethod;
    // To verify CASE-6829
    static UInt  mSMChecksumDisable;
    // BUG-17526
    static UInt  mCheckDiskAgerStat;
    static UInt  mExtDescCntInExtDirPage;

    // sdn
    static UInt  mMaxTempHashTableCount;
    static ULong mSortAreaSize;
    static UInt  mMergePageCount;
    static UInt  mKeyRedistributionLowLimit;
    static SLong mMaxTraverseLength;
    static UInt  mUnbalancedSplitRate;
    // BUG-29506 TBT�� TBK�� ��ȯ�� ����� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    static UInt  mDisableTransactionBoundInCTS;

    // BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
    // �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
    static UInt  mManualBindingTXSegByEntryID;

    // PROJ-1591
    static UInt mRTreeSplitMode;
    static UInt mRTreeSplitRate;
    static UInt mRTreeMaxKeyCount;

    // sdc
#if 0 // not used
    static UInt mTssCntPctToBufferPool;
#endif
    // To verify CASE-6829
    static UInt mSMAgerDisable;

    // smm
    static UInt   mDirtyPagePool;
    static SChar *mDBName;
    static ULong  mMaxDBSize;
    static ULong  mDefaultMemDBFileSize;
    static UInt   mDefaultExtCntForExtentGroup;
    static UInt   mMemDPFlushWaitTime;
    static UInt   mMemDPFlushWaitSID;
    static UInt   mMemDPFlushWaitPID;
    static UInt   mMemDPFlushWaitOffset;

    static UInt   mIOType;
    static UInt   mSyncCreate;
    static UInt   mCreateMethod;
    static UInt   mTempPageChunkCount;
    static ULong  mSCNSyncInterval;

    static UInt   mCheckStartupVersion;
    static UInt   mCheckStartupBitMode;
    static UInt   mCheckStartupEndian;
    static UInt   mCheckStartupLogSize;

    static ULong  mShmChunkSize;
    static UInt   mRestoreMethod;
    static UInt   mRestoreThreadCount;
    static UInt   mRestoreAIOCount;

    /* BUG-�����ͺ��̽� ���� �ε��,
       �ѹ��� ���Ͽ��� �޸𸮷� �о���� ������ �� */
    static UInt   mRestoreBufferPageCount;

    static UInt   mCheckpointAIOCount;
    static UInt   mCheckpointFlushResponsibility;

    // PROJ-1490 ����������Ʈ ����ȭ �� �޸𸮹ݳ�
    // �ϳ��� Expand Chunk�� ���ϴ� Page��
    static UInt  mExpandChunkPageCount;

    // ������ ���� Page List���� ���� ��� List�� ����ȭ �� �� �����Ѵ�.
    //
    // �����ͺ��̽� Free Page List
    // ���̺��� Allocated Page List
    // ���̺��� Free Page List
    static UInt   mPageListGroupCount;

    // Expand ChunkȮ��ÿ� Free Page���� �������� ���ļ�
    // ����ȭ�� Free Page List�� �й�ȴ�.
    //
    // �� ��, �ѹ��� ��� Page�� Free Page List�� �й������� �����Ѵ�.
    static UInt   mPerListDistPageCount;

    // Free Page List�� �������� ����ȭ�ʿ� ����,
    // Free Page�� Ư�� Free Page List�� ���� �� �ִ�.
    //
    // �̸� �л��Ű�� ���ؼ�, Free Page List�� Free Page�� ������ ���,
    // �ٸ� ���� Free Page�� ���� ���� Free Page List���� ������ �� ����´�.
    //
    // �� �� Free Page List�� ���� �Ǳ� ���� ������ �ϴ�
    // �ּ����� Page���� MIN_PAGES_ON_DB_FREE_LIST ������Ƽ�� �����Ѵ�.
    static UInt mMinPagesOnDBFreeList ;

    /* 
     * BUG-35443 Add Property for Exceping SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE
     */
    static UInt mSeparateDicTBSSizeEnable;

// smr
    static UInt   mChkptBulkWritePageCount;
    static UInt   mChkptBulkWriteSleepSec;
    static UInt   mChkptBulkWriteSleepUSec;
    static UInt   mChkptBulkSyncPageCount;

    /* BUG-33008 [sm-disk-collection] [DRDB] The migration row logic
     * generates invalid undo record.
     * ���׿� ���� ���� ó�� ���� ���� */
    static UInt   mAfterCare;
    /* BUG-27776 */
    static UInt  mSMStartupTest;

    /* BUG-36662 Add property for archive thread to kill server when doesn't
     * exist source logfile */
    static UInt  mCheckLogFileWhenArchThrAbort;

    static UInt   mChkptEnabled;
    static UInt   mChkptIntervalInSec;
    static UInt   mChkptIntervalInLog;

    static UInt   mSyncIntervalSec;
    static UInt   mSyncIntervalMSec;

    /* BUG-35392 */
    static UInt   mUCLSNChkThrInterval;
    static UInt   mLFThrSyncWaitMin;
    static UInt   mLFThrSyncWaitMax;
    static UInt   mLFGMgrSyncWaitMin;
    static UInt   mLFGMgrSyncWaitMax;

    static UInt   mDPathBuffFThreadSyncInterval;

    static SChar* mLogAnchorDir[SM_LOGANCHOR_FILE_COUNT];
    static SChar* mArchiveDirPath;
    static UInt   mArchiveFullAction;
    static UInt   mArchiveThreadAutoStart;
    static UInt   mArchiveMultiplexCount;
    static UInt   mArchiveMultiplexDirCount;
    static SChar* mArchiveMultiplexDirPath[ SM_ARCH_MULTIPLEX_PATH_CNT ];

    static SChar* mLogDirPath;
    static UInt   mLogMultiplexCount;
    static UInt   mLogMultiplexDirCount; 
    static SChar* mLogMultiplexDirPath[ SM_LOG_MULTIPLEX_PATH_MAX_CNT ]; 
    static UInt   mLogMultiplexThreadSpinCount;

    static UInt   mFileInitBufferSize;
    static ULong  mLogFileSize;
    static UInt   mZeroSizeLogFileAutoDelete;
    static UInt   mLogFilePrepareCount;
    static idBool mUseTempForPrepareLogFile;
    static UInt   mLogFilePreCreateInterval;
    //static UInt   mMaxKeepLogFile;
    static UInt   mShmDBKey;
    static UInt   mShmPageCountPerKey;

    static UInt   mMinLogRecordSizeForCompress; // ������ �α��� �ּ� ũ��

    static UInt   mLogAllocMutexType;
    static UInt   mLogReadMethodType;
    static idBool mFastUnlockLogAllocMutex; /* BUG-35392 */
    static UInt   mLogCompAcceleration;     /* BUG-45744 */ 

    // TASK-2398 Log Compress
    // ���� �����Ǿ� �ؽ̵� Disk Log�� ������ �����ص� ������ ũ��
    static ULong  mDiskRedoLogDecompressBufferSize;


    // �ϳ��� LFG���� Group Commit�� �����ϱ� ���� Update Transaction�� ��
    static UInt   mLFGGroupCommitUpdateTxCount;

    // ���ÿ� ������ Log Flush��û�� ������ ���͹�
    static UInt   mLFGGroupCommitIntervalUSec;

    // Log Flush��û�� ������ Transaction���� Log�� FLush�Ǿ�����
    // üũ�� ���͹�
    static UInt   mLFGGroupCommitRetryUSec;

    // Log ��Ͻ� IOŸ��
    // 0 : buffered IO
    // 1 : direct IO
    static UInt   mLogIOType;

    // BUG-15396 : log buffer type ( mmap == 0, memory == 1 )
    static UInt   mLogBufferType;

    // �α� ���� ���ҽ� Ǯ�� ������ �ּ����� ���ҽ� ����
    static UInt   mMinCompressionResourceCount;

    // �α� ���� ���ҽ� Ǯ���� ���ҽ��� �� �� �̻�
    // ������ ���� ��� Garbage Collection����?
    static ULong  mCompressionResourceGCSecond;

    /*PROJ-2162 RestartRiskReduction Begin */
    static UInt  mSmEnableStartupBugDetector;
    static UInt  mSmMtxRollbackTest;
    static UInt  mEmergencyStartupPolicy;
    static UInt  mCrashTolerance;
    static UInt  mSmIgnoreLog4EmergencyCount;
    static UInt  mSmIgnoreLFGID4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    static UInt  mSmIgnoreFileNo4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    static UInt  mSmIgnoreOffset4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    static UInt  mSmIgnorePage4EmergencyCount;
    static ULong mSmIgnorePage4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    static UInt  mSmIgnoreTable4EmergencyCount;
    static ULong mSmIgnoreTable4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    static UInt  mSmIgnoreIndex4EmergencyCount;
    static UInt  mSmIgnoreIndex4Emergency[SMU_IGNORE4EMERGENCY_COUNT];
    /*PROJ-2162 RestartRiskReduction End */
    
    /* PROJ-2133 Incremental Backup Begin */
    static UInt mBackupInfoRetentionPeriod;
    static UInt mBackupInfoRetentionPeriodForTest;
    static UInt mIncrementalBackupChunkSize;
    static UInt mBmpBlockBitmapSize;
    static UInt mCTBodyExtCnt;
    static UInt mIncrementalBackupPathMakeABSPath;
    /* PROJ-2133 Incremental Backup End */

    /* BUG-38515 Add hidden property for skip scn check in startup */
    static UInt mSkipCheckSCNInStartup;

// smp
    // FreePageList�� �ּ� ������ Page ����
    static UInt   mMinPagesOnTableFreeList;
    // DB���� Table�� �Ҵ�޾ƿ��� Page ����
    static UInt   mAllocPageCount;
    static UInt   mMemSizeClassCount;

// smc
    static UInt   mTableBackupFileBufferSize;
    static UInt   mCheckDiskIndexIntegrity;
    static UInt   mVerifyDiskIndexCount;
    static UInt   mDiskIndexNameCntToVerify;
    static SChar *mVerifyDiskIndexName[SMU_MAX_VERIFY_DISK_INDEX_COUNT];
    static UInt   mIgnoreMemTbsMaxSize;
    static UInt   mEnableRowTemplate;

// sma
    static UInt mDeleteAgerCount;
    static UInt mAgerListCount;
    static UInt mLogicalAgerCount;
    static UInt mMinLogicalAgerCount;
    static UInt mMaxLogicalAgerCount;
    static UInt mAgerWaitMin;
    static UInt mAgerWaitMax;
    static UInt mAgerCommitInterval;
    static UInt mRefinePageCount;
    static UInt mTableCompactAtShutdown;
    static UInt mParallelLogicalAger;
    static UInt mParallelDeleteThread;
    static UInt mCatalogSlotReusable;

// sml
    static UInt mTableLockEnable;
    static UInt mTablespaceLockEnable;
    static SInt mDDLLockTimeOut; // PROJ-1548
    static UInt mLockNodeCacheCount; /* BUG-43408 */
    static UInt mSkipLockedTableAtFixedTable;

    /* PROJ-2734 */
    static idBool mDistributionDeadlockEnable;
    static ULong mDistributionDeadlockRiskLowWaitTime;
    static ULong mDistributionDeadlockRiskMidWaitTime;
    static ULong mDistributionDeadlockRiskHighWaitTime;

//smn
    static UInt mIndexBuildMinRecordCount;
    static UInt mIndexBuildThreadCount;
    static UInt mParallelLoadFactor;
    static UInt mIteratorMemoryParallelFactor;
    static UInt mMMDBDefIdxType;
    //static UInt mIndexStatParallelFactor;
    // PROJ-1629 : Run�� ũ��
    static ULong mMemoryIndexBuildRunSize;
    // PROJ-1629 : Key value�� build�ϱ� ���� threshold
    static ULong mMemoryIndexBuildValueLengthThreshold;
    // TASK-6006 : Index ���� Parallel Index Rebuilding
    static SInt mIndexRebuildParallelMode;
    /* PROJ-2433 */
    static UInt mMemBtreeNodeSize;
    static UInt mMemBtreeDefaultMaxKeySize;
    /* BUG-40509 Change Memory Index Node Split Rate */
    static UInt mMemoryIndexUnbalancedSplitRate;
    /* PROJ-2613 Key Redistribution In MRDB Index */
    static SInt mMemIndexKeyRedistribution;
    static SInt mMemIndexKeyRedistributionStandardRate;
    static idBool mScanlistMoveNonBlock;
    static idBool mIsCPUAffinity;
    static idBool mGatherIndexStatOnDDL; /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    // BUG-47540
    static idBool mSkipIdxStatNodeBound;

//smx
    static UInt  mRebuildMinViewSCNInterval;
    static UInt  mOIDListCount;
    static UInt  mTransTouchPageCntByNode;
    static UInt  mTransTouchPageCacheRatio;
    static UInt  mTransTblSize;
    static UInt  mTransTableFullTrcLogCycle;
    static ULong mLockTimeOut;
    static UInt  mReplLockTimeOut;
    static UInt  mTransAllocWaitTime;

    // Tx'PrivatePageListHashTable ũ��
    static UInt  mPrivateBucketCount;

    // PROJ-1671 Disk MVCC ������
    static UInt  mTXSEGEntryCnt;
    static UInt  mTXSegAllocWaitTime;

    /* BUG-47525 Group Commit */
    static UInt  mGroupCommitCnt;
    static UInt  mGroupCommitListCnt;

    static UInt  mTxOIDListMemPoolType;

    /* BUG-38515 Add hidden property for skip scn check in startup */
    static UInt  mTrcLogLegacyTxInfo;

    /* BUG-40790 */
    static UInt  mLobCursorHashBucketCount;
    static UInt  mLogCompResourceReuse; /* BUG-47365 */
    static UInt  mCompResTuneSize;      /* BUG-47365 */

    /* PROJ-2733 */
    static ULong mVersioningMinTime;

//smu
    static UInt   mArtDecreaseVal;
    static SChar* mDBDir[SM_DB_DIR_MAX_COUNT];
    static UInt   mDBDirCount;
    static SChar* mDefaultDiskDBDir;

    /* TASK-4990 changing the method of collecting index statistics */
    /* BUG-21147 remove DRDB Index meta bottleneck */
    static UInt  mDBMSStatMethod;
    static UInt  mDBMSStatMethod4MRDB;
    static UInt  mDBMSStatMethod4VRDB;
    static UInt  mDBMSStatMethod4DRDB;
    static UInt  mDBMSStatSamplingBaseCnt;
    static UInt  mDBMSStatParallelDegree;
    static UInt  mDBMSStatGatherInternalStat;
    static UInt  mDBMSStatAutoInterval;
    static UInt  mDBMSStatAutoPercentage;

    static UInt  mDefaultSegMgmtType;
    static ULong mSysDataTBSExtentSize;
    static ULong mSysDataFileInitSize;
    static ULong mSysDataFileMaxSize;
    static ULong mSysDataFileNextSize;

    static ULong mSysUndoTBSExtentSize;
    static ULong mSysUndoFileInitSize;
    static ULong mSysUndoFileMaxSize;
    static ULong mSysUndoFileNextSize;

    static ULong mSysTempTBSExtentSize;
    static ULong mSysTempFileInitSize;
    static ULong mSysTempFileMaxSize;
    static ULong mSysTempFileNextSize;

    static ULong mUserDataTBSExtentSize;
    static ULong mUserDataFileInitSize;
    static ULong mUserDataFileMaxSize;
    static ULong mUserDataFileNextSize;

    static ULong mUserTempTBSExtentSize;
    static ULong mUserTempFileInitSize;
    static ULong mUserTempFileMaxSize;
    static ULong mUserTempFileNextSize;

    static UInt  mLockEscMemSize;
    static UInt  mIndexBuildBottomUpThreshold;
    static UInt  mTableBackupTimeOut;
    /* BUG-38621 */
    static UInt  mRELPathInLog;
    /* PROJ-2433 */
    static idBool mForceIndexDirectKey;
    /* BUG-41121 */
    static UInt mForceIndexPersistenceMode;
    /* PROJ-2733 */
    static UInt mShardEnable;

    //svm
    static ULong  mVolMaxDBSize;

    //scp
    /* Proj-2059 DB Upgrade ��� */
    static UInt mDataPortFileBlockSize;
    static UInt mExportColumnChainingThreshold;
    static UInt mDataPortDirectIOEnable;

    //sds
    /* PROJ-2102 Fast Secondary Buffer */
    static UInt   mSBufferEnable;
    static UInt   mSBufferFlusherCount;
    static UInt   mMaxSBufferCPFlusherCnt;
    static UInt   mSBufferType;
    static ULong  mSBufferSize;
    static SChar *mSBufferFileDirectory;

    //util
    static UInt   mPortNo;
    //Server�� StartUp�ɶ� �� Property�� 1�� ��쿡�� Index Rebuild�� ����
    //�ϰ� 0�̸� Meta Table�� Index���� Rebuild�ϰ� �ٸ� Table�� Build����
    //�ʴ´�.
    static UInt   mIndexRebuildAtStartup;

    /* Proj-2162 RestartRiskReduction
     * ���� ������ �ε��� ���� ������ ���� */
    static UInt   mIndexRebuildParallelFactorAtSartup;

    static IDE_RC callbackDPathBufferFlushThreadInterval(idvSQL * /*aStatistics*/,
                                                         SChar * /*aName*/,
                                                         void  * /*aOldValue*/,
                                                         void  *aNewValue,
                                                         void  * /*aArg*/);

    //-------------------------------------
    // To Fix PR-14783
    // System Thread�� ������ ������.
    //-------------------------------------

    static UInt   mRunMemDeleteThread;
    static UInt   mRunMemGCThread;
    static UInt   mRunBufferFlushThread;
    static UInt   mRunArchiveThread;
    static UInt   mRunCheckpointThread;
    static UInt   mRunLogFlushThread;
    static UInt   mRunLogPrepareThread;
    static UInt   mRunLogPreReadThread;

    /*********************************************************************
     * TASK-6198 Samsung Smart SSD GC control
     *********************************************************************/
#ifdef ALTIBASE_ENABLE_SMARTSSD
    static UInt   mSmartSSDLogRunGCEnable;
    static SChar* mSmartSSDLogDevice;
    static UInt   mSmartSSDGCTimeMilliSec;
#endif

    // PRJ-1864 Partial Write Problem�� ���� ���� �ذ�.
    static UInt   mCorruptPageErrPolicy;

    static ULong  mReservedDiskSizeForLogFile;

    /*********************************************************************
     * PROJ-2201 Innovation in sorting and hashing(temp)
     *********************************************************************/
    SMU_PROPERTY_READONLY_DECLARE( ULong, HashAreaSize ); /* Callback�� ����*/
    SMU_PROPERTY_READONLY_DECLARE( ULong, InitTotalWASize ); /* Callback�� ����*/
    SMU_PROPERTY_READONLY_DECLARE( ULong, MaxTotalWASize );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TmpCheckUnique4Update );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TmpMinInitWAExtCnt );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TmpOverInitWAExtCnt );
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TmpInitWASegCnt ); /* Callback�� ����*/

    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TmpAllocWAExtCnt );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TmpHashFetchSubHashMaxRatio );

    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TempSortPartitionSize );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TempSortGroupRatio );
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TempHashGroupRatio );/* Callback����*/
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TempSubHashGroupRatio );/* Callback����*/
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TempAllocTryCount );
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TempRowSplitThreshold );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TempSleepInterval );
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TempMaxKeySize );
    SMU_PROPERTY_READONLY_DECLARE( UInt,  TempStatsWatchArraySize );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt,  TempStatsWatchTime );
    SMU_PROPERTY_WRITABLE_STR_DECLARE( TempDumpDirectory );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt, TempDumpEnable );
    SMU_PROPERTY_WRITABLE_DECLARE( idBool, WCBCleanMemset );
    SMU_PROPERTY_WRITABLE_DECLARE( UInt, SmTempOperAbort );
    SMU_PROPERTY_READONLY_DECLARE( UInt, TempHashBucketDensity );

    static IDE_RC callbackHashAreaSize( idvSQL * /*aStatistics*/,
                                        SChar * /*aName*/,
                                        void  * /*aOldValue*/,
                                        void  * aNewValue,
                                        void  * /*aArg*/);

    static IDE_RC callbackTempHashGroupRatio( idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  * aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackTempSubHashGroupRatio( idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  * aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackInitTotalWASize(idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  * aNewValue,    
                                          void  * /*aArg*/);
    static IDE_RC callbackMaxTotalWASize(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  * aNewValue,    
                                         void  * /*aArg*/);
    static IDE_RC callbackTmpInitWASegCnt(idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  * aNewValue,    
                                          void  * /*aArg*/);

    /* PROJ-2620 */
    SMU_PROPERTY_READONLY_DECLARE(SInt, LockMgrCacheNode);
private:

    //PROJ-2232 log multiplex
    static IDE_RC checkDuplecateMultiplexDirPath(
                                    SChar ** aArchMultiplexDirPath,
                                    SChar ** aLogMultiplexDirPath,
                                    UInt     aArchMultiplexDirCnt,
                                    UInt     aLogMultiplexDirCnt );

    static void initialize();
    static void destroy();

    static void loadForSDD();
    static void loadForSDB();
    static void loadForSDP();
    static void loadForSDC();
    static void loadForSDN();
    static void loadForSDR();

    static void loadForSMM();
    static void loadForSMR();
    static void loadForSMR_LogFile();     // To Fix PR-13786
    static void loadForSMP();
    static void loadForSMC();
    static void loadForSMA();
    static void loadForSML();

    static void loadForSMN();
    static void loadForSMX();
    static void loadForSMU();
    static void loadForSMI();
    static void loadForSCP();
    static void loadForSDS();             //PROJ-2102
    static void loadForUTIL();
    

    static void loadForSystemThread();


#ifdef ALTIBASE_ENABLE_SMARTSSD
    static void loadForSmartSSD();
#endif

    static void loadForLockMgr();

    static void registCallbacks();

    static IDE_RC checkConstraints();

    static IDE_RC callbackDBMSStatMethod(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  *aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackDBMSStatMethod4MRDB(idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackDBMSStatMethod4VRDB(idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackDBMSStatMethod4DRDB(idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackDBMSStatSamplingBaseCnt(idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  *aNewValue,
                                                  void  * /*aArg*/);
    static IDE_RC callbackDBMSStatParallelDegree(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);
    static IDE_RC callbackDBMSStatGatherInternalStat(idvSQL * /*aStatistics*/,
                                                     SChar * /*aName*/,
                                                     void  * /*aOldValue*/,
                                                     void  *aNewValue,
                                                     void  * /*aArg*/);

    static IDE_RC callbackDBMSStatAutoPercentage(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackDBMSStatAutoInterval(idvSQL * /*aStatistics*/,
                                               SChar * /*aName*/,
                                               void  * /*aOldValue*/,
                                               void  *aNewValue,
                                               void  * /*aArg*/);

    static IDE_RC callbackTableBackupTimeOut(idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * /*aArg*/);

    /* BUG-38621 */
    static IDE_RC callbackRELPathInLog(idvSQL * /*aStatistics*/,
                                       SChar * /*aName*/,
                                       void  * /*aOldValue*/,
                                       void  *aNewValue,
                                       void  * /*aArg*/);

    static IDE_RC callbackChkptBulkWritePageCount(idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  *aNewValue,
                                                  void  * /*aArg*/);

    static IDE_RC callbackChkptBulkWriteSleepSec(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackChkptBulkWriteSleepUSec(idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  *aNewValue,
                                                  void  * /*aArg*/);

    static IDE_RC callbackChkptBulkSyncPageCount(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackChkptIntervalInSec(idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackChkptIntervalInLog(idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackLockEscMemSize(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  *aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackDDLLockTimeOut(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  *aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackLockTimeOut(idvSQL * /*aStatistics*/,
                                      SChar * /*aName*/,
                                      void  * /*aOldValue*/,
                                      void  *aNewValue,
                                      void  * /*aArg*/);
    
    static IDE_RC callbackUseTempForPrepareLogFile(idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  *aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackLogFilePreCreateInterval(idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  *aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackReplLockTimeOut(idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  *aNewValue,
                                          void  * /*aArg*/);

    static IDE_RC callbackSyncCreate(idvSQL * /*aStatistics*/,
                                     SChar * /*aName*/,
                                     void  * /*aOldValue*/,
                                     void  *aNewValue,
                                     void  * /*aArg*/);

    static IDE_RC callbackOpenDataFileWaitInterval(idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  *aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackBufferPoolSize(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  *aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackBufferPoolMaxSize(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  *aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackBulkIOPageCnt4DPInsert( idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  *aNewValue,
                                                  void  * /*aArg*/);

    static IDE_RC callbackDPathBuffPageCnt( idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  *aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackDBFileMultiReadCnt( idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackSmallTblThreshold( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackFlusherBusyConditionCheckInterval( idvSQL * /*aStatistics*/,
                                                             SChar * /*aName*/,
                                                             void  * /*aOldValue*/,
                                                             void  *aNewValue,
                                                             void  * /*aArg*/);

    static IDE_RC callbackDPathBuffPageAllocRetryUSec(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/ );

    static IDE_RC callbackDPathInsertEnable( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * /*aArg*/ );

    static IDE_RC callbackReservedDiskSizeForLogFile( idvSQL * /*aStatistics*/,
                                                      SChar * /*aName*/,
                                                      void  * /*aOldValue*/,
                                                      void  * aNewValue,
                                                      void  * /*aArg*/);


    static IDE_RC callbackMemDPFlushWaitTime( idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  * aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackMemDPFlushWaitSID( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  * aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackMemDPFlushWaitPID( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  * aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackMemDPFlushWaitOffset( idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  * aNewValue,
                                                void  * /*aArg*/);
    
    static IDE_RC callbackTableLockEnable(idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  *aNewValue,
                                          void  * aArg);

    static IDE_RC callbackTablespaceLockEnable(idvSQL * /*aStatistics*/,
                                               SChar * /*aName*/,
                                               void  * /*aOldValue*/,
                                               void  *aNewValue,
                                               void  * aArg);
    static IDE_RC callbackSkipLockedTableAtFixedTable(idvSQL * /*aStatistics*/,
                                                      SChar * /*aName*/,
                                                      void  * /*aOldValue*/,
                                                      void  *aNewValue,
                                                      void  * aArg);
    static IDE_RC callbackTransAllocWaitTime(idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  *aNewValue,
                                             void  * aArg);

    static IDE_RC callbackTransTableFullTrcLogCycle(idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  *aNewValue,
                                                    void  * aArg);

    static IDE_RC callbackWaitCountToMakeFreeUndoBuffer(idvSQL * /*aStatistics*/,
                                                        SChar * /*aName*/,
                                                        void  * /*aOldValue*/,
                                                        void  *aNewValue,
                                                        void  * aArg);

    /* PROJ-2669 */
    static IDE_RC callbackDelayedFlushListPct( idvSQL * /*aStatistics*/,
                                               SChar * /*aName*/,
                                               void  * /*aOldValue*/,
                                               void  * aNewValue,
                                               void  * /*aArg*/);

    /* PROJ-2669 */
    static IDE_RC callbackDelayedFlushProtectionTimeMsec( idvSQL * /*aStatistics*/,
                                                          SChar * /*aName*/,
                                                          void  * /*aOldValue*/,
                                                          void  * aNewValue,
                                                          void  * /*aArg*/);

    // PROJ-1568
    static IDE_RC callbackHotTouchCnt(idvSQL * /*aStatistics*/,
                                      SChar * /*aName*/,
                                      void  * /*aOldValue*/,
                                      void  * aNewValue,
                                      void  * /*aArg*/);

    static IDE_RC callbackBufferVictimSearchInterval(idvSQL * /*aStatistics*/,
                                                     SChar * /*aName*/,
                                                     void  * /*aOldValue*/,
                                                     void  * aNewValue,
                                                     void  * /*aArg*/);

    static IDE_RC callbackBufferVictimSearchPct(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  * aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackHotListPct(idvSQL * /*aStatistics*/,
                                     SChar * /*aName*/,
                                     void  * /*aOldValue*/,
                                     void  * aNewValue,
                                     void  * /*aArg*/);

    static IDE_RC callbackBufferAreaSize(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  * aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackDefaultFlusherWaitSec(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  * aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackMaxFlusherWaitSec(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  * aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackCheckpointFlushCount(idvSQL * /*aStatistics*/,
                                               SChar * /*aName*/,
                                               void  * /*aOldValue*/,
                                               void  * aNewValue,
                                               void  * /*aArg*/);

    static IDE_RC callbackFastStartIoTarget(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  * aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackFastStartLogFileTarget(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  * aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackLowPreparePCT(idvSQL * /*aStatistics*/,
                                        SChar * /*aName*/,
                                        void  * /*aOldValue*/,
                                        void  * aNewValue,
                                        void  * /*aArg*/);

    static IDE_RC callbackHighFlushPCT(idvSQL * /*aStatistics*/,
                                       SChar * /*aName*/,
                                       void  * /*aOldValue*/,
                                       void  * aNewValue,
                                       void  * /*aArg*/);

    static IDE_RC callbackLowFlushPCT(idvSQL * /*aStatistics*/,
                                      SChar * /*aName*/,
                                      void  * /*aOldValue*/,
                                      void  * aNewValue,
                                      void  * /*aArg*/);

    static IDE_RC callbackTouchTimeInterval(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  * aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackCheckpointFlushMaxWaitSec(idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  * aNewValue,
                                                    void  * /*aArg*/);

    static IDE_RC callbackCheckpointFlushMaxGap(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  * aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackCheckpointFlushResponsibility(idvSQL * /*aStatistics*/,
                                                        SChar * /*aName*/,
                                                        void  * /*aOldValue*/,
                                                        void  * aNewValue,
                                                        void  * /*aArg*/);

    static IDE_RC callbackTransTouchPageCntByNode(idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  * aNewValue,
                                                  void  * /*aArg*/);

    static IDE_RC callbackTransTouchPageCacheRatio(idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  * aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackTableCompactAtShutdown( idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  *aNewValue,
                                                  void  * /*aArg*/ );
    
    static IDE_RC callbackBlockAllTxTimeOut(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  * aNewValue,
                                            void  * /*aArg*/);
    //proj-1568 end


    //-------------------------------------
    // To Fix PR-14783
    // System Thread�� ������ ������.
    //-------------------------------------

    static IDE_RC callbackDiskDeleteThread(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);
    static IDE_RC callbackDiskGCThread(idvSQL * /*aStatistics*/,
                                       SChar * /*aName*/,
                                       void  * /*aOldValue*/,
                                       void  *aNewValue,
                                       void  * /*aArg*/);
    static IDE_RC callbackMemDeleteThread(idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  *aNewValue,
                                          void  * /*aArg*/);
    static IDE_RC callbackMemGCThread(idvSQL * /*aStatistics*/,
                                      SChar * /*aName*/,
                                      void  * /*aOldValue*/,
                                      void  *aNewValue,
                                      void  * /*aArg*/);
    static IDE_RC callbackBufferFlushThread(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  *aNewValue,
                                            void  * /*aArg*/);
    static IDE_RC callbackArchiveThread(idvSQL * /*aStatistics*/,
                                        SChar * /*aName*/,
                                        void  * /*aOldValue*/,
                                        void  *aNewValue,
                                        void  * /*aArg*/);
    static IDE_RC callbackCheckpointThread(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);
    static IDE_RC callbackLogFlushThread(idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  *aNewValue,
                                         void  * /*aArg*/);
    static IDE_RC callbackLogPrepareThread(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);
    static IDE_RC callbackLogPreReadThread(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);
    //BUG-45598 CHECKSUM_METHOD �ݹ� ���
    static IDE_RC callbackSMChecksumMethod(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);

    static IDE_RC callbackSMChecksumDisable( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  * aNewValue,
                                             void  * /*aArg*/);

    static IDE_RC callbackSMAgerDisable( idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  * aNewValue,
                                         void  * /*aArg*/);

    static IDE_RC callbackCheckDiskIndexIntegrity( idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  * aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackMinLogRecordSizeForCompress( idvSQL * /*aStatistics*/,
                                                       SChar * /*aName*/,
                                                       void  * /*aOldValue*/,
                                                       void  * aNewValue,
                                                       void  * /*aArg*/);

    static IDE_RC callbackAfterCare( idvSQL * /*aStatistics*/,
                                     SChar * /*aName*/,
                                     void  * /*aOldValue*/,
                                     void  * aNewValue,
                                     void  * /*aArg*/);

    static IDE_RC callbackChkptEnabled( idvSQL * /*aStatistics*/,
                                        SChar * /*aName*/,
                                        void  * /*aOldValue*/,
                                        void  * aNewValue,
                                        void  * /*aArg*/);

    static IDE_RC callbackSeparateDicTBSSizeEnable( idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  * aNewValue,
                                                    void  * /*aArg*/);


    static IDE_RC callbackSmEnableStartupBugDetector( idvSQL * /*aStatistics*/,
                                                      SChar * /*aName*/,
                                                      void  * /*aOldValue*/,
                                                      void  * aNewValue,
                                                      void  * /*aArg*/);
    static IDE_RC callbackSmMtxRollbackTest( idvSQL * /*aStatistics*/,
                                             SChar * /*aName*/,
                                             void  * /*aOldValue*/,
                                             void  * aNewValue,
                                             void  * /*aArg*/);
    static IDE_RC callbackCrashTolerance( idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  * aNewValue,
                                          void  * /*aArg*/);

    // PROJ-1704 Disk MVCC ������
    static IDE_RC callbackTSSegSizeShrinkThreshold( idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  * aNewValue,
                                                    void  * /*aArg*/);

    static IDE_RC callbackUDSegSizeShrinkThreshold( idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  * aNewValue,
                                                    void  * /*aArg*/);

    static IDE_RC callbackRetryStealCount( idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  * aNewValue,
                                           void  * /*aArg*/);


    static IDE_RC callbackRebuildMinViewSCNInterval( idvSQL * /*aStatistics*/,
                                                     SChar * /*aName*/,
                                                     void  * /*aOldValue*/,
                                                     void  * aNewValue,
                                                     void  * /*aArg*/);

    static IDE_RC callbackDataFileWriteUnitSize(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  *aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackMaxOpenFDCount4File(idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackDiskIndexBottomUpBuildThreshold(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexKeyRedistributionLowLimit(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexMaxTraverseLength(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexUnbalancedSplitRate(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexRTreeMaxKeyCount(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexRTreeSplitMode(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    // BUG-32468 There is the need for rebuilding index statistics
    static IDE_RC callbackDiskIndexRebuildStat(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackDiskIndexBuildMergePageCount(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackSortAreaSize(idvSQL * /*aStatistics*/,
                                       SChar * /*aName*/,
                                       void  * /*aOldValue*/,
                                       void  *aNewValue,
                                       void  * /*aArg*/);

    // BUG-29506 TBT�� TBK�� ��ȯ�� ����� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    static IDE_RC callbackDisableTransactionBoundInCTS(idvSQL * /*aStatistics*/,
                                                       SChar * /*aName*/,
                                                       void  * /*aOldValue*/,
                                                       void  *aNewValue,
                                                       void  * /*aArg*/);

    // BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
    // �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
    static IDE_RC callbackManualBindingTXSegByEntryID(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackMemoryIndexBuildRunSize(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

    static IDE_RC callbackMemoryIndexBuildValueLengthThreshold(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/);

//    static IDE_RC callbackTssCntPctToBufferPool( idvSQL * /*aStatistics*/,
//                                                 SChar * /*aName*/,
//                                                 void  * /*aOldValue*/,
//                                                 void  * aNewValue,
//                                                 void  * /*aArg*/);

    static IDE_RC callbackShmPageCountPerKey( idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  * aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackMinPagesOnTableFreeList( idvSQL * /*aStatistics*/,
                                                   SChar * /*aName*/,
                                                   void  * /*aOldValue*/,
                                                   void  * aNewValue,
                                                   void  * /*aArg*/);

    static IDE_RC callbackAllocPageCount( idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  * aNewValue,
                                          void  * /*aArg*/);

    static IDE_RC callbackSyncIntervalSec( idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  * aNewValue,
                                           void  * /*aArg*/);

    static IDE_RC callbackSyncIntervalMSec( idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  * aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackMMDBDefIdxType( idvSQL * /*aStatistics*/,
                                          SChar * /*aName*/,
                                          void  * /*aOldValue*/,
                                          void  * aNewValue,
                                          void  * /*aArg*/);

    static IDE_RC callbackSetAgerCount(idvSQL * /*aStatistics*/,
                                       SChar * /*aName*/,
                                       void  * /*aOldValue*/,
                                       void  *aNewValue,
                                       void  * /*aArg*/);

    static IDE_RC callbackTransWaitTime4TTS(idvSQL * /*aStatistics*/,
                                            SChar * /*aName*/,
                                            void  * /*aOldValue*/,
                                            void  *aNewValue,
                                            void  * /*aArg*/);

    static IDE_RC callbackTransWaitTime( idvSQL * /*aStatistics*/,
                                         SChar * /*aName*/,
                                         void  * /*aOldValue*/,
                                         void  * aNewValue,
                                         void  * /*aArg*/ );

    static IDE_RC callbackTmsIgnoreHintPID(idvSQL * /*aStatistics*/,
                                           SChar * /*aName*/,
                                           void  * /*aOldValue*/,
                                           void  *aNewValue,
                                           void  * /*aArg*/);

    static IDE_RC callbackTmsManualSlotNoInItBMP(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);


    static IDE_RC callbackTmsManualSlotNoInLfBMP(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackTmsCandidateLfBMPCnt(idvSQL * /*aStatistics*/,
                                               SChar * /*aName*/,
                                               void  * /*aOldValue*/,
                                               void  *aNewValue,
                                               void  * /*aArg*/);

    static IDE_RC callbackTmsCandidatePageCnt(idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  *aNewValue,
                                              void  * /*aArg*/);

    static IDE_RC callbackTmsMaxSlotCntPerRtBMP(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  *aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackTmsMaxSlotCntPerItBMP(idvSQL * /*aStatistics*/,
                                                SChar * /*aName*/,
                                                void  * /*aOldValue*/,
                                                void  *aNewValue,
                                                void  * /*aArg*/);

    static IDE_RC callbackTmsMaxSlotCntPerExtDir(idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackDefaulExtCntForExtentGroup( idvSQL * /*aStatistics*/,
                                                      SChar * /*aName*/,
                                                      void  * /*aOldValue*/,
                                                      void  *aNewValue,
                                                      void  * /*aArg*/ );

    static IDE_RC callbackExtDescCntInExtDirPage(
        idvSQL * /*aStatistics*/,
        SChar * /*aName*/,
        void  * /*aOldValue*/,
        void  *aNewValue,
        void  * /*aArg*/ );

    static IDE_RC callbackIndexBuildThreadCount( idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  *aNewValue,
                                                 void  * /*aArg*/ );


    /* Proj-2059 DB Upgrade ��� */
    static IDE_RC callbackDataPortFileBlockSize( idvSQL * /*aStatistics*/,
                                                 SChar * /*aName*/,
                                                 void  * /*aOldValue*/,
                                                 void  * aNewValue,
                                                 void  * /*aArg*/);

    static IDE_RC callbackExportColumnChainingThreshold( idvSQL * /*aStatistics*/,
                                                         SChar * /*aName*/,
                                                         void  * /*aOldValue*/,
                                                         void  * aNewValue,
                                                         void  * /*aArg*/);

    static IDE_RC callbackDataPortDirectIOEnable( idvSQL * /*aStatistics*/,
                                                  SChar * /*aName*/,
                                                  void  * /*aOldValue*/,
                                                  void  * aNewValue,
                                                  void  * /*aArg*/);

    static IDE_RC callbackLogReadMethodType ( idvSQL * /*aStatistics*/,
                                              SChar  * /*aName*/,
                                              void   * /*aOldValue*/,
                                              void   * aNewValue,
                                              void   * /*aArg*/);
    /* Proj-2059 End */

    /* BUG-38515 __SM_SKIP_CHECKSCN_IN_STARTUP */
    static IDE_RC callbackTRCLogLegacyTxInfo( idvSQL * /*aStatistics*/,
                                              SChar * /*aName*/,
                                              void  * /*aOldValue*/,
                                              void  * aNewValue,
                                              void  * /*aArg*/ );

    static IDE_RC callbackLobCursorHashBucketCount( idvSQL * /*aStatistics*/,
                                                    SChar * /*aName*/,
                                                    void  * /*aOldValue*/,
                                                    void  * aNewValue,
                                                    void  * /*aArg*/ );

    /* BUG-40509 Change Memory Index Node Split Rate */
    static IDE_RC callbackMemoryIndexUnbalancedSplitRate( idvSQL * /*aStatistics*/,
                                                          SChar  * /*aName*/,
                                                          void   * /*aOldValue*/,
                                                          void   * aNewValue,
                                                          void   * /*aArg*/ );

    /* BUG-41550 */
    static IDE_RC callbackBackupInfoRetentionPeriodForTest( idvSQL * /*aStatistics*/,
                                                            SChar  * /*aName*/,
                                                            void   * /*aOldValue*/,
                                                            void   * aNewValue,
                                                            void   * /*aArg*/ );

    /* PROJ-2613 Key Redistribution in MRDB Index */
    static IDE_RC callbackMemIndexKeyRedistribution( idvSQL * /*aStatistics*/,
                                                     SChar  * /*aName*/,
                                                     void   * /*aOldValue*/,
                                                     void   * aNewValue,
                                                     void   * /*aArg*/ );

    /* PROJ-2613 Key Redistribution in MRDB Index */
    static IDE_RC callbackMemIndexKeyRedistributionStandardRate( idvSQL * /*aStatistics*/,
                                                                 SChar  * /*aName*/,
                                                                 void   * /*aOldValue*/,
                                                                 void   * aNewValue,
                                                                 void   * /*aArg*/ );

    static IDE_RC callbackScanlistMoveNonBlock( idvSQL * /*aStatistics*/,
                                                SChar  * /*aName*/,
                                                void   * /*aOldValue*/,
                                                void   * aNewValue,
                                                void   * /*aArg*/ );

    static IDE_RC callbackGatherIndexStatOnDDL( idvSQL * /*aStatistics*/,
                                                SChar  * /*aName*/,
                                                void   * /*aOldValue*/,
                                                void   * aNewValue,
                                                void   * /*aArg*/ );

    static IDE_RC callbackLogCompAcceleration( idvSQL * /*aStatistics*/,
                                               SChar  * /*aName*/,
                                               void   * /*aOldValue*/,
                                               void   * aNewValue,
                                               void   * /*aArg*/ );

    /* PROJ-2734 */
    static IDE_RC callbackDistributionDeadlockEnable( idvSQL * /*aStatistics*/,
                                                      SChar  * /*aName*/,
                                                      void   * /*aOldValue*/,
                                                      void   * aNewValue,
                                                      void   * /*aArg*/ );
    static IDE_RC callbackDistributionDeadlockRiskLowWaitTime( idvSQL * /*aStatistics*/,
                                                               SChar  * /*aName*/,
                                                               void   * /*aOldValue*/,
                                                               void   * aNewValue,
                                                               void   * /*aArg*/ );
    static IDE_RC callbackDistributionDeadlockRiskMidWaitTime( idvSQL * /*aStatistics*/,
                                                               SChar  * /*aName*/,
                                                               void   * /*aOldValue*/,
                                                               void   * aNewValue,
                                                               void   * /*aArg*/ );
    static IDE_RC callbackDistributionDeadlockRiskHighWaitTime( idvSQL * /*aStatistics*/,
                                                                SChar  * /*aName*/,
                                                                void   * /*aOldValue*/,
                                                                void   * aNewValue,
                                                                void   * /*aArg*/ );
    static IDE_RC callbackVersioningMinTime( idvSQL * /*aStatistics*/,
                                             SChar  * /*aName*/,
                                             void   * /*aOldValue*/,
                                             void   * aNewValue,
                                             void   * /*aArg*/ );


public:
    static IDE_RC load();

    /* PROJ-2162 RestartRiskReduction
     * dumpci �� util������ loganchor�� �����ϱ� ���� control�ܰ���� ������
     * �������Ѿ� �ϸ�, �̸� ���� BufferManager�� �ʱ�ȭ�ؾ� �մϴ�. 
     * �̷��� �������� Buffer�� �Ҵ��ϴµ� �ǹ̾��� �۾��� �մϴ�. ������
     * �̸� ������ ���� �����ϴ�. ���� Utility�� ���Ͽ� BUFFER_AREA_SIZE
     * �� �Ϻ� ������ �ּ������� �����Ͽ�, utility�� ���� ������ ���
     * ��ŵ�ϴ�. */
    static void init4Util();

    //sdd
    static UInt getOpenDataFileWaitInterval()
    {
        return mOpenDataFileWaitInterval;
    }

    static UInt getDataFileBackupEndWaitInterval()
    {
        return mBackupDataFileEndWaitInterval;
    }

    static UInt getMaxOpenDataFileCount()
    {
        return mMaxOpenDataFileCount;
    }

    static ULong getDataFileWriteUnitSize()
    {
        return mDataFileWriteUnitSize;
    }

    static ULong getMaxOpenFDCount4File()
    {
        return mMaxOpenFDCount4File;
    }

    static UInt getDWDirCount()
    {
        return mDWDirCount;
    }

    static SChar* getDWDir(UInt aIndex)
    {
        return mDWDir[aIndex];
    }

//    static UInt getValidateBuffer()
//    {
//        return mValidateBuffer;
//    }

    static idBool getUseDWBuffer()
    {
        return (mUseDWBuffer == 1 ? ID_TRUE : ID_FALSE);
    }

    static UInt getBulkIOPageCnt4DPInsert()
    {
        return mBulkIOPageCnt4DPInsert;
    }

    // PROJ-1568
    static UInt getHotTouchCnt()
    {
        return mHotTouchCnt;
    }

    static UInt getBufferVictimSearchInterval()
    {
        return mBufferVictimSearchInterval;
    }

    static UInt getBufferVictimSearchPct()
    {
        return mBufferVictimSearchPct;
    }

    /* PROJ-2669 */
    static UInt getDelayedFlushListPct()
    {
        return mDelayedFlushListPct;
    }

    /* PROJ-2669 */
    static UInt getDelayedFlushProtectionTimeMsec()
    {
        return mDelayedFlushProtectionTimeMsec;
    }

    static UInt getHotListPct()
    {
        return mHotListPct;
    }
    static UInt getBufferHashBucketDensity()
    {
        return mBufferHashBucketDensity;
    }

    static UInt getBufferHashChainLatchDensity()
    {
        return mBufferHashChainLatchDensity;
    }
    static UInt getBufferHashPermute1()
    {
        return mBufferHashPermute1;
    }
    static UInt getBufferHashPermute2()
    {
        return mBufferHashPermute2;
    }

    static UInt getBufferLRUListCnt()
    {
        return mBufferLRUListCnt;
    }

    static UInt getBufferFlushListCnt()
    {
        return mBufferFlushListCnt;
    }

    static UInt getBufferPrepareListCnt()
    {
        return mBufferPrepareListCnt;
    }

    static UInt getBufferCheckPointListCnt()
    {
        return mBufferCheckPointListCnt;
    }

    static UInt getBufferFlusherCnt()
    {
        return mBufferFlusherCnt;
    }

    static UInt getBufferIOBufferSize()
    {
        return mBufferIOBufferSize;
    }

    static ULong getBufferAreaSize()
    {
        return mBufferAreaSize;
    }

    static ULong getBufferAreaChunkSize()
    {
        return mBufferAreaChunkSize;
    }

    static UInt getBuffAreaCreatePDegree()
    {
        return mBuffAreaCreateParDeg;
    }

//    static UInt getBufferPinningCount()
//    {
//        return mBufferPinningCount;
//    }

//    static UInt getBufferPinningHistoryCount()
//    {
//        return mBufferPinningHistoryCount;
//    }

    static UInt getDefaultFlusherWaitSec()
    {
        return mDefaultFlusherWaitSec;
    }

    static UInt getMaxFlusherWaitSec()
    {
        return mMaxFlusherWaitSec;
    }

    static ULong getCheckpointFlushCount()
    {
        return mCheckpointFlushCount;
    }

    static ULong getFastStartIoTarget()
    {
        return mFastStartIoTarget;
    }

    static UInt getFastStartLogFileTarget()
    {
        return mFastStartLogFileTarget;
    }

    static UInt getLowPreparePCT()
    {
        return mLowPreparePCT;
    }

    static UInt getHighFlushPCT()
    {
        return mHighFlushPCT;
    }

    static UInt getLowFlushPCT()
    {
        return mLowFlushPCT;
    }

    static UInt getTouchTimeInterval()
    {
        return mTouchTimeInterval;
    }

    static UInt getCheckpointFlushMaxWaitSec()
    {
        return mCheckpointFlushMaxWaitSec;
    }

    static UInt getCheckpointFlushMaxGap()
    {
        return mCheckpointFlushMaxGap;
    }

    static UInt getBlockAllTxTimeOut()
    {
        return mBlockAllTxTimeOut;
    }
    //proj-1568 end


    //sdp
#if 0
    static ULong getTransWaitTime4TTS()
    {
        return mTransWaitTime4TTS;
    }
#endif

    static ULong getTransWaitTime()
    {
        return mTransWaitTime;
    }

    static UInt getTmsIgnoreHintPID()
    {
        return mTmsIgnoreHintPID;
    }

    static SShort getTmsManualSlotNoInItBMP()
    {
        return (SShort)mTmsManualSlotNoInItBMP;
    }

    static SShort getTmsManualSlotNoInLfBMP()
    {
        return (SShort)mTmsManualSlotNoInLfBMP;
    }

    static UInt getTmsCandidateLfBMPCnt()
    {
        return mTmsCandidateLfBMPCnt;
    }
    static UInt getTmsCandidatePageCnt()
    {
        return mTmsCandidatePageCnt;
    }
    static UInt getTmsMaxSlotCntPerRtBMP()
    {
        return mTmsMaxSlotCntPerRtBMP;
    }
    static UInt getTmsMaxSlotCntPerItBMP()
    {
        return mTmsMaxSlotCntPerItBMP;
    }
    static UInt getTmsMaxSlotCntPerExtDir()
    {
        return mTmsMaxSlotCntPerExtDir;
    }
    static idBool getTmsDelayedAllocHintPageArr()
    {
        return (mTmsDelayedAllocHintPageArr == 1 ? ID_TRUE : ID_FALSE);
    }
    static UInt getTmsHintPageArrSize()
    {
        return mTmsHintPageArrSize;
    }
    static UInt getSegStoInitExtCnt()
    {
        return mSegStoInitExtCnt;
    }
    static UInt getSegStoNextExtCnt()
    {
        return mSegStoNextExtCnt;
    }
    static UInt getSegStoMinExtCnt()
    {
        return mSegStoMinExtCnt;
    }
    static UInt getSegStoMaxExtCnt()
    {
        return mSegStoMaxExtCnt;
    }

    static UInt getTSSegExtDescCntPerExtDir()
    {
        return mTSSegExtDescCntPerExtDir;
    }
    static UInt getUDSegExtDescCntPerExtDir()
    {
        return mUDSegExtDescCntPerExtDir;
    }
    static UInt getTSSegSizeShrinkThreshold()
    {
        return mTSSegSizeShrinkThreshold;
    }
    static UInt getUDSegSizeShrinkThreshold()
    {
        return mUDSegSizeShrinkThreshold;
    }

    static UInt getRetryStealCount()
    {
        return mRetryStealCount;
    }

    static UInt getDefaultPctFree()
    {
        return mPctFree;
    }
    static UInt getDefaultPctUsed()
    {
        return mPctUsed;
    }

    static UInt getDefaultTableInitTrans()
    {
        return mTableInitTrans;
    }
    static UInt getDefaultTableMaxTrans()
    {
        return mTableMaxTrans;
    }

    static UInt getDefaultIndexInitTrans()
    {
        return mIndexInitTrans;
    }
    static UInt getDefaultIndexMaxTrans()
    {
        return mIndexMaxTrans;
    }

    static UInt getCheckSumMethod()
    {
        return mCheckSumMethod;
    }

    // To verify CASE-6829
    static UInt getSMChecksumDisable()
    {
        return mSMChecksumDisable;
    }

    // BUG-17526: CASE-8623�� ���� Ager�� Buffer ����
    // ������� �߰�
    static idBool getCheckDiskAgerStat()
    {
        return (mCheckDiskAgerStat == 1 ? ID_TRUE : ID_FALSE);
    }

    static UInt getDPathBuffPageCnt()
    {
        return mDPathBuffPageCnt;
    }

    static UInt getDBFileMutiReadCnt()
    {
        return mDBFileMultiReadCnt;
    }

    static UInt getSmallTblThreshold()
    {
        return mSmallTblThreshold;
    }

    static UInt getFlusherBusyConditionCheckInterval()
    {
        return mFlusherBusyConditionCheckInterval;
    }

    static SLong getDPathBuffPageAllocRetryUSec()
    {
        return mDPathBuffPageAllocRetryUSec;
    }

    static idBool getDPathInsertEnable()
    {
        return mDPathInsertEnable;
    }

    static UInt getExtDecCntInExtDirPage()
    {
        return mExtDescCntInExtDirPage;
    }

    // sdn
    static UInt getMaxTempHashTableCount()
    {
        return mMaxTempHashTableCount;
    }

    // BUG-29506 TBT�� TBK�� ��ȯ�� ����� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    static UInt getDisableTransactionBoundInCTS()
    {
        return mDisableTransactionBoundInCTS;
    }

    // BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
    // �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
    static UInt getManualBindingTXSegByEntryID()
    {
        return mManualBindingTXSegByEntryID;
    }

    static ULong getSortAreaSize()
    {
        return mSortAreaSize;
    }

#if 0 // not used 
    static UInt getTssCntPctToBufferPool()
    {
        return mTssCntPctToBufferPool;
    }
#endif
    // To verify CASE-6829
    static UInt getSMAgerDisable()
    {
        return mSMAgerDisable;
    }

    // smm
    static UInt getDirtyPagePool()
    {
        return mDirtyPagePool;
    }

    static const SChar* getDBName()
    {
        return (const SChar*)mDBName;
    }

    static ULong getMaxDBSize()
    {
        return mMaxDBSize;
    }

    static ULong getDefaultMemDBFileSize()
    {
        return mDefaultMemDBFileSize;
    }

    static UInt getIOType()
    {
        return mIOType;
    }

    static UInt getSyncCreate()
    {
        return mSyncCreate;
    }

    static UInt getCreateMethod()
    {
        return mCreateMethod;
    }

    static UInt getTempPageChunkCount()
    {
        return mTempPageChunkCount;
    }

    static ULong getSCNSyncInterval()
    {
        return mSCNSyncInterval;
    }

    static UInt getCheckStartupVersion()
    {
        return mCheckStartupVersion;
    }

    static UInt getCheckStartupBitMode()
    {
        return mCheckStartupBitMode;
    }

    static UInt getCheckStartupEndian()
    {
        return mCheckStartupEndian;
    }

    static UInt getCheckStartupLogSize()
    {
        return mCheckStartupLogSize;
    }

    static ULong getShmChunkSize()
    {
        return mShmChunkSize;
    }

    static UInt getRestoreMethod()
    {
        return mRestoreMethod;
    }

    static UInt getRestoreThreadCount()
    {
        return mRestoreThreadCount;
    }

    static UInt getRestoreAIOCount()
    {
        return mRestoreAIOCount;
    }

    static UInt getRestoreBufferPageCount()
    {
        return mRestoreBufferPageCount;
    }

    static UInt getCheckpointAIOCount()
    {
        return mCheckpointAIOCount;
    }


    // PROJ-1490 ����������Ʈ ����ȭ �� �޸𸮹ݳ�
    // �ϳ��� Expand Chunk�� ���ϴ� Page��
    static UInt getExpandChunkPageCount()
    {
        return mExpandChunkPageCount;
    }


    // ������ ���� Page List���� ���� ��� List�� ����ȭ �� �� �����Ѵ�.
    //
    // �����ͺ��̽� Free Page List
    // ���̺��� Allocated Page List
    // ���̺��� Free Page List
    static UInt getPageListGroupCount()
    {
        return mPageListGroupCount;
    }


    // �ѹ��� ��� Page�� Free Page List�� �й������� �����Ѵ�.
    static UInt getPerListDistPageCount()
    {
        return mPerListDistPageCount;
    }

    // Free Page List�� List ������ ������ �ϴ� �ּ����� Page��
    // Free Page List ���ҽÿ� �ּ��� �� ��ŭ�� Page��
    // List�� ���� ���� �� �ִ� ��쿡�� Free Page List�� �����Ѵ�.
    static UInt getMinPagesDBTableFreeList()
    {
        return mMinPagesOnDBFreeList;
    }

    static UInt getSeparateDicTBSSizeEnable()
    {
        return (mSeparateDicTBSSizeEnable == 1 ? ID_TRUE : ID_FALSE);
    }

// smr

    static UInt getChkptBulkWritePageCount()
    {
        return mChkptBulkWritePageCount;
    }

    static UInt getChkptBulkWriteSleepSec()
    {
        return mChkptBulkWriteSleepSec;
    }

    static UInt getChkptBulkWriteSleepUSec()
    {
        return mChkptBulkWriteSleepUSec;
    }

    static UInt getChkptBulkSyncPageCount()
    {
        return mChkptBulkSyncPageCount;
    }

    static UInt getAfterCare()
    {
        return mAfterCare;
    }
    static UInt getSMStartupTest()
    {
        return mSMStartupTest;
    }

    static idBool getCheckSrcLogFileWhenArchThrAbort()
    {
        return ( mCheckLogFileWhenArchThrAbort == 0 ) ? ID_FALSE : ID_TRUE;
    }

    static UInt getChkptEnabled()
    {
        return mChkptEnabled;
    }

    static UInt getChkptIntervalInSec()
    {
        return mChkptIntervalInSec;
    }

    static UInt getChkptIntervalInLog()
    {
        return mChkptIntervalInLog;
    }


    static UInt getSyncIntervalSec()
    {
        return mSyncIntervalSec;
    }

    static UInt getSyncIntervalMSec()
    {
        return mSyncIntervalMSec;
    }

    /* BUG-35392 Begin */
    static UInt getUCLSNChkThrInterval()
    {
        return mUCLSNChkThrInterval;
    }

    static UInt getLFThrSyncWaitMin()
    {
        return mLFThrSyncWaitMin;
    }

    static UInt getLFThrSyncWaitMax()
    {
        return mLFThrSyncWaitMax;
    }

    static UInt getLFGMgrSyncWaitMin()
    {
        return mLFGMgrSyncWaitMin;
    }

    static UInt getLFGMgrSyncWaitMax()
    {
        return mLFGMgrSyncWaitMax;
    }
    /* BUG-35392 End */

    static UInt getDPathBuffFThreadSyncInterval()
    {
        return mDPathBuffFThreadSyncInterval;
    }

    static const SChar* getLogAnchorDir(UInt sNthValue)
    {
        return (const SChar*)mLogAnchorDir[sNthValue];
    }

    static const SChar* getLogDirPath()
    {
        return (const SChar*)mLogDirPath;
    }

    static const SChar** getLogMultiplexDirPath()
    {
        return (const SChar**)mLogMultiplexDirPath;
    }

    static UInt getLogMultiplexCount()
    {
        return mLogMultiplexCount;
    }

    static UInt getLogMultiplexThreadSpinCount()
    {
        return mLogMultiplexThreadSpinCount;
    }

    static const SChar* getArchiveDirPath()
    {
        return (const SChar*)mArchiveDirPath;
    }

    static const SChar** getArchiveMultiplexDirPath()
    {
        return (const SChar**)mArchiveMultiplexDirPath;
    }

    static UInt getArchiveMultiplexCount()
    {
        return mArchiveMultiplexCount;
    }

    // mmap����, memory���� log buffer type�� ��ȯ
    static UInt getLogBufferType()
    {
        return mLogBufferType;
    }

    static UInt getArchiveFullAction()
    {
        return mArchiveFullAction;
    }

    static UInt getArchiveThreadAutoStart()
    {
        return mArchiveThreadAutoStart;
    }

    static UInt getFileInitBufferSize()
    {
        return mFileInitBufferSize;
    }

    static ULong getLogFileSize()
    {
        return mLogFileSize;
    }

    static UInt getZeroSizeLogFileAutoDelete()
    {
        /* BUG-48409 temp log file ���ÿ��� logfile �ڵ� ���� ���� */
        return ( mUseTempForPrepareLogFile == ID_TRUE ) ? 0 : mZeroSizeLogFileAutoDelete;
    }

    static UInt getLogFilePrepareCount()
    {
        return mLogFilePrepareCount;
    }
     
    static idBool getUseTempForPrepareLogFile()
    {
        return mUseTempForPrepareLogFile;
    }

    static UInt getLogFilePreCreateInterval()
    {
        return mLogFilePreCreateInterval;
    }

//    static UInt getMaxKeepLogFile()
//    {
//        return mMaxKeepLogFile;
//    }


    static UInt getShmDBKey()
    {
        return mShmDBKey;
    }

    static UInt getShmPageCountPerKey()
    {
        return mShmPageCountPerKey;
    }


    static UInt getMinLogRecordSizeForCompress()
    {
        return mMinLogRecordSizeForCompress;
    }

    static ULong getDiskRedoLogDecompressBufferSize()
    {
        return mDiskRedoLogDecompressBufferSize;
    }

    // �ϳ��� LFG���� Group Commit�� �����ϱ� ���� Update Transaction�� ��
    // �� LFG���� Update Transaction�� ���� Count�Ͽ� �� ������ ũ�ų� ���� ��
    // �׷�Ŀ���� �ǽ��Ѵ�.
    static UInt getLFGGroupCommitUpdateTxCount()
    {
        return mLFGGroupCommitUpdateTxCount;
    }

    // ���ÿ� ������ Log Flush��û�� ������ ���͹�
    // ���ôٹ��� Log Flush��û�� �����Ͽ� I/OȽ���� ���� ���̴� ����
    // Group Commit�� �����̴�.
    static UInt getLFGGroupCommitIntervalUSec()
    {
        return mLFGGroupCommitIntervalUSec;
    }

    // Log Flush��û�� ������ Transaction���� �ֱ������� �����
    // Log�� Flush�Ǿ������� üũ�Ѵ�. �ٷ� �� �ֱ⸦ ������ ������Ƽ�̴�.
    static UInt getLFGGroupCommitRetryUSec()
    {
        return mLFGGroupCommitRetryUSec;
    }

    static UInt getLogIOType()
    {
        return mLogIOType;
    }

    static UInt getMinCompressionResourceCount()
    {
        return mMinCompressionResourceCount;
    }

    static ULong getCompressionResourceGCSecond()
    {
        return mCompressionResourceGCSecond;
    }

    static UInt getLogAllocMutexType()
    {
        return mLogAllocMutexType;
    }

    /*PROJ-2162 RestartRiskReduction Begin */
    static UInt getSmEnableStartupBugDetector()
    {
        return mSmEnableStartupBugDetector;
    }
    static UInt getSmMtxRollbackTest()
    {
        return mSmMtxRollbackTest;
    }
    static void resetSmMtxRollbackTest()
    {
        mSmMtxRollbackTest = 0;
    }
    static UInt getEmergencyStartupPolicy()
    {
        return mEmergencyStartupPolicy;
    }
    static UInt getCrashTolerance()
    {
        return mCrashTolerance;
    }
    static ULong getSmIgnoreLog4EmergencyCount()
    {
        return mSmIgnoreLog4EmergencyCount;
    }
    static ULong getSmIgnoreLFGID4Emergency( UInt aID )
    {
        return mSmIgnoreLFGID4Emergency[ aID ];
    }
    static ULong getSmIgnoreFileNo4Emergency( UInt aID )
    {
        return mSmIgnoreFileNo4Emergency[ aID ];
    }
    static ULong getSmIgnoreOffset4Emergency( UInt aID )
    {
        return mSmIgnoreOffset4Emergency[ aID ];
    }
    static ULong getSmIgnorePage4EmergencyCount()
    {
        return mSmIgnorePage4EmergencyCount;
    }
    static ULong getSmIgnorePage4Emergency( UInt aID )
    {
        return mSmIgnorePage4Emergency[ aID ];
    }
    static ULong getSmIgnoreTable4EmergencyCount()
    {
        return mSmIgnoreTable4EmergencyCount;
    }
    static ULong getSmIgnoreTable4Emergency( UInt aID )
    {
        return mSmIgnoreTable4Emergency[ aID ];
    }
    static ULong getSmIgnoreIndex4EmergencyCount()
    {
        return mSmIgnoreIndex4EmergencyCount;
    }
    static UInt getSmIgnoreIndex4Emergency( UInt aID )
    {
        return mSmIgnoreIndex4Emergency[ aID ];
    }
    /*PROJ-2162 RestartRiskReduction End */

    /* PROJ-2133 Incremental backup Begin */
    static UInt getBackupInfoRetentionPeriod()
    {
        return mBackupInfoRetentionPeriod;
    }
    static UInt getBackupInfoRetentionPeriodForTest()
    {
        return mBackupInfoRetentionPeriodForTest;
    }
    static UInt getIncrementalBackupChunkSize()
    {
        return mIncrementalBackupChunkSize;
    }
    static UInt getBmpBlockBitmapSize()
    {
        return mBmpBlockBitmapSize;
    }
    static UInt getCTBodyExtCnt()
    {
        return mCTBodyExtCnt;
    }
    static UInt getIncrementalBackupPathMakeABSPath()
    {
        return mIncrementalBackupPathMakeABSPath;
    }
    /* PROJ-2133 Incremental backup End */

    /* BUG-35392 */
    static idBool isFastUnlockLogAllocMutex()
    {
        return  mFastUnlockLogAllocMutex;
    }

    static UInt getLogCompAcceleration()
    {
        return mLogCompAcceleration;
    }
    /* BUG-38515 Add hidden property for skip scn check in startup 
     * BUG-41600 �� ������Ƽ�� ���� 2�� ��츦 �߰��Ѵ�. */    
    static UInt isSkipCheckSCNInStartup()
    {
        return mSkipCheckSCNInStartup;
    }
    
    static UInt getLogReadMethodType()
    {
        return mLogReadMethodType;
    }
    // smp
    static UInt getMinPagesOnTableFreeList()
    {
        return mMinPagesOnTableFreeList;
    }

    static UInt getAllocPageCount()
    {
        return mAllocPageCount;
    }

    static UInt getMemSizeClassCount()
    {
        return mMemSizeClassCount;
    }

    // smc
    static UInt getTableBackupFileBufferSize()
    {
        return mTableBackupFileBufferSize;
    }

    static UInt getCheckDiskIndexIntegrity()
    {
        return mCheckDiskIndexIntegrity;
    }

    static UInt getVerifyDiskIndexCount()
    {
        return mVerifyDiskIndexCount;
    }

    static SChar* getVerifyDiskIndexName( UInt aIndex )
    {
        return mVerifyDiskIndexName[aIndex];
    }

    /* BUG-32632 User Memory Tablesapce���� Max size�� �����ϴ� ���� Property �߰�
     * User Memory Tablespace�� Max Size�� �����ϰ� ������ Ȯ�� */
    static idBool   getIgnoreMemTbsMaxSize( )
    {
        return ( mIgnoreMemTbsMaxSize == 0 ) ? ID_FALSE : ID_TRUE;
    }

    static idBool getEnableRowTemplate( )
    {
        return ( mEnableRowTemplate == 0 ) ? ID_FALSE : ID_TRUE;
    }

    // sma

    static UInt getDeleteAgerCount()
    {
        return mDeleteAgerCount;
    }

    static UInt getAgerListCount()
    {
        return mAgerListCount;
    }

    static UInt getLogicalAgerCount()
    {
        return mLogicalAgerCount;
    }

    static UInt getMinLogicalAgerCount()
    {
        return mMinLogicalAgerCount;
    }

    static UInt getMaxLogicalAgerCount()
    {
        return mMaxLogicalAgerCount;
    }

    static UInt getAgerWaitMin()
    {
        return mAgerWaitMin;
    }

    static UInt getAgerWaitMax()
    {
        return mAgerWaitMax;
    }

    static UInt getAgerCommitInterval()
    {
        return mAgerCommitInterval;
    }

    static UInt getRefinePageCount()
    {
        return mRefinePageCount;
    }

    static UInt getTableCompactAtShutdown()
    {
        return mTableCompactAtShutdown;
    }

    static UInt getParallelLogicalAger()
    {
        return mParallelLogicalAger;
    }

    static UInt getParallelDeleteThread()
    {
        return mParallelDeleteThread;
    }

    static UInt getCatalogSlotReusable()
    {
        return mCatalogSlotReusable;
    }

    // sml
    static UInt getTableLockEnable()
    {
        return mTableLockEnable;
    }
    static UInt getTablespaceLockEnable()
    {
        return mTablespaceLockEnable;
    }

    static SInt getDDLLockTimeOut()
    {
        return mDDLLockTimeOut;
    }

    static UInt getLockNodeCacheCount()
    {  
        return mLockNodeCacheCount;
    }

    /* PROJ-2734 */
    static idBool getDistributionDeadlockEnable()
    {
        return mDistributionDeadlockEnable;
    }
    static ULong getDistributionDeadlockRiskLowWaitTime()
    {
        return mDistributionDeadlockRiskLowWaitTime;
    }
    static ULong getDistributionDeadlockRiskMidWaitTime()
    {
        return mDistributionDeadlockRiskMidWaitTime;
    }
    static ULong getDistributionDeadlockRiskHighWaitTime()
    {
        return mDistributionDeadlockRiskHighWaitTime;
    }

    static UInt getMemoryIndexUnbalancedSplitRate()
    {
        return mMemoryIndexUnbalancedSplitRate;
    }
    static UInt getSkipLockedTableAtFixedTable()
    {
        return mSkipLockedTableAtFixedTable;
    }
    //smn
    static UInt getMergePageCount()
    {
        return mMergePageCount;
    }

    static UInt getKeyRedistributionLowLimit()
    {
        return mKeyRedistributionLowLimit;
    }

    static SLong getMaxTraverseLength()
    {
        return mMaxTraverseLength;
    }

    static UInt getUnbalancedSplitRate()
    {
        return mUnbalancedSplitRate;
    }

    static UInt getIndexBuildMinRecordCount()
    {
        return mIndexBuildMinRecordCount;
    }
    static UInt getIndexBuildThreadCount()
    {
        return mIndexBuildThreadCount;
    }

    static UInt getParallelLoadFactor()
    {
        return mParallelLoadFactor;
    }

    static UInt getIteratorMemoryParallelFactor()
    {
        return mIteratorMemoryParallelFactor;
    }

//    static UInt getIndexStatParallelFactor()
//    {
//        return mIndexStatParallelFactor;
//    }

    static ULong getMemoryIndexBuildRunSize()
    {
        return mMemoryIndexBuildRunSize;
    }

    static ULong getMemoryIndexBuildValueLengthThreshold()
    {
        return mMemoryIndexBuildValueLengthThreshold;
    }

    // TASK-6006
    static SInt getRebuildIndexParallelMode()
    {
        return mIndexRebuildParallelMode;
    }
    
    // BUG-47540
    static idBool getSkipIdxStatNodeBound()
    {
        return mSkipIdxStatNodeBound;
    }

    // PROJ-1591
    static UInt getRTreeSplitMode()
    {
        return mRTreeSplitMode;
    }

    static UInt getRTreeSplitRate()
    {
        return mRTreeSplitRate;
    }
    
    static UInt getRTreeMaxKeyCount()
    {
        return mRTreeMaxKeyCount;
    }

    /* PROJ-2433 */
    static UInt getMemBtreeNodeSize()
    {
        return mMemBtreeNodeSize;
    }
    static UInt getMemBtreeDefaultMaxKeySize()
    {
        return mMemBtreeDefaultMaxKeySize;
    }

    /* PROJ-2613 Key Redistribution In MRDB Index */
    static idBool getMemIndexKeyRedistribute()
    {
        if ( mMemIndexKeyRedistribution == 1 )
        {
            return ID_TRUE;
        }
        else
        {
            return ID_FALSE;
        }
    }

    /* PROJ-2613 Key Redistribution In MRDB Index */
    static SInt getMemIndexKeyRedistributionStandardRate()
    {
        return mMemIndexKeyRedistributionStandardRate;
    }

    static idBool getScanlistMoveNonBlock()
    {
        return mScanlistMoveNonBlock;
    }

    static idBool getGatherIndexStatOnDDL()
    {
        return mGatherIndexStatOnDDL;
    }
    //smx

    static UInt getRebuildMinViewSCNInterval()
    {
        return mRebuildMinViewSCNInterval;
    }

    static UInt getOIDListCount()
    {
        return mOIDListCount;
    }

    static UInt getGroupCommitCnt()
    {
        return mGroupCommitCnt;
    }

    static UInt getGroupCommitListCnt()
    {
        return mGroupCommitListCnt;
    }

    static UInt getTransTouchPageCntByNode()
    {
        return mTransTouchPageCntByNode;
    }

    static UInt getTransTouchPageCacheRatio()
    {
        return mTransTouchPageCacheRatio;
    }

    static UInt getTransTblSize()
    {
        return mTransTblSize;
    }

    static UInt getCheckOverflowTransTblSize()
    {
        /* 0�� ��� ������ ����Ѵ�, UInt Max */
        return ( mTransTableFullTrcLogCycle > 0 ) ? mTransTableFullTrcLogCycle : ID_UINT_MAX;
    }

    static ULong getLockTimeOut()
    {
        return mLockTimeOut;
    }

    static UInt getReplLockTimeOut()
    {
        return mReplLockTimeOut;
    }

    static UInt getTransAllocWait()
    {
        return mTransAllocWaitTime;
    }

    static UInt getPrivateBucketCount()
    {
        return mPrivateBucketCount;
    }

    static UInt getTXSEGEntryCnt()
    {
        return mTXSEGEntryCnt;
    }

    static UInt getTXSegAllocWaitTime()
    {
        return mTXSegAllocWaitTime;
    }

    /* BUG-38515 Add hidden property for skip scn check in startup */    
    static idBool isTrcLogLegacyTxInfo()
    {
        return ( mTrcLogLegacyTxInfo == 0 ) ? ID_FALSE : ID_TRUE;
    }

    static UInt getLobCursorHashBucketCount()
    {
        return mLobCursorHashBucketCount;
    }

    /* BUG-47365 */
    static UInt getLogCompResourceReuse()
    {
        return mLogCompResourceReuse;
    }

    static UInt getCompResTuneSize()
    {
        return mCompResTuneSize;
    }

    static ULong getVersioningMinTime()
    {
        return mVersioningMinTime;
    }


    //smu
    static UInt getArtDecreaseVal()
    {
        return mArtDecreaseVal;
    }

    static const SChar* getDBDir(UInt sNthValue)
    {
        if( sNthValue > getDBDirCount() )
        {
            return NULL;
        }

        return (const SChar *)mDBDir[sNthValue];
    }

    static UInt getDBDirCount()
    {
        return mDBDirCount;
    }

    static const SChar* getDefaultDiskDBDir()
    {
        return (const SChar *)mDefaultDiskDBDir;
    }

    static UInt  getDBMSStatMethod()
    {
        return mDBMSStatMethod;
    }
    static UInt  getDBMSStatMethod4MRDB()
    {
        return mDBMSStatMethod4MRDB;
    }

    static UInt  getDBMSStatMethod4VRDB()
    {
        return mDBMSStatMethod4VRDB;
    }

    static UInt  getDBMSStatMethod4DRDB()
    {
        return mDBMSStatMethod4DRDB;
    }

    static UInt  getDBMSStatSamplingBaseCnt()
    {
        return mDBMSStatSamplingBaseCnt;
    }
    static UInt  getDBMSStatParallelDegree()
    {
        return mDBMSStatParallelDegree;
    }
    static UInt  getDBMSStatGatherInternalStat()
    {
        return mDBMSStatGatherInternalStat;
    }
    static UInt  getDBMSStatAutoPercentage()
    {
        return mDBMSStatAutoPercentage;
    }
    static UInt  getDBMSStatAutoInterval()
    {
        return mDBMSStatAutoInterval;
    }

    static UInt  getDefaultSegMgmtType()
    {
        return mDefaultSegMgmtType;
    }

    static UInt  getExtCntPerExtentGroup()
    {
        return mDefaultExtCntForExtentGroup;
    }

    static UInt  getMemDPFlushWaitTime()
    {
        return mMemDPFlushWaitTime;
    }
    
    static UInt  getMemDPFlushWaitSID()
    {
        return mMemDPFlushWaitSID;
    }

    static UInt  getMemDPFlushWaitPID()
    {
        return mMemDPFlushWaitPID;
    }

    static UInt  getMemDPFlushWaitOffset()
    {
        return mMemDPFlushWaitOffset;
    }

    static ULong getSysDataTBSExtentSize()
    {
        return mSysDataTBSExtentSize;
    }

    static ULong getSysDataFileInitSize()
    {
        return mSysDataFileInitSize;
    }

    static ULong getSysDataFileMaxSize()
    {
        return mSysDataFileMaxSize;
    }

    static ULong getSysDataFileNextSize()
    {
        return mSysDataFileNextSize;
    }

    static ULong getSysUndoTBSExtentSize()
    {
        return mSysUndoTBSExtentSize;
    }

    static ULong getSysUndoFileInitSize()
    {
        return mSysUndoFileInitSize;
    }

    static ULong getSysUndoFileMaxSize()
    {
        return mSysUndoFileMaxSize;
    }

    static ULong getSysUndoFileNextSize()
    {
        return mSysUndoFileNextSize;
    }

    static ULong getSysTempTBSExtentSize()
    {
        return mSysTempTBSExtentSize;
    }

    static ULong getSysTempFileInitSize()
    {
        return mSysTempFileInitSize;
    }

    static ULong getSysTempFileMaxSize()
    {
        return mSysTempFileMaxSize;
    }

    static ULong getSysTempFileNextSize()
    {
        return mSysTempFileNextSize;
    }

    static ULong getUserDataTBSExtentSize()
    {
        return mUserDataTBSExtentSize;
    }

    static ULong getUserDataFileInitSize()
    {
        return mUserDataFileInitSize;
    }

    static ULong getUserDataFileMaxSize()
    {
        return mUserDataFileMaxSize;
    }

    static ULong getUserDataFileNextSize()
    {
        return mUserDataFileNextSize;
    }

    static ULong getUserTempTBSExtentSize()
    {
        return mUserTempTBSExtentSize;
    }

    static ULong getUserTempFileInitSize()
    {
        return mUserTempFileInitSize;
    }

    static ULong getUserTempFileMaxSize()
    {
        return mUserTempFileMaxSize;
    }

    static ULong getUserTempFileNextSize()
    {
        return mUserTempFileNextSize;
    }

    static UInt getLockEscMemSize()
    {
        return mLockEscMemSize;
    }

    static UInt getIndexBuildBottomUpThreshold()
    {
        return mIndexBuildBottomUpThreshold;
    }

    static UInt getTableBackupTimeOut()
    {       
        return mTableBackupTimeOut;
    }

    /* BUG-38621 */
    static UInt getRELPathInLog()
    {       
        return mRELPathInLog;
    }

    /* PROJ-2102 Fast Secondary Buffer Start */
    //sds
    static UInt getSBufferEnable()
    {
        return mSBufferEnable;
    }

    static UInt getSBufferFlusherCnt()
    {
        return mSBufferFlusherCount;
    }

    static UInt getMaxSBufferCPFlusherCnt()
    {
        return mMaxSBufferCPFlusherCnt;
    }
   
    static UInt getSBufferType()
    {
        return mSBufferType;
    }
    
    static ULong getSBufferSize()
    {
        return mSBufferSize;
    }

    static const SChar* getSBufferDirectoryName()
    {
        return (const SChar*)mSBufferFileDirectory;
    }
    /* PROJ-2102 Fast Secondary Buffer End */

    //util
    static UInt getPortNo()
    {
        return mPortNo;
    }

    static UInt getIndexRebuildAtStartup()
    {
        return mIndexRebuildAtStartup;
    }

    static UInt getIndexRebuildParallelFactorAtStartup()
    {
        return mIndexRebuildParallelFactorAtSartup;
    }

    static UInt getMMDBDefIdxType()
    {
        return mMMDBDefIdxType;
    }

    //-------------------------------------
    // To Fix PR-14783
    // System Thread�� ������ ������.
    //-------------------------------------

    static UInt isRunMemDeleteThread()
    {
        return mRunMemDeleteThread;
    }
    static UInt isRunMemGCThread()
    {
        return mRunMemGCThread;
    }
    static UInt isRunBufferFlushThread()
    {
        return mRunBufferFlushThread;
    }
    static UInt isRunArchiveThread()
    {
        return mRunArchiveThread;
    }
    static UInt isRunCheckpointThread()
    {
        return mRunCheckpointThread;
    }
    static UInt isRunLogFlushThread()
    {
        return mRunLogFlushThread;
    }
    static UInt isRunLogPrepareThread()
    {
        return mRunLogPrepareThread;
    }
    static UInt isRunLogPreReadThread()
    {
        return mRunLogPreReadThread;
    }
#ifdef ALTIBASE_ENABLE_SMARTSSD
    static UInt getSmartSSDLogRunGCEnable()
    {
        return mSmartSSDLogRunGCEnable;
    }
    static const SChar* getSmartSSDLogDevice()
    {
        return mSmartSSDLogDevice;
    }
    static UInt getSmartSSDGCTimeMilliSec()
    {
        return mSmartSSDGCTimeMilliSec;
    }
#endif
    static UInt getCorruptPageErrPolicy()
    {
        return mCorruptPageErrPolicy;
    }

    // svm
    static ULong getVolMaxDBSize()
    {
        return mVolMaxDBSize;
    }

    // scp
    /* Proj-2059 DB Upgrade ��� */
    static UInt getDataPortFileBlockSize()
    {
        return mDataPortFileBlockSize;
    }

    static UInt getExportColumnChainingThreshold()
    {
        return mExportColumnChainingThreshold;
    }

    static UInt getDataPortDirectIOEnable()
    {
        return mDataPortDirectIOEnable;
    }

    static UInt getTxOIDListMemPoolType()
    {
        return mTxOIDListMemPoolType;
    }

    static UInt getCheckpointFlushResponsibility()
    {
        return mCheckpointFlushResponsibility;
    }

    // File Size�� OS��  Limit���� ū�� üũ�Ѵ�.
    //
    // �� �Լ��� sm�� �پ��� ��⿡�� ���Ǵ� �Լ���
    // smu���� �� ���� Layer�� �δ� ���� �´�.
    // ���� sm�ȿ��� smu���� ���� Layer�� ���� ���
    // �ش� Layer�� �ű�� ���� ����.
    static IDE_RC checkFileSizeLimit( const SChar * aUserKeyword,
                                      ULong aFileSize );

    static IDE_RC checkFileSizeProperty( const SChar * aInitPropName,
                                         const SChar * aMaxPropName,
                                         ULong aInitFileSize ,
                                         ULong aMaxFileSize );

    static IDE_RC checkSBufferPropery( void ); 
 
    static ULong  getReservedDiskSizeForLogFile(void)
    {
        return mReservedDiskSizeForLogFile;
    }

    static idBool isForceIndexDirectKey()
    {
        return  mForceIndexDirectKey;
    }

    /* BUG-41121 */
    static UInt forceIndexPersistenceMode()
    {
        return  mForceIndexPersistenceMode;
    }

    static idBool getIsCPUAffinity()
    {
        return mIsCPUAffinity;
    }

    /* PROJ-2733 */
    static idBool getShardEnable()
    {
        return ( (mShardEnable == 1) ? ID_TRUE : ID_FALSE );
    }
};


#endif /*  O_SMU_PROPERTY_H */
