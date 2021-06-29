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
 * $Id: rpxSender.cpp 90266 2021-03-19 05:23:09Z returns $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smi.h>
#include <smErrorCode.h>

#include <qci.h>

#include <rpDef.h>
#include <rpuProperty.h>
#include <rpcManager.h>
#include <rpxSender.h>
#include <rpuProperty.h>

#include <rpxReplicator.h>

#include <rpnMessenger.h>

/* PROJ-2240 */
#include <rpdCatalog.h>

PDL_Time_Value rpxSender::mTvRetry;
PDL_Time_Value rpxSender::mTvTimeOut;
PDL_Time_Value rpxSender::mTvRecvTimeOut;

rpValueLen     rpxSender::mSyncPKMtdValueLen[QCI_MAX_KEY_COLUMN_COUNT] = {{0, 0},};

//===================================================================
//
// Name:          Constructor
//
// Return Value:
//
// Argument:
//
// Called By:     rpcExecutor.cpp
//
// Description:
//
//===================================================================

rpxSender::rpxSender() : idtBaseThread()
{
}

//===================================================================
//
// Name:          initialize
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:      SChar* a_repName, RP_START_MODE a_flag
//
// Called By:     rpcExecutor.cpp
//
// Description:
//
//--------------------------------------------------------------//
//                     !!   Notice    !!
//                  for the function rpxSender::initialize(a, b, c, d)
//
//    if sender be started by user, d = ID_TRUE
//           rpxSender::initialize(..., ID_TRUE );
//           if ( rpxSender::handshake(...) == ID_TRUE )
//           {
//               rpxSender::start();
//           }
//    if sender be started by system, d = ID_FALSE
//           rpxSender::initialize(..., ID_FALSE );
//           rpxSender::start();
//--------------------------------------------------------------//
//===================================================================

IDE_RC
rpxSender::initialize(smiStatement    * aSmiStmt,
                      SChar           * aRepName,
                      RP_SENDER_TYPE    aStartType,
                      idBool            aTryHandshakeOnce,
                      idBool            aMetaForUpdateFlag,
                      SInt              aParallelFactor,
                      qciSyncItems    * aSyncItemList,
                      rpdSenderInfo   * aSenderInfoArray,
                      rpdLogBufferMgr * aLogBufMgr,
                      rprSNMapMgr     * aSNMapMgr,
                      smSN              aActiveRPRecoverySN,
                      rpdMeta         * aMeta,
                      UInt              aParallelID,
                      UInt              aSenderListIndex )
{
    SChar                sName[IDU_MUTEX_NAME_LEN];
    SInt                 sHostNum;
    rpdReplSyncItem       * sSyncItem          = NULL;
    qciSyncItems       * sSyncItemList      = NULL;
    rpdSenderInfo      * sSenderInfo        = NULL;
    /* PROJ-1915 */
    UInt                 sLFGID;
    rpdReplOfflineDirs * sReplOfflineDirs   = NULL;
    idBool sIsOfflineStatusFound = ID_FALSE;
    idBool               sMessengerInitialized = ID_FALSE;
    idBool               sNeedMessengerLock = ID_FALSE;
    SInt                 sIndex = 0;

    RP_MESSENGER_CHECK_VERSION    sCheckVersion = RP_MESSENGER_NONE;

    mRPLogBufMgr            = NULL;
    mIsRemoteFaultDetect    = ID_TRUE;
    mActiveRPRecoverySN     = aActiveRPRecoverySN;
    mSNMapMgr               = aSNMapMgr;

    mParallelID             = aParallelID;
    mChildArray             = NULL;
    mChildCount             = 0;
    mParallelFactor         = IDL_MIN(aParallelFactor, RP_MAX_PARALLEL_FACTOR);

    mRemoteLFGCount         = 0;

    for(sLFGID = 0; sLFGID < SM_LFG_COUNT; sLFGID++)
    {
        mLogDirPath[sLFGID] = NULL;
    }

    mSenderInfoArray        = aSenderInfoArray;
    mSenderInfo             = NULL;
    mSenderApply            = NULL;
    mSyncInsertItems        = NULL;
    mSyncTruncateItems      = NULL;

    mApplyRetryCount        = 0;
    mRetryError             = ID_FALSE;
    mSetHostFlag            = ID_FALSE;
    mStartComplete          = ID_FALSE;
    mStartError             = ID_FALSE;
    mCheckingStartComplete  = ID_FALSE;

    mExitFlag               = ID_FALSE;
    mApplyFaultFlag         = ID_FALSE;
    mRetry                  = 0;
    mTryHandshakeOnce       = aTryHandshakeOnce;
    mMetaIndex              = -1;

    mStatus                 = RP_SENDER_STOP;
    mFailbackStatus         = RP_FAILBACK_NONE;

    mXSN                    = SM_SN_NULL;
    mSkipXSN                = SM_SN_NULL;
    mCommitXSN              = 0;
    mOldMaxXSN              = 0;

    idlOS::memset(mSocketFile, 0x00, RP_SOCKET_FILE_LEN);
    mRsc                    = NULL;

    mSyncer                 = NULL;

    mAllocator              = NULL;
    mAllocatorState         = 0;

    mRangeColumn            = NULL;
    mRangeColumnCount       = 0;

    mIsServiceFail          = ID_FALSE;
    mTransTableSize         = smiGetTransTblSize();

    mAssignedTransTbl       = NULL;

    mSenderListIndex = aSenderListIndex;

    mIsSetRestartSNforSync = ID_FALSE;

    (void)idCore::acpAtomicSet64( &mServiceThrRefCount, 0 );
    mFailbackEndSN = SM_SN_MAX;

    idlOS::snprintf( sName, IDU_MUTEX_NAME_LEN, "RP_%s_SYNCER_MUTEX",
                     aRepName);
    IDE_TEST_RAISE( mSyncerMutex.initialize( sName,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT );

    /* BUG-31545 : sender ������� ������ ���� ���� session�� �ʱ�ȭ */
    idvManager::initSession(&mStatSession, 0 /* unuse */, NULL /* unuse */);
    idvManager::initSession(&mOldStatSession, 0 /* unuse */, NULL /* unuse */);
    idvManager::initSQL(&mOpStatistics,
                        &mStatSession,
                        NULL, NULL, NULL, NULL, IDV_OWNER_UNKNOWN);

    mTvRetry.initialize(0,0);
    mTvTimeOut.initialize(30,0);
    mTvRecvTimeOut.initialize(300,0);

    mTvRecvTimeOut.set(RPU_REPLICATION_RECEIVE_TIMEOUT, 0);
    mStartType = aStartType;

    if((aTryHandshakeOnce != ID_TRUE) && (aStartType == RP_QUICK))
    {
        /* QUICKSTART RETRY�� ���,
        * Service Thread�� Restart SN�� �����ϰ� Sender Thread�� NORMAL START�� ó���Ѵ�.
        */
        mCurrentType = RP_NORMAL;
    }
    else
    {
        mCurrentType = aStartType;
    }

    if(aSmiStmt != NULL)
    {
        // BUG-40603
        if ( mCurrentType != RP_RECOVERY )
        {
            // PROJ-1442 Replication Online �� DDL ���
            // Service Thread�� Transaction�� �����Ѵ�.
            mSvcThrRootStmt = aSmiStmt->getTrans()->getStatement();
        }
        else
        {    
            // BUG-40603
            mSvcThrRootStmt = NULL;
        }
    }
    else
    {
        mSvcThrRootStmt = NULL;
    }

    mSentLogCountArray       = NULL;
    mSentLogCountSortedArray = NULL;

    mMeta.initialize();

    if ( aStartType == RP_OFFLINE )
    {
        rpcManager::setOfflineCompleteFlag( aRepName, ID_FALSE, &sIsOfflineStatusFound );
        IDE_ASSERT( sIsOfflineStatusFound == ID_TRUE );
        
        rpcManager::setOfflineStatus( aRepName, RP_OFFLINE_STARTED, &sIsOfflineStatusFound );
        IDE_ASSERT( sIsOfflineStatusFound == ID_TRUE );
    }

    ////////////////////////////////////////////////////////////////////////////
    // ��������� �������� �ʴ´�.
    ////////////////////////////////////////////////////////////////////////////

    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_TIME_MUTEX", aRepName);
    IDE_TEST_RAISE(mTimeMtxRmt.initialize(sName,
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    IDE_TEST_RAISE(mTimeCondRmt.initialize() != IDE_SUCCESS,
                   ERR_COND_INIT);

    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_FINAL_MUTEX", aRepName);
    IDE_TEST_RAISE(mFinalMtx.initialize(sName,
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_CHILD_MUTEX", aRepName);
    IDE_TEST_RAISE(mChildArrayMtx.initialize(sName,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);
    
    idlOS::snprintf( sName, IDU_MUTEX_NAME_LEN, "RP_%s_STATUS_MUTEX",
                     aRepName);
    IDE_TEST_RAISE( mStatusMutex.initialize( sName,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT );


    //--------------------------------------------------------------//
    //    set replication & start flag
    //--------------------------------------------------------------//

    /* PROJ-1442 Replication Online �� DDL ���
     * Meta�� �����ϱ� ���� SYNC ITEM�� ���Ѵ�.
     */
    if(aSyncItemList != NULL)
    {
        for(sSyncItemList = aSyncItemList;
            sSyncItemList != NULL;
            sSyncItemList = sSyncItemList->next)
        {
            sSyncItem = NULL;

            IDU_FIT_POINT_RAISE( "rpxSender::initialize::malloc::SyncItem",
                                  ERR_MEMORY_ALLOC_SYNC_ITEM );
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPX_SENDER,
                                             ID_SIZEOF(rpdReplSyncItem),
                                             (void**)&sSyncItem,
                                             IDU_MEM_IMMEDIATE)
                           != IDE_SUCCESS, ERR_MEMORY_ALLOC_SYNC_ITEM);

            idlOS::strncpy( sSyncItem->mUserName,
                            sSyncItemList->syncUserName,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mUserName [ QCI_MAX_OBJECT_NAME_LEN ] = '\0';

            idlOS::strncpy( sSyncItem->mTableName,
                            sSyncItemList->syncTableName,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mTableName [ QCI_MAX_OBJECT_NAME_LEN ] = '\0';

            idlOS::strncpy( sSyncItem->mPartitionName,
                            sSyncItemList->syncPartitionName,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mPartitionName[ QCI_MAX_OBJECT_NAME_LEN ] = '\0';
            
            sSyncItem->mTableOID = SM_NULL_OID;
            sSyncItem->mReplUnit[0] = '\0';

            sSyncItem->next       = mSyncInsertItems;
            mSyncInsertItems      = sSyncItem;
        }
    }

    IDE_TEST(buildMeta(aSmiStmt,
                       aRepName,
                       aStartType,
                       aMetaForUpdateFlag,
                       aMeta)
             != IDE_SUCCESS);

    if ( rpdMeta::isUseV6Protocol( &( mMeta.mReplication ) ) == ID_TRUE )
    {
        IDE_TEST_RAISE( mMeta.hasLOBColumn() == ID_TRUE,
                        ERR_NOT_SUPPORT_LOB_COLUMN_WITH_V6_PROTOCOL );
    }
    else
    {
        /* do nothing */
    }
    idlOS::strncpy(mRepName,mMeta.mReplication.mRepName, QCI_MAX_NAME_LEN + 1);
    mSentLogCountArraySize      = mMeta.mReplication.mItemCount;
   
    /* PROJ-1915 : off �α� ��� �� LFG ī��Ʈ ��� */
    switch(mCurrentType)
    {
        case RP_PARALLEL:
        case RP_NORMAL:
        case RP_QUICK:
        case RP_SYNC:
        case RP_SYNC_ONLY:
            if(mMeta.mReplication.mReplMode == RP_EAGER_MODE)
            {
                rpdMeta::setReplFlagParallelSender(&mMeta.mReplication);
            }
            break;

        case RP_OFFLINE:
            IDE_TEST(rpdCatalog::getReplOfflineDirCount(aSmiStmt,
                                                        mRepName,
                                                       &mRemoteLFGCount)
                     != IDE_SUCCESS);

            IDU_FIT_POINT_RAISE( "rpxSender::initialize::malloc::ReplOfflineDirs",
                                  ERR_MEMORY_ALLOC_OFFLINE_DIRS );
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD_META,
                                ID_SIZEOF(rpdReplOfflineDirs) * mRemoteLFGCount,
                                (void **)&sReplOfflineDirs,
                                IDU_MEM_IMMEDIATE)
                           != IDE_SUCCESS, ERR_MEMORY_ALLOC_OFFLINE_DIRS);

            IDE_TEST(rpdCatalog::selectReplOfflineDirs(aSmiStmt,
                                                       mRepName,
                                                       sReplOfflineDirs,
                                                       mRemoteLFGCount)
                     != IDE_SUCCESS);

            for(sLFGID =0 ; sLFGID < mRemoteLFGCount; sLFGID++)
            {
                IDU_FIT_POINT_RAISE( "rpxSender::initialize::malloc::LogDirPath",
                                      ERR_MEMORY_ALLOC_LOG_DIR_PATH );
        
                IDE_TEST_RAISE( sLFGID == SM_LFG_COUNT, ERR_OVERFLOW_MAX_LFG_COUNT );

                IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD_META,
                                    SM_MAX_FILE_NAME,
                                    (void **)&mLogDirPath[sLFGID],
                                    IDU_MEM_IMMEDIATE)
                               != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOG_DIR_PATH);
                idlOS::memset(mLogDirPath[sLFGID], 0x00, SM_MAX_FILE_NAME);
                idlOS::memcpy(mLogDirPath[sLFGID],
                              sReplOfflineDirs[sLFGID].mLogDirPath,
                              SM_MAX_FILE_NAME);
            }

            (void)iduMemMgr::free(sReplOfflineDirs);
            sReplOfflineDirs = NULL;

            //Check OFFLOG info
            IDE_TEST(checkOffLineLogInfo() != IDE_SUCCESS);

            //OFFLINE_RECEIVER�� ���� �ϰ�
            rpdMeta::setReplFlagOfflineSender(&mMeta.mReplication);
            break;

        case RP_RECOVERY:
            rpdMeta::setReplFlagRecoverySender(&mMeta.mReplication);
            break;
        
        case RP_START_CONDITIONAL:
            sCheckVersion = RP_MESSENGER_CONDITIONAL;
            rpdMeta::setReplFlagStartCondition(&mMeta.mReplication);
            break;
        case RP_SYNC_CONDITIONAL:
            sCheckVersion = RP_MESSENGER_CONDITIONAL;
            rpdMeta::setReplFlagSyncCondition(&mMeta.mReplication);
            break;

        case RP_XLOGFILE_FAILBACK_MASTER:
            rpdMeta::setReplFlagXLogfileFailbackIncrementalSyncMaster(&mMeta.mReplication);
            break;
        case RP_XLOGFILE_FAILBACK_SLAVE:
            rpdMeta::setReplFlagXLogfileFailbackIncrementalSyncSlave(&mMeta.mReplication);
            mMeta.mReplication.mRPRecoverySN = aActiveRPRecoverySN;
            
            break;
        default: /*Sync,Quick,Normal,Sync Only ...*/
            break;
    }

    /* PROJ-1915 off-line replicator�� RPLogBuffer�� ������� �ʴ´�.
     * BUG-32654 ALA�̸鼭 archive log�϶��� logBufferMgr�� �̿����� �ʴ´�
     * ALA���� LSN�� �̿��ؾ��ϴµ�, �α׹��۸Ŵ������� LSN�� ������� �����Ƿ� �̿��� �� ����.
     * ����, ALA�̸鼭 archive log�϶��� �α� ���۸Ŵ����� �̿����� �ʴ� ������ �����Ѵ�.
     * BUG-42613 propagator�� ���� �α׸� logbuffer���� ������ �����Ƿ� logbuffer�� ������� ����.
     */
    if ( ( aStartType == RP_OFFLINE ) ||
         ( aStartType == RP_XLOGFILE_FAILBACK_MASTER ) ||
         ( aStartType == RP_XLOGFILE_FAILBACK_SLAVE ) ||
         ( ( getRole() == RP_ROLE_ANALYSIS ) &&
           ( smiGetArchiveMode() == SMI_LOG_ARCHIVE ) ) ||
         ( getRole() == RP_ROLE_PROPAGATION ) ||
         ( getRole() == RP_ROLE_ANALYSIS_PROPAGATION )
       )
    {
        mRPLogBufMgr = NULL;
    }
    else
    {
        mRPLogBufMgr = aLogBufMgr;
    }

    //----------------------------------------------------------------//
    //   set Communication information
    //----------------------------------------------------------------//

    // PROJ-1537
    IDE_TEST(rpdCatalog::getIndexByAddr(mMeta.mReplication.mLastUsedHostNo,
                                        mMeta.mReplication.mReplHosts,
                                        mMeta.mReplication.mHostCount,
                                       &sHostNum)
             != IDE_SUCCESS);

    /* PROJ-1915 : IP , PORT�� ���� */
    if(mCurrentType == RP_OFFLINE)
    {
        idlOS::memset(mMeta.mReplication.mReplHosts[sHostNum].mHostIp,
                      0x00,
                      QC_MAX_IP_LEN + 1);
        idlOS::strcpy(mMeta.mReplication.mReplHosts[sHostNum].mHostIp,
                      (SChar *)"127.0.0.1");
        mMeta.mReplication.mReplHosts[sHostNum].mPortNo = RPU_REPLICATION_PORT_NO;
        mMeta.mReplication.mReplHosts[sHostNum].mConnType = RP_SOCKET_TYPE_TCP;
    }

    /* PROJ-2453 */
    if ( mMeta.getReplMode() == RP_EAGER_MODE )
    {
        sNeedMessengerLock = ID_TRUE;
    }
    else
    {
        sNeedMessengerLock = ID_FALSE;
    }

    mSocketType = (RP_SOCKET_TYPE)mMeta.mReplication.mReplHosts[sHostNum].mConnType;
    
    IDE_TEST( mMessenger.initialize( NULL,
                                     sCheckVersion,
                                     mSocketType,
                                     &mExitFlag,
                                     &( mMeta.mReplication ),
                                     NULL,
                                     sNeedMessengerLock )
              != IDE_SUCCESS );



    sMessengerInitialized = ID_TRUE;

    if ( mCurrentType == RP_RECOVERY )
    {
        mMessenger.setSnMapMgr( mSNMapMgr );
    }
    else
    {
        /* nothing to do */
    }

    /* PROJ-1969 Replicated Transaction Grouping */
    checkAndSetGroupingMode();

    IDE_TEST( mReplicator.initialize( mAllocator,
                                      this,
                                      &mSenderInfoArray[mParallelID],
                                      &mMeta,
                                      &mMessenger,
                                      mRPLogBufMgr,
                                      &mOpStatistics,
                                      &mStatSession,
                                      mIsGroupingMode )
              != IDE_SUCCESS );

    //----------------------------------------------------------------//
    //   set TCP information
    //----------------------------------------------------------------//
    if ( mSocketType == RP_SOCKET_TYPE_TCP )
    {

        if( rpdCatalog::getIndexByAddr( mMeta.mReplication.mLastUsedHostNo,
                                     mMeta.mReplication.mReplHosts,
                                     mMeta.mReplication.mHostCount,
                                     &sIndex )
            == IDE_SUCCESS )
        {
            mMessenger.setRemoteTcpAddress(
                mMeta.mReplication.mReplHosts[sIndex].mHostIp,
                mMeta.mReplication.mReplHosts[sIndex].mPortNo );
        }
        else
        {
            mMessenger.setRemoteTcpAddress( (SChar *)"127.0.0.1", 
                                            RPU_REPLICATION_PORT_NO );
        }
    }
    else if ( mSocketType == RP_SOCKET_TYPE_IB )
    {
        if( rpdCatalog::getIndexByAddr( mMeta.mReplication.mLastUsedHostNo,
                                     mMeta.mReplication.mReplHosts,
                                     mMeta.mReplication.mHostCount,
                                     &sIndex )
            == IDE_SUCCESS )
        {
            mMessenger.setRemoteIBAddress(
                mMeta.mReplication.mReplHosts[sIndex].mHostIp,
                mMeta.mReplication.mReplHosts[sIndex].mPortNo, 
                mMeta.mReplication.mReplHosts[sIndex].mIBLatency );
        }
        else
        {
            mMessenger.setRemoteIBAddress( (SChar *)"127.0.0.1", 
                                            RPU_REPLICATION_IB_PORT_NO,
                                            (rpIBLatency)RPU_REPLICATION_IB_LATENCY );
        }
    }
    else
    {
        /* nothing to do */
    }

    // BUG-15362
    if(mCurrentType == RP_RECOVERY) //proj-1608 dummy sender info
    {
        IDU_FIT_POINT_RAISE( "rpxSender::initialize::malloc::SenderInfo",
                              ERR_MEMORY_ALLOC_SENDER_INFO );
        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPX_SENDER,
                                         ID_SIZEOF(rpdSenderInfo),
                                         (void**)&sSenderInfo,
                                         IDU_MEM_IMMEDIATE)
                       != IDE_SUCCESS, ERR_MEMORY_ALLOC_SENDER_INFO);

        new (sSenderInfo) rpdSenderInfo;
        IDE_TEST(sSenderInfo->initialize(RP_LIST_IDX_NULL) != IDE_SUCCESS);
        mSenderInfo = sSenderInfo;
    }
    else
    {
        mSenderInfo = &mSenderInfoArray[mParallelID];
    }

    //���� Sender�� �������� �ʾұ� ������ ���񽺿� ������ ���� �ʱ� ����
    //mSenderInfo�� deActivate�ϰ�, doReplication�ϱ� ���� activate�ؾ� ��
    mSenderInfo->deactivate();

    setStatus(RP_SENDER_STOP);

    /* BUG-22703 thr_join Replace */
    mIsThreadDead = ID_FALSE;
    IDE_TEST_RAISE(mThreadJoinCV.initialize() != IDE_SUCCESS,
                   ERR_COND_INIT);
    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_THREAD_JOIN_MUTEX",
                    aRepName);
    IDE_TEST_RAISE(mThreadJoinMutex.initialize(sName,
                                               IDU_MUTEX_KIND_POSIX,
                                               IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_LOB_COLUMN_WITH_V6_PROTOCOL )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_NOT_SUPPORT_FEATURE_WITH_V6_PROTOCOL,
                                  "Replication with LOB columns") );
    }
    IDE_EXCEPTION(ERR_MUTEX_INIT);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_FATAL_ThrMutexInit));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl Sender] Mutex initialization error");
    }
    IDE_EXCEPTION(ERR_COND_INIT);
    {
        IDE_SET(ideSetErrorCode(rpERR_FATAL_ThrCondInit));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl Sender] Condition variable initialization error");
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_SYNC_ITEM);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::initialize",
                                "sSyncItem"));
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_OFFLINE_DIRS);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::initialize",
                                "sReplOfflineDirs"));
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_LOG_DIR_PATH);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::initialize",
                                "mLogDirPath[sLFGID]"));
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_SENDER_INFO);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::initialize",
                                "sSenderInfo"));
    }
    IDE_EXCEPTION( ERR_OVERFLOW_MAX_LFG_COUNT );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_OVERFLOW_COUNT_REPL_OFFLINE_DIR_PATH ) );
    }
    IDE_EXCEPTION_END;

    if ( aStartType == RP_OFFLINE )
    {
        rpcManager::setOfflineStatus( aRepName, RP_OFFLINE_FAILED, &sIsOfflineStatusFound );
        IDE_ASSERT( sIsOfflineStatusFound == ID_TRUE );
    }

    IDE_ERRLOG(IDE_RP_0);

    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_INIT));
    IDE_ERRLOG(IDE_RP_0);

    IDE_PUSH();

    while(mSyncInsertItems != NULL)
    {
        sSyncItem = mSyncInsertItems;
        mSyncInsertItems = mSyncInsertItems->next;
        (void)iduMemMgr::free(sSyncItem);
    }

    if(sSenderInfo != NULL)
    {
        (void)iduMemMgr::free(sSenderInfo);
    }

    /* PROJ-1915 */
    if(mCurrentType == RP_OFFLINE)
    {
        if(sReplOfflineDirs != NULL)
        {
            (void)iduMemMgr::free(sReplOfflineDirs);
        }

        for(sLFGID = 0 ; sLFGID < SM_LFG_COUNT; sLFGID++)
        {
            if(mLogDirPath[sLFGID] != NULL)
            {
                (void)iduMemMgr::free(mLogDirPath[sLFGID]);
                mLogDirPath[sLFGID] = NULL;
            }
        }
    }

    mMeta.finalize();

    if ( sMessengerInitialized == ID_TRUE)
    {
        mMessenger.destroy(RPN_RELEASE_PROTOCOL_CONTEXT);
    }

    IDE_POP();

    /* BUG-22703 thr_join Replace */
    mIsThreadDead = ID_TRUE;

    return IDE_FAILURE;
}

