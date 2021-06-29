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

#include <smErrorCode.h>
#include <smi.h>
#include <mtuProperty.h>
#include <qci.h>
#include <sdi.h>
#include <mmErrorCode.h>
#include <mmm.h>
#include <mmcTrans.h>
#include <mmcSession.h>
#include <mmcTask.h>
#include <mmdXa.h>
#include <mmqQueueInfo.h>
#include <mmtAdminManager.h>
#include <mmtSessionManager.h>
#include <mmtServiceThread.h>
#include <mmuProperty.h>
#include <mmuOS.h>
#include <mmcPlanCache.h>
#include <mtz.h>
#include <mmuAccessList.h>

#include <dki.h>

typedef IDE_RC (*mmcSessionSetFunc)(mmcSession *aSession, SChar *aValue);

typedef struct mmcSessionSetList
{
    SChar             *mName;
    mmcSessionSetFunc  mFunc;
} mmcSessionSetList;

static mmcSessionSetList gCmsSessionSetLists[] =
{
    { (SChar *)"AGER", mmcSession::setAger },
    { NULL,            NULL                }
};

 /* SM�� callback �Լ� ����*/
smiSessionCallback mmcSession::mSessionInfoCallback =
{
    mmcSession::setAllocTransRetryCountCallback, // BUG-47655 TRANSACTION_TABLE_SIZE ���� ���� �޽��� ����

    /* BUG-48250 */
    mmcSession::getIndoubtFetchTimeoutCallback,
    mmcSession::getIndoubtFetchMethodCallback
};

qciSessionCallback mmcSession::mCallback =
{
    mmcSession::getLanguageCallback,
    mmcSession::getDateFormatCallback,
    mmcSession::getUserNameCallback,
    mmcSession::getUserPasswordCallback,
    mmcSession::getUserIDCallback,
    mmcSession::setUserIDCallback,
    mmcSession::getSessionIDCallback,
    mmcSession::getSessionLoginIPCallback,
    mmcSession::getTableSpaceIDCallback,
    mmcSession::getTempSpaceIDCallback,
    mmcSession::isSysdbaUserCallback,
    mmcSession::isBigEndianClientCallback,
    mmcSession::getStackSizeCallback,
    mmcSession::getNormalFormMaximumCallback,
    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    mmcSession::getOptimizerDefaultTempTbsTypeCallback,
    mmcSession::getOptimizerModeCallback,
    mmcSession::getSelectHeaderDisplayCallback,
    mmcSession::savepointCallback,
    mmcSession::commitCallback,
    mmcSession::rollbackCallback,
    mmcSession::setReplicationModeCallback,
    mmcSession::setTXCallback,
    mmcSession::setStackSizeCallback,
    mmcSession::setCallback,
    mmcSession::setPropertyCallback,
    mmcSession::memoryCompactCallback,
    mmcSession::printToClientCallback,
    mmcSession::getSvrDNCallback,
    mmcSession::getSTObjBufSizeCallback,
    NULL, // mmcSession::getSTAllowLevCallback
    mmcSession::isParallelDmlCallback,
    mmcSession::commitForceCallback,
    mmcSession::rollbackForceCallback,
    mmcPlanCache::compact,
    mmcPlanCache::reset,
    mmcSession::getNlsNcharLiteralReplaceCallback,
    //BUG-21122
    mmcSession::getAutoRemoteExecCallback,
    NULL, // mmcSession::getDetailSchemaCallback
    // BUG-25999
    mmcSession::removeHeuristicXidCallback,
    mmcSession::getTrclogDetailPredicateCallback,
    mmcSession::getOptimizerDiskIndexCostAdjCallback,
    mmcSession::getOptimizerMemoryIndexCostAdjCallback,
    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* Callback function to obtain a mutex from the mutex pool in mmcSession. */
    mmcSession::getMutexFromPoolCallback,
    /* Callback function to free a mutex from the mutex pool in mmcSession. */
    mmcSession::freeMutexFromPoolCallback,
    /* PROJ-2208 Multi Currency */
    mmcSession::getNlsISOCurrencyCallback,
    mmcSession::getNlsCurrencyCallback,
    mmcSession::getNlsNumCharCallback,
    /* BUG-41511 supporting to similar DBMS_APPLICATION_INFO */
    mmcSession::setClientAppInfoCallback,
    mmcSession::setModuleInfoCallback,
    mmcSession::setActionInfoCallback,
    /* PROJ-2209 DBTIMEZONE */
    mmcSession::getTimezoneSecondCallback,
    mmcSession::getTimezoneStringCallback,
    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    mmcSession::getLobCacheThresholdCallback,
    /* PROJ-1090 Function-based Index */
    mmcSession::getQueryRewriteEnableCallback,

    mmcSession::getDatabaseLinkSessionCallback,

    mmcSession::commitForceDatabaseLinkCallback,
    mmcSession::rollbackForceDatabaseLinkCallback,
    /* BUG-38430 */
    mmcSession::getSessionLastProcessRowCallback,
    /* BUG-38409 autonomous transaction */
    mmcSession::swapTransaction,
    /* PROJ-1812 ROLE */
    mmcSession::getRoleListCallback,
    
    /* PROJ-2441 flashback */
    mmcSession::getRecyclebinEnableCallback,

    /* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
    mmcSession::getLockTableUntilNextDDLCallback,
    mmcSession::setLockTableUntilNextDDLCallback,
    mmcSession::getTableIDOfLockTableUntilNextDDLCallback,
    mmcSession::setTableIDOfLockTableUntilNextDDLCallback,

    // BUG-41398 use old sort
    mmcSession::getUseOldSortCallback,

    /* PROJ-2451 Concurrent Execute Package */
    mmcSession::allocInternalSession,
    mmcSession::freeInternalSession,
    mmcSession::getSessionSmiTrans,
    // PROJ-2446
    mmcSession::getStatisticsCallback,
    /* BUG-41452 Built-in functions for getting array binding info.*/
    mmcSession::getArrayBindInfo,
    /* BUG-41561 */
    mmcSession::getLoginUserIDCallback,
    // BUG-41944
    mmcSession::getArithmeticOpModeCallback,
    /* PROJ-2462 Result Cache */
    mmcSession::getResultCacheEnableCallback,
    mmcSession::getTopResultCacheModeCallback,
    mmcSession::getIsAutoCommitCallback,
    // PROJ-1904 Extend UD
    mmcSession::getQciSessionCallback,
    // PROJ-2492 Dynamic sample selection
    mmcSession::getOptimizerAutoStatsCallback,
    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    mmcSession::getOptimizerTransitivityOldRuleCallback,
    // BUG-42464 dbms_alert package
    mmcSession::registerCallback,
    mmcSession::removeCallback,
    mmcSession::removeAllCallback,
    mmcSession::setDefaultsCallback,
    mmcSession::signalCallback,
    mmcSession::waitAnyCallback,
    mmcSession::waitOneCallback,
    mmcSession::getOptimizerPerformanceViewCallback,
    /* PROJ-2624 [��ɼ�] MM - ������ access_list ������� ���� */
    mmcSession::loadAccessListCallback,
    /* PROJ-2701 Sharding online data rebuild */
    mmcSession::isShardUserSessionCallback,  /* BUG-47739 */
    /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
    mmcSession::getCallByShardAnalyzeProtocolCallback,
    /* PROJ-2638 shard native linker */
    mmcSession::getShardPINCallback,
    mmcSession::getShardMetaNumberCallback, /* BUG-46090 Meta Node SMN ���� */
    mmcSession::getShardNodeNameCallback,
    mmcSession::reloadShardMetaNumberCallback,
    /* BUG-45899 */
    mmcSession::getTrclogDetailShardCallback,
    mmcSession::getExplainPlanCallback,
    /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
    mmcSession::getGTXLevelCallback,
    /* PROJ-2677 DDL synchronization */
    mmcSession::getReplicationDDLSyncCallback,

    /* PROJ-2735 DDL Transaction */
    mmcSession::getTransactionalDDLCallback,
    /* PROJ-2736 Global DDL */
    mmcSession::getGlobalDDLCallback,

    mmcSession::getPrintOutEnableCallback,
    mmcPlanCache::planCacheKeep,  /* BUG-46158 */
    mmcSession::isShardCliCallback,             /* BUG-46092 */
    mmcSession::getShardStmtCallback,           /* BUG-46092 */
    mmcSession::freeShardStmtCallback,          /* BUG-46092 */
    mmcSession::getShardFailoverTypeCallback,   /* BUG-46092 */
    mmcSession::getSerialExecuteModeCallback, /* PROJ-2632 */
    mmcSession::getTrcLogDetailInformationCallback, /* PROJ-2632 */
    mmcSession::getReducePartPrepareMemoryCallback, /* BUG-47648 */ 
    mmcSession::getShardSessionTypeCallback,
    // PROJ-2727 get session property callback
    mmcSession::getSessionPropertyInfoCallback,
    mmcSession::getCommitWriteWaitModeCallback,
    mmcSession::getDblinkRemoteStatementAutoCommitCallback,
    mmcSession::getDdlTimeoutCallback,
    mmcSession::getFetchTimeoutCallback,
    mmcSession::getIdleTimeoutCallback,
    mmcSession::getMaxStatementsPerSessionCallback,
    mmcSession::getNlsNcharConvExcpCallback,
    mmcSession::getNlsTerritoryCallback,
    mmcSession::getQueryTimeoutCallback,
    mmcSession::getReplicationDDLSyncTimeoutCallback,
    mmcSession::getUpdateMaxLogSizeCallback,
    mmcSession::getUTransTimeoutCallback,
    mmcSession::getPropertyAttributeCallback,
    mmcSession::setPropertyAttributeCallback,
    mmcSession::getShardInPSMEnableCallback,
    mmcSession::setShardInPSMEnableCallback,
    mmcSession::getStmtIdCallback,              /* PROJ-2728 */
    mmcSession::findShardStmtCallback,          /* PROJ-2728 */
    /* PROJ-2729 setShardTable & Objects: add internal local operation*/
    mmcSession::getShardInternalLocalOperation,
    mmcSession::setShardInternalLocalOperationCallback,
    // BUG-47861 INVOKE_USER_ID, INVOKE_USER_NAME function
    mmcSession::getInvokeUserNameCallback,
    mmcSession::setInvokeUserNameCallback,
    // BUG-47862 AUTHID definer for SHARD
    mmcSession::setInvokeUserPropertyInternalCallback,
    mmcSession::getPlanStringCallback, /* TASK-7219 */

    /* PROJ-2733 */
    mmcSession::getStatementRequestSCNCallback,
    mmcSession::setStatementRequestSCNCallback,
    mmcSession::getStatementTxFirstStmtSCNCallback,
    mmcSession::getStatementTxFirstStmtTimeCallback,
    mmcSession::getStatementDistLevelCallback,
    mmcSession::isGTxCallback,
    mmcSession::isGCTxCallback,
    mmcSession::getSessionTypeStringCallback,
    mmcSession::getShardStatementRetryCallback,
    mmcSession::getIndoubtFetchTimeoutCallback,
    mmcSession::getIndoubtFetchMethodCallback,

    mmcSession::commitInternalCallback,
    mmcSession::setShardMetaNumberCallback,

    mmcSession::pauseShareTransFixCallback,
    mmcSession::resumShareTransFixCallback,
    mmcSession::getPlanHashOrSortMethodCallback, /* BUG-48132 */
    mmcSession::getBucketCountMaxCallback,    /* BUG-48161 */

    mmcSession::getShardDDLLockTimeout,
    mmcSession::getShardDDLLockTryCount,
    mmcSession::getDDLLockTimeout,
    mmcSession::allocInternalSessionWithUserInfo,
    mmcSession::setShardPINCallback,
    mmcSession::getUserInfoCallback,
    mmcSession::getSessionSmiTransWithBegin,

    mmcSession::getEliminateCommonSubexpressionCallback, /* BUG-48348 */

    mmcSession::getLastShardMetaNumberCallback,
    mmcSession::detectShardMetaChangeCallback,
    mmcSession::getClientTouchNodeCountCallback,  /* BUG-48384 */

    /* TASK-7219 Non-shard DML */
    mmcSession::increaseStmtExecSeqForShardTxCallback,
    mmcSession::decreaseStmtExecSeqForShardTxCallback,
    mmcSession::getStmtExecSeqForShardTxCallback,
    mmcSession::getStatementShardPartialExecTypeCallback,

    /* BUG-48770 */
    mmcSession::checkSessionCountCallback
};


IDE_RC mmcSession::initialize(mmcTask *aTask, mmcSessID aSessionID)
{
    dkiSessionInit( &mDatabaseLinkSession );
    mInfo.mSessionID = aSessionID;

    /*
     * Runtime Task Info
     */

    mInfo.mTask      = aTask;
    mInfo.mTaskState = aTask->getTaskStatePtr();

    /*
     * Client Info
     */

    mInfo.mClientPackageVersion[0]  = 0;
    mInfo.mClientProtocolVersion[0] = 0;
    mInfo.mClientPID                = ID_ULONG(0);
    mInfo.mClientType[0]            = 0;
    mInfo.mClientAppInfo[0]         = 0;
    mInfo.mDatabaseName[0]          = 0;
    mInfo.mNlsUse[0]                = 0;
    // PROJ-1579 NCHAR
    mInfo.mNlsNcharLiteralReplace   = 0;
    idlOS::memset(&mInfo.mUserInfo, 0, ID_SIZEOF(mInfo.mUserInfo));
    /* BUG-30406 */
    idlOS::memset(&mInfo.mClientType, 0, ID_SIZEOF(mInfo.mClientType));
    idlOS::memset(&mInfo.mClientAppInfo, 0, ID_SIZEOF(mInfo.mClientAppInfo));
    idlOS::memset(&mInfo.mModuleInfo, 0, ID_SIZEOF(mInfo.mModuleInfo));
    idlOS::memset(&mInfo.mActionInfo, 0, ID_SIZEOF(mInfo.mActionInfo));

    /* BUG-31144 */
    mInfo.mNumberOfStatementsInSession = 0;
    mInfo.mMaxStatementsPerSession = 0;

    /* BUG-31390 Failover info for v$session */
    idlOS::memset(&mInfo.mFailOverSource, 0, ID_SIZEOF(mInfo.mFailOverSource));

    // PROJ-1752: �Ϲ����� ���� ODBC��� ������
    mInfo.mHasClientListChannel = ID_TRUE;

    // BUG-34725
    mInfo.mFetchProtocolType = MMC_FETCH_PROTOCOL_TYPE_LIST;

    // PROJ-2256
    mInfo.mRemoveRedundantTransmission = 0;

    /* PROJ-2626 Snapshot Export */
    mInfo.mClientAppInfoType = MMC_CLIENT_APP_INFO_TYPE_NONE;

    mInfo.mTransactionalDDL = ID_FALSE;
    mInfo.mGlobalDDL        = ID_FALSE;
    /*
     * Session Property
     */

    IDE_ASSERT(mmuProperty::getIsolationLevel() < 3);

    // BUG-15396
    // transaction ������ �Ѱ������ flag ���� ����
    // (1) isolation level
    // (2) replication mode
    // (3) transaction mode
    // (4) commit write wait mode
    setSessionInfoFlagForTx( (UInt)mmuProperty::getIsolationLevel(),
                             SMI_TRANSACTION_REPL_DEFAULT,
                             SMI_TRANSACTION_NORMAL,
                             (idBool)mmuProperty::getCommitWriteWaitMode() );

    // BUG-26017 [PSM] server restart�� ����Ǵ� psm load��������
    // ����������Ƽ�� �������� ���ϴ� ��� ����.
    mInfo.mOptimizerMode     = qciMisc::getOptimizerMode();
    mInfo.mHeaderDisplayMode = mmuProperty::getSelectHeaderDisplay();
    mInfo.mStackSize         = qciMisc::getQueryStackSize();
    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    mInfo.mOptimizerDefaultTempTbsType = qciMisc::getOptimizerDefaultTempTbsType();    
    mInfo.mExplainPlan       = ID_FALSE;
    mInfo.mSTObjBufSize      = 0;
    
    /* BUG-19080: Update Version�� ���������̻� �߻��ϸ� �ش�
     *            Transaction�� Abort�ϴ� ����� �ʿ��մϴ�. */
    mInfo.mUpdateMaxLogSize  = mmuProperty::getUpdateMaxLogSize();

    /* ------------------------------------------------
     * BUG-11522
     *
     * Restart Recovery �������� Begin�� Ʈ�������
     * �����ؼ��� �ȵȴ�.
     * �ֳ��ϸ�, Recovery �ܰ迡�� �α׿� ��ϵ�
     * Ʈ����� ���̵� �״�� ����Ͽ�, Ʈ�����
     * Entry�� �Ҵ�ޱ� �����̴�.
     * ����, Meta ��� �������� ��� autocommit����
     * �����ϵ��� �Ѵ�.
     * ----------------------------------------------*/

    if (mmm::getCurrentPhase() < MMM_STARTUP_META)
    {
        mInfo.mCommitMode = MMC_COMMITMODE_AUTOCOMMIT;
    }
    else
    {
        mInfo.mCommitMode = (mmuProperty::getAutoCommit() == 1 ?
                             MMC_COMMITMODE_AUTOCOMMIT : MMC_COMMITMODE_NONAUTOCOMMIT);

        /* BUG-48247 */
        if ( (SDU_SHARD_ENABLE == 1) && (mInfo.mCommitMode == MMC_COMMITMODE_AUTOCOMMIT) )
        {
            IDE_TEST_RAISE( SDU_SHARD_ALLOW_AUTO_COMMIT != 1, ConnectErrInShardEnv );  
        }

    }

    // PROJ-1665 : Parallel DML Mode ����
    // ( default�� FALSE �̴� )
    mInfo.mParallelDmlMode = ID_FALSE;

    setQueryTimeout(mmuProperty::getQueryTimeout());
    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    setDdlTimeout(mmuProperty::getDdlTimeout());
    setFetchTimeout(mmuProperty::getFetchTimeout());
    setUTransTimeout(mmuProperty::getUtransTimeout());
    setIdleTimeout(mmuProperty::getIdleTimeout());
    setNormalFormMaximum(QCU_NORMAL_FORM_MAXIMUM);
    setDateFormat(MTU_DEFAULT_DATE_FORMAT, MTU_DEFAULT_DATE_FORMAT_LEN);

    /* PROJ-2209 DBTIMEZONE */
    setTimezoneString( MTU_DB_TIMEZONE_STRING, MTU_DB_TIMEZONE_STRING_LEN );
    setTimezoneSecond( MTU_DB_TIMEZONE_SECOND );

    /* BUG-31144 */
    setMaxStatementsPerSession(mmuProperty::getMaxStatementsPerSession());

    // PROJ-1579 NCHAR
    setNlsNcharConvExcp(MTU_NLS_NCHAR_CONV_EXCP);
    //BUG-21122 : AUTO_REMOTE_EXEC �ʱ�ȭ
    setAutoRemoteExec(qciMisc::getAutoRemoteExec());
    // BUG-34830
    setTrclogDetailPredicate(QCU_TRCLOG_DETAIL_PREDICATE);
    // BUG-32101
    setOptimizerDiskIndexCostAdj(QCU_OPTIMIZER_DISK_INDEX_COST_ADJ);
    // BUG-43736
    setOptimizerMemoryIndexCostAdj(QCU_OPTIMIZER_MEMORY_INDEX_COST_ADJ);

    /* PROJ-2208 Multi Currency */
    setNlsTerritory( MTU_NLS_TERRITORY,
                     MTU_NLS_TERRITORY_LEN );
    setNlsISOCurrency( MTU_NLS_ISO_CURRENCY,
                       MTU_NLS_ISO_CURRENCY_LEN );
    setNlsCurrency( MTU_NLS_CURRENCY,
                    MTU_NLS_CURRENCY_LEN );
    setNlsNumChar( MTU_NLS_NUM_CHAR,
                   MTU_NLS_NUM_CHAR_LEN );

#ifdef ENDIAN_IS_BIG_ENDIAN
    setClientHostByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
#else
    setClientHostByteOrder(MMC_BYTEORDER_LITTLE_ENDIAN);
#endif

    setNumericByteOrder(MMC_BYTEORDER_BIG_ENDIAN);

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    setLobCacheThreshold(mmuProperty::getLobCacheThreshold());

    /* PROJ-1090 Function-based Index */
    setQueryRewriteEnable(QCU_QUERY_REWRITE_ENABLE);
    
    /* PROJ-2441 flashback */
    setRecyclebinEnable( QCU_RECYCLEBIN_ENABLE );

    /* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
    setLockTableUntilNextDDL( ID_FALSE );
    setTableIDOfLockTableUntilNextDDL( 0 );

    // BUG-41398 use old sort
    setUseOldSort( QCU_USE_OLD_SORT );

    // BUG-41944
    setArithmeticOpMode( MTU_ARITHMETIC_OP_MODE );
    
    /*
     * qciSession �ʱ�ȭ
     */

    IDE_TEST(qci::initializeSession(&mQciSession, this) != IDE_SUCCESS);

    /* PROJ-2462 Result Cache */
    setResultCacheEnable(QCU_RESULT_CACHE_ENABLE);
    setTopResultCacheMode(QCU_TOP_RESULT_CACHE_MODE);

    /* PROJ-2492 Dynamic sample selection */
    setOptimizerAutoStats(QCU_OPTIMIZER_AUTO_STATS);

    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    setOptimizerTransitivityOldRule(QCU_OPTIMIZER_TRANSITIVITY_OLD_RULE);

    /* BUG-42639 Monitoring query */
    setOptimizerPerformanceView(QCU_OPTIMIZER_PERFORMANCE_VIEW);

    setReplicationDDLSync( mmuProperty::getReplicationDDLSync() );
    setReplicationDDLSyncTimeout( mmuProperty::getReplicationDDLSyncTimeout() );

    setPrintOutEnable( QCU_PRINT_OUT_ENABLE );

    /* PROJ-2632 */
    setSerialExecuteMode( QCU_SERIAL_EXECUTE_MODE );
    setTrcLogDetailInformation( QCU_TRCLOG_DETAIL_INFORMATION );

    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    setReducePartPrepareMemory( QCU_REDUCE_PART_PREPARE_MEMORY );

    // BUG-47804 Default value of SHARD_IN_PSM_ENABLE is ID_TRUE
    mInfo.mShardInPSMEnable = ID_TRUE;

    /* BUG-48132 */
    setPlanHashOrSortMethod( QCU_OPTIMIZER_PLAN_HASH_OR_SORT_METHOD );

    /* BUG-48161 */
    setBucketCountMax( QCU_OPTIMIZER_BUCKET_COUNT_MAX );

    /* BUG-48348 */
    setEliminateCommonSubexpression( QCU_OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION );

    /*
     * Runtime Session Info
     */

    mInfo.mSessionState     = MMC_SESSION_STATE_INIT;
    mInfo.mEventFlag        = ID_ULONG(0);

    mInfo.mConnectTime      = mmtSessionManager::getBaseTime();
    mInfo.mIdleStartTime    = mInfo.mConnectTime;

    mInfo.mActivated        = ID_FALSE;
    mInfo.mOpenStmtCount    = 0;
    mInfo.mOpenHoldFetchCount = 0; /* PROJ-1381 FAC */
    mInfo.mCurrStmtID       = 0;
    mInfo.mAllocTransRetryCount = 0;

    /*
     * XA
     */

    mInfo.mXaSessionFlag    = ID_FALSE;
    mInfo.mXaAssocState     = MMD_XA_ASSOC_STATE_NOTASSOCIATED;

    /*
     * Database link
     */
    IDE_TEST( dkiGetGlobalTransactionLevel(
                  &(mInfo.mGlobalTransactionLevel) )
              != IDE_SUCCESS );
    IDE_TEST( dkiGetRemoteStatementAutoCommit(
                  &(mInfo.mDblinkRemoteStatementAutoCommit) )
              != IDE_SUCCESS );

    /*
     * MTL Language �ʱ�ȭ
     */

    mLanguage = NULL;

    /*
     * Statement List �ʱ�ȭ
     */

    IDU_LIST_INIT(&mStmtList);
    IDU_LIST_INIT(&mFetchList);
    IDU_LIST_INIT(&mCommitedFetchList); /* PROJ-1381 FAC : Commit�� FetchList ���� */

    mExecutingStatement = NULL;

    IDE_TEST_RAISE(mStmtListMutex.initialize((SChar*)"MMC_SESSION_STMT_LIST_MUTEX",
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS,
                   MutexInitFailed);

    /* PROJ-1381 FAC : FetchList�� �ٲܶ��� lock���� ��ȣ */
    IDE_TEST_RAISE(mFetchListMutex.initialize((SChar*)"MMC_SESSION_FETCH_LIST_MUTEX",
                                              IDU_MUTEX_KIND_POSIX,
                                              IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS,
                   MutexInitFailed);

    /*
     * Non-Autocommit Mode�� ���� Transaction ���� �ʱ�ȭ
     */

    mTrans          = NULL;
    mTransAllocFlag = ID_FALSE;
    mSessionBegin   = ID_FALSE;
    mTransLazyBegin = ID_FALSE;
    mTransPrepared  = ID_FALSE;
    idlOS::memset( &mTransXID, 0x00, ID_SIZEOF(ID_XID) );

    mLocalTrans     = NULL; /* PROJ-2436 ADO.NET MSDTC */
    mLocalTransBegin = ID_FALSE;

    mZKPendingFunc = NULL;
    /*
     * PROJ-1629 2���� ��� �������� ������ ���� �ʱ�ȭ
     */
    mChunk     = NULL;
    mChunkSize = 0;

    mOutChunk     = NULL;
    mOutChunkSize = 0;

    /*
     * LOB �ʱ�ȭ
     */
    //fix BUG-21311
    IDE_TEST(smuHash::initialize(&mLobLocatorHash,
                                 1,
                                 MMC_SESSION_LOB_HASH_BUCKET_CNT,
                                 ID_SIZEOF(smLobLocator),
                                 ID_FALSE,
                                 mmcLob::hashFunc,
                                 mmcLob::compFunc) != IDE_SUCCESS);

    /*
     * Queue �ʱ�ȭ
     */

    mQueueInfo     = NULL;
    mQueueEndTime  = 0;
    mQueueWaitTime = 0;
    mNeedQueueWait = ID_FALSE;  /* BUG-46183 */

    IDU_LIST_INIT_OBJ(&mQueueListNode, this);

    IDE_TEST(smuHash::initialize(&mEnqueueHash,
                                 1,
                                 mmuProperty::getQueueSessionHashTableSize(),
                                 ID_SIZEOF(UInt),
                                 ID_FALSE,
                                 mmqQueueInfo::hashFunc,
                                 mmqQueueInfo::compFunc) != IDE_SUCCESS);
    //PROJ-1677 DEQUEUE 
    IDE_TEST(smuHash::initialize(&mDequeueHash4Rollback,
                                 1,
                                 mmuProperty::getQueueSessionHashTableSize(),
                                 ID_SIZEOF(UInt),
                                 ID_FALSE,
                                 mmqQueueInfo::hashFunc,
                                 mmqQueueInfo::compFunc) != IDE_SUCCESS);

    clearPartialRollbackFlag();

    /*
     * Statistics �ʱ�ȭ
     */

    idvManager::initSession(&mStatistics, getSessionID(), this);
    idvManager::initSession(&mOldStatistics, getSessionID(), this);

    idvManager::initSQL(&mStatSQL,
                        &mStatistics,
                        getEventFlag(),
                        &(mInfo.mCurrStmtID),
                        aTask->getLink(),
                        aTask->getLinkCheckTime(),
                        IDV_OWNER_TRANSACTION);
    
    /*
     * Link�� Statistics ����
     */
    cmiSetLinkStatistics(aTask->getLink(), &mStatistics);

    IDE_TEST( dkiSessionCreate( getSessionID(), &mDatabaseLinkSession )
              != IDE_SUCCESS );
    
    SM_INIT_SCN(&mDeqViewSCN);

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* If mmcStatementManager::mStmtPageTableArr[ session ID ] is NULL, then alloc new StmtPageTable */
    IDE_TEST( mmcStatementManager::allocStmtPageTable( aSessionID ) != IDE_SUCCESS );

    /*
     * Initialize MutexPool
     */
    IDE_TEST_RAISE( mMutexPool.initialize() != IDE_SUCCESS, InsufficientMemory );

    /* BUG-21149 */
    //fix BUG-21794
    IDU_LIST_INIT(&(mXidLst));
    setNeedLocalTxBegin(ID_FALSE);

    /* PROJ-2177 User Interface - Cancel */
    IDE_TEST_RAISE(mStmtIDMap.initialize(IDU_MEM_MMC,
                                         MMC_STMT_ID_CACHE_COUNT,
                                         MMC_STMT_ID_MAP_SIZE) != IDE_SUCCESS,
                   InsufficientMemory);

    IDE_TEST_RAISE(mStmtIDPool.initialize(IDU_MEM_MMC,
                                          (SChar *)"MMC_STMTID_POOL",
                                          ID_SCALABILITY_SYS,
                                          ID_SIZEOF(mmcStmtID),
                                          MMC_STMT_ID_POOL_ELEM_COUNT,
                                          IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                          ID_TRUE,							/* UseMutex */
                                          IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                          ID_FALSE,							/* ForcePooling */
                                          ID_TRUE,							/* GarbageCollection */
                                          ID_TRUE,                          /* HWCacheLine */
                                          IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/)
                  != IDE_SUCCESS , InsufficientMemory);
    /*
     * BUG-38430
     */
    mInfo.mLastProcessRow = 0;

    /* PROJ-2473 SNMP ���� */
    mInfo.mSessionFailureCount = 0;

    /* PROj-2436 ADO.NET MSDTC */
    mTransEscalation = MMC_TRANS_DO_NOT_ESCALATE;

    // BUG-42464 dbms_alert package
    IDE_TEST( mInfo.mEvent.initialize( this ) != IDE_SUCCESS );

    /* PROJ-2638 shard native linker */
    mInfo.mShardNodeName[0] = '\0';

    /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
    mInfo.mCallByShardAnalyzeProtocol = ID_FALSE;

    /* PROJ-2660 */
    mInfo.mShardPin = SDI_SHARD_PIN_INVALID;

    /* BUG-46090 Meta Node SMN ���� */
    mInfo.mToBeShardMetaNumber = SDI_NULL_SMN;
    mInfo.mShardMetaNumber          = SDI_NULL_SMN;
    mInfo.mLastShardMetaNumber      = SDI_NULL_SMN;
    mInfo.mReceivedShardMetaNumber  = SDI_NULL_SMN;

    /* BUG-44967 */
    mInfo.mTransID = 0;

    /* BUG-45707 */
    mInfo.mShardClient = SDI_SHARD_CLIENT_FALSE;
    mInfo.mShardSessionType = SDI_SESSION_TYPE_USER;    // BUG-47324
    mInfo.mShardInternalLocalOperation = SDI_INTERNAL_OP_NOT;
    /* BUG-45899 */
    setTrclogDetailShard( SDU_TRCLOG_DETAIL_SHARD );
    setShardDDLLockTimeout( SDU_SHARD_DDL_LOCK_TIMEOUT );
    setShardDDLLockTryCount( SDU_SHARD_DDL_LOCK_TRY_COUNT );
    setDDLLockTimeout( smiGetDDLLockTimeOutProperty() );
    /* BUG-46092 */
    mInfo.mDataNodeFailoverType.initialize();

    /* BUG-46019 ���� ȣȯ�� ���� �ʱⰪ�� UNKNOWN���� �����ؾ� �Ѵ�. */
    setMessageCallback(MMC_MESSAGE_CALLBACK_UNKNOWN);

    // PROJ-2727
    idlOS::memset(&mInfo.mSessionPropValueStr, 0, ID_SIZEOF(mInfo.mSessionPropValueStr));
    mInfo.mSessionPropValueLen = 0;
    mInfo.mSessionPropID       = CMP_DB_PROPERTY_MAX;

    mInfo.mPropertyAttribute   = 0;

    mIsNeedBlockCommit         = ID_FALSE;
    
    SM_INIT_SCN(&mInfo.mGCTxCommitInfo.mCoordSCN);
    SM_INIT_SCN(&mInfo.mGCTxCommitInfo.mPrepareSCN);
    SM_INIT_SCN(&mInfo.mGCTxCommitInfo.mGlobalCommitSCN);
    SM_INIT_SCN(&mInfo.mGCTxCommitInfo.mLastSystemSCN);

    mInfo.mShardStatementRetry = SDU_SHARD_STATEMENT_RETRY;
    mInfo.mIndoubtFetchTimeout = mmuProperty::getIndoubtFetchTimeout(); 
    mInfo.mIndoubtFetchMethod  = mmuProperty::getIndoubtFetchMethod();

    setGlobalTransactionLevelFlag();
    
    setGCTxPermit(ID_TRUE);  /* PROJ-2733-DistTxInfo */

    /* TASK-7219 Non-shard DML */
    initStmtExecSeqForShardTx();

    mTransDump.init();

    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemory)
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION(MutexInitFailed)
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_LATCH_INIT));
    }
    IDE_EXCEPTION(ConnectErrInShardEnv)
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_CONNECT_ERROR_IN_SHARD_ENV));
    }
    IDE_EXCEPTION_END;

    (void)dkiSessionFree( &mDatabaseLinkSession );

    if( aTask != NULL )
    {
        cmiSetLinkStatistics(aTask->getLink(), NULL); /* BUG-41456 */
    }

    return IDE_FAILURE;
}