void rpxSender::destroy()
{
    rpdReplSyncItem * sSyncItem = NULL;
    UInt           i;

    IDE_DASSERT(mChildArray == NULL);

    mReplicator.destroy();

    mMessenger.destroy(RPN_RELEASE_PROTOCOL_CONTEXT);

    IDE_ASSERT(mSenderInfo != NULL);
    if(mCurrentType == RP_RECOVERY)
    {
        mSenderInfo->destroy();
        (void)iduMemMgr::free(mSenderInfo);
    }
    mSenderInfo = NULL;

    /* PROJ-1915 */
    if(mCurrentType == RP_OFFLINE)
    {
        for(i = 0; i < mRemoteLFGCount; i++)
        {
            if(mLogDirPath[i] != NULL)
            {
                (void)iduMemMgr::free(mLogDirPath[i]);
                mLogDirPath[i] = NULL;
            }
        }
    }

    while(mSyncInsertItems != NULL)
    {
        sSyncItem = mSyncInsertItems;
        mSyncInsertItems = mSyncInsertItems->next;
        (void)iduMemMgr::free(sSyncItem);
        sSyncItem = NULL;
    }

    while(mSyncTruncateItems != NULL)
    {
        sSyncItem = mSyncTruncateItems;
        mSyncTruncateItems = mSyncTruncateItems->next;
        (void)iduMemMgr::free(sSyncItem);
        sSyncItem = NULL;
    }

    if(mTimeMtxRmt.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mTimeCondRmt.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mFinalMtx.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mChildArrayMtx.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    /* BUG-22703 thr_join Replace */
    if(mThreadJoinCV.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mThreadJoinMutex.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if ( mSyncerMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if ( mStatusMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }
    else
    {
        /* do nothing */
    }

    mMeta.finalize();

    /*PROJ-1670 replication log buffer*/
    mRPLogBufMgr      = NULL;

    if ( mAssignedTransTbl != NULL )
    {
        (void)iduMemMgr::free( mAssignedTransTbl );
        mAssignedTransTbl = NULL;
    }
    else
    {
        /* do nothing */
    }
    return;
}

IDE_RC rpxSender::initializeThread()
{
    idCoreAclMemAllocType sAllocType = (idCoreAclMemAllocType)iduMemMgr::getAllocatorType();
    idCoreAclMemTlsfInit  sAllocInit = {0};
    idBool                sIsAssignedTransAlloced = ID_FALSE;

    /* Thread�� run()������ ����ϴ� �޸𸮸� �Ҵ��Ѵ�. */

    /* mSyncerMutex�� Service Thread���� ����ϹǷ�, ���ɿ� ������ ���� �ʴ´�. */

    if ( sAllocType != ACL_MEM_ALLOC_LIBC )
    {
        IDU_FIT_POINT_RAISE( "rpxSender::initializeThread::malloc::MemAllocator",
                              ERR_MEMORY_ALLOC_ALLOCATOR );
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_RP_RPX_SENDER,
                                           ID_SIZEOF( iduMemAllocator ),
                                           (void **)&mAllocator,
                                           IDU_MEM_IMMEDIATE )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOC_ALLOCATOR );
        mAllocatorState = 1;

        sAllocInit.mPoolSize = RP_MEMPOOL_SIZE;

        IDE_TEST( iduMemMgr::createAllocator( mAllocator,
                                              sAllocType,
                                              &sAllocInit )
                  != IDE_SUCCESS );
        mAllocatorState = 2;
    }
    else
    {
        /* Nothing to do */
    }

    /* mMeta�� rpcManager Thread�� Handshake�� ������ ������ ����ϹǷ�, ���⿡ ���� �� �ȴ�. */

    /* mSyncInsertItems�� Meta�� ������ �� ����ϹǷ�, ���⿡ ���� �� �ȴ�. */

    /* mFinalMtx, mTimeMtxRmt, mTimeCondRmt, mChildArrayMtx�� Thread ���� �ÿ� ����ϹǷ�, ���⿡ ���� �� �ȴ�. */

    IDE_TEST( allocSentLogCount() != IDE_SUCCESS );
    rebuildSentLogCount();

    /* mLogDirPath�� RP_OFFLINE�� ���� ����Ѵ�. ���� ����� ���� ���⿡ �ű� �� �ִ�. */

    /* mMessenger, mReplicator, mSenderInfo�� Service Thread�� Handshake�� ���� ������ ������ ����ϹǷ�, ���⿡ ���� �� �ȴ�. */

    /* mThreadJoinCV, mThreadJoinMutex�� Thread ���� �ÿ� ����ϹǷ�, ���⿡ ���� �� �ȴ�. */

    if ( ( mMeta.mReplication.mReplMode == RP_EAGER_MODE ) &&
         ( mParallelID == RP_PARALLEL_PARENT_ID ) )
    {
        /* 
         * rpxSender ������ : 4
         * mTransTableSize �� MAXSIZE :16384
         */
        IDU_FIT_POINT( "rpxSender::initializeThread::malloc::AssignedTransTbl" );
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_RP_RPX_SENDER,
                                     ID_SIZEOF( rpxSender * ) * mTransTableSize,
                                     (void **)&mAssignedTransTbl,
                                     IDU_MEM_IMMEDIATE )
                  != IDE_SUCCESS );
        sIsAssignedTransAlloced = ID_TRUE;

        initializeAssignedTransTbl();
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_ALLOCATOR )
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::initializeThread",
                                  "mAllocator" ) );
    }
    IDE_EXCEPTION_END;
    IDE_ERRLOG( IDE_RP_0 );

    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_SENDER_INIT ) );
    IDE_ERRLOG( IDE_RP_0 );

    IDE_PUSH();

    freeSentLogCount();
    
    if ( sIsAssignedTransAlloced == ID_TRUE )
    {
        (void)iduMemMgr::free( mAssignedTransTbl );
        mAssignedTransTbl = NULL;
    }
    else
    {
        /* do nothing */
    }

    switch ( mAllocatorState )
    {
        case 2:
            (void)iduMemMgr::freeAllocator( mAllocator );
            /* fall through */
        case 1:
            (void)iduMemMgr::free( mAllocator );
            break;
        default:
            break;
    }

    mAllocator      = NULL;
    mAllocatorState = 0;

    IDE_POP();

    // BUG-22703 thr_join Replace
    signalThreadJoin();

    return IDE_FAILURE;
}

void rpxSender::finalizeThread()
{
    freeSentLogCount();

    /* Ahead Analyzer */
    mReplicator.shutdownAheadAnalyzerThread();
    mReplicator.joinAheadAnalyzerThread();

    switch ( mAllocatorState )
    {
        case 2:
            (void)iduMemMgr::freeAllocator( mAllocator );
            /* fall through */
        case 1:
            (void)iduMemMgr::free( mAllocator );
            break;
        default:
            break;
    }

    mAllocator      = NULL;
    mAllocatorState = 0;

    return;
}

//===================================================================
//
// Name:          final
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:
//
// Called By:     rpcExecutor.cpp
//
// Description:
//
//===================================================================

void rpxSender::shutdown() // call by rpcExecutor
{
    mReplicator.shutdownAheadAnalyzerThread();

    IDE_ASSERT(final_lock() == IDE_SUCCESS);

    if(mExitFlag != ID_TRUE)
    {
        mExitFlag = ID_TRUE;
        setStatus(RP_SENDER_STOP);

        mSenderInfo->wakeupEagerSender();
  
        IDE_ASSERT(time_lock() == IDE_SUCCESS);
        IDE_ASSERT(wakeup() == IDE_SUCCESS);
        IDE_ASSERT(time_unlock() == IDE_SUCCESS);
    }

    IDE_ASSERT(final_unlock() == IDE_SUCCESS);

    return;
}


void rpxSender::finalize() // call by sender itself
{
    RP_OFFLINE_STATUS sOfflineStatus;
    idBool            sIsOfflineStatusFound;
    UInt              sSenderInfoIdx;

    rpcManager::setDDLSyncCancelEvent( mRepName );

    shutdownNDestroyChildren();

    mReplicator.leaveLogBuffer();

    IDE_ASSERT(final_lock() == IDE_SUCCESS);

    mReplicator.rollbackAllTrans();

    //To fix BUG-5371
    mExitFlag = ID_TRUE;
    setStatus(RP_SENDER_STOP);

    if(mStartComplete != ID_TRUE)
    {
        //fix BUG-12131
        mStartError = ID_TRUE;
        IDL_MEM_BARRIER;
        mStartComplete = ID_TRUE;
    }

    if ( mReplicator.destroyLogMgr() != IDE_SUCCESS )
    {
        IDE_WARNING(IDE_RP_0,
                    "rpxSender::finalize log manager destroy failed.");
        IDE_ERRLOG(IDE_RP_0);
    }

    if ( mCurrentType == RP_OFFLINE )
    {
      rpcManager::getOfflineStatus( mRepName, &sOfflineStatus, &sIsOfflineStatusFound );
      IDE_ASSERT( sIsOfflineStatusFound == ID_TRUE );
      if ( sOfflineStatus != RP_OFFLINE_END )
      {
          rpcManager::setOfflineStatus( mRepName, RP_OFFLINE_FAILED, &sIsOfflineStatusFound );
          IDE_ASSERT( sIsOfflineStatusFound == ID_TRUE );
      }
    }

    if ( isParallelParent() == ID_TRUE )
    {
        for ( sSenderInfoIdx = 0; sSenderInfoIdx < RPU_REPLICATION_EAGER_PARALLEL_FACTOR; sSenderInfoIdx++ )
        {
            mSenderInfoArray[sSenderInfoIdx].setOriginReplMode( RP_USELESS_MODE );
        }
    }
    else
    {
        /* Nothing to do */
    }

    mSenderInfo->finalize();

    IDE_ASSERT(final_unlock() == IDE_SUCCESS);

    mReplicator.shutdownAheadAnalyzerThread();

    return;
}
IDE_RC rpxSender::setHostForNetwork( SChar* aIP, SInt aPort )
{
    SInt i;
    SInt sIPLen1, sIPLen2;
    idBool sIsExistMatch = ID_FALSE;

    if ( ( mRetryError != ID_TRUE ) && 
         ( mMessenger.mRemotePort == aPort ) && 
         ( idlOS::strncmp( mMessenger.mRemoteIP, aIP, QCI_MAX_IP_LEN ) == 0 ) )
    {
        sIsExistMatch = ID_TRUE;
    }
    else
    {
        if ( mSetHostFlag != ID_TRUE )
        {
            mSetHostFlag = ID_TRUE;
        }
        else
        {
            /*do nothing*/
        }

        for(i = 0; i < mMeta.mReplication.mHostCount; i ++)
        {
            if( aPort == (SInt)mMeta.mReplication.mReplHosts[i].mPortNo )
            {
                sIPLen1 = idlOS::strlen(mMeta.mReplication.mReplHosts[i].mHostIp);
                sIPLen2 = idlOS::strlen(aIP);

                if ( sIPLen1 == sIPLen2 )
                {
                    if(idlOS::strncmp(aIP,
                                      mMeta.mReplication.mReplHosts[i].mHostIp,
                                      sIPLen1) == 0)
                    {
                        mMeta.mReplication.mLastUsedHostNo = mMeta.mReplication.mReplHosts[i].mHostNo;
                        sIsExistMatch = ID_TRUE;
                        break;
                    }
                    else
                    {
                        /*do nothing*/
                    }
                }
                else
                {
                    /*do nothing*/
                }
            }
            else
            {
                /*do nothing*/
            }
        }
        IDL_MEM_BARRIER;
        mRetryError = ID_TRUE;
    }
    IDE_TEST_RAISE( sIsExistMatch != ID_TRUE, ERR_HOST_MISMATCH );
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_HOST_MISMATCH );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_NOT_HAVE_HOST,
                                  aIP,
                                  aPort ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//===================================================================
//
// Name:          run
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:
//
// Called By:     rpcExecutor.cpp
//
// Description:
//
//===================================================================
void rpxSender::run()
{
    idBool     sHandshakeFlag;  // for releaseHandshake()
    UInt       sRetryCount  = 0;
    smSN       sRestartSN     = SM_SN_NULL;
    smSN       sDummySN;
    
    // mTryHandshakeOnce�� ID_TRUE�̸�, Handshake�� �̹� ������ ����.
    sHandshakeFlag = mTryHandshakeOnce;

    IDE_CLEAR();

    while((checkInterrupt() != RP_INTR_FAULT) &&
          (checkInterrupt() != RP_INTR_EXIT))
    {
        IDE_CLEAR();

        /* if mTryHandshakeOnce == ID_TRUE,
         * handshake have already been done
         */
        if(mTryHandshakeOnce == ID_TRUE)
        {
            /* already connected by rpcExecutor
             * from now on, sender will be started by system automatically
             */
            mTryHandshakeOnce = ID_FALSE;   // do handshake in next loop
        }
        else
        {
            IDE_DASSERT( mCurrentType != RP_XLOGFILE_FAILBACK_MASTER );
            /* �������� ������ ������ �Ϸ��� ���� ���ù� ������ �����̹Ƿ�
             * �������� ������ ���� ���ᰡ �ƴϰ� ������ �����̴�.
             */
            IDE_TEST_RAISE ( mCurrentType == RP_OFFLINE, ERR_OFFLINE_SENDER_ABNORMALLY_EXIT );
            
            //proj-1608 handshake���� refine�� ����Ǿ�� �Ѵ�
            if(mCurrentType == RP_RECOVERY)
            {
                (void)mSNMapMgr->refineSNMap(mActiveRPRecoverySN);
            }
            // Auto Start
            /* handshake with Peer Server */
            IDU_FIT_POINT( "rpxSender::run::attemptHandshake::SLEEP" );
            IDE_TEST(attemptHandshake(&sHandshakeFlag) != IDE_SUCCESS);

            if(checkInterrupt() == RP_INTR_EXIT)
            {
                continue;
            }
            IDE_DASSERT(sHandshakeFlag == ID_TRUE);


            /* attemptHandshake���� mXSN�� ������ �ǹǷ�,
             * mLogMgr�� �ٽ� �ʱ�ȭ ���־�� �Ѵ�.
             */
            IDE_TEST( mReplicator.destroyLogMgr() != IDE_SUCCESS );
        }

        /* Handshake�� ���������Ƿ�, Replication�� �����Ѵ�. */
        //proj-1608 recovery sender�� ��� sender apply�� �������� ����
        if(mCurrentType != RP_RECOVERY)
        {
            /* SenderApply�� Handshake�� �����ϸ� �ٷ� �����Ѵ�.
             * Handshake�� �����ϸ� Sender�� Sync ���̶� KeepAlive��
             * ������ ������ KeepAlive�� �ش��ϴ� Ack�� �����ؾ��Ѵ�.
             */
            IDE_TEST(startSenderApply() != IDE_SUCCESS);
        }

        IDE_TEST( execOnceAtStart() != IDE_SUCCESS );

        IDU_FIT_POINT( "rpxSender::run::checkInterrupt" );
        // BUG-22291 RP_RECOVERY�� ��쿡�� ���� + SYNC ONLY
        if(checkInterrupt() == RP_INTR_EXIT)
        {
            continue;
        }

        IDE_TEST_RAISE( mReplicator.initTransTable() != IDE_SUCCESS,
                        ERR_TRANS_TABLE_INIT );

        mSenderInfo->activate( mReplicator.getTransTbl(),
                               mMeta.mReplication.mXSN,
                               mMeta.mReplication.mReplMode,
                               mMeta.mReplication.mRole,
                               mSenderListIndex,
                               mAssignedTransTbl );

        // BUG-18527 mXSN�� ���������� ������ ��, Log Manager�� �ʱ�ȭ
        if ( mReplicator.isLogMgrInit() != ID_TRUE )
        {
            /* PROJ-1915 off-line replicator ����Ʈ �α� �޴��� init */
            if(mCurrentType == RP_OFFLINE)
            {
                IDE_TEST_RAISE(mXSN == SM_SN_NULL, ERR_INVALID_SN);
                IDE_TEST( mReplicator.initializeLogMgr( mXSN,
                                                        0,
                                                        ID_TRUE, //����Ʈ �α�
                                                        mMeta.mReplication.mLogFileSize,
                                                        mMeta.mReplication.mLFGCount,
                                                        mLogDirPath )
                          != IDE_SUCCESS );
            }
            else if( mCurrentType == RP_XLOGFILE_FAILBACK_MASTER )
            {
                IDE_DASSERT( mXSN == SM_SN_NULL );
            }
            else
            {
                if(isArchiveALA() == ID_TRUE)
                {
                    // BUGBUG
                    // archive ALA�� start�ɶ� init log mgr�ÿ��� file�� ����������
                    // ���� log file�� �������� checkpoint�� file�� ������ �� �־�
                    // archive ALA�� ���� log file�� ���� �� �ִ�.

                    // BUG-29115
                    // ala�� archive log�� ��� archive log�� �̿��Ͽ� start�� �� �ִ�.
                    IDE_TEST( mReplicator.switchToRedoLogMgr( &sDummySN )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( mReplicator.initializeLogMgr( mXSN,
                                                       RPU_PREFETCH_LOGFILE_COUNT,
                                                       ID_FALSE,
                                                       0,
                                                       0,
                                                       NULL )
                         == IDE_SUCCESS )
                    {
                        sRetryCount = 0;
                    }
                    else
                    {
                        IDE_TEST( ideFindErrorCode( smERR_ABORT_NotFoundLog ) != IDE_SUCCESS );
                        IDE_ERRLOG( IDE_RP_0 );
                        mRetryError = ID_TRUE;

                        sRetryCount += 1;
                        IDE_TEST( sRetryCount > RPU_REPLICATION_SENDER_RETRY_COUNT ); 
                        idlOS::sleep( 1 );                        
                    }
                }

            }

            mReplicator.setNeedSN( mXSN ); //proj-1670
            rebuildSentLogCount();
        }
        
        IDL_MEM_BARRIER;

        if ( mReplicator.isLogMgrInit() == ID_TRUE )
        {
            mReplicator.setLogMgrInitStatus( RP_LOG_MGR_INIT_SUCCESS );

            initReadLogCount();

            // Eager�� ���, Failback�� �����ϰ� Child�� ����&�����Ѵ�.
            if( prepareForRunning() != IDE_SUCCESS )
            {
                IDE_TEST( checkInterrupt() != RP_INTR_RETRY );
            }
            else /* prepare for parallel success */
            {
                IDE_TEST( doRunning() != IDE_SUCCESS );
            }
        }
        else
        {
            if ( mCurrentType == RP_XLOGFILE_FAILBACK_MASTER )
            {
                if( prepareForRunning() != IDE_SUCCESS )
                {
                    IDE_TEST( checkInterrupt() != RP_INTR_RETRY );
                }
            }
            else
            {
                mReplicator.setLogMgrInitStatus( RP_LOG_MGR_INIT_FAIL );
            }
        }

        mReplicator.leaveLogBuffer();

        // Sender Apply Thread�� ������ �����ϸ�, Commit�� ������Ű��
        // Sender Thread�� ������ �����Ų��.
        IDE_TEST_RAISE(checkInterrupt() == RP_INTR_FAULT,
                       ERR_SENDER_APPLY_ABNORMAL_EXIT);
        mSenderInfo->checkAndRunDeactivate();

        // Network ��ֺ��� ���� ���ͷ�Ʈ�� ���, ��õ��� ���� �غ� ���� �ʴ´�.
        if(checkInterrupt() == RP_INTR_RETRY)
        {
            rpcManager::setDDLSyncCancelEvent( mRepName );

            // Retry ���·� �����Ѵ�.
            setStatus( RP_SENDER_RETRY );

            waitUntilSendingByServiceThr();

            cleanupForParallel();

            /* BUG-42138 */
            initializeAssignedTransTbl();
            // Parallel Child�� ���⿡ �� �� ����.
            IDE_DASSERT(isParallelChild() != ID_TRUE);

            // ��Ʈ��ũ�� �������Ƿ� sender apply�� ����Ǿ�� �Ѵ�.
            shutdownSenderApply();

            // replication�� ����Ǿ����Ƿ�, ���ҽ� ��� ����
            sHandshakeFlag = ID_FALSE;  // reset
            releaseHandshake();
            
            checkXSNAndSleep();
           
            IDE_TEST_RAISE( mReplicator.initTransTable() != IDE_SUCCESS,
                            ERR_TRANS_TABLE_INIT );
            
            if( ( mCurrentType != RP_OFFLINE ) && ( mSetHostFlag != ID_TRUE ) )
            {
                IDE_TEST(getNextLastUsedHostNo() != IDE_SUCCESS);
                mSetHostFlag = ID_FALSE;
                // fix BUG-9671 : ���� ȣ��Ʈ�� �ٽ� �����ϱ� ���� SLEEP
                mRetry++;
            }
        }
        else
        {
            if ( mMeta.getReplMode() == RP_CONSISTENT_MODE )
            {
                /*
                 * PROJ-2725 consistent replication
                 * service thread�� ��⸦ ���� sender �� stop, senderInfo�� run ���·� �����Ѵ�.
                 */
                mStatus = RP_SENDER_STOP;
            }
            else
            {
                setStatus( RP_SENDER_STOP );
            }

            waitUntilSendingByServiceThr();

            cleanupForParallel();

            // Parallel Child�� ��� ���ͷ�Ʈ�� ���� ������ �����ϹǷ�, ����� ���´�.
            // BUG-17748
            sRestartSN = getNextRestartSN();
        }
    } /* while */
    // Sender Apply Thread�� ������ �����ϸ�, Sender Thread�� ������ �����Ų��.
    IDE_TEST_RAISE(checkInterrupt() == RP_INTR_FAULT,
                   ERR_SENDER_APPLY_ABNORMAL_EXIT);

    //sync only�� ���
    mSenderInfo->checkAndRunDeactivate();

    /* ���ͷ�Ʈ ��� mRetryError�� ��� : ���� ���ͷ�Ʈ�� �ɷ��ִ� ��쿡��,
     *      Network ��ְ� ������ ���������� �������ؾ� �Ѵ�.
     */
    if ( ( sHandshakeFlag == ID_TRUE ) && ( mRetryError != ID_TRUE ) )
    {
        if ( mCurrentType != RP_OFFLINE )
        {
            /* Send Replication Stop Message */
            ideLog::log( IDE_RP_0, "SEND Stop Message!\n" );

            IDE_TEST( mMessenger.sendStop( sRestartSN,
                                           NULL,
                                           RPN_STOP_MSG_NETWORK_TIMEOUT_SEC ) 
                      != IDE_SUCCESS );

            ideLog::log( IDE_RP_0, "SEND Stop Message SUCCESS!!!\n" );
        }
        else
        {
            /* Send Replication Stop Message */
            ideLog::log( IDE_RP_0, "[OFFLINE SENDER]SEND Stop Message %d!\n",
                                   sRestartSN );

            IDE_TEST( mMessenger.sendStop( sRestartSN,
                                           NULL,
                                           RPN_STOP_MSG_NETWORK_TIMEOUT_SEC ) 
                      != IDE_SUCCESS );

            ideLog::log( IDE_RP_0, "[OFFLINE SENDER]SEND Stop Message SUCCESS!!!\n" );
        }

        if ( mCurrentType != RP_RECOVERY )
        {
            //Send Stop �޼����� ���� Ack�� �ް� ������ ����
            finalizeSenderApply();
        }
        else
        {
            IDE_TEST( mMessenger.receiveAckDummy() != IDE_SUCCESS );
        }
    }
    else
    {
        shutdownSenderApply();
    }

    /* STOP�� ���� ACK�� ���� �Ŀ�, Handshake �ڿ��� �����Ѵ�. */
    if ( sHandshakeFlag == ID_TRUE )
    {
        sHandshakeFlag = ID_FALSE;
        releaseHandshake();
    }
    else
    {
        /* Nothing to do */
    }

    /* �������� Shutdown�� ��쿡�� ������ �ƴ�, �Ϲ� �޼����� ó�� */
    ideLog::log(IDE_RP_0, RP_TRC_S_SENDER_STOP,
                mMeta.mReplication.mRepName,
                mParallelID,
                mXSN,
                getRestartSN());

    finalize();

    // BUG-22703 thr_join Replace
    IDU_FIT_POINT( "1.BUG-22703@rpxSender::run" );
    signalThreadJoin();

    return;

    IDE_EXCEPTION(ERR_TRANS_TABLE_INIT);
    {
    }
    IDE_EXCEPTION(ERR_INVALID_SN);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_INVALID_OFFLINE_RESTARTSN));
    }
    IDE_EXCEPTION(ERR_SENDER_APPLY_ABNORMAL_EXIT);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_SENDER_APPLY_ABNORMAL_EXIT));
    }
    IDE_EXCEPTION( ERR_OFFLINE_SENDER_ABNORMALLY_EXIT );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_OFFLINE_SENDER_ABNORMALLY_EXIT ) );
    }
    IDE_EXCEPTION_END;
    IDE_ERRLOG(IDE_RP_0);

    IDE_SET(ideSetErrorCode(rpERR_IGNORE_RP_SENDER_STOP,
                            mMeta.mReplication.mRepName,
                            mParallelID,
                            mXSN,
                            getRestartSN()));

    IDE_ERRLOG(IDE_RP_0);

    if(mExitFlag != ID_TRUE)
    {
        if ( ( mMeta.mReplication.mReplMode == RP_EAGER_MODE ) &&
             ( ( mCurrentType == RP_NORMAL ) || ( mCurrentType == RP_PARALLEL ) ) &&
             ( ( mStatus == RP_SENDER_FLUSH_FAILBACK ) || ( mStatus == RP_SENDER_IDLE ) ) )
        {
            ideLog::log(IDE_RP_0, RP_TRC_S_ERR_EAGER_ABNORMAL_STOP,
                                  mMeta.mReplication.mRepName);
            IDE_ASSERT(0);
        }
    }

    //doReplication���� Fail�� ���
    mSenderInfo->checkAndRunDeactivate();

    if ( mReplicator.isLogMgrInit() == ID_FALSE )
    {
        mReplicator.setLogMgrInitStatus(RP_LOG_MGR_INIT_FAIL);
    }

    // BUG-16377
    shutdownSenderApply();

    if(sHandshakeFlag == ID_TRUE) // ���ҽ� ���� �۾� �ʿ�.
    {
        // replication�� ����Ǿ����Ƿ�, ���ҽ� ��� ����
        releaseHandshake();
    }

    finalize();

    // BUG-22703 thr_join Replace
    signalThreadJoin();

    return;
}
void rpxSender::cleanupForParallel()
{
    shutdownNDestroyChildren();
}

void rpxSender::initializeAssignedTransTbl()
{
    UInt i = 0;

    if ( mAssignedTransTbl != NULL )
    {
        for ( i = 0 ; i < mTransTableSize ; i++ )
        {
            mAssignedTransTbl[i] = NULL;
        }
    }
    else
    {
        /* nothing to do */
    }
}

/*****************************************************************
 * prepareForParallel()
 * run�� �����ϱ� ���� ó���ؾ��� �۾��� �Ѵ�.
 * ���� eager replication���� run ���� failback�� �ؾ��Ѵ�.
 *****************************************************************/
IDE_RC rpxSender::prepareForParallel()
{
    if((mCurrentType == RP_NORMAL) &&
       (mMeta.mReplication.mReplMode == RP_EAGER_MODE))
    {
        mCurrentType = RP_PARALLEL;
    }

    // Eager mode�� Failback�� ������ ��, Replication�� �����Ѵ�.
    if( (mMeta.mReplication.mReplMode == RP_EAGER_MODE) &&
        (isParallelParent() == ID_TRUE) &&
        (mMeta.mReplication.mItemCount != 0) )
    {
        switch(mFailbackStatus)
        {
            case RP_FAILBACK_NORMAL :
                setStatus(RP_SENDER_FAILBACK_NORMAL);
                IDE_TEST(failbackNormal() != IDE_SUCCESS);
                break;

            case RP_FAILBACK_MASTER :
                setStatus(RP_SENDER_FAILBACK_MASTER);
                IDE_TEST(failbackMaster() != IDE_SUCCESS);
                break;

            case RP_FAILBACK_SLAVE :
                setStatus(RP_SENDER_FAILBACK_SLAVE);
                IDE_TEST(failbackSlave() != IDE_SUCCESS);
                break;

            case RP_FAILBACK_NONE :
            default:
                IDE_ASSERT(0);
        }

        IDU_FIT_POINT( "rpxSender::prepareForParallel::SLEEP::afterFailback1" );
        IDU_FIT_POINT( "rpxSender::prepareForParallel::SLEEP::afterFailback2" );
    }

    // Service Thread�� Statement�� �� �̻� �ʿ����� �ʴ�.
    mSvcThrRootStmt = NULL;

    /* BUG-42732 */
    if ( checkInterrupt() == RP_INTR_NONE )
    {
        if ( mCurrentType == RP_PARALLEL ) 
        {
            mSenderInfo->initializeLastProcessedSNTable();
            IDE_TEST( addXLogAckOnDML() != IDE_SUCCESS );

            if ( isParallelParent() == ID_TRUE )
            {
                IDE_TEST( createNStartChildren() != IDE_SUCCESS );

                IDE_ASSERT( mStatusMutex.lock(NULL) == IDE_SUCCESS );
                setStatus( RP_SENDER_FLUSH_FAILBACK );
                (void)smiGetLastValidGSN( &mFailbackEndSN );
                IDE_ASSERT( mStatusMutex.unlock() == IDE_SUCCESS );
            }
            else
            {
                setStatus( RP_SENDER_IDLE );
            }
        }
        else
        {
            setStatus( RP_SENDER_RUN );
        }

        /* sender�� ���°� run���� �ٲ� �� remotefaultdetect�� �ʱ�ȭ */
        mIsRemoteFaultDetect = ID_FALSE;
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
//===================================================================
//
// Name:          execOnceAtStart
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:
//
// Called By:     rpxSender::run()
//
// Description: sender ���۽� �ѹ��� �����ؾ��ϴ� �۾��� ó���ϰ�,
//              sender�� Ÿ���� �����Ͽ� �ٽ� ������� �ʵ��� �Ѵ�.
//
//===================================================================

IDE_RC rpxSender::execOnceAtStart()
{
    //--------------------------------------------------------------//
    // quickstart/sync start/ sync only start����
    // ������ ������ �� ó���ϱ� ���� �۾��̸�,
    // �� �Լ����� normal/parallel sender�� Ư�� �۾��� ó���ϸ� �ȵ�.
    //--------------------------------------------------------------//
    switch ( mCurrentType )
    {
        case RP_QUICK:
            IDE_TEST(quickStart() != IDE_SUCCESS);

            if(mMeta.mReplication.mReplMode == RP_EAGER_MODE)
            {
                mCurrentType = RP_PARALLEL;
            }
            else
            {
                mCurrentType = RP_NORMAL;

                // �� �̻� Service Thread�� ������� �ʴ´�.
                mSvcThrRootStmt = NULL;
            }
            break;

        case RP_SYNC:
            IDE_TEST_RAISE( rpdMeta::isUseV6Protocol( &( mMeta.mReplication ) ) == ID_TRUE,
                            ERR_NOT_SUPPORT_SYNC_WITH_V6_PROTOCOL );

            switch ( mMeta.mReplication.mRole )
            {
                case RP_ROLE_REPLICATION:
                case RP_ROLE_PROPAGABLE_LOGGING:
                case RP_ROLE_PROPAGATION:
                    IDE_TEST( syncStart() != IDE_SUCCESS );
                    break;
                case RP_ROLE_ANALYSIS:
                case RP_ROLE_ANALYSIS_PROPAGATION:
                    IDE_TEST( syncALAStart() != IDE_SUCCESS );
                    break;
            }
            mSvcThrRootStmt = NULL;
            break;

        case RP_SYNC_ONLY: 
            IDE_TEST_RAISE( rpdMeta::isUseV6Protocol( &( mMeta.mReplication ) ) == ID_TRUE,
                            ERR_NOT_SUPPORT_SYNC_WITH_V6_PROTOCOL );

            //fix BUG-9023
            switch ( mMeta.mReplication.mRole )
            {
                case RP_ROLE_REPLICATION:
                case RP_ROLE_PROPAGABLE_LOGGING:
                case RP_ROLE_PROPAGATION:
                    IDE_TEST( syncStart() != IDE_SUCCESS );
                    break;
                case RP_ROLE_ANALYSIS:
                case RP_ROLE_ANALYSIS_PROPAGATION:
                    IDE_TEST( syncALAStart() != IDE_SUCCESS );
                    break;
            }
            mExitFlag = ID_TRUE;

            // �� �̻� Service Thread�� ������� �ʴ´�.
            mSvcThrRootStmt = NULL;
            break;

        case RP_NORMAL:
        case RP_PARALLEL:
            if ( ( mMeta.getReplMode() != RP_EAGER_MODE ) ||
                 ( mMeta.getReplMode() != RP_CONSISTENT_MODE ) )
            {
                // �� �̻� Service Thread�� ������� �ʴ´�.
                mSvcThrRootStmt = NULL;
            }
            if ( mMeta.getReplMode() == RP_CONSISTENT_MODE )
            {
                IDE_TEST( mMessenger.sendFlush() != IDE_SUCCESS );
            }
            break;

        case RP_RECOVERY:
        case RP_OFFLINE:
            // Service Thread�� Statement�� �� �̻� �ʿ����� �ʴ�.
            mSvcThrRootStmt = NULL;
            break;

        case RP_START_CONDITIONAL: 
            // �� �̻� Service Thread�� ������� �ʴ´�.
            mSvcThrRootStmt = NULL;
            mCurrentType = RP_NORMAL;
            if ( mMeta.getReplMode() == RP_CONSISTENT_MODE )
            {
                IDE_TEST( mMessenger.sendFlush() != IDE_SUCCESS );
            }
            break;
                
        case RP_SYNC_CONDITIONAL: 
            if ( ( mSyncInsertItems != NULL ) ||
                 ( mSyncTruncateItems != NULL ) )
            {
                IDE_TEST( handshakeWithoutReconnect( SM_NULL_TID ) != IDE_SUCCESS );

                if ( mSyncInsertItems != NULL )
                {
                    IDE_TEST( syncStart() != IDE_SUCCESS );
                }

                if ( mSyncTruncateItems != NULL )
                {
                    IDE_TEST( truncateStart() != IDE_SUCCESS );
                }
            }
            // �� �̻� Service Thread�� ������� �ʴ´�.
            mSvcThrRootStmt = NULL;
            mCurrentType = RP_NORMAL;
            break;

        case RP_XLOGFILE_FAILBACK_MASTER:
        case RP_XLOGFILE_FAILBACK_SLAVE:
            IDE_TEST_RAISE( mMeta.getReplMode() != RP_CONSISTENT_MODE,
                            ERR_INVALID_OPTION );
            break;
        default:
            IDE_RAISE(ERR_INVALID_OPTION);
            break;
    }

    //fix BUG-12131
    mStartError = ID_FALSE;
    IDL_MEM_BARRIER;
    mStartComplete = ID_TRUE;


    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_OPTION);
    {
        IDE_SET(ideSetErrorCode(rpERR_FATAL_RP_SENDER_INVALID_OPTION));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl Sender] Invalid Option");
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_SYNC_WITH_V6_PROTOCOL )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_NOT_SUPPORT_FEATURE_WITH_V6_PROTOCOL,
                                  "Replication SYNC") );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_START));

    IDE_ERRLOG(IDE_RP_0);

    //fix BUG-12131
    mStartError = ID_TRUE;
    IDL_MEM_BARRIER;
    mStartComplete = ID_TRUE;
    return IDE_FAILURE;
}

//===================================================================
//
// Name:          quickStart
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:
//
// Called By:     rpxSender::execOnceAtStart()
//
// Description:
//
//===================================================================

IDE_RC
rpxSender::quickStart()
{
    return IDE_SUCCESS;
}