IDE_RC mmcSession::finalize()
{
    mmcStatement *sStmt;
    iduListNode  *sIterator;
    iduListNode  *sNodeNext;

    if (isXaSession() == ID_TRUE)
    {
        mmdXa::terminateSession(this);
    }

    IDE_ASSERT(clearLobLocator() == IDE_SUCCESS);
    IDE_ASSERT(clearEnqueue() == IDE_SUCCESS);

    if( mChunk != NULL)
    {
        IDE_ASSERT( mChunkSize != 0 );
        IDE_TEST( iduMemMgr::free( mChunk ) != IDE_SUCCESS );
        mChunk     = NULL;
        mChunkSize = 0;
    }

    if( mOutChunk != NULL)
    {
        IDE_ASSERT( mOutChunkSize != 0 );
        IDE_TEST( iduMemMgr::free( mOutChunk ) != IDE_SUCCESS );
        mOutChunk     = NULL;
        mOutChunkSize = 0;
    }

    /* BUG-47650 BUG-38585 IDE_ASSERT remove 
     * endSession()���� �̹� stmt�� free�ϹǷ� �� �������� getStmtList �� ����
     * ���� ITERATE �������� FIT�� ���� */
    IDU_FIT_POINT("mmcSession::finalize::FreeStatement");
    IDU_LIST_ITERATE_SAFE(getStmtList(), sIterator, sNodeNext)
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        /* BUG-38585 IDE_ASSERT remove */
        IDE_TEST(mmcStatementManager::freeStatement(sStmt) != IDE_SUCCESS);
    }

    IDE_ASSERT( dkiSessionFree( &mDatabaseLinkSession ) == IDE_SUCCESS );

    /* PROJ-1381 FAC : Stmt ���� �Ŀ��� CommitedFetchList�� ����־�� �Ѵ�. */
    IDE_ASSERT(IDU_LIST_IS_EMPTY(getCommitedFetchList()) == ID_TRUE);

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* mmtStmtPageTable�� ���� mmcStmtSlot���� ó�� �ϳ��� ����� ��� ������ */
    IDE_ASSERT( mmcStatementManager::freeAllStmtSlotExceptFirstOne( getSessionID() ) == IDE_SUCCESS );

    /* BUG-47650 BUG-38585 IDE_ASSERT remove 
     * disconnectProtocol ���� getSessionState ���� �����. 
     * ���� if�� �������� FIT ���� */
    IDU_FIT_POINT("mmcSession::finalize::EndSession");
    if (getSessionState() == MMC_SESSION_STATE_SERVICE)
    {
        setSessionState(MMC_SESSION_STATE_ROLLBACK);

        /* BUG-38585 IDE_ASSERT remove */
        IDE_TEST(endSession() != IDE_SUCCESS);
    }

    if ((mTrans != NULL) && (mTransAllocFlag == ID_TRUE))
    {
        /*mmcTrans::free inside mTrans = NULL*/
        IDE_ASSERT(mmcTrans::free(this, mTrans) == IDE_SUCCESS);
        mTrans = NULL;
        mTransAllocFlag = ID_FALSE;
        setSessionBegin( ID_FALSE );
    }

    mZKPendingFunc = NULL;
    // fix BUG-23374
    // ���� �߻��� �����ڵ带 ��� �� ASSERT
    IDE_ASSERT(qci::finalizeSession(&mQciSession, this) == IDE_SUCCESS);

    cmiSetLinkStatistics(getTask()->getLink(), NULL);

    IDE_ASSERT(smuHash::destroy(&mLobLocatorHash) == IDE_SUCCESS);
    IDE_ASSERT(smuHash::destroy(&mEnqueueHash) == IDE_SUCCESS);
    //PROJ-1677 DEQUEUE
    IDE_ASSERT(smuHash::destroy(&mDequeueHash4Rollback) == IDE_SUCCESS);

    /* BUG-38585 IDE_ASSERT remove */
    IDE_TEST(disconnect(ID_FALSE) != IDE_SUCCESS);

    IDE_ASSERT(mStmtListMutex.destroy() == IDE_SUCCESS);
    IDE_ASSERT(mFetchListMutex.destroy() == IDE_SUCCESS); /* PROJ-1381 FAC */

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /*
     * finalize MutexPool
     */
    IDE_TEST( mMutexPool.finalize() != IDE_SUCCESS );

    /* PROJ-2177 User Interface - Cancel */
    IDE_TEST(mStmtIDMap.destroy() != IDE_SUCCESS);
    IDE_TEST(mStmtIDPool.destroy() != IDE_SUCCESS);

    // BUG-42464 dbms_alert package
    IDE_ASSERT(mInfo.mEvent.finalize() == IDE_SUCCESS);

    /* BUG-46092 */
    mInfo.mDataNodeFailoverType.finalize();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmcSession::preFinalizeShardSession()
{
    mmcTransObj         * sTrans            = NULL;
    mmcTxConcurrency    * sConcurrency      = NULL;
    mmcSession          * sDelegatedSession = NULL;

    sTrans = getTransPtr();

    if ( mmcTrans::isShareableTrans( sTrans ) == ID_TRUE )
    {
        sConcurrency = &sTrans->mShareInfo->mConcurrency;

        if ( isShardCoordinatorSession() == ID_TRUE )
        {
            IDE_ASSERT( sConcurrency->mMutex.lock( NULL ) == IDE_SUCCESS );

            if ( dkiIsReadOnly( getDatabaseLinkSession() ) == ID_FALSE )
            {
                mmcTrans::setLocalTransactionBroken( sTrans,
                                                     getSessionID(),
                                                     ID_TRUE );
            }

            IDE_ASSERT( dkiSessionFree( getDatabaseLinkSession() ) == IDE_SUCCESS );

            sDelegatedSession = sTrans->mShareInfo->mTxInfo.mDelegatedSessions;
            if ( sDelegatedSession != NULL )
            {
                if ( dkiIsReadOnly( sDelegatedSession->getDatabaseLinkSession() ) == ID_FALSE )
                {
                    mmcTrans::setLocalTransactionBroken( sTrans,
                                                         getSessionID(),
                                                         ID_TRUE );
                }

                IDE_ASSERT( dkiSessionFree( sDelegatedSession->getDatabaseLinkSession() ) == IDE_SUCCESS );

                mmcTrans::removeDelegatedSession( sTrans->mShareInfo , sDelegatedSession );

                MMC_SHARED_TRANS_TRACE( this,
                                        sTrans,
                                        "preFinalizeShardSession: remove delegate session");
            }

            IDE_ASSERT( sConcurrency->mMutex.unlock() == IDE_SUCCESS );
        }
        else if ( isShardLibrarySession() == ID_TRUE )
        {
            IDE_ASSERT( sConcurrency->mMutex.lock( NULL ) == IDE_SUCCESS );

            while ( sConcurrency->mBlockCount > 0 )
            {
                (void)sConcurrency->mCondVar.wait( &sConcurrency->mMutex );
            }

            if ( ( getSessionBegin() == ID_TRUE ) ||
                 ( dkiIsReadOnly( getDatabaseLinkSession() ) == ID_FALSE ) )
            {
                mmcTrans::setLocalTransactionBroken( sTrans,
                                                     getSessionID(),
                                                     ID_TRUE );
            }

            IDE_ASSERT( dkiSessionFree( getDatabaseLinkSession() ) == IDE_SUCCESS );

            mmcTrans::removeDelegatedSession( sTrans->mShareInfo , this );

            MMC_SHARED_TRANS_TRACE( this,
                                    sTrans,
                                    "preFinalizeShardSession: remove delegate session");

            IDE_ASSERT( sConcurrency->mMutex.unlock() == IDE_SUCCESS );
        }
    }
}

IDE_RC mmcSession::findLanguage()
{
    IDE_TEST(qciMisc::getLanguage(mInfo.mNlsUse, &mLanguage) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_IDN_MISMATCH_ERROR, 
                                mInfo.mNlsUse,
                                mtl::mDBCharSet->names->string));
    }

    return IDE_FAILURE;
}

/* BUG-28866 : Log ��� �Լ� */
void mmcSession::loggingSession(SChar *aLogInOut, mmcSessionInfo *aInfo)
{
    if ( mmuProperty::getMmSessionLogging() == 1 )
    {
        SChar *sClientAppInfo;
        if ( aInfo->mClientAppInfo[0] == 0 )
        {
            sClientAppInfo = (SChar*)"";
        }
        else
        {
            sClientAppInfo = (SChar*)", APP_INFO: ";
        }
        ideLog::logLine(IDE_MM_0, "[%s: %d] DBUser: %s, IP: %s, PID: %llu, Client_Type: %s%s%s",
                        aLogInOut,
                        aInfo->mSessionID,
                        aInfo->mUserInfo.loginID,
                        aInfo->mUserInfo.loginIP,
                        aInfo->mClientPID,
                        aInfo->mClientType,
                        sClientAppInfo,
                        aInfo->mClientAppInfo);
    }
    else
    {
        /* Do nothing */
    }
}