/****************************************************************************
* Description :
*   run status���� �ؾ��� �۾��� �����Ѵ�.
*   run status������ sender type���� �޼����� ����ϰ�
*   replication�� �ϱ����� ó���ؾ��� �۾��� ������ �� replication�� �����Ѵ�.
*****************************************************************************/
IDE_RC rpxSender::doRunning()
{
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);
    
    switch(mCurrentType)
    {
        case RP_RECOVERY:
            ideLog::log(IDE_RP_0, RP_TRC_S_RECOVERY_SENDER_START,
                        mMeta.mReplication.mRepName,
                        mParallelID,
                        getRestartSN());
            break;
        case RP_OFFLINE:
            ideLog::log(IDE_RP_0, RP_TRC_S_OFFLINE_SENDER_START,
                        mMeta.mReplication.mRepName,
                        mParallelID,
                        getRestartSN());
            break;
        case RP_NORMAL:
        case RP_PARALLEL:
            ideLog::log(IDE_RP_0, RP_TRC_S_SENDER_START,
                        mMeta.mReplication.mRepName,
                        mParallelID,
                        getRestartSN());
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    // BUG-22309
    if(mCurrentType != RP_RECOVERY)
    {
        /* PROJ-1608 recovery ������ standby�� ���� ���� �Ͽ����Ƿ�, invalid_recovery�� ����
         *  invalid_recovery�� default�� RP_CAN_RECOVERY�̹Ƿ�, recovery�� �������� �ʴ�
         *  replication�� �׻� �Ʒ� �Լ��� �����ص� ��������
         */
        IDE_TEST(updateInvalidRecovery(mRepName, RP_CAN_RECOVERY) != IDE_SUCCESS);
    }

    IDE_TEST(doReplication() != IDE_SUCCESS);

    // ���� ���ͷ�Ʈ�� �ɷ��ִ� ��쿡��,
    // Network/Apply ��ֿ� ���� ���� �޽����� ����Ѵ�.
    if((mRetryError == ID_TRUE) || (mApplyFaultFlag == ID_TRUE))
    {
        IDE_SET(ideSetErrorCode(rpERR_IGNORE_RP_SENDER_STOP,
                                mMeta.mReplication.mRepName,
                                mParallelID,
                                mXSN,
                                getRestartSN()));
        IDE_ERRLOG(IDE_RP_0);
    }

    RP_LABEL(NORMAL_EXIT);

    // ���� ���ͷ�Ʈ�� �ɷ��ִ� ��쿡��,
    // Network ��ְ� �߻��ϸ� Failback ���¸� �����ϴ� ���ڸ� �����Ѵ�.
    if((mRetryError == ID_TRUE) && (isParallelChild() != ID_TRUE))
    {
        mIsRemoteFaultDetect = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//===================================================================
//
// Name:          doReplication
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:
//
// Called By:     rpxSender::run()
//
// Description:
//
//===================================================================

IDE_RC
rpxSender::doReplication()
{
    while(checkInterrupt() == RP_INTR_NONE)
    {
        if ( mStatus != RP_SENDER_IDLE )
        {
            /* For Parallel Logging: Log Base�� Replication�� ���� */
            IDU_FIT_POINT( "rpxSender::doReplication::SLEEP::replicateLogFiles::mReplicator" );
            IDE_TEST( mReplicator.replicateLogFiles( RP_WAIT_ON_NOGAP,
                                                     RP_SEND_XLOG_ON_ADD_XLOG,
                                                     NULL,
                                                     NULL )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Statistics */
            idvManager::applyOpTimeToSession( &mStatSession, &mOpStatistics );
            idvManager::initRPSenderAccumTime( &mOpStatistics );

            mReplicator.sleepForKeepAlive();
        }

        IDE_TEST_RAISE( mIsServiceFail == ID_TRUE, ERR_DOREPLICATION );
    }

    IDU_FIT_POINT_RAISE( "1.TASK-2004@rpxSender::doReplication", 
                          ERR_DOREPLICATION );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DOREPLICATION );

    IDE_EXCEPTION_END;

    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_DO_REPLICATION));
    IDE_ERRLOG(IDE_RP_0);

    return IDE_FAILURE;
}

void rpxSender::sleepForNextConnect()
{
    IDE_TEST_CONT(mExitFlag == ID_TRUE, NORMAL_EXIT);

    mRetry++;

    // BUG-15507
    IDE_TEST_CONT((SInt)mRetry < mMeta.mReplication.mHostCount, NORMAL_EXIT);

    sleepForSenderSleepTime();

    // BUG-15507
    mRetry = 0;

    RP_LABEL(NORMAL_EXIT);

    return;
}


void rpxSender::checkXSNAndSleep()
{
    if ( mOldMaxXSN != SM_SN_NULL )
    {
        if ( mXSN <= mOldMaxXSN )
        {
            if ( (SInt)mRetry >= mMeta.mReplication.mHostCount )
            {
                sleepForSenderSleepTime();
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            mRetry = 0;
        }
    }
    else
    {
        /* Nothing to do */
    }
}


void rpxSender::sleepForSenderSleepTime()
{
    PDL_Time_Value  sSleepSec;

    IDE_TEST_CONT(mExitFlag == ID_TRUE, NORMAL_EXIT);

    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_SLEEP,
                            (UInt)RPU_REPLICATION_SENDER_SLEEP_TIMEOUT));
    IDE_ERRLOG(IDE_RP_0);
    IDE_CLEAR();

    sSleepSec.initialize();
    sSleepSec.set( RPU_REPLICATION_SENDER_SLEEP_TIMEOUT );

    mTvRetry = idlOS::gettimeofday() + sSleepSec;

    IDE_ASSERT(time_lock() == IDE_SUCCESS);
    if(mTimeCondRmt.timedwait(&mTimeMtxRmt, &mTvRetry, IDU_IGNORE_TIMEDOUT) != IDE_SUCCESS)
    {
        IDE_WARNING(IDE_RP_0, RP_TRC_S_ERR_SLP_COND_TIMEDWAIT);
    }
    IDE_ASSERT(time_unlock() == IDE_SUCCESS);
    
    RP_LABEL(NORMAL_EXIT);

    return;
}


IDE_RC
rpxSender::getNextLastUsedHostNo( SInt *aIndex )
{
    SInt              sOld   = 0;
    SInt              sIndex = 0;
    smiTrans          sTrans;
    SInt              sStage = 0;
    idBool            sIsTxBegin = ID_FALSE;
    smiStatement     *spRootStmt;
    //PROJ- 1677 DEQ
    UInt              sFlag = 0;
    UInt              sSenderInfoIdx;

    IDE_TEST(rpdCatalog::getIndexByAddr(mMeta.mReplication.mLastUsedHostNo,
                                     mMeta.mReplication.mReplHosts,
                                     mMeta.mReplication.mHostCount,
                                     &sIndex)
             != IDE_SUCCESS);

    IDE_ASSERT(sIndex < mMeta.mReplication.mHostCount);

    sOld = sIndex;

    if(++sIndex >= mMeta.mReplication.mHostCount)
    {
        sIndex = 0;
    }

    if(aIndex != NULL)
    {
        *aIndex = sIndex;
    }

    /* Service Thread�� Transaction���� �����ǰ� �ִ� ���,
     * Service Thread�� Transaction�� ����Ѵ�.
     */
    if(mSvcThrRootStmt != NULL)
    {
        spRootStmt = mSvcThrRootStmt;

        IDE_TEST(rpcManager::updateLastUsedHostNo(spRootStmt,
                        mMeta.mReplication.mRepName,
                        mMeta.mReplication.mReplHosts[sIndex].mHostIp,
                        mMeta.mReplication.mReplHosts[sIndex].mPortNo)
                 != IDE_SUCCESS);
        mMeta.mReplication.mLastUsedHostNo = mMeta.mReplication.mReplHosts[sIndex].mHostNo;
    }
    else
    {
        IDE_TEST(sTrans.initialize() != IDE_SUCCESS);
        sStage = 1;

        sFlag = (sFlag & ~SMI_ISOLATION_MASK) | (UInt)RPU_ISOLATION_LEVEL;
        sFlag = (sFlag & ~SMI_TRANSACTION_MASK) | SMI_TRANSACTION_NORMAL;
        sFlag = (sFlag & ~SMI_TRANSACTION_REPL_MASK) | SMI_TRANSACTION_REPL_NONE;
        sFlag = (sFlag & ~SMI_COMMIT_WRITE_MASK) | SMI_COMMIT_WRITE_NOWAIT;

        IDE_TEST(sTrans.begin(&spRootStmt, NULL, sFlag, SMX_NOT_REPL_TX_ID)
                 != IDE_SUCCESS);
        sIsTxBegin = ID_TRUE;
        sStage = 2;

        IDE_TEST(rpcManager::updateLastUsedHostNo(spRootStmt,
                        mMeta.mReplication.mRepName,
                        mMeta.mReplication.mReplHosts[sIndex].mHostIp,
                        mMeta.mReplication.mReplHosts[sIndex].mPortNo)
                 != IDE_SUCCESS);
        mMeta.mReplication.mLastUsedHostNo = mMeta.mReplication.mReplHosts[sIndex].mHostNo;

        sStage = 1;
        IDE_TEST(sTrans.commit() != IDE_SUCCESS);
        sIsTxBegin = ID_FALSE;

        sStage = 0;
        IDE_TEST(sTrans.destroy( NULL ) != IDE_SUCCESS);
    }

    if ( isParallelParent() == ID_TRUE )
    {
        for ( sSenderInfoIdx = 0; sSenderInfoIdx < RPU_REPLICATION_EAGER_PARALLEL_FACTOR; sSenderInfoIdx++ )
        {
            mSenderInfoArray[sSenderInfoIdx].increaseReconnectCount();
        }
    }
    else
    {
        /* Nothing to do */
    }

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_NLU_HOST_NO,
                          mMeta.mReplication.mReplHosts[sOld].mHostIp,
                          mMeta.mReplication.mReplHosts[sOld].mPortNo,
                          mMeta.mReplication.mReplHosts[sIndex].mHostIp,
                          mMeta.mReplication.mReplHosts[sIndex].mPortNo);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 2:
            IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
            sIsTxBegin = ID_FALSE;
        case 1:
            if(sIsTxBegin == ID_TRUE)
            {
                IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
                sIsTxBegin = ID_FALSE;
            }
            (void)sTrans.destroy( NULL );
        default:
            break;
    }

    return IDE_FAILURE;
}


/*----------------------------------------------------------------------------
 * Name :
 *     rpxSender::initReadLogCount()
 *
 * Argument :
 *
 * Description :
 *     initialize V$REPSENDER Performance View's Count
 *
 *----------------------------------------------------------------------------*/
void rpxSender::initReadLogCount()
{
    mReplicator.resetReadLogCount();
    mReplicator.resetSendLogCount();
}

/*
 *
 */
ULong rpxSender::getReadLogCount( void )
{
    return mReplicator.getReadLogCount();
}

/*
 *
 */
ULong rpxSender::getSendLogCount( void )
{
    return mReplicator.getSendLogCount();
}

ULong rpxSender::getSendDataSize( void )
{
    return mMessenger.getSendDataSize();
}

ULong rpxSender::getSendDataCount( void )
{
    return mMessenger.getSendDataCount();
}

/*----------------------------------------------------------------------------
Name:
    waitComplete()
Description:
    �ٸ� ������ alter replication sync�� �����Ͽ� sender�� startComplete
    flag�� �˻��ϴ� ���� sender�� free�ϴ� ���� ���� ���� ���
    �޸� free�� �߻��ϱ� ���� �׻� ����Ǿ�� ��
*-----------------------------------------------------------------------------*/
IDE_RC
rpxSender::waitComplete( idvSQL     * aStatistics )
{
    while(mCheckingStartComplete != ID_FALSE)
    {
        PDL_Time_Value sPDL_Time_Value;
        sPDL_Time_Value.initialize(1, 0);
        idlOS::sleep(sPDL_Time_Value);

        IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
rpxSender::startSenderApply()
{
    idBool sIsLocked = ID_FALSE;
    idBool sIsSupportRecovery = ID_FALSE;

    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    IDE_ASSERT(final_lock() == IDE_SUCCESS);
    sIsLocked = ID_TRUE;

    IDU_FIT_POINT_RAISE( "rpxSender::startSenderApply::malloc::SenderApply",
                          ERR_MEMORY_ALLOC_SENDER_APPLY );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPX_SENDER,
                                     ID_SIZEOF(rpxSenderApply),
                                     (void**)&mSenderApply,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_SENDER_APPLY);

    new (mSenderApply) rpxSenderApply;

    if((mMeta.mReplication.mOptions & RP_OPTION_RECOVERY_MASK)
       == RP_OPTION_RECOVERY_SET)
    {
        sIsSupportRecovery = ID_TRUE;
    }
    IDE_TEST_RAISE(mSenderApply->initialize(this,
                                            &mOpStatistics,
                                            &mStatSession,
                                            &mSenderInfoArray[mParallelID],
                                            &mMessenger,
                                            &mMeta,
                                            mRsc,
                                            &mApplyRetryCount,
                                            &mRetryError,
                                            &mApplyFaultFlag,
                                            &mExitFlag,
                                            sIsSupportRecovery,
                                            &mCurrentType,//PROJ-1915
                                            mParallelID,
                                            &mStatus,
                                            mSocketType)
                   != IDE_SUCCESS, APPLY_INIT_ERR);

    IDU_FIT_POINT_RAISE( "rpxSender::startSenderApply::Thread::mSenderApply",
                         APPLY_START_ERR,
                         idERR_ABORT_THR_CREATE_FAILED,
                         "rpxSender::startSenderApply",
                         "mSenderApply" );
    IDE_TEST_RAISE(mSenderApply->start() != IDE_SUCCESS, APPLY_START_ERR);

    sIsLocked = ID_FALSE;
    IDE_ASSERT(final_unlock() == IDE_SUCCESS);

    RP_LABEL(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MEMORY_ALLOC_SENDER_APPLY);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::startSenderApply",
                                "mSenderApply"));
    }
    IDE_EXCEPTION(APPLY_START_ERR);
    {
        mSenderApply->destroy();
    }
    IDE_EXCEPTION(APPLY_INIT_ERR);
    {
        //no action required
    }
    IDE_EXCEPTION_END;
    IDE_PUSH();

    if(sIsLocked == ID_TRUE)
    {
        (void)final_unlock();
    }

    if(mSenderApply != NULL)
    {
        (void)iduMemMgr::free(mSenderApply);
        mSenderApply = NULL;
    }

    IDE_POP();
    return IDE_FAILURE;
}
void rpxSender::finalizeSenderApply()
{
    if(mSenderApply != NULL)
    {
        if(mSenderApply->join() != IDE_SUCCESS)
        {
            IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_JOIN_THREAD));
            IDE_ERRLOG(IDE_RP_0);
            IDE_CALLBACK_FATAL("[Repl SenderApply] Thread join error");
        }

        mSenderApply->destroy();
        (void)iduMemMgr::free(mSenderApply);
        mSenderApply = NULL;
    }

    return;
}
void rpxSender::shutdownSenderApply()
{
    if(mSenderApply != NULL)
    {
        mSenderApply->shutdown();
    }
    finalizeSenderApply();
}

IDE_RC rpxSender::buildMeta(smiStatement   * aSmiStmt,
                            SChar          * aRepName,
                            RP_SENDER_TYPE   aStartType,
                            idBool           aMetaForUpdateFlag,
                            rpdMeta        * aMeta)
{
    RP_META_BUILD_TYPE   sMetaBuildType = RP_META_BUILD_AUTO;
    SInt                 sTC;             // Table Count
    SInt                 sCC;             // Column Count
    rpdMeta            * sMeta          = NULL;

    switch(aStartType)
    {
        case RP_NORMAL :
        case RP_START_CONDITIONAL :
            sMetaBuildType = RP_META_BUILD_AUTO;
            break;

        case RP_QUICK :
        case RP_SYNC :
        case RP_SYNC_ONLY :
        case RP_SYNC_CONDITIONAL :
            sMetaBuildType = RP_META_BUILD_LAST;
            break;

        case RP_XLOGFILE_FAILBACK_SLAVE :
        case RP_XLOGFILE_FAILBACK_MASTER :
        case RP_RECOVERY :
            sMetaBuildType = RP_META_BUILD_LAST;
            break;

        /*offline sender�� meta�� build���� �ʰ� �����Ѵ�.*/
        case RP_OFFLINE :
            sMetaBuildType = RP_META_NO_BUILD;
            sMeta = mRemoteMeta;
            break;

        /*parallel sender*/
        case RP_PARALLEL :
            if(isParallelChild() == ID_TRUE)
            {
                sMeta = aMeta;
                sMetaBuildType = RP_META_NO_BUILD;
            }
            else
            {
                // buildMeta�� �Ҹ��� initialize�������� parallel manager��
                // normal type���� ���´�.
                IDE_DASSERT(0);
            }
            break;

        default:
            IDE_ASSERT(0);
    }

    IDE_DASSERT( sMetaBuildType == getMetaBuildType( aStartType, mParallelID ) );

    if(sMetaBuildType != RP_META_NO_BUILD)
    {
        //fix BUG-9371
        IDE_TEST_RAISE(mMeta.build( aSmiStmt,
                                    aRepName,
                                    aMetaForUpdateFlag,
                                    sMetaBuildType,
                                    SMI_TBSLV_DDL_DML )
                       != IDE_SUCCESS, ERR_META_BUILD);
    }
    else
    {
        IDE_DASSERT(sMeta != NULL);
        IDE_TEST(sMeta->copyMeta(&mMeta) != IDE_SUCCESS);
    }

    mMeta.mReplication.mParallelID = mParallelID;
    // Meta�� ���� �� �̹� IS_LOCK�� ��Ƽ�, DDL�� ������ �� ����.
    switch(aStartType)
    {
        case RP_NORMAL :
        case RP_START_CONDITIONAL :
        case RP_SYNC_CONDITIONAL :
            // ������ Meta�� ������, �ֽ� Meta�� �����Ѵ�.
            if(mMeta.mReplication.mXSN == SM_SN_NULL)
            {
                IDE_TEST(rpdMeta::insertOldMetaRepl(aSmiStmt, &mMeta)
                         != IDE_SUCCESS);
            }
            break;

        case RP_QUICK :
            // ������ Meta�� ������, ������ Meta�� �����Ѵ�.
            if(mMeta.mReplication.mXSN != SM_SN_NULL)
            {
                IDE_TEST(rpdMeta::removeOldMetaRepl(aSmiStmt, aRepName)
                         != IDE_SUCCESS);
            }

            // �ֽ� Meta�� �����Ѵ�.
            IDE_TEST(rpdMeta::insertOldMetaRepl(aSmiStmt, &mMeta)
                     != IDE_SUCCESS);
            break;

        case RP_SYNC :
        case RP_SYNC_ONLY :
            // Sync ��� Item�� �����ǰ� ������ Meta�� �ִ� ���
            if((mSyncInsertItems != NULL) && (mMeta.mReplication.mXSN != SM_SN_NULL))
            {
                IDE_TEST( rebuildWithUpdateOldMeta( aSmiStmt,
                                                    aRepName,
                                                    aMetaForUpdateFlag,
                                                    &mMeta,
                                                    mSyncInsertItems )
                          != IDE_SUCCESS);
            }
            // ���� Meta�� ������ ������� �ֽ� Meta�� �ʿ��� ���
            else
            {
                // ������ Meta�� ������, ������ Meta�� �����Ѵ�.
                if(mMeta.mReplication.mXSN != SM_SN_NULL)
                {
                    IDE_TEST(rpdMeta::removeOldMetaRepl(aSmiStmt, aRepName)
                             != IDE_SUCCESS);
                }

                // �ֽ� Meta�� �����Ѵ�.
                IDE_TEST(rpdMeta::insertOldMetaRepl(aSmiStmt, &mMeta)
                         != IDE_SUCCESS);
            }
            break;

        case RP_RECOVERY :
        case RP_XLOGFILE_FAILBACK_MASTER :
        case RP_XLOGFILE_FAILBACK_SLAVE :

            break;

        case RP_OFFLINE :
            IDE_TEST_RAISE( mMeta.mDictTableCount > 0, ERR_OFFLINE_WITH_COMPRESS );

            mMeta.mReplication.mHostCount = 1;

            IDU_FIT_POINT_RAISE( "rpxSender::buildMeta::calloc::ReplHosts",
                                  ERR_MEMORY_ALLOC_HOST );
            IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_RP_RPD_META,
                           mMeta.mReplication.mHostCount,
                           ID_SIZEOF(rpdReplHosts),
                           (void **)&mMeta.mReplication.mReplHosts,
                           IDU_MEM_IMMEDIATE)
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_HOST);

            idlOS::memcpy(mMeta.mReplication.mReplHosts[0].mRepName,
                          mMeta.mReplication.mRepName,
                          QCI_MAX_NAME_LEN + 1);

            for(sTC = 0; sTC < mMeta.mReplication.mItemCount; sTC++)
            {
                //BUG-28014 : off-line replicator DISK ���̺� ��� module�� �ʿ� �մϴ�.
                for(sCC = 0; sCC < mMeta.mItems[sTC].mColCount; sCC++)
                {
                    IDU_FIT_POINT_RAISE( "rpxSender::buildMeta::Erratic::rpERR_ABORT_GET_MODULE",
                                         ERR_GET_MODULE );
                    IDE_TEST_RAISE(mtd::moduleById(
                                        &mMeta.mItems[sTC].mColumns[sCC].mColumn.module,
                                        mMeta.mItems[sTC].mColumns[sCC].mColumn.type.dataTypeId)
                                   != IDE_SUCCESS, ERR_GET_MODULE);
                }
            }
            break;

        case RP_PARALLEL :
            if(isParallelChild() == ID_TRUE)
            {
                mMeta.mReplication.mXSN = aMeta->mChildXSN;
            }
            else
            {
                // buildMeta�� �Ҹ��� initialize�������� parallel manager��
                // normal type���� �����Ƿ�, �� �κ��� Ż �� ����.
                IDE_DASSERT(0);
            }
            break;

        default:
            IDE_ASSERT(0);
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_BUILD);
    {
        mMeta.finalize();
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_HOST);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                "rpxSender::initialize",
                "mMeta.mReplication.mReplHosts"));
    }
    IDE_EXCEPTION(ERR_GET_MODULE);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_GET_MODULE));
    }
    IDE_EXCEPTION(ERR_OFFLINE_WITH_COMPRESS);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RPC_NOT_SUPPORT_OFFLINE_REPLICATE_TABLE_WITH_COMPRESSED_COLUMN));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::rebuildWithUpdateOldMeta( smiStatement    * aSmiStmt,
                                            SChar           * aRepName,
                                            idBool            aMetaForUpdateFlag,
                                            rpdMeta         * aMeta,
                                            rpdReplSyncItem * aSyncItemList )
{
    rpdReplSyncItem    * sSyncItem      = NULL;
    SInt                 sIndex;
    rpdMetaItem        * sItem          = NULL;

    sSyncItem = aSyncItemList;
    while( sSyncItem != NULL )
    {
        /* PROJ-2366 partition case 
         * ������ Item�� Meta�� �����Ѵ�.
         * syncItem�� ��� partition������ ������ ������ ������ ���ڴ� PARTITION UNIT�̴�.
         * Ȥ�� PARTITIONED TABLE�� �ƴ� ��쿡�� partitionName�� syncItem�� �Ͱ� select�ؿ� �� ���
         * \0�̱� ������ �������.
         */
        IDE_TEST( rpdMeta::deleteOldMetaItems( aSmiStmt,
                                               aRepName,
                                               sSyncItem->mUserName,
                                               sSyncItem->mTableName,
                                               sSyncItem->mPartitionName,
                                               RP_REPLICATION_PARTITION_UNIT /*aReplUnit*/ )
                 != IDE_SUCCESS);

        sSyncItem = sSyncItem->next;
    }

    // SYNC ��� Item�� Meta�� �����Ѵ�.
    for( sIndex = 0;
         sIndex < aMeta->mReplication.mItemCount;
         sIndex++ )
    {
        sItem = aMeta->mItemsOrderByTableOID[sIndex];

        // Replication Table�� ������ Sync Table�� ��쿡�� ��� �����Ѵ�.
        if( isSyncItem( aSyncItemList,
                        sItem->mItem.mLocalUsername,
                        sItem->mItem.mLocalTablename,
                        sItem->mItem.mLocalPartname) != ID_TRUE )
        {
            continue;
        }

        // Item�� �ֽ� Meta�� �����Ѵ�.
        IDE_TEST( rpdMeta::insertOldMetaItem(aSmiStmt, sItem)
                  != IDE_SUCCESS );
    }

    // �ֽ� Meta�� �����ϰ� ������ Meta�� ��´�.
    aMeta->finalize();
    aMeta->initialize();

    IDE_TEST_RAISE( aMeta->build( aSmiStmt,
                                  aRepName,
                                  aMetaForUpdateFlag,
                                  RP_META_BUILD_OLD,
                                  SMI_TBSLV_DDL_DML )
                    != IDE_SUCCESS, ERR_META_BUILD );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_BUILD);
    {
        aMeta->finalize();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::updateInvalidRecovery(SChar* aRepName, SInt aValue)
{
    smiTrans          sTrans;
    SInt              sStage = 0;
    idBool            sIsTxBegin = ID_FALSE;
    smiStatement     *spRootStmt;
    UInt              sFlag = 0;

    if(isParallelChild() == ID_TRUE)
    {
        return IDE_SUCCESS;
    }

    IDE_TEST(sTrans.initialize() != IDE_SUCCESS);
    sStage = 1;

    sFlag = (sFlag & ~SMI_ISOLATION_MASK) | (UInt)RPU_ISOLATION_LEVEL;
    sFlag = (sFlag & ~SMI_TRANSACTION_MASK) | SMI_TRANSACTION_NORMAL;
    sFlag = (sFlag & ~SMI_TRANSACTION_REPL_MASK) | SMI_TRANSACTION_REPL_NONE;
    sFlag = (sFlag & ~SMI_COMMIT_WRITE_MASK) | SMI_COMMIT_WRITE_WAIT;

    IDE_TEST(sTrans.begin(&spRootStmt, NULL, sFlag, SMX_NOT_REPL_TX_ID )
             != IDE_SUCCESS);
    sIsTxBegin = ID_TRUE;
    sStage = 2;

    IDU_FIT_POINT( "rpxSender::updateInvalidRecovery::Erratic::rpERR_ABORT_RP_SENDER_UPDATE_INVALID_MAX_SN" ); 
    IDE_TEST(rpcManager::updateInvalidRecovery(spRootStmt, aRepName, aValue)
             != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sTrans.commit() != IDE_SUCCESS);
    sIsTxBegin = ID_FALSE;

    sStage = 0;
    IDE_TEST(sTrans.destroy( NULL ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 2:
            IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
            sIsTxBegin = ID_FALSE;
        case 1:
            if(sIsTxBegin == ID_TRUE)
            {
                IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
                sIsTxBegin = ID_FALSE;
            }
            (void)sTrans.destroy( NULL );
        default:
            break;
    }

    IDE_ERRLOG(IDE_RP_0);
    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_UPDATE_INVALID_MAX_SN));

    return IDE_FAILURE;
}

IDE_RC
rpxSender::waitThreadJoin(idvSQL *aStatistics)
{
    PDL_Time_Value sTvCpu;
    PDL_Time_Value sPDL_Time_Value;
    idBool         sIsLock = ID_FALSE;
    UInt           sTimeoutMilliSec = 0;

    sPDL_Time_Value.initialize(0, 1000);

    IDE_ASSERT(threadJoinMutex_lock() == IDE_SUCCESS);
    sIsLock = ID_TRUE;

    while(mIsThreadDead != ID_TRUE)
    {
        sTvCpu  = idlOS::gettimeofday();
        sTvCpu += sPDL_Time_Value;

        (void)mThreadJoinCV.timedwait(&mThreadJoinMutex, &sTvCpu);

        if ( aStatistics != NULL )
        {
            // BUG-22637 MM���� QUERY_TIMEOUT, Session Closed�� �����ߴ��� Ȯ���Ѵ�
            IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
        }
        else
        {
            // 5 Sec
            IDE_TEST_RAISE( sTimeoutMilliSec >= 5000, ERR_TIMEOUT );
            sTimeoutMilliSec++;
        }
    }

    sIsLock = ID_FALSE;
    IDE_ASSERT(threadJoinMutex_unlock() == IDE_SUCCESS);

    if(join() != IDE_SUCCESS)
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_JOIN_THREAD));
        IDE_ERRLOG(IDE_RP_0);
        IDE_CALLBACK_FATAL("[Repl Sender] Thread join error");
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TIMEOUT )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "rpxSender::waitThreadJoin exceeds timeout" ) );
    }
    IDE_EXCEPTION_END;

    if(sIsLock == ID_TRUE)
    {
        IDE_ASSERT(threadJoinMutex_unlock() == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

void rpxSender::signalThreadJoin()
{
    IDE_ASSERT(threadJoinMutex_lock() == IDE_SUCCESS);

    mIsThreadDead = ID_TRUE;
    IDE_ASSERT(mThreadJoinCV.signal() == IDE_SUCCESS);

    IDE_ASSERT(threadJoinMutex_unlock() == IDE_SUCCESS);

    return;
}

/* PROJ-1915 RemoteLog�� ������ SN�� ���� smrRemoteLFGMgr���� �ѹ� ���� �� ���� ���� �Ѵ�.*/
IDE_RC rpxSender::getRemoteLastUsedGSN(smSN * aSN)
{
    IDE_TEST( mReplicator.getRemoteLastUsedGSN( aSN ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* PROJ-1915 RemoteLog ��� ���� ���� ���� */
IDE_RC rpxSender::checkOffLineLogInfo()
{
    UInt   sCompileBit;
    SChar  sOSInfo[QC_MAX_NAME_LEN + 1];
    UInt   sSmVersionID;
    UInt   sSmVer1;
    UInt   sSmVer2;

#ifdef COMPILE_64BIT
    sCompileBit = 64;
#else
    sCompileBit = 32;
#endif

    UInt   sLFGID;

    sSmVersionID = smiGetSmVersionID();

    idlOS::snprintf(sOSInfo,
                    ID_SIZEOF(sOSInfo),
                    "%s %"ID_INT32_FMT" %"ID_INT32_FMT"",
                    OS_TARGET,
                    OS_MAJORVER,
                    OS_MINORVER);

    IDU_FIT_POINT_RAISE( "rpxSender::checkOffLineLogInfo::Erratic::rpERR_ABORT_MISMATCH_OFFLINE_LOG_LFGCOUNT",
                         ERR_LFGCOUNT_MISMATCH ); 
    IDE_TEST_RAISE(mRemoteLFGCount != mMeta.mReplication.mLFGCount,
                   ERR_LFGCOUNT_MISMATCH);

    IDU_FIT_POINT_RAISE( "rpxSender::checkOffLineLogInfo::Erratic::rpERR_ABORT_MISMATCH_OFFLINE_LOG_COMPILEBIT",
                         ERR_COMPILEBIT_MISMATCH ); 
    IDE_TEST_RAISE(sCompileBit != mMeta.mReplication.mCompileBit,
                   ERR_COMPILEBIT_MISMATCH);

    //sm Version �� ����ũ �ؼ� �˻� �Ѵ�.
    sSmVer1 = sSmVersionID & SM_CHECK_VERSION_MASK;
    sSmVer2 = mMeta.mReplication.mSmVersionID & SM_CHECK_VERSION_MASK;

    IDU_FIT_POINT_RAISE( "rpxSender::checkOffLineLogInfo::Erratic::rpERR_ABORT_MISMATCH_OFFLINE_LOG_SMVERSION",
                         ERR_SMVERSION_MISMATCH );
    IDE_TEST_RAISE(sSmVer1 != sSmVer2,
                   ERR_SMVERSION_MISMATCH);

    IDU_FIT_POINT_RAISE( "rpxSender::checkOffLineLogInfo::Erratic::rpERR_ABORT_MISMATCH_OFFLINE_LOG_OSVERSION",
                         ERR_OSVERSION_MISMATCH );
    IDE_TEST_RAISE(idlOS::strcmp(sOSInfo, mMeta.mReplication.mOSInfo) != 0,
                   ERR_OSVERSION_MISMATCH);

    for (sLFGID = 0; sLFGID < mRemoteLFGCount; sLFGID++)
    {
        IDE_TEST_RAISE(idf::access(mLogDirPath[sLFGID], F_OK) != 0,
                       ERR_LOGDIR_NOT_EXIST);
    }

    IDE_TEST_RAISE(smiGetLogFileSize() != mMeta.mReplication.mLogFileSize,
                   ERR_LOGFILESIZE_MISMATCH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOGDIR_NOT_EXIST);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, mLogDirPath[sLFGID]));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION(ERR_LFGCOUNT_MISMATCH);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MISMATCH_OFFLINE_LOG_LFGCOUNT));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION(ERR_OSVERSION_MISMATCH);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MISMATCH_OFFLINE_LOG_OSVERSION));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION(ERR_SMVERSION_MISMATCH);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MISMATCH_OFFLINE_LOG_SMVERSION));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION(ERR_COMPILEBIT_MISMATCH);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MISMATCH_OFFLINE_LOG_COMPILEBIT));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION(ERR_LOGFILESIZE_MISMATCH);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MISMATCH_OFFLINE_LOG_FILESIZE));
        IDE_ERRLOG(IDE_RP_0);
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void rpxSender::getMinRestartSNFromAllApply( smSN* aRestartSN )
{
    UInt i = 0;

    smSN sTmpSN = SM_SN_NULL;
    smSN sMinRestartSN = *aRestartSN;

    if ( mChildArray != NULL )
    {
        for ( i = 0; i < mChildCount; i++ )
        {
            sTmpSN = mChildArray[i].mSenderApply->getMinRestartSN();
            if( sTmpSN < sMinRestartSN )
            {
                sMinRestartSN = sTmpSN;
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

    *aRestartSN = sMinRestartSN;

    return;
}

/***********************************************************************
 *  Description:
 *
 *    checkpoint thread�� ���� ȣ��Ǹ�, ���� redo log�� �а� �ְ�
 *    give-up��Ȳ�� ��� archive log�� ��ȯ�� ������ �Ǵ��Ѵ�. �̶�
 *    mLogSwitchMtx lock�� ȹ���Ͽ� redo log ������ �а� ���� �ʴ�
 *    ���� �����Ѵ�.
 **********************************************************************/
void rpxSender::checkAndSetSwitchToArchiveLogMgr(const UInt  * aLastArchiveFileNo,
                                                 idBool      * aSetLogMgrSwitch)
{

    mReplicator.checkAndSetSwitchToArchiveLogMgr( aLastArchiveFileNo,
                                                  aSetLogMgrSwitch );
}

idBool rpxSender::isFailbackComplete(smSN aLastSN)
{
    smSN sLastProcessedSN;

    sLastProcessedSN = getLastProcessedSN();

    if((sLastProcessedSN != SM_SN_NULL) && (sLastProcessedSN >= aLastSN))
    {
        // gap is 0
        return ID_TRUE;
    }
    return ID_FALSE;
}

IDE_RC rpxSender::removeReplGapOnLazyMode()
{
    // Lazy Mode�� Replication Gap�� �����Ѵ�.
    /* For Parallel Logging: Log Base�� Replication�� ���� */
    IDE_TEST( mReplicator.replicateLogFiles( RP_RETURN_ON_NOGAP,
                                             RP_SEND_XLOG_ON_ADD_XLOG,
                                             NULL,
                                             NULL )
              != IDE_SUCCESS );
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    if ( mMeta.mReplication.mReplMode != RP_CONSISTENT_MODE)
    {
        // Eager Mode�� ��ȯ�Ѵ�.
        mSenderInfo->setSenderStatus(RP_SENDER_FAILBACK_EAGER);
    }
    else
    {
        setStatus( RP_SENDER_RUN );
    }

    IDU_FIT_POINT( "rpxSender::failbackNormal::SLEEP::failbackEager" );

    // Eager/Consistent Mode�� Replication Gap�� �����Ѵ�.
    while(checkInterrupt() == RP_INTR_NONE)
    {
        /* For Parallel Logging: Log Base�� Replication�� ���� */
        IDE_TEST( mReplicator.replicateLogFiles( RP_RETURN_ON_NOGAP,
                                                 RP_SEND_XLOG_ON_ADD_XLOG,
                                                 NULL,
                                                 NULL )
                  != IDE_SUCCESS );

        if(isFailbackComplete(mXSN) == ID_TRUE)
        {
            break;
        }
    }
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    RP_LABEL(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::failbackNormal()
{
    IDE_DASSERT( ( ( getMode() == RP_EAGER_MODE ) && ( isParallelParent() == ID_TRUE ) ) ||
                 ( getMode() == RP_CONSISTENT_MODE ) );

    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_NOMRAL_BEGIN,
                          mMeta.mReplication.mRepName,
                          mXSN);

    IDE_TEST( removeReplGapOnLazyMode() != IDE_SUCCESS );

    if(checkInterrupt() == RP_INTR_NONE)
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_SUCCEED,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }
    else
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }
    
    RP_LABEL(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                          mMeta.mReplication.mRepName,
                          mXSN);

    return IDE_FAILURE;
}

IDE_RC rpxSender::incrementalSyncMaster()
{
    rpdSyncPKEntry *sSyncPKEntry    = NULL;
    idBool          sIsSyncPKBegin  = ID_FALSE;
    idBool          sIsSyncPKSent   = ID_FALSE;
    idBool          sIsSyncPKEnd    = ID_FALSE;

    PDL_Time_Value  sTimeValue;
    UInt            sFailbackWaitTime = 0;

    UInt            sMaxPkColCount = 0;

    sTimeValue.initialize(1, 0);

    sMaxPkColCount = mMeta.getMaxPkColCountInAllItem();
    IDE_DASSERT( sMaxPkColCount != 0 );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                       sMaxPkColCount,
                                       ID_SIZEOF(qriMetaRangeColumn),
                                       (void **)&mRangeColumn,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_RANGE_COLUMN );
    mRangeColumnCount = sMaxPkColCount;

    // Peer Server���� Incremental Sync Primary Key�� �����Ͽ�,
    // �ش� Row�� Select & Send�Ѵ�.
    while((checkInterrupt() == RP_INTR_NONE) && (sIsSyncPKEnd != ID_TRUE))
    {
        if ( sFailbackWaitTime >= RPU_REPLICATION_RECEIVE_TIMEOUT )
        {
            mRetryError = ID_TRUE;
            mSenderInfo->checkAndRunDeactivate();

            IDE_CONT( NORMAL_EXIT );
        }

        if(sSyncPKEntry == NULL)
        {
            mSenderInfo->getFirstSyncPKEntry(&sSyncPKEntry);
        }

        if(sSyncPKEntry != NULL)
        {
            switch(sSyncPKEntry->mType)
            {
                case RP_SYNC_PK_BEGIN :
                    if(sIsSyncPKSent == ID_TRUE)
                    {
                        // Rollback�� �����Ͽ� ���� ���̴� Transaction�� ����Ѵ�.
                        IDE_TEST(addXLogSyncAbort() != IDE_SUCCESS);
                        sIsSyncPKSent = ID_FALSE;
                    }

                    sIsSyncPKBegin = ID_TRUE;
                    break;

                case RP_SYNC_PK :
                    if(sIsSyncPKBegin == ID_TRUE)
                    {
                        // �ش� Row�� ������ Delete & Insert�� ó���ϰ�, ������ Delete�� ó���Ѵ�.
                        IDE_TEST(addXLogSyncRow(sSyncPKEntry) != IDE_SUCCESS);
                        sIsSyncPKSent = ID_TRUE;
                    }
                    break;

                case RP_SYNC_PK_END :
                            
                    if(sIsSyncPKBegin == ID_TRUE)
                    {
                        // �������� �ƴ� �� �����Ƿ�, Queue���� �ϳ� �� �о�´�.
                        mSenderInfo->removeSyncPKEntry(sSyncPKEntry);
                        sSyncPKEntry = NULL;
                        mSenderInfo->getFirstSyncPKEntry(&sSyncPKEntry);
                        
                        if(sIsSyncPKSent == ID_TRUE)
                        {
                            // Commit�� �����Ͽ� ���� ���̴� Transaction�� �Ϸ��Ѵ�.
                            IDE_TEST(addXLogSyncCommit() != IDE_SUCCESS);
                        }

                        // �� �̻� ������, ���� �ܰ�� �Ѿ��.
                        if(sSyncPKEntry == NULL)
                        {
                            sIsSyncPKEnd = ID_TRUE;
                        }
                        else
                        {
                            sIsSyncPKBegin = ID_FALSE;
                            sIsSyncPKSent = ID_FALSE;
                        }

                        continue;
                    }

                    break;

                default :
                    IDE_ASSERT(0);
            } // switch

            mSenderInfo->removeSyncPKEntry(sSyncPKEntry);
            sSyncPKEntry = NULL;
            sFailbackWaitTime = 0;
        }
        else
        {
            IDE_TEST(addXLogKeepAlive() != IDE_SUCCESS);
            idlOS::sleep(sTimeValue);
            sFailbackWaitTime++;
        }
    } // while

    if ( sIsSyncPKEnd == ID_TRUE )
    {
        IDE_TEST( mSenderInfo->destroySyncPKPool( ID_TRUE ) != IDE_SUCCESS );
    }

    if ( mRangeColumn != NULL )
    {
        (void)iduMemMgr::free( mRangeColumn );
        mRangeColumn = NULL;
        mRangeColumnCount = 0;
    }
    else
    {
        /* do nothing */
    }
    
    RP_LABEL(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_RANGE_COLUMN );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::failbackMaster",
                                  "mRangeColumn" ) );
    }
    IDE_EXCEPTION_END;

    if(sSyncPKEntry != NULL)
    {
        mSenderInfo->removeSyncPKEntry(sSyncPKEntry);
    }

    if ( mRangeColumn != NULL )
    {
        (void)iduMemMgr::free( mRangeColumn );
        mRangeColumn = NULL;
        mRangeColumnCount = 0;
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;
}