IDE_RC mmcSession::disconnect(idBool aClearClientInfoFlag)
{
    /*
     * BUGBUG: Alloc�� Statement�鿡 ���� �ٽ� Execute�� ������ Prepare�� �ٽ� �ϵ��� �ؾ���
     */

    if ((mInfo.mUserInfo.mIsSysdba == ID_TRUE) && (getTask()->isShutdownTask() != ID_TRUE))
    {
        IDE_ASSERT(mmtAdminManager::unsetTask(getTask()) == IDE_SUCCESS);
    }

    /* BUG-28866 */
    loggingSession((SChar*)"Logout", &mInfo);

    idlOS::memset(&mInfo.mUserInfo, 0, ID_SIZEOF(mInfo.mUserInfo));

    if (aClearClientInfoFlag == ID_TRUE)
    {
        mInfo.mClientPackageVersion[0]  = 0;
        mInfo.mClientProtocolVersion[0] = 0;
        mInfo.mClientPID                = ID_ULONG(0);
        mInfo.mClientType[0]            = 0;
        mInfo.mNlsUse[0]                = 0;
        mLanguage                       = NULL;

        // PROJ-1579 NCHAR
        mInfo.mNlsNcharLiteralReplace   = 0;
    }

    /* BUG-47650 BUG-38585 IDE_ASSERT remove */
    IDU_FIT_POINT("mmcSession::disconnect::EndSession");
    if (getSessionState() == MMC_SESSION_STATE_SERVICE)
    {
        setSessionState(MMC_SESSION_STATE_END);

        /* BUG-38585 IDE_ASSERT remove */
        IDE_TEST(endSession() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


void mmcSession::changeSessionStateService()
{
    UInt sLen;
    if (getSessionState() == MMC_SESSION_STATE_AUTH)
    {
        if ((mInfo.mClientPackageVersion[0]  != 0) &&
            (mInfo.mClientProtocolVersion[0] != 0) &&
            (mInfo.mClientPID                != ID_ULONG(0)) &&
            (mInfo.mClientType[0]            != 0)) 
        {
            if ((mInfo.mNlsUse[0] != 0) && (mLanguage != NULL))
            {
                if (idlOS::strncmp(mInfo.mClientType, "JDBC", 4) == 0)
                {
                    // BUGBUG : [PROJ-1752] ���������� JDBC�� LIST ���������� �������� ����.
                    mInfo.mHasClientListChannel = ID_FALSE;
                    mInfo.mFetchProtocolType    = MMC_FETCH_PROTOCOL_TYPE_NORMAL;   // BUG-34725
                    mInfo.mRemoveRedundantTransmission = 0;                         // PROJ-2256

                    setClientHostByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
                    setNumericByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
                    setSessionState(MMC_SESSION_STATE_SERVICE);
                }
                else if (idlOS::strncmp(mInfo.mClientType, "NEW_JDBC", 8) == 0)
                {
                    mInfo.mHasClientListChannel = ID_TRUE;

                    setClientHostByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
                    setNumericByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
                    setSessionState(MMC_SESSION_STATE_SERVICE);
                }
                else
                {
                    if (idlOS::strchr(mInfo.mClientType, '-'))
                    {
                        sLen = idlOS::strlen(mInfo.mClientType);

                        if (idlOS::strcmp(mInfo.mClientType + sLen - 2, "BE") == 0)
                        {
                            setClientHostByteOrder(MMC_BYTEORDER_BIG_ENDIAN);
                        }
                        else if (idlOS::strcmp(mInfo.mClientType + sLen - 2, "LE") == 0)
                        {
                            setClientHostByteOrder(MMC_BYTEORDER_LITTLE_ENDIAN);
                        }
                    }

                    setNumericByteOrder(MMC_BYTEORDER_LITTLE_ENDIAN);
                    setSessionState(MMC_SESSION_STATE_SERVICE);
                }
                /* BUG-28866 */
                loggingSession((SChar*)"Login", &mInfo);
            }
        }

        if (getSessionState() == MMC_SESSION_STATE_SERVICE)
        {
            IDE_ASSERT(beginSession() == IDE_SUCCESS);
        }
    }
}

IDE_RC mmcSession::beginSession()
{
    idBool      sIsDummyBegin = ID_FALSE;

    /*
     * Non-AUTO COMMIT ����� ������ TX�� begin�Ѵ�.
     */

    if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT)
    {
        initTransStartMode();
        (void)allocTrans();
        if (getTransLazyBegin() == ID_FALSE) // BUG-45772 TRANSACTION_START_MODE ����
        {
            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             getSessionInfoFlagForTx(), 
                             this,
                             &sIsDummyBegin );
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

    return IDE_SUCCESS;
}

IDE_RC mmcSession::endSession()
{
    mmcStatement *sStmt;
    iduListNode  *sIterator;
    iduListNode  *sNodeNext;
    idBool        sSuccess = ID_FALSE;
    
    switch (getSessionState())
    {
        case MMC_SESSION_STATE_END:
            sSuccess = ID_TRUE;
            break;

        case MMC_SESSION_STATE_ROLLBACK:
            sSuccess = ID_FALSE;
            break;

        default:
            IDE_CALLBACK_FATAL("invalid session state");
            break;
    }

    IDU_LIST_ITERATE_SAFE(getStmtList(), sIterator, sNodeNext)
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        IDE_TEST(sStmt->closeCursor(sSuccess) != IDE_SUCCESS);
        
        IDE_TEST(mmcStatementManager::freeStatement(sStmt) != IDE_SUCCESS);
    }

    if ( getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT ) // non-autocommit �� Transaction
    {
        // fix BUG-20850
        if ( getXaAssocState() != MMD_XA_ASSOC_STATE_ASSOCIATED )
        {
            if ( getTransBegin() == ID_TRUE )
            {
                if ( mTrans == NULL )
                {
                    ideLog::log( IDE_SERVER_0, "#### mmcSession::endSession (%d) XaAssocState: %d,LINE:%d ",
                                 getSessionID(),
                                 getXaAssocState(),
                                 __LINE__);
                    IDE_ASSERT(0);
                }
                else
                {   
                    if ( mmcTrans::getSmiTrans(mTrans)->getTrans() == NULL )
                    {
                        ideLog::log( IDE_SERVER_0, "#### mmcSession::endSession (%d) XaAssocState: %d,LINE:%d",
                                     getSessionID(),
                                     getXaAssocState(),
                                     __LINE__);
                        IDE_ASSERT(0);
                    }
                    else
                    {
                        //nothing to do.
                    }
                }
            }
            else
            {
                //nothing to do.
            }

            switch ( getSessionState() )
            {
                case MMC_SESSION_STATE_END:
                    if ( mmcTrans::commit( mTrans, this ) != IDE_SUCCESS )
                    {
                        /* PROJ-1832 New database link */
                        IDE_ASSERT( mmcTrans::rollbackForceDatabaseLink(
                                        mTrans, this )
                                    == IDE_SUCCESS );
                    }
                    break;

                case MMC_SESSION_STATE_ROLLBACK:
                    /* PROJ-1832 New database link */
                    IDE_ASSERT( mmcTrans::rollbackForceDatabaseLink(
                                    mTrans, this )
                                == IDE_SUCCESS );
                    break;

                default:
                    IDE_CALLBACK_FATAL("invalid session state");
                    break;
            }
        }
        else
        {
            /* Nothing to do */
        }
        
        // PROJ-1407 Temporary Table
        qci::endSession( &mQciSession );    
    }
    else
    {
        /* Nothing to do */
    }

    setSessionState(MMC_SESSION_STATE_INIT);

    // ���� ����� ��� ���� �߰�
    applyStatisticsToSystem();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1381 Fetch Across Commits */

/**
 * FetchList�� �ִ� Stmt�� Ŀ���� �ݴ´�.
 *
 * @param aSuccess[IN] ?
 * @param aCursorCloseMode[IN] Cursor close mode
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 */
IDE_RC mmcSession::closeAllCursor(idBool        aSuccess,
                                  mmcCloseMode  aCursorCloseMode)
{
    mmcStatement *sStmt;
    iduListNode  *sIterator;
    iduListNode  *sNodeNext;

    if (aCursorCloseMode == MMC_CLOSEMODE_NON_COMMITED)
    {
        IDU_LIST_ITERATE_SAFE(getFetchList(), sIterator, sNodeNext)
        {
            sStmt = (mmcStatement *)sIterator->mObj;

            IDE_TEST(sStmt->closeCursor(aSuccess) != IDE_SUCCESS);

            if (sStmt->getFetchFlag() == MMC_FETCH_FLAG_PROCEED)
            {
                sStmt->setFetchFlag(MMC_FETCH_FLAG_INVALIDATED);
            }
        }
    }
    else /* is MMC_CLOSEMODE_REMAIN_HOLD */
    {
        IDU_LIST_ITERATE_SAFE(getFetchList(), sIterator, sNodeNext)
        {
            sStmt = (mmcStatement *)sIterator->mObj;

            if (sStmt->getCursorHold() == MMC_STMT_CURSOR_HOLD_OFF)
            {
                IDE_TEST(sStmt->closeCursor(aSuccess) != IDE_SUCCESS);

                if (sStmt->getFetchFlag() == MMC_FETCH_FLAG_PROCEED)
                {
                    sStmt->setFetchFlag(MMC_FETCH_FLAG_INVALIDATED);
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * FetchList�� �ִ� Stmt�� Ŀ���� ��� �ݴ´�.
 *
 * @param aFetchList[IN] Ŀ���� ���� Stmt�� ���� ����Ʈ
 * @param aSuccess[IN] ?
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 */
IDE_RC mmcSession::closeAllCursorByFetchList(iduList *aFetchList,
                                             idBool   aSuccess)
{
    mmcStatement *sStmt;
    iduListNode  *sIterator;
    iduListNode  *sNodeNext;

    IDU_LIST_ITERATE_SAFE(aFetchList, sIterator, sNodeNext)
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        IDE_TEST(sStmt->closeCursor(aSuccess) != IDE_SUCCESS);

        if (sStmt->getFetchFlag() == MMC_FETCH_FLAG_PROCEED)
        {
            sStmt->setFetchFlag(MMC_FETCH_FLAG_INVALIDATED);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ���� :
 *   �������̵� ������ ���õ� �Լ�.
 *   ���� ���̵��� ���, 2PC ������ ���ؼ� DK ���� NativeLinker ����� ���� �����.
 *   �̰��� ���� �Ǵ� �κ��� �ڵ��������
 *
 *     dktGlobalCoordinator::executePrepare -->
 *     dktGlobalCoordinator::executeTwoPhaseCommitPrepareForShard
 *
 *   ���� ���۵Ǹ�, �� �Լ��� �����ͳ�忡�� DK�� Ʈ������� �����ϱ����Ͽ� ȣ��Ǵ� �κ��Դϴ�.
 *
 * @param aXID[IN]       XaTransID
 * @param aReadOnly[OUT] Ʈ������� ReadOnly
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 **/
IDE_RC mmcSession::prepareForShard( ID_XID * aXID,
                                    idBool * aReadOnly,
                                    smSCN  * aPrepareSCN )
{
    IDE_TEST_RAISE( isAutoCommit() == ID_TRUE, ERR_AUTOCOMMIT_MODE );

    IDE_TEST_CONT( ( ( getTransPrepared() == ID_TRUE ) &&
                     ( mmdXid::compFunc( aXID, getTransPreparedXID() ) == 0 ) ),
                   END_OF_FUNC );

    if ( getSessionBegin() == ID_TRUE )
    {
        /* Session commit */
        IDE_TEST(closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) != IDE_SUCCESS);

        lockForFetchList();
        IDU_LIST_JOIN_LIST(getCommitedFetchList(), getFetchList());
        unlockForFetchList();

        IDE_TEST_RAISE(isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);
    }
    else
    {
        /* Nothing to do */
    }

    /* Prepare OR Check Read-only */
    IDE_TEST( mmcTrans::prepareForShard( mTrans,
                                         this,
                                         aXID,
                                         aReadOnly,
                                         aPrepareSCN )
              != IDE_SUCCESS );

    if ( *aReadOnly == ID_TRUE )
    {
        IDU_FIT_POINT("mmcSession::prepareForShard::readOnlyCommit");

        /* read only�� ��� commit�� �����Ѵ�. */
        IDE_TEST( commit( ID_FALSE ) != IDE_SUCCESS );
    }
    else
    {
        (void)clearLobLocator();

        setActivated(ID_FALSE); // ������ �ʱ���·� ����.
    }

    IDE_EXCEPTION_CONT( END_OF_FUNC );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_AUTOCOMMIT_MODE)
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_CANT_SET_TRANSACTION_IN_AUTOCOMMIT_MODE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::prepareForShardDelegateSession( mmcTransObj * aTransObj,
                                                   ID_XID      * aXID,
                                                   idBool      * aIsReadOnly,
                                                   smSCN       * aPrepareSCN )
{
    mmcSession * sDelegatedSession = NULL;
    idBool       sBlocked          = ID_FALSE;
    idBool       sLocked           = ID_TRUE;

    IDE_TEST( blockDelegateSession( aTransObj, &sDelegatedSession ) != IDE_SUCCESS );
    sBlocked = ID_TRUE;

    if ( sDelegatedSession != NULL )
    {
        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "prepareForShardDelegateSession: unlock");
        mmcTrans::unfixSharedTrans( aTransObj, getSessionID() );
        sLocked = ID_FALSE;

        IDE_TEST( sDelegatedSession->prepareForShard( aXID,
                                                     aIsReadOnly,
                                                     aPrepareSCN )
                  != IDE_SUCCESS );

        mmcTrans::fixSharedTrans( aTransObj, getSessionID()  );
        sLocked = ID_TRUE;
        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "prepareForShardDelegateSession: lock");

        unblockDelegateSession( aTransObj );
        sBlocked = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocked == ID_FALSE )
    {
        mmcTrans::fixSharedTrans( aTransObj, getSessionID() );

        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "prepareForShardDelegateSession: endPending fail");
    }

    if ( sBlocked == ID_TRUE )
    {
        unblockDelegateSession( aTransObj );
    }

    return IDE_FAILURE;
}

/**
 * ���� :
 *     DK ��⿡ �����͸� �߰��ϴ� ���� �����ϰ� Commit/Rollback�ϴ� �Լ�
 *
 * @param aXID[OUT]     XaTransID
 * @param aIsCommit[IN] Commit/Rollback �÷���
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 **/
IDE_RC mmcSession::endPendingTrans( ID_XID * aXID,
                                    idBool   aIsCommit,
                                    smSCN  * aGlobalCommitSCN )
{
    idBool  sIsDummyBegin = ID_FALSE;

    if ( getTransPrepared() == ID_TRUE )
    {
        setCoordGlobalCommitSCN( getShardClientInfo(), aGlobalCommitSCN );

        if ( aIsCommit == ID_TRUE )
        {
            dkiCommit( getDatabaseLinkSession() );
        }
        else
        {
            (void)dkiRollback( getDatabaseLinkSession(), NULL );  /* BUG-48489 */
        }

        IDE_TEST( mmcTrans::endPendingSharedTx( this,
                                                aXID,
                                                aIsCommit,
                                                aGlobalCommitSCN )
                  != IDE_SUCCESS );

        if ( ( getTransLazyBegin() == ID_FALSE ) || // BUG-45772 TRANSACTION_START_MODE ����
             ( isAllStmtEnd() == ID_FALSE ) )       // BUG-45772 Fetch Across Commit ����
        {
            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             getSessionInfoFlagForTx(), 
                             this,
                             &sIsDummyBegin );
        }

        setActivated(ID_FALSE); // ������ �ʱ���·� ����.
    }
    else
    {
        IDE_TEST( mmcTrans::endPendingSharedTx( this,
                                                aXID,
                                                aIsCommit,
                                                aGlobalCommitSCN )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::endPendingSharedTxDelegateSession( mmcTransObj * aTransObj,
                                                      ID_XID      * aXID,
                                                      idBool        aIsCommit,
                                                      smSCN       * aGlobalCommitSCN )
{
    mmcSession * sDelegatedSession = NULL;
    idBool       sBlocked          = ID_FALSE;
    idBool       sLocked           = ID_TRUE;

    IDE_TEST( blockDelegateSession( aTransObj, &sDelegatedSession ) != IDE_SUCCESS );
    sBlocked = ID_TRUE;

    if ( sDelegatedSession != NULL )
    {
        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "endPendingSharedTxDelegateSession: unlock");
        mmcTrans::unfixSharedTrans( aTransObj, getSessionID() );
        sLocked = ID_FALSE;

        IDE_TEST( sDelegatedSession->endPendingTrans( aXID, aIsCommit, aGlobalCommitSCN ) != IDE_SUCCESS);

        mmcTrans::fixSharedTrans( aTransObj, getSessionID()  );
        sLocked = ID_TRUE;
        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "endPendingSharedTxDelegateSession: lock");

        unblockDelegateSession( aTransObj );
        sBlocked = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocked == ID_FALSE )
    {
        mmcTrans::fixSharedTrans( aTransObj, getSessionID() );

        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "endPendingSharedTxDelegateSession: endPending fail");
    }

    if ( sBlocked == ID_TRUE )
    {
        unblockDelegateSession( aTransObj );
    }

    return IDE_FAILURE;
}

// autocommit ��忡���� ����!! -> BUG-11251 ���� �ƴ�
IDE_RC mmcSession::commit(idBool aInStoredProc)
{
    idBool      sIsDummyBegin = ID_FALSE;

    IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE,
                   InvalidSessionState);
    
    if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT)
    {
        if (aInStoredProc == ID_FALSE)
        {
            /* PROJ-1381 FAC : commit�� Holdable Fetch�� CommitedFetchList�� ���� */
            /* BUG-38585 IDE_ASSERT remove */
            IDU_FIT_POINT("mmcSession::commit::CloseCursor");
            IDE_TEST(closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) != IDE_SUCCESS);

            lockForFetchList();
            IDU_LIST_JOIN_LIST(getCommitedFetchList(), getFetchList());
            unlockForFetchList();
            IDE_TEST_RAISE(isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);

            IDE_TEST(mmcTrans::commit(mTrans, this, SMI_DO_NOT_RELEASE_TRANSACTION) != IDE_SUCCESS);

            if ( ( getTransLazyBegin() == ID_FALSE ) || // BUG-45772 TRANSACTION_START_MODE ����
                 ( isAllStmtEnd() == ID_FALSE ) )       // BUG-45772 Fetch Across Commit ����
            {
                mmcTrans::begin( mTrans, 
                                 &mStatSQL, 
                                 getSessionInfoFlagForTx(), 
                                 this,
                                 &sIsDummyBegin );
            }
            else
            {
                /* Nothing to do */
            }

            setActivated(ID_FALSE); // ������ �ʱ���·� ����.
        }
        else
        {
            // Commit In Stored Procedure
            IDE_TEST(mmcTrans::commit(mTrans, this, SMI_DO_NOT_RELEASE_TRANSACTION) != IDE_SUCCESS);

            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             getSessionInfoFlagForTx(), 
                             this,
                             &sIsDummyBegin );

            // SP ���ο��� commit�� ��� �ǵ��ư� �κ��� ���.
            // To Fix BUG-12512 : PSM ���۽� EXP SVP -> IMP SVP
            mmcTrans::reservePsmSvp(mTrans, ID_FALSE);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::commitForceDatabaseLink( idBool aInStoredProc )
{
    idBool      sIsDummyBegin = ID_FALSE;

    IDE_TEST_RAISE( getSessionState() != MMC_SESSION_STATE_SERVICE,
                    InvalidSessionState );


    if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT)
    {
        if ( aInStoredProc == ID_FALSE )
        {
            /* PROJ-1381 FAC : commit�� Holdable Fetch�� CommitedFetchList�� ���� */
            /* BUG-38585 IDE_ASSERT remove */
            IDU_FIT_POINT("mmcSession::commitForceDatabaseLink::CloseAllCursor");
            IDE_TEST( closeAllCursor( ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD )
                        != IDE_SUCCESS );

            lockForFetchList();
            IDU_LIST_JOIN_LIST( getCommitedFetchList(), getFetchList() );
            unlockForFetchList();
            IDE_TEST_RAISE( isAllStmtEndExceptHold() != ID_TRUE,
                            StmtRemainError );
            
            IDE_TEST( mmcTrans::commitForceDatabaseLink(
                          mTrans, this, SMI_DO_NOT_RELEASE_TRANSACTION )
                      != IDE_SUCCESS );

            if ( ( getTransLazyBegin() == ID_FALSE ) || // BUG-45772 TRANSACTION_START_MODE ����
                 ( isAllStmtEnd() == ID_FALSE ) )       // BUG-45772 Fetch Across Commit ����
            {
                mmcTrans::begin( mTrans,
                                 &mStatSQL,
                                 getSessionInfoFlagForTx(),
                                 this,
                                 &sIsDummyBegin );
            }
            else
            {
                /* Nothing to do */
            }
            
            setActivated( ID_FALSE ); // ������ �ʱ���·� ����.
        }
        else
        {
            // Commit In Stored Procedure
            IDE_TEST( mmcTrans::commitForceDatabaseLink(
                          mTrans, this, SMI_DO_NOT_RELEASE_TRANSACTION )
                      != IDE_SUCCESS );
            
            mmcTrans::begin( mTrans,
                             &mStatSQL,
                             getSessionInfoFlagForTx(),
                             this,
                             &sIsDummyBegin );
            
            // SP ���ο��� commit�� ��� �ǵ��ư� �κ��� ���.
            // To Fix BUG-12512 : PSM ���۽� EXP SVP -> IMP SVP
            mmcTrans::reservePsmSvp(mTrans, ID_FALSE);
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( InvalidSessionState );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_INVALID_SESSION_STATE ) );
    }
    IDE_EXCEPTION( StmtRemainError );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_OTHER_STATEMENT_REMAINS ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// autocommit ��忡���� ����!! -> BUG-11251 ���� �ƴ�
// Do not raise fatal errors when preserving statements is needed,
// in other words, when aInStoredProc is ID_TRUE.

IDE_RC mmcSession::rollback(const SChar *aSavePoint, idBool aInStoredProc)
{
    const SChar *sDecidedSavePoint = aSavePoint;
    idBool       sIsDummyBegin     = ID_FALSE;
    idBool       sIsTxBegin        = ID_FALSE;

    IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE, InvalidSessionState);

    if (getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT)
    {
        IDE_TEST_RAISE(aSavePoint != NULL, SavepointNotFoundError);
    }
    else
    {
        /* BUG-48489 Partial, Total rollback�� �Ǵ��ؾ� �Ѵ�. */
        if ((aSavePoint != NULL) && (isShardCoordinatorSession() == ID_TRUE))
        {
            sDecidedSavePoint = mmcTrans::decideTotalRollback(mTrans, aSavePoint);
        }

        if (sDecidedSavePoint == NULL) // total rollback
        {
            if (aInStoredProc == ID_FALSE)
            {
                /* PROJ-1381 FAC, PROJ-2694 FAR : rollback �Ŀ��� ���� ������ hold Ŀ���� ���� */
                if (mmcTrans::isReusableRollback(mTrans) == ID_TRUE)
                {
                    IDE_TEST( closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( closeAllCursor(ID_TRUE, MMC_CLOSEMODE_NON_COMMITED) != IDE_SUCCESS );
                }

                lockForFetchList();
                IDU_LIST_JOIN_LIST( getCommitedFetchList(), getFetchList() );
                unlockForFetchList();

                IDE_TEST_RAISE(isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);

                IDE_TEST( mmcTrans::rollback( mTrans,
                                              this,
                                              sDecidedSavePoint,
                                              SMI_DO_NOT_RELEASE_TRANSACTION )
                          != IDE_SUCCESS );

                if ( ( getTransLazyBegin() == ID_FALSE ) || // BUG-45772 TRANSACTION_START_MODE ����
                     ( isAllStmtEnd() == ID_FALSE ) )       // BUG-45772 Fetch Across Commit ����
                {
                    mmcTrans::begin( mTrans, 
                                     &mStatSQL, 
                                     getSessionInfoFlagForTx(), 
                                     this,
                                     &sIsDummyBegin );
                    sIsTxBegin = ID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }

                // TASK-7244
                // SHARD ENABLE�̰� PSM implicit savepoint�� ������
                // PSM���� total rollback�� �����ϰ� �����Ѵ�.
                if ((SDU_SHARD_ENABLE == 1) &&
                   (mmcTrans::isShardPsmSvpReserved(mTrans) == ID_TRUE) )
                {
                    if ( sIsTxBegin == ID_FALSE )
                    {
                        mmcTrans::begin( mTrans, 
                                         &mStatSQL, 
                                         getSessionInfoFlagForTx(), 
                                         this,
                                         &sIsDummyBegin );
                    }
                    mmcTrans::reservePsmSvp(mTrans, ID_TRUE);
                }

                setActivated(ID_FALSE); // ������ �ʱ���·� ����.
            }
            else
            {
                // stored procedure needs the statement to be preserved.
                IDE_TEST(mmcTrans::rollback(mTrans, this, sDecidedSavePoint, SMI_DO_NOT_RELEASE_TRANSACTION) != IDE_SUCCESS);

                mmcTrans::begin( mTrans, 
                                 &mStatSQL, 
                                 getSessionInfoFlagForTx(), 
                                 this,
                                 &sIsDummyBegin );

                // SP ���ο��� commit�� ��� �ǵ��ư� �κ��� ���.
                // To Fix BUG-12512 : PSM ���۽� EXP SVP -> IMP SVP
                mmcTrans::reservePsmSvp(mTrans, ID_FALSE);
            }
        }
        else // partial rollback to explicit savepoint
        {
            /* To Fix BUG-47069 : For shard transaction lazy begin */
            if (getSessionBegin() == ID_FALSE)
            {
                /* BUG-48489 Coord sesssion������ TX�� Begin ���°� �ƴϸ� ������ �Ѿ��. */
                if (isShardCoordinatorSession() == ID_TRUE)
                {
                    IDE_CONT(SKIP_PARTIAL_ROLLBACK);
                }
                else
                {
                    IDE_RAISE(SavepointNotFoundError);
                }
            }

            // TASK-7244 Shard Global Procedure partial rollback�� PSM implicit savepoint rollback���� ����Ѵ�.
            if ( (SDU_SHARD_ENABLE == 1) &&
                 (idlOS::strMatch( aSavePoint,
                                   idlOS::strlen(aSavePoint),
                                   SAVEPOINT_FOR_SHARD_GLOBAL_PROC_PARTIAL_ROLLBACK,
                                   idlOS::strlen(SAVEPOINT_FOR_SHARD_GLOBAL_PROC_PARTIAL_ROLLBACK) )
                  == 0) )
            {
                IDE_TEST(mmcTrans::abortToPsmSvp(mTrans) != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST(mmcTrans::rollback(mTrans, this, sDecidedSavePoint, SMI_DO_NOT_RELEASE_TRANSACTION) != IDE_SUCCESS);
            }
        }
    }

    IDE_EXCEPTION_CONT(SKIP_PARTIAL_ROLLBACK);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION(SavepointNotFoundError);
    {
        // non-autocommit��忡���� savepoint�����ڵ��
        // ��ġ��Ű�� ���� sm�����ڵ带 ������
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundSavepoint));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::rollbackForceDatabaseLink( idBool aInStoredProc )
{
    idBool      sIsDummyBegin = ID_FALSE;

    IDE_TEST_RAISE( getSessionState() != MMC_SESSION_STATE_SERVICE,
                    InvalidSessionState );

    if ( getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT )
    {
        /* nothing to do */
    }
    else
    {
        if ( aInStoredProc == ID_FALSE )
        {
            /* PROJ-1381 FAC, PROJ-2694 FAR : rollback �Ŀ��� ���� ������ hold Ŀ���� ���� */
            if (mmcTrans::isReusableRollback(mTrans) == ID_TRUE)
            {
                IDE_TEST( closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( closeAllCursor(ID_TRUE, MMC_CLOSEMODE_NON_COMMITED) != IDE_SUCCESS );
            }

            lockForFetchList();
            IDU_LIST_JOIN_LIST( getCommitedFetchList(), getFetchList() );
            unlockForFetchList();

            IDE_TEST_RAISE( isAllStmtEndExceptHold() != ID_TRUE,
                            StmtRemainError );
            
            IDE_ASSERT( mmcTrans::rollbackForceDatabaseLink(
                            mTrans,
                            this,
                            SMI_DO_NOT_RELEASE_TRANSACTION )
                        == IDE_SUCCESS );    
            
            if ( ( getTransLazyBegin() == ID_FALSE ) || // BUG-45772 TRANSACTION_START_MODE ����
                 ( isAllStmtEnd() == ID_FALSE ) )       // BUG-45772 Fetch Across Commit ����
            {
                mmcTrans::begin( mTrans,
                                 &mStatSQL,
                                 getSessionInfoFlagForTx(),
                                 this,
                                 &sIsDummyBegin );
            }
            else
            {
                /* Nothing to do */
            }
            
            setActivated( ID_FALSE ); // ������ �ʱ���·� ����.
        }
        else
        {
            // stored procedure needs the statement to be preserved.
            IDE_TEST( mmcTrans::rollbackForceDatabaseLink(
                          mTrans,
                          this,
                          SMI_DO_NOT_RELEASE_TRANSACTION )
                      != IDE_SUCCESS );
            
            mmcTrans::begin( mTrans,
                             &mStatSQL,
                             getSessionInfoFlagForTx(),
                             this,
                             &sIsDummyBegin );
            
            // SP ���ο��� commit�� ��� �ǵ��ư� �κ��� ���.
            // To Fix BUG-12512 : PSM ���۽� EXP SVP -> IMP SVP
            mmcTrans::reservePsmSvp(mTrans, ID_FALSE);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( InvalidSessionState );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_INVALID_SESSION_STATE ) );
    }
    IDE_EXCEPTION( StmtRemainError );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_OTHER_STATEMENT_REMAINS ) );
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::savepoint(const SChar *aSavePoint, idBool /*aInStoredProc*/)
{
    idBool      sIsDummyBegin = ID_FALSE;

    IDE_ASSERT(aSavePoint[0] != 0);

    if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT)
    {
        /* BUG-46785 Shard statement partial rollback */
        /* tx�� begin���� �ʾ����� begin�Ѵ�. */
        if ( getTransBegin() == ID_FALSE )
        {
            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             getSessionInfoFlagForTx(), 
                             this,
                             &sIsDummyBegin );
        }
        else
        {
            /* Nothing to do. */
        }

        // TASK-7244 Shard Global Procedure�� �������� ���� explicit savepoint�� implicit savepoint�� �����Ѵ�.
        if ( (SDU_SHARD_ENABLE == 1) &&
             (idlOS::strMatch( aSavePoint,
                               idlOS::strlen(aSavePoint),
                               SAVEPOINT_FOR_SHARD_GLOBAL_PROC_PARTIAL_ROLLBACK,
                               idlOS::strlen(SAVEPOINT_FOR_SHARD_GLOBAL_PROC_PARTIAL_ROLLBACK) )
              == 0) )
        {
            mmcTrans::reservePsmSvp(mTrans, ID_TRUE);
        }
        else
        {
            IDE_TEST(mmcTrans::savepoint(mTrans, this, aSavePoint) != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-46785 Shard statement partial rollback */
IDE_RC mmcSession::shardStmtPartialRollback( void )
{
    IDE_TEST( mmcTrans::shardStmtPartialRollback( mTrans, this ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// ���� �Ѱ��� Ʈ������� ���°� ACTIVE�̸� return FALSE�ؾ� ��.
IDE_RC mmcSession::setCommitMode(mmcCommitMode aCommitMode)
{
    idBool      sIsDummyBegin = ID_FALSE;
    /*
     * BUG-29634 : autocommit mode should be set 
     *             before the recovery process
     */
    if (mmm::getCurrentPhase() < MMM_STARTUP_META)
    {
        mInfo.mCommitMode = MMC_COMMITMODE_AUTOCOMMIT;
    }
    else
    {
        if ( (SDU_SHARD_ENABLE == 1) && (SDU_SHARD_ALLOW_AUTO_COMMIT == 0) )
        {
            /* AUTO_COMMIT�� 1�� �����Ǿ� �ִ� ��� ERROR �߻� */ 
            IDE_TEST_RAISE( getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT, InvalidSessionState ); 

            /* AUTO_COMMIT�� 1�� �����Ϸ��� ��� ERROR �߻� */ 
            IDE_TEST_RAISE( aCommitMode == MMC_COMMITMODE_AUTOCOMMIT, ERR_CANNOT_SET_AUTOCOMMIT ); 
        }

        if (getCommitMode() != aCommitMode) // ���� ���� �����ϸ� SUCCESS
        {
            //fix BUG-29749 Changing a commit mode of a session is  allowed only if  a state  of a session is SERVICE.
            IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE,InvalidSessionState);

            /* PROJ-1381 FAC : Notes
             * Altibase�� Autocommit On�� �� FAC�� �������� �ʴ´�.
             * Commit Mode�� �ٲٷ��� �ݵ�� �����ִ� Ŀ���� ��� �ݾƾ� �Ѵ�. */
            IDE_TEST_RAISE(isAllStmtEnd() != ID_TRUE, StmtRemainError);
            IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);

            // change a commit mode from none auto commit to autocommit.
            if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT)
            {
                IDE_TEST(mmcTrans::commit(mTrans, this) != IDE_SUCCESS);

                /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
                IDE_TEST( sdi::setCommitMode( & mQciSession,
                                              ID_TRUE,
                                              getGlobalTransactionLevel(),
                                              isGTx(),
                                              isGCTx() )
                          != IDE_SUCCESS );

                mInfo.mCommitMode = aCommitMode;
                /* PROJ-2701 transaction realloc for new transaction mode after alter commit mode */
                reallocTrans();
            }
            else
            {
                /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
                IDE_TEST( sdi::setCommitMode( & mQciSession,
                                              ID_FALSE,
                                              getGlobalTransactionLevel(),
                                              isGTx(),
                                              isGCTx() )
                          != IDE_SUCCESS );

                // change a commit mode from autocommit to non auto commit.
                initTransStartMode();

                // BUG-45772 Transaction Begin ���� Commit Mode�� �����ؾ� �Ѵ�.
                mInfo.mCommitMode = aCommitMode;
                /* PROJ-2701 transaction realloc for new transaction mode after alter commit mode */
                reallocTrans();

                if ( getTransLazyBegin() == ID_FALSE ) // BUG-45772 TRANSACTION_START_MODE ����
                {
                    mmcTrans::begin( mTrans, 
                                     &mStatSQL, 
                                     getSessionInfoFlagForTx(), 
                                     this,
                                     &sIsDummyBegin );
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    //fix BUG-29749 Changing a commit mode of a session is  allowed only if  a state  of a session is SERVICE.
    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    /* BUG-48247 */ 
    IDE_EXCEPTION(ERR_CANNOT_SET_AUTOCOMMIT);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_CANNOT_CHANGE_AUTOCOMMIT_IN_SHARD_ENV));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setTransactionalDDL( idBool aTransactionalDDL )
{
    if (mmm::getCurrentPhase() < MMM_STARTUP_SERVICE)
    {
        mInfo.mTransactionalDDL = ID_FALSE;
    }
    else
    {
        if ( getTransactionalDDL() != aTransactionalDDL )
        {
            IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE,InvalidSessionState);

            IDE_TEST_RAISE(isAllStmtEnd() != ID_TRUE, StmtRemainError);
            IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);

            mInfo.mTransactionalDDL = aTransactionalDDL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setGlobalDDL( idBool aGlobalDDL )
{
    if (mmm::getCurrentPhase() < MMM_STARTUP_SERVICE)
    {
        mInfo.mGlobalDDL = ID_FALSE;
    }
    else
    {
        if ( getGlobalDDL() != aGlobalDDL ) // ���� ���� �����ϸ� SUCCESS
        {
            IDE_TEST_RAISE( SDU_SHARD_ENABLE != 1, ERR_GLOBAL_DDL_ONLY_SHARD_ENABLE );

            IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE,InvalidSessionState);

            IDE_TEST_RAISE(isAllStmtEnd() != ID_TRUE, StmtRemainError);
            IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);

            IDE_TEST( sdi::setTransactionalDDLMode( this->getQciSession(), aGlobalDDL )
                      != IDE_SUCCESS );

            mInfo.mGlobalDDL = aGlobalDDL;
        }
    }

    return IDE_SUCCESS;
     IDE_EXCEPTION( ERR_GLOBAL_DDL_ONLY_SHARD_ENABLE );
     {
         IDE_SET( ideSetErrorCode( mmERR_ABORT_INTERNAL_SERVER_ERROR_ARG,
                                   "Global DDL can only be used among the sharding cluster nodes." ) );
     }
    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    //fix BUG-29749 Changing a commit mode of a session is  allowed only if  a state  of a session is SERVICE.
    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mmcSession::setStackSize(SInt aStackSize)
{
    IDE_TEST_RAISE(aStackSize < 1 || aStackSize > QCI_TEMPLATE_MAX_STACK_COUNT, StackSizeError);

    mInfo.mStackSize = aStackSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION(StackSizeError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVAILD_STACKCOUNT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-41511 supporting to similar DBMS_APPLICATION_INFO */
IDE_RC mmcSession::setClientAppInfo( SChar *aClientAppInfo, UInt aLength )
{
    IDE_TEST_RAISE( aLength > MMC_APPINFO_MAX_LEN, ERR_CLIENT_INFO_LENGTH );
    
    idlOS::strncpy( mInfo.mClientAppInfo, aClientAppInfo, aLength );
    mInfo.mClientAppInfo[aLength] = 0;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_CLIENT_INFO_LENGTH );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_VALUE_LENGTH_EXCEED, "CLIENT_INFO", MMC_APPINFO_MAX_LEN ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setModuleInfo( SChar *aModuleInfo, UInt aLength )
{
    IDE_TEST_RAISE( aLength > MMC_APPINFO_MAX_LEN, ERR_MODULE_LENGTH );
    
    idlOS::strncpy( mInfo.mModuleInfo, aModuleInfo, aLength );
    mInfo.mModuleInfo[aLength] = 0;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MODULE_LENGTH );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_VALUE_LENGTH_EXCEED, "MODULE", MMC_APPINFO_MAX_LEN ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setActionInfo( SChar *aActionInfo, UInt aLength )
{
    IDE_TEST_RAISE( aLength > MMC_APPINFO_MAX_LEN, ERR_ACTION_LENGTH );
    
    idlOS::strncpy( mInfo.mActionInfo, aActionInfo, aLength );
    mInfo.mActionInfo[aLength] = 0;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_ACTION_LENGTH );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_VALUE_LENGTH_EXCEED, "ACTION", MMC_APPINFO_MAX_LEN ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PORJ-2209 DBTIMEZONE */
IDE_RC mmcSession::setTimezoneString( SChar *aTimezoneString, UInt aLength )
{
    IDE_TEST_RAISE( aLength < 1 || aLength > MMC_TIMEZONE_MAX_LEN, ERR_TIMEZONE_LENGTH )

    idlOS::memcpy( mInfo.mTimezoneString, aTimezoneString, aLength );
    mInfo.mTimezoneString[aLength] = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TIMEZONE_LENGTH );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_MMC_TIMEZONE_LENGTH_EXCEED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setTimezoneSecond( SLong aTimezoneSecond )
{
    mInfo.mTimezoneSecond = aTimezoneSecond;

    return IDE_SUCCESS;
}

IDE_RC mmcSession::setDateFormat(SChar *aDateFormat, UInt aLength)
{
    IDE_TEST(aLength >= MMC_DATEFORMAT_MAX_LEN)

    idlOS::memcpy(mInfo.mDateFormat, aDateFormat, aLength);
    mInfo.mDateFormat[aLength] = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_DATE_FORMAT_LENGTH_EXCEED,
                                MMC_DATEFORMAT_MAX_LEN));
    }

    return IDE_FAILURE;
}

/**
 * PROJ-2208 getNlsTerritory
 *
 *  NLS_TERRITORY �̸��� ��ȯ�Ѵ�.
 */
const SChar * mmcSession::getNlsTerritory()
{
    return mtlTerritory::getNlsTerritoryName( mInfo.mNlsTerritory );
}

/**
 * PROJ-2208 getNlsISOCurrency
 *
 *  NLS_ISO_CURRENCY CODE ���� ��ȯ�Ѵ�.
 */
SChar * mmcSession::getNlsISOCurrency()
{
    return mInfo.mNlsISOCode;
}

/**
 * PROJ-2208 setNlsTerritory
 *
 *  �Է� ���� NLS_TERRITORY �� �����Ѵ�. �̶�
 *  NLS_ISO_CURRENCY, NLS_CURRENCY, NLS_NUMERIC_CHARACTERS �� �Բ� �ٲ��ش�.
 */
IDE_RC mmcSession::setNlsTerritory( SChar * aValue, UInt aLength )
{
    SInt    sNlsTerritory = -1;

    IDE_TEST_RAISE( aLength >= QC_MAX_NAME_LEN, ERR_NOT_SUPPORT_TERRITORY )

    IDE_TEST( mtlTerritory::searchNlsTerritory( aValue, &sNlsTerritory )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( ( sNlsTerritory < 0 ) || ( sNlsTerritory >= TERRITORY_MAX ),
                    ERR_NOT_SUPPORT_TERRITORY );

    mInfo.mNlsTerritory = sNlsTerritory;
    mInfo.mNlsISOCurrency = sNlsTerritory;

    IDE_TEST( mtlTerritory::setNlsISOCurrencyCode( sNlsTerritory, mInfo.mNlsISOCode )
              != IDE_SUCCESS );

    IDE_TEST( mtlTerritory::setNlsNumericChar( sNlsTerritory, mInfo.mNlsNumChar )
              != IDE_SUCCESS );

    IDE_TEST( mtlTerritory::setNlsCurrency( sNlsTerritory, mInfo.mNlsCurrency )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_TERRITORY )
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMI_NOT_IMPLEMENTED));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-2208 setNlsISOCurrency
 *
 *  aValue���� �ش��ϴ� Territory�� ã�� �̸� �����Ѵ�.
 */
IDE_RC mmcSession::setNlsISOCurrency( SChar * aValue, UInt aLength )
{
    SInt    sNlsISOCurrency = -1;

    IDE_TEST_RAISE( aLength >= QC_MAX_NAME_LEN, ERR_NOT_SUPPORT_TERRITORY )

    IDE_TEST( mtlTerritory::searchISOCurrency( aValue, &sNlsISOCurrency )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( ( sNlsISOCurrency < 0 ) ||
                    ( sNlsISOCurrency >= TERRITORY_MAX ),
                    ERR_NOT_SUPPORT_TERRITORY );

    mInfo.mNlsISOCurrency = sNlsISOCurrency;

    IDE_TEST( mtlTerritory::setNlsISOCurrencyCode( sNlsISOCurrency, mInfo.mNlsISOCode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_TERRITORY )
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMI_NOT_IMPLEMENTED));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-2208 setNlsCurrency
 *
 *  �ش��ϴ� Territory�� ã�Ƽ� NLS_CURRENCY���� �����Ѵ�.
 */
IDE_RC mmcSession::setNlsCurrency( SChar * aValue, UInt aLength )
{
    if ( aLength > MTL_TERRITORY_CURRENCY_LEN )
    {
        aLength = MTL_TERRITORY_CURRENCY_LEN;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( mtlTerritory::checkCurrencySymbol( aValue )
              != IDE_SUCCESS );

    idlOS::memcpy( mInfo.mNlsCurrency, aValue, aLength );
    mInfo.mNlsCurrency[aLength] = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * PROJ-2208 setNlsNumChar
 *
 *  �ش��ϴ� Territory�� ã�Ƽ� NLS_NUMERIC_CHARACTERS ���� �����Ѵ�.
 */
IDE_RC mmcSession::setNlsNumChar( SChar * aValue, UInt aLength )
{
    if ( aLength > MTL_TERRITORY_NUMERIC_CHAR_LEN )
    {
        aLength = MTL_TERRITORY_NUMERIC_CHAR_LEN;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( mtlTerritory::checkNlsNumericChar( aValue )
              != IDE_SUCCESS );

    idlOS::memcpy( mInfo.mNlsNumChar, aValue, aLength );
    mInfo.mNlsNumChar[aLength] = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC mmcSession::setReplicationMode(UInt aReplicationMode)
{
    idBool      sIsDummyBegin = ID_FALSE;

    IDE_TEST_RAISE(isAllStmtEnd() != ID_TRUE, StmtRemainError);
    IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);

    /*
     * ��� unset�� �� �Էµ� Flag ������ ReplicationMode�� Set PROJ-1541
     * aReplicationMode�� REPLICATED�� �Բ� Set�Ǿ �Էµ�
     */

    IDE_TEST_RAISE(aReplicationMode == SMI_TRANSACTION_REPL_NOT_SUPPORT, NotSupportReplModeError)

    if (getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT)
    {
        // autocommit ����� ���,
        mInfo.mReplicationMode = aReplicationMode;
    }
    else
    {
        // non autocommit ����� ���, commit�� �ٽ� begin
        IDE_TEST(mmcTrans::commit(mTrans, this) != IDE_SUCCESS);

        mInfo.mReplicationMode = aReplicationMode;

        if (getTransLazyBegin() == ID_FALSE) // BUG-45772 TRANSACTION_START_MODE ����
        {
            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             getSessionInfoFlagForTx(), 
                             this,
                             &sIsDummyBegin );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(NotSupportReplModeError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_NOT_SUPPORT_REPL_MODE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Transaction �Ӽ�(isolation level, transaction mode)�� �����ϴ� �Լ�.
 * 
 * - Non-autocommit mode���,
 *     1. ���� transaction�� �Ӽ��� ����
 *     2. transaction commit
 *     3. ������ transaction�� �Ӽ��� oring �Ͽ� ���ο� transaction begin
 *
 * - Autocommit mode���� transaction level���� ������ ���ǹ���.
 * - Non-autocommit mode���, ���� transaction�� ���� session info�� ���Ž�Ŵ.
 *
 * @param[in] aType      ������ transaction �Ӽ��� mask ��.
 *                       (SMI_TRANSACTION_MASK �Ǵ� SMI_ISOLATION_MASK)
 * @param[in] aValue     ������ transaction �Ӽ� ��.
 * @param[in] aIsSession Session level�� ���������� ����.
 *                       ID_FALSE��� transaction level�� ����.
 */
IDE_RC mmcSession::setTX(UInt aType, UInt aValue, idBool aIsSession)
{
    UInt sFlag;
    UInt sTxIsolationLevel;
    UInt sTxTransactionMode;
    idBool sIsDummyBegin = ID_FALSE;

    // BUG-47024
    IDE_TEST_RAISE( ( isShareableTrans() == ID_TRUE ) &&
                    ( aIsSession == ID_FALSE ), UnsupportedOnShardingError );

    if (getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT) /* BUG-39817 */
    {
        /* PROJ-1381 FAC : Holdable Fetch�� trancsaction�� ������ ���� �ʴ´�. */
        IDE_TEST_RAISE(isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);
        IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);

        // set transaction �������� ���� transaction��
        // transaction �Ӽ��� �ߺ� ������ �� ����.
        // ���� transaction commit ��, transaction ������ ȹ���ؾ� ��
        sTxIsolationLevel = getTxIsolationLevel(mTrans);
        sTxTransactionMode = getTxTransactionMode(mTrans);

        // ���� transaction commit
        IDE_TEST(mmcTrans::commit(mTrans, this) != IDE_SUCCESS);

        // ���ο� transaction begin�� �ʿ���,
        // session flag ������ ȹ����

        // BUG-17878 : session flag ���� ��,
        // ���� transaction �Ӽ����� ����
        sFlag = getSessionInfoFlagForTx();
        sFlag &= ~SMI_TRANSACTION_MASK;
        sFlag |= sTxTransactionMode;
        sFlag &= ~SMI_ISOLATION_MASK;
        sFlag |= sTxIsolationLevel;

        if ( aType == (UInt)(~SMI_TRANSACTION_MASK) )
        {
            // transaction�� transaction mode ����
            sFlag &= ~SMI_TRANSACTION_MASK;
            sFlag |= aValue;

            if ( aIsSession == ID_TRUE )
            {
                // alter session property �� ���,
                // session�� transaction mode�� ������
                mInfo.mTransactionMode = aValue;
            }
        }
        else
        {
            // transaction�� isolation level ����
            sFlag &= ~SMI_ISOLATION_MASK;
            sFlag |= aValue;

            if ( aIsSession == ID_TRUE )
            {
                // alter session property �� ���,
                // session�� isolation level�� ������
                mInfo.mIsolationLevel = aValue;
            }
        }

        if ( getTransLazyBegin() == ID_FALSE )
        {
            // ���ο� transaction begin
            mmcTrans::begin( mTrans, 
                             &mStatSQL, 
                             sFlag, 
                             this,
                             &sIsDummyBegin );
        }
    }
    else
    {
        /* cannot set, if auto-commit mode and transaction level */
        IDE_TEST_RAISE(aIsSession == ID_FALSE, AutocommitError);

        if (aType == (UInt)(~SMI_TRANSACTION_MASK))
        {
            /* alter session property �� ���,
               session�� transaction mode�� ������ */
            mInfo.mTransactionMode = aValue;
        }
        else
        {
            /* alter session property �� ���,
               session�� isolation level�� ������ */
            mInfo.mIsolationLevel = aValue;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(UnsupportedOnShardingError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMI_NOT_IMPLEMENTED));
    }
    IDE_EXCEPTION(AutocommitError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_CANT_SET_TRANSACTION_IN_AUTOCOMMIT_MODE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mmcSession::set(SChar *aName, SChar *aValue)
{
    mmcSessionSetList *aSetList;

    for (aSetList = gCmsSessionSetLists; aSetList->mName != NULL; aSetList++)
    {
        if (idlOS::strcmp(aSetList->mName, aName) == 0)
        {
            IDE_TEST(aSetList->mFunc(this, aValue) != IDE_SUCCESS);
            break;
        }
    }
    IDE_TEST_RAISE(aSetList->mName == NULL, NotApplicableError);

    return IDE_SUCCESS;

    IDE_EXCEPTION(NotApplicableError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setAger(mmcSession * /* aSession */, SChar *aValue)
{
    IDE_RC _smiSetAger(idBool aValue);

    if (idlOS::strcmp(aValue, "ENABLE") == 0)
    {
        IDE_TEST(_smiSetAger(ID_TRUE) != IDE_SUCCESS);
    }
    else if (idlOS::strcmp(aValue, "DISABLE") == 0)
    {
        IDE_TEST(_smiSetAger(ID_FALSE) != IDE_SUCCESS);
    }
    else
    {
        IDE_RAISE(NotApplicableError);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(NotApplicableError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::allocLobLocator(mmcLobLocator **aLobLocator,
                                   UInt            aStatementID,
                                   smLobLocator    aLocatorID)
{
    IDE_TEST(mmcLob::alloc(aLobLocator, aStatementID, aLocatorID) != IDE_SUCCESS);
    IDE_TEST(smuHash::insertNode(&mLobLocatorHash, &aLocatorID, *aLobLocator) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC mmcSession::freeLobLocator(smLobLocator aLocatorID, idBool *aFound)
{
    mmcLobLocator *sLobLocator = NULL;

    IDE_TEST(smuHash::findNode(&mLobLocatorHash,
                               &aLocatorID,
                               (void **)&sLobLocator) != IDE_SUCCESS);

    if (sLobLocator != NULL)
    {
        IDE_TEST(smuHash::deleteNode(&mLobLocatorHash,
                                     &aLocatorID,
                                     (void **)&sLobLocator) != IDE_SUCCESS);

        IDE_TEST(mmcLob::free(getStatSQL(), sLobLocator) != IDE_SUCCESS);

        *aFound = ID_TRUE;
    }
    else
    {
        *aFound = ID_FALSE;
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC mmcSession::findLobLocator(mmcLobLocator **aLobLocator, smLobLocator aLocatorID)
{
    *aLobLocator = NULL;

    IDE_TEST(smuHash::findNode(&mLobLocatorHash, &aLocatorID, (void **)aLobLocator) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC mmcSession::clearLobLocator()
{
    mmcLobLocator *sLobLocator;
    
    IDE_TEST(smuHash::open(&mLobLocatorHash) != IDE_SUCCESS);
    //fix BUG-21311
    if(mLobLocatorHash.mCurChain != NULL)
    {
        //not empty.
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mLobLocatorHash, (void **)&sLobLocator) != IDE_SUCCESS);

            if (sLobLocator != NULL)
            {
                mmcLob::free(getStatSQL(), sLobLocator);
            }
            else
            {
                break;
            }
        }
    }
    IDE_TEST(smuHash::close(&mLobLocatorHash) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    IDE_ASSERT(0);
    return IDE_FAILURE;
}

IDE_RC mmcSession::clearLobLocator(UInt aStatementID)
{
    mmcLobLocator *sLobLocator;

    IDE_TEST(smuHash::open(&mLobLocatorHash) != IDE_SUCCESS);

    while (1)
    {
        IDE_TEST(smuHash::cutNode(&mLobLocatorHash, (void **)&sLobLocator) != IDE_SUCCESS);

        if (sLobLocator != NULL)
        {
            if( sLobLocator->mStatementID == aStatementID )
            {
                mmcLob::free(getStatSQL(), sLobLocator);
            }
            else
            {
                IDE_TEST(smuHash::insertNode(&mLobLocatorHash,
                                             &sLobLocator->mLocatorID,
                                             sLobLocator) != IDE_SUCCESS);
            }
        }
        else
        {
            break;
        }
    }

    IDE_TEST(smuHash::close(&mLobLocatorHash) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    IDE_ASSERT(0);
    return IDE_FAILURE;
}

void mmcSession::beginQueueWait()
{
    //fix BUG-19321
    if (IDU_LIST_IS_EMPTY(getQueueListNode()) == ID_TRUE)
    {
        mQueueInfo->addSession(this);
    }
}

void mmcSession::endQueueWait()
{
    //fix BUG-24362 dequeue ��� ������ close�ɶ� , session�� queue ��������
    // ���ü��� ������ ����.
    //doExecute�� fail�Ǵ� ��� �ƹ����� check���� endQueueWait��
    //ȣ���ϰ� �־ ������ ���� �� �Ѵ�.
    if(mQueueInfo != NULL)
    {
        
        mQueueInfo->removeSession(this);
    }
    else
    {
        //nothing to do.
    }
}

IDE_RC mmcSession::bookEnqueue(UInt aTableID)
{
    mmqQueueInfo *sQueueInfo = NULL;

    IDE_TEST(smuHash::findNode(&mEnqueueHash, &aTableID, (void **)&sQueueInfo) != IDE_SUCCESS);

    if (sQueueInfo == NULL)
    {
        IDE_TEST(mmqManager::findQueue(aTableID, &sQueueInfo) != IDE_SUCCESS);

        IDE_TEST(smuHash::insertNode(&mEnqueueHash, &aTableID, sQueueInfo) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
//PROJ-1677 DEQUEUE  
IDE_RC mmcSession::bookDequeue(UInt aTableID)
{
    mmqQueueInfo *sQueueInfo = NULL;

    IDE_TEST(smuHash::findNode(&mDequeueHash4Rollback, &aTableID, (void **)&sQueueInfo) 
            != IDE_SUCCESS);

    if (sQueueInfo == NULL)
    {
        IDE_TEST(mmqManager::findQueue(aTableID, &sQueueInfo) != IDE_SUCCESS);

        IDE_TEST(smuHash::insertNode(&mDequeueHash4Rollback, &aTableID, sQueueInfo) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC mmcSession::flushEnqueue(smSCN* aCommitSCN)
{
    mmqQueueInfo *sQueueInfo;
    mmqQueueInfo *sDummyQueueInfo = NULL;
    UInt          sQueueTableID;
    /* fix BUG-31008, When commit the transaction which had enqueued and did partial rollbacks,
       abnormal exit has been occurred. */
    IDE_RC        sRC;
    
    IDE_TEST(smuHash::open(&mEnqueueHash) != IDE_SUCCESS);
    //fix BUG-21311
    if(mEnqueueHash.mCurChain  != NULL )
    {   
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mEnqueueHash, (void **)&sQueueInfo) != IDE_SUCCESS);

            if (sQueueInfo == NULL)
            {
                break;
            }

            sQueueInfo->wakeup(aCommitSCN);
            
            //PROJ-1677 DEQUEUE
            if(mPartialRollbackFlag == MMC_DID_PARTIAL_ROLLBACK)
            {
                sQueueTableID = sQueueInfo->getTableID();
                /* fix BUG-31008, When commit the transaction which had enqueued and did partial rollbacks,
                   abnormal exit has been occurred. */
                sRC = smuHash::findNode(&mDequeueHash4Rollback,
                                        &sQueueTableID,
                                        (void **)&sDummyQueueInfo);
                if(sRC == IDE_SUCCESS)
                {
                    /* fix BUG-31008, When commit the transaction which had enqueued and did partial rollbacks,
                       abnormal exit has been occurred. */
                    if(sDummyQueueInfo != NULL)
                    {
                        (void)smuHash::deleteNode(&mDequeueHash4Rollback,
                                                  &sQueueTableID,
                                                  (void **)&sDummyQueueInfo);
                    }
                    else
                    {
                        //nothing to do
                    }   
                }
                else
                {
                    //nothing to do
                }
            }//if 
        }//while
    }//if
    IDE_TEST(smuHash::close(&mEnqueueHash) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


ULong mmcSession::getSessionUpdateMaxLogSizeCallback( idvSQL* aStatistics )
{
    if( aStatistics != NULL )
    {
        if( aStatistics->mSess != NULL )
        {
            if( aStatistics->mSess->mSession != NULL )
            {
                return ((mmcSession*)(aStatistics->mSess->mSession))->getUpdateMaxLogSize();
            }
        }
    }

    return 0;
}
IDE_RC mmcSession::getSessionSqlText( idvSQL * aStatistics,
                                      UChar  * aStrBuffer,
                                      UInt     aStrBufferSize)
{
    mmcSession   * sSession;
    mmcStatement * sStatement;

    IDE_ERROR( aStrBufferSize > 1 );

    aStrBuffer[ 0 ] = '\0';

    if( aStatistics != NULL )
    {
        if( aStatistics->mSess != NULL )
        {
            if( aStatistics->mSess->mSession != NULL )
            {
                sSession   = ((mmcSession*)(aStatistics->mSess->mSession));
                if( sSession != NULL )
                {
                    sStatement =  sSession->getExecutingStatement();
                    if( sStatement != NULL )
                    {
                        /* ���� QueryString�� ���̰� StrBufferSize�� ���� ���,
                         * �������� Null(\0)�� �������� �ʴ� ������ �߻��� ��
                         * ���� */
                        idlOS::strncpy( (SChar*)aStrBuffer,
                                        (SChar*)sStatement->getQueryString(),
                                        aStrBufferSize - 1 );
                        aStrBuffer[ aStrBufferSize - 1 ] = '\0';
                    }
                }
            }
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


//PROJ-1677 DEQUEUE
IDE_RC mmcSession::flushDequeue(smSCN* aCommitSCN)
{
    mmqQueueInfo *sQueueInfo;

    IDE_TEST(smuHash::open(&mDequeueHash4Rollback) != IDE_SUCCESS);
    
    //fix BUG-21311
    if(mDequeueHash4Rollback.mCurChain != NULL)
    {   
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mDequeueHash4Rollback, (void **)&sQueueInfo) != IDE_SUCCESS);

            if (sQueueInfo == NULL)
            {
                break;
            }
            
            sQueueInfo->wakeup(aCommitSCN);
        }
    }
    IDE_TEST(smuHash::close(&mDequeueHash4Rollback) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

//PROJ-1677 DEQUEUE
IDE_RC mmcSession::flushDequeue()
{
    mmqQueueInfo *sQueueInfo;

    IDE_TEST(smuHash::open(&mDequeueHash4Rollback) != IDE_SUCCESS);
    //fix BUG-21311
    if(mDequeueHash4Rollback.mCurChain != NULL)
    {   
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mDequeueHash4Rollback, (void **)&sQueueInfo) != IDE_SUCCESS);

            if (sQueueInfo == NULL)
            {
                break;
            }
            //fix BUG-19320.
            sQueueInfo->wakeup4DeqRollback();
        }
    }
    IDE_TEST(smuHash::close(&mDequeueHash4Rollback) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

IDE_RC mmcSession::clearEnqueue()
{
    mmqQueueInfo *sQueueInfo;

    IDE_TEST(smuHash::open(&mEnqueueHash) != IDE_SUCCESS);
    if(mEnqueueHash.mCurChain != NULL)
    {   
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mEnqueueHash, (void **)&sQueueInfo) != IDE_SUCCESS);

            if (sQueueInfo == NULL)
            {
                break;
            }
        }
    }
    IDE_TEST(smuHash::close(&mEnqueueHash) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

//PROJ-1677
IDE_RC mmcSession::clearDequeue()
{
    mmqQueueInfo *sQueueInfo;

    IDE_TEST(smuHash::open(&mDequeueHash4Rollback) != IDE_SUCCESS);
    if(mDequeueHash4Rollback.mCurChain != NULL)
    {    
        while (1)
        {
            IDE_TEST(smuHash::cutNode(&mDequeueHash4Rollback,(void **)&sQueueInfo) != IDE_SUCCESS);

            if (sQueueInfo == NULL)
            {
                break;
            }
        }
    }
    
    IDE_TEST(smuHash::close(&mDequeueHash4Rollback) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


const mtlModule *mmcSession::getLanguageCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getLanguage();
}

SChar *mmcSession::getDateFormatCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getDateFormat();
}

// BUG-20767
SChar *mmcSession::getUserNameCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->loginID;
}

UInt mmcSession::getUserIDCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->userID;
}

void mmcSession::setUserIDCallback(void *aSession, UInt aUserID)
{
    ((mmcSession *)aSession)->getUserInfo()->userID = aUserID;
}

// BUG-19041
UInt mmcSession::getSessionIDCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getInfo()->mSessionID;
}

// PROJ-2002 Column Security
SChar *mmcSession::getSessionLoginIPCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->loginIP;
}

/* PROJ-1812 ROLE */
UInt *mmcSession::getRoleListCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->mRoleList;
}

scSpaceID mmcSession::getTableSpaceIDCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->tablespaceID;
}

scSpaceID mmcSession::getTempSpaceIDCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->tempTablespaceID;
}

idBool mmcSession::isSysdbaUserCallback(void *aSession)
{
    return ((mmcSession *)aSession)->isSysdba();
}

idBool mmcSession::isBigEndianClientCallback(void *aSession)
{
    return (((mmcSession *)aSession)->getClientHostByteOrder() == MMC_BYTEORDER_BIG_ENDIAN) ? ID_TRUE : ID_FALSE;
}

UInt mmcSession::getStackSizeCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getStackSize();
}

/* BUG-41511 supporting to similar DBMS_APPLICATION_INFO */
IDE_RC mmcSession::setClientAppInfoCallback(void *aSession, SChar *aClientAppInfo, UInt aLength)
{
    return ((mmcSession *)aSession)->setClientAppInfo( aClientAppInfo, aLength );
}

IDE_RC mmcSession::setModuleInfoCallback(void *aSession, SChar *aModlueInfo, UInt aLength)
{
    return ((mmcSession *)aSession)->setModuleInfo( aModlueInfo, aLength );
}

IDE_RC mmcSession::setActionInfoCallback(void *aSession, SChar *aActionInfo, UInt aLength)
{
    return ((mmcSession *)aSession)->setActionInfo( aActionInfo, aLength );
}

/* PROJ-2441 flashback */
UInt mmcSession::getRecyclebinEnableCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;
    
    return sSession->getRecyclebinEnable();
}

/* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
idBool mmcSession::getLockTableUntilNextDDLCallback( void * aSession )
{
    mmcSession * sSession = (mmcSession *)aSession;

    return sSession->getLockTableUntilNextDDL();
}

void mmcSession::setLockTableUntilNextDDLCallback( void * aSession, idBool aValue )
{
    mmcSession * sSession = (mmcSession *)aSession;

    sSession->setLockTableUntilNextDDL( aValue );
}

UInt mmcSession::getTableIDOfLockTableUntilNextDDLCallback( void * aSession )
{
    mmcSession * sSession = (mmcSession *)aSession;

    return sSession->getTableIDOfLockTableUntilNextDDL();
}

void mmcSession::setTableIDOfLockTableUntilNextDDLCallback( void * aSession, UInt aValue )
{
    mmcSession * sSession = (mmcSession *)aSession;

    sSession->setTableIDOfLockTableUntilNextDDL( aValue );
}

// BUG-41398 use old sort
UInt mmcSession::getUseOldSortCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;
    
    return sSession->getUseOldSort();
}

/* PROJ-2209 DBTIMEZONE */
SLong mmcSession::getTimezoneSecondCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getTimezoneSecond();
}

SChar *mmcSession::getTimezoneStringCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getTimezoneString();
}

UInt mmcSession::getNormalFormMaximumCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getNormalFormMaximum();
}

// BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
UInt mmcSession::getOptimizerDefaultTempTbsTypeCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getOptimizerDefaultTempTbsType();    
}

UInt mmcSession::getOptimizerModeCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getOptimizerMode();
}

UInt mmcSession::getSelectHeaderDisplayCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getHeaderDisplayMode();
}

// PROJ-1579 NCHAR
UInt mmcSession::getNlsNcharLiteralReplaceCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getNlsNcharLiteralReplace();
}

//BUG-21122
UInt mmcSession::getAutoRemoteExecCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getAutoRemoteExec();
}

IDE_RC mmcSession::savepointCallback(void *aSession, const SChar *aSavepoint, idBool aInStoredProc)
{
    return ((mmcSession *)aSession)->savepoint(aSavepoint, aInStoredProc);
}

IDE_RC mmcSession::commitCallback(void *aSession, idBool aInStoredProc)
{
    return ((mmcSession *)aSession)->commit(aInStoredProc);
}

IDE_RC mmcSession::rollbackCallback(void *aSession, const SChar *aSavepoint, idBool aInStoredProc)
{
    return ((mmcSession *)aSession)->rollback(aSavepoint, aInStoredProc);
}

IDE_RC mmcSession::setTXCallback(void *aSession, UInt aType, UInt aValue, idBool aIsSession)
{
    return ((mmcSession *)aSession)->setTX(aType, aValue, aIsSession);
}

IDE_RC mmcSession::setStackSizeCallback(void *aSession, SInt aStackSize)
{
    return ((mmcSession *)aSession)->setStackSize(aStackSize);
}

IDE_RC mmcSession::setCallback(void *aSession, SChar *aName, SChar *aValue)
{
    return ((mmcSession *)aSession)->set(aName, aValue);
}

IDE_RC mmcSession::setReplicationModeCallback(void *aSession, UInt aReplicationMode)
{
    return ((mmcSession *)aSession)->setReplicationMode(aReplicationMode);
}

IDE_RC mmcSession::setPropertyCallback(void            *aSession,
                                       SChar           *aPropName,
                                       UInt             aPropNameLen,
                                       SChar           *aPropValue,
                                       UInt             aPropValueLen )
{
    mmcSession * sSession = (mmcSession *)aSession;
    SChar        sTimezoneString[MTC_TIMEZONE_NAME_LEN + 1] = {0,};
    SLong        sTimezoneSecond    = 0;    
    UShort       sPropertyId        = CMP_DB_PROPERTY_MAX;
    UInt         sPropertyAttribute = 0;
    idBool       sIsSystem          = ID_FALSE;
    
    /* BUG-18623: Alter Session set commit_write_wait_mode����� ������ �Ѿ ������
     *            �����˴ϴ�. system property�� �����Ǿ����� session�� �ִ� ���� �ٲܶ���
     *            system property���� validate�� ���� �ʽ��ϴ�. ������ �������� set�ϴ���
     *            set�ǰ� �־����ϴ�. ��� session property�� ������ ������ ������ �־����ϴ�.
     *            �̸� �����ϱ� ���ؼ� set�ϱ����� idp::validate�Լ��� ȣ���ϵ��� �Ͽ����ϴ�.*/
    if (idlOS::strMatch("QUERY_TIMEOUT", idlOS::strlen("QUERY_TIMEOUT"),
                        aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "QUERY_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "QUERY_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setQueryTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_QUERY_TIMEOUT;
    }
    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    else if (idlOS::strMatch("DDL_TIMEOUT", idlOS::strlen("DDL_TIMEOUT"),
                        aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "DDL_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "DDL_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setDdlTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_DDL_TIMEOUT;
    }
    else if (idlOS::strMatch("FETCH_TIMEOUT", idlOS::strlen("FETCH_TIMEOUT"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "FETCH_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "FETCH_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setFetchTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_FETCH_TIMEOUT;
    }
    else if (idlOS::strMatch("UTRANS_TIMEOUT", idlOS::strlen("UTRANS_TIMEOUT"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "UTRANS_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "UTRANS_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setUTransTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_UTRANS_TIMEOUT;
    }
    else if (idlOS::strMatch("IDLE_TIMEOUT", idlOS::strlen("IDLE_TIMEOUT"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "IDLE_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "IDLE_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setIdleTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_IDLE_TIMEOUT;        
    }
    else if (idlOS::strMatch("OPTIMIZER_MODE", idlOS::strlen("OPTIMIZER_MODE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "OPTIMIZER_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "OPTIMIZER_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerMode(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_OPTIMIZER_MODE;
    }
    else if (idlOS::strMatch("SELECT_HEADER_DISPLAY", idlOS::strlen("SELECT_HEADER_DISPLAY"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "SELECT_HEADER_DISPLAY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "SELECT_HEADER_DISPLAY", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setHeaderDisplayMode(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        // CHECK
        sPropertyId = CMP_DB_PROPERTY_HEADER_DISPLAY_MODE;
    }
    else if (idlOS::strMatch("NORMALFORM_MAXIMUM", idlOS::strlen("NORMALFORM_MAXIMUM"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "NORMALFORM_MAXIMUM",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NORMALFORM_MAXIMUM", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setNormalFormMaximum(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_NORMALFORM_MAXIMUM;
    }
    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    else if (idlOS::strMatch("__OPTIMIZER_DEFAULT_TEMP_TBS_TYPE", idlOS::strlen("__OPTIMIZER_DEFAULT_TEMP_TBS_TYPE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "__OPTIMIZER_DEFAULT_TEMP_TBS_TYPE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "__OPTIMIZER_DEFAULT_TEMP_TBS_TYPE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerDefaultTempTbsType(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY___OPTIMIZER_DEFAULT_TEMP_TBS_TYPE;
    }
    else if (idlOS::strMatch("COMMIT_WRITE_WAIT_MODE",
                             idlOS::strlen("COMMIT_WRITE_WAIT_MODE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "COMMIT_WRITE_WAIT_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "COMMIT_WRITE_WAIT_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setCommitWriteWaitMode(
            (idBool)(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen)));

        // CHECK
        sPropertyId = CMP_DB_PROPERTY_COMMIT_WRITE_WAIT_MODE;
    }
    // PROJ-1583 large geometry
    else if (idlOS::strMatch("ST_OBJECT_BUFFER_SIZE",
                             idlOS::strlen("ST_OBJECT_BUFFER_SIZE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "ST_OBJECT_BUFFER_SIZE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "ST_OBJECT_BUFFER_SIZE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setSTObjBufSize(
            (idBool)(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen)));
        
        sPropertyId = CMP_DB_PROPERTY_ST_OBJECT_BUFFER_SIZE;
    }
    else if (idlOS::strMatch("TRX_UPDATE_MAX_LOGSIZE", idlOS::strlen("TRX_UPDATE_MAX_LOGSIZE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "TRX_UPDATE_MAX_LOGSIZE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TRX_UPDATE_MAX_LOGSIZE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setUpdateMaxLogSize(idlOS::strToULong((UChar *)aPropValue, aPropValueLen));

        // CHECK
        sPropertyId = CMP_DB_PROPERTY_TRX_UPDATE_MAX_LOGSIZE;
    }
    else if (idlOS::strMatch("PARALLEL_DML_MODE",
                             idlOS::strlen("PARALLEL_DML_MODE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "PARALLEL_DML_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "PARALLEL_DML_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( 
            sSession->setParallelDmlMode(
                (idBool)(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen)))
            != IDE_SUCCESS );

        // CHECK
        sPropertyId = CMP_DB_PROPERTY_PARALLEL_DML_MODE;
    }
    // PROJ-1579 NCHAR
    else if( idlOS::strMatch("NLS_NCHAR_CONV_EXCP", 
                             idlOS::strlen("NLS_NCHAR_CONV_EXCP"),
                             aPropName, aPropNameLen) == 0 )
    {
        IDE_TEST( idp::validate( "NLS_NCHAR_CONV_EXCP",
                                 aPropValue,
                                 sIsSystem ) 
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NLS_NCHAR_CONV_EXCP", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setNlsNcharConvExcp(idlOS::strToUInt((UChar *)aPropValue, 
                                      aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_NLS_NCHAR_CONV_EXCP;
    }
    //BUG-21122
    else if (idlOS::strMatch("AUTO_REMOTE_EXEC", idlOS::strlen("AUTO_REMOTE_EXEC"),
                        aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "AUTO_REMOTE_EXEC",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "AUTO_REMOTE_EXEC", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setAutoRemoteExec(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_AUTO_REMOTE_EXEC;
    }
    /* BUG-31144 */
    else if (idlOS::strMatch("MAX_STATEMENTS_PER_SESSION", idlOS::strlen("MAX_STATEMENTS_PER_SESSION"),
                        aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "MAX_STATEMENTS_PER_SESSION",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "MAX_STATEMENTS_PER_SESSION", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST_RAISE( idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) < sSession->getNumberOfStatementsInSession(), StatementNumberExceedsInputValue);
        sSession->setMaxStatementsPerSession(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_MAX_STATEMENTS_PER_SESSION;
    }
    else if (idlOS::strMatch("TRCLOG_DETAIL_PREDICATE", idlOS::strlen("TRCLOG_DETAIL_PREDICATE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "TRCLOG_DETAIL_PREDICATE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TRCLOG_DETAIL_PREDICATE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setTrclogDetailPredicate(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_TRCLOG_DETAIL_PREDICATE;
    }
    else if (idlOS::strMatch("OPTIMIZER_DISK_INDEX_COST_ADJ", idlOS::strlen("OPTIMIZER_DISK_INDEX_COST_ADJ"),
                        aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "OPTIMIZER_DISK_INDEX_COST_ADJ",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "OPTIMIZER_DISK_INDEX_COST_ADJ", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerDiskIndexCostAdj((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_OPTIMIZER_DISK_INDEX_COST_ADJ;
    }
    else if (idlOS::strMatch("OPTIMIZER_MEMORY_INDEX_COST_ADJ", idlOS::strlen("OPTIMIZER_MEMORY_INDEX_COST_ADJ"),
                        aPropName, aPropNameLen) == 0)
    {
        // BUG-43736
        IDE_TEST( idp::validate( "OPTIMIZER_MEMORY_INDEX_COST_ADJ",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "OPTIMIZER_MEMORY_INDEX_COST_ADJ", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerMemoryIndexCostAdj((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_OPTIMIZER_MEMORY_INDEX_COST_ADJ;
    }
    else if (idlOS::strMatch("NLS_TERRITORY", idlOS::strlen("NLS_TERRITORY"),
                             aPropName, aPropNameLen ) == 0)
    {
        IDE_TEST( idp::validate( "NLS_TERRITORY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NLS_TERRITORY", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( sSession->setNlsTerritory((SChar *)aPropValue, aPropValueLen )
                  != IDE_SUCCESS );
        
        sPropertyId = CMP_DB_PROPERTY_NLS_TERRITORY;
    }
    else if (idlOS::strMatch("NLS_ISO_CURRENCY", idlOS::strlen("NLS_ISO_CURRENCY"),
                             aPropName, aPropNameLen ) == 0)
    {
        IDE_TEST( idp::validate( "NLS_ISO_CURRENCY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NLS_ISO_CURRENCY", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( sSession->setNlsISOCurrency((SChar *)aPropValue, aPropValueLen )
                  != IDE_SUCCESS );
        
        sPropertyId = CMP_DB_PROPERTY_NLS_ISO_CURRENCY;
    }
    else if (idlOS::strMatch("NLS_CURRENCY", idlOS::strlen("NLS_CURRENCY"),
                             aPropName, aPropNameLen ) == 0)
    {
        // validate check
        IDE_TEST( idp::validate( "NLS_CURRENCY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NLS_CURRENCY", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( sSession->setNlsCurrency((SChar *)aPropValue, aPropValueLen )
                  != IDE_SUCCESS );
        
        sPropertyId = CMP_DB_PROPERTY_NLS_CURRENCY;        
    }
    else if (idlOS::strMatch("NLS_NUMERIC_CHARACTERS", idlOS::strlen("NLS_NUMERIC_CHARACTERS"),
                             aPropName, aPropNameLen ) == 0)
    {
        IDE_TEST( idp::validate( "NLS_NUMERIC_CHARACTERS",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "NLS_NUMERIC_CHARACTERS", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( sSession->setNlsNumChar((SChar *)aPropValue, aPropValueLen )
                  != IDE_SUCCESS );
        
        sPropertyId = CMP_DB_PROPERTY_NLS_NUMERIC_CHARACTERS;         
    }
    /* PROJ-2209 DBTIMEZONE */
    else if ( idlOS::strMatch( "TIME_ZONE", idlOS::strlen("TIME_ZONE"),
                        aPropName, aPropNameLen ) == 0 )
    {
        // SYSTEM(READABLE), SESSION(WRITEABLE) PROPERTY�� �Ӽ��� �ٸ�
        IDE_TEST( idp::validate( "TIME_ZONE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TIME_ZONE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( mtz::getTimezoneSecondAndString( aPropValue,
                                                   &sTimezoneSecond,
                                                   sTimezoneString )
                  != IDE_SUCCESS );
        sSession->setTimezoneSecond( sTimezoneSecond );
        sSession->setTimezoneString( sTimezoneString, idlOS::strlen(sTimezoneString) );

        sPropertyId = CMP_DB_PROPERTY_TIME_ZONE;
    }
    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    else if (idlOS::strMatch("LOB_CACHE_THRESHOLD", idlOS::strlen("LOB_CACHE_THRESHOLD"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "LOB_CACHE_THRESHOLD",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "LOB_CACHE_THRESHOLD", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setLobCacheThreshold((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_LOB_CACHE_THRESHOLD;
    }
    /* PROJ-1090 Function-based Index */
    else if (idlOS::strMatch("QUERY_REWRITE_ENABLE", idlOS::strlen("QUERY_REWRITE_ENABLE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "QUERY_REWRITE_ENABLE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "QUERY_REWRITE_ENABLE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setQueryRewriteEnable((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_QUERY_REWRITE_ENABLE;         
    }
    /* BUG-47257 */
    else if ( ( idlOS::strMatch( /* deprecated */"DBLINK_GLOBAL_TRANSACTION_LEVEL", 
                                 idlOS::strlen( "DBLINK_GLOBAL_TRANSACTION_LEVEL" ),
                                 aPropName, 
                                 aPropNameLen ) == 0 ) ||
              ( idlOS::strMatch( "GLOBAL_TRANSACTION_LEVEL", 
                                 idlOS::strlen( "GLOBAL_TRANSACTION_LEVEL" ),
                                 aPropName, 
                                 aPropNameLen ) == 0 ) )
    {
        // validate check
        IDE_TEST( idp::validate( "GLOBAL_TRANSACTION_LEVEL",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "GLOBAL_TRANSACTION_LEVEL", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
        IDE_TEST( sSession->setGlobalTransactionLevel(
                                 idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) )
                  != IDE_SUCCESS );

        sPropertyId = CMP_DB_PROPERTY_GLOBAL_TRANSACTION_LEVEL;                 
    }
    else if ( idlOS::strMatch( "DBLINK_REMOTE_STATEMENT_AUTOCOMMIT",
                               idlOS::strlen("DBLINK_REMOTE_STATEMENT_AUTOCOMMIT"),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "DBLINK_REMOTE_STATEMENT_AUTOCOMMIT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "DBLINK_REMOTE_STATEMENT_AUTOCOMMIT", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        IDE_TEST( dkiSessionSetRemoteStatementAutoCommit(
                                  &(sSession->mDatabaseLinkSession),
                                  idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) )
                  != IDE_SUCCESS );
        
        sSession->setDblinkRemoteStatementAutoCommit(
                                  idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) );
        
        sPropertyId = CMP_DB_PROPERTY_DBLINK_REMOTE_STATEMENT_AUTOCOMMIT;                         
    }
    /* PROJ-2441 flashback */
    else if ( idlOS::strMatch( "RECYCLEBIN_ENABLE", idlOS::strlen( "RECYCLEBIN_ENABLE" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "RECYCLEBIN_ENABLE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "RECYCLEBIN_ENABLE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setRecyclebinEnable( (SInt)idlOS::strToUInt( (UChar *)aPropValue,
                                                               aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_RECYCLEBIN_ENABLE;                         
    }
    // BUG-41398 use old sort
    else if ( idlOS::strMatch( "__USE_OLD_SORT", idlOS::strlen( "__USE_OLD_SORT" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "__USE_OLD_SORT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "__USE_OLD_SORT", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setUseOldSort( (SInt)idlOS::strToUInt( (UChar *)aPropValue,
                                                         aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY___USE_OLD_SORT;
    }
    // BUG-41944
    else if ( idlOS::strMatch( "ARITHMETIC_OPERATION_MODE",
                               idlOS::strlen( "ARITHMETIC_OPERATION_MODE" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "ARITHMETIC_OPERATION_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "ARITHMETIC_OPERATION_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setArithmeticOpMode( (SInt)idlOS::strToUInt( (UChar *)aPropValue,
                                                               aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_ARITHMETIC_OPERATION_MODE;        
    }
    else if (idlOS::strMatch("RESULT_CACHE_ENABLE", idlOS::strlen("RESULT_CACHE_ENABLE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "RESULT_CACHE_ENABLE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "RESULT_CACHE_ENABLE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setResultCacheEnable((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_RESULT_CACHE_ENABLE;                
    }
    else if (idlOS::strMatch("TOP_RESULT_CACHE_MODE", idlOS::strlen("TOP_RESULT_CACHE_MODE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "TOP_RESULT_CACHE_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TOP_RESULT_CACHE_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setTopResultCacheMode((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_TOP_RESULT_CACHE_MODE; 
    }
    else if (idlOS::strMatch("OPTIMIZER_AUTO_STATS", idlOS::strlen("OPTIMIZER_AUTO_STATS"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "OPTIMIZER_AUTO_STATS",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "OPTIMIZER_AUTO_STATS", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerAutoStats((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_OPTIMIZER_AUTO_STATS;        
    }
    else if (idlOS::strMatch("__OPTIMIZER_TRANSITIVITY_OLD_RULE", idlOS::strlen("__OPTIMIZER_TRANSITIVITY_OLD_RULE"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "__OPTIMIZER_TRANSITIVITY_OLD_RULE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "__OPTIMIZER_TRANSITIVITY_OLD_RULE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerTransitivityOldRule((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY___OPTIMIZER_TRANSITIVITY_OLD_RULE;        
    }
    else if (idlOS::strMatch("OPTIMIZER_PERFORMANCE_VIEW", idlOS::strlen("OPTIMIZER_PERFORMANCE_VIEW"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "OPTIMIZER_PERFORMANCE_VIEW",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "OPTIMIZER_PERFORMANCE_VIEW", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setOptimizerPerformanceView((SInt)idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));
        
        sPropertyId = CMP_DB_PROPERTY_OPTIMIZER_PERFORMANCE_VIEW;        
    }
    else if ( idlOS::strMatch( "REPLICATION_DDL_SYNC", idlOS::strlen( "REPLICATION_DDL_SYNC" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "REPLICATION_DDL_SYNC",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "REPLICATION_DDL_SYNC", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setReplicationDDLSync( (UInt)idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_REPLICATION_DDL_SYNC;        
    }
    else if ( idlOS::strMatch( "REPLICATION_DDL_SYNC_TIMEOUT",
                               idlOS::strlen( "REPLICATION_DDL_SYNC_TIMEOUT" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "REPLICATION_DDL_SYNC_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "REPLICATION_DDL_SYNC_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setReplicationDDLSyncTimeout( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_REPLICATION_DDL_SYNC_TIMEOUT;        
    }
    else if ( idlOS::strMatch( "__PRINT_OUT_ENABLE", idlOS::strlen( "__PRINT_OUT_ENABLE" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "__PRINT_OUT_ENABLE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "__PRINT_OUT_ENABLE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setPrintOutEnable( (SInt)idlOS::strToUInt( (UChar *)aPropValue,
                                                             aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY___PRINT_OUT_ENABLE;        
    }
    else if ( idlOS::strMatch( "TRCLOG_DETAIL_SHARD", idlOS::strlen( "TRCLOG_DETAIL_SHARD" ),
                              aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "TRCLOG_DETAIL_SHARD",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TRCLOG_DETAIL_SHARD", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setTrclogDetailShard( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_TRCLOG_DETAIL_SHARD;        
    }
    /* PROJ-2632 */
    else if ( idlOS::strMatch( "SERIAL_EXECUTE_MODE", idlOS::strlen( "SERIAL_EXECUTE_MODE" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "SERIAL_EXECUTE_MODE",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "SERIAL_EXECUTE_MODE", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setSerialExecuteMode( (SInt)idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_SERIAL_EXECUTE_MODE;
    }
    else if ( idlOS::strMatch( "TRCLOG_DETAIL_INFORMATION",
                               idlOS::strlen( "TRCLOG_DETAIL_INFORMATION" ),
                              aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "TRCLOG_DETAIL_INFORMATION",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "TRCLOG_DETAIL_INFORMATION", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setTrcLogDetailInformation( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY_TRCLOG_DETAIL_INFORMATION;        
    }
    else if ( idlOS::strMatch( "__REDUCE_PARTITION_PREPARE_MEMORY",
                               idlOS::strlen( "__REDUCE_PARTITION_PREPARE_MEMORY" ),
                              aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "__REDUCE_PARTITION_PREPARE_MEMORY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "__REDUCE_PARTITION_PREPARE_MEMORY", &sPropertyAttribute )
                  != IDE_SUCCESS );
        
        sSession->setReducePartPrepareMemory( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
        
        sPropertyId = CMP_DB_PROPERTY___REDUCE_PARTITION_PREPARE_MEMORY;
    }
    else if (( idlOS::strMatch( "DATE_FORMAT", idlOS::strlen( "DATE_FORMAT" ),
                                aPropName, aPropNameLen ) == 0 ) ||
             ( idlOS::strMatch( "DEFAULT_DATE_FORMAT", idlOS::strlen( "DEFAULT_DATE_FORMAT" ),
                                aPropName, aPropNameLen ) == 0 ))
    {
        // 1. SYSTEM(READABLE), SESSION(WRITEABLE) PROPERTY�� �Ӽ��� �ٸ�
        // 2. shard prefix ���� ��� �ش� �κ� ������. ex> NODE[META]
        //    isql> ALTER SESSION SET DATE_FORMAT ; �� isql���� SQLSetConnectAttr()�� �����.
        IDE_TEST( idp::validate( "DEFAULT_DATE_FORMAT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );
        
        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "DEFAULT_DATE_FORMAT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        IDE_TEST( sSession->setDateFormat((SChar *)aPropValue, aPropValueLen )
                  != IDE_SUCCESS );
        
        sPropertyId = CMP_DB_PROPERTY_DATE_FORMAT;         
    }    
    else if ( idlOS::strMatch( "TRANSACTIONAL_DDL", idlOS::strlen( "TRANSACTIONAL_DDL" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        /* PROJ-2735 DDL Transaction */
        IDE_TEST_RAISE( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) > 1, NotUpdatableProperty );

        IDE_TEST( sSession->setTransactionalDDL( (idBool)(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) ) ) 
                  != IDE_SUCCESS );

        sPropertyId = CMP_DB_PROPERTY_TRANSACTIONAL_DDL;
    }
    else if ( idlOS::strMatch( "SHARD_INTERNAL_LOCAL_OPERATION", idlOS::strlen( "SHARD_INTERNAL_LOCAL_OPERATION" ),
                              aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST(sSession->setShardInternalLocalOperation( (sdiInternalOperation)idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) ) != IDE_SUCCESS);
    }
    else if ( idlOS::strMatch( "INVOKE_USER", idlOS::strlen( "INVOKE_USER" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        // BUG-47862
        // Internal use only.
        IDE_TEST_RAISE( sSession->getUserInfo()->invokeUserPropertyEnable == ID_FALSE,
                        NotUpdatableProperty);

        sPropertyId = CMP_DB_PROPERTY_INVOKE_USER;
    }
    else if ( idlOS::strMatch( "GLOBAL_DDL", idlOS::strlen( "GLOBAL_DDL" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        /* PROJ-2736 Global DDL */
        IDE_TEST_RAISE( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) > 1, NotUpdatableProperty );

        IDE_TEST( sSession->setGlobalDDL( (idBool)(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) ) )
                  != IDE_SUCCESS );

        sPropertyId = CMP_DB_PROPERTY_GLOBAL_DDL;
    }
    else if (idlOS::strMatch("SHARD_STATEMENT_RETRY", idlOS::strlen("SHARD_STATEMENT_RETRY"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "SHARD_STATEMENT_RETRY",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        // PROJ-2727
        IDE_TEST( idp::getPropertyAttribute( "SHARD_STATEMENT_RETRY", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setShardStatementRetry(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        sPropertyId = CMP_DB_PROPERTY_SHARD_STATEMENT_RETRY;
    }
    else if (idlOS::strMatch("INDOUBT_FETCH_TIMEOUT", idlOS::strlen("INDOUBT_FETCH_TIMEOUT"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "INDOUBT_FETCH_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        IDE_TEST( idp::getPropertyAttribute( "INDOUBT_FETCH_TIMEOUT", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setIndoubtFetchTimeout(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        /* BUG-48250 : TX�� �ݿ� */
        if ( ( sSession->getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT ) && 
             ( mmcTrans::getSmiTrans( sSession->getTransPtr() ) != NULL ) )

        {
            mmcTrans::getSmiTrans( sSession->getTransPtr() )
                ->setIndoubtFetchTimeout( idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) );
        }

        sPropertyId = CMP_DB_PROPERTY_INDOUBT_FETCH_TIMEOUT;
    }
    else if (idlOS::strMatch("INDOUBT_FETCH_METHOD", idlOS::strlen("INDOUBT_FETCH_METHOD"),
                             aPropName, aPropNameLen) == 0)
    {
        IDE_TEST( idp::validate( "INDOUBT_FETCH_METHOD",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        IDE_TEST( idp::getPropertyAttribute( "INDOUBT_FETCH_METHOD", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setIndoubtFetchMethod(idlOS::strToUInt((UChar *)aPropValue, aPropValueLen));

        /* BUG-48250 : TX�� �ݿ� */
        if ( ( sSession->getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT ) && 
             ( mmcTrans::getSmiTrans( sSession->getTransPtr() ) != NULL ) )
        {
            mmcTrans::getSmiTrans( sSession->getTransPtr() )
                ->setIndoubtFetchMethod( idlOS::strToUInt((UChar *)aPropValue, aPropValueLen) );
        }

        sPropertyId = CMP_DB_PROPERTY_INDOUBT_FETCH_METHOD;
    }
    else if ( idlOS::strMatch( "__OPTIMIZER_PLAN_HASH_OR_SORT_METHOD", idlOS::strlen( "__OPTIMIZER_PLAN_HASH_OR_SORT_METHOD" ),
                              aPropName, aPropNameLen ) == 0 )
    {   /* BUG-48132 */
        IDE_TEST( idp::validate( "__OPTIMIZER_PLAN_HASH_OR_SORT_METHOD",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        IDE_TEST( idp::getPropertyAttribute( "__OPTIMIZER_PLAN_HASH_OR_SORT_METHOD", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setPlanHashOrSortMethod( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );

        sPropertyId = CMP_DB_PROPERTY___OPTIMIZER_PLAN_HASH_OR_SORT_METHOD;
    }
    else if ( idlOS::strMatch( "__OPTIMIZER_BUCKET_COUNT_MAX", idlOS::strlen( "__OPTIMIZER_BUCKET_COUNT_MAX" ),
                              aPropName, aPropNameLen ) == 0 )
    {   /* BUG-48161 */
        IDE_TEST( idp::validate( "__OPTIMIZER_BUCKET_COUNT_MAX",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        IDE_TEST( idp::getPropertyAttribute( "__OPTIMIZER_BUCKET_COUNT_MAX", &sPropertyAttribute )
                  != IDE_SUCCESS );

        sSession->setBucketCountMax( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );

        sPropertyId = CMP_DB_PROPERTY___OPTIMIZER_BUCKET_COUNT_MAX;
    }
    else if ( idlOS::strMatch( "SHARD_DDL_LOCK_TIMEOUT", idlOS::strlen( "SHARD_DDL_LOCK_TIMEOUT" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "SHARD_DDL_LOCK_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        sSession->setShardDDLLockTimeout( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
    }
    else if ( idlOS::strMatch( "SHARD_DDL_LOCK_TRY_COUNT", idlOS::strlen( "SHARD_DDL_LOCK_TRY_COUNT" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "SHARD_DDL_LOCK_TRY_COUNT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        sSession->setShardDDLLockTryCount( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) );
    }
    else if ( idlOS::strMatch( "DDL_LOCK_TIMEOUT", idlOS::strlen( "DDL_LOCK_TIMEOUT" ),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate( "DDL_LOCK_TIMEOUT",
                                 aPropValue,
                                 sIsSystem )
                  != IDE_SUCCESS );

        sSession->setDDLLockTimeout( idlOS::strToInt( (UChar *)aPropValue, aPropValueLen ) );
        sPropertyId = CMP_DB_PROPERTY_DDL_LOCK_TIMEOUT;
    }
    else if ( idlOS::strMatch( "__OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION", idlOS::strlen( "__OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION"),
                               aPropName, aPropNameLen ) == 0 )
    {
        IDE_TEST( idp::validate("__OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION",
                                aPropValue,
                                sIsSystem )
                  != IDE_SUCCESS );

        sSession->setEliminateCommonSubexpression( idlOS::strToUInt( (UChar *)aPropValue, aPropValueLen ) ); 
        sPropertyId = CMP_DB_PROPERTY___OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION;
    }
    else
    {
        IDE_RAISE(NotUpdatableProperty);
    }


    if ( sPropertyId != CMP_DB_PROPERTY_MAX )
    {
        // PROJ-2727
        sSession->setPropertyAttrbute( sPropertyAttribute );
        IDE_TEST( sSession->setSessionPropertyInfo( sPropertyId,
                                                    aPropValue,
                                                    aPropValueLen )
                != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    /* BUG-31144 */
    IDE_EXCEPTION(StatementNumberExceedsInputValue);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_STATEMENT_NUMBER_EXCEEDS_INPUT_VALUE));
    }
    IDE_EXCEPTION(NotUpdatableProperty);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_NOT_UPDATE_PROPERTY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmcSession::memoryCompactCallback()
{
    mmuOS::heapmin();
}

IDE_RC mmcSession::printToClientCallback(void *aSession, UChar *aMessage, UInt aMessageLen)
{
    mmcSession         *sSession         = (mmcSession *)aSession;
    cmiProtocolContext *sCtx = sSession->getTask()->getProtocolContext();

    cmiProtocol         sProtocol;
    cmpArgDBMessageA5  *sArg;

    UInt                sMessageLen = aMessageLen;
    mmcMessageCallback  sMessageCallback = sSession->getMessageCallback();

    /* PROJ-2160 CM Ÿ������
       �ִ� QSF_PRINT_VARCHAR_MAX �����ŭ ������ �ִ�. */
    if (cmiGetPacketType(sCtx) != CMP_PACKET_TYPE_A5)
    {
        /* BUG-46019 UNKNOWN (�������� Ŭ���̾�Ʈ), REG �����̸� �����ؾ� �Ѵ�. */
        if ((sMessageCallback == MMC_MESSAGE_CALLBACK_UNKNOWN) ||
            (sMessageCallback == MMC_MESSAGE_CALLBACK_REG))
        {
            /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
            CMI_WRITE_CHECK_WITH_IPCDA(sCtx, 5, 5 + sMessageLen);

            CMI_WOP(sCtx, CMP_OP_DB_Message);
            CMI_WR4(sCtx, &sMessageLen);

            IDE_TEST( cmiSplitWrite( sCtx, sMessageLen, aMessage )
                      != IDE_SUCCESS );
            if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
            {
                MMT_IPCDA_INCREASE_DATA_COUNT(sCtx);
            }
            else
            {
                IDE_TEST(cmiSend(sCtx, ID_FALSE) != IDE_SUCCESS);
            }
        }
    }
    // A5 client
    else
    {
        CMI_PROTOCOL_INITIALIZE(sProtocol, sArg, DB, Message);
        IDE_TEST(cmtVariableSetData(&sArg->mMessage, aMessage, sMessageLen) != IDE_SUCCESS);
        IDE_TEST(cmiWriteProtocol(sCtx, &sProtocol) != IDE_SUCCESS);
        IDE_TEST(cmiFlushProtocol(sCtx, ID_FALSE) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        IDE_PUSH();

        // bug-26983: codesonar: return value ignored
        // void �� assert �� ó���ؾ� �ϴµ� �޸� ���� ��
        // �ʱ�ȭ�� �����ϸ� �ȵǹǷ� assert�� ó���ϱ�� ��.
        if (cmiGetPacketType(sCtx) == CMP_PACKET_TYPE_A5)
        {
            IDE_ASSERT(cmiFinalizeProtocol(&sProtocol) == IDE_SUCCESS);
        }
        IDE_POP();
    }

    return IDE_FAILURE;
}

IDE_RC mmcSession::getSvrDNCallback(void * /*aSession*/,
                                    SChar ** /*aSvrDN*/,
                                    UInt * /*aSvrDNLen*/)
{
    // proj_2160 cm_type removal: packet encryption codes removed
    return IDE_SUCCESS;
}

UInt mmcSession::getSTObjBufSizeCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getSTObjBufSize();
}

// PROJ-1665 : session�� parallel dml mode�� ��ȯ�Ѵ�.
idBool mmcSession::isParallelDmlCallback(void *aSession)
{
    mmcSession *sMmcSession = (mmcSession *)aSession;

    return sMmcSession->getParallelDmlMode();
}

SInt mmcSession::getOptimizerDiskIndexCostAdjCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getOptimizerDiskIndexCostAdj();
}

// BUG-43736
SInt mmcSession::getOptimizerMemoryIndexCostAdjCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getOptimizerMemoryIndexCostAdj();
}

UInt mmcSession::getTrclogDetailPredicateCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getTrclogDetailPredicate();
}

// BUG-15396 ���� ��, �߰��Ǿ���
// Transaction Begin �ÿ� �ʿ��� session flag �������� ����
void mmcSession::setSessionInfoFlagForTx(UInt   aIsolationLevel,
                                         UInt   aReplicationMode,
                                         UInt   aTransactionMode,
                                         idBool aCommitWriteWaitMode)
{
    mInfo.mIsolationLevel      = aIsolationLevel;
    mInfo.mReplicationMode     = aReplicationMode;
    mInfo.mTransactionMode     = aTransactionMode;
    mInfo.mCommitWriteWaitMode = aCommitWriteWaitMode;
}

// PROJ-1583 large geometry
void mmcSession::setSTObjBufSize(UInt aObjBufSize )
{
    mInfo.mSTObjBufSize = aObjBufSize;
}

UInt mmcSession::getSTObjBufSize()
{
    return mInfo.mSTObjBufSize;
}

/* BUG-46041 Shard Meta ������ ���� ������ ó���ϴ� ������ �ʿ��մϴ�. */
IDE_RC mmcSession::reloadShardMetaNumber( idBool aIsLocalOnly )
{
    sdiClientInfo * sOrgClientInfo = mQciSession.mQPSpecific.mClientInfo;
    ULong           sSMN = ID_ULONG(0);

    mQciSession.mQPSpecific.mClientInfo = NULL;

    if ( ( qci::getStartupPhase() == QCI_STARTUP_SERVICE ) &&
         ( sdi::isShardEnable() == ID_TRUE ) )
    {
        IDE_TEST( sdi::reloadSMNForDataNode(NULL) != IDE_SUCCESS );

        sSMN = sdi::getSMNForDataNode();

        /*  BUG-47148 
         *  [sharding] shard meta �� ����Ǵ� procedure ����� trc log �� ������ ���ܾ� �մϴ�.  
         */
        if ( mExecutingStatement != NULL )
        {
            ideLog::log( IDE_SD_17, 
                         "[CHANGE_SHARD_META : %s, SMN(%"ID_UINT64_FMT")]", 
                         mExecutingStatement->getQueryString(), 
                         sSMN );
        }
        else
        {
            ideLog::log( IDE_SD_17, 
                         "[CHANGE_SHARD_META : RELOAD SHARD META NUMBER%s, SMN(%"ID_UINT64_FMT")]", 
                         (  aIsLocalOnly == ID_TRUE ) ? " LOCAL": " ",
                         sSMN );
        }

        if ( aIsLocalOnly == ID_FALSE )
        {
            /* Session �� ShardMetaNumber �� �������� �ʱ� ����
             * mmcSession::makeShardSession ���
             * mmcSession::makeShardSessionWithoutSession �� ȣ���Ѵ�.
             */
            IDE_TEST( makeShardSessionWithoutSession( sSMN,
                                                      sSMN,
                                                      NULL, // smiTrans
                                                      ID_FALSE )
                      != IDE_SUCCESS );

            IDE_TEST( sdi::openAllShardConnections( &mQciSession ) != IDE_SUCCESS );

            sdi::finalizeSession( &mQciSession );

            ideLog::log( IDE_SD_17, 
                         "[CHANGE_SHARD_META : APPLY SHARD CHANGE META]" );
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        /* Nothing to do. */
    }

    mQciSession.mQPSpecific.mClientInfo = sOrgClientInfo;
    /* temporary inserted for test, lswhh
     * the function below(loadShardPinInfo()) must be moved
     * to the shard add or join syntax in shard cluster management project*/
    sdi::loadShardPinInfo();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    sdi::finalizeSession( &mQciSession );
    IDE_POP();

    mQciSession.mQPSpecific.mClientInfo = sOrgClientInfo;

    ideLog::log( IDE_SD_17, "[CHANGE_SHARD_META : FAILURE] ERR-<%"ID_xINT32_FMT"> : <%s>",
                 E_ERROR_CODE( ideGetErrorCode() ),
                 ideGetErrorMsg( ideGetErrorCode() ) );

    return IDE_FAILURE;
}

IDE_RC mmcSession::setCallbackForReloadNewIncreasedDataSMN(smiTrans * aTrans)
{
    ULong     sNewSMN = SDI_NULL_SMN;
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTrans->isBegin() == ID_TRUE );

    if ( sdi::isShardEnable() == ID_TRUE )
    {
        IDE_TEST_RAISE( isMetaNodeShardCli() == ID_TRUE, ERR_SHARD_META_CHANGE_BY_SHARDCLI );

        if ( ( mQciSession.mQPSpecific.mFlag & QC_SESSION_SHARD_META_TOUCH_MASK ) ==
             QC_SESSION_SHARD_META_TOUCH_TRUE ) // shard ddl, and shard meta procedure
        {
            IDE_TEST( sdi::getIncreasedSMNForMetaNode( aTrans, &(sNewSMN)) != IDE_SUCCESS );

            mInfo.mToBeShardMetaNumber = sNewSMN;
            IDE_DASSERT( mInfo.mToBeShardMetaNumber != SDI_NULL_SMN );
            aTrans->setGlobalSMNChangeFunc(mmcSession::reloadDataShardMetaNumber);
        }
        else
        {
            /* no shard meta touch: do nothing */
        }
    }
    else
    {
        /* no shard: do nothing */
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SHARD_META_CHANGE_BY_SHARDCLI );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_MMI_NOT_IMPLEMENTED ) );
    }

    IDE_EXCEPTION_END;

    IDE_ERRLOG(IDE_SERVER_0);
    ideLog::log(IDE_SERVER_0, "global shard meta number change failure");

    return IDE_FAILURE;
}

void mmcSession::reloadDataShardMetaNumber( void * aSession )
{
    mmcSession * sSession = (mmcSession*) aSession;
    ULong   sNewSMN = SDI_NULL_SMN;
    
    IDE_DASSERT ( sdi::isShardEnable() == ID_TRUE );

    // R2HA zookeeper smn validation , not user session only , thing later
    // IDE_TEST(qdsd::checkZookeeperSMNAndDataSMN() != IDE_SUCCESS);???

    sNewSMN = sSession->mInfo.mToBeShardMetaNumber;

    if ( sNewSMN > sdi::getSMNForDataNode() )
    {
        sdi::setSMNForDataNode(sNewSMN );
        ideLog::log( IDE_SD_17, "[CHANGE_SHARD_META : RELOAD SMN(%"ID_UINT64_FMT")]", sNewSMN );
    }
    else
    {
        // Nothing to do.
    }

    return;
}

/* BUG-48586 */
void mmcSession::setInternalTableSwap( smiTrans * aTrans )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTrans->isBegin() == ID_TRUE );
    IDE_DASSERT( sdi::isShardEnable() == ID_TRUE );
    IDE_DASSERT( ( mQciSession.mQPSpecific.mFlag & QC_SESSION_INTERNAL_TABLE_SWAP_MASK ) ==
                 QC_SESSION_INTERNAL_TABLE_SWAP_TRUE )

    aTrans->setInternalTableSwap();
}

/* BUG-46090 Meta Node SMN ���� */
void mmcSession::clearShardDataInfo()
{
    mmcStatement * sStmt     = NULL;
    iduListNode  * sIterator = NULL;

    IDU_LIST_ITERATE( getStmtList(), sIterator )
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        sStmt->clearShardDataInfo();
    }
}

void mmcSession::clearShardDataInfoForRebuild()
{
    mmcStatement * sStmt     = NULL;
    iduListNode  * sIterator = NULL;

    IDU_LIST_ITERATE( getStmtList(), sIterator )
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        sStmt->clearShardDataInfoForRebuild();
    }
}

/* BUG-46092 */
void mmcSession::freeRemoteStatement( UInt aNodeId, UChar aMode )
{
    mmcStatement * sStmt     = NULL;
    iduListNode  * sIterator = NULL;

    IDU_LIST_ITERATE( getStmtList(), sIterator )
    {
        sStmt = (mmcStatement *)sIterator->mObj;

        sStmt->freeRemoteStatement( aNodeId, aMode );
    }
}

/* BUG-46092 */
UInt mmcSession::getShardFailoverType( UInt aNodeId )
{
    return mInfo.mDataNodeFailoverType.getClientConnectionStatus( aNodeId );
}

void mmcSession::setSessionBegin(idBool aBegin)
{
    mSessionBegin = aBegin;
}

void mmcSession::setTransPrepared(ID_XID * aXID)
{
    if ( aXID != NULL )
    {
        mTransPrepared = ID_TRUE;
        idlOS::memcpy( &mTransXID, aXID, ID_SIZEOF(ID_XID) );
    }
    else
    {
        mTransPrepared = ID_FALSE;
        idlOS::memset( &mTransXID, 0x00, ID_SIZEOF(ID_XID) );
    }
}

void mmcSession::initTransStartMode()
{
    /* shard meta Ȥ�� shard data session�� ��� tx�� lazy start�Ѵ�. */
    if ( (mmuProperty::getTxStartMode() == 1) ||
         (sdi::isShardEnable() == ID_TRUE) ||
         (isShardUserSession() != ID_TRUE) )
    {
        setTransLazyBegin( ID_TRUE );
    }
    else
    {
        setTransLazyBegin( ID_FALSE );
    }
}

/*******************************************************************
 PROJ-1665
 Description : Parallel DML Mode ����
 Implementation : 
     ���� �Ѱ��� Ʈ������� ���°� ACTIVE�̸� return FALSE
********************************************************************/
IDE_RC mmcSession::setParallelDmlMode(idBool aParallelDmlMode)
{   
    IDE_TEST_RAISE(isAllStmtEnd() != ID_TRUE, StmtRemainError);
    IDE_TEST_RAISE(isActivated() != ID_FALSE, AlreadyActiveError);
    
    mInfo.mParallelDmlMode = aParallelDmlMode;

    return IDE_SUCCESS;

    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2727
IDE_RC mmcSession::setSessionPropertyInfo( UShort   aSessionPropID,
                                           SChar  * aSessionPropValue,
                                           UInt     aSessionPropValueLen )
{    
    idlOS::strncpy( mInfo.mSessionPropValueStr,
                    aSessionPropValue,
                    aSessionPropValueLen );
    mInfo.mSessionPropValueStr[aSessionPropValueLen] = '\0';
    
    mInfo.mSessionPropValueLen = aSessionPropValueLen;
    mInfo.mSessionPropID       = aSessionPropID;
    
    IDE_TEST( sdi::setSessionPropertyFlag( &mQciSession,
                                           aSessionPropID)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setSessionPropertyInfo( UShort   aSessionPropID,
                                           UInt     aSessionPropValue )
{
    idlOS::snprintf( mInfo.mSessionPropValueStr,
                     IDP_MAX_VALUE_LEN + 1,
                     "%"ID_UINT32_FMT,
                     aSessionPropValue );
  
    mInfo.mSessionPropValueLen = idlOS::strlen(mInfo.mSessionPropValueStr);
    mInfo.mSessionPropID       = aSessionPropID;

    IDE_TEST( sdi::setSessionPropertyFlag( &mQciSession,
                                           aSessionPropID)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-47773
IDE_RC mmcSession::setShardSessionProperty()
{
    sdiClientInfo  * sClientInfo        = NULL;
    sdiConnectInfo * sConnectInfo       = NULL;
    UInt             i = 0;

    sClientInfo = mQciSession.mQPSpecific.mClientInfo;

    if ( sClientInfo != NULL )
    {
        sConnectInfo = sClientInfo->mConnectInfo;

        // property copy node
        IDE_TEST( sdi::copyPropertyFlagToCoordPropertyFlag( &mQciSession,
                                                            sClientInfo )
                  != IDE_SUCCESS );

        // property set node
        for ( i = 0; i < sClientInfo->mCount; i++, sConnectInfo++ )
        {
            if ( sConnectInfo->mDbc != NULL )
            {
                IDE_TEST( sdi::setShardSessionProperty( &mQciSession, 
                                                        sClientInfo,
                                                        sConnectInfo )
                          != IDE_SUCCESS );
            }            
        }

        // property all node complete check and set flag
        if (( mQciSession.mQPSpecific.mFlag & QC_SESSION_ATTR_CHANGE_MASK )
            == QC_SESSION_ATTR_CHANGE_TRUE )
        {
            sConnectInfo = sClientInfo->mConnectInfo;
        
            for ( i = 0; i < sClientInfo->mCount; i++, sConnectInfo++ )
            {
                if (( sConnectInfo->mFlag & SDI_CONNECT_ATTR_CHANGE_MASK )
                    == SDI_CONNECT_ATTR_CHANGE_TRUE )
                {
                    // �� ��忡 ������Ƽ�� ���� ���� ����
                    mQciSession.mQPSpecific.mFlag &= ~QC_SESSION_ATTR_SET_NODE_MASK;
                    mQciSession.mQPSpecific.mFlag |= QC_SESSION_ATTR_SET_NODE_FALSE;

                    break;
                }
                else
                {
                    // nothing to do
                }

                if (( sClientInfo->mCount -1 ) == i )
                {
                    // �� ��忡 ������Ƽ�� ���� ��
                    // PROJ-2727
                    sdi::unSetSessionPropertyFlag( &mQciSession );
                }
            }
        }
    }
    else
    {
        // nothing to do
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

IDE_RC mmcSession::commitForceCallback( void    * aSession, 
                                        SChar   * aXIDStr, 
                                        UInt      aXIDStrSize )
{
    if ( sdi::isShardEnable() == ID_TRUE )
    {
        IDE_TEST( ((mmcSession *)aSession)->endPendingBySyntax( aXIDStr, 
                                                                aXIDStrSize,
                                                                ID_TRUE ) 
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mmdXa::commitForce( ((mmcSession*)aSession)->getStatSQL(), /* PROJ-2446 */
                                      aXIDStr, 
                                      aXIDStrSize ) 
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::rollbackForceCallback( void   * aSession, 
                                          SChar  * aXIDStr, 
                                          UInt    aXIDStrSize )
{
    if ( sdi::isShardEnable() == ID_TRUE )
    {
        IDE_TEST( ((mmcSession *)aSession)->endPendingBySyntax( aXIDStr, 
                                                                aXIDStrSize,
                                                                ID_FALSE ) 
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mmdXa::rollbackForce( ((mmcSession*)aSession)->getStatSQL(),  /* PROJ-2446 */
                                        aXIDStr, 
                                        aXIDStrSize ) 
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//BUG-25999
IDE_RC mmcSession::removeHeuristicXidCallback( void  * aSession, 
                                               SChar * aXIDStr, 
                                               UInt    aXIDStrSize ) 
{
    IDE_TEST( mmdXa::removeHeuristicXid( ((mmcSession*)aSession)->getStatSQL(), /* PROJ-2446 */
                                         aXIDStr, 
                                         aXIDStrSize ) 
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


//fix BUG-21794,40772
IDE_RC mmcSession::addXid(ID_XID *aXID)
{
    iduListNode     *sIterator;
    iduListNode     *sNodeNext;
    mmdIdXidNode    *sXidNode;
    idBool           sFound = ID_FALSE;
    
    //fix BUG-21891
    IDU_LIST_ITERATE_SAFE(&mXidLst,sIterator,sNodeNext)
    {
        sXidNode = (mmdIdXidNode*) sIterator->mObj;
        if(mmdXid::compFunc(&(sXidNode->mXID),aXID) == 0)
        {
            //�̹� �ִ� ��쿡�� list�ǳ����� �̵��ؾ� �Ѵ�.
            sFound = ID_TRUE;
            //fix BUG-21891
            IDU_LIST_REMOVE(&sXidNode->mLstNode);
            break;
        }
    }
    
    if(sFound == ID_FALSE)
    {    
        IDE_TEST( mmdXidManager::alloc(&sXidNode, aXID) != IDE_SUCCESS);
    }

    IDU_LIST_ADD_LAST(&mXidLst,&(sXidNode->mLstNode));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
//fix BUG-21794.
void   mmcSession::removeXid(ID_XID * aXID)
{
    iduListNode     *sIterator;
    iduListNode     *sNodeNext;
    mmdIdXidNode    *sXidNode;
    
    IDU_LIST_ITERATE_SAFE(&mXidLst,sIterator,sNodeNext)
    {
        sXidNode = (mmdIdXidNode*) sIterator->mObj;
        if(mmdXid::compFunc(&(sXidNode->mXID),aXID) == 0)
        {
            //same.
            IDU_LIST_REMOVE(&(sXidNode->mLstNode));
            IDE_ASSERT(mmdXidManager::free(sXidNode) == IDE_SUCCESS) ;
            break;
        }
    }
}

/* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
/* Callback function to obtain a mutex from the mutex pool in mmcSession. */
IDE_RC mmcSession::getMutexFromPoolCallback(void      *aSession,
                                            iduMutex **aMutexObj,
                                            SChar     *aMutexName)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getMutexPool()->getMutexFromPool(aMutexObj, aMutexName);
}

/* Callback function to free a mutex from the mutex pool in mmcSession. */
IDE_RC mmcSession::freeMutexFromPoolCallback(void     *aSession,
                                             iduMutex *aMutexObj )
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getMutexPool()->freeMutexFromPool(aMutexObj);
}

/* PROJ-2177 User Interface - Cancel */

/**
 * StmtIDMap�� �� �������� �߰��Ѵ�.
 *
 * @param aStmtCID StmtCID
 * @param aStmtID StmtID
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 */
IDE_RC mmcSession::putStmtIDMap(mmcStmtCID aStmtCID, mmcStmtID aStmtID)
{
    mmcStmtID *sStmtID = NULL;

    IDU_FIT_POINT_RAISE( "mmcSession::putStmtIDMap::alloc::StmtID",
                          InsufficientMemoryException );

    IDE_TEST_RAISE(mStmtIDPool.alloc((void **)&sStmtID)
                   != IDE_SUCCESS, InsufficientMemoryException);
    *sStmtID = aStmtID;

    IDE_TEST_RAISE(mStmtIDMap.insert(aStmtCID, sStmtID)
                   != IDE_SUCCESS, InsufficientMemoryException);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemoryException)
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if (sStmtID != NULL)
    {
        mStmtIDPool.memfree(sStmtID);
    }

    return IDE_FAILURE;
}

/**
 * StmtIDMap���� StmtCID�� �ش��ϴ� StmtID�� ��´�.
 *
 * @param aStmtCID StmtCID
 * @return StmtCID�� �ش��ϴ� StmtID. ������ MMC_STMT_ID_NONE
 */
mmcStmtID mmcSession::getStmtIDFromMap(mmcStmtCID aStmtCID)
{
    mmcStmtID *sStmtID;

    sStmtID = (mmcStmtID *) mStmtIDMap.search(aStmtCID);
    IDE_TEST(sStmtID == NULL);

    return *sStmtID;

    IDE_EXCEPTION_END;

    return MMC_STMT_ID_NONE;
}

/**
 * StmtIDMap���� StmtCID�� �ش��ϴ� �������� �����Ѵ�.
 *
 * @param aStmtCID StmtCID
 * @return ���������� StmtCID�� �ش��ϴ� StmtID. �ش� �������� ���ų� �����ϸ� MMC_STMT_ID_NONE
 */
mmcStmtID mmcSession::removeStmtIDFromMap(mmcStmtCID aStmtCID)
{
    mmcStmtID *sStmtID;
    mmcStmtID sRemovedStmtID;

    sStmtID = (mmcStmtID *) mStmtIDMap.search(aStmtCID);
    IDE_TEST(sStmtID == NULL);

    IDE_TEST(mStmtIDMap.remove(aStmtCID) != IDE_SUCCESS);

    sRemovedStmtID = *sStmtID;
    IDE_TEST(mStmtIDPool.memfree(sStmtID) != IDE_SUCCESS);

    return sRemovedStmtID;

    IDE_EXCEPTION_END;

    return MMC_STMT_ID_NONE;
}

/**
 * PROJ-2208 getNlsISOCurrencyCallback
 *
 *  qp �ʿ� ��� �Ǵ� callback Function ���� ������ ���ڷ� �޾Ƽ�
 *  ������ NLS_ISO_CURRENCY Code�� ��ȯ�Ѵ�. ���� ������ ���ٸ�
 *  Property ���� ��ȯ�Ѵ�.
 */
SChar * mmcSession::getNlsISOCurrencyCallback( void * aSession )
{
    SChar * sCode = NULL;

    if ( aSession != NULL )
    {
        sCode = ((mmcSession *)aSession)->getNlsISOCurrency();
    }
    else
    {
        sCode = MTU_NLS_ISO_CURRENCY;
    }

    return sCode;
}

/**
 * PROJ-2208 getNlsCurrencyCallback
 *
 *  qp �ʿ� ��� �Ǵ� callback Function ���� ������ ���ڷ� �޾Ƽ�
 *  �ش� ������ NLS_CURRENCY Symbol�� ��ȯ�Ѵ�. ���� ������ ���ٸ�
 *  Property ���� ��ȯ�Ѵ�.
 */
SChar * mmcSession::getNlsCurrencyCallback( void * aSession )
{
    SChar * sSymbol = NULL;

    if ( aSession != NULL )
    {
        sSymbol = ((mmcSession *)aSession)->getNlsCurrency();
    }
    else
    {
        sSymbol = MTU_NLS_CURRENCY;
    }

    return sSymbol;
}

/**
 * PROJ-2208 getNlsNumCharCallback
 *
 *  qp �ʿ� ��� �Ǵ� callback Function ���� ������ ���ڷ� �޾Ƽ�
 *  �ش� ������ NLS_NUMERIC_CHARS �� ��ȯ�Ѵ�. ���� ������ ���ٸ�
 *  Property ���� ��ȯ�Ѵ�.
 */
SChar * mmcSession::getNlsNumCharCallback( void * aSession )
{
    SChar * sNumeric = NULL;

    if ( aSession != NULL )
    {
        sNumeric = ((mmcSession *)aSession)->getNlsNumChar();
    }
    else
    {
        sNumeric = MTU_NLS_NUM_CHAR;
    }

    return sNumeric;
}

/* PROJ-2047 Strengthening LOB - LOBCACHE */
UInt mmcSession::getLobCacheThresholdCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getLobCacheThreshold();
}

/* PROJ-1090 Function-based Index */
UInt mmcSession::getQueryRewriteEnableCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getQueryRewriteEnable();
}

/*
 *
 */ 
void * mmcSession::getDatabaseLinkSessionCallback( void * aSession )
{
    mmcSession * sSession = (mmcSession *)aSession;

    return (void *)&(sSession->mDatabaseLinkSession);
}

IDE_RC mmcSession::setGlobalTransactionLevel( UInt aValue )
{
    SInt        sStep = 0;
    smiTrans  * sSmiTrans;

    if ( mInfo.mGlobalTransactionLevel != aValue )
    {
        IDE_TEST( checkGCTxPermit( aValue ) != IDE_SUCCESS );

        IDE_TEST( dkiSessionSetGlobalTransactionLevel( & mDatabaseLinkSession,
                                                       aValue )
                  != IDE_SUCCESS );
        sStep = 1;

        IDE_TEST( sdi::setTransactionLevel( & mQciSession,
                                            mInfo.mGlobalTransactionLevel,
                                            aValue )
                  != IDE_SUCCESS );
        sStep = 2;

        /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
        IDE_TEST( sdi::setCommitMode( & mQciSession,
                                      isAutoCommit(),
                                      aValue,
                                      dkiIsGTx( aValue ),
                                      dkiIsGCTx( aValue ) )
                  != IDE_SUCCESS );

        mInfo.mGlobalTransactionLevel = aValue;

        setGlobalTransactionLevelFlag();

        /* BUG-48829 : TX�� �ݿ� */
        sSmiTrans = mmcTrans::getSmiTrans( getTransPtr() );

        if ( ( getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT ) && 
             ( sSmiTrans != NULL ) )
        {
            sSmiTrans->setGlobalTransactionLevel(mIsGCTx);
        }
    }
    else
    {
        /* �����Ϸ��� ���̶� ������ ���� ������ return success */
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStep )
    {
        case 2 :
            /* fall through */
            (void)sdi::setTransactionLevel( & mQciSession,
                                            aValue,
                                            mInfo.mGlobalTransactionLevel );
        case 1 :
            (void) dkiSessionSetGlobalTransactionLevel( & mDatabaseLinkSession,
                                                        mInfo.mGlobalTransactionLevel );
            /* fall through */
        default :
            break;
    }

    return IDE_FAILURE;
}

IDE_RC mmcSession::checkGCTxPermit( UInt aValue )
{
    if ( dkiIsGCTx(aValue) == ID_TRUE )
    {
        /* PROJ-2733-DistTxInfo GCTx ������ Ŭ���̾�Ʈ������ GCTx�� ������� ����. (ALTER SESSION) */
        IDE_TEST_RAISE( getGCTxPermit() == ID_FALSE , GCTxNotPermit );
        /* HDB �� GCTx�� ������� ����. */
        IDE_TEST_RAISE( SDU_SHARD_ENABLE == 0, GCTxNotAllow );
    }

    /* BUG-48352 GlobalTransactionLevel 3 <-> 1,2 �� ������. */
    if ( ( dkiIsGCTx( mInfo.mGlobalTransactionLevel ) == ID_TRUE ) ||
         ( dkiIsGCTx( aValue ) == ID_TRUE ) )
    {
        IDE_DASSERT ( SDU_SHARD_ENABLE == 1 );

        IDE_TEST_RAISE( isAllStmtEnd() != ID_TRUE, StmtRemainError );
        IDE_TEST_RAISE( isActivated() != ID_FALSE, AlreadyActiveError );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( GCTxNotPermit )
    {
        IDE_SET( ideSetErrorCode(mmERR_ABORT_GCTX_NOT_PERMIT) );
    }
    IDE_EXCEPTION( GCTxNotAllow )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_GCTX_NOT_ALLOW) );
    }
    IDE_EXCEPTION( StmtRemainError );
    {
        IDE_SET( ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS) );
    }
    IDE_EXCEPTION( AlreadyActiveError );
    {
        IDE_SET( ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmcSession::setDblinkRemoteStatementAutoCommit( UInt aValue )
{
    mInfo.mDblinkRemoteStatementAutoCommit = aValue;
}

dkiSession * mmcSession::getDatabaseLinkSession( void )
{
    return &mDatabaseLinkSession;
}

/*
 *
 */ 
IDE_RC mmcSession::commitForceDatabaseLinkCallback( void * aSession,
                                                    idBool aInStoredProc )
{
    return ((mmcSession *)aSession)->commitForceDatabaseLink( aInStoredProc );
}

/*
 *
 */ 
IDE_RC mmcSession::rollbackForceDatabaseLinkCallback( void * aSession,
                                                      idBool aInStoredProc )
{
    return ((mmcSession *)aSession)->rollbackForceDatabaseLink( aInStoredProc );
}

ULong mmcSession::getSessionLastProcessRowCallback( void * aSession )
{
    return ((mmcSession *)aSession)->getInfo()->mLastProcessRow; 
}

/* BUG-38509 autonomous transaction */
IDE_RC mmcSession::swapTransaction( void * aUserContext , idBool aIsAT )
{
    qciSwapTransactionContext * sArg;
    mmcSession                * sSession;
    mmcStatement              * sStatement;
    mmcTransObj               * sTrans = NULL;
    UInt                        sStage = 0;
    smSCN                       sDummySCN = SM_SCN_INIT;

    sArg       = (qciSwapTransactionContext *)aUserContext;
    sSession   = (mmcSession *)(sArg->mmSession);
    sStatement = (mmcStatement *)(sArg->mmStatement);

    if ( aIsAT == ID_TRUE )
    {
        IDE_TEST( mmcTrans::alloc( NULL, &sTrans ) != IDE_SUCCESS );
        sStage = 1;
        mmcTrans::beginRaw( sTrans, 
                            sSession->getStatSQL(), 
                            sSession->getSessionInfoFlagForTx(), 
                            sSession->getEventFlag() );
        sStage = 2;
        // 1. ���� transaction �� smiStatement�� ����Ѵ�.
        // 2. AT�� �����Ѵ�.
        // 3. mmcStatement->mSmiStmtPtr�� AT�� smiStatement�� �����Ѵ�.
        sArg->mOriSmiStmt = (void *)sStatement->getSmiStmt();
        sArg->mNewMmcTrans = sTrans;
        sArg->mNewSmiTrans = mmcTrans::getSmiTrans(sTrans);
        if ( sSession->getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT )
        {
            sArg->mOriMmcTrans = sSession->mTrans;
            sSession->mTrans = sTrans;
        }
        else
        {
            if ( sStatement->isRootStmt() == ID_TRUE )
            {
                sArg->mOriMmcTrans = sStatement->getTransPtr();
                sStatement->setTrans(sTrans);
            }
            else
            {
                sArg->mOriMmcTrans = sStatement->getParentStmt()->getTransPtr();
                sStatement->getParentStmt()->setTrans(sTrans);
            }
        }

        sStatement->setSmiStmtForAT( sArg->mNewSmiTrans->getStatement() );
    }
    else
    {
        IDE_DASSERT( aIsAT == ID_FALSE );

        sTrans = (mmcTransObj*)sArg->mNewMmcTrans;
        sStage = 2;
        // 1. mmcStatement->mSmiStmtPtr�� ������ smiStatement�� �����Ѵ�.
        // 2. ���� �������̾��� transaction�� �����Ѵ�.
        sStatement->setSmiStmtForAT( (smiStatement *)sArg->mOriSmiStmt );

        if ( sSession->getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT )
        {
            sSession->mTrans = (mmcTransObj*)sArg->mOriMmcTrans;
        }
        else
        {
            if ( sStatement->isRootStmt() == ID_TRUE )
            {
                sStatement->setTrans((mmcTransObj*)sArg->mOriMmcTrans);
            }
            else
            {
                sStatement->getParentStmt()->setTrans((mmcTransObj*)sArg->mOriMmcTrans);
            }
        }

        if ( sArg->mIsExecSuccess == ID_TRUE )
        {
            IDE_TEST( mmcTrans::commitRaw( sTrans,
                                           sSession,
                                           sSession->getEventFlag(),
                                           sSession->getSessionInfoFlagForTx(),
                                           &sDummySCN ) 
                      != IDE_SUCCESS );
            sStage = 1;
            /*
             * �Լ� ��ü���� ����ó�� ������ �����ϰ� ����ϱ� ���ؼ� sStage�� �÷��־�����, 
             * �ǹ̻����ε� commit�� free�� �ٸ� �ܰ�� ��������� commit�� sStage = 1 ���� ó���� ������
             */
        }
        else
        {
            sStage = 1;
            IDE_TEST( mmcTrans::rollbackRaw( sTrans,
                                             sSession,
                                             sSession->getEventFlag(),
                                             sSession->getSessionInfoFlagForTx() )
                      != IDE_SUCCESS );
        }

        sStage = 0;
        IDE_TEST( mmcTrans::free( NULL, sTrans ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sTrans != NULL )
    {
        switch ( sStage )
        {
            case 2:
                (void)mmcTrans::rollbackRaw( sTrans,
                                             sSession,
                                             sSession->getEventFlag(),
                                             sSession->getSessionInfoFlagForTx() );
                /* fall through */
            case 1:
                (void)mmcTrans::free( NULL, sTrans );
                /* fall through */
            case 0:
                /* fall through */
            default:
                break;
        }
    }

    return IDE_FAILURE;
}

/* PROJ-2451 Concurrent Execute Package */
IDE_RC mmcSession::allocInternalSession( void ** aMmSession, void * aOrgMmSession )
{
    return mmtSessionManager::allocInternalSession(
        (mmcSession**)aMmSession,
        ((mmcSession*)aOrgMmSession)->getUserInfo() );
}

IDE_RC mmcSession::allocInternalSessionWithUserInfo( void ** aMmSession, void * aUserInfo )
{
    return mmtSessionManager::allocInternalSession(
        (mmcSession**)aMmSession,
        (qciUserInfo*)aUserInfo );
}


/* PROJ-2451 Concurrent Execute Package */
IDE_RC mmcSession::freeInternalSession( void * aMmSession, idBool aIsSuccess )
{
    IDE_RC sRet;
    mmcSession * sMmSession = (mmcSession*)aMmSession;

    IDE_ERROR( sMmSession != NULL );

    if ( aIsSuccess == ID_TRUE )
    {
        sMmSession->setSessionState( MMC_SESSION_STATE_END );
    }
    else
    {
        sMmSession->setSessionState( MMC_SESSION_STATE_ROLLBACK );
    }
    (void)sMmSession->endSession();

    sRet = mmtSessionManager::freeInternalSession( sMmSession );

    return sRet;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2451 Concurrent Execute Package */
smiTrans * mmcSession::getSessionSmiTrans( void * aMmSession )
{
    mmcSession * sMmSession = (mmcSession*)aMmSession;

    return mmcTrans::getSmiTrans(sMmSession->getTransPtr());
}

smiTrans * mmcSession::getSessionSmiTransWithBegin( void * aMmSession )
{
    mmcSession * sMmSession = (mmcSession*)aMmSession;
    idBool   sIsDummyBegin = ID_FALSE;
    
    if (sMmSession->getTransBegin() == ID_FALSE)
    {
        mmcTrans::begin(sMmSession->mTrans, 
                        &(sMmSession->mStatSQL), 
                        sMmSession->getSessionInfoFlagForTx(), 
                        sMmSession,
                        &sIsDummyBegin);
    }
    else
    {
        /* Nothing to do. */
    }
    return mmcTrans::getSmiTrans(sMmSession->getTransPtr());
}

idvSQL * mmcSession::getStatisticsCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getStatSQL();
}

/* BUG-41452 Built-in functions for getting array binding info. */
IDE_RC mmcSession::getArrayBindInfo( void * aUserContext )
{
    qciArrayBindContext * sArg        = NULL;
    mmcStatement        * sStatement  = NULL;

    sArg       = (qciArrayBindContext *)aUserContext;
    sStatement = (mmcStatement *)(sArg->mMmStatement);

    sArg->mIsArrayBound = sStatement->isArray();

    if ( sArg->mIsArrayBound == ID_TRUE )
    {
        sArg->mCurrRowCnt  = sStatement->getRowNumber();
        sArg->mTotalRowCnt = sStatement->getTotalRowNumber();
    }
    else
    {
        sArg->mCurrRowCnt  = 0;
        sArg->mTotalRowCnt = 0;
    }

    return IDE_SUCCESS;
}

/* BUG-41561 */
UInt mmcSession::getLoginUserIDCallback( void * aMmSession )
{
    return ((mmcSession *)aMmSession)->getUserInfo()->loginUserID;
}

// BUG-41944
UInt mmcSession::getArithmeticOpModeCallback( void * aMmSession )
{
    return ((mmcSession *)aMmSession)->getArithmeticOpMode();
}

/* PROJ-2462 Result Cache */
UInt mmcSession::getResultCacheEnableCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getResultCacheEnable();
}

/* PROJ-2462 Result Cache */
UInt mmcSession::getTopResultCacheModeCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getTopResultCacheMode();
}

/* PROJ-2492 Dynamic sample selection */
UInt mmcSession::getOptimizerAutoStatsCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    // BUG-43629 plan_cache �� ����Ҽ� ��������
    // OPTIMIZER_AUTO_STATS �� ������� �ʴ´�.
    if ( mmuProperty::getSqlPlanCacheSize() > 0 )
    {
        return sSession->getOptimizerAutoStats();
    }
    else
    {
        return 0;
    }
}

/* PROJ-2462 Result Cache */
idBool mmcSession::getIsAutoCommitCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->isAutoCommit();
}

// PROJ-1904 Extend UDT
qciSession * mmcSession::getQciSessionCallback( void * aMmSession )
{
    mmcSession * sMmSession = (mmcSession*)aMmSession;

    return sMmSession->getQciSession();
}

/* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
UInt mmcSession::getOptimizerTransitivityOldRuleCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getOptimizerTransitivityOldRule();
}

/* BUG-42639 Monitoring query */
UInt mmcSession::getOptimizerPerformanceViewCallback( void * aMmSession )
{
    return ((mmcSession *)aMmSession)->getOptimizerPerformanceView();
}

/* PROJ-2638 shard native linker */
SChar *mmcSession::getUserPasswordCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->loginOrgPassword;
}

/* PROJ-2701 Sharding online data rebuild */
idBool mmcSession::isShardUserSessionCallback( void *aSession )
{
    return ((mmcSession *)aSession)->isShardUserSession();
}

/* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
idBool mmcSession::getCallByShardAnalyzeProtocolCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getCallByShardAnalyzeProtocol();
}

ULong mmcSession::getShardPINCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getShardPIN();
}

void mmcSession::setShardPINCallback( void * aSession )
{
    ((mmcSession *)aSession)->setNewSessionShardPin();
}

void mmcSession::getUserInfoCallback( void * aSession, void * aUserInfo )
{
    qciUserInfo * sSrcUserInfo = ((mmcSession *)aSession)->getUserInfo();
    qciUserInfo * sDstUserInfo = (qciUserInfo *)aUserInfo;
    QCI_COPY_USER_INFO( sDstUserInfo, sSrcUserInfo );
}

ULong mmcSession::getShardMetaNumberCallback( void * aSession )
{
    /* BUG-46090 Meta Node SMN ���� */
    return ((mmcSession *)aSession)->getShardMetaNumber();
}

SChar *mmcSession::getShardNodeNameCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getInfo()->mShardNodeName;
}

sdiSessionType mmcSession::getShardSessionTypeCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getInfo()->mShardSessionType;
}

IDE_RC mmcSession::reloadShardMetaNumberCallback( void   *aSession,
                                                  idBool  aIsLocalOnly )
{
    return ((mmcSession *)aSession)->reloadShardMetaNumber( aIsLocalOnly );
}

/* BUG-45899 */
UInt mmcSession::getTrclogDetailShardCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getTrclogDetailShard();
}

UChar mmcSession::getExplainPlanCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getExplainPlan();
}

UInt mmcSession::getGTXLevelCallback( void * aSession )
{
    return ((mmcSession *)aSession)->getGlobalTransactionLevel();
}

/* PROJ-2677 DDL synchronization */
UInt mmcSession::getReplicationDDLSyncCallback( void *aSession )
{
    return ( (mmcSession *)aSession )->getReplicationDDLSync();
}

idBool mmcSession::getTransactionalDDLCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getTransactionalDDL();
}

idBool mmcSession::getGlobalDDLCallback(void *aSession)
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getGlobalDDL();
}

/* BUG-46092 */
UInt mmcSession::isShardCliCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->isShardClient();
}

/* BUG-46092 */
void * mmcSession::getShardStmtCallback( void * aUserContext )
{
    mmcStatement * sStatement = (mmcStatement *)aUserContext;

    if ( sStatement == NULL )
    {
        return NULL;
    }

    return sStatement->getShardStatement();
}

/* BUG-46092 */
void mmcSession::freeShardStmtCallback( void  * aSession, 
                                        UInt    aNodeId, 
                                        UChar   aMode )
{
    ( (mmcSession *)aSession )->freeRemoteStatement( aNodeId, aMode );
}

/* BUG-46092 */
UInt mmcSession::getShardFailoverTypeCallback( void *aSession, UInt aNodeId )
{
    return ( (mmcSession *)aSession )->getShardFailoverType( aNodeId );
}

UInt mmcSession::getPrintOutEnableCallback( void *aSession )
{    
    return  ( (mmcSession *)aSession )->getPrintOutEnable();
}

/* PROJ-2632 */
UInt mmcSession::getSerialExecuteModeCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getSerialExecuteMode();
}

UInt mmcSession::getTrcLogDetailInformationCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getTrcLogDetailInformation();
}

/* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
UInt mmcSession::getReducePartPrepareMemoryCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getReducePartPrepareMemory();
}

// PROJ-2727
void mmcSession::getSessionPropertyInfoCallback( void   * aSession,
                                                 UShort * aSessionPropID,
                                                 SChar  **aSessionPropValue,
                                                 UInt   * aSessionPropValueLen )
{
    ( (mmcSession *)aSession )->getSessionPropertyInfo( aSessionPropID,
                                                        aSessionPropValue,
                                                        aSessionPropValueLen );
}

UInt mmcSession::getCommitWriteWaitModeCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getCommitWriteWaitMode();
}

UInt mmcSession::getDblinkRemoteStatementAutoCommitCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getDblinkRemoteStatementAutoCommit();
}

UInt mmcSession::getDdlTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getDdlTimeout();
}

UInt mmcSession::getFetchTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getFetchTimeout();
}

UInt mmcSession::getIdleTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getIdleTimeout();
}

UInt mmcSession::getMaxStatementsPerSessionCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getMaxStatementsPerSession();
}

UInt mmcSession::getNlsNcharConvExcpCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getNlsNcharConvExcp();
}

void mmcSession::getNlsTerritoryCallback( void * aSession, SChar * aBuffer )
{
    if ( aSession != NULL )
    {
        idlOS::snprintf( aBuffer, MTL_TERRITORY_NAME_LEN + 1, "%s",
                         ((mmcSession *)aSession)->getNlsTerritory() );
    }
    else
    {
        idlOS::snprintf( aBuffer, MTL_TERRITORY_NAME_LEN + 1, "%s",
                         MTU_NLS_TERRITORY );
    }
}

UInt mmcSession::getQueryTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getQueryTimeout();
}

UInt mmcSession::getReplicationDDLSyncTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getReplicationDDLSyncTimeout();
}

ULong mmcSession::getUpdateMaxLogSizeCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getUpdateMaxLogSize();
}

UInt mmcSession::getUTransTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getUTransTimeout();
}

UInt mmcSession::getPropertyAttributeCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getPropertyAttrbute();
}

void mmcSession::setPropertyAttributeCallback( void * aSession, UInt aValue )
{
    ( (mmcSession *)aSession )->setPropertyAttrbute( aValue );
}

sdiInternalOperation mmcSession::getShardInternalLocalOperation( void * aSession )
{
    return ( (mmcSession *)aSession )->getInfo()->mShardInternalLocalOperation;
}

IDE_RC mmcSession::setShardInternalLocalOperationCallback( void * aSession, sdiInternalOperation aValue )
{
    return ( (mmcSession *)aSession )->setShardInternalLocalOperation(aValue);
}
IDE_RC mmcSession::setShardInternalLocalOperation( sdiInternalOperation aValue )
{
    IDE_TEST_RAISE( mInfo.mShardSessionType == SDI_SESSION_TYPE_LIB, NotApplicableError);
    IDE_TEST_RAISE( aValue >= SDI_INTERNAL_OP_MAX, ErrInternalRange );

    mInfo.mShardInternalLocalOperation = aValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION(NotApplicableError);
    { 
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_PROPERTY));
    }
    IDE_EXCEPTION(ErrInternalRange);
    { 
        IDE_SET( ideSetErrorCode( mmERR_ABORT_INTERNAL_SERVER_ERROR_ARG,
                                  "value range overflow" )  );
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7219 */
void * mmcSession::getPlanStringCallback( void * aUserContext )
{
    mmcStatement * sStatement = (mmcStatement *)aUserContext;

    if ( sStatement == NULL )
    {
        return NULL;
    }

    return sStatement->getPlanString();
}

/* BUG-48132 */
UInt mmcSession::getPlanHashOrSortMethodCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getPlanHashOrSortMethod();
}

/* BUG-48161 */
UInt mmcSession::getBucketCountMaxCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getBucketCountMax();
}

/* BUG-48348 */
UInt mmcSession::getEliminateCommonSubexpressionCallback( void * aSession )
{
    return ( (mmcSession *)aSession )->getEliminateCommonSubexpression();
}

// BUG-42464 dbms_alert package
IDE_RC mmcSession::registerCallback( void  * aSession, 
                                     SChar * aName,
                                     UShort  aNameSize )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.regist( aName,
                                                  aNameSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::removeCallback( void  * aSession,
                                   SChar * aName,
                                   UShort  aNameSize )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.remove( aName,
                                                  aNameSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::removeAllCallback( void * aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.removeall() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::setDefaultsCallback( void * aSession,
                                        SInt   aPollingInterval )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.setDefaults( aPollingInterval ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::signalCallback( void  * aSession,
                                   SChar * aName,
                                   UShort  aNameSize,
                                   SChar * aMessage,
                                   UShort  aMessageSize )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.signal( aName,
                                                  aNameSize,
                                                  aMessage,
                                                  aMessageSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::waitAnyCallback( void   * aSession,
                                    idvSQL * aStatistics,
                                    SChar  * aName,
                                    UShort * aNameSize,
                                    SChar  * aMessage,  
                                    UShort * aMessageSize,  
                                    SInt   * aStatus,
                                    SInt     aTimeout )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.waitany( aStatistics,
                                                   aName,
                                                   aNameSize,
                                                   aMessage,
                                                   aMessageSize,
                                                   aStatus,
                                                   aTimeout ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::waitOneCallback( void   * aSession,
                                    idvSQL * aStatistics,
                                    SChar  * aName,
                                    UShort * aNameSize,
                                    SChar  * aMessage,
                                    UShort * aMessageSize,
                                    SInt   * aStatus,
                                    SInt     aTimeout )
{
    mmcSession *sSession = (mmcSession *)aSession;

    IDE_TEST( sSession->getInfo()->mEvent.waitone( aStatistics,
                                                   aName,
                                                   aNameSize,
                                                   aMessage,
                                                   aMessageSize,
                                                   aStatus,
                                                   aTimeout ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::loadAccessListCallback()
{
    IDE_TEST( mmuAccessList::loadAccessList() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::touchShardNode(UInt aNodeId)
{
    ULong       sSMN = ID_ULONG(0);
    ULong       sLastSMN = ID_ULONG(0);
    idBool      sIsDummyBegin = ID_FALSE;
    
    /* autocommit mode������ ����� �� ����. */
    IDE_TEST_RAISE( isAutoCommit() == ID_TRUE, ERR_AUTOCOMMIT_MODE );

    /* tx�� begin���� �ʾ����� begin�Ѵ�. */
    if (getTransBegin() == ID_FALSE)
    {
        mmcTrans::begin( mTrans, 
                         &mStatSQL, 
                         getSessionInfoFlagForTx(), 
                         this,
                         &sIsDummyBegin );
    }
    else
    {
        /* Nothing to do. */
    }

    /* client info�� ������ �����Ѵ�. */
    if ( ( qci::getStartupPhase() == QCI_STARTUP_SERVICE ) &&
         ( sdi::isShardEnable() == ID_TRUE ) &&
         ( isShardUserSession() == ID_TRUE ) )
    {
        sSMN = getShardMetaNumber();
        sLastSMN = getLastShardMetaNumber();

        IDE_TEST( makeShardSession( sSMN,
                                    sLastSMN,
                                    mmcTrans::getSmiTrans(mTrans),
                                    ID_FALSE,
                                    SDI_REBUILD_SMN_PROPAGATE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    IDE_TEST( sdi::touchShardNode( &mQciSession,
                                   &mStatSQL,
                                   mmcTrans::getTransID(mTrans),
                                   aNodeId )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_AUTOCOMMIT_MODE )
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_CANT_SET_TRANSACTION_IN_AUTOCOMMIT_MODE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-46092 */
IDE_RC mmcSession::shardNodeConnectionReport( UInt              aNodeId, 
                                              UChar             aDestination )
{
    idBool sIsDummyBegin = ID_FALSE;

    IDE_TEST_RAISE( isMetaNodeShardCli() != ID_TRUE, ERR_SHARD_META_CHANGE_BY_SHARDCLI );

    if ( ( qci::getStartupPhase() == QCI_STARTUP_SERVICE ) &&
         ( sdi::isShardEnable() == ID_TRUE ) &&
         ( isShardUserSession() == ID_TRUE ) )
    {
        (void)freeRemoteStatement( aNodeId, CMP_DB_FREE_DROP );

        if ( mQciSession.mQPSpecific.mClientInfo != NULL )
        {
            sdi::closeShardSessionByNodeId( &mQciSession,
                                            aNodeId,
                                            aDestination );
        }

        if ( getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT )
        {
            if (getTransBegin() == ID_FALSE)
            {
                mmcTrans::begin( mTrans, 
                                 &mStatSQL, 
                                 getSessionInfoFlagForTx(), 
                                 this,
                                 &sIsDummyBegin );
            }
            else
            {
                /* Nothing to do. */
            }

            sdi::setTransactionBroken( ( ( getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT )
                                         ? ID_TRUE : ID_FALSE ),
                                       &mDatabaseLinkSession,
                                       mmcTrans::getSmiTrans(mTrans) );
        }

        IDE_TEST( mInfo.mDataNodeFailoverType.setClientConnectionStatus( aNodeId,
                                                                         aDestination )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SHARD_META_CHANGE_BY_SHARDCLI );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_MMI_NOT_IMPLEMENTED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::shardNodeTransactionBrokenReport( void )
{
    IDE_TEST_RAISE( isMetaNodeShardCli() != ID_TRUE, ERR_SHARD_META_CHANGE_BY_SHARDCLI );

    if ( ( qci::getStartupPhase() == QCI_STARTUP_SERVICE ) &&
         ( sdi::isShardEnable() == ID_TRUE ) &&
         ( isShardUserSession() == ID_TRUE ) )
    {
        if ( getCommitMode() == MMC_COMMITMODE_AUTOCOMMIT )
        {
            sdi::setTransactionBroken( ID_TRUE,
                                       &mDatabaseLinkSession,
                                       mmcTrans::getSmiTrans(mTrans) );
        }
        else
        {
            sdi::setTransactionBroken( ID_FALSE,
                                       &mDatabaseLinkSession,
                                       mmcTrans::getSmiTrans(mTrans) );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SHARD_META_CHANGE_BY_SHARDCLI );
    {
        IDE_SET( ideSetErrorCode( mmERR_ABORT_MMI_NOT_IMPLEMENTED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-47655 TRANSACTION_TABLE_SIZE ���� ���� �޽��� ����
 * SM���� ����ϴ� callback �Լ�
 * X$SESSION�� Transaction �Ҵ� ��õ� Ƚ���� ����ϱ� ����
 * Session Info �� Retry Count�� �����, */
void mmcSession::setAllocTransRetryCountCallback( void   * aSession,
                                                  ULong    aRetryCount )
{
    mmcSessionInfo * sSessionInfo;
    if ( aSession != NULL )
    {
        sSessionInfo = ((mmcSession*)aSession)->getInfo();
        sSessionInfo->mAllocTransRetryCount = aRetryCount;
    }
}

idBool mmcSession::getShardInPSMEnableCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getShardInPSMEnable();
}

void mmcSession::setShardInPSMEnableCallback( void *aSession, idBool aValue )
{
    mmcSession *sSession = (mmcSession *)aSession;

    sSession->setShardInPSMEnable(aValue);
}

// BUG-47861 INVOKE_USER_ID, INVOKE_USER_NAME function
void mmcSession::setInvokeUserNameCallback(void *aSession, SChar * aInvokeUserName)
{
    ((mmcSession *)aSession)->getUserInfo()->invokeUserNamePtr = aInvokeUserName;
}

SChar * mmcSession::getInvokeUserNameCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getUserInfo()->invokeUserNamePtr;
}

UInt mmcSession::getShardDDLLockTimeout(void *aSession)
{
    mmcSession * sMmSession = (mmcSession*)aSession;
    return sMmSession->mInfo.mShardDDLLockTimeout;
}

UInt mmcSession::getShardDDLLockTryCount(void *aSession)
{
    mmcSession * sMmSession = (mmcSession*)aSession;
    return sMmSession->mInfo.mShardDDLLockTryCount;
}

SInt mmcSession::getDDLLockTimeout(void *aSession)
{
    mmcSession * sMmSession = (mmcSession*)aSession;
    IDE_DASSERT(sMmSession != NULL);
    return sMmSession->mInfo.mDDLLockTimeout;
}

IDE_RC mmcSession::blockDelegateSession( mmcTransObj * aTransObj, mmcSession ** aDelegatedSession )
{
    mmcSession          * sDelegatedSession = NULL;
    mmcTxConcurrency    * sConcurrency      = NULL;
    idBool                sLocked           = ID_FALSE;

    sConcurrency = &aTransObj->mShareInfo->mConcurrency;

    IDE_ASSERT( sConcurrency->mMutex.lock( NULL ) == IDE_SUCCESS );
    sLocked = ID_TRUE;

    IDE_DASSERT( sConcurrency->mFixCount > 0 );
    IDE_DASSERT( sConcurrency->mFixOwner == getSessionID() );

    /* �Ʒ� ������ ����Ǿ�� �ȵȴ�. */

    if ( this == aTransObj->mShareInfo->mTxInfo.mDelegatedSessions )
    {
        /* 1. ���� session �� delegate session �̶�� mBlockCount �� ������� +1 �Ѵ�.
         *    Library session ���� 2PC commit �ϴ� ��� recursive �� �����Ѵ�.
         *    sDelegatedSession �� deletegate session �̾�� �Ѵ�. */
        sDelegatedSession = aTransObj->mShareInfo->mTxInfo.mDelegatedSessions;
        ++sConcurrency->mBlockCount;
    }
    else if ( sConcurrency->mBlockCount > 0 )
    {
        /* 2. ���� session �� delegate session �ƴϴ�.
         *    mBlockCount �� 0 ���� ũ�ٸ� mBlockCount �� �������� �ʴ´�.
         *    recursive �� �����Ѵ�.
         *    sDelegatedSession �� NULL �̾�� �Ѵ�. */
        /* Nothing to do */
    }
    else if ( aTransObj->mShareInfo->mTxInfo.mDelegatedSessions == NULL )
    {
        /* 3. Delegate session �� ���ٸ� mBlockCount �� �������� �ʴ´�.
         *    sDelegatedSession �� NULL �̾�� �Ѵ�. */
        /* Nothing to do */
    }
    else
    {
        /* 4. mBlockCount + 1 ����.
         *    sDelegatedSession �� deletegate session �̾�� �Ѵ�. */
        sDelegatedSession = aTransObj->mShareInfo->mTxInfo.mDelegatedSessions;

        MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                sDelegatedSession->getTransPtr(),
                                "blockDelegateSession: found delegate session");

        if ( ( sDelegatedSession->getSessionState() == MMC_SESSION_STATE_SERVICE ) &&
             ( mInfo.mShardPin == sDelegatedSession->mInfo.mShardPin ) &&
             ( sDelegatedSession->isShardLibrarySession() == ID_TRUE ) )
        {
            /* Valid delegate session
             * Nothing to do */
        }
        else
        {
            /* Nothing to do */
            MMC_SHARED_TRANS_TRACE( sDelegatedSession,
                                    sDelegatedSession->getTransPtr(),
                                    "blockDelegateSession: delegate session is INVALID");

            sDelegatedSession = NULL;
        }

        IDU_FIT_POINT_RAISE( "mmcSession::blockDelegateSession::invalidDelegateSession",
                             ERR_DELEGATE_SESSION_IS_INVALID );
        IDE_TEST_RAISE( sDelegatedSession == NULL,
                        ERR_DELEGATE_SESSION_IS_INVALID );

        ++sConcurrency->mBlockCount;
    }


    IDE_ASSERT( sConcurrency->mMutex.unlock() == IDE_SUCCESS );
    sLocked = ID_FALSE;

    *aDelegatedSession = sDelegatedSession;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DELEGATE_SESSION_IS_INVALID );
    {
        mmcTrans::removeDelegatedSession( aTransObj->mShareInfo , sDelegatedSession );

        IDE_SET( ideSetErrorCode( mmERR_ABORT_INTERNAL_SERVER_ERROR_ARG,
                                  "Delegate session is invalid" ) );
    }
    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( sConcurrency->mMutex.unlock() == IDE_SUCCESS );
    }

    *aDelegatedSession = NULL;

    return IDE_FAILURE;
}

void mmcSession::unblockDelegateSession( mmcTransObj * aTransObj )
{
    mmcTxConcurrency * sConcurrency = NULL;

    sConcurrency = &aTransObj->mShareInfo->mConcurrency;

    IDE_ASSERT( sConcurrency->mMutex.lock( NULL ) == IDE_SUCCESS );

    IDE_DASSERT( sConcurrency->mFixCount > 0 );

    --sConcurrency->mBlockCount;

    IDE_DASSERT( sConcurrency->mBlockCount >= 0 );

    if ( sConcurrency->mBlockCount == 0 )
    {
        sConcurrency->mCondVar.broadcast();
    }

    IDE_ASSERT( sConcurrency->mMutex.unlock() == IDE_SUCCESS );
}

IDE_RC mmcSession::endPendingBySyntax( SChar * aXIDStr,
                                       UInt    aXIDStrSize,
                                       idBool  aIsCommit )
{
    ID_XID    sXID;
    smSCN     sDummySCN;

    /* COMMIT FORCE / ROLLBACK FORCE ������ ���� commit ��
     * GCTx �� Global Commit SCN �� ������ �� �����Ƿ�
     * GTx �� �����ϰ� getCommitSCN �� �̿��� commit �� �����Ѵ�. */
    SM_INIT_SCN(&sDummySCN);

    IDE_TEST( mmdXa::convertStringToXid( aXIDStr, aXIDStrSize, &sXID )
              != IDE_SUCCESS);


    IDE_TEST( mmcTrans::endPending( this,
                                    &sXID,
                                    aIsCommit,
                                    &sDummySCN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2728 */
UInt mmcSession::getStmtIdCallback( void * aUserContext )
{
    return ((mmcStatement *)aUserContext)->getStmtID();
}

void * mmcSession::findShardStmtCallback( void * aSession,
                                          UInt   aStmtId )
{
    mmcStatement * sStatement = NULL;

    IDE_TEST(mmcStatementManager::findStatement(&sStatement,
                                                (mmcSession*)aSession,
                                                aStmtId)
             != IDE_SUCCESS);

    return sStatement->getShardStatement();

    IDE_EXCEPTION_END;

    return NULL;
}

// BUG-47862 Internal use only.
IDE_RC mmcSession::setInvokeUserPropertyInternalCallback( void  * aSession,
                                                          SChar * aPropName,
                                                          UInt    aPropNameLen,
                                                          SChar * aPropValue,
                                                          UInt    aPropValueLen )
{
    if ( aSession != NULL )
    {
        ((mmcSession*)aSession)->getUserInfo()->invokeUserPropertyEnable = ID_TRUE;

        IDE_TEST( mmcSession::setPropertyCallback( aSession,
                                                   aPropName,
                                                   aPropNameLen,
                                                   aPropValue,
                                                   aPropValueLen )
                  != IDE_SUCCESS );

        ((mmcSession*)aSession)->getUserInfo()->invokeUserPropertyEnable = ID_FALSE;
    }
    else
    {
        // nothing to do
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ((mmcSession*)aSession)->getUserInfo()->invokeUserPropertyEnable = ID_FALSE;

    return IDE_FAILURE;
}

/* PROJ-2733-DistTxInfo */
void mmcSession::getStatementRequestSCNCallback(void *aMmStatement, smSCN *aSCN)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    sStatement->getRequestSCN(aSCN);
}

void mmcSession::setStatementRequestSCNCallback(void *aMmStatement, smSCN *aSCN)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    sStatement->setRequestSCN(aSCN);
}

void mmcSession::getStatementTxFirstStmtSCNCallback(void *aMmStatement, smSCN *aTxFirstStmtSCN)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    sStatement->getTxFirstStmtSCN(aTxFirstStmtSCN);
}

ULong mmcSession::getStatementTxFirstStmtTimeCallback(void *aMmStatement)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    return sStatement->getTxFirstStmtTime();
}

sdiDistLevel mmcSession::getStatementDistLevelCallback(void *aMmStatement)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    return sStatement->getDistLevel();
}

idBool mmcSession::isGTxCallback(void *aSession)
{
    return ((mmcSession *)aSession)->isGTx();
}

idBool mmcSession::isGCTxCallback(void *aSession)
{
    return ((mmcSession *)aSession)->isGCTx();
}

UChar *mmcSession::getSessionTypeStringCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getSessionTypeString();
}

UInt mmcSession::getShardStatementRetryCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getShardStatementRetry();
}

UInt mmcSession::getIndoubtFetchTimeoutCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getInfo()->mIndoubtFetchTimeout;
}

UInt mmcSession::getIndoubtFetchMethodCallback( void *aSession )
{
    return ((mmcSession *)aSession)->getInfo()->mIndoubtFetchMethod;
}


void mmcSession::getLastSystemSCN( UChar aOpID, smSCN * aLastSystemSCN )
{
    SM_INIT_SCN( aLastSystemSCN );

    /* PROJ-2733-DistTxInfo */
    if ( isGCTx() == ID_TRUE )
    {
        switch ( aOpID )
        {
            case CMP_OP_DB_ExecuteV3:
            case CMP_OP_DB_ShardEndPendingTxV3:
            case CMP_OP_DB_ParamDataInListV3:
            case CMP_OP_DB_FetchV3:
                switch ( getShardSessionType() )
                {
                    case SDI_SESSION_TYPE_COORD:
                    case SDI_SESSION_TYPE_LIB:
                        sdi::getSystemSCN4GCTx( aLastSystemSCN );
                        break;

                    case SDI_SESSION_TYPE_USER:
                        /* CLI�� SCN�� �������� �ʾƵ� �ȴ�. */
                        if (isShardClient() == SDI_SHARD_CLIENT_TRUE)
                        {
                            sdi::getSystemSCN4GCTx( aLastSystemSCN );
                        }
                        break;

                    default:  /* Non-reachable */
                        break;
                }
                break;

            case CMP_OP_DB_ConnectV3:
            case CMP_OP_DB_PropertySetV3:
                /* answerConnectResult ���� SetProperty ��û (SHARD_SESSION_TYPE, SHARD_CLIENT)��
                   ó���ϱ� ������ ����Ÿ���� �Ǵ� �� �� ����. �׷��� Client���� �Ǵ��ؼ� SCN�� Drop�Ѵ�.
                   Connection ���Ŀ� PropertySetV3 ��û�� �������� ó���� �� ������ ���������� �ʾҴ�. */
                sdi::getSystemSCN4GCTx( aLastSystemSCN );
                break;

            case CMP_OP_DB_ShardTransactionV3:
                /* ShardCLI������ ��û�ϱ� ������ SCN�� �׻� �����ؾ� �Ѵ�. */
                IDE_DASSERT( isShardClient() == SDI_SHARD_CLIENT_TRUE );
                sdi::getSystemSCN4GCTx( aLastSystemSCN );
                break;

            default:
                IDE_CONT( SkipSetScn );
                break;
        }
    }

    if ( SM_SCN_IS_NOT_INIT( *aLastSystemSCN ) )
    {
        SM_SET_SCN( &mInfo.mGCTxCommitInfo.mLastSystemSCN, aLastSystemSCN );
    }

    IDE_EXCEPTION_CONT( SkipSetScn );
}

void mmcSession::setGlobalTransactionLevelFlag()
{
    mIsGTx  = dkiIsGTx( mInfo.mGlobalTransactionLevel );
    mIsGCTx = dkiIsGCTx( mInfo.mGlobalTransactionLevel );

#ifdef DEBUG
    /* BUG-48352 */
    if ( SDU_SHARD_ENABLE == 0 ) 
    {
        IDE_DASSERT ( mIsGCTx == ID_FALSE );
    }
#endif

    mQciSession.mQPSpecific.mIsGTx  = mIsGTx;
    mQciSession.mQPSpecific.mIsGCTx = mIsGCTx;
}

IDE_RC mmcSession::commitInternalCallback( void  * aSession,
                                           void  * aUserContext )
{
    return ((mmcSession *)aSession)->commitInternal( aUserContext );
}

IDE_RC mmcSession::commitInternal( void  * aUserContext )
{
    idBool         sIsBegin = ID_FALSE;
    mmcStatement * sStatement = NULL;
    
    sStatement = (mmcStatement *)aUserContext;
    
    IDE_TEST_RAISE(getSessionState() != MMC_SESSION_STATE_SERVICE,
                   InvalidSessionState);

    IDE_TEST( sStatement->endSmiStmt(SMI_STATEMENT_RESULT_SUCCESS)
              != IDE_SUCCESS);
    
    IDE_TEST( mmcTrans::commit(mTrans, this, SMI_DO_NOT_RELEASE_TRANSACTION )
              != IDE_SUCCESS);
    
    mmcTrans::begin( mTrans, 
                     &mStatSQL, 
                     getSessionInfoFlagForTx(), 
                     this, 
                     &sIsBegin );
    
    IDE_TEST( sStatement->beginSmiStmt( mTrans, SMI_STATEMENT_NORMAL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    // R2HA ����Ѵٸ� ����ó���� �߰� �ؾ���.
    IDE_EXCEPTION(InvalidSessionState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmcSession::executeZookeeperPendingJob()
{
    if ( mZKPendingFunc != NULL )
    {
        mZKPendingFunc(mInfo.mToBeShardMetaNumber, mInfo.mShardMetaNumber);
        clearToBeShardMetaNumber();
        mZKPendingFunc = NULL;
    }
}
void mmcSession::setShardMetaNumberCallback( void  * aSession,
                                             ULong   aSMN )
{
    ((mmcSession *)aSession)->setShardMetaNumber( aSMN );
}

void mmcSession::pauseShareTransFixCallback( void * aMmSession )
{
    mmcSession  * sSession = (mmcSession *)aMmSession;
    mmcTransObj * sTrans   = sSession->mTrans;

    if ( mmcTrans::isShareableTrans( sTrans ) == ID_TRUE )
    {
        mmcTrans::pauseFix( sTrans,
                            &sSession->mTransDump,
                            sSession->mInfo.mSessionID );
    }
}

void mmcSession::resumShareTransFixCallback( void * aMmSession )
{
    mmcSession  * sSession = (mmcSession *)aMmSession;
    mmcTransObj * sTrans   = sSession->mTrans;

    if ( mmcTrans::isShareableTrans( sTrans ) == ID_TRUE )
    {
        mmcTrans::resumeFix( sTrans,
                             &sSession->mTransDump,
                             sSession->mInfo.mSessionID );
    }
}

ULong mmcSession::getLastShardMetaNumberCallback( void * aMmSession )
{
    mmcSession  * sSession = (mmcSession *)aMmSession;

    return sSession->getLastShardMetaNumber();
}

idBool mmcSession::detectShardMetaChangeCallback( void * aMmSession )
{
    mmcSession * sSession = (mmcSession *)aMmSession;

    return sSession->detectShardMetaChange();
}

idBool mmcSession::detectShardMetaChange()
{
    idBool sIsSMNChagned = ID_FALSE;

    /*
     * PROJ-2701 Sharding online data rebuild
     *
     * Rebuild ������ ����
     *     1. shard_meta enable�̾�� �Ѵ�.
     *     2. user connection �� �ƴϾ�� �Ѵ�.
     *     3. sessionSMN < dataSMN�̾�� �Ѵ�.
     */
    if ( ( SDU_SHARD_ENABLE == 1 ) &&
         ( getShardInPSMEnable() == ID_TRUE ) )
    {
        if ( getShardMetaNumber() < sdi::getSMNForDataNode() )
        {
            // session SMN < data SMN
            sIsSMNChagned = ID_TRUE;
        }
    }

    return sIsSMNChagned;
}

IDE_RC mmcSession::rebuildShardSession( ULong         aTargetSMN,
                                        mmcTransObj * aTrans )
{
    ULong         sSMN               = SDI_NULL_SMN;
    ULong         sLastSMN           = SDI_NULL_SMN;
    mmcTransObj * sTargetTrans       = NULL;
    smiTrans    * sSmiTrans          = NULL;
    idBool        sLocked            = ID_FALSE;
    idBool        sIsChangeShardMeta = ID_FALSE;

    IDU_FIT_POINT( "mmcSession::rebuildShardSession" );

    if ( aTargetSMN == SDI_NULL_SMN )
    {
        sSMN = sdi::getSMNForDataNode();
    }
    else
    {
        sSMN = aTargetSMN;
    }

    sLastSMN = getLastShardMetaNumber();

    if ( sLastSMN == SDI_NULL_SMN )
    {
        sLastSMN = getShardMetaNumber();
    }

    if ( getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT )
    {
        /* Non-Autocommit �� ��� aTrans �� ������� �ʰ�
         * Session->mTrans �� ����Ѵ�. */
        sTargetTrans = getTransPtr();

        if ( mmcTrans::isShareableTrans( sTargetTrans ) == ID_TRUE )
        {
            mmcTrans::fixSharedTrans( sTargetTrans, getSessionID() );
            sLocked = ID_TRUE;

            if ( mmcTrans::isSharableTransBegin( sTargetTrans ) == ID_TRUE )
            {
                sSmiTrans = &sTargetTrans->mSmiTrans;
            }
            else
            {
                sLocked = ID_FALSE;
                mmcTrans::unfixSharedTrans( sTargetTrans, getSessionID() );

                sSmiTrans = NULL;
            }
        }
        else
        {
            /* No need to fix trans. */
            if ( getTransBegin() == ID_TRUE )
            {
                sSmiTrans = &sTargetTrans->mSmiTrans;
            }
            else
            {
                sSmiTrans = NULL;
            }
        }
    }
    else
    {
        /* Autocommit �� ��� aTrans �� ����Ѵ�. */
        sTargetTrans = aTrans;

        if ( sTargetTrans != NULL )
        {
            sSmiTrans = &sTargetTrans->mSmiTrans;
        }
        else
        {
            /* sSmiTrans = NULL */
        }
    }

    if ( ( getQciSession()->mQPSpecific.mFlag & QC_SESSION_SHARD_META_TOUCH_MASK ) ==
         QC_SESSION_SHARD_META_TOUCH_TRUE )
    {
        sIsChangeShardMeta = ID_TRUE;
    }

    IDE_TEST( makeShardSession( sSMN,
                                sLastSMN,
                                sSmiTrans,
                                sIsChangeShardMeta,
                                SDI_REBUILD_SMN_DO_NOT_PROPAGATE )
              != IDE_SUCCESS );

    if ( sLocked == ID_TRUE )
    {
        sLocked = ID_FALSE;
        mmcTrans::unfixSharedTrans( sTargetTrans, getSessionID() );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        sLocked = ID_FALSE;
        mmcTrans::unfixSharedTrans( sTargetTrans, getSessionID() );
    }

    return IDE_FAILURE;
}

IDE_RC mmcSession::makeShardSession( ULong                   aTargetSMN,
                                     ULong                   aLastSessionSMN,
                                     smiTrans              * aSmiTrans,
                                     idBool                  aIsShardMetaChanged,
                                     sdiRebuildPropaOption   aRebuildPropaOpt )
{
    setLastShardMetaNumber( aLastSessionSMN );

    setShardMetaNumber( aTargetSMN );

    IDE_TEST( sdi::makeShardSession( &mQciSession,
                                     (void*)&mDatabaseLinkSession,
                                     aSmiTrans,
                                     aIsShardMetaChanged,
                                     aTargetSMN,
                                     aLastSessionSMN,
                                     aRebuildPropaOpt,
                                     ID_FALSE /* aIsPartialCoord */ )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::makeShardSessionWithoutSession( ULong      aTargetSMN,
                                                   ULong      aLastSessionSMN,
                                                   smiTrans * aSmiTrans,
                                                   idBool     aIsShardMetaChanged )
{
#if DEBUG
    ULong sSessionSMN       = getShardMetaNumber();
    ULong sLastSessionSMN   = getLastShardMetaNumber();
#endif

    IDE_TEST( sdi::makeShardSession( &mQciSession,
                                     (void*)&mDatabaseLinkSession,
                                     aSmiTrans,
                                     aIsShardMetaChanged,
                                     aTargetSMN,
                                     aLastSessionSMN,
                                     SDI_REBUILD_SMN_DO_NOT_PROPAGATE,
                                     ID_FALSE /* aIsPartialCoord */ )
              != IDE_SUCCESS );

#if DEBUG
    IDE_DASSERT( sSessionSMN     == getShardMetaNumber() );
    IDE_DASSERT( sLastSessionSMN == getLastShardMetaNumber() );
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmcSession::cleanupShardRebuildSession()
{
    idBool sNodeRemoved = ID_FALSE;

    if ( sdi::setFailoverSuspend( &mQciSession,
                                  SDI_FAILOVER_SUSPEND_ALL,
                                  sdERR_ABORT_FAILED_TO_PROPAGATE_SHARD_META_NUMBER )
         == IDE_SUCCESS )
    {
        if ( propagateShardMetaNumber() == IDE_SUCCESS )
        {
            sdi::cleanupShardRebuildSession( &mQciSession, &sNodeRemoved );

            if ( sNodeRemoved == ID_TRUE )
            {
                clearShardDataInfoForRebuild();
            }

            clearLastShardMetaNumber();

            if ( isShardClient() == ID_FALSE )
            {
                setReceivedShardMetaNumber( getShardMetaNumber() );
            }
        }

        (void)sdi::setFailoverSuspend( &mQciSession,
                                       SDI_FAILOVER_SUSPEND_NONE );
    }
}

IDE_RC mmcSession::propagateRebuildShardMetaNumber()
{
    IDE_TEST( sdi::propagateRebuildShardMetaNumber( &mQciSession )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::propagateShardMetaNumber()
{
    IDE_TEST( sdi::propagateShardMetaNumber( &mQciSession )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmcSession::processShardRetryError( mmcStatement * aStatement,
                                           UInt         * aStmtRetryMax,
                                           UInt         * aRebuildRetryMax )
{
    UInt                   sErrorType;
    smSCN                  sRequestSCN;
    sdiStmtShardRetryType  sRetryType;
    ULong                  sSMN = ID_ULONG(0);

    /* Error �� �߻��� ���·� ���� �Լ� ���� */
    sErrorType  = (ideGetErrorCode() & E_ACTION_MASK);

    aStatement->getRequestSCN( &sRequestSCN );

    switch ( sErrorType )
    {
        case E_ACTION_RETRY:
        case E_ACTION_REBUILD:
            if ( isShardUserSession() == ID_FALSE )
            {
                /* Resharding �� �߻��� ���
                 * Library, Coord session ���� ȥ�� Rebuild �Ǵ� Retry ���� �ʴ´�.
                 * (BUG-46902.tc) */
                IDE_TEST_RAISE( detectShardMetaChange() == ID_TRUE,
                                RebuildEvent );

                /* PROJ-2733 �� GCTx �̸鼭 ReuqestSCN �� ������ ���
                 * ȥ�� Rebuild �Ǵ� Retry ���� �ʰ�
                 * ������ �����Ͽ� ��õ��� �����Ѵ�.
                 * (RebuildRaiseError/TX_LEVEL_03.tc) */
                IDE_TEST_RAISE( ( ( SM_SCN_IS_NOT_INIT( sRequestSCN ) ) &&
                                  ( isGCTx() == ID_TRUE ) &&
                                  ( aStatement->isNeedRequestSCN() == ID_TRUE ) ),
                                StatementIsTooOld );
            }
            break;

        case E_ACTION_ABORT:
            if ( sdi::isShardCoordinator( &aStatement->getQciStmt()->statement ) == ID_TRUE )
            {
                sdi::checkErrorIsShardRetry( &sRetryType );

                switch ( sRetryType )
                {
                    case SDI_STMT_SHARD_REBUILD_RETRY :
                        /* Shard Rebuild Retry ���� �߰� */

                        sSMN = sdi::getSMNForDataNode();

                        /* Rebuild Retry limit �ʰ���. ���� �߻� */
                        IDE_TEST( *aRebuildRetryMax == 0 );

                        --(*aRebuildRetryMax);

                        if ( ( isAutoCommit() == ID_TRUE ) &&
                             ( getShardClientInfo() != NULL ) )
                        {
                            IDE_TEST( ( isGTx() == ID_FALSE ) &&
                                      ( getShardClientInfo()->mGCTxInfo.mDistLevel
                                          > SDI_DIST_LEVEL_SINGLE ) );
                        }

                        /* ideErrorCollectionInit() ȣ�� ���� ������
                         * ���� �Լ�(ex: rebuildShardSession)���� ������ �߻��ϴ� ���
                         * ����ڿ��� gIdeErrorMgr.Stack �� ������ ���޵��� �ʰ�
                         * gIdeErrorMgr.mMultiErrorMgr �� ������ ���޵ȴ�.
                         */
                        ideErrorCollectionInit();

                        IDE_TEST( rebuildShardSession( sSMN,
                                                       aStatement->getExecutingTrans() )
                                  != IDE_SUCCESS );

                        aStatement->addRebuildCount();

                        IDE_TEST( propagateRebuildShardMetaNumber()
                                  != IDE_SUCCESS );

                        IDE_SET( ideSetErrorCode( mmERR_REBUILD_SHARD_INTERNAL_SHARD_META_OUT_OF_DATE ) );
                        break;

                    case SDI_STMT_SHARD_VIEW_OLD_RETRY :
                        /* Shard Statement is too old ���� �߰� */
                        IDE_DASSERT( isGCTx() == ID_TRUE );

                        /* Shard Statement is too old ���� �߻� ������ �ƴ�.
                         * ���� ó���� �Ѵ�. (���� �߻�) */
                        IDE_TEST( isGCTx() == ID_FALSE );

                        /* Statement Retry limit �ʰ���. ���� �߻� */
                        IDE_TEST( *aStmtRetryMax == 0 );

                        --(*aStmtRetryMax);

                        #if defined(DEBUG)
                        ideLog::log( IDE_SD_18, "= [%s] shard statement retry",
                                     getSessionTypeString() );
                        #endif

                        ideErrorCollectionInit();

                        IDE_SET( ideSetErrorCode( mmERR_RETRY_SHARD_INTERNAL_STATEMENT_IS_TOO_OLD ) );
                        break;

                    case SDI_STMT_SHARD_SMN_PROPAGATION :
                        IDE_TEST( getShardClientInfo() == NULL );

                        /* Rebuild Retry limit �ʰ���. ���� �߻� */
                        IDE_TEST( *aRebuildRetryMax == 0 );

                        --(*aRebuildRetryMax);

                        ideErrorCollectionInit();

                        sSMN = getShardClientInfo()->mTargetShardMetaNumber;

                        IDE_TEST( rebuildShardSession( sSMN,
                                                       aStatement->getExecutingTrans() )
                                  != IDE_SUCCESS );

                        aStatement->addRebuildCount();

                        IDE_TEST( propagateRebuildShardMetaNumber()
                                  != IDE_SUCCESS );

                        IDE_SET( ideSetErrorCode( mmERR_REBUILD_SHARD_INTERNAL_SHARD_META_OUT_OF_DATE ) );
                        break;

                    default :
                        /* Do Nothing */
                        break;
                }
            }
            break;

        default :
            /* Do Nothing */
            break;
    }

    /* Do Retry or Rebuild or Abort */

    return IDE_SUCCESS;

    IDE_EXCEPTION( RebuildEvent )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SHARD_META_OUT_OF_DATE ) );
    }
    IDE_EXCEPTION( StatementIsTooOld )
    {
        SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];

        idlOS::snprintf( sMsgBuf,
                         SMI_MAX_ERR_MSG_LEN,
                         "[Exception] "
                         "%s",
                         ideGetErrorMsg() );

        IDE_SET( ideSetErrorCode( smERR_ABORT_StatementTooOld, sMsgBuf ) )
    }
    IDE_EXCEPTION_END;

    switch ( sErrorType )
    {
        case E_ACTION_REBUILD:
            /* ���� ���� ����� rebuild �� �����ϵ��� �Ѵ�. */
            aStatement->getQciStmt()->flag &= ~QCI_STMT_SHARD_RETRY_REBUILD_MASK;
            aStatement->getQciStmt()->flag |= QCI_STMT_SHARD_RETRY_REBUILD_TRUE;
            break;

        default:
            /* Do Nothing */
            break;
    }

    return IDE_FAILURE;
}

void mmcSession::transBeginForGTxEndTran()
{
    mmcTransObj * sTrans        = NULL;
    idBool        sIsDummyBegin = ID_FALSE;

    if ( ( isShareableTrans() == ID_TRUE ) &&
         ( isShardCoordinatorSession() == ID_TRUE ) &&
         ( isGTx() == ID_TRUE ) )
    {
        sTrans = getTransPtr();

        if ( getTransBegin() == ID_FALSE )
        {
            mmcTrans::begin( sTrans,
                             getStatSQL(),
                             getSessionInfoFlagForTx(),
                             this,
                             &sIsDummyBegin );
        }
        else
        {
            /* Nothing to do */
            IDE_DASSERT( sTrans->mSmiTrans.isBegin() == ID_TRUE );
        }
    }
}

/* BUG-48384 */
UShort mmcSession::getClientTouchNodeCountCallback(void *aSession)
{
    return ((mmcSession *)aSession)->getClientTouchNodeCount();
}

/* TASK-7219 Non-shard DML */
void mmcSession::increaseStmtExecSeqForShardTxCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    sSession->increaseStmtExecSeqForShardTx();
}

/* TASK-7219 Non-shard DML */
void mmcSession::decreaseStmtExecSeqForShardTxCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    sSession->decreaseStmtExecSeqForShardTx();
}

/* TASK-7219 Non-shard DML */
UInt mmcSession::getStmtExecSeqForShardTxCallback( void *aSession )
{
    mmcSession *sSession = (mmcSession *)aSession;

    return sSession->getStmtExecSeqForShardTx();
}

/* TASK-7219 Non-shard DML */
sdiShardPartialExecType mmcSession::getStatementShardPartialExecTypeCallback( void *aMmStatement )
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    return sStatement->getShardPartialExecType();
}

/* BUG-48700 */
UInt mmcSession::checkSessionCountCallback()
{
    return mmtSessionManager::getSessionCount();
}