IDE_RC rpxSender::failbackMaster()
{
    IDE_DASSERT( ( ( getMode() == RP_EAGER_MODE ) && ( isParallelParent() == ID_TRUE ) ) ||
                 ( getMode() == RP_CONSISTENT_MODE ) );

    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_MASTER_BEGIN,
                          mMeta.mReplication.mRepName,
                          mXSN);

    IDE_TEST( incrementalSyncMaster() != IDE_SUCCESS );

    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    if ( getMode() == RP_EAGER_MODE )
    {
        IDE_TEST( removeReplGapOnLazyMode() != IDE_SUCCESS );
    }
    
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // Peer Server�� Failback �ϷḦ �˸���.
    IDE_TEST(addXLogFailbackEnd() != IDE_SUCCESS);

    RP_LABEL(NORMAL_EXIT);

    if(checkInterrupt() == RP_INTR_NONE)
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_SUCCEED,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }
    else
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                          mMeta.mReplication.mRepName,
                          mXSN);

    return IDE_FAILURE;
}

IDE_RC rpxSender::failbackSlave()
{
    // Committed Transaction�� Begin SN ����
    iduMemPool     sBeginSNPool;
    idBool         sBeginSNPoolInit = ID_FALSE;
    iduList        sBeginSNList;

    PDL_Time_Value sTimeValue;
    sTimeValue.initialize(1, 0);

    IDE_DASSERT( ( ( getMode() == RP_EAGER_MODE ) && ( isParallelParent() == ID_TRUE ) ) ||
                 ( getMode() == RP_CONSISTENT_MODE ) );

    IDE_TEST(sBeginSNPool.initialize(IDU_MEM_RP_RPX_SENDER,
                                     (SChar *)"RP_BEGIN_SN_POOL",
                                     1,
                                     ID_SIZEOF(rpxSNEntry),
                                     smiGetTransTblSize(),
                                     IDU_AUTOFREE_CHUNK_LIMIT, //chunk max(default)
                                     ID_FALSE,                 //use mutex(no use)
                                     8,                        //align byte(default)
                                     ID_FALSE,				   //ForcePooling
                                     ID_TRUE,				   //GarbageCollection
                                     ID_TRUE,                          /* HWCacheLine */
                                     IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
              != IDE_SUCCESS);			
    sBeginSNPoolInit = ID_TRUE;

    IDU_LIST_INIT(&sBeginSNList);

    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_SLAVE_BEGIN,
                          mMeta.mReplication.mRepName,
                          mXSN);

    /* BUG-31679 Incremental Sync �߿� SenderInfo�� ���� Transaction Table�� ���ٱ���
     *  Failback Slave�� Incremental Sync �߿� Transaction Table�� �ʱ�ȭ�ϹǷ�,
     *  Incremental Sync ���� SenderInfo�� ��Ȱ��ȭ�Ѵ�.
     */
    mSenderInfo->checkAndRunDeactivate();

    mSenderInfo->setPeerFailbackEnd(ID_FALSE);

    // Peer Server���� Incremental Sync�� ���� Primary Key�� ������ ������ �˸���.
    IDE_TEST(addXLogSyncPKBegin() != IDE_SUCCESS);
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // Phase 1 : Commit�� Transaction�� Begin SN�� �����Ѵ�.
    /* For Parallel Logging: Log Base�� Replication�� ���� */
    IDE_TEST( mReplicator.replicateLogFiles( RP_RETURN_ON_NOGAP,
                                             RP_COLLECT_BEGIN_SN_ON_ADD_XLOG,
                                             &sBeginSNPool,
                                             &sBeginSNList )
              != IDE_SUCCESS );
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // SN�� �ʱ�ȭ�Ѵ�.
    mXSN = mMeta.mReplication.mXSN;
    mCommitXSN = mXSN;

    mReplicator.leaveLogBuffer();

    // Log Manager�� �ʱ�ȭ�Ѵ�.
    IDE_ASSERT( mReplicator.isLogMgrInit() == ID_TRUE );
    IDE_TEST( mReplicator.destroyLogMgr() != IDE_SUCCESS );

    IDE_TEST( mReplicator.initializeLogMgr( mXSN,
                                            RPU_PREFETCH_LOGFILE_COUNT,
                                            ID_FALSE,
                                            0,
                                            0,
                                            NULL )
              != IDE_SUCCESS );
    mReplicator.setNeedSN( mXSN ); //proj-1670

    // Transaction Table�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( mReplicator.initTransTable() != IDE_SUCCESS );

    // Phase 2 : Commit�� Transaction���� DML�� Primary Key�� �����Ͽ� �����Ѵ�.
    /* For Parallel Logging: Log Base�� Replication�� ���� */
    IDE_TEST( mReplicator.replicateLogFiles( RP_RETURN_ON_NOGAP,
                                             RP_SEND_SYNC_PK_ON_ADD_XLOG,
                                             &sBeginSNPool,
                                             &sBeginSNList )
              != IDE_SUCCESS );
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // Transaction Table�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( mReplicator.initTransTable() != IDE_SUCCESS );

    // Peer Server���� Incremental Sync�� �ʿ��� Primary Key�� �� �̻� ������ �˸���.
    IDE_TEST(addXLogSyncPKEnd() != IDE_SUCCESS);
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // Peer Server�� Failback�� ���� ������ ��ٸ���.
    while(checkInterrupt() == RP_INTR_NONE)
    {
        if ( ( rpcManager::isStartupFailback() != ID_TRUE ) &&
             ( mCurrentType != RP_XLOGFILE_FAILBACK_SLAVE ) )
        {
            mRetryError = ID_TRUE;
            mSenderInfo->checkAndRunDeactivate();

            IDE_CONT( NORMAL_EXIT );
        }

        if(mSenderInfo->getPeerFailbackEnd() == ID_TRUE)
        {
            break;
        }

        IDE_TEST(addXLogKeepAlive() != IDE_SUCCESS);
        idlOS::sleep(sTimeValue);
    }
    IDE_TEST_CONT(checkInterrupt() != RP_INTR_NONE, NORMAL_EXIT);

    // Restart SN�� �����Ͽ� ���� �α׸� �����Ѵ�.
    //  mXSN�� Phase 2�� ��ġ�鼭 �ֽ����� �����Ǿ� �ִ�.
    IDE_TEST(updateXSN(mXSN) != IDE_SUCCESS);
    mCommitXSN = mXSN;

    /* BUG-31679 Incremental Sync �߿� SenderInfo�� ���� Transaction Table�� ���ٱ���
     *  Failback Slave�� Incremental Sync �߿� Transaction Table�� �ʱ�ȭ�ϹǷ�,
     *  Incremental Sync �Ŀ� SenderInfo�� Ȱ��ȭ�Ѵ�.
     */
    mSenderInfo->activate( mReplicator.getTransTbl(),
                           mMeta.mReplication.mXSN,
                           mMeta.mReplication.mReplMode,
                           mMeta.mReplication.mRole,
                           mSenderListIndex,
                           mAssignedTransTbl );

    RP_LABEL(NORMAL_EXIT);

    sBeginSNPoolInit = ID_FALSE;
    if(sBeginSNPool.destroy(ID_FALSE) != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(checkInterrupt() == RP_INTR_NONE)
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_SUCCEED,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }
    else
    {
        ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                              mMeta.mReplication.mRepName,
                              mXSN);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sBeginSNPoolInit == ID_TRUE)
    {
        (void)sBeginSNPool.destroy(ID_FALSE);
    }

    ideLog::log(IDE_RP_0, RP_TRC_S_NTC_FAILBACK_FAIL,
                          mMeta.mReplication.mRepName,
                          mXSN);

    return IDE_FAILURE;
}

IDE_RC rpxSender::createNStartChildren()
{
    UInt        sChildInitIdx = 0;
    UInt        sChildStartIdx = 0;
    UInt        sTmpIdx = 0;
    UInt        sStage = 0;
    rpxSender*  sTmpChildArray = NULL;
    // child�� parallel factor���� �ڽ��� �� ��(factor -1)��ŭ �����ϸ� �ȴ�.
    UInt        sChildCount = RPU_REPLICATION_EAGER_PARALLEL_FACTOR - 1;
    SChar     * sParentConnectedIP = NULL;
    SInt        sParentConnectedPort = 0;
    SChar     * sChildConnectedIP = NULL;
    SInt        sChildConnectedPort = 0;
    
    if(sChildCount != 0)
    {
        IDU_FIT_POINT_RAISE( "rpxSender::createNStartChildren::malloc::TmpChildArray",
                              ERR_CHILD_ARRAY_ALLOC );
        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPX_SENDER,
                                         ID_SIZEOF(rpxSender) * sChildCount,
                                         (void**)&sTmpChildArray,
                                         IDU_MEM_IMMEDIATE)
                != IDE_SUCCESS, ERR_CHILD_ARRAY_ALLOC);

        mMeta.mChildXSN = mXSN;

        sStage = 1;
        for(sChildInitIdx = 0; sChildInitIdx < sChildCount; sChildInitIdx++)
        {
            new (&sTmpChildArray[sChildInitIdx]) rpxSender;

            // i�� ID�� ���� Parallel sender����
            //type,copyed meta, ID:i
            IDE_TEST(sTmpChildArray[sChildInitIdx].initialize(
                        NULL,                        //aSmiStmt
                        mMeta.mReplication.mRepName, //aRepName
                        RP_PARALLEL,                 //aStartType
                        ID_FALSE,                    //aTryHandshakeOnce
                        ID_FALSE,                    //aForUpdateFlag, not Use
                        0,                           //aParallelFactor, not Use
                        NULL,                        //aSyncItemList, not use
                        mSenderInfoArray,            //lswhh to be fix , sender info
                        mRPLogBufMgr,                //aRPLogBufMgr
                        NULL,                        //aSNMapMgr
                        SM_SN_NULL,                  //aActiveRPRecoverySN
                        &mMeta,                      //aMeta
                        makeChildID(sChildInitIdx),  //aParallelID
                        mSenderListIndex )
                    != IDE_SUCCESS);
        }

        sStage = 2;
        for(sChildStartIdx = 0; sChildStartIdx < sChildCount; sChildStartIdx++)
        {
            IDU_FIT_POINT( "rpxSender::createNStartChildren::Thread::sTmpChildArray",
                           idERR_ABORT_THR_CREATE_FAILED,
                           "rpxSender::createNStartChildren",
                           "sTmpChildArray" );
            IDE_TEST(sTmpChildArray[sChildStartIdx].start() != IDE_SUCCESS);
        }
    }

    // Child�� ���°� ��� RUN�� �� ������ ��ٸ���.
    for(sTmpIdx = 0; sTmpIdx < sChildCount; sTmpIdx++)
    {
        while ( ( sTmpChildArray[sTmpIdx].checkInterrupt() == RP_INTR_NONE ) &&
                ( checkInterrupt() == RP_INTR_NONE ) )
        {
            if ( sTmpChildArray[sTmpIdx].getSenderInfo()->getSenderStatus()
                 == RP_SENDER_IDLE )
            {
                break;
            }
            else
            {
                idlOS::thr_yield();
            }
        }
    }

    getRemoteAddress( &sParentConnectedIP, &sParentConnectedPort );
    for( sTmpIdx = 0; sTmpIdx < sChildCount; sTmpIdx++ )
    {
        sTmpChildArray[sTmpIdx].getRemoteAddress( &sChildConnectedIP, &sChildConnectedPort );

        IDE_TEST_RAISE( idlOS::strncmp( sParentConnectedIP, 
                                        sChildConnectedIP, 
                                        QC_MAX_IP_LEN + 1 ) 
                        != 0, ERR_DIFFERENT_IP );
    }

    // performanceview���� mChildArray�� �����ϹǷ� atomic operation���� �ؾ��Ѵ�.
    // �׷��Ƿ�, ��� �۾��� �Ϸ�� �Ŀ� mChildArray�� �����Ѵ�.
    IDE_ASSERT(mChildArrayMtx.lock(NULL) == IDE_SUCCESS);
    mChildArray = sTmpChildArray;
    mChildCount = sChildCount;
    IDE_ASSERT(mChildArrayMtx.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CHILD_ARRAY_ALLOC);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpxSender::createParallelChilds",
                                "mChildArray"));
    }
    IDE_EXCEPTION( ERR_DIFFERENT_IP )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_PARENT_AND_CHILD_CONNECT_DIFFERENT ) );
        mRetryError = ID_TRUE;
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 2:
            for(sTmpIdx = 0; sTmpIdx < sChildStartIdx; sTmpIdx++)
            {
                sTmpChildArray[sTmpIdx].shutdown();
                (void)sTmpChildArray[sTmpIdx].waitComplete( NULL );
                if(sTmpChildArray[sTmpIdx].join() != IDE_SUCCESS)
                {
                    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_JOIN_THREAD));
                    IDE_ERRLOG(IDE_RP_0);
                    IDE_CALLBACK_FATAL("[Repl Parallel Child] Thread join error");
                }
            }

        case 1:
            for(sTmpIdx = 0; sTmpIdx < sChildInitIdx ; sTmpIdx++)
            {
                sTmpChildArray[sTmpIdx].destroy();
            }
            (void)iduMemMgr::free(sTmpChildArray);
            sTmpChildArray = NULL;
            break;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

void rpxSender::shutdownNDestroyChildren()
{
    UInt        i;
    rpxSender * sTmpChildArray = mChildArray;
    UInt        sChildCount    = mChildCount;

    if(isParallelParent() == ID_TRUE)
    {
        if(sTmpChildArray != NULL)
        {
            // childArray�� destroy�ϴ� �� performance view�� ���� �˻����� ���ϵ���
            // �ϱ����ؼ� mChildArray�� �̸� NULL�� �����Ѵ�.
            IDE_ASSERT(mChildArrayMtx.lock(NULL) == IDE_SUCCESS);
            mChildArray = NULL;
            mChildCount = 0;
            IDE_ASSERT(mChildArrayMtx.unlock() == IDE_SUCCESS);

            for(i = 0; i < sChildCount; i++)
            {
                sTmpChildArray[i].waitUntilSendingByServiceThr();
                sTmpChildArray[i].shutdown();

            }
            for(i = 0; i < sChildCount; i++)
            {
                if(sTmpChildArray[i].join() != IDE_SUCCESS)
                {
                    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_JOIN_THREAD));
                    IDE_ERRLOG(IDE_RP_0);
                    IDE_CALLBACK_FATAL("[Repl Parallel Child] Thread join error");
                }
                (void)sTmpChildArray[i].waitComplete( NULL );
                sTmpChildArray[i].destroy();
            }

            (void)iduMemMgr::free(sTmpChildArray);
            sTmpChildArray = NULL;
        }
    }
    else
    {
        // Parallel Parent�� �ƴϸ�, mChildArray�� NULL�̴�.
        IDE_DASSERT(sTmpChildArray == NULL);
    }
}

/*****************************************************************************
 * Description:
 *
 * All Sender-> mExitFlag, mApplyFaultFlag, mRetryError���� ���ͷ�Ʈ ������ Ȯ���Ѵ�.
 *              mApplyFaultFlag���� mRetryError�� �켱�Ͽ� �����Ѵ�.
 *
 * Parallel-> Parent Sender�� ��� Child�� ���ͷ�Ʈ�� Ȯ���Ͽ�, �� Child�� ���ͷ�Ʈ
 *            �����̸� �ڽ��� Network Error�� ó���Ѵ�.
 *
 *            Child Sender�� ���ͷ�Ʈ�� �߻��ϸ�, Child�� Exit Flag�� ������ ������ ó���Ѵ�.
 *
 * Return value: check ����� ���� �ؾ��� �Ͽ� ���� code�� ��Ÿ����
 *               intrLevel(interrupt Level)�� ��ȯ�Ѵ�. intrLevel�� �Ʒ��� ����.
 * intrLevel: 0, RP_INTR_NONE        (No problem, continue replication)
 *            1, RP_INTR_RETRY     (Network error occur, The sender must retry.)
 *            2, RP_INTR_FAULT       (Some fault occur, The sender must stop.)
 *            3, RP_INTR_EXIT        (Exit flag was set, The sender must stop.)
 * interrupt: vt, vi, noun
 *****************************************************************************/
RP_INTR_LEVEL rpxSender::checkInterrupt()
{
    RP_INTR_LEVEL sCheckResult;
    UInt          i;

    //-------------------------------------------------------------
    // Self Check
    //-------------------------------------------------------------
    // Normal(lazy/acked)/Recovery/Offline/Parallel Sender�� ��� Self Check�� �ؾ��Ѵ�.
    if(mExitFlag == ID_TRUE)
    {
        sCheckResult = RP_INTR_EXIT;
    }
    else if(mRetryError == ID_TRUE)
    {
        if(isParallelChild() == ID_TRUE)
        {
            mExitFlag    = ID_TRUE;
            sCheckResult = RP_INTR_EXIT;
        }
        else
        {
            sCheckResult = RP_INTR_RETRY;
        }
    }
    else if(mApplyFaultFlag == ID_TRUE)
    {
        if(isParallelChild() == ID_TRUE)
        {
            mExitFlag    = ID_TRUE;
            sCheckResult = RP_INTR_EXIT;
        }
        else
        {
            sCheckResult = RP_INTR_FAULT;
        }
    }
    else
    {
        sCheckResult = RP_INTR_NONE;
    }

    //---------------------------------------------------------------------------
    // Children Check
    // Self Check�� ������ ���� ��� Parallel Parent�� Child���� �����ִ��� Ȯ���ؾ���.
    //---------------------------------------------------------------------------
    if((isParallelParent() == ID_TRUE) && (sCheckResult == RP_INTR_NONE))
    {
        // parallel Parent�� child���� ��� ������ �����Ͽ�
        // ������ �ִ� ��� Parent�� Network Error�� ó���Ͽ�
        // �� ������ �� �ֵ��� �Ѵ�.
        if(mChildArray != NULL)
        {
            for(i = 0; i < mChildCount; i++)
            {
                if(mChildArray[i].checkInterrupt() != RP_INTR_NONE)
                {
                    // child�� network error�� �߻��� ���� �Ǵ�
                    // error�� ���� ����Ǿ� exit flag�� �����Ǿ���.
                    mRetryError = ID_TRUE;
                    mSenderInfo->checkAndRunDeactivate(); //isDisconnect()
                    sCheckResult = RP_INTR_RETRY;
                    break;
                }
            }
        }
    }
    else
    {
        //nothing to do
    }

    return sCheckResult;
}

IDE_RC rpxSender::updateRemoteFaultDetectTime()
{
    smiTrans          sTrans;
    SInt              sStage = 0;
    idBool            sIsTxBegin = ID_FALSE;
    smiStatement     *spRootStmt;
    UInt              sFlag = 0;

    if( mSvcThrRootStmt != NULL )
    {
        spRootStmt = mSvcThrRootStmt;
        IDE_TEST(rpcManager::updateRemoteFaultDetectTime(
                                        spRootStmt,
                                        mMeta.mReplication.mRepName,
                                        mMeta.mReplication.mRemoteFaultDetectTime)
                 != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(sTrans.initialize() != IDE_SUCCESS);
        sStage = 1;
    
        sFlag = (sFlag & ~SMI_ISOLATION_MASK) | (UInt)RPU_ISOLATION_LEVEL;
        sFlag = (sFlag & ~SMI_TRANSACTION_MASK) | SMI_TRANSACTION_NORMAL;
        sFlag = (sFlag & ~SMI_TRANSACTION_REPL_MASK) | SMI_TRANSACTION_REPL_NONE;
        sFlag = (sFlag & ~SMI_COMMIT_WRITE_MASK) | SMI_COMMIT_WRITE_NOWAIT;
    
        IDE_TEST(sTrans.begin(&spRootStmt, NULL, sFlag, SMX_NOT_REPL_TX_ID)
                 != IDE_SUCCESS);
        sIsTxBegin = ID_TRUE;
        sStage = 2;
    
        IDE_TEST(rpcManager::updateRemoteFaultDetectTime(
                                        spRootStmt,
                                        mMeta.mReplication.mRepName,
                                        mMeta.mReplication.mRemoteFaultDetectTime)
                 != IDE_SUCCESS);
    
        sStage = 1;
        IDE_TEST(sTrans.commit() != IDE_SUCCESS);
        sIsTxBegin = ID_FALSE;
    
        sStage = 0;
        IDE_TEST(sTrans.destroy( NULL ) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_ERRLOG(IDE_RP_0);

    switch(sStage)
    {
        case 2:
            IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
            sIsTxBegin = ID_FALSE;
        case 1:
            if(sIsTxBegin == ID_TRUE)
            {
                IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
                sIsTxBegin = ID_FALSE;
            }
            (void)sTrans.destroy( NULL );
        default:
            break;
    }

    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_UPDATE_REMOTE_FAULT_DETECT_TIME));

    return IDE_FAILURE;
}

/*
 *
 */
rpdTransTbl * rpxSender::getTransTbl( void )
{
    return mReplicator.getTransTbl();
}

/*
 *
 */
void rpxSender::getAllLFGReadLSN( smLSN  * aArrReadLSN )
{ 
    mReplicator.getAllLFGReadLSN( aArrReadLSN );
}

/*
 *
 */
idBool rpxSender::isLogMgrInit( void )
{
    return mReplicator.isLogMgrInit();
}

/*
 *
 */
RP_LOG_MGR_INIT_STATUS rpxSender::getLogMgrInitStatus( void )
{
    return mReplicator.getLogMgrInitStatus();
}

/*
 *
 */
void rpxSender::setExitFlag( void )
{
    mExitFlag = ID_TRUE;

    mReplicator.setExitFlagAheadAnalyzerThread();
}

IDE_RC rpxSender::waitStartComplete( idvSQL * aStatistics )
{
    PDL_Time_Value  sPDL_Time_Value;

    sPDL_Time_Value.initialize(1, 0);

    while( mStartComplete != ID_TRUE )
    {
        idlOS::sleep(sPDL_Time_Value);

        /* BUG-32855 Replication sync command ignore timeout property
         * if timeout(DDL_TIMEOUT) occur then replication sync must be terminated.
         */
        IDE_TEST(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
extern "C" SInt compareSentLogCount( const void * aItem1,
                                     const void * aItem2 )
{
    SInt sResult = 0;

    if ( (*(rpxSentLogCount **)aItem1)->mTableOID ==
         (*(rpxSentLogCount **)aItem2)->mTableOID )
    {
        sResult = 0;
    }
    else
    {
        if ( (*(rpxSentLogCount **)aItem1)->mTableOID > 
             (*(rpxSentLogCount **)aItem2)->mTableOID )
        {
            sResult = 1;
        }
        else
        {
            sResult = -1;            
        }
    }
    
    return sResult;
}

/*
 *
 */ 
void rpxSender::rebuildSentLogCount( void )
{
    UInt i = 0;

    for ( i = 0; i < mSentLogCountArraySize; i++ )
    {
        mSentLogCountArray[i].mTableOID = mMeta.getTableOID(i);

        mSentLogCountArray[i].mInsertLogCount    = 0;
        mSentLogCountArray[i].mDeleteLogCount    = 0;
        mSentLogCountArray[i].mUpdateLogCount    = 0;
        mSentLogCountArray[i].mLOBLogCount       = 0;
        
        mSentLogCountSortedArray[i] = &(mSentLogCountArray[i]);
    }

    idlOS::qsort( mSentLogCountSortedArray,
                  mSentLogCountArraySize,
                  ID_SIZEOF( rpxSentLogCount *),
                  compareSentLogCount );
}

IDE_RC rpxSender::updateSentLogCount( smOID aNewTableOID,
                                      smOID aOldTableOID )
{
    rpxSentLogCount * sSentLogCount = NULL;

    searchSentLogCount( aOldTableOID, &sSentLogCount );
    IDE_TEST_RAISE( sSentLogCount == NULL, ERR_NOT_FOUND_TABLE );

    sSentLogCount->mTableOID = aNewTableOID;

    idlOS::qsort( mSentLogCountSortedArray,
                  mSentLogCountArraySize,
                  ID_SIZEOF( rpxSentLogCount * ),
                  compareSentLogCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND_TABLE );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_META_NO_SUCH_DATA ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*
 * Search by using binary search algorithm.
 */ 
void rpxSender::searchSentLogCount(
    smOID                  aTableOID,
    rpxSentLogCount     ** aSentLogCount )
{
    SInt sLow;
    SInt sHigh;
    SInt sMid;

    sLow  = 0;
    sHigh = mSentLogCountArraySize - 1;
    *aSentLogCount = NULL;

    while ( sLow <= sHigh )
    {
        sMid = (sHigh + sLow) >> 1;

        if ( mSentLogCountSortedArray[sMid]->mTableOID == aTableOID )
        {
            *aSentLogCount = mSentLogCountSortedArray[sMid];
            break;
        }
        else
        {
            if ( mSentLogCountSortedArray[sMid]->mTableOID > aTableOID )
            {
                sHigh = sMid - 1;
            }
            else
            {
                sLow = sMid + 1;                
            }
        }
    }
}

void rpxSender::copySentLogCount( rpxSentLogCount * aSrc, UInt aSentLogCountArraySize )
{
    SInt i = 0;
    UInt j = 0;

    for ( i = 0; i < mMeta.mReplication.mItemCount; i++ )
    {
        for ( j = 0; j < aSentLogCountArraySize; j++ )
        {
            if ( mSentLogCountArray[i].mTableOID == aSrc[j].mTableOID )
            {
                mSentLogCountArray[i].mInsertLogCount = aSrc[j].mInsertLogCount;
                mSentLogCountArray[i].mUpdateLogCount = aSrc[j].mUpdateLogCount;
                mSentLogCountArray[i].mDeleteLogCount = aSrc[j].mDeleteLogCount;
                mSentLogCountArray[i].mLOBLogCount    = aSrc[j].mLOBLogCount;

                break;
            }
        }
    }
}

IDE_RC rpxSender::allocAndRebuildNewSentLogCount()
{
    UInt               sSentLogCountArraySize   = 0;
    rpxSentLogCount  * sSentLogCountArray       = NULL;
    rpxSentLogCount ** sSentLogCountSortedArray = NULL;
    idBool             sIsAlloc                 = ID_FALSE;

    sSentLogCountArray       = mSentLogCountArray;
    sSentLogCountSortedArray = mSentLogCountSortedArray;
    sSentLogCountArraySize   = mSentLogCountArraySize;

    mSentLogCountArray       = NULL;
    mSentLogCountSortedArray = NULL;

    mSentLogCountArraySize = mMeta.mReplication.mItemCount;
    if ( mSentLogCountArraySize > 0 )
    {
        IDE_TEST( allocSentLogCount() != IDE_SUCCESS );
        sIsAlloc = ID_TRUE;
        rebuildSentLogCount();
        if ( sSentLogCountArraySize > 0 )
        {
            copySentLogCount( sSentLogCountArray, sSentLogCountArraySize );
        }
        if ( sSentLogCountArray != NULL )
        {
            (void)iduMemMgr::free( sSentLogCountArray );
            sSentLogCountArray = NULL;
        }

        if ( sSentLogCountSortedArray != NULL )
        {
            (void)iduMemMgr::free( sSentLogCountSortedArray );
            sSentLogCountSortedArray = NULL;
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAlloc != ID_TRUE )
    {
        mSentLogCountArray       = sSentLogCountArray;
        mSentLogCountSortedArray = sSentLogCountSortedArray;
        mSentLogCountArraySize   = sSentLogCountArraySize;
    }
    else
    {
        (void)iduMemMgr::free( sSentLogCountArray );
        sSentLogCountArray = NULL;
        (void)iduMemMgr::free( sSentLogCountSortedArray );        
        sSentLogCountSortedArray = NULL;
    }

    IDE_POP();

    return IDE_FAILURE;
}
/*
 *
 */
IDE_RC rpxSender::allocSentLogCount( void )
{
    SInt sStage = 0;

    if ( mSentLogCountArraySize > 0 )
    {
        IDU_FIT_POINT_RAISE( "rpxSender::allocSentLogCount::calloc::LogCountArray",
                              ERR_MEMORY_ALLOC_LOG_COUNT_ARRAY );
        IDE_TEST_RAISE( iduMemMgr::calloc(
                            IDU_MEM_RP_RPX_SENDER,
                            mSentLogCountArraySize,
                            ID_SIZEOF( rpxSentLogCount ),
                            (void **)&mSentLogCountArray,
                            IDU_MEM_IMMEDIATE )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOG_COUNT_ARRAY );
        sStage = 1;

        IDU_FIT_POINT_RAISE( "rpxSender::allocSentLogCount::calloc::LogCountSortedArray",
                              ERR_MEMORY_ALLOC_LOG_COUNT_SORTED_ARRAY );
        IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                           mSentLogCountArraySize,
                                           ID_SIZEOF( rpxSentLogCount * ),
                                           (void **)&mSentLogCountSortedArray,
                                           IDU_MEM_IMMEDIATE )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOG_COUNT_SORTED_ARRAY );
    }
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_LOG_COUNT_ARRAY );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::allocSentLogCount",
                                  "mSentLogCountArray" ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_LOG_COUNT_SORTED_ARRAY );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::allocSentLogCount",
                                  "mSentLogCountSortedArray" ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 1:
            (void)iduMemMgr::free( mSentLogCountArray );
            mSentLogCountArray = NULL;
        default:
            break;
    }

    IDE_POP();
        
    return IDE_FAILURE;
}

/*
 *
 */ 
void rpxSender::freeSentLogCount( void )
{
    if ( mSentLogCountArray != NULL )
    {
        (void)iduMemMgr::free( mSentLogCountArray );
    }
    else
    {
        /* nothing to do */
    }
    mSentLogCountArray = NULL;
    
    if ( mSentLogCountSortedArray != NULL )
    {
        (void)iduMemMgr::free( mSentLogCountSortedArray );        
    }
    else
    {
        /* nothing to do */
    }
    mSentLogCountSortedArray = NULL;
}

/*
 *
 */ 
IDE_RC rpxSender::buildRecordsForSentLogCount(
    idvSQL              * /*aStatistics*/,
    void                * aHeader,
    void                * /* aDumpObj */,
    iduFixedTableMemory * aMemory )
{
    UInt i;
    rpcSentLogCount sSentLogCount;

    sSentLogCount.mRepName = mRepName;
    sSentLogCount.mCurrentType = mCurrentType;
    sSentLogCount.mParallelID = mParallelID;
    
    if ( mSentLogCountArray != NULL )
    {
        for ( i = 0; i < mSentLogCountArraySize; i++ )
        {
            sSentLogCount.mTableOID       =
                mSentLogCountArray[i].mTableOID;
            sSentLogCount.mInsertLogCount =
                mSentLogCountArray[i].mInsertLogCount;
            sSentLogCount.mUpdateLogCount =
                mSentLogCountArray[i].mUpdateLogCount;
            sSentLogCount.mDeleteLogCount =
                mSentLogCountArray[i].mDeleteLogCount;
            sSentLogCount.mLOBLogCount    =
                mSentLogCountArray[i].mLOBLogCount;

            IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                                  aMemory,
                                                  (void *)&sSentLogCount )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */  
void rpxSender::increaseInsertLogCount( smOID aTableOID )
{
    rpxSentLogCount * sSentLogCount = NULL;
    
    searchSentLogCount( aTableOID, &sSentLogCount );

    if ( sSentLogCount != NULL )
    {
        sSentLogCount->mInsertLogCount++;
    }
    else
    {
            /* nothing to do */
    }
}

/*
 *
 */
void rpxSender::increaseDeleteLogCount( smOID aTableOID )
{
    rpxSentLogCount * sSentLogCount = NULL;

    searchSentLogCount( aTableOID, &sSentLogCount );

    if ( sSentLogCount != NULL )
    {
        sSentLogCount->mDeleteLogCount++;
    }
    else
    {
        /* nothing to do */
    }
}

/*
 *
 */ 
void rpxSender::increaseUpdateLogCount( smOID aTableOID )
{
    rpxSentLogCount * sSentLogCount = NULL;

    searchSentLogCount( aTableOID, &sSentLogCount );
    
    if ( sSentLogCount != NULL )
    {
        sSentLogCount->mUpdateLogCount++;
    }
    else
    {
        /* nothing to do */
    }
}

/*
 *
 */ 
void rpxSender::increaseLOBLogCount( smOID aTableOID )
{
    rpxSentLogCount * sSentLogCount = NULL;
    
    searchSentLogCount( aTableOID, &sSentLogCount );
   
    if ( sSentLogCount != NULL )
    {
        sSentLogCount->mLOBLogCount++;
    }
    else
    {
        /* nothing to do */
    }
}

/*
 *
 */
void rpxSender::increaseLogCount( rpXLogType aType,
                                  smOID      aTableOID )
{
    switch ( aType )
    {
        case RP_X_INSERT:
            increaseInsertLogCount( aTableOID );
            break;
        
        case RP_X_UPDATE:
            increaseUpdateLogCount( aTableOID );
            break;
            
        case RP_X_DELETE:
            increaseDeleteLogCount( aTableOID );
            break;
        
        case RP_X_LOB_CURSOR_OPEN:
        case RP_X_LOB_CURSOR_CLOSE:
        case RP_X_LOB_PREPARE4WRITE:
        case RP_X_LOB_PARTIAL_WRITE:
        case RP_X_LOB_FINISH2WRITE:
            increaseLOBLogCount( aTableOID );
            break;
            
        default:
            /* nothing to do */
            break;
    }
}

void rpxSender::checkAndSetGroupingMode( void )
{
    UInt sOptions = 0;

    sOptions = mMeta.mReplication.mOptions;

    if ( ( sOptions & RP_OPTION_GROUPING_MASK ) == RP_OPTION_GROUPING_SET )
    {
        mIsGroupingMode = ID_TRUE;
    }
    else
    {
        mIsGroupingMode = ID_FALSE;
    }
}

IDE_RC rpxSender::buildRecordForReplicatedTransGroupInfo(  void                * aHeader,
                                                           void                * aDumpObj,
                                                           iduFixedTableMemory * aMemory )
{
    IDE_TEST( mReplicator.buildRecordForReplicatedTransGroupInfo( mRepName,
                                                                  aHeader,
                                                                  aDumpObj,
                                                                  aMemory )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::buildRecordForReplicatedTransSlotInfo( void                * aHeader,
                                                         void                * aDumpObj,
                                                         iduFixedTableMemory * aMemory )
{
    IDE_TEST( mReplicator.buildRecordForReplicatedTransSlotInfo( mRepName,
                                                                 aHeader,
                                                                 aDumpObj,
                                                                 aMemory )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SInt rpxSender::getCurrentFileNo( void )
{
    return mReplicator.getCurrentFileNo();
}

IDE_RC rpxSender::buildRecordForAheadAnalyzerInfo( void                * aHeader,
                                                   void                * aDumpObj,
                                                   iduFixedTableMemory * aMemory )
{
    IDE_TEST( mReplicator.buildRecordForAheadAnalyzerInfo( mRepName,
                                                           aHeader,
                                                           aDumpObj,
                                                           aMemory )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool rpxSender::waitUntilFlushFailbackComplete()
{
    idBool sRC = ID_TRUE;

    UInt sTimeLimit = 300 * 100;
    UInt sCurTime = 0;
    PDL_Time_Value sPDL_Time_Value;

    if ( mStatus == RP_SENDER_IDLE )
    {
        /* nothing to do */
    }
    else
    {
        sPDL_Time_Value.initialize(0, 10000); /* 0.01 sec */

        while( mStatus < RP_SENDER_IDLE )
        {
            idlOS::sleep(sPDL_Time_Value);
            sCurTime += 1;

            if ( ( sCurTime > sTimeLimit ) ||
                 ( mStatus == RP_SENDER_RETRY ) ||
                 ( mExitFlag == ID_TRUE ) )
            {
                sRC = ID_FALSE;
                break;
            }
            else
            {
                /* nothing to do */
            }
        }
    }
    return  sRC;
}

void rpxSender::sendXLog( const SChar * aLogPtr,
                          smTID         aTransID,
                          smSN          aCurrentSN,
                          idBool        aIsBeginLog )
{
    rpxSender * sSender = NULL;

    (void)idCore::acpAtomicInc64( &mServiceThrRefCount );

    sSender = getAssignedSender( aTransID, aCurrentSN, aIsBeginLog );
    if ( sSender != NULL )
    {
        if ( sSender->replicateLogWithLogPtr( aLogPtr ) != IDE_SUCCESS )
        {
            mIsServiceFail = ID_TRUE;

            IDE_ERRLOG( IDE_RP_0 );
            mSenderInfo->wakeupEagerSender();
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

    (void)idCore::acpAtomicDec64( &mServiceThrRefCount );
}

rpxSender * rpxSender::assignSenderBySlotIndex( UInt     aSlotIndex,
                                                smSN     aCurrentSN,
                                                idBool   aIsBeginLog )
{
    rpxSender   * sSender = NULL;

    switch( mStatus )
    {
        case RP_SENDER_IDLE:
            sSender = getLessBusySender();
            mAssignedTransTbl[aSlotIndex] = sSender;
            break;

        case RP_SENDER_FLUSH_FAILBACK:
            IDE_ASSERT( mStatusMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS );

            IDE_DASSERT( ( mStatus == RP_SENDER_FLUSH_FAILBACK ) || ( mStatus == RP_SENDER_IDLE ) );

            if ( ( aCurrentSN > mFailbackEndSN ) && ( aIsBeginLog == ID_TRUE ) )
            {
                IDE_ASSERT( mStatusMutex.unlock() == IDE_SUCCESS );

                sSender = getLessBusyChildSender();
                if ( sSender == NULL )
                {
                    /* child �� �������� ������ sSender �� null �̴�
                     * �̶����� �������� Failback �� ���������� ��� �Ѵ�.
                     */
                    if ( waitUntilFlushFailbackComplete() == ID_TRUE )
                    {
                        sSender = this;
                    }
                    else
                    {
                        /* ���������� ������ �Ҽ� ������ exitflag �� ���� �Ѵ�. */
                        mExitFlag = ID_TRUE;
                    }
                }
                else
                {
                    /* do nothing */
                }

                mAssignedTransTbl[aSlotIndex] = sSender;
            }
            else
            {
                IDE_ASSERT( mStatusMutex.unlock() == IDE_SUCCESS );
                /* sender thread �� ���� ó�� �Ѵ�. */
            }

            break;

        default:
            break;
    }

    return sSender;
}

rpxSender * rpxSender::getLessBusySender( void )
{
    rpxSender   * sSender = NULL;
    UInt          sMinActiveTransCount = 0;
    UInt          sActiveTransCount = 0;

    UInt          i = 0;

    sMinActiveTransCount = getActiveTransCount();
    sSender = this;

    IDE_ASSERT( mChildArrayMtx.lock( NULL ) == IDE_SUCCESS );

    for ( i = 0; i < mChildCount; i++ )
    {
        sActiveTransCount = mChildArray[i].getActiveTransCount();

        if ( sMinActiveTransCount > sActiveTransCount )
        {
            sMinActiveTransCount = sActiveTransCount;
            sSender = &(mChildArray[i]);
        }
        else
        {
            /* do nothing */
        }
    }

    IDE_ASSERT( mChildArrayMtx.unlock() == IDE_SUCCESS );

    return sSender;
}

rpxSender * rpxSender::getLessBusyChildSender( void )
{
    rpxSender   * sSender = NULL;
    UInt          i = 0;
    UInt          sActiveTransCount = 0;
    UInt          sMinActiveTransCount = 0;

    IDE_ASSERT( mChildArrayMtx.lock( NULL ) == IDE_SUCCESS );

    if ( mChildCount != 0 )
    {
        sMinActiveTransCount = mChildArray[0].getActiveTransCount();
        sSender = &(mChildArray[0]);

        for ( i = 1; i < mChildCount; i++ )
        {
            sActiveTransCount = mChildArray[i].getActiveTransCount();

            if ( sMinActiveTransCount >= sActiveTransCount )
            {
                sMinActiveTransCount = sActiveTransCount;
                sSender = &(mChildArray[i]);
            }
            else
            {
                /* do nothing */
            }
        }
    }
    else
    {
        /* do nothing */
    }

    IDE_ASSERT( mChildArrayMtx.unlock() == IDE_SUCCESS );

    return sSender;
}

UInt rpxSender::getActiveTransCount( void )
{
    return mReplicator.getActiveTransCount();
}

IDE_RC rpxSender::replicateLogWithLogPtr( const SChar    * aLogPtr )
{
    IDE_RC sResult = IDE_SUCCESS;

    (void)idCore::acpAtomicInc64( &mServiceThrRefCount );

    if ( ( mStatus != RP_SENDER_STOP ) && ( mStatus != RP_SENDER_RETRY ) )
    {
        sResult = mReplicator.replicateLogWithLogPtr( aLogPtr );
    }
    else
    {
        /* do nothing */
    }

    (void)idCore::acpAtomicDec64( &mServiceThrRefCount );

    return sResult;
}


rpxSender * rpxSender::getAssignedSender( smTID        aTransID, 
                                          smSN         aCurrentSN,
                                          idBool       aIsBeginLog )
{
    UInt               sSlotIndex = 0;
    rpxSender        * sSender = NULL;

    sSlotIndex = aTransID % mTransTableSize;
    sSender = mAssignedTransTbl[sSlotIndex];
    if ( sSender == NULL )
    {
        sSender = assignSenderBySlotIndex( sSlotIndex,
                                           aCurrentSN,
                                           aIsBeginLog );
    }
    else
    {
        /* do nothing */
    }

    return sSender;
}

void rpxSender::waitUntilSendingByServiceThr( void )
{
    PDL_Time_Value  sTimeValue;
    
    sTimeValue.initialize( 0, 1000 );
    
    while ( idCore::acpAtomicGet64( &mServiceThrRefCount ) > 0 )  
    {
        idlOS::sleep( sTimeValue );
    }
}

smSN rpxSender::getFailbackEndSN( void )
{
    return mFailbackEndSN;
}

idBool rpxSender::isSkipLog( smSN  aSN )
{
    idBool sRes = ID_FALSE;

    if ( mSkipXSN == SM_SN_NULL )
    {
        sRes = ID_FALSE;
    }
    else if ( mSkipXSN <= aSN )
    {
        sRes = ID_FALSE;
    }
    else
    {
        sRes = ID_TRUE;
    }

    return sRes;
}

IDE_RC rpxSender::sendDDLASyncStart( UInt aType )
{
    return mMessenger.sendDDLASyncStart( aType );
}

IDE_RC rpxSender::recvDDLASyncStartAck( UInt * aType )
{
    return mMessenger.recvDDLASyncStartAck( aType );
}

IDE_RC rpxSender::sendDDLASyncExecute( UInt    aType,
                                       SChar * aUserName,
                                       UInt    aDDLEnableLevel,
                                       UInt    aTargetCount,
                                       SChar * aTargetTableName,
                                       SChar * aTargetPartNames,
                                       smSN    aDDLCommitSN,
                                       SChar * aDDLStmt )
{
    return mMessenger.sendDDLASyncExecute( aType,
                                           aUserName,
                                           aDDLEnableLevel,
                                           aTargetCount,
                                           aTargetTableName,
                                           aTargetPartNames,
                                           aDDLCommitSN,
                                           aDDLStmt );
}

IDE_RC rpxSender::recvDDLASyncExecuteAck( UInt  * aType,
                                          UInt  * aIsSuccess,
                                          UInt  * aErrCode,
                                          SChar * aErrMsg )

{
    return  mMessenger.recvDDLASyncExecuteAck( aType,
                                               aIsSuccess,
                                               aErrCode,
                                               aErrMsg );
}

IDE_RC rpxSender::getTargetNamesFromItemMetaEntry( smTID    aTID,
                                                   UInt   * aTargetCount,
                                                   SChar  * aTargetTableName,
                                                   SChar ** aTargetPartNames )
{
    return mReplicator.getTargetNamesFromItemMetaEntry( aTID,
                                                        aTargetCount,
                                                        aTargetTableName,
                                                        aTargetPartNames );
}

IDE_RC rpxSender::getDDLInfoFromDDLStmtLog( smTID   aTID,
                                               SInt    aMaxDDLStmtLen,
                                               SChar * aUserName,
                                               SChar * aDDLStmt )
{
    return mReplicator.getDDLInfoFromDDLStmtLog( aTID, 
                                                 aMaxDDLStmtLen,
                                                 aUserName,
                                                 aDDLStmt );
}

idBool rpxSender::isSuspendedApply( void )
{
    return mSenderApply->isSuspended();
}

void rpxSender::resumeApply( void )
{
    mSenderApply->resume();
}

IDE_RC rpxSender::checkAndErrorConditionalStart( )
{
    rpdConditionItemInfo * sSendConditionInfo   = NULL;
    rpdConditionActInfo  * sResultConditionInfo = NULL;

    smiStatement           sSmiStmt;
    idBool                 sIsBegin = ID_FALSE;
    SChar                  sRepName[QCI_MAX_NAME_LEN + 1];
    
    IDE_DASSERT( mCurrentType == RP_START_CONDITIONAL );
    idlOS::strncpy( sRepName, mRepName, QCI_MAX_NAME_LEN + 1 );
 
    IDU_FIT_POINT_RAISE( "rpxSender::checkAndErrorConditionalStart::calloc::sSendConditionInfo",
                         ERR_MEMORY_ALLOC_CONDITION_LIST );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                       mMeta.mReplication.mItemCount,
                                       ID_SIZEOF(rpdConditionItemInfo),
                                       (void **)&sSendConditionInfo,
                                       IDU_MEM_IMMEDIATE)
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_CONDITION_LIST );

    IDU_FIT_POINT_RAISE( "rpxSender::checkAndErrorConditionalStart::calloc::sResultConditionInfo",
                         ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                       mMeta.mReplication.mItemCount,
                                       ID_SIZEOF(rpdConditionActInfo),
                                       (void **)&sResultConditionInfo,
                                       IDU_MEM_IMMEDIATE)
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );

    IDE_TEST( sSmiStmt.begin( NULL, 
                              mSvcThrRootStmt, 
                              SMI_STATEMENT_NORMAL | SMI_STATEMENT_MEMORY_CURSOR )
              != IDE_SUCCESS );
    sIsBegin = ID_TRUE;

    /* item list ���鼭 ���̺� ������ ������ sSendConditionInfo�� �����Ѵ�. */
    IDE_TEST( rpdMeta::makeConditionInfoWithItems( &sSmiStmt,
                                                   mMeta.mReplication.mItemCount,
                                                   mMeta.mItemsOrderByTableOID,
                                                   sSendConditionInfo )
              != IDE_SUCCESS );
       

    IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );
    sIsBegin = ID_FALSE; 


    /* ��� */
    IDE_TEST( mMessenger.communicateConditionInfo( sSendConditionInfo,
                                                   mMeta.mReplication.mItemCount,
                                                   sResultConditionInfo ) 
              != IDE_SUCCESS );

    IDE_TEST( checkErrorWithConditionActInfo( sResultConditionInfo,
                                              mMeta.mReplication.mItemCount )
              != IDE_SUCCESS );

    (void)iduMemMgr::free( sResultConditionInfo );
    sResultConditionInfo = NULL;

    (void)iduMemMgr::free( sSendConditionInfo );
    sSendConditionInfo = NULL;

    return IDE_SUCCESS;
  
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_CONDITION_LIST );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::checkAndErrorConditionalStart",
                                  "sConditionList" ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::checkAndErrorConditionalStart",
                                  "sResultConditionList" ) );
    }

    IDE_EXCEPTION_END;

    if ( sIsBegin == ID_TRUE )
    {
        (void)sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
    }

    if ( sResultConditionInfo != NULL )
    {
        (void) iduMemMgr::free( sResultConditionInfo );
        sResultConditionInfo = NULL;
    }

    if ( sSendConditionInfo != NULL )
    {
        (void) iduMemMgr::free( sSendConditionInfo );
        sSendConditionInfo = NULL;
    }

    return IDE_FAILURE;
}

IDE_RC rpxSender::checkAndSetConditionalSync( )
{
    rpdConditionItemInfo * sSendConditionInfo   = NULL;
    rpdConditionActInfo  * sResultConditionInfo = NULL;

    smiStatement           sSmiStmt;
    idBool                 sIsBegin = ID_FALSE;
    rpdMeta                sTempMeta;
    SChar                  sRepName[QCI_MAX_NAME_LEN + 1];
    
    IDE_DASSERT( mCurrentType == RP_SYNC_CONDITIONAL );
    idlOS::strncpy( sRepName, mRepName, QCI_MAX_NAME_LEN + 1 );
        
    IDE_TEST( sSmiStmt.begin( NULL, 
                              mSvcThrRootStmt, 
                              SMI_STATEMENT_NORMAL | SMI_STATEMENT_MEMORY_CURSOR )
              != IDE_SUCCESS );
    sIsBegin = ID_TRUE;

    IDU_FIT_POINT_RAISE( "rpxSender::checkAndSetConditionalSync::calloc::sSendConditionInfo",
                         ERR_MEMORY_ALLOC_CONDITION_LIST );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                       mMeta.mReplication.mItemCount,
                                       ID_SIZEOF(rpdConditionItemInfo),
                                       (void **)&sSendConditionInfo,
                                       IDU_MEM_IMMEDIATE)
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_CONDITION_LIST );

    IDU_FIT_POINT_RAISE( "rpxSender::checkAndSetConditionalSync::calloc::sResultConditionInfo",
                         ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_RP_RPX_SENDER,
                                       mMeta.mReplication.mItemCount,
                                       ID_SIZEOF(rpdConditionItemInfo),
                                       (void **)&sResultConditionInfo,
                                       IDU_MEM_IMMEDIATE)
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );



    /* item list ���鼭 ���̺� ������ ������ sSendConditionInfo�� �����Ѵ�. */
    IDE_TEST( rpdMeta::makeConditionInfoWithItems( &sSmiStmt,
                                                   mMeta.mReplication.mItemCount,
                                                   mMeta.mItemsOrderByTableOID,
                                                   sSendConditionInfo )
              != IDE_SUCCESS );
       
    /* ��� */
    IDE_TEST( mMessenger.communicateConditionInfo( sSendConditionInfo,
                                                   mMeta.mReplication.mItemCount,
                                                   sResultConditionInfo ) 
              != IDE_SUCCESS );

    /* truncate ����� ������ mSyncTruncateItems�� �߰� */
    IDE_TEST( makeSyncItemsWithConditionActInfo( sResultConditionInfo,
                                                 mMeta.mReplication.mItemCount,
                                                 RP_CONDITION_TRUNCATE,
                                                 &mSyncTruncateItems ) 
              != IDE_SUCCESS );

    if ( mSyncTruncateItems != NULL )
    {
        IDE_TEST( rebuildWithUpdateOldMeta( &sSmiStmt,
                                            sRepName,
                                            ID_FALSE,
                                            &mMeta,
                                            mSyncTruncateItems )
                  != IDE_SUCCESS );

    }

    /* sync ����� ������ mSyncInsertItems�� �߰� */
    IDE_TEST( makeSyncItemsWithConditionActInfo( sResultConditionInfo,
                                                 mMeta.mReplication.mItemCount,
                                                 RP_CONDITION_SYNC,
                                                 &mSyncInsertItems ) 
              != IDE_SUCCESS );

    if ( mSyncInsertItems != NULL )
    {
        IDE_TEST( rebuildWithUpdateOldMeta( &sSmiStmt,
                                            sRepName,
                                            ID_FALSE,
                                            &mMeta,
                                            mSyncInsertItems )
                  != IDE_SUCCESS );

    }

    IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );
    sIsBegin = ID_FALSE; 

    (void) iduMemMgr::free( sResultConditionInfo );
    sResultConditionInfo = NULL;

    (void) iduMemMgr::free( sSendConditionInfo );
    sSendConditionInfo = NULL;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_CONDITION_LIST );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::checkAndSetConditionalSync",
                                  "sConditionList" ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_RESULT_CONDITION_LIST );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::checkAndSetConditionalSync",
                                  "sResultConditionList" ) );
    }
    IDE_EXCEPTION_END;

    if ( sResultConditionInfo != NULL )
    {
        (void) iduMemMgr::free( sResultConditionInfo );
        sResultConditionInfo = NULL;
    }

    if ( sSendConditionInfo != NULL )
    {
        (void) iduMemMgr::free( sSendConditionInfo );
        sSendConditionInfo = NULL;
    }

    if ( sIsBegin == ID_TRUE )
    {
        (void)sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        sIsBegin = ID_FALSE;
    }

    return IDE_FAILURE;
}

IDE_RC rpxSender::checkErrorWithConditionActInfo( rpdConditionActInfo * aConditionInfo, 
                                                  UInt                  aItemCount )
{
    UInt                   i;
    rpdMetaItem * sMetaItem = NULL;

    for ( i = 0; i < aItemCount; i++ )
    {
        IDE_TEST_RAISE( aConditionInfo[i].mAction != RP_CONDITION_START,
                        ERR_CONDITION_FAILURE );
    }
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_CONDITION_FAILURE )
    {
        if ( mMeta.searchTable( &sMetaItem, aConditionInfo[i].mTableOID ) == IDE_SUCCESS )
        {
            idlOS::snprintf( mRCMsg,
                             RP_ACK_MSG_LEN,
                             "Start conditional failure. [Table Name:%s.%s %s, Action:%"ID_UINT32_FMT"]",
                             sMetaItem->mItem.mLocalUsername,
                             sMetaItem->mItem.mLocalTablename,
                             sMetaItem->mItem.mLocalPartname,
                             aConditionInfo[i].mAction );
        }
        else
        {
            idlOS::snprintf( mRCMsg,
                             RP_ACK_MSG_LEN,
                             "Start conditional failure. [Table OID:%"ID_UINT64_FMT", Action:%"ID_UINT32_FMT"]",
                             aConditionInfo[i].mTableOID, 
                             aConditionInfo[i].mAction );
        }
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, mRCMsg ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC rpxSender::makeSyncItemsWithConditionActInfo( rpdConditionActInfo * aConditionInfo, 
                                                     UInt                  aItemCount,
                                                     RP_CONDITION_ACTION   aAction,
                                                     rpdReplSyncItem    ** aItemList )
{
    rpdReplSyncItem      * sSyncItem          = NULL;
    rpdReplSyncItem      * sSyncItemList      = NULL;
    UInt                   i;

    for ( i = 0; i < aItemCount; i++ )
    {
        if ( aConditionInfo[i].mAction == aAction )
        {
            sSyncItem = NULL;

            IDU_FIT_POINT_RAISE( "rpxSender::makeSyncItemsWithConditionActInfo::malloc::sSyncItem",
                                 ERR_MEMORY_ALLOC_SYNC_ITEM );
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPX_SENDER,
                                             ID_SIZEOF(rpdReplSyncItem),
                                             (void**)&sSyncItem,
                                             IDU_MEM_IMMEDIATE)
                           != IDE_SUCCESS, ERR_MEMORY_ALLOC_SYNC_ITEM);

            idlOS::strncpy( (SChar*) sSyncItem->mUserName,
                            mMeta.mItemsOrderByTableOID[i]->mItem.mLocalUsername,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mUserName [ QCI_MAX_OBJECT_NAME_LEN ] = '\0';

            idlOS::strncpy( (SChar*) sSyncItem->mTableName,
                            mMeta.mItemsOrderByTableOID[i]->mItem.mLocalTablename,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mTableName [ QCI_MAX_OBJECT_NAME_LEN ] = '\0';

            idlOS::strncpy( (SChar*) sSyncItem->mPartitionName,
                            mMeta.mItemsOrderByTableOID[i]->mItem.mLocalPartname,
                            QCI_MAX_OBJECT_NAME_LEN + 1 );
            sSyncItem->mPartitionName[ QCI_MAX_OBJECT_NAME_LEN ] = '\0';
           
            sSyncItem->mTableOID = mMeta.mItemsOrderByTableOID[i]->mItem.mTableOID;
            sSyncItem->mReplUnit[0] = '\0';

            sSyncItem->next = sSyncItemList;
            sSyncItemList       = sSyncItem;
        }
    }

    *aItemList = sSyncItemList;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_SYNC_ITEM );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_MEMORY_ALLOC,
                                  "rpxSender::makeSyncItemsWithConditionActInfo",
                                  "sSyncItem"));
    }
    IDE_EXCEPTION_END;

    while( sSyncItemList != NULL )
    {
        sSyncItem = sSyncItemList;
        sSyncItemList = sSyncItemList->next;
        (void)iduMemMgr::free(sSyncItem);
        sSyncItem = NULL;
    }

    return IDE_FAILURE;
}

IDE_RC rpxSender::truncateStart()
{
    PDL_Time_Value sPDL_Time_Value;
    rpdReplSyncItem  *sTruncateItem; 

    IDE_DASSERT( mSyncTruncateItems != NULL );

    IDE_TEST( setRestartSNforSync() != IDE_SUCCESS );

    for( sTruncateItem = mSyncTruncateItems;
         sTruncateItem != NULL;
         sTruncateItem = sTruncateItem->next )
    {
        IDE_TEST( mMessenger.sendTruncate( sTruncateItem->mTableOID ) 
                  != IDE_SUCCESS );

        while ( ( mSenderInfo->getFlagTruncate() == ID_FALSE ) &&
                ( checkInterrupt() == RP_INTR_NONE ) )
        {
            sPDL_Time_Value.initialize( 1, 0 );
            idlOS::sleep( sPDL_Time_Value );
        }
        IDE_TEST_RAISE( ( mExitFlag == ID_TRUE ) || ( mRetryError == ID_TRUE ), ERR_NETWORK );

        mSenderInfo->setFlagTruncate( ID_FALSE );

        IDE_TEST( handshakeWithoutReconnect( SM_NULL_TID ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NETWORK);
    {
        IDE_SET(ideSetErrorCode(rpERR_IGNORE_RP_SENDER_STOP,
                                mMeta.mReplication.mRepName,
                                mParallelID,
                                mXSN,
                                getRestartSN()));
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::prepareForRunning()
{
    if( mMeta.mReplication.mReplMode == RP_CONSISTENT_MODE )
    {
        IDE_TEST( prepareForConsistent() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( prepareForParallel() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-2725 consistent replication
 * ���� gap�� �ؼ��Ѵ�.
 */
IDE_RC rpxSender::prepareForConsistent()
{
    switch ( mCurrentType )
    {
        case RP_XLOGFILE_FAILBACK_MASTER: 
            IDE_TEST( xlogfileFailbackMaster() != IDE_SUCCESS );
            break;

        case  RP_XLOGFILE_FAILBACK_SLAVE :
            IDE_TEST( xlogfileFailbackSlave() != IDE_SUCCESS );
            break;

        case RP_NORMAL:
            setStatus( RP_SENDER_CONSISTENT_FAILBACK );
            IDE_TEST( failbackNormal() != IDE_SUCCESS );
            break;
    
        default:
            IDE_DASSERT( 0 );
            break;
    }
    mSvcThrRootStmt = NULL;

    setStatus( RP_SENDER_RUN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::xlogfileFailbackMaster()
{
    IDE_TEST( mMeta.checkItemReplaceHistoryAndSetTableOID() != IDE_SUCCESS );
    IDE_TEST( handshakeWithoutReconnect( SM_NULL_TID ) != IDE_SUCCESS );

    IDE_TEST( failbackMaster() != IDE_SUCCESS );
            
    mExitFlag = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpxSender::xlogfileFailbackSlave()
{
    IDE_TEST( failbackSlave() != IDE_SUCCESS );
            
    mExitFlag = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
