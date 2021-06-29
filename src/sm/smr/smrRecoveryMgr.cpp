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
 * $Id: smrRecoveryMgr.cpp 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <svm.h>
#include <smu.h>
#include <sdb.h>
#include <sct.h>
#include <smr.h>
#include <sds.h>
#include <smx.h>
#include <sdr.h>
#include <smrReq.h>

smrLogAnchorMgr    smrRecoveryMgr::mAnchorMgr;
idBool             smrRecoveryMgr::mFinish;
idBool             smrRecoveryMgr::mABShutDown;
idBool             smrRecoveryMgr::mRestart;
idBool             smrRecoveryMgr::mRedo;
smGetMinSN         smrRecoveryMgr::mGetMinSNFunc;
smLSN              smrRecoveryMgr::mIdxSMOLSN;
iduMutex           smrRecoveryMgr::mDeleteLogFileMutex;
idBool             smrRecoveryMgr::mRestartRecoveryPhase = ID_FALSE;
idBool             smrRecoveryMgr::mMediaRecoveryPhase   = ID_FALSE;
smManageDtxInfo    smrRecoveryMgr::mManageDtxInfoFunc;
smGetDtxMinLSN     smrRecoveryMgr::mGetDtxMinLSNFunc;

// PR-14912
idBool             smrRecoveryMgr::mRefineDRDBIdx;

/* BUG-42785 */
SInt               smrRecoveryMgr::mOnlineDRDBRedoCnt;

/* PROJ-2118 BUG Reporting - Debug Info for Fatal */
smLSN              smrRecoveryMgr::mLstRedoLSN    = { 0,0 };
smLSN              smrRecoveryMgr::mLstUndoLSN    = { 0,0 };
SChar*             smrRecoveryMgr::mCurLogPtr     = NULL;
smrLogHead*        smrRecoveryMgr::mCurLogHeadPtr = NULL;

/* PROJ-2162 RestartRiskReduction */
smrEmergencyRecoveryPolicy smrRecoveryMgr::mEmerRecovPolicy;
idBool                     smrRecoveryMgr::mDurability        = ID_TRUE;
idBool                     smrRecoveryMgr::mDRDBConsistency   = ID_TRUE;
idBool                     smrRecoveryMgr::mMRDBConsistency   = ID_TRUE;
idBool                     smrRecoveryMgr::mLogFileContinuity = ID_TRUE;
SChar                      smrRecoveryMgr::mLostLogFile[SM_MAX_FILE_NAME];
smLSN                      smrRecoveryMgr::mEndChkptLSN;
smLSN                      smrRecoveryMgr::mLstDRDBRedoLSN;
iduMutex                   smrRecoveryMgr::mIOLMutex;
smrRTOI                    smrRecoveryMgr::mIOLHead;
UInt                       smrRecoveryMgr::mIOLCount;

/* PROJ-2133 incremental backup */
UInt        smrRecoveryMgr::mCurrMediaTime;
SChar       smrRecoveryMgr::mLastRestoredTagName[ SMI_MAX_BACKUP_TAG_NAME_LEN ];

smLSN       smrRecoveryMgr::mSkipRedoLSN       = { 0, 0 };

UInt        smrRecoveryMgr::mLastRemovedFileNo;

// * REPLICATION concern function where   * //
// * stubs make not NULL function forever * //

UInt   stubGetMinFileNoFunc() { return ID_UINT_MAX; };
IDE_RC stubIsReplCompleteBeforeCommitFunc( idvSQL *, 
                                           const  smTID, 
                                           const  smSN, 
                                           const  UInt ) 
{ return IDE_SUCCESS; };

void   stubIsReplCompleteAfterCommitFunc( idvSQL *, 
                                          const smTID, 
                                          const smSN,
                                          const smSN, 
                                          const UInt, 
                                          const smiCallOrderInCommitFunc ) 
{ return; };
void   stubCopyToRPLogBufFunc(idvSQL*, UInt, SChar*, smLSN){ return; };
/* PROJ-2453 Eager Replication performance enhancement */
void   stubSendXLogFunc( const SChar* )
{ 
    return; 
};

IDE_RC stubIsReplWaitGlobalTxAfterPrepareFunc( idvSQL       * /* aStatistics */,
                                               idBool         /* aIsRequestNode */,
                                               const smTID    /* aTID */ ,
                                               const smSN     /* aSN */ )
{
    return IDE_SUCCESS;
};

smIsReplCompleteBeforeCommit smrRecoveryMgr::mIsReplCompleteBeforeCommitFunc = stubIsReplCompleteBeforeCommitFunc;
smIsReplCompleteAfterCommit  smrRecoveryMgr::mIsReplCompleteAfterCommitFunc = stubIsReplCompleteAfterCommitFunc;
//PROJ-1670: Log Buffer for Replication
smCopyToRPLogBuf smrRecoveryMgr::mCopyToRPLogBufFunc = stubCopyToRPLogBufFunc;
/* PROJ-2453 Eager Replication performance enhancement */
smSendXLog smrRecoveryMgr::mSendXLogFunc = stubSendXLogFunc;
/* PROJ-2747 Global Tx Consistent */
smIsReplWaitGlobalTxAfterPrepare smrRecoveryMgr::mIsReplWaitGlobalTxAfterPrepareFunc = stubIsReplWaitGlobalTxAfterPrepareFunc;

void smrRecoveryMgr::setCallbackFunction(
                            smGetMinSN                   aGetMinSNFunc,
                            smIsReplCompleteBeforeCommit aIsReplCompleteBeforeCommitFunc,
                            smIsReplCompleteAfterCommit  aIsReplCompleteAfterCommitFunc,
                            smCopyToRPLogBuf             aCopyToRPLogBufFunc,
                            smSendXLog                   aSendXLogFunc,
                            smIsReplWaitGlobalTxAfterPrepare aIsReplWaitGlobalTxAfterPrepareFunc )
{
    mGetMinSNFunc   = aGetMinSNFunc;

    if ( aIsReplCompleteBeforeCommitFunc != NULL)
    {
        mIsReplCompleteBeforeCommitFunc = aIsReplCompleteBeforeCommitFunc;
    }
    else
    {
        mIsReplCompleteBeforeCommitFunc = stubIsReplCompleteBeforeCommitFunc;
    }
    if ( aIsReplCompleteAfterCommitFunc != NULL)
    {
        mIsReplCompleteAfterCommitFunc = aIsReplCompleteAfterCommitFunc;
    }
    else
    {
        mIsReplCompleteAfterCommitFunc = stubIsReplCompleteAfterCommitFunc;
    }
    /*PROJ-1670: Log Buffer for Replication*/
    /* BUG-35392 */
    mCopyToRPLogBufFunc = aCopyToRPLogBufFunc;

    /* PROJ-2453 Eager Replication performance enhancement */
    if ( aSendXLogFunc != NULL )
    {
        mSendXLogFunc = aSendXLogFunc;
    }
    else
    {
        mSendXLogFunc = stubSendXLogFunc;
    }

    if ( aIsReplWaitGlobalTxAfterPrepareFunc != NULL )
    {
        mIsReplWaitGlobalTxAfterPrepareFunc = aIsReplWaitGlobalTxAfterPrepareFunc;
    }
    else
    {
        mIsReplWaitGlobalTxAfterPrepareFunc = stubIsReplWaitGlobalTxAfterPrepareFunc;
    }
}

/***********************************************************************
 * Description : createdb�������� �α� ���� �۾� ����
 *
 * createdb �������� ȣ��Ǵ� �Լ��ν� �α׾�Ŀ ���� ��
 * ù��° �α�����(logfile0)�� �����ϰ�, ���� �����ڸ�
 * �ʱ�ȭ�ϸ�, �α����� ������ thread�� �����Ѵ�.
 **********************************************************************/
IDE_RC smrRecoveryMgr::create()
{
    /* create loganchor file */
    IDE_TEST( mAnchorMgr.create() != IDE_SUCCESS );
    /* create log files */
    IDE_TEST( smrLogMgr::create() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ������ �ʱ�ȭ
 *
 * A4������ ���� �α׾�Ŀ����(3���� ���纻����)�� �����Ѵ�.
 * �׷��Ƿ�, ���������� �ʱ�ȭ�������� �α׾�Ŀ ������
 * �о �޸𸮿� �����ϴ� ������ ó���ϱ� ���� ���� ����
 * �α׾�Ŀ ���� �� �ý��� ������ ����� ��ȿ�� �α׾�Ŀ
 * �� �����ϴ� ������ �ʿ��ϴ�.
 *
 * - 2nd. code design
 *   + MRDB�� DRDB�� redo/undo �Լ� ������ map�� �ʱ�ȭ�Ѵ�.
 *   + loganchor �����ڸ� �ʱ�ȭ�Ѵ�.
 *   + �α������� �غ��ϰ�, �α����� ������(smrLogFileMgr) thread��
 *     �����Ѵ�.
 *   + �α����� Sync thread�� �ʱ�ȭ�ϰ�, dirty page list
 *     (smrDirtyPageList)�� �ʱ�ȭ�Ѵ�.
 **********************************************************************/
IDE_RC smrRecoveryMgr::initialize()
{
    mGetMinSNFunc       = NULL;
    /* mManageDtxInfoFunc and mGetDtxMinLSNFunc are initailized at PRE-PROCESS phase */

    IDE_TEST( mDeleteLogFileMutex.initialize( (SChar*) "Delete LogFile Mutex",
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );

    /* BUG-42785 mOnlineDRDBRedoCnt�� ���� OnlineDRDBRedo�� �����ϰ� �ִ�
     * Ʈ������� ���� ��Ÿ����. */
    mOnlineDRDBRedoCnt = 0;

    /* ------------------------------------------------
     * MRDB�� DRDB�� redo/undo �Լ� ������ map �ʱ�ȭ
     * ----------------------------------------------*/
    smrUpdate::initialize();
    smLayerCallback::initRecFuncMap();

    /* PROJ-2162 RestartRiskReduction */
    mDurability          = ID_TRUE;
    mDRDBConsistency     = ID_TRUE;
    mMRDBConsistency     = ID_TRUE;
    mLogFileContinuity   = ID_TRUE;

    SM_LSN_MAX( mEndChkptLSN ); // BUG-47404 ���� WALȮ���� ���� UINT_MAX �� ������.
    SM_LSN_INIT( mLstRedoLSN );

    IDE_TEST( initializeIOL() != IDE_SUCCESS );

    /* ------------------------------------------------
     * loganchor ������ �ʱ�ȭ�������� invalid��
     * loganchor ����(��)�� ���ؼ� valid�� ������
     * flush �Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST( mAnchorMgr.initialize() != IDE_SUCCESS );

    /* PROJ-2133 incremental backup */
    if ( isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smriChangeTrackingMgr::begin() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( getBIMgrState() != SMRI_BI_MGR_FILE_REMOVED )
    {
        IDE_TEST( smriBackupInfoMgr::begin() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* PROJ-2133 incremental backup */
    mCurrMediaTime        = ID_UINT_MAX;
    initLastRestoredTagName();

    mFinish               = ID_FALSE;
    mRestart              = ID_FALSE;
    mRedo                 = ID_FALSE;
    mMediaRecoveryPhase   = ID_FALSE; /* �̵�� ���� �����߿��� TRUE */
    mRestartRecoveryPhase = ID_FALSE;

    mRefineDRDBIdx = ID_FALSE;

    mLastRemovedFileNo = 0;

    SM_SET_LSN(mIdxSMOLSN, 0, 0);

    // �� Tablespace�� dirty page�����ڸ� �����ϴ� ��ü �ʱ�ȭ
    IDE_TEST( smrDPListMgr::initializeStatic() != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::initializeCorruptPageMgr( sdbBufferMgr::getPageCount() )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description : Server Shutdown�� ���������� �����۾� ����
 *
 * ��������������� ����Ǵ� ��������� �����Ѵٸ� ��� ��ҽ�Ű��, ����� ȸ����
 * ������ �ϱ� ���ؼ� ��� Dirty �������� ��� ������, üũ����Ʈ�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::finalize()
{
    sdbCPListSet * sCPListSet;
    UInt           sBCBCount;
    smLSN          sEndLSN;
    UInt           sLstCreatedLogFile;

    mFinish    = ID_TRUE;

    if ( smrBackupMgr::getBackupState() != SMR_BACKUP_NONE )
    {
        sddDiskMgr::abortBackupAllTableSpace( NULL /* idvSQL* */); 
    }

    if ( smrLogMgr::getLogFileMgr().isStarted() == ID_TRUE )
    {
        /* PROJ-2102 */
        IDE_TEST( sdsFlushMgr::turnOnAllflushers() != IDE_SUCCESS );
        
        IDE_TEST( sdsBufferMgr::flushDirtyPagesInCPList( NULL,
                                                         ID_TRUE )// FLUSH ALL
                  != IDE_SUCCESS );

        /* To fix BUG-22679 */
        IDE_TEST( sdbFlushMgr::turnOnAllflushers() != IDE_SUCCESS );

        // To fix BUG-17953
        // checkpoint -> checkpoint -> flush�� ������
        // flush -> checkpoint -> checkpoint�� ����
        IDE_TEST( sdbBufferMgr::flushDirtyPagesInCPList(NULL,
                                                       ID_TRUE)// FLUSH ALL
                 != IDE_SUCCESS );

        IDE_TEST( checkpoint( NULL, /* idvSQL* */
                              SMR_CHECKPOINT_BY_SYSTEM,
                              SMR_CHKPT_TYPE_BOTH,
                              ID_FALSE,
                              ID_TRUE )
                  != IDE_SUCCESS );

        IDE_TEST( checkpoint( NULL, /* idvSQL* */
                              SMR_CHECKPOINT_BY_SYSTEM,
                              SMR_CHKPT_TYPE_BOTH,
                              ID_FALSE,
                              ID_TRUE)
                  != IDE_SUCCESS );

        sCPListSet = sdbBufferMgr::getPool()->getCPListSet();
        sBCBCount  = sCPListSet->getTotalBCBCnt();

        /* BUG-40139 The server can`t realize that there is dirty pages in buffer 
         * when the server shutdown in normal 
         * ���������� Checkpoint List�� BCB�� �����ִٸ�(dirty page)��
         * �����Ѵٸ� ������ ���� �Ͽ� restart recovery�� �����ϰ� �Ѵ�.*/
        IDE_ASSERT_MSG( sBCBCount == 0,
                        "Forced shutdown due to remaining dirty pages in CPList(%"ID_UINT32_FMT")\n", 
                        sBCBCount);

        smrLogMgr::getLstLSN( &sEndLSN );

        // Loganchor�� EndLSN�� checkpoint���� Memory RedoLSN �Ǵ�
        // Shutdown���� EndLSN���� ���Ǳ⶧���� Shutdown�ÿ���
        // üũ����Ʈ�̹��� ��Ÿ����� ������ �������־�� �Ѵ�.
        sctTableSpaceMgr::setRedoLSN4DBFileMetaHdr( NULL,       // Disk Redo LSN
                                                    &sEndLSN ); // Mem Redo LSN

        // üũ����Ʈ�̹����� ��Ÿ����� RedoLSN�� ����Ѵ�.
        IDE_TEST( smmTBSMediaRecovery::updateDBFileHdr4AllTBS()
                  != IDE_SUCCESS );

        /* ------------------------------------------------
         * loganchor ����
         * - ���� shutdown ���� ����
         * - last LSN ����
         * - last create log ��ȣ ����
         * ----------------------------------------------*/
        smrLogMgr::getLogFileMgr().getLstLogFileNo( &sLstCreatedLogFile );

        IDE_TEST( mAnchorMgr.updateSVRStateAndFlush( SMR_SERVER_SHUTDOWN,
                                                     &sEndLSN,
                                                     &sLstCreatedLogFile )
                  != IDE_SUCCESS );

        IDE_TEST( updateAnchorAllTBS() != IDE_SUCCESS );
        /* PROJ-2102 Fast Secondary Buffer */
        IDE_TEST( updateAnchorAllSB() != IDE_SUCCESS ); 
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ������ ����
 *
 * ��������������� �α� ���� �ڿ��� �����ϰ�, �α׾�Ŀ�� ����������� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::destroy()
{
    IDE_TEST( smLayerCallback::destroyCorruptPageMgr() != IDE_SUCCESS );

    IDE_TEST( smrDPListMgr::destroyStatic() != IDE_SUCCESS );

    //PROJ-2133 incremental backup
    if ( isCTMgrEnabled() == ID_TRUE )
    {  
        IDE_TEST( smriChangeTrackingMgr::end() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }
 
    if ( getBIMgrState() == SMRI_BI_MGR_INITIALIZED )
    {
        IDE_TEST( smriBackupInfoMgr::end() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_TEST( mAnchorMgr.destroy() != IDE_SUCCESS );

    IDE_TEST( finalizeIOL() != IDE_SUCCESS );

    IDE_TEST( mDeleteLogFileMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : restart ����
 *
 * restart �Լ� ��� �и� ������ ���� 4���� �Լ��� �и��Ѵ�.
 *
 * - initialize restart ����
 *   + restart ó���� ǥ��
 *   + �ʿ���� logfile���� ����
 *   + stableDB�� ���Ͽ� prepareDB->restoreDB
 *   + ���� shutdown ���¿� ���� recovery�� �ʿ��Ѱ�
 *     �Ǵ�
 * - restart normal ����
 *   + ������ �������Ḧ �Ͽ��⶧����, ������ �ʿ���� �����
 *   + �α׾�Ŀ �������� updateFlush
 *   + log ���� ������ ����
 *
 * - restart recovery ����
 *   + active transaction list
 *   + redoAll ó��
 *   + DRDB table�� ���Ͽ� index header �ʱ�ȭ
 *   + undoAll ó��
 *
 * - finalize restart ����
 *   + flag ���� �� ������ ���� dbfile ���� ����
 **********************************************************************/
IDE_RC smrRecoveryMgr::restart( UInt aPolicy )
{
    idBool                sNeedRecovery;

    // fix BUG-17158
    // offline DBF�� write�� �����ϵ��� flag �� �����Ѵ�.
    sddDiskMgr::setEnableWriteToOfflineDBF( ID_TRUE );

    /* ------------------------------------------------
     * 1. restart �غ� ���� -> smrRecoveryMgr::initRestart()
     *
     * - restart ó���� ǥ��
     * - �ʿ���� logfile���� ����
     * - stableDB�� ���Ͽ� prepareDB->restoreDB
     * - ���� shutdown ���¿� ���� recovery�� �ʿ��Ѱ�
     *   �Ǵ�
     * ----------------------------------------------*/
    IDE_TEST( initRestart( &sNeedRecovery ) != IDE_SUCCESS );

    /* ------------------------------------------------
     * BUG-12246 ���� ������ ARCHIVE�� �α������� ���ؾ���
     * 1) ������ ������������ ���
     * lst delete logfile no ���� recovery lsn���� archive dir��
     * �˻��Ͽ� �������� �ʴ� �α����ϸ� �ٽ� archive list�� ����Ѵ�.
     * �� ���Ĵ� redoAll �ܰ迡�� ����ϵ��� �Ѵ�.
     * 2) ������ ���������� ���
     * lst delete logfile no���� end lsn���� archive dir�� �˻��Ͽ�
     * �������� �ʴ� �α����ϸ� �ٽ� archive list�� ����Ѵ�.
     * ----------------------------------------------*/

    /* PROJ-2162 RestartRiskReduction */
    mEmerRecovPolicy = (smrEmergencyRecoveryPolicy)aPolicy;
    if ( mEmerRecovPolicy == SMR_RECOVERY_SKIP )
    {
        IDE_CALLBACK_SEND_MSG(
            "----------------------------------------"
            "----------------------------------------\n"
            "                            "
            "Emergency - skip restart recovery\n"
            "----------------------------------------"
            "----------------------------------------\n");

        sNeedRecovery    = ID_FALSE; /* Recovery ���� */

        IDE_TEST( prepare4DBInconsistencySetting() != IDE_SUCCESS );
        mMRDBConsistency = ID_FALSE; /* DB Consistency���� ���� */
        mDRDBConsistency = ID_FALSE;
        mDurability      = ID_FALSE;
        IDE_TEST( finish4DBInconsistencySetting() != IDE_SUCCESS );
    }
    
    /* PROJ-2102 Fast Secondary Buffer �� ���� BCB �籸�� */    
    IDE_TEST( sdsBufferMgr::restart( sNeedRecovery ) != IDE_SUCCESS );

    if (sNeedRecovery != ID_TRUE) /* ������ �ʿ���ٸ� */
    {
        /* ------------------------------------------------
         * 2-1. ���� ���� -> smrRecoveryMgr::restartNormal()
         *
         * - ������ �������Ḧ �Ͽ��⶧����, ������ �ʿ���� �����
         * - �α׾�Ŀ �������� updateFlush
         * - log ���� ������ ����
         * - DRDB table�� ���Ͽ� index header �ʱ�ȭ
         * ----------------------------------------------*/
        IDE_TEST( restartNormal() != IDE_SUCCESS );
    }
    else /* ������ �ʿ��ϴٸ� */
    {
        /* ------------------------------------------------
         * 2-2. restart recovery ->smrRecoveryMgr::restartRecovery()
         * - active transaction list
         * - redoAll ó��
         * - DRDB table�� ���Ͽ� index header �ʱ�ȭ
         * - undoAll ó��
         * ----------------------------------------------*/
        IDE_TEST( restartRecovery() != IDE_SUCCESS );
    }

    // fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline �����
    // �ùٸ��� Index Runtime Header �������� ����
    // Restart REDO, UNDO�����Ŀ� Offline ������ Disk Tablespace��
    // Index Runtime Header�� �����Ѵ�.
    IDE_TEST( finiOfflineTBS(NULL /* idvSQL* */) != IDE_SUCCESS );

    /* ------------------------------------------------
     * 3. smrRecoveryMgr::finalRestart
     * restart ���� ������
     * - flag ���� �� ������ ���� dbfile ���� ����
     * ----------------------------------------------*/
    (void)smrRecoveryMgr::finalRestart();

    // offline DBF�� write�� �Ұ����ϵ��� flag �� �����Ѵ�.
    sddDiskMgr::setEnableWriteToOfflineDBF( ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    sddDiskMgr::setEnableWriteToOfflineDBF( ID_FALSE );

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : restart �غ� ���� ����
 *
 * �ý��� restart �غ� ������ �ʱ�ȭ �۾��� �����Ѵ�.
 * �ʿ���� �α������� �����ϰ�, checkpoint�� �ݿ��� DB�� �ε��� ��,
 * �ý����� ���� ������¿� ���� ���������� �ʿ����� �����Ѵ�.
 *
 * - 2nd. code design
 *   + restart ó���� ǥ��
 *   + �ʿ���� logfile���� ����
 *   + stableDB�� ���Ͽ� prepareDB,
 *   +  membase�� timestamp�� ����
 *   + checkpoint�� �ݿ��� �̹����� restoreDB
 *   + ���� shutdown ���¿� ���� recovery�� �ʿ��Ѱ� ����
 **********************************************************************/
IDE_RC smrRecoveryMgr::initRestart( idBool*  aIsNeedRecovery )
{
    UInt        sFstDeleteLogFile;
    UInt        sLstDeleteLogFile;

    mRestart = ID_TRUE;

    /* remove log file that is not needed for recovery */
    mAnchorMgr.getFstDeleteLogFileNo( &sFstDeleteLogFile );
    mAnchorMgr.getLstDeleteLogFileNo( &sLstDeleteLogFile );

    /* BUG-40323 */
    // ������ ������ ������ �������� ���� �õ�
    if ( sFstDeleteLogFile != sLstDeleteLogFile )
    {
        (void)smrLogMgr::getLogFileMgr().removeLogFile( sFstDeleteLogFile,
                                                        sLstDeleteLogFile,
                                                        ID_FALSE /* Not Checkpoint */ );

        // fix BUG-20241 : ������ LogFileNo�� LogAnchor ����ȭ
        IDE_TEST( mAnchorMgr.updateFstDeleteFileAndFlush()
                  != IDE_SUCCESS );
    }
    else
    {
        // do nothing
    }

    *aIsNeedRecovery = ( ( mAnchorMgr.getStatus() != SMR_SERVER_SHUTDOWN ) ?
                         ID_TRUE : ID_FALSE );

    IDE_CALLBACK_SEND_MSG("  [SM] Recovery Phase - 1 : Preparing Database");

    IDE_TEST( smmManager::prepareDB( ( *aIsNeedRecovery == ID_TRUE ) ?
                                      SMM_PREPARE_OP_DBIMAGE_NEED_RECOVERY :
                                      SMM_PREPARE_OP_NONE)
             != IDE_SUCCESS );

    /* PROJ-1594 Volatile TBS */
    /* Volatile Tablespace���� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( svmTBSStartupShutdown::prepareAllTBS() != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("  [SM] Recovery Phase - 2 : Loading Database ");

    /*BUG-BUG-17697 Performance Center P-14 Startup �������� ����� �̻���
     *
     *���� �ڵ忡�� "BEGIN DATABASE RESTORATION" �� �޼����� �����.
     *���ָ� �ȵ�. */
    ideLog::log(IDE_SERVER_0, "     BEGIN DATABASE RESTORATION\n");

    IDE_TEST( smmManager::restoreDB( ( *aIsNeedRecovery == ID_TRUE ) ?
                                    SMM_RESTORE_OP_DBIMAGE_NEED_RECOVERY :
                                    SMM_RESTORE_OP_NONE ) != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0, "     END DATABASE RESTORATION\n");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ �ʿ���� ���� ����
 *
 * system shutdown ������ normal, immediate, abort�� 3������
 * ������, �� �߿��� normal�� immediate shutdown�� �ش��ϴ�
 * ���������� ������ ����� ��쿡 ���󱸵��Ѵ�.
 * �ֳ��ϸ�, ��� ������ Ʈ������� �Ϸ�(���� öȸ�� ���������)
 * ������ active Ʈ������� �������� ������, ����, �������� checkpoint��
 * �����Ͽ� �����ϱ� ������ dirty page�� ��� disk�� �ݿ��Ǳ� �����̴�.
 * ���������� DRDB ���� ��������ÿ��� ���ǿϷ��� ���� ��������
 * flush ����� ��� page�� ��ũ�� �ݿ��Ͽ� �����ϵ��� �Ѵ�.
 *
 * - 2nd. code design
 *   + ������ �������Ḧ �Ͽ��⶧����, ������ �ʿ���� �����
 *   + �α׾�Ŀ �������� updateFlush
 *   + log ���� ������ ����
 *   + DRDB table�� ���Ͽ� index header �ʱ�ȭ
 **********************************************************************/
IDE_RC smrRecoveryMgr::restartNormal()
{

    smLSN   sEndLSN;
    UInt    sLstCreatedLogFileNo;

    mAnchorMgr.getEndLSN( &sEndLSN );
    mAnchorMgr.getLstCreatedLogFileNo( &sLstCreatedLogFileNo );

    /* Transaction Free List�� Valid���� �˻��Ѵ�. */
    smLayerCallback::checkFreeTransList();

    /* Archive ��� Logfile��  Archive List�� �߰��Ѵ�. */
    if ( getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        IDE_TEST( rebuildArchLogfileList( &sEndLSN )
                  != IDE_SUCCESS );
    }

    IDE_CALLBACK_SEND_MSG("  [SM] Recovery Phase - 3 : Skipping Recovery & Starting Threads...");

    IDE_TEST( mAnchorMgr.updateSVRStateAndFlush( SMR_SERVER_STARTED,
                                                 &sEndLSN,
                                                 &sLstCreatedLogFileNo )
             != IDE_SUCCESS );

    /* ------------------------------------------------
     * page�� index SMO No. �ʱ�ȭ�� ��� [���󱸵���]
     * Index SMO�� Disk�� ���ؼ��� �αװ� ��ϵȴ�. ����
     * Disk�� Lst LSN�� index SMO LSN���� �����Ѵ�.
     * ----------------------------------------------*/
    mIdxSMOLSN = sEndLSN;

    //star log file mgr thread
    ideLog::log(IDE_SERVER_0,"      [SM-PREPARE] Prepare Logfile Thread Start...");
    IDE_TEST( smrLogMgr::startupLogPrepareThread( &sEndLSN,
                                                  sLstCreatedLogFileNo,
                                                  ID_FALSE /* aIsRecovery */ )
             != IDE_SUCCESS );

    /*
     * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
     * space cache�� ��Ȯ�� ����  ��Ʈ�Ѵ�.
     */
    IDE_TEST( sdpTableSpace::refineDRDBSpaceCache() != IDE_SUCCESS );

    // init Disk Table Header & rebuild Index Runtime Header
    IDE_CALLBACK_SEND_MSG("                            Refining Disk Table ");
    IDE_TEST( smLayerCallback::refineDRDBTables() != IDE_SUCCESS );

    /* BUG-27122 __SM_CHECK_DISK_INDEX_INTEGRITY ������Ƽ Ȱ��ȭ��
     * ��ũ �ε��� Integrity üũ�� �����Ѵ�. */
    IDE_TEST( smLayerCallback::verifyIndexIntegrityAtStartUp()
              != IDE_SUCCESS );
    mRefineDRDBIdx = ID_TRUE;

    IDE_TEST( updateAnchorAllTBS() != IDE_SUCCESS );
    IDE_TEST( updateAnchorAllSB() != IDE_SUCCESS ); 

    /*
      PRJ-1548 SM - User Memroy TableSpace ���䵵��
      ���������� �������Ŀ� ��� ���̺����̽���
      DataFileCount�� TotalPageCount�� ����Ͽ� �����Ѵ�.
    */
    sddDiskMgr::calculateFileSizeOfAllTBS();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : restart recovery ����
 *
 * �ý��� abort �߻��� restart �������� recovery�� �����ϸ�,
 * ���� checkpoint �ݿ��������� ������ ����� �α�LSN����
 * Ʈ����� ������� �ϸ�, �Ϸ���� ���� Ʈ����� ����� ���Ͽ�
 * Ʈ����� öȸ�� ó���ϵ��� �Ѵ�.
 *
 * A3�� A4�� memory table�� index�� restart recovery �Ŀ� index��
 * ������ϱ� ������ Ʈ����� öȸ�ܰ迡�� ������ ���� ����.
 * ������, disk table�� persistent index�� ���� ������ Ʈ�����
 * öȸ�ܰ迡�� �����ϵ��� ���־�� �ϱ� ������, öȸ�ܰ� ����
 * disk index�� runtime index�� ���ؼ��� �������־�� �Ѵ�.
 *
 * - 2nd. code design
 *   + active transaction list
 *   + redoAll ó��
 *   + DRDB table�� ���Ͽ� index header �ʱ�ȭ
 *   + undoAll ó��
 **********************************************************************/
IDE_RC smrRecoveryMgr::restartRecovery()
{
    mABShutDown = ID_TRUE;

    IDE_CALLBACK_SEND_MSG("  [SM] Recovery Phase - 3 : Starting Recovery");

    /* PROJ-2162 RestartRiskReduction
     * LogFile�� ���������� ���ϸ� LogFile�� �ջ��� �ִٴ� ����, Durability��
     * ������ ���ɼ��� ����. */
    IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                    ( mLogFileContinuity == ID_FALSE ),
                    err_lost_logfile );

    /* BUG-29633 Recovery�� ��� Transaction�� End�������� ������ �ʿ��մϴ�.
     * Recovery�ÿ� �ٸ������� ���Ǵ� Active Transaction�� �����ؼ��� �ȵȴ�. */
    IDE_TEST_RAISE( smLayerCallback::existActiveTrans() == ID_TRUE,
                    err_exist_active_trans);

    ideLog::log(IDE_SERVER_0, "     BEGIN DATABASE RECOVERY\n");

    IDE_CALLBACK_SEND_MSG("                            Initializing Active Transaction List ");
    smLayerCallback::initActiveTransLst(); // layer callback

    mRestartRecoveryPhase = ID_TRUE;

    //Redo All
    IDE_CALLBACK_SEND_MSG("                            Redo ");

    mRedo = ID_TRUE;
    IDE_TEST( redoAll( NULL /* idvSQL* */) != IDE_SUCCESS );
    mRedo = ID_FALSE;

    /* PROJ-2162 RestartRiskReduction ���� */
    IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                    ( mDurability == ID_FALSE ),
                    err_durability );
    IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                    ( mDRDBConsistency == ID_FALSE ),
                    err_drdb_consistency );
    IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                    ( mMRDBConsistency == ID_FALSE ),
                    err_mrdb_consistency );

    /*
     * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
     * space cache�� ��Ȯ�� ����  ��Ʈ�Ѵ�.
     */
    IDE_TEST( sdpTableSpace::refineDRDBSpaceCache() != IDE_SUCCESS );

    // Init Disk Index Runtime Header
    // fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline �����
    // �ùٸ��� Index Runtime Header �������� ����
    // Refine DRDB ������ Online �������� ȣ��� �� �ִµ�,
    // INVALID_DISK_TBS( DROPPED/DISCARDED ) �ΰ�쿡��
    // Skip �ϰ� �ִ�.
    IDE_CALLBACK_SEND_MSG("                            Refine Disk Table..");
    IDE_TEST( smLayerCallback::refineDRDBTables() != IDE_SUCCESS );

    /* BUG-27122 __SM_CHECK_DISK_INDEX_INTEGRITY ������Ƽ Ȱ��ȭ��
     * ��ũ �ε��� Integrity üũ�� �����Ѵ�. */
    IDE_TEST( smLayerCallback::verifyIndexIntegrityAtStartUp()
              != IDE_SUCCESS );
    mRefineDRDBIdx = ID_TRUE;

    /* �̷�� InconsistentFlag ���� ������ ������ */
    applyIOLAtRedoFinish();

    //Undo All
    IDE_CALLBACK_SEND_MSG("                            Undo ");

    IDE_TEST( undoAll(NULL /* idvSQL* */) != IDE_SUCCESS );

    /* PROJ-2162 RestartRiskReduction
     * Undo �Ŀ� �ٽ� ���� */
    IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                    ( mDurability == ID_FALSE ),
                    err_durability );

    /* Transaction Free List�� Valid���� �˻��Ѵ�. */
    smLayerCallback::checkFreeTransList();

     //------------------------------------------------------------------
    // PROJ-1665 : Corrupted Page���� ���¸� �˻�
    // Corrupted Page�� Redo ���� �� ������ �� ����
    // Corrupted Page�� ���� Extent�� ���°� Free�� ��츦 �����ϰ�
    // Corrupted page�� �����ϴ� ���, fatal error ó��
    //------------------------------------------------------------------
    IDE_TEST( smLayerCallback::checkCorruptedPages() != IDE_SUCCESS );

    // Restart REDO, UNDO�����Ŀ� Offline ������ Memory Tablespace��
    // �޸𸮸� ���� �ڼ��� ������ finiOfflineTBS�� �ּ�����
    IDE_TEST( smLayerCallback::finiOfflineTBS() != IDE_SUCCESS );
    // Disk�� RestartNormal�̰� Restart Recovery �̰� ��� ó���ؾ��ϹǷ�
    // restart()���� �ϰ�ó���Ѵ�.

    IDE_TEST( updateAnchorAllTBS() != IDE_SUCCESS );
    IDE_TEST( updateAnchorAllSB() !=  IDE_SUCCESS ); 

    /*
      PRJ-1548 SM - User Memroy TableSpace ���䵵��
      ���������� �������Ŀ� ��� ���̺����̽���
      DataFileCount�� TotalPageCount�� ����Ͽ� �����Ѵ�.
    */
    sddDiskMgr::calculateFileSizeOfAllTBS();

    mRestartRecoveryPhase = ID_FALSE;


#if defined( DEBUG_PAGE_ALLOC_FREE )
    smmFPLManager::dumpAllFPLs();
#endif

    // �����ͺ��̽� Page�� Free Page���� Allocated Page������ Free List Info Page��
    // ���� �� �� �ִµ�, �� ������ Restart Recovery�� ������ ������ �Ϸ�ǹǷ�,
    // Restart Recovery�� �Ϸ�� �Ŀ� Free Page���� ���θ� �а��� �� �ִ�.

    // Restart Recovery������ Free Page����, Allocated Page���� �������� �ʰ�
    // ������ �������� ��ũ�κ��� �޸𸮷� �ε��Ѵ�.
    // �׸��� Restart Recovery�� �Ϸ�ǰ� ����, ���ʿ��ϰ� �ε�� Free Page����
    // Page �޸𸮸� �޸𸮸� �ݳ��Ѵ�.

    IDE_TEST( smmManager::freeAllFreePageMemory() != IDE_SUCCESS );


    ideLog::log(IDE_SERVER_0, "     END DATABASE RECOVERY\n");

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_lost_logfile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_LOG_FILE_MISSING, mLostLogFile ));
    }
    IDE_EXCEPTION( err_durability );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FAILURE_DURABILITY_AT_STARTUP ));
    }
    IDE_EXCEPTION( err_drdb_consistency );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FAILURE_DRDB_WAL_AT_STARTUP ));
    }
    IDE_EXCEPTION( err_mrdb_consistency );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FAILURE_MRDB_WAL_AT_STARTUP ));
    }
    IDE_EXCEPTION( err_exist_active_trans);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_EXIST_ACTIVE_TRANS_IN_RECOV ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : restart ���� ������
 * - 2nd. code design
 *   + flag ���� �� ������ ���� dbfile ���� ����
 **********************************************************************/
void smrRecoveryMgr::finalRestart()
{
    smmManager::setLstCreatedDBFileToAllTBS();

    mRestart = ID_FALSE;

    smmDatabase::makeMembaseBackup();

    return;
}

/*
  SKIP�� REDO�α����� ���θ� �����Ѵ�.

  - MEDIA Recovery��
  - DISCARD, DROP�� Tablespace�� ���� Redo Skip�ǽ�
  - Restart Recovery��
  - DISCARD, DROP, OFFLINE�� Tablespace�� ���� Redo Skip�ǽ�

  ��, Tablespace ���º��濡 ���� �α״� Tablespace�� ���¿�
  ������� REDO�Ѵ�.
*/
idBool smrRecoveryMgr::isSkipRedo( scSpaceID aSpaceID,
                                   idBool    aUsingTBSAttr )
{
    idBool      sRet;
    sctStateSet sSSet4RedoSkip;

    if ( isMediaRecoveryPhase() == ID_TRUE )
    {
        sSSet4RedoSkip = SCT_SS_SKIP_MEDIA_REDO;
    }
    else
    {
        sSSet4RedoSkip = SCT_SS_SKIP_REDO;
    }

    // Redo SKIP�� Tablespace�� ���� �α����� üũ
    if ( sctTableSpaceMgr::hasState( aSpaceID,
                                     sSSet4RedoSkip,
                                     aUsingTBSAttr ) == ID_TRUE )
    {
        sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    //it could be dropped.... check again using tbs node.
    if ( ( sRet == ID_FALSE ) && ( aUsingTBSAttr == ID_TRUE ) )
    {
         sRet = sctTableSpaceMgr::hasState( aSpaceID,
                                            sSSet4RedoSkip );
    }

    return sRet;
}

/***********************************************************************
 * Description : redo a log
 * [IN] aLogSizeAtDisk - �α����ϻ��� �α� ũ��
 **********************************************************************/
IDE_RC smrRecoveryMgr::redo( void       * aCurTrans,
                             smLSN      * aCurLSN,
                             UInt       * aFileCount,
                             smrLogHead * aLogHead,
                             SChar      * aLogPtr,
                             UInt         aLogSizeAtDisk,
                             idBool       aAfterChkpt )
{
    UInt                i;
    SChar             * sRedoBuffer;
    UInt                sRedoSize;
    SChar             * sAfterImage;
    SChar             * sBeforeImage;
    ULong               sLogBuffer[ (SM_PAGE_SIZE * 2) / ID_SIZEOF(ULong) ];
    smmPCH            * sPCH;
    scGRID            * sArrPageGRID;
    scSpaceID           sSpaceID;
    scPageID            sPageID;
    UInt                sDirtyPage;
    smLSN               sBeginLSN;
    smLSN               sEndLSN;
    smLSN               sCurLSN;                            /* PROJ-1923 */
    smrNTALog           sNTALog;
    smrCMPSLog          sCMPSLog;
    smrUpdateLog        sUpdateLog;
    smrXaPrepareLog     sXAPrepareLog;
    smrTBSUptLog        sTBSUptLog;
    smrXaSegsLog        sXaSegsLog;
    smrTransCommitLog   sCommitLog;
    smrTransAbortLog    sAbortLog;
    smOID               sTableOID;
    idBool              sIsDeleteBit;
    void              * sTrans;
    idBool              sIsExistTBS;
    idBool              sIsFailureDBF;
    UInt                sMemRedoSize;
    smrLogType          sLogType;
    void              * sDRDBRedoLogDataList;
    SChar             * sDummyPtr;
    idBool              sIsDiscarded;

    /* BUG-47525 Group Commit */
    void                   * sGCTrans = NULL;
    UInt                     sGCCnt;
    smTID                    sTID;
 
    IDE_DASSERT( aLogHead   != NULL );
    IDE_DASSERT( aLogPtr    != NULL );

    
    sTrans  = NULL;
    SM_GET_LSN(sBeginLSN, *aCurLSN);
    SM_GET_LSN(sEndLSN, *aCurLSN);
    SM_GET_LSN(sCurLSN, *aCurLSN);

    sEndLSN.mOffset += aLogSizeAtDisk;
    sLogType         = smrLogHeadI::getType(aLogHead);

    switch( sLogType )
    {
        /* ------------------------------------------------
         * ## drdb �α��� parsing �� hash�� ����
         * disk �α׸� �ǵ��� ��� body�� ����Ǿ� �ִ�
         * disk redo�α׵��� �Ľ��Ͽ� (space ID, pageID)��
         * ���� �ؽ����̺� ������ �д�.
         * -> sdrRedoMgr::scanRedoLogRec()
         * ----------------------------------------------*/
        case SMR_DLT_REDOONLY:
        case SMR_DLT_UNDOABLE:
        case SMR_DLT_NTA:
        case SMR_DLT_REF_NTA:
        case SMR_DLT_COMPENSATION:
            smrRecoveryMgr::getDRDBRedoBuffer(
                sLogType,
                smrLogHeadI::getSize( aLogHead ),
                aLogPtr,
                &sRedoSize,
                &sRedoBuffer );

            if ( sRedoSize > 0 )
            {
                IDE_TEST( smLayerCallback::generateRedoLogDataList(
                                                  smrLogHeadI::getTransID( aLogHead ),
                                                  sRedoBuffer,
                                                  sRedoSize,
                                                  &sBeginLSN,
                                                  &sEndLSN,
                                                  &sDRDBRedoLogDataList )
                          != IDE_SUCCESS );
                IDE_TEST( smLayerCallback::addRedoLogToHashTable( sDRDBRedoLogDataList ) 
                          != IDE_SUCCESS );
            }
            break;
        case SMR_LT_TBS_UPDATE:

            idlOS::memcpy(&sTBSUptLog, aLogPtr, SMR_LOGREC_SIZE(smrTBSUptLog));

            // Tablespace ��� ���� ���� �α״� SKIP���� �ʴ´�.
            // - Drop�� Tablespace�� ���
            //   - Redo Function�ȿ��� SKIP�Ѵ�.
            // - Offline�� Tablespace�� ���
            //   - Online�̳� Dropped�� ������ �� �����Ƿ� SKIP�ؼ��� �ȵȴ�.
            // - Discard�� Tablespace�� ���
            //   - Dropped�� ������ �� �����Ƿ� TBS DROP �α׸� redo�Ѵ�.
            sRedoBuffer = aLogPtr 
                          + SMR_LOGREC_SIZE(smrTBSUptLog)
                          + sTBSUptLog.mBImgSize;

            sTrans = smLayerCallback::getTransByTID(
                                     smrLogHeadI::getTransID( &sTBSUptLog.mHead ) );

            /* BUG-41689 A discarded tablespace is redone in recovery
             * Discard�� TBS���� Ȯ���Ѵ�. */
            sIsDiscarded = sctTableSpaceMgr::hasState( sTBSUptLog.mSpaceID,
                                                       SCT_SS_SKIP_AGING_DISK_TBS,
                                                       ID_FALSE );

            if ( ( sIsDiscarded == ID_FALSE ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_MRDB_DROP_TBS ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_DRDB_DROP_TBS ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_DRDB_DROP_DBF ) )
            {
                /*
                   PRJ-1548 User Memory Tablespace
                   ���̺����̽� UPDATE �α׿� ���� ������� �����Ѵ�.
                   �̵�� ���� �߿��� EXTEND_DBF�� ���� redo�� �����ϹǷ�
                   Pending Operation�� ��ϵ� �� �ִ�
                   */
                IDE_TEST( gSmrTBSUptRedoFunction[ sTBSUptLog.mTBSUptType ]( NULL,  /* idvSQL* */
                                                                            sTrans,
                                                                            sCurLSN,  // redo �α��� LSN
                                                                            sTBSUptLog.mSpaceID,
                                                                            sTBSUptLog.mFileID,
                                                                            sTBSUptLog.mAImgSize,
                                                                            sRedoBuffer,
                                                                            mRestart )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do 
                 * discard�� TBS�� redo�� drop tbs�� �����ϰ� �����Ѵ�.*/
            }
            break;

        case SMR_LT_UPDATE:

            idlOS::memcpy(&sUpdateLog, aLogPtr, SMR_LOGREC_SIZE(smrUpdateLog));

            // Redo SKIP�� Tablespace�� ���� �α����� üũ
            if ( isSkipRedo( SC_MAKE_SPACE(sUpdateLog.mGRID) ) == ID_TRUE )
            {
                break;
            }

            sAfterImage = aLogPtr +
                SMR_LOGREC_SIZE(smrUpdateLog) +
                sUpdateLog.mBImgSize;

            if ( isMediaRecoveryPhase() == ID_TRUE )
            {
                // PRJ-1548 User Memory Tablespace
                // Memory �α��� ������ Logical Redo Log�� ���ؼ���
                // �̵����ÿ��� Ư���ϰ� ����Ͽ��� �Ѵ�.
                if ( sUpdateLog.mType ==
                     SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK )
                {
                    IDE_TEST( 
                        smmUpdate::redo4MR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK(
                            SC_MAKE_SPACE(sUpdateLog.mGRID),
                            SC_MAKE_PID(sUpdateLog.mGRID),
                            SC_MAKE_OFFSET(sUpdateLog.mGRID),
                            sAfterImage ) != IDE_SUCCESS );
                    break;
                }
                else
                {
                    // update type�� �°� redo �α׸� ȣ���Ѵ�.
                }
            }
            /* MediaRecovery�ÿ��� �ݿ��ϴ� Log������ �ش� Update�Լ�����
             * ���� �Ǵ��Ͽ� ��� */

            if (gSmrRedoFunction[sUpdateLog.mType] != NULL)
            {
                IDE_TEST( gSmrRedoFunction[sUpdateLog.mType](
                             smrLogHeadI::getTransID(&sUpdateLog.mHead),
                             SC_MAKE_SPACE(sUpdateLog.mGRID),
                             SC_MAKE_PID(sUpdateLog.mGRID),
                             SC_MAKE_OFFSET(sUpdateLog.mGRID),
                             sUpdateLog.mData,
                             sAfterImage,
                             sUpdateLog.mAImgSize,
                             smrLogHeadI::getFlag(&sUpdateLog.mHead) )
                         != IDE_SUCCESS );
            }
            break;

        case SMR_LT_DIRTY_PAGE:
            if ( aAfterChkpt == ID_TRUE )
            {
                idlOS::memcpy((SChar*)sLogBuffer,
                              aLogPtr,
                              smrLogHeadI::getSize(aLogHead));

                sDirtyPage = (smrLogHeadI::getSize(aLogHead) -
                              ID_SIZEOF(smrLogHead) - ID_SIZEOF(smrLogTail))
                    / ID_SIZEOF(scGRID);
                sArrPageGRID = (scGRID*)((SChar*)sLogBuffer +
                                         ID_SIZEOF(smrLogHead));

                for(i = 0; i < sDirtyPage; i++)
                {
                    sSpaceID = SC_MAKE_SPACE(sArrPageGRID[i]);


                    // Redo SKIP�� Tablespace�� ���� �α����� üũ
                    /*
                     * BUG-31423 [sm] the server shutdown abnormaly when
                     *           referencing unavalible tablespaces on redoing.
                     *
                     * TBS node�� state���� ó���� restore�� �Ұ����ϴ���
                     * SMM_UPDATE_MRDB_DROP_TBS�� redo�߿� ������  �Ͻ�������
                     * restore�� ������ �Ǵ޵� ���ɼ��� �ֱ� ������ �ŷ��� ��
                     * ����. �׷��Ƿ� TBS attr�� state�� ���� �Ǵ��Ѵ�.
                     */
                    if ( isSkipRedo( sSpaceID,
                                     ID_TRUE ) == ID_TRUE ) //try TBS attribute
                    {
                        continue;
                    }

                    sPageID = SC_MAKE_PID(sArrPageGRID[i]);

                    if ( isMediaRecoveryPhase() == ID_TRUE )
                    {
                        // �̵�� ������ �ʿ��� ����Ÿ������
                        // DIRTY Page�� ����ϱ� ���� Ȯ���Ѵ�.
                        IDE_TEST( smmTBSMediaRecovery::findMatchFailureDBF(
                                      sSpaceID,
                                      sPageID,
                                      &sIsExistTBS,
                                      &sIsFailureDBF )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Restart Recovery�ÿ��� ������
                        // DIRTY page�� ����Ѵ�.
                        sIsExistTBS   = ID_TRUE;
                        sIsFailureDBF = ID_TRUE;
                    }

                    if ( (sIsExistTBS == ID_TRUE) &&
                         (sIsFailureDBF == ID_TRUE) )
                    {
                        // SpaceID�� �ش��ϴ� Tablespace�� Restore���� ���� ���
                        // SMM_PID_PTR�� �ش� Page�� �Ϻ� Restore �ǽ�
                        IDE_TEST( smmManager::getPersPagePtr( sSpaceID,
                                                              sPageID,
                                                              (void**)&sDummyPtr )
                                  != IDE_SUCCESS );

                        sPCH = smmManager::getPCH(sSpaceID, sPageID);
                        smrDPListMgr::add(sSpaceID, sPCH, sPageID);
                    }
                }
            }
            break;

        case SMR_LT_NTA:
            idlOS::memcpy(&sNTALog, aLogPtr, SMR_LOGREC_SIZE(smrNTALog));

            // Redo SKIP�� Tablespace�� ���� �α����� üũ
            if ( isSkipRedo( sNTALog.mSpaceID ) == ID_TRUE )
            {
                break;
            }

            if( aCurTrans != NULL )
            {
                if ( smLayerCallback::IsBeginTrans( aCurTrans ) == ID_TRUE )
                {
                    // �̵�� �����ÿ��� Trans�� ���°� Begin�� �� ����.
                    // Restart Recovery�ÿ��� OID�� ����Ѵ�.
                    IDE_ASSERT( isMediaRecoveryPhase() == ID_FALSE );

                }
            }

            sIsDeleteBit = ID_FALSE;

            switch(sNTALog.mOPType)
            {
                case SMR_OP_SMC_TABLEHEADER_ALLOC:
                    /* BUG-19151: Disk Table �������� ����� Server Startup��
                     *           �ȵ˴ϴ�.
                     *
                     * Create Table�� ���ؼ� Catalog Table���� Slot�� Alloc�ÿ�
                     * �� �αװ� ��ϵȴ�. SlotHeader�� SCN���� Delete Bit�� ������
                     * Redo�ÿ� Delete Bit�� Settting�ϰ� smcTable::createTable��
                     * �Ϸ�ɶ� SMR_OP_CREATE_TABLE�̶�� NTA�αװ� �����µ� ��
                     * �α��� Redo�ÿ� Delete Bit�� Clear��Ų��.
                     *
                     * ����: smcTable::createTable�� �Ϸ���� ���ϰ� �״°��
                     * refineDRDBTables�ÿ� smcTableHeader�� �����ϴ� �� ��������
                     * �����Ⱚ�̵˴ϴ�. �Ͽ� smcTable::createTable�� SMR_OP_CREATE_TABLE
                     * �̶�� NTA�α׸� ������� ������ SlotHeader�� SCN�� DeleteBit��
                     * �����ǰ� ���� refineDBDBTables�� DeleteBit�� �����Ǿ� ������
                     * �� Table�� Skip�ϵ��� �ϱ� ���ؼ� �Դϴ�.
                     */
                    sIsDeleteBit = ID_TRUE;
                case SMR_OP_SMC_FIXED_SLOT_ALLOC:

                    IDE_TEST( smLayerCallback::redo_SMP_NTA_ALLOC_FIXED_ROW(
                                                            sNTALog.mSpaceID,
                                                            SM_MAKE_PID(sNTALog.mData1),
                                                            SM_MAKE_OFFSET(sNTALog.mData1),
                                                            sIsDeleteBit )
                              != IDE_SUCCESS );
                     break;
                case SMR_OP_CREATE_TABLE:
                     sTableOID = (smOID)(sNTALog.mData1);
                     IDE_ASSERT( smmManager::getOIDPtr( sNTALog.mSpaceID,
                                                        sTableOID,
                                                        (void**)&sDummyPtr )
                                 == IDE_SUCCESS );
                    IDE_TEST( smLayerCallback::setDeleteBitOnHeader(
                                                              sNTALog.mSpaceID,
                                                              sDummyPtr,
                                                              ID_FALSE /* Clear Delete Bit */)
                              != IDE_SUCCESS );

                    break;
                default :
                    break;
            }
            break;

        case SMR_LT_COMPENSATION:

            idlOS::memcpy(&sCMPSLog, aLogPtr, SMR_LOGREC_SIZE(smrCMPSLog));

            sBeforeImage = aLogPtr + SMR_LOGREC_SIZE(smrCMPSLog);

            if ( (sctUpdateType)sCMPSLog.mTBSUptType != SCT_UPDATE_MAXMAX_TYPE )
            {
                // �̵�� �����ÿ��� TBS CLR �αװ� ������ �ʴ´�.
                IDE_DASSERT( isMediaRecoveryPhase() == ID_FALSE );

                // Tablespace ��� ���� ���� �α״� SKIP���� �ʴ´�.
                // ��, ���� Undo�����߿� Undo�� �Ϸ�Ǿ����� üũ�ϰ� SKIP
                //
                // Undo��ƾ�� CLR Redo���� ���̱� ������,
                // ������ ���� ����Ǿ ����� ������ �����ؾ� �Ѵ�.
                //
                // - Create Tablespace�� Undo
                //   - �̹� Undo�� �Ϸ�Ǿ� DROPPED������ ��� SKIP
                //
                // - Alter Tablespace �� Undo
                //   - �̹� Alter������ ���·� Undo�� ��� Skip
                //
                // - Drop Tablespace�� Undo
                //   - �̹� Drop Tablespace�� Undo�� �Ϸ�Ǿ����� üũ��
                //     SKIP
                IDE_TEST( gSmrTBSUptUndoFunction[ sCMPSLog.mTBSUptType ](
                              NULL, /* idvSQL* */
                              aCurTrans,
                              sCurLSN,
                              SC_MAKE_SPACE(sCMPSLog.mGRID),
                              sCMPSLog.mFileID,
                              sCMPSLog.mBImgSize,
                              sBeforeImage,
                              mRestart ) != IDE_SUCCESS );
            }
            else
            {
                // Redo SKIP�� Tablespace�� ���� �α����� üũ
                if ( isSkipRedo( SC_MAKE_SPACE(sCMPSLog.mGRID) ) == ID_TRUE )
                {
                    break;
                }

                if ( gSmrUndoFunction[sCMPSLog.mUpdateType] != NULL)
                {
                    IDE_TEST( gSmrUndoFunction[sCMPSLog.mUpdateType](
                                  smrLogHeadI::getTransID(&sCMPSLog.mHead),
                                  SC_MAKE_SPACE(sCMPSLog.mGRID),
                                  SC_MAKE_PID(sCMPSLog.mGRID),
                                  SC_MAKE_OFFSET(sCMPSLog.mGRID),
                                  sCMPSLog.mData,
                                  sBeforeImage,
                                  sCMPSLog.mBImgSize,
                                  smrLogHeadI::getFlag(&sCMPSLog.mHead) )
                              != IDE_SUCCESS );
                }
            }
            break;
        case SMR_LT_XA_PREPARE:

            if ( smLayerCallback::IsBeginTrans( aCurTrans ) == ID_TRUE )
            {
                // �̵�� �����ÿ��� Trans�� ���°� Begin�� �� ����.
                IDE_ASSERT( isMediaRecoveryPhase() == ID_FALSE );
                idlOS::memcpy( &sXAPrepareLog,
                               aLogPtr,
                               SMR_LOGREC_SIZE(smrXaPrepareLog) );

                /* - Ʈ����� ��Ʈ���� xa ���� ���� ���
                   - Update Transaction���� 1����
                   - add trans to Prepared List */
                IDE_TEST( smLayerCallback::setXAInfoAnAddPrepareLst(
                                              aCurTrans,
                                              sXAPrepareLog.mIsGCTx,
                                              sXAPrepareLog.mPreparedTime,
                                              sXAPrepareLog.mXaTransID,
                                              &sXAPrepareLog.mFstDskViewSCN )
                          != IDE_SUCCESS );
            }
            break;

        case SMR_LT_XA_SEGS:

            if ( smLayerCallback::IsBeginTrans( aCurTrans ) == ID_TRUE )
            {
                IDE_ASSERT( isMediaRecoveryPhase() == ID_FALSE );
                idlOS::memcpy( &sXaSegsLog,
                               aLogPtr,
                               SMR_LOGREC_SIZE(smrXaSegsLog) );

                smxTrans::setXaSegsInfo( aCurTrans,
                                         sXaSegsLog.mTxSegEntryIdx,
                                         sXaSegsLog.mExtRID4TSS,
                                         sXaSegsLog.mFstPIDOfLstExt4TSS,
                                         sXaSegsLog.mFstExtRID4UDS,
                                         sXaSegsLog.mLstExtRID4UDS,
                                         sXaSegsLog.mFstPIDOfLstExt4UDS,
                                         sXaSegsLog.mFstUndoPID,
                                         sXaSegsLog.mLstUndoPID );
            }
            break;

        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_DSKTRANS_COMMIT:

            idlOS::memcpy( &sCommitLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE(smrTransCommitLog) );

            sMemRedoSize = smrLogHeadI::getSize(aLogHead) -
                           SMR_LOGREC_SIZE(smrTransCommitLog) -
                           sCommitLog.mDskRedoSize -
                           ID_SIZEOF(smrLogTail);

            // Commit Log�� Body���� Disk Table�� Record Count�� �α�Ǿ��ִ�.
            // Body Size�� ��Ȯ�� ( TableOID, Record Count ) �� ������� üũ.
            IDE_ASSERT( sMemRedoSize %
                        (ID_SIZEOF(scPageID) +
                         ID_SIZEOF(scOffset) +
                         ID_SIZEOF(ULong)) == 0 );

            if ((sMemRedoSize > 0) && (aCurTrans != NULL))
            {
                sAfterImage = aLogPtr + SMR_LOGREC_SIZE(smrTransCommitLog);

                IDE_TEST( smLayerCallback::redoAllTableInfoToDB( sAfterImage,
                                                                 sMemRedoSize,
                                                                 isMediaRecoveryPhase() )
                          != IDE_SUCCESS );
            }

            if ( sCommitLog.mDskRedoSize > 0 )
            {
                sRedoBuffer = aLogPtr +
                              SMR_LOGREC_SIZE(smrTransCommitLog) +
                              sMemRedoSize;
                IDE_TEST( smLayerCallback::generateRedoLogDataList(
                                              smrLogHeadI::getTransID( &sCommitLog.mHead ),
                                              sRedoBuffer,
                                              sCommitLog.mDskRedoSize,
                                              &sBeginLSN,
                                              &sEndLSN,
                                              &sDRDBRedoLogDataList )
                          != IDE_SUCCESS );
                IDE_TEST( smLayerCallback::addRedoLogToHashTable( sDRDBRedoLogDataList ) 
                          != IDE_SUCCESS );
            }

            if ( aCurTrans != NULL )
            {
                IDE_TEST( smLayerCallback::makeTransEnd( aCurTrans,
                                                         ID_TRUE ) /* COMMIT */
                          != IDE_SUCCESS );
            }
            else
            {
                /* Active Transaction�� �ƴ�����, Disk Redo ������
                 * �ش��ϴ� Commit �α��ΰ�� makeTransEnd���ʿ���� */
                IDE_ASSERT( sLogType == SMR_LT_DSKTRANS_COMMIT );
            }

            break;

        case SMR_LT_MEMTRANS_GROUPCOMMIT:
            /* BUG-48543 Recovery Failure in HPUX */
            idlOS::memcpy( &sGCCnt,
                           aLogPtr + SMR_LOG_GROUP_COMMIT_GROUPCNT_OFFSET,
                           ID_SIZEOF( sGCCnt ) );   //BUG-48576: mGroupCnt -> sGCCnt 
                           
            sRedoBuffer = aLogPtr + SMR_LOGREC_SIZE( smrTransGroupCommitLog );

            for ( i = 0 ; i < sGCCnt ; i++ )
            {
                sGCTrans = NULL;

                idlOS::memcpy( &sTID,
                               sRedoBuffer,
                               ID_SIZEOF(smTID) );

                sRedoBuffer += ( ID_SIZEOF(UInt) * 2 );

                sGCTrans = smLayerCallback::getTransByTID( sTID );

                if ( sGCTrans != NULL )
                {
                    IDE_TEST( smLayerCallback::makeTransEnd( sGCTrans,
                                                             ID_TRUE ) /* COMMIT */
                    != IDE_SUCCESS );
                }
            }

            break;

        case SMR_LT_MEMTRANS_ABORT:
        case SMR_LT_DSKTRANS_ABORT:

            idlOS::memcpy( &sAbortLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE(smrTransAbortLog) );

            /* ------------------------------------------------
             * FOR A4 : DRDB ���� DML(select ����)�� ��쿡 ����
             * commit�α� ����Ŀ� ������ ����� �������� ���ߴ�
             * Ʈ������� ��� �������� ���־�� �Ѵ�.
             * - ����ϴ� tss�� commit SCN�� �������� �ʾҴٸ�,
             * tss�� commit list �� �߰��ؼ� ����� disk G.C��
             * garbage collecting�� �� �� �ֵ��� �Ͽ��� �Ѵ�.
             * ----------------------------------------------*/
            // destroy OID, tx end , remvoe Active Transaction list
            if ( sAbortLog.mDskRedoSize > 0 )
            {
                sRedoBuffer = aLogPtr +
                              SMR_LOGREC_SIZE(smrTransAbortLog);

                IDE_TEST( smLayerCallback::generateRedoLogDataList(
                                          smrLogHeadI::getTransID(&sAbortLog.mHead),
                                          sRedoBuffer,
                                          sAbortLog.mDskRedoSize,
                                          &sBeginLSN,
                                          &sEndLSN,
                                          &sDRDBRedoLogDataList )
                          != IDE_SUCCESS );
                IDE_TEST( smLayerCallback::addRedoLogToHashTable(
                                                           sDRDBRedoLogDataList ) 
                          != IDE_SUCCESS );
            }

            if ( aCurTrans != NULL )
            {
                IDE_TEST( smLayerCallback::makeTransEnd( aCurTrans,
                                                         ID_FALSE ) /* ROLLBACK */ 
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_ASSERT( sLogType == SMR_LT_DSKTRANS_ABORT );
            }

            break;

        case SMR_LT_FILE_BEGIN:
            /* SMR_LT_FILE_BEGIN �α��� �뵵�� ���ؼ��� smrDef.h�� ����
             * �� �α׿� ���� üũ�� smrLogFile::open���� �����ϱ� ������
             * redo�߿��� �ƹ��͵� ó������ �ʾƵ� �ȴ�.
             * do nothing */
            break;

        case SMR_LT_FILE_END:
            /* To Fix PR-13786 */
            IDE_TEST( redo_FILE_END( &sEndLSN,
                                     aFileCount )
                      != IDE_SUCCESS );
            break;
        default:
            break;
    }

    // ���� Check�� Log LSN�� �����Ѵ�.
    SM_GET_LSN(*aCurLSN, sEndLSN);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 *
 *    To Fix PR-13786 ���⵵ ����
 *    smrRecoveryMgr::redo() �Լ�����
 *    SMR_LT_FILE_END �α� ó�� �κ��� ���� �и��س�.
 *
 * Implementation :
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::redo_FILE_END( smLSN     * aCurLSN,
                                      UInt      * aFileCount )
{
    IDE_DASSERT( aCurLSN    != NULL );
    IDE_DASSERT( aFileCount != NULL );

    SChar       sLogFilename[ SM_MAX_FILE_NAME ];
    iduFile     sFile;
    UInt        sFileCount;
    ULong       sLogFileSize = 0;

    //------------------------------------------
    // Initialize local variables
    //------------------------------------------
    sFileCount = *aFileCount;

    /* ------------------------------------------------
     * for archive log backup
     * ----------------------------------------------*/
    if ( (getArchiveMode() == SMI_LOG_ARCHIVE) &&
         (smLayerCallback::getRecvType() == SMI_RECOVER_RESTART) )
    {
        IDE_TEST( smrLogMgr::getArchiveThread().addArchLogFile( aCurLSN->mFileNo )
                  != IDE_SUCCESS );
    }

    aCurLSN->mFileNo++;
    aCurLSN->mOffset = 0;

    if (getArchiveMode() == SMI_LOG_ARCHIVE &&
        (getLstDeleteLogFileNo() > aCurLSN->mFileNo))
    {
        idlOS::snprintf(sLogFilename,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        smrLogMgr::getArchivePath(),
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        aCurLSN->mFileNo);
    }
    else
    {
        idlOS::snprintf(sLogFilename,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        smrLogMgr::getLogPath(),
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        aCurLSN->mFileNo);
    }

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( sFile.setFileName(sLogFilename) != IDE_SUCCESS );

    if (idf::access(sLogFilename, F_OK) != 0)
    {
        /* ���� LogFile�� �����Ƿ� ���̻� Redo�� �αװ� ����. */
        smrRedoLSNMgr::setRedoLSNToBeInvalid();
    }
    else
    {
        /* Fix PR-2730 */
        if ( smLayerCallback::getRecvType() == SMI_RECOVER_RESTART )
        {
            IDE_TEST( sFile.open() != IDE_SUCCESS );
            IDE_TEST( sFile.getFileSize(&sLogFileSize) != IDE_SUCCESS );
            IDE_TEST( sFile.close() != IDE_SUCCESS );

            if ( sLogFileSize != smuProperty::getLogFileSize() )
            {
                (void)idf::unlink(sLogFilename);
                /* ���� LogFile�� �����Ƿ� ���̻� Redo�� �αװ� ����. */
                smrRedoLSNMgr::setRedoLSNToBeInvalid();
            }
        }
    }

    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    (sFileCount)++;
    (((sFileCount) % 5) == 0 ) ? IDE_CALLBACK_SEND_SYM("*") : IDE_CALLBACK_SEND_SYM(".");

    /*
     * BUG-26350 [SD] startup�ÿ� redo�� �����Ȳ�� �α����ϸ����� sm log�� ���
     */
    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, "%s", sLogFilename );

    *aFileCount = sFileCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Active Ʈ������� ��ũ �α��߿� ������ ���� �����ϴ� �޸��ڷᱸ����
 *               �ݿ��� ���� ��� �ݿ��Ѵ�.
 *
 * aLogType [IN]  - �α�Ÿ��
 * aLogPtr  [IN]  - SMR_DLT_REF_NTA �α��� RefOffset + ID_SIZEOF(sdrLogHdr)
 *                  �ش��ϴ� LogPtr
 * aContext [OUT] - RollbackContext
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::applyDskLogInstantly4ActiveTrans( void        * aCurTrans,
                                                         SChar       * aLogPtr,
                                                         smrLogHead  * aLogHeadPtr,
                                                         smLSN       * aCurRedoLSNPtr )
{
    scSpaceID           sSpaceID;
    smOID               sTableOID;
    smOID               sIndexOID;
    smrDiskLog          sDiskLogRec;
    smrDiskRefNTALog    sDiskRefNTALogRec;
    SChar            *  sLogBuffer;

    IDE_ASSERT( aCurTrans      != NULL );
    IDE_ASSERT( aLogPtr        != NULL );
    IDE_ASSERT( aLogHeadPtr    != NULL );
    IDE_ASSERT( aCurRedoLSNPtr != NULL );

    IDE_TEST_CONT( smLayerCallback::isTxBeginStatus( aCurTrans )
                   == ID_FALSE, cont_finish );

    switch( smrLogHeadI::getType(aLogHeadPtr) )
    {
        case SMR_DLT_REDOONLY:

            idlOS::memcpy( &sDiskLogRec,
                           aLogPtr,
                           SMR_LOGREC_SIZE(smrDiskLog) );

            // BUG-7983 dml ó���� undo segment commit�� ����, Shutdown��
            // ���� undo ó���� �ʿ��մϴ�.
            if ( sDiskLogRec.mRedoType == SMR_RT_WITHMEM )
            {
                sLogBuffer = aLogPtr +
                             SMR_LOGREC_SIZE(smrDiskLog) +
                             sDiskLogRec.mRefOffset;

                smLayerCallback::redoRuntimeMRDB( aCurTrans,
                                                  sLogBuffer );
            }
            break;

        case SMR_DLT_REF_NTA:

            if ( isVerifyIndexIntegrityLevel2() == ID_TRUE)
            {
                idlOS::memcpy( &sDiskRefNTALogRec,
                               aLogPtr,
                               SMR_LOGREC_SIZE(smrDiskRefNTALog) );

                sLogBuffer = aLogPtr +
                             SMR_LOGREC_SIZE(smrDiskRefNTALog) +
                             sDiskRefNTALogRec.mRefOffset;

                IDE_TEST( smLayerCallback::getIndexInfoToVerify( sLogBuffer,
                                                                 &sTableOID,
                                                                 &sIndexOID,
                                                                 &sSpaceID )
                          != IDE_SUCCESS );

                if ( sIndexOID != SM_NULL_OID )
                {
                    IDE_TEST( smLayerCallback::addOIDToVerify( aCurTrans,
                                                               sTableOID,
                                                               sIndexOID,
                                                               sSpaceID )
                              != IDE_SUCCESS );
                }
            }
            break;

        default:
            break;
    }

    IDE_EXCEPTION_CONT( cont_finish );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrRecoveryMgr::addActiveTrans( smrLogHead    * aLogHeadPtr,
                                       SChar         * aLogPtr,
                                       smTID           aTID,
                                       smLSN         * aCurRedoLSNPtr,
                                       void         ** aCurTrans)
{
    UInt    sTransFlag;
    void  * sCurTrans;

    sCurTrans     = smLayerCallback::getTransByTID( aTID );
    IDE_DASSERT( sCurTrans != NULL );

    switch( smrLogHeadI::getFlag(aLogHeadPtr) & SMR_LOG_TYPE_MASK )
    {
    case SMR_LOG_TYPE_REPLICATED:
        sTransFlag = SMI_TRANSACTION_REPL_NONE;
        break;
    case SMR_LOG_TYPE_REPL_RECOVERY:
        sTransFlag = SMI_TRANSACTION_REPL_RECOVERY;
        break;
    case SMR_LOG_TYPE_NORMAL:
        sTransFlag = SMI_TRANSACTION_REPL_DEFAULT;
        break;
    default:
        IDE_ERROR( 0 );
        break;
    }
    sTransFlag |= SMI_COMMIT_WRITE_NOWAIT;

    /* add active transaction list if tx has beging log. */
    smLayerCallback::addActiveTrans( sCurTrans,
                                     aTID,
                                     sTransFlag,
                                     isBeginLog( aLogHeadPtr ),
                                     aCurRedoLSNPtr );

    IDE_TEST( applyDskLogInstantly4ActiveTrans( sCurTrans,
                                                aLogPtr,
                                                aLogHeadPtr,
                                                aCurRedoLSNPtr ) 
              != IDE_SUCCESS );

    (*aCurTrans) = sCurTrans;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �������������� Ʈ����� ����� (redoall pass)
 *
 * - MRDB�� DRDB�� checkpoint�� ��������
 *   redo ���� LSN�� ���Ͽ� ������ ���� ��찡 �߻�
 *   ���� ��Ȳ�� ���� ���� �ٸ��� ó���ؾ��Ѵ�.
 *   + CASE 0 - active trans�� recvLSN = DRDB�� RecvLSN
 *     : ���ݰ� �����ϰ� ó��
 *   + CASE 1 - active trans�� recvLSN > DRDB�� RecvLSN
 *     : DRDB�� RecvLSN���� active trans�� RecvLSN�� ����������
 *       drdb redo �α׸� �����Ѵ�.
 *       active trans�� RecvLSN���ʹ� ���ݰ� �����ϰ� ó��
 *   + CASE 2 - active trans�� recvLSN < DRDB�� RecvLSN
 *     : active trans�� RecvLSN���� DRDB�� RecvLSN�� ����������
 *       drdb redo �α״� �������� �ʰ�,
 *       �� ���� ���ݰ� �����ϰ� ó���Ѵ�.
 *
 * - Redo �α� ���� ���
 *   + MRDB�� redo�α׸� �ǵ��ϸ� �ٷ� redo �Լ��� ȣ���Ͽ�
 *     �ٷ� �����Ѵ�.
 *   + DRDB�� redo�α׸� �ǵ��ϸ� �ؽ����̺� space ID, page ID
 *     �� ���� redo �α׸� ������ ��, ��� redo �α��ǵ��� �Ϸ��
 *     �Ŀ� �ؽÿ� ����� DRDB�� redo�α׸� disk�� �����Ѵ�.
 *
 * - 2nd. code design
 *   + �α׾�Ŀ�κ��� begin Checkpoint LSN�� ����
 *   + begin checkpoint �α׸� �ǵ��Ͽ�  memory RecvLSN��
 *     disk RecvLSN�� ����
 *   + �� RecvLSN �߿��� ���������� RecvLSN�� �����Ͽ�, RecvLSN����
 *     ������ �αױ��� �ǵ��ϸ鼭 ������� ó���Ѵ�.
 *     : ����� ó���ÿ� �������� ����ߵ��� �̹� disk�� �ݿ���
 *       �α׿� ���ؼ��� skip �ϵ��� �Ѵ�.
 *   + �������� �ǵ��� �α� ����
 *   + !!DRDB�� redo ������ �ʱ�ȭ
 *   + �� �α� Ÿ�Կ� ���� redo ����
 *   + loop - !! �α� �����
 *     : !!disk �α׸� �ǵ��� ��� body�� ����Ǿ� �ִ�
 *       disk redo�α׵��� �Ľ��Ͽ� (space ID, pageID)��
 *       ���� �ؽ����̺� ������ �д�.
 *        -> sdrRedoMgr::scanRedoLogRec()
 *     : mrdb �α״� ������ �����ϰ� ó���Ѵ�.
 *
 *   + !!�ؽ� ���̺� ������ ��, DRDB�� redo �α׵���
 *       disk tablespace�� �ݿ�
 *       -> sdrRedoMgr::applyHashedLogRec()
 *   + !!DRDB�� redo ������ ����
 *   + ��� �α������� close �Ѵ�.
 *   + end of log�� �̿��Ͽ� Log File Manager Thread ���� �� �ʱ�ȭ
 **********************************************************************/
IDE_RC smrRecoveryMgr::redoAll( idvSQL* aStatistics )
{
    UInt                  sState = 0;
    SChar*                sLogPtr;
    void*                 sCurTrans = NULL;
    smTID                 sTID;
    smrLogHead*           sLogHeadPtr;
    smLSN                 sChkptBeginLSN;
    smLSN                 sChkptEndLSN;
    smLSN*                sCurRedoLSNPtr = NULL;
    smLSN                 sDummyRedoLSN4Check;
    smLSN                 sLstRedoLSN;
    smrBeginChkptLog      sBeginChkptLog;
    idBool                sIsValid    = ID_FALSE;
    idBool                sAfterChkpt = ID_FALSE;
    smLSN                 sMemRecvLSNOfDisk;
    smLSN                 sDiskRedoLSN;
    smLSN                 sOldestLSN;
    smLSN                 sMustRedoToLSN;
    UInt                  sCapacityOfBufferMgr;
    smrLogFile*           sLogFile   = NULL;
    UInt                  sFileCount = 1;
    smLSN                 sLstLSN;
    UInt                  sCrtLogFileNo;
    UInt                  sLogSizeAtDisk = 0;
    SChar*                sDBFileName    = NULL;
    smrLogType            sLogType;
    smrRTOI               sRTOI;
    idBool                sRedoSkip          = ID_FALSE;
    idBool                sRedoFail          = ID_FALSE;
    idBool                sObjectConsistency = ID_TRUE;
    smLSN                 sMemBeginLSN; 
    smLSN                 sDiskBeginLSN;

    SM_LSN_MAX( sMemBeginLSN );
    SM_LSN_MAX( sDiskBeginLSN );

    if ( getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        IDE_TEST( smrLogMgr::getArchiveThread().lockThreadMtx()
                  != IDE_SUCCESS );
        sState = 1;
    }

    /* ------------------------------------------------
     * 0. redo �����ڸ� �ʱ�ȭ�Ѵ�.
     * ----------------------------------------------*/
    sCapacityOfBufferMgr = sdbBufferMgr::getPageCount();

    IDE_TEST( smLayerCallback::initializeRedoMgr( sCapacityOfBufferMgr,
                                                  SMI_RECOVER_RESTART )
              != IDE_SUCCESS );

    /* -----------------------------------------------------------------
     * 1. begin checkpoint �α� �ǵ�
     *    Checkpoint log�� ������ Disk�� ��ϵȴ�.
     * ---------------------------------------------------------------- */
    sChkptBeginLSN  = mAnchorMgr.getBeginChkptLSN();
    sChkptEndLSN    = mAnchorMgr.getEndChkptLSN();

    IDE_TEST( smrLogMgr::readLog( NULL, /* ���� ���� �ڵ� => NULL
                                          Checkpoint Log�� ���
                                          �������� �ʴ´� */
                                 &sChkptBeginLSN,
                                 ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                 &sLogFile,
                                 (smrLogHead*)&sBeginChkptLog,
                                 &sLogPtr,
                                 &sIsValid,
                                 &sLogSizeAtDisk )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsValid == ID_FALSE, err_invalid_log );

    idlOS::memcpy(&sBeginChkptLog, sLogPtr, SMR_LOGREC_SIZE(smrBeginChkptLog));

    /* Checkpoint�� ���ؼ� open�� LogFile�� Close�Ѵ�.
     */
    IDE_TEST( smrLogMgr::closeLogFile(sLogFile) != IDE_SUCCESS );
    sLogFile = NULL;

    IDE_ASSERT(smrLogHeadI::getType(&sBeginChkptLog.mHead) == SMR_LT_CHKPT_BEGIN);

    /* ------------------------------------------------
     * 2. RecvLSN ����
     * DRDB�� oldest LSN�� loganchor�� ����� �Ͱ� begin
     * checkpoint �α׿� ����� ���� ���� ū������ ������ ��,
     * Active Ʈ������� minimum LSN�� oldest LSN�� ���ؼ�
     * ���������� RecvLSN�� ����
     * ----------------------------------------------*/
    sMemRecvLSNOfDisk = sBeginChkptLog.mEndLSN;
    sDiskRedoLSN      = sBeginChkptLog.mDiskRedoLSN;
    sOldestLSN        = mAnchorMgr.getDiskRedoLSN();
    if (smrCompareLSN::isGT(&sOldestLSN, &sDiskRedoLSN) == ID_TRUE)
    {
        sDiskRedoLSN = sOldestLSN;
    }

    /*
      Disk�� ���ؼ� RecvLSN�� ������Ѵ�. Disk Checkpoint�� MMDB��
      ������ ������ �ǰ� �׶� RecvLSN�� LogAnchor�� ��ϵǱ⶧���� LogAnchor��
      oldestLSN�� ���ؼ� ū ���� �� recv lsn���� �����ϰ� �ٽ� Disk�� RecvLSN��
      �� �ؼ� ���� ���� recv lsn�� �����Ѵ�.
    */
    if ( smrCompareLSN::isGTE(&sMemRecvLSNOfDisk, &sDiskRedoLSN)
         == ID_TRUE )
    {
        // AT recvLSN >= drdb recvLSN
        sBeginChkptLog.mEndLSN = sDiskRedoLSN;
    }
    else
    {
        /* nothing to do */
    }

    /* PROJ-2569 2pc�� ���� �ʿ��� LSN�� recv lsn�� �� ���� ������ recv lsn�� ����.
     * mSkipRedoLSN�� ���� LSN���� ������ �д�. */
    if ( smrCompareLSN::isGT( &sBeginChkptLog.mEndLSN, &sBeginChkptLog.mDtxMinLSN )
         == ID_TRUE )
    {
        mSkipRedoLSN = sBeginChkptLog.mEndLSN;
        sBeginChkptLog.mEndLSN = sBeginChkptLog.mDtxMinLSN;

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_SKIP_REDO_LSN,
                    mSkipRedoLSN.mFileNo,
                    mSkipRedoLSN.mOffset);
    }
    else
    {
        /* Nothing to do */
    }

    /* Archive ��� Logfile��  Archive List�� �߰��Ѵ�. */
    if ( getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        IDE_TEST( rebuildArchLogfileList( &sBeginChkptLog.mEndLSN )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    SM_LSN_MAX( mIdxSMOLSN );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_REDOALL_START,
                sBeginChkptLog.mEndLSN.mFileNo,
                sBeginChkptLog.mEndLSN.mOffset);

    /*
      Redo�Ҷ����� RedoLSN�� ����Ű�� Log�߿��� ����
      ���� mLSN���� ���� �α׸� ���� Redo�Ѵ�. �̷��� �����ν� Transaction��
      ����� �α׼����� Redo�� �����Ѵ�. �̸� ���� smrRedoLSNMgr�� �̿��Ѵ�.
    */
    IDE_TEST( smrRedoLSNMgr::initialize( &sBeginChkptLog.mEndLSN )
              != IDE_SUCCESS );

    /* -----------------------------------------------------------------
     * recovery ���� �α׺��� end of log���� �� �α׿� ���� redo ����
     * ---------------------------------------------------------------- */

    while ( 1 )
    {
        if ( sRedoFail == ID_TRUE ) /* Redo���� �̻� �߰� */
        {
            IDE_TEST( startupFailure( &sRTOI, ID_TRUE ) // isRedo
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        if ( sRedoSkip == ID_TRUE ) /* ���� Loop���� Redo�� �������� */
        {
            sCurRedoLSNPtr->mOffset += sLogSizeAtDisk; /* �������� �Ű��� */
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( smrRedoLSNMgr::readLog( &sCurRedoLSNPtr,
                                          &sLogHeadPtr,
                                          &sLogPtr,
                                          &sLogSizeAtDisk,
                                          &sIsValid) != IDE_SUCCESS );
        SM_GET_LSN( mLstDRDBRedoLSN , *sCurRedoLSNPtr );

        if ( sIsValid == ID_FALSE )
        {
            ideLog::log( IDE_SM_0, 
                         "Restart Recovery Completed "
                         "[ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]",
                         sCurRedoLSNPtr->mFileNo,
                         sCurRedoLSNPtr->mOffset );

            break; /* Invalid�� �α׸� �ǵ��� ��� */
        }
        else
        {
            /* nothing to do */
        }

        /* Debuging ���� ��������� Log ��ġ ����ص� */
        SM_GET_LSN( mLstRedoLSN, *sCurRedoLSNPtr );
        mCurLogPtr     = sLogPtr;
        mCurLogHeadPtr = sLogHeadPtr;

        sRedoSkip    = ID_FALSE;
        sRedoFail    = ID_FALSE;
        SM_GET_LSN( sLstRedoLSN, *sCurRedoLSNPtr );
        sLogType     = smrLogHeadI::getType(sLogHeadPtr);
      
        IDE_TEST( processDtxLog( mCurLogPtr,
                                 sLogType,
                                 sCurRedoLSNPtr,
                                 &sRedoSkip )
                  != IDE_SUCCESS );  

        /* �̹� Checkpoint�� ¦�� �Ǵ� DirtyPageList Log�� ã�� ����,
         * �� ���°� Checkpoint �������� Ȯ���� */
        if ( smrCompareLSN::isGTE( sCurRedoLSNPtr, &sChkptBeginLSN )
             == ID_TRUE )
        {
            sAfterChkpt = ID_TRUE;
        }
        else
        {
            /* nothing to do ... */
        }

        /* -------------------------------------------------------------
         * [3] ���� �αװ� Ʈ������� ����� �α��� ��� �� Ʈ�������
         * Ʈ����� Queue�� ����Ѵ�.
         * - active Ʈ����� ����� active trans recvLSN�� ������ LSN����
         *   ó���Ѵ�.
         * - drdb Ʈ������� ���, �Ҵ�Ǿ��� tss�� ������ ��� assign
         *   ���־�� �Ѵ�.
         * ------------------------------------------------------------ */
        sCurTrans = NULL;
        sTID = smrLogHeadI::getTransID(sLogHeadPtr);

        /* �Ϲ����� Tx�϶��� ActiveTx�� ����� */
        /* MemRecvLSN�� OldestTxLSN�̱⿡, �̺��� �����̸� ������  */
        if ( ( sTID != SM_NULL_TID ) &&
             ( smrCompareLSN::isLT( sCurRedoLSNPtr,
                                    &sMemRecvLSNOfDisk) == ID_FALSE ) &&
             ( sRedoSkip == ID_FALSE ) )
        {
            IDE_TEST( addActiveTrans( sLogHeadPtr,
                                      sLogPtr,
                                      sTID,
                                      sCurRedoLSNPtr,
                                      (&sCurTrans) ) != IDE_SUCCESS );
        }

        /*
         * [4] RecvLSN�� ���� ���������� �α׸� ������Ѵ�.
         * BUG-25014 redo�� ���� �����������մϴ�. (by BUG-23919)
         * ������, ���� �α׷��ڵ�� ������ redo �Ѵ�.
         * SMR_LT_FILE_END
         * : ���� �α������� Open�ؾ��ϱ� �����̴�.
         * SMR_LT_DSKTRANS_COMMIT�� SMR_LT_DSKTRANS_ABORT
         * : Active Transaction�� �ƴ� ��쿡�� Disk LogSlot�� �����ϱ�
         *   ������ Redo�ؾ� �Ѵ�.
         *   �ݴ�� Active Transaction������, Disk Redo������ ���Ե���
         *   �ʴ´ٸ�, Memory Redo�� MakeTransEnd�� �����ؾ��ϹǷ� Redo
         *   �Ǿ���Ѵ�. �̶� Diskk LogSlot�� Hashing �Ǿ �ݿ��� ����
         *   �ʴ´�.
         */
        if ( ( sLogType != SMR_LT_DSKTRANS_COMMIT )      &&
             ( sLogType != SMR_LT_DSKTRANS_ABORT )       &&
             ( sLogType != SMR_LT_FILE_END ) )
        {
            if ( smrLogMgr::isDiskLogType(sLogType) == ID_TRUE )
            {
                /* DiskRedoLSN ���� ���� LSN�� ���� Disk �α״� Skip �Ѵ�. */
                if ( smrCompareLSN::isLT( sCurRedoLSNPtr, &sDiskRedoLSN ) == ID_TRUE )
                {
                    IDE_ASSERT( SM_IS_LSN_MAX( sDiskBeginLSN ) );
                    sRedoSkip = ID_TRUE;
                }
                else
                {
                    if ( SM_IS_LSN_MAX( sDiskBeginLSN ) )
                    {
                         SM_GET_LSN( sDiskBeginLSN, sLstRedoLSN );
                    }
                }
            }
            else
            {
                /* MemRecvLSN ���� ���� LSN�� ���� Mem �α״� Skip �Ѵ�. */
                if ( smrCompareLSN::isLT( sCurRedoLSNPtr, &sMemRecvLSNOfDisk ) == ID_TRUE )
                {
                    IDE_ASSERT( SM_IS_LSN_MAX( sMemBeginLSN ) );
                    sRedoSkip = ID_TRUE;
                }
                else
                {
                    if ( SM_IS_LSN_MAX( sMemBeginLSN ) )
                    {
                        SM_GET_LSN( sMemBeginLSN, sLstRedoLSN );
                    } 
                    else
                    {
                        /* nothing to do */
                    }
                }
            }
        }
        else
        {
            /* nothing to do ... */
        }

        if ( sRedoSkip == ID_TRUE )
        {
            /* PROJ-2133 Incremental Backup */
            if ( isCTMgrEnabled() == ID_TRUE )
            {
                smLSN   sLastFlushLSN;

                sLastFlushLSN = getCTFileLastFlushLSNFromLogAnchor();
                
                if ( smrCompareLSN::isLT( &sLastFlushLSN, sCurRedoLSNPtr )  == ID_TRUE )
                {
                    IDE_TEST( smriChangeTrackingMgr::removeCTFile() 
                              != IDE_SUCCESS );
                    ideLog::log( IDE_SM_0,
                    "====================================================\n"
                    " change tracking is disabled during restart recovery\n"
                    "====================================================\n" );
                }
            }
            continue;
        }

        /*******************************************************************
         * PROJ-2162 RestartRiskReduction
         *******************************************************************/
        if ( smrCompareLSN::isEQ( sCurRedoLSNPtr, &sChkptEndLSN ) == ID_TRUE )
        {
            SM_GET_LSN( mEndChkptLSN, sChkptEndLSN ); /* EndCheckpointLSN ȹ�� */ 
        }

        /* Redo �����ص� �Ǵ� ��ü���� Ȯ�� */
        prepareRTOI( sLogPtr,
                     sLogHeadPtr,
                     sCurRedoLSNPtr,
                     NULL, /* aDRDBRedoLogData */
                     NULL, /* aDRDBPagePtr */
                     ID_TRUE, /* aIsRedo */
                     &sRTOI );
        checkObjectConsistency( &sRTOI,
                                &sObjectConsistency );
        if ( ( sLogType == SMR_LT_FILE_END ) &&
             ( sObjectConsistency == ID_FALSE ) )
        {
            // LOGFILE END Log�� ���� ������ �� ����.
            IDE_DASSERT( 0 );
            sObjectConsistency = ID_TRUE;
        }

        if ( sObjectConsistency == ID_FALSE )
        {
            sRedoSkip = ID_TRUE;
            sRedoFail = ID_TRUE;
            continue; /* Redo ���� */
        }

        /* Redo�ϸ鼭 LSN�� �̵���Ű�� ������, ���������� Redo�ѹ� ���ϱ� ����
         * LSN�� �����ص� */
        SM_GET_LSN( sDummyRedoLSN4Check, *sCurRedoLSNPtr );

        /* -------------------------------------------------------------
         * [5] �� �α� Ÿ�Կ� ���� redo ����
         * ------------------------------------------------------------ */
        if ( redo( sCurTrans,
                   sCurRedoLSNPtr,
                   &sFileCount,
                   sLogHeadPtr,
                   sLogPtr,
                   sLogSizeAtDisk,
                   sAfterChkpt ) == IDE_SUCCESS )
        {
            sRedoSkip = ID_FALSE;
            sRedoFail = ID_FALSE;

            if ( ( smuProperty::getSmEnableStartupBugDetector() == ID_TRUE ) &&
                 ( isIdempotentLog( sLogType ) == ID_TRUE ) )
            {
                /* Idempotent�� Log�̱� ������, �ѹ� ���������� ������ ����
                 * �ؾ� �� */
                IDE_ASSERT( redo( sCurTrans,
                                  &sDummyRedoLSN4Check,
                                  &sFileCount,
                                  sLogHeadPtr,
                                  sLogPtr,
                                  sLogSizeAtDisk,
                                  sAfterChkpt )
                            == IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* Redo ������ */
            sRedoSkip    = ID_TRUE;
            sRedoFail    = ID_TRUE;
            sRTOI.mCause = SMR_RTOI_CAUSE_REDO;
        }

        // �ؽ̵� ��ũ�α׵��� ũ���� ������
        // DISK_REDO_LOG_DECOMPRESS_BUFFER_SIZE �� �����
        // �ؽ̵� �α׵��� ��� ���ۿ� �����Ѵ�.
        IDE_TEST( checkRedoDecompLogBufferSize() != IDE_SUCCESS );
    }//While

    ideLog::log( IDE_SERVER_0,
                  "MemBeginLSN : [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n"
                  "DskBeginLSN : [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n"
                  "LastRedoLSN : [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n",
                  sMemBeginLSN.mFileNo,
                  sMemBeginLSN.mOffset,
                  sDiskBeginLSN.mFileNo,
                  sDiskBeginLSN.mOffset,
                  sLstRedoLSN.mFileNo,
                  sLstRedoLSN.mOffset );

    IDE_ASSERT( SM_IS_LSN_MAX( sLstRedoLSN ) == ID_FALSE );
    mCurLogPtr     = NULL;
    mCurLogHeadPtr = NULL;
    SM_GET_LSN( mLstRedoLSN, sLstRedoLSN );

    // PROJ-1867
    // ���� Online Backup�޾Ҵ� DBFile�� recovery�� ����̸�
    // sLstRedoLSN�� endLSN���� ���� �Ͽ������� Ȯ���Ѵ�.
    // Online Backup ���� �ʾҴٸ� sMustRedoToLSN�� init�����̴�.

    IDE_TEST( getMaxMustRedoToLSN( aStatistics,
                                   &sMustRedoToLSN,
                                   &sDBFileName )
              != IDE_SUCCESS );

    if ( smrCompareLSN::isGT( &sMustRedoToLSN,
                              &mLstDRDBRedoLSN ) == ID_TRUE )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_DRECOVER_NEED_MORE_LOGFILE,
                    mLstDRDBRedoLSN.mFileNo,
                    mLstDRDBRedoLSN.mOffset,
                    sDBFileName,
                    sMustRedoToLSN.mFileNo,
                    sMustRedoToLSN.mOffset );

        IDE_RAISE( err_need_more_log );
    }

    /* redo�� �������� ���������� redo�� �α��� SN�� Repl Recovery LSN���� �����Ѵ�.
     * repl recovery���� �ݺ����� ��ֽÿ��� ���� �� ���� ó���� �߻��� ��ֽÿ���
     * repl recovery LSN�� �����ؾ� �Ѵ�. �׷��Ƿ�, ���� ���°� ���� ��(SM_SN_NULL)
     * �� ��쿡�� �����Ѵ�. �׸���, �ٽ� ���� ���� ��(replication �ʱ�ȭ ���� ��)
     * repl recovery LSN�� SM_SN_NULL�� �����Ѵ�. proj-1608
     */
    if ( SM_IS_LSN_MAX( getReplRecoveryLSN() ) )
    {
        IDE_TEST( setReplRecoveryLSN( sLstRedoLSN ) != IDE_SUCCESS );
    }

    /* ------------------------------------------------
     * �ؽ� ���̺� ������ ��, DRDB�� redo �α׵���
     * disk tablespace�� �ݿ�
     * sdrRedoMgr::applyHashedLogRec() �Լ� ȣ��
     * ----------------------------------------------*/
    IDE_TEST( applyHashedDiskLogRec( aStatistics) != IDE_SUCCESS );

    /* redo �����ڸ� �����Ѵ�. */
    IDE_TEST( smLayerCallback::destroyRedoMgr() != IDE_SUCCESS );

    sLstRedoLSN = smrRedoLSNMgr::getNextLogLSNOfLstRedoLog();

    /* Restart ���Ŀ� ���ۻ� Index Page�� �����Ҷ�, RedoLSN ����
     * ū LSN�� ���� ��� Index Page�� SMO No�� 0���� �ʱ�ȭ�Ѵ�.
     * �ֳ��ϸ�, SMO No�� Logging ����� �ƴ� ��߿� ��� �����ϴ�
     * Runtime �����̸�, �̰��� SMO ���������� �Ǵ��Ͽ�
     * index traverse�� retry ���θ� �Ǵ��Ѵ�. �̴� ��� index ��
     * no-latch traverse scheme �̱� �����̴�.
     * �׷��Ƿ� ���⼭�� retry�� �������� �ʵ��� LSN�� �ӽ÷� �����Ѵ�. */
    mIdxSMOLSN = smrRedoLSNMgr::getLstCheckLogLSN();

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                 SM_TRC_MRECOVERY_RECOVERYMGR_REDOALL_SUCCESS,
                 sLstRedoLSN.mFileNo,
                 sLstRedoLSN.mOffset);

    /* -----------------------------------------------------------------
     * [8] smrLogMgr�� Prepare, Sync Thread��
     *     ���۽�Ų��.
     * ---------------------------------------------------------------- */
    sLstLSN = smrRedoLSNMgr::getLstCheckLogLSN();
    sCrtLogFileNo = sLstLSN.mFileNo;

    /* Redo�� ����� ��� LogFile�� Close�Ѵ�. */
    IDE_TEST( smrLogMgr::getLogFileMgr().closeAllLogFile() != IDE_SUCCESS );

    /* ------------------------------------------------
     * [9] ���̻� Redo�� �αװ� ���� ������ smrRedoLSNMgr��
     *     �����Ų��.
     * ----------------------------------------------*/
    IDE_TEST( smrRedoLSNMgr::destroy() != IDE_SUCCESS );

    // BUG-47404 Log Prepare ���� WAL �˻縦 �ؾ� �Ѵ�.
    // WAL�� ���������� Consistency�� �����Ͽ�
    // ���� Log Prepare Thread���� logfile �ʱ�ȭ�� ���� �ʴ´�.
    IDE_TEST( checkMemWAL()  != IDE_SUCCESS );
    IDE_TEST( checkDiskWAL() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0,"      [SM-PREPARE] Prepare Logfile Thread Start...");
    IDE_TEST( smrLogMgr::startupLogPrepareThread( &sLstLSN,
                                                  sCrtLogFileNo,
                                                  ID_TRUE /* aIsRecovery */ )
             != IDE_SUCCESS );
    ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");

    IDU_FIT_POINT( "BUG-45283@smrRecoveryMgr::redoAll::unlockThreadMtx" );

    if ( getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        sState = 0;
        IDE_TEST( smrLogMgr::getArchiveThread().unlockThreadMtx()
                  != IDE_SUCCESS );
    }

    IDE_CALLBACK_SEND_MSG("");

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_need_more_log );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ErrNeedMoreLog, sDBFileName ) );
    }
    IDE_EXCEPTION( err_invalid_log );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidLog,
                                  sChkptBeginLSN.mFileNo,
                                  sChkptBeginLSN.mOffset ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_DASSERT( getArchiveMode() == SMI_LOG_ARCHIVE );
        IDE_ASSERT( smrLogMgr::getArchiveThread().unlockThreadMtx()
                    == IDE_SUCCESS );
    }
    if ( sLogFile != NULL )
    {
        (void)smrLogMgr::closeLogFile( sLogFile );
        sLogFile = NULL;
    }

    IDE_POP();

    mCurLogPtr     = NULL;
    mCurLogHeadPtr = NULL;

    return IDE_FAILURE;

}

/*
  Decompress Log Buffer ũ�Ⱑ
  ������Ƽ�� ������ ������ Ŭ ��� Hashing�� Disk Log���� ����
*/
IDE_RC smrRecoveryMgr::checkRedoDecompLogBufferSize()
{
    if ( smrRedoLSNMgr::getDecompBufferSize() >
         smuProperty::getDiskRedoLogDecompressBufferSize() )
    {
        IDE_TEST( applyHashedDiskLogRec( NULL ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Hashing�� Disk Log���� ����
 */
IDE_RC smrRecoveryMgr::applyHashedDiskLogRec( idvSQL* aStatistics)
{
    IDE_TEST( smLayerCallback::applyHashedLogRec( aStatistics )
              != IDE_SUCCESS );

    // Decompress Log Buffer�� �Ҵ��� ��� �޸𸮸� �����Ѵ�.
    IDE_TEST( smrRedoLSNMgr::clearDecompBuffer() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description : �������������� active Ʈ����� öȸ (undoall pass)
 *
 * Ʈ����� ����� �������� �����ߴ� active Ʈ����� ��Ͽ�
 * �ִ� ��� Ʈ����ǵ鿡 ���� �αװ� ������ �������� ����
 * ū LSN�� ������ �α׸� ���� Ʈ����Ǻ��� öȸ�� �Ѵ�.
 *
 * - 2nd. code design
 *   + �� active Ʈ������� ���������� ������ �α���
 *     LSN���� queue �����Ѵ�.
 *   + �� active Ʈ������� ������ �� �α׿� ����
 *     Ʈ����� öȸ�� �����Ѵ�.
 *   + �� active Ʈ����ǿ� ���� abort log �����Ѵ�.
 *   + prepared list�� �ִ� �� Ʈ����ǿ� ���� record lock ȹ��
 **********************************************************************/
IDE_RC smrRecoveryMgr::undoAll( idvSQL* /*aStatistics*/ )
{
    smrUTransQueue      sTransQueue;
    smrLogFile        * sLogFile;
    void              * sActiveTrans;
    smLSN               sCurUndoLSN;
    smrLogFile        * sCurLogFile;
    smrUndoTransInfo  * sTransInfo;

    sLogFile = NULL;

    /* ------------------------------------------------
     * active list�� �ִ� ��� Ʈ����ǵ鿡 ����
     * �αװ� ������ �������� ���� ū LSN�� ������
     * �α׺��� undo�Ѵ�.
     * ----------------------------------------------*/

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_UNDOALL_START);

    if ( smLayerCallback::isEmptyActiveTrans() == ID_FALSE )
    {
        IDE_TEST( sTransQueue.initialize(  smLayerCallback::getActiveTransCnt() )
                  != IDE_SUCCESS );

        /* ------------------------------------------------
         * [1] �� active Ʈ������� ���������� ������ �α���
         * LSN���� queue �����Ѵ�.
         * ----------------------------------------------*/
        IDE_TEST( smLayerCallback::insertUndoLSNs( &sTransQueue )
                  != IDE_SUCCESS );

        /* ------------------------------------------------
         * [2] �� active Ʈ������� ������ �� �α׿� ���� undo ����
         * - DRDB �α� Ÿ���̵� MRDB �α� Ÿ���̵� ���⼭�� ���������,
         * undo() �Լ����ο��� �����Ͽ� ó���Ѵ�.
         * ----------------------------------------------*/
        sTransInfo = sTransQueue.remove();

        // BUG-27574 klocwork SM
        IDE_ASSERT( sTransInfo != NULL );

        /* For Parallel Logging: Active Transaction�� undoNxtLSN�߿���
           ���� ū LSN���� ���� Transaction�� undoNextLSN�� ����Ű��
           Log�� ���� Undo�Ѵ� */
        do
        {
            /*
              BUBBUG:For Replication.
              if ( sTransQueue.m_cUTrans == 1 )
              {
              mSyncReplLSN.mFileNo = sCurUndoLSN.mFileNo;
              mSyncReplLSN.mOffset = sCurUndoLSN.mOffset;
              }
            */

            sActiveTrans = sTransInfo->mTrans;
            sCurUndoLSN  = smLayerCallback::getCurUndoNxtLSN( sActiveTrans );

            if ( undo( NULL, /* idvSQL* */
                       sActiveTrans,
                       &sLogFile )
                != IDE_SUCCESS )
            {
                IDE_TEST( startupFailure( 
                              smLayerCallback::getRTOI4UndoFailure( sActiveTrans ), 
                              ID_FALSE ) // isRedo
                          != IDE_SUCCESS );
            }

            sCurUndoLSN  = smLayerCallback::getCurUndoNxtLSN( sActiveTrans );

            if ( ( sCurUndoLSN.mFileNo == ID_UINT_MAX ) &&
                 ( sCurUndoLSN.mOffset == ID_UINT_MAX ) )
            {
                ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                             SM_TRC_MRECOVERY_RECOVERYMGR_UNDO_TRANS_ID,
                             smLayerCallback::getTransID(sActiveTrans),
                             sCurUndoLSN.mFileNo,
                             sCurUndoLSN.mOffset );
            }
            else
            {
                IDE_TEST( sTransQueue.insert( sTransInfo ) != IDE_SUCCESS );
            }

            sTransInfo = sTransQueue.remove();

            /*BUBBUG:For Replication.
              sCurUndoLSN = sTransQueue.replace(&sPrvUndoLSN);
            */
        }
        while(sTransInfo != NULL);

        sTransQueue.destroy(); //error üũ�� �ʿ����...

        /* -----------------------------------------------
           [3] �� active Ʈ����ǿ� ���� abort log ����
           ----------------------------------------------- */
        IDE_TEST( smLayerCallback::abortAllActiveTrans() != IDE_SUCCESS );

        /* -----------------------------------------------
           [4] Undo�� ���ؼ� ������ ��� Log File�� Close�Ѵ�.
           ----------------------------------------------- */
        if ( sLogFile != NULL )
        {
            sCurLogFile = sLogFile;
            sLogFile = NULL;
            IDE_TEST( smrLogMgr::closeLogFile( sCurLogFile )
                      != IDE_SUCCESS );
            sCurLogFile = NULL;
        }
        else
        {
            /* nothing to do ... */
        }
    }


    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_UNDOALL_SUCCESS );

    /* ------------------------------------------------
     * [5]prepared list�� �ִ� �� Ʈ����ǿ� ���� record lock ȹ��
     * - table lock�� refineDB �Ŀ� ȹ����
     * (�ֳ��ϸ� table header�� LockItem�� �ʱ�ȭ�� �Ŀ�
     * table lock�� ��ƾ� �ϱ� ������)
     * => prepared transaction�� access�� oid list ������
     * redo �������� �̹� �����Ǿ� ����)
     * - recovery ���� Ʈ����� ���̺��� ��� ��Ʈ����
     *  transaction free list�� ����Ǿ� �����Ƿ�
     *  prepare transaction�� ���� �̸� �籸���Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST( smLayerCallback::setRowSCNForInDoubtTrans() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if ( sLogFile != NULL )
    {
        IDE_ASSERT( smrLogMgr::closeLogFile( sLogFile )
                    == IDE_SUCCESS );
        sLogFile = NULL;
    }
    IDE_POP();

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Ʈ����� öȸ
 *
 * �α׸� �ǵ��Ͽ� undo�� ������ �α׿� ���ؼ�
 * �α� Ÿ�Ժ� physical undo �Ǵ� ���꿡 ����(NTA)
 * logical undo�� �����ϱ⵵ �Ѵ�.
 **********************************************************************/
IDE_RC smrRecoveryMgr::undo( idvSQL          * aStatistics,
                             void            * aTrans,
                             smrLogFile     ** aLogFilePtr)
{

    smrDiskLog            sDiskLog;        // disk redoonly/undoable �α�
    smrDiskNTALog         sDiskNTALog;     // disk NTA �α�
    smrDiskRefNTALog      sDiskRefNTALog;  // Referenced NTA �α�
    smrTBSUptLog          sTBSUptLog;      // file ���� �α�
    smrUpdateLog          sUpdateLog;
    smrNTALog             sNTALog;
    smrLogHead            sLogHead;
    smOID                 sOID;
    smOID                 sTableOID;
    scPageID              sFirstPID;
    scPageID              sLastPID;
    SChar               * sLogPtr;
    SChar               * sBeforeImage;
    smLSN                 sCurUndoLSN;
    smLSN                 sNTALSN;
    smLSN                 sPrevUndoLSN;
    smLSN                 sCurLSN;      /* PROJ-1923 */
    sctTableSpaceNode   * sSpaceNode;
    smrCompRes          * sCompRes;
    SChar               * sFirstPagePtr;
    SChar               * sLastPagePtr;
    SChar               * sTargetObject;
    smrRTOI               sRTOI;
    idBool                sObjectConsistency = ID_TRUE;
    UInt                  sState   = 0;
    idBool                sIsValid = ID_FALSE;
    UInt                  sLogSizeAtDisk = 0;
    idBool                sIsDiscarded;

    
    IDE_ASSERT(aTrans != NULL);

    SM_LSN_INIT( sCurLSN );     // dummy

    /* ------------------------------------------------
     * [1] �α׸� �ǵ��Ѵ�.
     * ----------------------------------------------*/
    sCurUndoLSN = smLayerCallback::getCurUndoNxtLSN( aTrans );

    // Ʈ������� �α� ����/�������� ���ҽ��� �����´�
    IDE_TEST( smLayerCallback::getTransCompRes( aTrans, &sCompRes )
              != IDE_SUCCESS );

    /* BUG-19150: [SKT Rating] Ʈ����� �ѹ�ӵ��� ����
     *
     * mmap�� ������������ File�� ������������ �ӵ��� �߳�����
     * ������ ������ ������������ Read�Ҷ����� IO�� �߻��Ͽ� IOȽ����
     * ���� �ǿ� ������ ���ϵȴ�. ������ Rollback�ÿ��� mmap�� �̿��ؼ�
     * LogFile�� �������� �ʰ� Dynamic Memory�� ������ �о ó���Ѵ�.
     * */
    IDE_TEST( smrLogMgr::readLog( &sCompRes->mDecompBufferHandle,
                                  &sCurUndoLSN,
                                  ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                  aLogFilePtr,
                                  &sLogHead,
                                  &sLogPtr,
                                  &sIsValid,
                                  &sLogSizeAtDisk ) 
             != IDE_SUCCESS );
  
    IDE_TEST_RAISE( sIsValid == ID_FALSE, err_invalid_log )

    mCurLogPtr     = sLogPtr;
    mCurLogHeadPtr = &sLogHead;
    SM_GET_LSN( mLstUndoLSN, sCurUndoLSN );

    sState = 1;

    /* Undo �����ص� �Ǵ� ��ü���� Ȯ�� */
    prepareRTOI( (void*)sLogPtr,
                 &sLogHead,
                 &sCurUndoLSN,
                 NULL, /* aDRDBRedoLogData */
                 NULL, /* aDRDBPagePtr */
                 ID_FALSE, /* aIsRedo */
                 &sRTOI );
    checkObjectConsistency( &sRTOI,
                            &sObjectConsistency );

    if ( sObjectConsistency == ID_FALSE )
    {
        IDE_TEST( startupFailure( &sRTOI,
                                  ID_FALSE ) // isRedo
                  != IDE_SUCCESS );
    }
    else
    {
        /* ���� �����Ѵٸ�, Undo���꿡 ������ �ִ� �� */
        sRTOI.mCause = SMR_RTOI_CAUSE_UNDO;

        /* ------------------------------------------------
         * [2] MRDB�� update Ÿ���� �α׿� ���� öȸ
         * - �ش� undo LSN�� ���� CLR �α׸� ����Ѵ�.
         * - �ش� �α� Ÿ�Կ� ���� undo�� �����Ѵ�.
         * ----------------------------------------------*/
        if ( smrLogHeadI::getType(&sLogHead) == SMR_LT_UPDATE )
        {
            idlOS::memcpy(&sUpdateLog, sLogPtr, SMR_LOGREC_SIZE(smrUpdateLog));

            // Undo SKIP�� Tablespace�� ���� �αװ� �ƴ� ���
            if ( sctTableSpaceMgr::hasState( SC_MAKE_SPACE(sUpdateLog.mGRID),
                                             SCT_SS_SKIP_UNDO ) == ID_FALSE )
            {
                sBeforeImage = sLogPtr + SMR_LOGREC_SIZE(smrUpdateLog);

                if ( gSmrUndoFunction[sUpdateLog.mType] != NULL)
                {
                    //append compensation log
                    sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);

                    IDE_TEST( smrLogMgr::writeCMPSLogRec( aStatistics,
                                                          aTrans,
                                                          SMR_LT_COMPENSATION,
                                                          &sPrevUndoLSN,
                                                          &sUpdateLog,
                                                          sBeforeImage )
                              != IDE_SUCCESS );

                    smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);

                    IDE_TEST( gSmrUndoFunction[sUpdateLog.mType](
                                        smrLogHeadI::getTransID(&sUpdateLog.mHead),
                                        SC_MAKE_SPACE(sUpdateLog.mGRID),
                                        SC_MAKE_PID(sUpdateLog.mGRID),
                                        SC_MAKE_OFFSET(sUpdateLog.mGRID),
                                        sUpdateLog.mData,
                                        sBeforeImage,
                                        sUpdateLog.mBImgSize,
                                        smrLogHeadI::getFlag(&sUpdateLog.mHead) )
                              != IDE_SUCCESS );
                }
                else
                {
                    // BUG-15474
                    // undo function�� NULL�̶�� �ǹ̴� undo �۾��� ��������
                    // �ʴ� �α׶�� �ǹ��̴�.
                    // undo �۾��� ���ٴ� ���� ������ ���� �۾��� ���� ������
                    // ���� CLR�� ����� �ʿ䰡 ��� dummy CLR ����ϴ� �κ���
                    // �����Ѵ�.
                    // Nothing to do...
                }
            }

        }
        else if ( smrLogHeadI::getType(&sLogHead) == SMR_LT_TBS_UPDATE)
        {
            idlOS::memcpy(&sTBSUptLog, sLogPtr, SMR_LOGREC_SIZE(smrTBSUptLog));

            // Tablespace ��� ���� ���� �α״� SKIP���� �ʴ´�.
            // - Drop�� Tablespace�� ���
            //   - Undo Function�ȿ��� SKIP�Ѵ�.
            // - Offline�� Tablespace�� ���
            //   - Online���·� ������ �� �����Ƿ� SKIP�ؼ��� �ȵȴ�.
            // - Discard�� Tablespace�� ���
            //   - Drop Tablespace�� Undo�� �� �� �ִ�.

            sBeforeImage = sLogPtr + SMR_LOGREC_SIZE(smrTBSUptLog);

            //append disk compensation log
            sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);

            IDE_TEST( smrLogMgr::writeCMPSLogRec4TBSUpt( aStatistics,
                                                         aTrans,
                                                         &sPrevUndoLSN,
                                                         &sTBSUptLog,
                                                         sBeforeImage )
                      != IDE_SUCCESS );

            smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);


            /* BUG-41689 A discarded tablespace is redone in recovery
             * Discard�� TBS���� Ȯ���Ѵ�. */
            sIsDiscarded = sctTableSpaceMgr::hasState( sTBSUptLog.mSpaceID,
                                                       SCT_SS_SKIP_AGING_DISK_TBS,
                                                       ID_FALSE );

            if ( ( sIsDiscarded == ID_FALSE ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_MRDB_DROP_TBS ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_DRDB_DROP_TBS ) ||
                 ( sTBSUptLog.mTBSUptType == SCT_UPDATE_DRDB_DROP_DBF ) )
            {
                IDE_TEST( gSmrTBSUptUndoFunction[sTBSUptLog.mTBSUptType](
                                                                aStatistics,
                                                                aTrans,
                                                                sCurLSN,
                                                                sTBSUptLog.mSpaceID,
                                                                sTBSUptLog.mFileID,
                                                                sTBSUptLog.mBImgSize,
                                                                sBeforeImage,
                                                                mRestart )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do 
                 * discard�� TBS�� undo�� drop tbs�� �����ϰ� �����Ѵ�.*/
            }
        }
        else if (smrLogHeadI::getType(&sLogHead) == SMR_DLT_UNDOABLE)
        {
            idlOS::memcpy(&sDiskLog, sLogPtr, SMR_LOGREC_SIZE(smrDiskLog));

            // BUGBUG-1548 Disk Undoable�α� Tablespace�� SKIP���� �����Ͽ� ó���ؾ���

            /* ------------------------------------------------
             * DRDB�� SMR_DLT_UNDOABLE Ÿ�Կ� ���� undo
             * - undo�ϱ����� compensation log�� �����
             * - DRDB�� undosegment���� redo �α�Ÿ���� �ǵ��� ���̸�,
             *   �� layer�� undo log Ÿ�Կ� ���� ������ undo �Լ���
             *   ȣ���Ͽ� �� �α׿� ���ؼ� undo ó��
             * ----------------------------------------------*/
            sBeforeImage = sLogPtr + SMR_LOGREC_SIZE(smrDiskLog) + sDiskLog.mRefOffset;

            sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sDiskLog.mHead);

            IDE_TEST( smLayerCallback::doUndoFunction(
                                              aStatistics,
                                              smrLogHeadI::getTransID( &sDiskLog.mHead ),
                                              sDiskLog.mTableOID,
                                              sBeforeImage,
                                              &sPrevUndoLSN )
                      != IDE_SUCCESS );

            smrLogHeadI::setPrevLSN(&sDiskLog.mHead, sPrevUndoLSN);
        }
        else if ( smrLogHeadI::getType(&sLogHead) == SMR_DLT_NTA)
        {
            idlOS::memcpy(&sDiskNTALog, sLogPtr, SMR_LOGREC_SIZE(smrDiskNTALog));
            // BUGBUG-1548 Disk NTA�α� Tablespace�� SKIP���� �����Ͽ� ó���ؾ���

            /* ------------------------------------------------
             * DRDB�� SMR_DLT_NTA Ÿ�Կ� ���� Logical UNDO
             * - DRDB�� redo �α��߿� operation NTA �α׿� ���Ͽ�
             *   ������ logical undo ó��
             * - dummy compensation �α� ���
             * ----------------------------------------------*/
            sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);
            IDE_TEST( smLayerCallback::doNTAUndoFunction( aStatistics,
                                                          aTrans,
                                                          sDiskNTALog.mOPType,
                                                          sDiskNTALog.mSpaceID,
                                                          &sPrevUndoLSN,
                                                          sDiskNTALog.mData,
                                                          sDiskNTALog.mDataCount )
                      != IDE_SUCCESS );

            smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);
        }
        else if (smrLogHeadI::getType(&sLogHead) == SMR_DLT_REF_NTA )
        {
            idlOS::memcpy(&sDiskRefNTALog, sLogPtr, SMR_LOGREC_SIZE(smrDiskRefNTALog));

            sBeforeImage = sLogPtr +
                           SMR_LOGREC_SIZE(smrDiskRefNTALog) +
                           sDiskRefNTALog.mRefOffset;

            sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);

            IDE_TEST( smLayerCallback::doRefNTAUndoFunction( aStatistics,
                                                             aTrans,
                                                             sDiskRefNTALog.mOPType,
                                                             &sPrevUndoLSN,
                                                             sBeforeImage )
                      != IDE_SUCCESS );

            smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);
        }
        else if ( smrLogHeadI::getType(&sLogHead) == SMR_LT_NTA )
        {
            idlOS::memcpy(&sNTALog, sLogPtr, SMR_LOGREC_SIZE(smrNTALog));

            if ( sNTALog.mOPType != SMR_OP_NULL )
            {
                switch(sNTALog.mOPType)
                {
                case SMR_OP_SMM_PERS_LIST_ALLOC:
                    // Undo SKIP�� Tablespace�� ���� �αװ� �ƴ� ���
                    if ( sctTableSpaceMgr::hasState( sNTALog.mSpaceID,
                                                     SCT_SS_SKIP_UNDO )
                         == ID_FALSE )
                    {
                        sFirstPID = (scPageID)(sNTALog.mData1);
                        sLastPID  = (scPageID)(sNTALog.mData2);

                        sNTALSN   = smLayerCallback::getLstUndoNxtLSN( aTrans );

                        IDE_ASSERT( smmManager::getPersPagePtr( sNTALog.mSpaceID,
                                                                sFirstPID,
                                                                (void**)&sFirstPagePtr )
                                    == IDE_SUCCESS );
                        IDE_ASSERT( smmManager::getPersPagePtr( sNTALog.mSpaceID,
                                                                sLastPID,
                                                                (void**)&sLastPagePtr )
                                    == IDE_SUCCESS );

                        IDE_TEST( smmManager::freePersPageList( aTrans,
                                                                sNTALog.mSpaceID,
                                                                sFirstPagePtr,
                                                                sLastPagePtr,
                                                                & sNTALSN ) 
                                  != IDE_SUCCESS);
                    }
                    break;

                case SMR_OP_SMM_CREATE_TBS:
                    if ( sctTableSpaceMgr::hasState( sNTALog.mSpaceID,
                                                     SCT_SS_SKIP_UNDO )
                         == ID_FALSE )
                    {
                       sSpaceNode = sctTableSpaceMgr::findSpaceNodeWithoutException( sNTALog.mSpaceID );

                        IDE_TEST( smmTBSDrop::doDropTableSpace( sSpaceNode,
                                                                SMI_ALL_TOUCH )
                                  != IDE_SUCCESS );
                    }
                    break;

                case SMR_OP_SMC_FIXED_SLOT_ALLOC:
                    // Undo SKIP�� Tablespace�� ���� �αװ� �ƴ� ���
                    if ( sctTableSpaceMgr::hasState( sNTALog.mSpaceID,
                                                     SCT_SS_SKIP_UNDO )
                         == ID_FALSE )
                    {
                        // SMR_OP_SMC_LOCK_ROW_ALLOC�� ���� undo�ÿ���
                        // Delete Bit�� �������� �ʴ´�.(BUG-14596)
                        sOID = (smOID)(sNTALog.mData1);
                        IDE_ASSERT( smmManager::getOIDPtr( sNTALog.mSpaceID,
                                                           sOID,
                                                           (void**)&sTargetObject )
                                    == IDE_SUCCESS );

                        IDE_TEST( smLayerCallback::setDeleteBit( aTrans,
                                                                 sNTALog.mSpaceID,
                                                                 sTargetObject,
                                                                 SMC_WRITE_LOG_OK) 
                                  != IDE_SUCCESS );
                    }
                    break;

                case SMR_OP_CREATE_TABLE:
                    // BUGBUG-1548 DISCARD/DROP�� Tablespace�� ��� SKIP����
                    // �����ϰ� IF�б��ؾ���
                    sOID = (smOID)(sNTALog.mData1);
                    /*  �ش� table�� disk table�� ��쿡�� �����ߴ� segment�� free �Ѵ� */
                    IDE_ASSERT( smmManager::getOIDPtr( sNTALog.mSpaceID,
                                                       sOID,
                                                       (void**)&sTargetObject )
                                == IDE_SUCCESS );
                    IDE_TEST( smLayerCallback::setDropTable( aTrans,
                                                             (SChar*)sTargetObject )
                              != IDE_SUCCESS );

                    if ( smrRecoveryMgr ::isRestart() == ID_FALSE )
                    {
                        IDE_TEST( smLayerCallback::doNTADropDiskTable( NULL,
                                                                       aTrans,
                                                                       (SChar*)sTargetObject )
                                  != IDE_SUCCESS );
                    }
                    break;

                case SMR_OP_CREATE_INDEX:
                    //drop index by abort,drop index pending.
                    IDE_TEST( smLayerCallback::dropIndexByAbort( NULL,
                                                                 aTrans,
                                                                (smOID)sNTALog.mData2,
                                                                (smOID)sNTALog.mData1 )
                              != IDE_SUCCESS );
                    break;

                case SMR_OP_DROP_INDEX:
                    // BUGBUG-1548 DISCARD/DROP�� Tablespace�� ��� SKIP����
                    // �����ϰ� IF�б��ؾ���
                    IDE_TEST( smLayerCallback::clearDropIndexList( sNTALog.mData1 )
                              != IDE_SUCCESS );
                    break;

                case SMR_OP_INIT_INDEX:
                    /* PROJ-2184 RP Sync ���� ����
                     * Index init �Ŀ� abort �� ��� index runtime header��
                     * drop �Ѵ�. */
                    /* BUG-42460 runtime�ÿ��� �����ϵ��� ��. */
                    if ( mRestart == ID_FALSE )
                    {
                        IDE_TEST( smLayerCallback::dropIndexRuntimeByAbort( (smOID)sNTALog.mData1,
                                                                            (smOID)sNTALog.mData2 )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                       /* nothing to do */
                    }
                    break;

                case SMR_OP_ALTER_TABLE:
                    // BUGBUG-1548 DISCARD/DROP�� Tablespace�� ��� SKIP����
                    // �����ϰ� IF�б��ؾ���
                    sOID      = (smOID)(sNTALog.mData1);
                    sTableOID = (smOID)(sNTALog.mData2);
                    IDE_ASSERT( smLayerCallback::restoreTableByAbort( aTrans,
                                                                      sOID,
                                                                      sTableOID,
                                                                      mRestart )
                                == IDE_SUCCESS );
                    break;

                case SMR_OP_INSTANT_AGING_AT_ALTER_TABLE:
                    //BUG-15627 Alter Add Column�� Abort�� ��� Table�� SCN�� �����ؾ� ��.
                    if ( mRestart == ID_FALSE )
                    {
                        sOID = (smOID)(sNTALog.mData1);
                        smLayerCallback::changeTableSCNForRebuild( sOID );
                    }

                    break;

                case SMR_OP_DIRECT_PATH_INSERT:
                    /* Direct-Path INSERT�� rollback �� ���, table info��
                     * �����ص� DPathIns ������ �������־�� �Ѵ�. */
                    if ( mRestart == ID_FALSE )
                    {
                        IDE_TEST( smLayerCallback::setExistDPathIns(
                                                aTrans,
                                                (smOID)sNTALog.mData1,
                                                ID_FALSE ) /* aExistDPathIns */
                                  != IDE_SUCCESS );
                    }
                    break;

                default :
                    break;
                }

                //write dummy compensation log
                sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);
                IDE_TEST( smrLogMgr::writeCMPSLogRec(aStatistics,
                                                    aTrans,
                                                    SMR_LT_DUMMY_COMPENSATION,
                                                    &sPrevUndoLSN,
                                                    NULL,
                                                    NULL) != IDE_SUCCESS );
                smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);

            }
        }
    }

    /*
      Transaction�� Cur Undo Next LSN�� �����Ѵ�.
    */

    sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);
    smLayerCallback::setCurUndoNxtLSN( aTrans, &sPrevUndoLSN );
    smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);
    mCurLogPtr     = NULL;
    mCurLogHeadPtr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_log );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidLog,
                                  sCurUndoLSN.mFileNo,
                                  sCurUndoLSN.mOffset ) );
    } 
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        sPrevUndoLSN = smrLogHeadI::getPrevLSN(&sLogHead);
        smLayerCallback::setCurUndoNxtLSN( aTrans, &sPrevUndoLSN );
        smrLogHeadI::setPrevLSN(&sLogHead, sPrevUndoLSN);
        mCurLogPtr     = NULL;
        mCurLogHeadPtr = NULL;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ʈ����� öȸ
 * aLSN [IN]   - rollback�� ������ LSN 
                 (������� undo�� Ư�� LSN , Ȥ�� init ���� �Ѿ�� ) 
 **********************************************************************/
IDE_RC smrRecoveryMgr::undoTrans( idvSQL   * aStatistics,
                                  void     * aTrans,
                                  smLSN    * aLSN )
{
    smLSN         sCurUndoLSN;
    smrLogFile*   sLogFilePtr = NULL;
    SInt          sState      = 1;

    sCurUndoLSN = smLayerCallback::getLstUndoNxtLSN( aTrans );

    smLayerCallback::setCurUndoNxtLSN( aTrans, &sCurUndoLSN );

    /* BUG-21620: PSM���� Explicit Savepoint�� ������, PSM������ Explicit
     * Savepoint�� Partial Rollback�ϸ� ������ ������ �����մϴ�. */
    while ( 1 )
    {
        if( ( !SM_IS_LSN_MAX( *aLSN ) ) &&
            ( aLSN->mFileNo >= sCurUndoLSN.mFileNo ) &&
            ( aLSN->mOffset >= sCurUndoLSN.mOffset ) )
        {
            break;
        }

        /* Transaction�� ù��° �α��� Prev LSN�� Transaction�� ���
         * �α׸� Rolback������ Transaction�� ������ CLR�α��� PrevLsN����
         * < -1, -1>���� ������ �ȴ�. */
        if ( ( sCurUndoLSN.mFileNo == ID_UINT_MAX  ) ||
             ( sCurUndoLSN.mOffset == ID_UINT_MAX  ) )
        {
            IDE_ASSERT( sCurUndoLSN.mFileNo == ID_UINT_MAX );
            IDE_ASSERT( sCurUndoLSN.mOffset == ID_UINT_MAX );
            break;
        }

        IDE_TEST( undo( aStatistics,
                        aTrans,
                        &( sLogFilePtr )) != IDE_SUCCESS );

        sCurUndoLSN = smLayerCallback::getCurUndoNxtLSN( aTrans );
    }

    if ( sLogFilePtr != NULL)
    {
        sState = 0;
        IDE_TEST( smrLogMgr::closeLogFile( sLogFilePtr ) != IDE_SUCCESS );
        sLogFilePtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( ( sLogFilePtr != NULL ) && ( sState != 0 ) )
    {
        IDE_ASSERT( smrLogMgr::closeLogFile( sLogFilePtr )
                    == IDE_SUCCESS );
        sLogFilePtr = NULL;
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : prepare Ʈ������� ���� table lock ȹ��
 **********************************************************************/
IDE_RC smrRecoveryMgr::acquireLockForInDoubt()
{

    void*              sTrans;
    smrLogFile*        sLogFile = NULL;
    smLSN              sCurLSN;
    SChar*             sLogPtr;
    smrLogHead         sLogHead;
    smrXaPrepareLog    sXAPrepareLog;
    UInt               sOffset;
    smLSN              sTrxLstUndoNxtLSN;
    SInt               sState = 1;
    smrCompRes       * sCompRes;
    idBool             sIsValid = ID_FALSE;
    UInt               sLogSizeAtDisk = 0;


    if ( smLayerCallback::getPrepareTransCount()  > 0 )
    {
        sTrans = smLayerCallback::getFirstPreparedTrx();
        while( sTrans != NULL )
        {
            sOffset = 0;
            /* ----------------------------------------
               heuristic complete�� Ʈ����ǿ� ���ؼ���
               table lock�� ȹ������ ����
               ---------------------------------------- */
            IDE_ASSERT( smLayerCallback::isXAPreparedCommitState( sTrans ) );

            sTrxLstUndoNxtLSN = smLayerCallback::getLstUndoNxtLSN( sTrans );
            SM_SET_LSN( sCurLSN,
                        sTrxLstUndoNxtLSN.mFileNo,
                        sTrxLstUndoNxtLSN.mOffset );

            // Ʈ������� �α� ����/�������� ���ҽ��� �����´�
            IDE_TEST( smLayerCallback::getTransCompRes( sTrans, &sCompRes )
                      != IDE_SUCCESS );

            IDE_TEST( smrLogMgr::readLog(
                        & sCompRes->mDecompBufferHandle,
                        &sCurLSN,
                        ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                        &sLogFile,
                        &sLogHead,
                        &sLogPtr,
                        &sIsValid,
                        &sLogSizeAtDisk ) != IDE_SUCCESS );

            IDE_TEST_RAISE( sIsValid == ID_FALSE, err_invalid_log )

            /* ----------------------------------------------------------------
               prepared transaction�� ���������� ����� �α״� prepare �α��̸�
               ���� ���� prepare log�� ������ ��� �̵���
               ���������� �������� �����Ѵ�.
               -------------------------------------------------------------- */
            while (smrLogHeadI::getType(&sLogHead) == SMR_LT_XA_PREPARE)
            {
                idlOS::memcpy(&sXAPrepareLog,
                              sLogPtr,
                              SMR_LOGREC_SIZE(smrXaPrepareLog));

                IDE_ASSERT( smrLogHeadI::getTransID( &sXAPrepareLog.mHead ) ==
                            smLayerCallback::getTransID( sTrans ) );

                //lockTabe using preparedLog .
                //BUGBUG s_offset .
                smLayerCallback::lockTableByPreparedLog( sTrans,
                                                         (SChar *)(sLogPtr + SMR_LOGREC_SIZE(smrXaPrepareLog)),
                                                         sXAPrepareLog.mLockCount,&sOffset );

                SM_SET_LSN(sCurLSN,
                           smrLogHeadI::getPrevLSNFileNo(&sLogHead),
                           smrLogHeadI::getPrevLSNOffset(&sLogHead));

                // Ʈ������� �α� ����/�������� ���ҽ��� �����´�
                IDE_TEST( smLayerCallback::getTransCompRes( sTrans, &sCompRes )
                          != IDE_SUCCESS );

                IDE_TEST( smrLogMgr::readLog(
                                     & sCompRes->mDecompBufferHandle,
                                     &sCurLSN,
                                     ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                     &sLogFile,
                                     &sLogHead,
                                     &sLogPtr,
                                     &sIsValid,
                                     &sLogSizeAtDisk ) 
                         != IDE_SUCCESS );

                IDE_TEST_RAISE( sIsValid == ID_FALSE, err_invalid_log )
            }
            sTrans = smLayerCallback::getNxtPreparedTrx( sTrans );
        }

        if ( sLogFile != NULL)
        {
            sState = 0;
            IDE_TEST( smrLogMgr::closeLogFile(sLogFile) != IDE_SUCCESS );
            sLogFile = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_log );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidLog,
                                  sCurLSN.mFileNo,
                                  sCurLSN.mOffset ) );
    }
    IDE_EXCEPTION_END;

    if ( (sLogFile != NULL) && (sState != 0) )
    {
        IDE_PUSH();
        IDE_ASSERT(smrLogMgr::closeLogFile(sLogFile)
                   == IDE_SUCCESS );
        sLogFile = NULL;

        IDE_POP();
    }
    return IDE_FAILURE;

}

/***********************************************************************
 * Description : archive �α� ��� ���� �� �α׾�Ŀ flush
 **********************************************************************/
IDE_RC smrRecoveryMgr::updateArchiveMode(smiArchiveMode aArchiveMode)
{
    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_UPDATE_ARCH_MODE,
                (aArchiveMode == SMI_LOG_ARCHIVE) ?
                (SChar*)"ARCHIVELOG" : (SChar*)"NOARCHIVELOG");

    IDE_TEST( mAnchorMgr.updateArchiveAndFlush(aArchiveMode) != IDE_SUCCESS );
    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_UPDATE_ARCH_MODE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_UPDATE_ARCH_MODE_FAILURE);

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : checkpoint for DRDB call by buffer flush thread
 ***********************************************************************/
IDE_RC smrRecoveryMgr::checkpointDRDB(idvSQL* aStatistics)
{
    smLSN  sOldestLSN;
    smLSN  sEndLSN;

    /* PROJ-2162 RestartRiskReduction
     * DB�� Consistent���� ������, Checkpoint�� ���� �ʴ´�. */
    if ( ( getConsistency() == ID_FALSE ) &&
         ( smuProperty::getCrashTolerance() != 2 ) )
    {
        IDE_CALLBACK_SEND_MSG("[CHECKPOINT-FAILURE] Inconsistent DB.");
    }
    else
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_DRDB_START);

        smrLogMgr::getLstLSN( &sEndLSN );

        IDE_TEST( makeBufferChkpt( aStatistics,
                                   ID_FALSE,
                                   &sEndLSN,
                                   &sOldestLSN )
                  != IDE_SUCCESS );

        // fix BUG-16467
        IDE_ASSERT( sctTableSpaceMgr::lockForCrtTBS() == IDE_SUCCESS );

        // ��ũ ���̺����̽��� Redo LSN�� DBF ����� ����Ÿ���� �����
        // �����ϱ� ���� TBS �����ڿ� �ӽ÷� �����Ѵ�.
        sctTableSpaceMgr::setRedoLSN4DBFileMetaHdr( &sOldestLSN,  // DiskRedoLSN
                                                    NULL );       // MemRedoLSN 

        // ����Ÿ���Ͽ� ����� ����Ѵ�.
        IDE_TEST( sddDiskMgr::syncAllTBS( aStatistics,
                                          SDD_SYNC_CHKPT )
                  != IDE_SUCCESS );

    	IDE_TEST( sdsBufferMgr::syncAllSB( aStatistics ) != IDE_SUCCESS );

        // ����üũ����Ʈ�̱� ������ Disk Redo LSN�� �����ϸ� �ȴ�
        // LogAnchor�� Disk Redo LSN�� �����ϰ� Flush�Ѵ�.
        IDE_TEST( mAnchorMgr.updateRedoLSN( &sOldestLSN, NULL )
                  != IDE_SUCCESS );

        // üũ����Ʈ ����
        // Stable�� �ݵ�� �־�� �ϰ� , Unstable�� �����Ѵٸ�
        // �ּ��� ��ȿ�� �����̾�� �Ѵ�.
        IDE_ASSERT( smmTBSMediaRecovery::identifyDBFilesOfAllTBS( ID_TRUE )
                    == IDE_SUCCESS );
        IDE_ASSERT( sddDiskMgr::identifyDBFilesOfAllTBS( aStatistics,
                                                         ID_TRUE )
                    == IDE_SUCCESS );

        //PROJ-2133 incremental backup
        if ( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE )
        {
            IDE_TEST( smriChangeTrackingMgr::flush() != IDE_SUCCESS );
        }       

        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_DRDB_SUMMARY,
                    sOldestLSN.mFileNo,
                    sOldestLSN.mOffset);

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_DRDB_END);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_DRDB_END);

    return IDE_FAILURE;

}


/***********************************************************************
 * Description : buffer checkpoint ����
 **********************************************************************/
IDE_RC smrRecoveryMgr::makeBufferChkpt( idvSQL      * aStatistics,
                                        idBool        aIsFinal,
                                        smLSN       * aEndLSN,     // in
                                        smLSN       * aOldestLSN)  // out
{
    smLSN  sDiskRedoLSN;
    smLSN  sSBufferRedoLSN;
 

    /* PROJ-2102 buffer pool�� dirtypage�� �ֽ� �̹Ƿ� 2nd->bufferpool�� ������ ���� */
    IDE_TEST( sdsBufferMgr::flushDirtyPagesInCPList( aStatistics,
                                                     ID_FALSE )// do not flush all
              != IDE_SUCCESS );

    /* BUG-22386 �� internal checkpoint������ CPList�� BCB�� flush�Ѵ�. */
    IDE_TEST( sdbBufferMgr::flushDirtyPagesInCPListByCheckpoint( 
                                                        aStatistics,
                                                        aIsFinal )
              != IDE_SUCCESS );
    /* replacement flush�� ���� secondary buffer�� dirty page�� �߰� �߻��Ҽ� ����. */
    IDE_TEST( sdsBufferMgr::flushDirtyPagesInCPList( aStatistics,
                                                     ID_FALSE )// do not flush all
              != IDE_SUCCESS );

    sdbBufferMgr::getMinRecoveryLSN( aStatistics, &sDiskRedoLSN );

    sdsBufferMgr::getMinRecoveryLSN( aStatistics, &sSBufferRedoLSN );

    if ( smrCompareLSN::isLT( &sDiskRedoLSN, &sSBufferRedoLSN )
            == ID_TRUE )
    {
        /* PBuffer�� �ִ� RecoveryLSN(sDiskRedoLSN)�� 
         * Sscondary Buffer�� �ִ� RecoveryLSN(sSBufferRedoLSN) ���� ������
         * PBuffer�� �ִ� RecoveryLSN  ���� */
        SM_GET_LSN( *aOldestLSN, sDiskRedoLSN );
    }
    else
    {
        SM_GET_LSN( *aOldestLSN, sSBufferRedoLSN );
    }

    /* DRDB�� DIRTY PAGE�� �������� �ʴ´ٸ� ���� EndLSN���� �����Ѵ�. */
    if ( ( aOldestLSN->mFileNo == ID_UINT_MAX ) &&
         ( aOldestLSN->mOffset == ID_UINT_MAX ) )
    {
        *aOldestLSN = *aEndLSN;
    }
    else
    {
        IDE_ASSERT( ( aOldestLSN->mFileNo != ID_UINT_MAX) &&
                    ( aOldestLSN->mOffset != ID_UINT_MAX) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 Description :free space�� üũ����Ʈ�� ������ �� ���� ��ŭ �������
              ���θ� ������. (BUG-32177)
**********************************************************************/
idBool smrRecoveryMgr::existDiskSpace4LogFile(void)
{

    ULong           sSize;
    const SChar  *  sLogFileDir=NULL;
    idBool          sRet= ID_TRUE;

    sSize = smuProperty::getReservedDiskSizeForLogFile();

    if ( sSize != 0 )
    {
        sLogFileDir = smuProperty::getLogDirPath();

        IDE_ASSERT( sLogFileDir != NULL );

        if ( (ULong)idlVA::getDiskFreeSpace( sLogFileDir ) <= sSize )
        {
            return ID_FALSE;
        }
    }

    return sRet;
}

/***********************************************************************
 Description : checkpoint ����
    [IN] aTrans        - Checkpoint�� �����ϴ� Transaction
    [IN] aCheckpointReason - Checkpoint�� �ϰ� �� ����
    [IN] aChkptType    - Checkpoint���� - Memory �� ?
                                          Disk �� Memory ��� ?
    [IN] aRemoveLogFile - ���ʿ��� �α����� ���� ����
    [IN] aIsFinal       - smrRecoveryMgr::finalize() ���� ȣ��Ǿ�����
**********************************************************************/
IDE_RC smrRecoveryMgr::checkpoint( idvSQL*      aStatistics,
                                   SInt         aCheckpointReason,
                                   smrChkptType aChkptType,
                                   idBool       aRemoveLogFile,
                                   idBool       aIsFinal )
{
    IDE_DASSERT((aChkptType == SMR_CHKPT_TYPE_MRDB) ||
                (aChkptType == SMR_CHKPT_TYPE_BOTH));


    /*
     * BUG-32177  The server might hang when disk is full during checkpoint.
     */
    if ( existDiskSpace4LogFile() == ID_FALSE )
    {
        /*
         * �α������� ���� ��ũ������ �����Ͽ� check point�� ������.
         * �̶��� �޽������� �Ѹ��� ���� checkpoint�� �ٽýõ��ϰԵ�.
         * (internal only!)
         */
        IDE_CALLBACK_SEND_MSG("[CHECKPOINT-FAILURE] disk is full.");
        IDE_CONT( SKIP );
    }

    /* PROJ-2162 RestartRiskReduction
     * DB�� Consistent���� ������, Checkpoint�� ���� �ʴ´�. */
    if ( ( getConsistency() == ID_FALSE ) &&
         ( smuProperty::getCrashTolerance() != 2 ) )
    {
        IDE_CALLBACK_SEND_MSG("[CHECKPOINT-FAILURE] Inconsistent DB.");
        IDE_CONT( SKIP );
    }

    //--------------------------------------------------
    //  Checkpoint�� �� �ϰ� �Ǿ������� log�� �����.
    //--------------------------------------------------
    IDE_TEST( logCheckpointReason( aCheckpointReason ) != IDE_SUCCESS );

    //---------------------------------
    // ���� Checkpoint ����
    //---------------------------------
    IDE_TEST( checkpointInternal( aStatistics,
                                  aChkptType,
                                  aRemoveLogFile,
                                  aIsFinal ) != IDE_SUCCESS );

    // PROJ-2133 incremental backup
    if ( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smriChangeTrackingMgr::flush() != IDE_SUCCESS );
    }       

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL("[CHECKPOINT-FAILURE]");

    idlOS::abort();

    return IDE_FAILURE;
}


/*
  Checkpoint ���� �Լ� - ���� Checkpoint�� �����Ѵ�.

  [IN] aChkptType    - Checkpoint���� - Memory �� ?
  Disk �� Memory ��� ?
  [IN] aRemoveLogFile - ���ʿ��� �α����� ���� ����
  [IN] aFinal         -

  checkpoint�� �����Ǵ� DRDB�� MRDB�� RecvLSN��
  �ٸ��� �ֱ� ������ begin checkpoint log�� ���
  ����س��ƾ� �Ѵ�.

  RecvLSN�� ������������ �����ȴ�.
  - MRDB�� ��� : checkpoint������ Active Transaction
  �� ���� ���� transaction�� ���� ���� LSN
  - DRDB�� ��� : ���۸Ŵ����� flush-list�� �����ϴ�
  BCB�� ���� �������� ����� BCB�� oldest LSN

  + 2nd. code design
  - logging level�� ���� ��� Ʈ������� ���Ḧ �����
  - MRDB/DRDB�� recovery LSN�� �����Ͽ� ���� checkpoint �α׿� ���
  - ���� checkpoint �α� ����Ѵ�.
  - phase 1 : dirty-page list�� flush �Ѵ�.(smr->disk)
  - dirty-page�� �Űܿ´�.(smm->smr)
  - ���� �αװ������� last LSN������ �α׸� sync��Ų��.
  - phase 2 : dirty-page list�� flush �Ѵ�.(smr->disk)
  - DB ������ sync��Ų��.
  - �Ϸ� checkpoint �α׸� ����Ѵ�.
  - ������ fst �α����Ϲ�ȣ�� lst �α����Ϲ�ȣ�� ���Ѵ�.
  - loganchor�� checkpoint ������ ����Ѵ�.
  - logging level�� ���� Ʈ������� �߻��� �� �ֵ��� ó���Ѵ�.

  ============================================================
  CHECKPOINT ����
  ============================================================
  A. Restart Recovery�ÿ� Redo���� LSN���� ���� Recovery LSN�� ���
  A-1 Memory Recovery LSN ����
  A-2 Disk Recovery LSN ����
  - Disk Dirty Page Flush        [step0]
  - Tablespace Log Anchor ����ȭ [step1]
  B. Write  Begin_CHKPT                 [step2]
  C. Memory Dirty Page���� Checkpoint Image�� Flush
  C-1 Flush Dirty Pages              [step3]
  C-2 Sync Log Files
  C-3 Sync Memory Database           [step4]
  D. ��� ���̺����̽��� ����Ÿ���� ��Ÿ����� RedoLSN ����
  E. Write  End_CHKPT                   [step5]
  F. After  End_CHKPT
  E-1. Sync Log Files            [step6]
  E-2. Get Remove Log File #         [step7]
  E-3. Update Log Anchor             [step8]
  E-4. Remove Log Files              [step9]
  G. üũ����Ʈ ����
  ============================================================
*/
IDE_RC smrRecoveryMgr::checkpointInternal( idvSQL*      aStatistics,
                                           smrChkptType aChkptType,
                                           idBool       aRemoveLogFile,
                                           idBool       aIsFinal )
{
    smLSN   sBeginChkptLSN;
    smLSN   sEndChkptLSN;

    smLSN   sDiskRedoLSN;
    smLSN   sEndLSN;

    smLSN   sRedoLSN;
    smLSN   sSyncLstLSN;

    smLSN   sDtxMinLSN; /* BUG-46754 */

    smLSN * sMediaRecoveryLSN;

    IDU_FIT_POINT( "1.BUG-42785@smrRecoveryMgr::checkpointInternal::sleep" );

    //=============================================
    //  A. Restart Recovery�ÿ� Redo���� LSN���� ���� Recovery LSN�� ���
    //    A-1 Memory Recovery LSN ����
    //    A-2 Disk Recovery LSN ����
    //        - Disk Dirty Page Flush        [step0]
    //        - Tablespace Log Anchor ����ȭ [step1]
    //=============================================

    IDE_TEST( chkptCalcRedoLSN( aStatistics,
                                aChkptType,
                                aIsFinal,
                                &sRedoLSN,
                                &sDiskRedoLSN,
                                &sEndLSN ) != IDE_SUCCESS );

    //=============================================
    // B. Write Begin_CHKPT
    //=============================================
    IDE_TEST( writeBeginChkptLog( aStatistics,
                                  &sRedoLSN,
                                  sDiskRedoLSN,
                                  sEndLSN,
                                  &sBeginChkptLSN,
                                  &sDtxMinLSN,
                                  aIsFinal ) != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-13894/BUG-13894.sql */
    IDU_FIT_POINT( "1.BUG-13894@smrRecoveryMgr::checkpointInternal" );

    //=============================================
    // C. Memory Dirty Page���� Checkpoint Image�� Flush
    //    C-1 Flush Dirty Pages              [step3]
    //    C-2 Sync Log Files
    //    C-3 Sync Memory Database           [step4]
    //=============================================

    IDE_TEST( chkptFlushMemDirtyPages( &sSyncLstLSN, aIsFinal )
              != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-13894/BUG-13894.sql */
    IDU_FIT_POINT( "2.BUG-13894@smrRecoveryMgr::checkpointInternal" );

    /* FIT/ART/sm/Bugs/BUG-23700/BUG-23700.tc */
    IDU_FIT_POINT( "1.BUG-23700@smrRecoveryMgr::checkpointInternal" );

    //=============================================
    // D. ��� ���̺����̽��� ����Ÿ���� ��Ÿ����� RedoLSN ����
    //=============================================
    IDE_TEST( chkptSetRedoLSNOnDBFiles( aStatistics,
                                        &sRedoLSN,
                                        sDiskRedoLSN ) != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-13894/BUG-13894.sql */
    IDU_FIT_POINT( "3.BUG-13894@smrRecoveryMgr::checkpointInternal" );

    //=============================================
    // E. Write End_CHKPT
    //=============================================
    IDE_TEST( writeEndChkptLog( aStatistics,
                                & sEndChkptLSN ) != IDE_SUCCESS );


    /* CASE-6985 ���� ������ startup�ϸ� user�� ���̺�,
     * �����Ͱ� ��� �����: Checkpoint�Ϸ����� �̿����� ������ �����Ѵ�.
     * ���� Valid���� �ʴٸ� ������ �Ϸ�� ����Ÿ���̽� �̹����� �о ������ ���̴�.*/
    smmDatabase::validateCommitSCN();

    //=============================================
    // F. After End_CHKPT
    //    E-1. Sync Log Files                [step6]
    //    E-2. Get Remove Log File #         [step7]
    //    E-3. Update Log Anchor             [step8]
    //    E-4. Remove Log Files              [step9]
    //=============================================
    IDE_TEST( chkptAfterEndChkptLog( aRemoveLogFile,
                                     aIsFinal,
                                     &sBeginChkptLSN,
                                     &sEndChkptLSN,
                                     &sDiskRedoLSN,
                                     &sRedoLSN,
                                     &sSyncLstLSN,
                                     &sDtxMinLSN )
              != IDE_SUCCESS );

    /* BUG-43499
     * Disk�� Memory redo LSN�� ���� LSN�� Media Recovery LSN���� ���� �Ѵ�. */
    sMediaRecoveryLSN = (smrCompareLSN::isLTE( &sDiskRedoLSN, &sRedoLSN ) == ID_TRUE ) 
                        ? &sDiskRedoLSN : &sRedoLSN;

    IDE_TEST( mAnchorMgr.updateMediaRecoveryLSN(sMediaRecoveryLSN) != IDE_SUCCESS );

    //=============================================
    // G. üũ����Ʈ ����
    // üũ����Ʈ ���̸� ���ڷ� ID_TRUE�� �ѱ�� .
    //=============================================
    // Stable�� �ݵ�� �־�� �ϰ� , Unstable�� �����Ѵٸ�
    // �ּ��� ��ȿ�� �����̾�� �Ѵ�.
    IDE_ASSERT( smmTBSMediaRecovery::identifyDBFilesOfAllTBS( ID_TRUE )
                 == IDE_SUCCESS );
    IDE_ASSERT( sddDiskMgr::identifyDBFilesOfAllTBS( aStatistics,
                                                     ID_TRUE )
                 == IDE_SUCCESS );

    //---------------------------------
    // Checkpoint Summary Message
    //---------------------------------
    IDE_TEST( logCheckpointSummary( sBeginChkptLSN,
                                    sEndChkptLSN,
                                    &sRedoLSN,
                                    sDiskRedoLSN ) != IDE_SUCCESS );

    IDU_FIT_POINT( "5.BUG-42785@smrRecoveryMgr::checkpointInternal::wakeup" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*
  Checkpoint�� �� �ϰ� �Ǿ������� altibase_sm.log�� �����.

  [IN] aCheckpointReason - Checkpoint�� �ϰ� �� ����
*/
IDE_RC smrRecoveryMgr::logCheckpointReason( SInt aCheckpointReason )
{
    switch (aCheckpointReason)
    {
        case SMR_CHECKPOINT_BY_SYSTEM:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_RECOVERYMGR_CHKP1);
            break;

        case SMR_CHECKPOINT_BY_LOGFILE_SWITCH:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_RECOVERYMGR_CHKP2,
                        smuProperty::getChkptIntervalInLog());
            break;

        case SMR_CHECKPOINT_BY_TIME:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_RECOVERYMGR_CHKP3,
                        smuProperty::getChkptIntervalInSec());
            break;

        case SMR_CHECKPOINT_BY_USER:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_RECOVERYMGR_CHKP4);
            break;
    }

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP5);

    return IDE_SUCCESS;
}


/*
  Begin Checkpoint Log�� ����Ѵ�.

  [IN] aRedoLSN - Redo ���� LSN
  [IN] aDiskRedoLSN - Disk Redo LSN
  [OUT] aBeginChkptLSN - Begin Checkpoint Log�� LSN
*/
IDE_RC smrRecoveryMgr::writeBeginChkptLog( idvSQL* aStatistics,
                                           smLSN * aRedoLSN,
                                           smLSN   aDiskRedoLSN,
                                           smLSN   aEndLSN,
                                           smLSN * aBeginChkptLSN,
                                           smLSN * aDtxMinLSN,
                                           idBool  aIsFinal )
{
    smrBeginChkptLog   sBeginChkptLog;
    smLSN              sBeginChkptLSN;

    //---------------------------------
    // Set Begin CHKPT Log
    //---------------------------------
    smrLogHeadI::setType(&sBeginChkptLog.mHead, SMR_LT_CHKPT_BEGIN);
    smrLogHeadI::setSize(&sBeginChkptLog.mHead,
                         SMR_LOGREC_SIZE(smrBeginChkptLog));
    smrLogHeadI::setTransID(&sBeginChkptLog.mHead, SM_NULL_TID);
    smrLogHeadI::setPrevLSN(&sBeginChkptLog.mHead,
                            ID_UINT_MAX,
                            ID_UINT_MAX);

    smrLogHeadI::setFlag(&sBeginChkptLog.mHead, SMR_LOG_TYPE_NORMAL);
    sBeginChkptLog.mTail = SMR_LT_CHKPT_BEGIN;

    smrLogHeadI::setReplStmtDepth( &sBeginChkptLog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    sBeginChkptLog.mEndLSN = *aRedoLSN;

    sBeginChkptLog.mDiskRedoLSN = aDiskRedoLSN;

    /* BUG-48501 
       Shutdown�ÿ� DK����� SM��⺸�� ���� ��������.
       Shutdonw�ÿ��� �ʱⰪ�� �����ϵ��� �����Ͽ���. */
    if ( ( aIsFinal == ID_TRUE ) ||
         ( mGetDtxMinLSNFunc == NULL ) )
    {
        SMI_LSN_MAX( sBeginChkptLog.mDtxMinLSN );
    }
    else
    {
        sBeginChkptLog.mDtxMinLSN = mGetDtxMinLSNFunc(); /* dktGlobalTxMgr::getDtxMinLSN() */
    }

    IDE_TEST( smrLogMgr::writeLog( aStatistics,
                                   NULL,
                                   (SChar*)& sBeginChkptLog,
                                   NULL,             // Prev LSN Ptr
                                   & sBeginChkptLSN, // Log Begin LSN
                                   NULL,             // END LSN Ptr
                                   SM_NULL_OID )
              != IDE_SUCCESS );


    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP6,
                sBeginChkptLSN.mFileNo,
                sBeginChkptLSN.mOffset,
                aEndLSN.mFileNo,
                aEndLSN.mOffset,
                aDiskRedoLSN.mFileNo,
                aDiskRedoLSN.mOffset);

    * aBeginChkptLSN = sBeginChkptLSN ;
    * aDtxMinLSN     = sBeginChkptLog.mDtxMinLSN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ��� ���̺����̽��� ����Ÿ���� ��Ÿ����� RedoLSN ����

  [IN] aRedoLSN  - Disk/Memory DB�� Redo ���� LSN
  [IN] aDiskRedoLSN - Disk DB�� Redo ���� LSN
*/
IDE_RC smrRecoveryMgr::chkptSetRedoLSNOnDBFiles( idvSQL* aStatistics,
                                                 smLSN * aRedoLSN,
                                                 smLSN   aDiskRedoLSN )
{

    //=============================================
    // D. ��� ���̺����̽��� ����Ÿ���� ��Ÿ����� RedoLSN ����
    //=============================================

    // fix BUG-16467
    IDE_ASSERT( sctTableSpaceMgr::lockForCrtTBS() == IDE_SUCCESS );

    // Redo LSN�� ��� ���̺����̽��� ����Ÿ����/üũ����Ʈ
    // �̹����� ��Ÿ����� ����ϱ� ���� TBS �����ڿ� �ӽ÷� �����Ѵ�.
    sctTableSpaceMgr::setRedoLSN4DBFileMetaHdr( &aDiskRedoLSN,
                                                aRedoLSN );

    // ��ũ ����Ÿ���ϰ� üũ����Ʈ�̹����� ��Ÿ����� RedoLSN�� ����Ѵ�.
    IDE_TEST( smmTBSMediaRecovery::updateDBFileHdr4AllTBS() != IDE_SUCCESS );

    // ��ũ ����Ÿ���ϰ� üũ����Ʈ�̹����� ��Ÿ����� RedoLSN�� ����Ѵ�.
    IDE_TEST( sddDiskMgr::syncAllTBS( aStatistics,
                                      SDD_SYNC_CHKPT ) != IDE_SUCCESS );

    IDE_TEST( sdsBufferMgr::syncAllSB( aStatistics) != IDE_SUCCESS );

    // fix BUG-16467
    IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*  End Checkpoint Log�� ����Ѵ�.

    [OUT] aEndChkptLSN - End Checkpoint Log�� LSN
*/
IDE_RC smrRecoveryMgr::writeEndChkptLog( idvSQL* aStatistics,
                                         smLSN * aEndChkptLSN )
{
    smLSN             sEndChkptLSN;
    smrEndChkptLog    sEndChkptLog;

    //=============================================
    // E. Write End_CHKPT
    //=============================================

    // 5) Write end check point
    smrLogHeadI::setType(&sEndChkptLog.mHead, SMR_LT_CHKPT_END);
    smrLogHeadI::setSize(&sEndChkptLog.mHead, SMR_LOGREC_SIZE(smrEndChkptLog));
    smrLogHeadI::setTransID(&sEndChkptLog.mHead, SM_NULL_TID);

    smrLogHeadI::setPrevLSN(&sEndChkptLog.mHead,
                            ID_UINT_MAX,
                            ID_UINT_MAX);

    smrLogHeadI::setFlag(&sEndChkptLog.mHead, SMR_LOG_TYPE_NORMAL);
    sEndChkptLog.mTail = SMR_LT_CHKPT_END;

    smrLogHeadI::setReplStmtDepth( &sEndChkptLog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    IDE_TEST( smrLogMgr::writeLog( aStatistics,
                                   NULL,
                                   (SChar*)&sEndChkptLog,
                                   NULL,            // Prev LSN Ptr
                                   &(sEndChkptLSN), // Log Begin LSN
                                   NULL,            // END LSN Ptr
                                   SM_NULL_OID )
             != IDE_SUCCESS );


    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP7,
                sEndChkptLSN.mFileNo,
                sEndChkptLSN.mOffset);


    * aEndChkptLSN = sEndChkptLSN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  Checkpoint ��� Message�� �α��Ѵ�.
*/
IDE_RC smrRecoveryMgr::logCheckpointSummary( smLSN   aBeginChkptLSN,
                                             smLSN   aEndChkptLSN,
                                             smLSN * aRedoLSN,
                                             smLSN   aDiskRedoLSN )
{
    //=============================================
    // F. üũ����Ʈ ����
    // üũ����Ʈ ���̸� ���ڷ� ID_TRUE�� �ѱ�� .
    //=============================================
    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP8,
                aBeginChkptLSN.mFileNo,
                aBeginChkptLSN.mOffset,
                aEndChkptLSN.mFileNo,
                aEndChkptLSN.mOffset,
                aDiskRedoLSN.mFileNo,
                aDiskRedoLSN.mOffset);

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP9,
                aRedoLSN->mFileNo,
                aRedoLSN->mOffset);

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP10);

    return IDE_SUCCESS;
}

/*
  ALTER TABLESPACE OFFLINE�� ���� Tablespace�� ��� Dirty Page�� Flush

  [IN] aTBSNode - Tablespace�� Node

  PROJ-1548-M5 Alter Tablespace Offline/Online/Discard -----------------

  Alter Tablespace Offline�� ȣ��Ǿ�
  Offline���� �����Ϸ��� Tablespace�� ������ ��� Tablespace�� ����
  Checkpoint�� �����Ѵ�.

  Offline���� �����Ϸ��� Tablespace��
  Dirty Page���� Disk�� Checkpoint Image�� Flush�ϱ� ���� ���ȴ�.

  Offline���� �����Ϸ��� Tablespace�� Dirty Page�� ����
  Checkpoint Image�� Flush�ϴ°��� ��Ģ�� ������,
  ���� Checkpoint��ƾ�� �״�� �̿��Ͽ� �ڵ庯�淮�� �ּ�ȭ �Ͽ���.

  [�˰���]

  (010) Checkpoint Latch ȹ��
  (020)   Checkpoint�� �����Ͽ� Dirty Page Flush �ǽ�
  ( Unstable DB ����, 0�� 1�� DB ��� �ǽ� )

  (030) Checkpoint Latch ����
*/
IDE_RC smrRecoveryMgr::flushDirtyPages4AllTBS()
{
    UInt sStage = 0;
    UInt i;

    //////////////////////////////////////////////////////////////////////
    //  (010) Checkpoint �������� ���ϵ��� ����
    IDE_TEST( smrChkptThread::blockCheckpoint() != IDE_SUCCESS );
    sStage = 1;

    //////////////////////////////////////////////////////////////////////
    //  (020)   Dirty Page Flush �ǽ�
    //          Tablespace�� Unstable Checkpoint Image���� �����Ͽ�,
    //          0�� 1�� Checkpoint Image�� ���� ������ ����

    for (i=1; i<=SMM_PINGPONG_COUNT; i++)
    {
        IDE_TEST( checkpointInternal( NULL, /* idvSQL* */
                                      SMR_CHKPT_TYPE_MRDB,
                                      ID_TRUE, /* Remove Log File */
                                      ID_FALSE /* aFinal */ )
                  != IDE_SUCCESS );
    }

    //////////////////////////////////////////////////////////////////////
    //  (030) Checkpoint ���� �����ϵ��� unblock
    sStage = 0;
    IDE_TEST( smrChkptThread::unblockCheckpoint() != IDE_SUCCESS );

    return IDE_SUCCESS ;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( smrChkptThread::unblockCheckpoint()
                            == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE ;
}


/*
  Ư�� Tablespace�� Dirty Page�� ������ Ȯ���Ѵ�.

  [IN] aTBSNode - Dirty Page�� ������ Ȯ���� Tablespace�� Node
*/
IDE_RC smrRecoveryMgr::assertNoDirtyPagesInTBS( smmTBSNode * aTBSNode )
{
    scPageID sDirtyPageCount;

    IDE_TEST( smrDPListMgr::getDirtyPageCountOfTBS( aTBSNode->mHeader.mID,
                                                    & sDirtyPageCount )
              != IDE_SUCCESS );

    // Dirty Page�� ����� �Ѵ�.
    IDE_ASSERT( sDirtyPageCount == 0);

    return IDE_SUCCESS ;

    IDE_EXCEPTION_END;

    return IDE_FAILURE ;
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ��� ���̺����̽� ������ Loganchor�� �����Ѵ�.
   �������������� ���� ������������� ȣ��ȴ�.
*/
IDE_RC smrRecoveryMgr::updateAnchorAllTBS()
{
    return mAnchorMgr.updateAllTBSAndFlush();
}
/*
   PROJ-2102 Fast Sencondary Buffer
   ��� ���̺����̽� ������ Loganchor�� �����Ѵ�.
   �������������� ���� ������������� ȣ��ȴ�.
*/
IDE_RC smrRecoveryMgr::updateAnchorAllSB( void )
{
    return mAnchorMgr.updateAllSBAndFlush();
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ����� TBS Node ������ Loganchor�� �����Ѵ�.

   [IN] aSpaceNode : ���̺����̽� ���
*/
IDE_RC smrRecoveryMgr::updateTBSNodeToAnchor( sctTableSpaceNode*  aSpaceNode )
{
    return mAnchorMgr.updateTBSNodeAndFlush( aSpaceNode );
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ����� DBF Node ������ Loganchor�� �����Ѵ�.

   [IN] aFileNode  : ����Ÿ���� ���
*/
IDE_RC smrRecoveryMgr::updateDBFNodeToAnchor( sddDataFileNode*    aFileNode )
{
    return mAnchorMgr.updateDBFNodeAndFlush( aFileNode );
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   Chkpt Image �Ӽ��� Loganchor�� �����Ѵ�.

   [IN] aCrtDBFileInfo   -
   üũ����Ʈ Image �� ���� ���������� ���� Runtime ����ü
   Anchor Offset�� ����Ǿ� �ִ�.
   [IN] aChkptImageAttr  -
   üũ����Ʈ Image �Ӽ�
*/
IDE_RC smrRecoveryMgr::updateChkptImageAttrToAnchor( smmCrtDBFileInfo  * aCrtDBFileInfo,
                                                     smmChkptImageAttr * aChkptImageAttr )
{
    IDE_DASSERT( aCrtDBFileInfo  != NULL );
    IDE_DASSERT( aChkptImageAttr != NULL );

    return mAnchorMgr.updateChkptImageAttrAndFlush( aCrtDBFileInfo,
                                                    aChkptImageAttr );
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   Chkpt Path �Ӽ��� Loganchor�� �����Ѵ�.

   [IN] aChkptPathNode - ������ üũ����Ʈ ��� ���

*/
IDE_RC smrRecoveryMgr::updateChkptPathToLogAnchor( smmChkptPathNode * aChkptPathNode )
{
    IDE_ASSERT( aChkptPathNode != NULL );

    return mAnchorMgr.updateChkptPathAttrAndFlush( aChkptPathNode );
}

/* PDOJ-2102 Fast Secondary Buffer */
IDE_RC smrRecoveryMgr::updateSBufferNodeToAnchor( sdsFileNode  * aFileNode )
{
    return mAnchorMgr.updateSBufferNodeAndFlush( aFileNode );
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ������ TBS Node ������ Loganchor�� �߰��Ѵ�.

   [IN] aSpaceNode : ���̺����̽� ���
*/
IDE_RC smrRecoveryMgr::addTBSNodeToAnchor( sctTableSpaceNode*   aSpaceNode )
{
    UInt sAnchorOffset;

    IDE_TEST( mAnchorMgr.addTBSNodeAndFlush( aSpaceNode,
                                             &sAnchorOffset ) != IDE_SUCCESS );
    
    switch( sctTableSpaceMgr::getTBSLocation( aSpaceNode ) )
    {
        case SMI_TBS_MEMORY:
            {
                // ����Ǳ� ���̹Ƿ� ������ �����ؾ��Ѵ�.
                IDE_ASSERT( ((smmTBSNode*)aSpaceNode)->mAnchorOffset
                            == SCT_UNSAVED_ATTRIBUTE_OFFSET );

                // �޸� ���̺����̽�
                ((smmTBSNode*)aSpaceNode)->mAnchorOffset = sAnchorOffset;
            }
            break;
        case SMI_TBS_DISK:
            {
                IDE_ASSERT( ((sddTableSpaceNode*)aSpaceNode)->mAnchorOffset
                            == SCT_UNSAVED_ATTRIBUTE_OFFSET );

                // ��ũ ���̺����̽�
                ((sddTableSpaceNode*)aSpaceNode)->mAnchorOffset = sAnchorOffset;
            }
            break;
        case SMI_TBS_VOLATILE:
            {
                IDE_ASSERT( ((svmTBSNode*)aSpaceNode)->mAnchorOffset
                            == SCT_UNSAVED_ATTRIBUTE_OFFSET );

                // Volatile tablespace
                ((svmTBSNode*)aSpaceNode)->mAnchorOffset = sAnchorOffset;
            }
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ������ DBF Node ������ Loganchor�� �߰��Ѵ�.

   TBS Node�� NewFileID�� �Բ� �����Ͽ��� �Ѵ�.

   [IN] aSpaceNode : ���̺����̽� ���
   [IN] aFileNode  : ����Ÿ���� ���
*/
IDE_RC smrRecoveryMgr::addDBFNodeToAnchor( sddTableSpaceNode*   aSpaceNode,
                                           sddDataFileNode*     aFileNode )
{
    UInt  sAnchorOffset;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aFileNode  != NULL );

    IDE_TEST( mAnchorMgr.addDBFNodeAndFlush( aSpaceNode,
                                             aFileNode,
                                             &sAnchorOffset )
              != IDE_SUCCESS );

    // ����Ǳ� ���̹Ƿ� ������ �����ؾ��Ѵ�.
    IDE_ASSERT( aFileNode->mAnchorOffset
                == SCT_UNSAVED_ATTRIBUTE_OFFSET );

    aFileNode->mAnchorOffset = sAnchorOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ������ Chkpt Node Path������ Loganchor�� �߰��Ѵ�.

   TBS Node�� ChkptNode Count�� �Բ� �����Ͽ��� �Ѵ�.

   [IN] aSpaceNode : ���̺����̽� ���
   [IN] aChkptPathNode  : üũ����ƮPath ���
*/
IDE_RC smrRecoveryMgr::addChkptPathNodeToAnchor( smmChkptPathNode * aChkptPathNode )
{
    UInt sAnchorOffset;

    IDE_DASSERT( aChkptPathNode != NULL );

    IDE_TEST( mAnchorMgr.addChkptPathNodeAndFlush( aChkptPathNode,
                                                   &sAnchorOffset )
              != IDE_SUCCESS );

    // ����Ǳ� ���̹Ƿ� ������ �����ؾ��Ѵ�.
    IDE_ASSERT( aChkptPathNode->mAnchorOffset
                == SCT_UNSAVED_ATTRIBUTE_OFFSET );

    aChkptPathNode->mAnchorOffset = sAnchorOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   PROJ-1548 SM - User Memory TableSpace ���䵵��
   ������ Chkpt Image �Ӽ��� Loganchor�� �߰��Ѵ�.

   [IN] aCrtDBFileInfo   -
   üũ����Ʈ Image �� ���� ���������� ���� Runtime ����ü
   Anchor Offset�� ����Ǿ� �ִ�.
   [IN] aChkptImageAttr  : üũ����Ʈ Image �Ӽ�
*/
IDE_RC smrRecoveryMgr::addChkptImageAttrToAnchor(
    smmCrtDBFileInfo  * aCrtDBFileInfo,
    smmChkptImageAttr * aChkptImageAttr )
{
    UInt   sAnchorOffset;

    IDE_DASSERT( aCrtDBFileInfo  != NULL );
    IDE_DASSERT( aChkptImageAttr != NULL );

    IDE_TEST( mAnchorMgr.addChkptImageAttrAndFlush(
                                      aChkptImageAttr,
                                      &sAnchorOffset )
              != IDE_SUCCESS );

    // ����Ǳ� ���̹Ƿ� ������ �����ؾ��Ѵ�.
    IDE_ASSERT( aCrtDBFileInfo->mAnchorOffset
                == SCT_UNSAVED_ATTRIBUTE_OFFSET );

    aCrtDBFileInfo->mAnchorOffset = sAnchorOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   PROJ-2102 Fast Secondary Buffer

   [IN] aFileNode  : Secondary Buffer ���� ���
*/
IDE_RC smrRecoveryMgr::addSBufferNodeToAnchor( sdsFileNode*   aFileNode )
{
    UInt  sAnchorOffset;

    IDE_DASSERT( aFileNode  != NULL );

    IDE_TEST( mAnchorMgr.addSBufferNodeAndFlush( aFileNode,
                                                 &sAnchorOffset )
              != IDE_SUCCESS );

    // ����Ǳ� ���̹Ƿ� ������ �����ؾ��Ѵ�.
    IDE_ASSERT( aFileNode->mAnchorOffset
                == SCT_UNSAVED_ATTRIBUTE_OFFSET );

    aFileNode->mAnchorOffset = sAnchorOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description : loganchor�� TXSEG Entry ������ �ݿ��Ѵ�.
 *******************************************************************/
IDE_RC smrRecoveryMgr::updateTXSEGEntryCnt( UInt sEntryCnt )
{
    IDE_ASSERT( sEntryCnt > 0 );

    IDE_TEST( mAnchorMgr.updateTXSEGEntryCntAndFlush( sEntryCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� Ʈ����� ���׸�Ʈ ��Ʈ�� ������ ��ȯ
 *******************************************************************/
UInt smrRecoveryMgr::getTXSEGEntryCnt()
{
    return mAnchorMgr.getTXSEGEntryCnt();
}


/*******************************************************************
 * DESCRITPION : loganchor�κ��� binary db version�� ��ȯ
 *******************************************************************/
UInt smrRecoveryMgr::getSmVersionIDFromLogAnchor()
{
    return mAnchorMgr.getSmVersionID();
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� DISK REDO LSN�� ��ȯ
 *******************************************************************/
void smrRecoveryMgr::getDiskRedoLSNFromLogAnchor( smLSN* aDiskRedoLSN )
{
    *aDiskRedoLSN = mAnchorMgr.getDiskRedoLSN();
    return;
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� last delete �α����� no ��ȯ
 *******************************************************************/
UInt smrRecoveryMgr::getLstDeleteLogFileNo()
{

    return mAnchorMgr.getLstDeleteLogFileNo();

}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� archive �α� ��� ��ȯ
 *******************************************************************/
smiArchiveMode smrRecoveryMgr::getArchiveMode()
{

    return mAnchorMgr.getArchiveMode();
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� change tracking manager�� ���°� 
 *               Enable�Ǿ����� �����Ͽ� ��� ��ȯ
 * PROJ-2133 incremental backup
 *******************************************************************/
idBool smrRecoveryMgr::isCTMgrEnabled()
{
    idBool          sResult;
    smriCTMgrState  sCTMgrState;

    sCTMgrState = mAnchorMgr.getCTMgrState();

    if ( sCTMgrState == SMRI_CT_MGR_ENABLED )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }
    return sResult;
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� change tracking manager�� ���°� 
 *               Create������ �����Ͽ� ��� ��ȯ
 * PROJ-2133 incremental backup
 *******************************************************************/
idBool smrRecoveryMgr::isCreatingCTFile()
{
    idBool          sResult;
    smriCTMgrState  sCTMgrState;

    sCTMgrState = mAnchorMgr.getCTMgrState();

    if ( sCTMgrState == SMRI_CT_MGR_FILE_CREATING )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }
    return sResult;
}
/*******************************************************************
 * DESCRITPION :
 * PROJ-2133 incremental backup
 *******************************************************************/
IDE_RC smrRecoveryMgr::getDataFileDescSlotIDFromLogAncho4ChkptImage( 
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID)
{
    IDE_DASSERT( aDataFileDescSlotID != NULL );
    
    IDE_TEST( mAnchorMgr.getDataFileDescSlotIDFromChkptImageAttr( 
                                                aReadOffset,
                                                aDataFileDescSlotID )
              != IDE_SUCCESS );    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION :
 * PROJ-2133 incremental backup
 *******************************************************************/
IDE_RC smrRecoveryMgr::getDataFileDescSlotIDFromLogAncho4DBF( 
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID)
{
    IDE_DASSERT( aDataFileDescSlotID != NULL );
    
    IDE_TEST( mAnchorMgr.getDataFileDescSlotIDFromDBFNodeAttr( 
                                                aReadOffset,
                                                aDataFileDescSlotID )
              != IDE_SUCCESS );    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION : loganchor�κ��� backup info manager�� ���°� 
 *               initialize���� �����Ͽ� ��� ��ȯ
 * PROJ-2133 incremental backup
 *******************************************************************/
smriBIMgrState smrRecoveryMgr::getBIMgrState()
{
    return mAnchorMgr.getBIMgrState();
}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� CTFileAttr�� ������ �����Ѵ�.
 * PROJ-2133 incremental backup
 *******************************************************************/
IDE_RC smrRecoveryMgr::updateCTFileAttrToLogAnchor( 
                                            SChar          * aFileName,
                                            smriCTMgrState * aCTMgrState,
                                            smLSN          * aFlushLSN )
{
    IDE_TEST( mAnchorMgr.updateCTFileAttr( aFileName,
                                           aCTMgrState,
                                           aFlushLSN ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� BIFileAttr�� ������ �����Ѵ�.
 * PROJ-2133 incremental backup
 *******************************************************************/
IDE_RC smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                           SChar          * aFileName,
                                           smriBIMgrState * aBIMgrState,
                                           smLSN          * aBackupLSN,
                                           SChar          * aBackupDir,
                                           UInt           * aDeleteArchLogFile )
{
    IDE_TEST( mAnchorMgr.updateBIFileAttr( aFileName,
                                           aBIMgrState,
                                           aBackupLSN,
                                           aBackupDir,
                                           aDeleteArchLogFile ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� CTFile�̸��� �����´�.
 * PROJ-2133 incremental backup
 *******************************************************************/
SChar* smrRecoveryMgr::getCTFileName() 
{

    return mAnchorMgr.getCTFileName();

}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� BIFile�̸��� �����´�.
 * PROJ-2133 incremental backup
 *******************************************************************/
SChar* smrRecoveryMgr::getBIFileName() 
{

    return mAnchorMgr.getBIFileName();

}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� ������ flush LSN�� �����´�.
 * PROJ-2133 incremental backup
 *******************************************************************/
smLSN smrRecoveryMgr::getCTFileLastFlushLSNFromLogAnchor()
{

    return mAnchorMgr.getCTFileLastFlushLSN();

}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� ������ backup LSN�� �����´�.
 * PROJ-2133 incremental backup
 *******************************************************************/
smLSN smrRecoveryMgr::getBIFileLastBackupLSNFromLogAnchor()
{

    return mAnchorMgr.getBIFileLastBackupLSN();

}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� ������ ������ ����� backup LSN�� �����´�.
 * PROJ-2133 incremental backup
 *******************************************************************/
smLSN smrRecoveryMgr::getBIFileBeforeBackupLSNFromLogAnchor()
{

    return mAnchorMgr.getBIFileBeforeBackupLSN();

}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� incremental backup ���丮 ��θ�
 * �����Ѵ�.
 * PROJ-2133 incremental backup
 *******************************************************************/
IDE_RC smrRecoveryMgr::changeIncrementalBackupDir( SChar * aNewBackupDir )
{
    UInt    sBackupPathLen;
    SChar   sBackupDir[ SM_MAX_FILE_NAME ];

    idlOS::memset( sBackupDir, 0x00, SM_MAX_FILE_NAME );

    sBackupPathLen = idlOS::strlen(aNewBackupDir);

    if ( aNewBackupDir[ sBackupPathLen -1 ] != IDL_FILE_SEPARATOR)
    {
        idlOS::snprintf( sBackupDir, SM_MAX_FILE_NAME, "%s%c",
                         aNewBackupDir,                 
                         IDL_FILE_SEPARATOR );
    }  
    else
    {
        /* BUG-35113 ALTER DATABASE CHANGE BACKUP DIRECTORY '/backup_dir/'
         * statement is not working */
        idlOS::snprintf( sBackupDir, SM_MAX_FILE_NAME, "%s",
                         aNewBackupDir );
    } 

    IDE_TEST( mAnchorMgr.updateBIFileAttr( NULL,
                                           NULL,
                                           NULL,
                                           sBackupDir,
                                           NULL ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * DESCRITPION : loganchor�� ����� incremental backup ��ġ�� ��ȯ�Ѵ�.
 * PROJ-2133 incremental backup
 *******************************************************************/
SChar* smrRecoveryMgr::getIncrementalBackupDirFromLogAnchor()
{

     return mAnchorMgr.getIncrementalBackupDir();

}

/*********************************************************
 * function description: smrRecoveryMgr::resetLogFiles
 * -  incompleted media recovery�� �Ǿ�����,
 * �� ������ ����������� ���� �ʰ�, ������ Ư����������
 * �Ǿ����� active  redo log������ reset����,
 * redo all, undo all�� skip�ϵ��� �Ѵ�.
 *********************************************************/
IDE_RC smrRecoveryMgr::resetLogFiles()
{
    UInt               j;
    UInt               sBeginLogFileNo;
    UInt               sArchMultiplexDirPathCnt;
    UInt               sLogMultiplexDirPathCnt;
    smLSN              sResetLSN;
    smLSN              sEndLSN;
    SChar              sMsgBuf[SM_MAX_FILE_NAME];
    SChar              sLogFileName[SM_MAX_FILE_NAME];
    const SChar**      sArchMultiplexDirPath;
    const SChar**      sLogMultiplexDirPath;
    smrLogFile*        sLogFile = NULL;

    sArchMultiplexDirPath    = smuProperty::getArchiveMultiplexDirPath();
    sArchMultiplexDirPathCnt = smuProperty::getArchiveMultiplexCount();

    sLogMultiplexDirPath     = smuProperty::getLogMultiplexDirPath();
    sLogMultiplexDirPathCnt  = smuProperty::getLogMultiplexCount();

    IDE_TEST_RAISE( getArchiveMode() != SMI_LOG_ARCHIVE, err_archivelog_mode );

    mAnchorMgr.getResetLogs( &sResetLSN );
    mAnchorMgr.getEndLSN( &sEndLSN );

    IDE_TEST_RAISE( ( sResetLSN.mFileNo == ID_UINT_MAX ) &&
                    ( sResetLSN.mOffset == ID_UINT_MAX ),
                    err_resetlog_no_needed );

    IDE_ASSERT( smrCompareLSN::isEQ( &(sResetLSN),
                                     &(sEndLSN) ) == ID_TRUE );

    IDE_CALLBACK_SEND_SYM("  [SM] Recovery Phase - 0 : Reset logfiles");

    // fix BUG-15840
    // �̵������� resetlogs �� �����Ҷ� archive log�� ���ؼ��� ����ؾ���
    //   Online �α����� �� Archive �α����ϵ� �Բ� ResetLogs�� �����Ѵ�.

    // [1] Delete Archive Logfiles
    idlOS::snprintf( sMsgBuf,
                     SM_MAX_FILE_NAME,
                     "\n     Archive Deleting logfile%"ID_UINT32_FMT,
                     sResetLSN.mFileNo );

    IDE_CALLBACK_SEND_SYM(sMsgBuf);

    // Delete archive logfiles
    IDE_TEST( deleteLogFiles( (SChar*)smuProperty::getArchiveDirPath(),
                              sResetLSN.mFileNo )
              != IDE_SUCCESS );

    // Delete multiplex archive logfiles
    for ( j = 0; j < sArchMultiplexDirPathCnt; j++ )
    {
        IDE_TEST( deleteLogFiles(  (SChar*)sArchMultiplexDirPath[j],
                                   sResetLSN.mFileNo )
                  != IDE_SUCCESS );
    }

    IDE_CALLBACK_SEND_SYM("\n     [ SUCCESS ]");

    // [2] Delete Online Logfiles
    if ( sResetLSN.mOffset == 0 )
    {
        // 3�ܰ迡�� partial clear�� �ʿ���� ������ �����Ѵ�.
        sBeginLogFileNo = sResetLSN.mFileNo;
    }
    else
    {
        // 3�ܰ迡�� partial clear�ϱ� ���� ���ܵд�.
        sBeginLogFileNo = sResetLSN.mFileNo + 1;
    }

    idlOS::snprintf(
            sMsgBuf,
            SM_MAX_FILE_NAME,
            "\n     Online  Deleting logfile%"ID_UINT32_FMT,
            sBeginLogFileNo );

    IDE_CALLBACK_SEND_SYM(sMsgBuf);

    IDE_TEST( deleteLogFiles( (SChar*)smuProperty::getLogDirPath(),
                              sBeginLogFileNo )
              != IDE_SUCCESS );

    // Delete multiplex logfiles
    for ( j = 0; j < sLogMultiplexDirPathCnt; j++ )
    {
        IDE_TEST( deleteLogFiles( (SChar*)sLogMultiplexDirPath[j],
                                  sBeginLogFileNo )
                  != IDE_SUCCESS );
    }

    IDE_CALLBACK_SEND_SYM("\n     [ SUCCESS ]");

    // [3] Clear Online Logfile
    idlOS::snprintf(sMsgBuf,
                    SM_MAX_FILE_NAME,
                    "\n     Reset log sequence number < %"ID_UINT32_FMT",%"ID_UINT32_FMT" >",
                    sResetLSN.mFileNo,
                    sResetLSN.mOffset);

    IDE_CALLBACK_SEND_MSG(sMsgBuf);

    if (sResetLSN.mOffset != 0)
    {
        idlOS::snprintf( sLogFileName,
                         SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT,
                         smuProperty::getLogDirPath(),
                         IDL_FILE_SEPARATOR,
                         SMR_LOG_FILE_NAME,
                         sResetLSN.mFileNo );
        IDE_TEST( idf::access(sLogFileName, F_OK) != 0);
        IDE_TEST( smrLogMgr::openLogFile( sResetLSN.mFileNo,
                                          ID_TRUE,  // Write Mode
                                          &sLogFile)
                 != IDE_SUCCESS );

        IDE_TEST( sLogFile == NULL);
        IDE_TEST( sLogFile->mFileNo != sResetLSN.mFileNo);

        // BUG-14771 Log File�� write mode�� open��
        //           Log Buffer Type�� memory�� ��쿡��
        //           memset �ϹǷ�, �α����� ���� �о�´�.
        //           sLogFile->syncToDisk ���� Direct I/O ������
        //           Disk�� �α׸� ������ ������ �α׸� �̸� �о�� ��
        if ( smuProperty::getLogBufferType( ) == SMU_LOG_BUFFER_TYPE_MEMORY )
        {
            IDE_TEST( sLogFile->readFromDisk(
                            0,
                            (SChar*)(sLogFile->mBase),
                            /* BUG-15532: ������ �÷������� DirectIO�� ����Ұ��
                             * ���� ���۽���IOũ�Ⱑ Sector Size�� Align�Ǿ�ߵ� */
                            idlOS::align(sResetLSN.mOffset,
                                         iduProperty::getDirectIOPageSize()))
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do ... */
        }

        sLogFile->clear(sResetLSN.mOffset);

        // Log Buffer�� Clear�߱� ������ Disk�� ����� ������ �ݿ���Ų��.
        IDE_TEST( sLogFile->syncToDisk( sResetLSN.mOffset,
                                        smuProperty::getLogFileSize() )
                 != IDE_SUCCESS );

        IDE_TEST( smrLogMultiplexThread::syncToDisk( 
                                                smrLogFileMgr::mSyncThread,
                                                sResetLSN.mOffset,         
                                                smuProperty::getLogFileSize(),
                                                sLogFile )
                  != IDE_SUCCESS );

        IDE_TEST( smrLogMgr::closeLogFile(sLogFile) != IDE_SUCCESS );
        sLogFile = NULL;

        IDE_CALLBACK_SEND_SYM("\n     [ SUCCESS ]");
    }
    else
    {
        /* nothing to do ... */
    }

    SM_LSN_MAX(sResetLSN);

    IDE_TEST( mAnchorMgr.updateResetLSN( &sResetLSN ) != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("[SUCCESS]");

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION(err_resetlog_no_needed);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidUseResetLog));
    }
    IDE_EXCEPTION_END;

    IDE_CALLBACK_SEND_MSG("[FAILURE]");

    IDE_PUSH();
    {
        if ( sLogFile != NULL )
        {
            IDE_ASSERT( smrLogMgr::closeLogFile(sLogFile) == IDE_SUCCESS );
            sLogFile = NULL;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
  �Էµ� Directory�� �Էµ� logfile ��ȣ���� �� ���� ��� �α�������
  �����Ѵ�.

  [ ���� ]

  {IN] aDirPath - �α������� �����ϴ� ���丮
  {IN] aBeginLogFileNo - ������ �α����� ���۹�ȣ
  ( ���� �α������� ��� ������ )

*/
IDE_RC smrRecoveryMgr::deleteLogFiles( SChar   * aDirPath,
                                       UInt      aBeginLogFileNo )
{
    UInt               sBeginLogFileNo;
    UInt               sCurLogFileNo;
    UInt               sDeleteCnt;
    SInt               sRc;
    DIR              * sDIR         = NULL;
    struct  dirent   * sDirEnt      = NULL;
    struct  dirent   * sResDirEnt;
    SChar              sLogFileName[SM_MAX_FILE_NAME];
    SChar              sMsgBuf[SM_MAX_FILE_NAME];

    IDE_DASSERT( aDirPath != NULL );

    sBeginLogFileNo = aBeginLogFileNo;
    sDeleteCnt = 1;

    idlOS::memset(sLogFileName, 0x00, SM_MAX_FILE_NAME);
    idlOS::memset(sMsgBuf, 0x00, SM_MAX_FILE_NAME);

    /* smrRecoveryMgr_deleteLogFiles_malloc_DirEnt.tc */
    /* smrRecoveryMgr_deleteLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::deleteLogFiles::malloc::DirEnt",
                         insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                       (void**)&sDirEnt,
                                       IDU_MEM_FORCE) != IDE_SUCCESS,
                    insufficient_memory );

    /* smrRecoveryMgr_deleteLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::deleteLogFiles::opendir", err_open_dir );
    sDIR = idf::opendir( aDirPath );
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);

    sResDirEnt = NULL;
    errno  = 0;

    /* smrRecoveryMgr_deleteLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::deleteLogFiles::readdir_r", err_read_dir );
    sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
    IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
    errno  = 0;

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sLogFileName, sResDirEnt->d_name);

        if (idlOS::strncmp(sLogFileName,
                           SMR_LOG_FILE_NAME,
                           idlOS::strlen(SMR_LOG_FILE_NAME)) == 0)
        {
            sCurLogFileNo = idlOS::strlen(SMR_LOG_FILE_NAME);
            sCurLogFileNo = idlOS::atoi(&sLogFileName[sCurLogFileNo]);

            if (sCurLogFileNo >= sBeginLogFileNo)
            {
                if (sCurLogFileNo > sBeginLogFileNo)
                {
                    (((sDeleteCnt) % 5) == 0 ) ?
                        IDE_CALLBACK_SEND_SYM("*") : IDE_CALLBACK_SEND_SYM(".");
                }
                sDeleteCnt++;

                idlOS::snprintf(sLogFileName,
                                SM_MAX_FILE_NAME,
                                "%s%c%s%"ID_UINT32_FMT,
                                aDirPath,
                                IDL_FILE_SEPARATOR,
                                SMR_LOG_FILE_NAME,
                                sCurLogFileNo);

                if ( idf::unlink(sLogFileName) == 0 )
                {
                    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                                 "[ Reset Log : Success ] Deleted ( %s )", sLogFileName );
                }
                else
                {
                    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                                 "[ Reset Log : Failure ] Cann't delete ( %s )", sLogFileName );
                }
            }
        }

        sResDirEnt = NULL;
        errno = 0;
        sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }

    idf::closedir(sDIR);
    sDIR = NULL;

    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );
    sDirEnt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, aDirPath ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sDIR != NULL )
        {
            idf::closedir( sDIR );
            sDIR = NULL;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sDirEnt != NULL )
        {
            IDE_ASSERT( iduMemMgr::free( sDirEnt ) == IDE_SUCCESS );
            sDirEnt = NULL;
        }
        else
        {
            /* Nothing to do */
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �߰��� ���ǵ� �α������� ������ �˻��Ѵ�.
 **********************************************************************/
IDE_RC smrRecoveryMgr::identifyLogFiles()
{
    UInt               sChecked;
    UInt               sState       = 0;
    SInt               sRc;
    DIR              * sDIR         = NULL;
    struct  dirent   * sDirEnt      = NULL;
    struct  dirent   * sResDirEnt;
    iduFile            sFile;
    SChar              sLogFileName[SM_MAX_FILE_NAME];
    UInt               sCurLogFileNo;
    UInt               sTotalLogFileCnt;
    UInt               sMinLogFileNo;
    UInt               sNoPos;
    UInt               sLstDeleteLogFileNo;
    UInt               sPrefixLen;
    ULong              sFileSize    = 0;
    idBool             sIsLogFile;
    idBool             sDeleteFlag;
    ULong              sLogFileSize = smuProperty::getLogFileSize();

    /* BUG-48409 prepare �Ϸ���� ���� logfile �� �����ϴ� ��� ����
     * prepare ���̴� �ӽ� �α� ���� ���� */
    idlOS::snprintf( sLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%",
                     smuProperty::getLogDirPath(),
                     IDL_FILE_SEPARATOR,
                     SMR_TEMP_LOG_FILE_NAME );
      
    if ( idf::access(sLogFileName, F_OK) == 0 )
    {
        (void)idf::unlink(sLogFileName);
    }

    /* smrRecoveryMgr_identifyLogFiles_malloc_DirEnt.tc */
    /* smrRecoveryMgr_identifyLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::identifyLogFiles::malloc::DirEnt",
                         insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                       (void**)&sDirEnt,
                                       IDU_MEM_FORCE ) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    ideLog::log(IDE_SERVER_0,"  \tchecking logfile(s)\n");

    sLstDeleteLogFileNo = mAnchorMgr.getLstDeleteLogFileNo();

    /* smrRecoveryMgr_identifyLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::identifyLogFiles::opendir", err_open_dir );
    sDIR = idf::opendir( smuProperty::getLogDirPath() );
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);

    sResDirEnt = NULL;
    errno = 0;

    /* smrRecoveryMgr_identifyLogFiles.tc */
    IDU_FIT_POINT_RAISE( "smrRecoveryMgr::identifyLogFiles::readdir_r", err_read_dir );
    sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
    IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
    errno = 0;

    sMinLogFileNo = ID_UINT_MAX;
    sTotalLogFileCnt = 0;
    sPrefixLen = idlOS::strlen(SMR_LOG_FILE_NAME);

    // BUG-20229
    sDeleteFlag = ID_FALSE;

    while ( sResDirEnt != NULL )
    {
        sNoPos = sPrefixLen;
        idlOS::strcpy(sLogFileName, sResDirEnt->d_name);

        if ( idlOS::strncmp(sLogFileName,
                            SMR_LOG_FILE_NAME,
                            sPrefixLen) == 0 )
        {
            sCurLogFileNo = idlOS::atoi(&sLogFileName[sNoPos]);

            sIsLogFile = ID_TRUE;

            while ( sLogFileName[sNoPos] != '\0' )
            {
                if ( smuUtility::isDigit(sLogFileName[sNoPos])
                     == ID_FALSE )
                {
                    sIsLogFile = ID_FALSE;
                    break;
                }
                else
                {
                    /* nothing to do ... */
                }
                sNoPos++;
            }

            if ( sIsLogFile == ID_TRUE )
            {
                // it is logfile.
                if ( sLstDeleteLogFileNo <= sCurLogFileNo )
                {
                    if (sCurLogFileNo < sMinLogFileNo)
                    {
                        sMinLogFileNo = sCurLogFileNo;
                    }
                    else
                    {
                        /* nothing to do ... */
                    }
                    sTotalLogFileCnt++;
                }
                else
                {
                    /* nothing to do ... */
                }
            }
            else
            {
                /* nothing to do ... */
            }
        }
        else
        {
            /* nothing to do ... */
        }

        sResDirEnt = NULL;
        errno = 0;
        sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }

    idf::closedir(sDIR);
    sDIR = NULL;

    for (sCurLogFileNo = sMinLogFileNo, sChecked = 0;
         sChecked < sTotalLogFileCnt;
         sCurLogFileNo++, sChecked++)
    {
        idlOS::snprintf( sLogFileName,
                         SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT,
                         smuProperty::getLogDirPath(),
                         IDL_FILE_SEPARATOR,
                         SMR_LOG_FILE_NAME,
                         sCurLogFileNo );

        /* PROJ-2162 RestartRiskReduction
         * LogFile�� ���������� ������, �ٷ� ������ �����ϴ� ��� �̸�
         * üũ�صд�.
         * ����  �� ������ LogFile�� �������� �ȵȴ�. */
        if ( idf::access(sLogFileName, F_OK) != 0 )
        {
            mLogFileContinuity = ID_FALSE;
            idlOS::strncpy( mLostLogFile,
                            sLogFileName,
                            SM_MAX_FILE_NAME );
            continue;
        }
        else
        {
            /* nothing to do ... */
        }

        IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                   1, /* Max Open FD Count */
                                   IDU_FIO_STAT_OFF,
                                   IDV_WAIT_INDEX_NULL )
                 != IDE_SUCCESS );
        sState = 2;
        IDE_TEST( sFile.setFileName(sLogFileName) != IDE_SUCCESS );
        IDE_TEST( sFile.open() != IDE_SUCCESS );
        sState = 3;

        IDE_TEST( sFile.getFileSize( &sFileSize ) != IDE_SUCCESS );

        sState = 2;
        IDE_TEST( sFile.close() != IDE_SUCCESS );

        sState = 1;
        IDE_TEST( sFile.destroy() != IDE_SUCCESS );
        
        // BUG-20229
        // sDeleteFlag�� ID_TRUE��� ����
        // ���� �α������߿� size�� 0�� �α������� �־��ٴ� ���̴�.
        // size�� 0�� �α������� �߰��ߴٸ�
        // �� ������ �α������� ��� �����.
        if ( sDeleteFlag == ID_TRUE )
        {
            ideLog::log( IDE_ERR_0,
                         "Remove logfile : %s (%"ID_UINT32_FMT")\n" ,
                         sLogFileName,
                         sFileSize );

            (void)idf::unlink(sLogFileName);
            continue;
        }
        else
        {
            /* nothing to do ... */
        }

        IDE_ASSERT( sFileSize <= sLogFileSize );

        // BUG-20229
        // size�� 0�� �α������� �߰��ߴٸ�
        // �� ������ �α������� ��� �����.
        // prepare logfile�ÿ��� sync�� ���� �ʱ� ������,
        // ���۽��� ���� ��ַ� ���Ͽ� �ý����� reboot�Ǵ� ���,
        // prepare ���̴� logfile�� size�� 0 �� �� �ִ�.
        if ( sFileSize == 0 )
        {
            if ( smuProperty::getZeroSizeLogFileAutoDelete() == 0 )
            {
                /* BUG-42930 OS�� �߸��� size�� ��ȯ�Ҽ� �ֽ��ϴ�.
                 * �α������� ������ ����� ���
                 * DBA�� ó���Ҽ� �ֵ��� Error �޽����� ����ϵ��� �Ͽ����ϴ�.
                 * Server Kill �׽�Ʈ ��� Log File Size�� 0�� �ɼ� �ֽ��ϴ�.
                 * Test�� Property�� �ξ� natc ���߿��� �ڵ����� �ɼ� �ְ� �Ͽ����ϴ�. */
                ideLog::log( IDE_ERR_0, "Error zero size logfile : %s\n" , sLogFileName );
                IDE_RAISE( LOG_FILE_SIZE_ZERO );
            }
            else
            {
                ideLog::log( IDE_ERR_0, "Remove zero size logfile : %s\n" , sLogFileName );
                (void)idf::unlink(sLogFileName);
                sDeleteFlag = ID_TRUE;
            }
        }
        else
        {
            if ( sFileSize != sLogFileSize )
            {
                /* BUG-48409 prepare �Ϸ���� ���� logfile �� �����ϴ� ��� ����
                 * use temp logfile������ prepare �߿� size�� ���� logfile �� ���� �ʴ´�. */
                if ( smuProperty::getUseTempForPrepareLogFile() == ID_TRUE )
                {
                    ideLog::log( IDE_ERR_0,
                                 "Invalid logfile size : %s (%"ID_UINT32_FMT")\n" ,
                                 sLogFileName,
                                 sFileSize );
                    IDE_RAISE( LOG_FILE_SIZE_ZERO );
                }
                else
                {
                    ideLog::log( IDE_ERR_0,
                                 "Resize logfile : %s (%"ID_UINT32_FMT" -> %"ID_UINT32_FMT")\n" ,
                                 sLogFileName,
                                 sFileSize,
                                 sLogFileSize );
                    // BUG-20229
                    // �α������� ũ�⸦ �������� �α����� ũ��� ���δ�.
                    IDE_TEST( resizeLogFile(sLogFileName,
                                            sLogFileSize)
                              != IDE_SUCCESS );
                }
            }
            else
            {
                /* nothing to do ... */
            }
        }
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );
    sDirEnt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, smuProperty::getLogDirPath() ) );
    }
    IDE_EXCEPTION( LOG_FILE_SIZE_ZERO );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_LogFileSizeIsZero, sLogFileName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch(sState)
    {
        case 3:
            IDE_ASSERT(sFile.close() == IDE_SUCCESS );

        case 2:
            IDE_ASSERT(sFile.destroy() == IDE_SUCCESS );

        case 1:
            IDE_ASSERT(iduMemMgr::free(sDirEnt) == IDE_SUCCESS );
            sDirEnt = NULL;
            break;

        default:
            break;
    }

    if ( sDIR != NULL )
    {
        idf::closedir( sDIR );
        sDIR = NULL;
    }
    else
    {
        /* Nothing to do */
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*
  incomplete �̵����� �����Ͽ� reset logs�� ������ ���
  startup meta resetlogs ������ �˻��Ѵ�.
*/
IDE_RC smrRecoveryMgr::checkResetLogLSN( UInt aActionFlag )
{
    smLSN               sResetLSN;

    mAnchorMgr.getResetLogs( &sResetLSN );

    if ( (sResetLSN.mFileNo != ID_UINT_MAX) &&
         (sResetLSN.mOffset != ID_UINT_MAX)) 
    {
        IDE_TEST_RAISE( (aActionFlag & SMI_STARTUP_ACTION_MASK) !=
                        SMI_STARTUP_RESETLOGS,
                        err_need_resetlogs);
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_need_resetlogs);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NeedResetLogs));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   PRJ-1149 ��� ���̺����̽��� �̵�� ���� �˻�

   [IN] aActionFlag - �ٴܰ� ������������������ �÷���
*/
IDE_RC smrRecoveryMgr::identifyDatabase( UInt  aActionFlag )
{
    ideLog::log( IDE_SERVER_0,
                 "  \tchecking file(s) of all memory tablespace\n" );

    // [1] ��� �޸� ���̺����̽��� Stable�� DBFile���� �ݵ��
    // �־�� �ϰ� (loganchor�� ����Ǿ��ִ���) Unstable DBFiles����
    // �����Ѵٸ� �ּ��� ��ȿ�� �����̾�� �Ѵ�.
    // �̵�� ���� �ʿ� ���θ� üũ�Ѵ�.
    // ���������ÿ��� ID_FALSE�� �ѱ��.
    IDE_TEST( smmTBSMediaRecovery::identifyDBFilesOfAllTBS( ID_FALSE )
              != IDE_SUCCESS );

    ideLog::log( IDE_SERVER_0,
                 "  \tchecking file(s) of all disk tablespace\n" );

    // [2] ��� ��ũ ���̺����̽��� DBFile����
    // �̵�� ���� �ʿ� ���θ� üũ�Ѵ�.
    // ���������ÿ��� ID_FALSE�� �ѱ��.
    IDE_TEST( sddDiskMgr::identifyDBFilesOfAllTBS( NULL, /* idvSQL* */
                                                   ID_FALSE )
              != IDE_SUCCESS );

    // [3] �ҿ��������� �����Ͽ��ٸ� alter databae mydb meta resetlogs;
    // �� �����Ͽ����� �˻��Ѵ�.
    IDE_TEST( checkResetLogLSN( aActionFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ��� ���̺����̽��� �˻��Ͽ� ������� ���ϸ���Ʈ�� �����Ѵ�.

  [IN]  aRecoveryType         - �̵�� ���� Ÿ�� (RESTART RECOVERY ����)
  [OUT] aFailureMediaType     - �̵�� ���� Ÿ��
  [OUT] aFromDiskRedoLSN      - �̵�� ������ ���� Disk RedoLSN
  [OUT] aToDiskRedoLSN        - �̵�� ������ �Ϸ� Disk RedoLSN
  [OUT] aFromMemRedoLSN    - �̵�� ������ ���� Memory RedoLSN 
  [OUT] aToMemRedoLSN      - �̵�� ������ �Ϸ� Memory RedoLSN 
*/
IDE_RC smrRecoveryMgr::makeMediaRecoveryDBFList4AllTBS(
    smiRecoverType    aRecoveryType,
    UInt            * aFailureMediaType,
    smLSN           * aFromDiskRedoLSN,
    smLSN           * aToDiskRedoLSN,
    smLSN           * aFromMemRedoLSN,
    smLSN           * aToMemRedoLSN )
{
    UInt                sFailureChkptImgCount;
    UInt                sFailureDBFCount;
    sctTableSpaceNode*  sCurrSpaceNode;
    sctTableSpaceNode*  sNextSpaceNode;
    SChar               sMsgBuf[ SM_MAX_FILE_NAME ];
    smLSN               sFromDiskRedoLSN;
    smLSN               sToDiskRedoLSN;
    smLSN               sFromMemRedoLSN;
    smLSN               sToMemRedoLSN;

    IDE_DASSERT( aRecoveryType         != SMI_RECOVER_RESTART );
    IDE_DASSERT( aFailureMediaType     != NULL );
    IDE_DASSERT( aToDiskRedoLSN        != NULL );
    IDE_DASSERT( aFromDiskRedoLSN      != NULL );
    IDE_DASSERT( aToMemRedoLSN         != NULL );
    IDE_DASSERT( aFromMemRedoLSN       != NULL );

    SM_LSN_MAX ( sFromDiskRedoLSN );
    SM_LSN_INIT( sToDiskRedoLSN );

    *aFailureMediaType    = 0;

    // BUG-27616 Klocwork SM (7)

    SM_LSN_MAX ( sFromMemRedoLSN );
    SM_LSN_INIT( sToMemRedoLSN );

    sFailureChkptImgCount = 0;
    sFailureDBFCount      = 0;

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        IDE_ASSERT( (sCurrSpaceNode->mState & SMI_TBS_DROPPED)
                    != SMI_TBS_DROPPED );

        if ( sctTableSpaceMgr::isTempTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {
            // �ӽ� ���̺����̽��� ��� üũ���� �ʴ´�.
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        if ( sctTableSpaceMgr::hasState( sCurrSpaceNode,
                                         SCT_SS_UNABLE_MEDIA_RECOVERY )
             == ID_TRUE )
        {
            // �̵����� �ʿ���� ���̺����̽�
            // DROPPED �̰ų� DISCARD �� ���
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }
        switch( sctTableSpaceMgr::getTBSLocation( sCurrSpaceNode ) )
        {
            case SMI_TBS_MEMORY:
                // �޸����̺����̽��� ���
                IDE_TEST( smmTBSMediaRecovery::makeMediaRecoveryDBFList(
                              sCurrSpaceNode,
                              aRecoveryType,
                              &sFailureChkptImgCount,
                              &sFromMemRedoLSN,
                              &sToMemRedoLSN )
                          != IDE_SUCCESS );
                break;
            case SMI_TBS_DISK:
                IDE_TEST( sddDiskMgr::makeMediaRecoveryDBFList( NULL, /* idvSQL* */
                                                                sCurrSpaceNode,
                                                                aRecoveryType,
                                                                &sFailureDBFCount,
                                                                &sFromDiskRedoLSN,
                                                                &sToDiskRedoLSN )
                          != IDE_SUCCESS );
                break;
            case SMI_TBS_VOLATILE:
                // Nothing to do...
                // volatile tablespace�� ���ؼ��� �ƹ� �۾����� �ʴ´�.

                break;
            default:
                IDE_ASSERT(0);
                break;
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    if ( (sFailureChkptImgCount + sFailureDBFCount) > 0 )
    {
        /*
           ������� ������ �����ϸ� �м��� ����౸����
           ������Ͽ� �̵����� �Ϸ��Ѵ�.
        */

        // ������ �ʱ�ȭ�Ѵ�.
        SM_GET_LSN( *aFromDiskRedoLSN, sFromDiskRedoLSN );
        SM_GET_LSN( *aToDiskRedoLSN, sToDiskRedoLSN );

        SM_GET_LSN( *aFromMemRedoLSN, sFromMemRedoLSN );
        SM_GET_LSN( *aToMemRedoLSN, sToMemRedoLSN );

        if ( sFailureChkptImgCount > 0)
        {
            *aFailureMediaType |= SMR_FAILURE_MEDIA_MRDB;

            idlOS::snprintf(sMsgBuf,
                            SM_MAX_FILE_NAME,
                            "             Memory Redo LSN <%"ID_UINT32_FMT",%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT",%"ID_UINT32_FMT">",
                            aFromMemRedoLSN->mFileNo,
                            aFromMemRedoLSN->mOffset,
                            aToMemRedoLSN  ->mFileNo,
                            aToMemRedoLSN  ->mOffset );

            IDE_CALLBACK_SEND_MSG( sMsgBuf );
        }

        if ( sFailureDBFCount > 0 )
        {
            *aFailureMediaType |= SMR_FAILURE_MEDIA_DRDB;

            idlOS::snprintf(sMsgBuf,
                            SM_MAX_FILE_NAME,
                            "             Disk Redo LSN <%"ID_UINT32_FMT",%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT",%"ID_UINT32_FMT">",
                            aFromDiskRedoLSN->mFileNo,
                            aFromDiskRedoLSN->mOffset,
                            aToDiskRedoLSN->mFileNo,
                            aToDiskRedoLSN->mOffset );

            IDE_CALLBACK_SEND_MSG( sMsgBuf );
        }

        idlOS::snprintf( sMsgBuf,
                         SM_MAX_FILE_NAME,
                         "  \t     Total %"ID_UINT32_FMT" database file(s)\n  \t     Memory %"ID_UINT32_FMT" Checkpoint Image(s)\n  \t     Disk %"ID_UINT32_FMT" Database File(s)",
                         sFailureChkptImgCount + sFailureDBFCount,
                         sFailureChkptImgCount,
                         sFailureDBFCount );

        IDE_CALLBACK_SEND_MSG(sMsgBuf);
    }
    else
    {
        // �ҿ��� ������ ��� ����Ÿ������ ��������̴�.
        // ���� ����ڰ� �ҿ��� ������ �����ϰ� �Ǹ�, ��� ����Ÿ������
        // ������ ������ ��������� �Ǳ� ������ else�� ���ü� ����.
        IDE_ASSERT( !((aRecoveryType == SMI_RECOVER_UNTILTIME  ) ||
                      (aRecoveryType == SMI_RECOVER_UNTILCANCEL)) );

        // �̵�� ������ ���� ��Ȳ���� ���������� �����Ϸ��� �Ҷ�
        // �̰����� ���´�.

        // �̵������� ����Ÿ������ ���� ���������� ������ ��� ERROR ó��
        // �ҿ��� ���� �Ǵ� RESTART RECOVERY�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( aRecoveryType == SMI_RECOVER_COMPLETE,
                        err_media_recovery_type );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_media_recovery_type );
    {
        IDE_SET( ideSetErrorCode(
                     smERR_ABORT_ERROR_MEDIA_RECOVERY_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : DBFileHdr���� ���� ū MustRedoToLSN�� �����´�.
 *  DBFile �� Backup�Ǿ��� ������ �ƴ� ��� InitLSN�� �Ѱ��ش�.
 *
 * aMustRedoToLSN - [OUT] Disk DBFile �� ���� ū MustRedoToLSN
 * aDBFileName    - [OUT] �ش� Must Redo to LSN�� ���� DBFile
 **********************************************************************/
IDE_RC smrRecoveryMgr::getMaxMustRedoToLSN( idvSQL   * aStatistics,
                                            smLSN    * aMustRedoToLSN,
                                            SChar   ** aDBFileName )
{
    sctTableSpaceNode*  sCurrSpaceNode;
    sctTableSpaceNode*  sNextSpaceNode;
    smLSN               sMaxMustRedoToLSN;
    smLSN               sMustRedoToLSN;
    SChar*              sDBFileName;
    SChar*              sMaxRedoLSNFileName = NULL;

    IDE_DASSERT( aMustRedoToLSN != NULL );
    IDE_DASSERT( aDBFileName    != NULL );

    *aDBFileName = NULL;
    SM_LSN_INIT( sMaxMustRedoToLSN );

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        if ( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode ) != ID_TRUE )
        {
            // ��ũ���̺����̽��� ��츸 Ȯ���Ѵ�.
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        if ( sctTableSpaceMgr::isTempTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {
            // �ӽ� ���̺����̽��� ��� üũ���� �ʴ´�.
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        if ( sctTableSpaceMgr::hasState( sCurrSpaceNode,
                                         SCT_SS_UNABLE_MEDIA_RECOVERY )
             == ID_TRUE )
        {
            // �̵����� �ʿ���� ���̺����̽�
            // DROPPED �̰ų� DISCARD �� ���
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        IDE_TEST( sddDiskMgr::getMustRedoToLSN(
                                  aStatistics,
                                  sCurrSpaceNode,
                                  &sMustRedoToLSN,
                                  &sDBFileName )
                  != IDE_SUCCESS );

        if ( smrCompareLSN::isGT( &sMustRedoToLSN,
                                 &sMaxMustRedoToLSN ) == ID_TRUE )
        {
            sMaxMustRedoToLSN   = sMustRedoToLSN;
            sMaxRedoLSNFileName = sDBFileName;
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    *aMustRedoToLSN = sMaxMustRedoToLSN;
    *aDBFileName    = sMaxRedoLSNFileName;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   �̵�� ������ �ǵ��� Log�� Scan �� ������ ��´�.

   [IN]  aFromDiskRedoLSN   - Disk �������� �ּ� LSN
   [IN]  aToDiskRedoLSN     - Disk �������� �ִ� LSN
   [IN]  aFromMemRedoLSN - Memory �������� �ּ� LSN
   [IN]  aToMemRedoLSN   - Memory �������� �ִ� LSN
   [OUT] aMinFromRedoLSN - �̵�� ������ scan�� �α��� �ּ� LSN
*/
void smrRecoveryMgr::getRedoLSN4SCAN( smLSN * aFromDiskRedoLSN,
                                      smLSN * aToDiskRedoLSN,
                                      smLSN * aFromMemRedoLSN,
                                      smLSN * aToMemRedoLSN,
                                      smLSN * aMinFromRedoLSN )
{
    smLSN       sInitLSN;
    smLSN       sRestartRedoLSN;

    IDE_ASSERT( aFromDiskRedoLSN   != NULL );
    IDE_ASSERT( aToDiskRedoLSN     != NULL );
    IDE_ASSERT( aFromMemRedoLSN != NULL );
    IDE_ASSERT( aToMemRedoLSN   != NULL );
    IDE_ASSERT( aMinFromRedoLSN != NULL );

    // Disk DBF���� Media Recovery ����Ǿ�� �ϴ� ���,
    // Memory�� From, To RedoLSN�� ���� �������� ���� �����̴�.
    //
    // ����������, Memory Checkpoint Image����
    // Media Recovery�� ����Ǿ�� �ϴ� ��� Disk�� From, To RedoLSN��
    // �������� ���� �����̴�.
    //
    // => ��� LFG�� ���� From, To RedoLSN�� �������� ���� ���
    //    Log Anchor���� Redo���� �������� �����Ѵ�.
    //
    mAnchorMgr.getEndLSN( &sRestartRedoLSN );

    if ( SM_IS_LSN_MAX( *aFromMemRedoLSN ) )
    {
        SM_GET_LSN( *aFromMemRedoLSN,
                    sRestartRedoLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    if ( SM_IS_LSN_INIT( *aToMemRedoLSN ) )
    {
        SM_GET_LSN( *aToMemRedoLSN,
                    sRestartRedoLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    if ( SM_IS_LSN_MAX( *aFromDiskRedoLSN ) )
    {
        SM_GET_LSN( *aFromDiskRedoLSN,
                    sRestartRedoLSN );
    }
    /*
      getRedoLogLSNRange ���ϱ�

      *[IN ]From Disk Redo LSN
      LFG 0  10

      *[IN ]Array of From Memory Redo LSN
      LFG 0         20
      LFG 1     15

      *[OUT]Array of Mininum From Redo LSN
      LFG 0  10
      LFG 1     15

      ���� �׸��� ���� DISK LFG �� ���ؼ��� �񱳸� �ٽ��Ͽ�
      Log Scan ������ �����Ѵ�.
      �ϴ� Memory �� From Redo LSN�� ����Ͽ��� Disk �� FromRedoLSN��
      ���Ͽ� LFG 0���� ���ؼ��� �ٽ� ���Ѵ�.

    */
    SM_GET_LSN( *aMinFromRedoLSN, *aFromMemRedoLSN );

    // ��ũ LFG�� ���� ������
    if ( smrCompareLSN::isGT( aFromMemRedoLSN, aFromDiskRedoLSN ) == ID_TRUE )
    {
        // LFG 0 > Disk Redo LSN
        // set minimum Redo LSN
        SM_GET_LSN( *aMinFromRedoLSN,
                    *aFromDiskRedoLSN );
    }

    SM_LSN_INIT( sInitLSN );

    IDE_ASSERT( !( (smrCompareLSN::isEQ( aFromMemRedoLSN,
                                        &sInitLSN ) == ID_TRUE) &&
                   (smrCompareLSN::isEQ( aToMemRedoLSN,
                                        &sInitLSN) == ID_TRUE)) );

    IDE_ASSERT( smrCompareLSN::isEQ( aMinFromRedoLSN,
                                     &sInitLSN )
                == ID_FALSE );

    return;
}


/*
   PRJ-1149 SM - Backup
   PRJ-1548 User Memory Tablespace ���䵵��

   ����Ÿ���̽��� �̵�� ���� ����

   - ���� �������� media recovery �Ҷ��� aUntilTime�� ULong max�� �Ѿ�´�.

   - ������ Ư���������� DB���� ��������
   date�� idlOS::time(0)���� ��ȯ�� ���� �Ѿ�´�.
   alter database recover database until time '2005-01-29:17:55:00'

   �����ؿ� ���� ����Ÿ������ OldestLSN����
   �ֱ� ����Ÿ������ OldestLSN(by anchor)���� ����Ÿ���Ͽ�
   ���� Redo �α׵��� ���������� �ݿ��ϰ�, �α׾�Ŀ�� ����Ÿ
   ������ ������������� ����ȭ��Ų��.
   �ԷµǴ� ����Ÿ������ �ټ����� �����ϹǷ� ���� ����Ÿ������
   redo ������ �ľ��ϰ�,  online �α׿� archive �α��� ������ ����Ͽ�
   redo�� �����Ѵ�.

   [ �˰��� ]
   - ���������ڸ� �ʱ�ȭ�Ѵ�.
   - �Էµ� ������ �����ϴ��� �˻��Ѵ�.
   - ������ ����(��)�� ����� �������� RecvFileHash�� �߰��Ѵ�.
   - ���������ڸ� �����Ѵ�.
   - �α����ϵ��� ��� �ݴ´�.

   [IN] aRecoveryType - �������� �Ǵ� �ҿ������� Ÿ��
   [IN] aUntilTIME    - Ÿ�Ӻ��̽� �ҿ��� �����ÿ� �ð�����

*/
IDE_RC smrRecoveryMgr::recoverDB( idvSQL*           aStatistics,
                                  smiRecoverType    aRecoveryType,
                                  UInt              aUntilTIME )
{

    UInt                sDiskState;
    UInt                sMemState;
    UInt                sCommonState;
    SChar               sMsgBuf[ SM_MAX_FILE_NAME ];
    time_t              sUntilTIME;
    struct tm           sUntil;
    smLSN               sCurRedoLSN;
    UInt                sFailureMediaType;

    smLSN               sMinFromRedoLSN;
    smLSN               sMaxToRedoLSN;

    smLSN               sToDiskRedoLSN;
    smLSN               sFromDiskRedoLSN;
    smLSN               sFromMemRedoLSN;
    smLSN               sToMemRedoLSN;

    smLSN               sResetLSN;
    smLSN              *sMemResetLSNPtr  = NULL;
    smLSN              *sDiskResetLSNPtr = NULL;

    IDE_DASSERT( aRecoveryType != SMI_RECOVER_RESTART );

    /* BUG-18428: Media Recovery�߿� Index�� ���� Undo������ �����ϸ� �ȵ˴ϴ�.
     *
     * Media Recovery�� Restart Recovery������ ����ϱ⶧����
     * Redo�ÿ� Temporal ������ ���� ������ ���ƾ� �Ѵ�. */
    mRestart = ID_TRUE;
    mMediaRecoveryPhase = ID_TRUE; /* �̵�� ���� �����߿��� TRUE */

    // fix BUG-17158
    // offline DBF�� write�� �����ϵ��� flag �� �����Ѵ�.
    sddDiskMgr::setEnableWriteToOfflineDBF( ID_TRUE );

    sCommonState = 0;
    sDiskState   = 0;
    sMemState    = 0;

    sFailureMediaType = SMR_FAILURE_MEDIA_NONE;
    idlOS::memset( sMsgBuf, 0x00, SM_MAX_FILE_NAME );

    // �̵����� ��ī�̺�α� ��忡���� �����ȴ�.
    IDE_TEST_RAISE( getArchiveMode() != SMI_LOG_ARCHIVE,
                    err_archivelog_mode );

    // BUG-29633 Recovery�� ��� Transaction�� End�������� ������ �ʿ��մϴ�.
    // Recovery�ÿ� �ٸ������� ���Ǵ� Active Transaction�� �����ؼ��� �ȵȴ�.
    IDE_TEST_RAISE( smLayerCallback::existActiveTrans() == ID_TRUE,
                    err_exist_active_trans);

    // Secondary  Buffer �� ����ϸ� �ȵȴ�. 
    IDE_TEST_RAISE( sdsBufferMgr::isServiceable() == ID_TRUE,
                    err_secondary_buffer_service );

    // �ҿ��� ������ �̹� �Ϸ�Ǿ��µ� �ٽ� �̵�� ������ �����ϴ�
    // ��츦 Ȯ���Ͽ� ������ ��ȯ�Ѵ�.
    IDE_TEST( checkResetLogLSN( SMI_STARTUP_NOACTION )
              != IDE_SUCCESS );

    // [DISK-1] ��ũ redo ������ �ʱ�ȭ
    IDE_TEST( smLayerCallback::initializeRedoMgr(
                  sdbBufferMgr::getPageCount(),
                  aRecoveryType )
              != IDE_SUCCESS );
    sDiskState = 1;

    IDE_CALLBACK_SEND_MSG("  [ RECMGR ] Checking database consistency..");

    switch ( aRecoveryType )
    {
        case SMI_RECOVER_COMPLETE:
            {
                // ���������� ��
                // �α׵��丮�� �α����ϵ��� ��ȿ���� �˻��Ѵ�.
                IDE_TEST( identifyLogFiles() != IDE_SUCCESS );
                // ���������ϰ�� �α����ϵ��� ��ȿ������ �����ϸ� restart recovery ���� ������ ����ϰų� 
                // �ҿ��� ������ ���� �Ѵ�. 
                break;
            }
        case SMI_RECOVER_UNTILTIME:
            {
                // Ÿ�Ӻ��̽� �ҿ��� ������
                // �α׵��丮�� �α����ϵ��� ��ȿ���� �˻��Ѵ�.
                IDE_TEST( identifyLogFiles() != IDE_SUCCESS );

                // �αװ� ���������� ������� UNTIL TIME �� �ǵ��Ѵ�� �������� �������ִ�. 
                // recovery �ؼ� �α� ������ ���� ������ �߻���Ų��. 
                IDE_TEST_RAISE( mLogFileContinuity == ID_FALSE, err_log_consistency );
                break;
            }
        case SMI_RECOVER_UNTILCANCEL:
            {
                // �α׺��̽� �ҿ��� �����ÿ��� �α������� ����������
                // �������� ���� �� �ֱ� ������ �����ϴ� ���� ���ʿ��ϴ�.
                break;
            }
        default :
            {
                IDE_ASSERT( 0 );
                break;
            }
    }

    ideLog::log( IDE_SERVER_0,
                 "  \tchecking inconsistency database file(s) count..");

    /*
       [1] ��� ���̺����̽��� ������ ��� ���ϵ��� ���Ѵ�.
       ��ũ ����Ÿ������ ������������ 2���ؽ���
       �����Ѵ�.
    */
    IDE_TEST( makeMediaRecoveryDBFList4AllTBS( aRecoveryType,
                                               &sFailureMediaType,
                                               &sFromDiskRedoLSN,
                                               &sToDiskRedoLSN,
                                               &sFromMemRedoLSN,
                                               &sToMemRedoLSN )
              != IDE_SUCCESS );

    // MEDIA_NONE �� ���� makeMediaRecoveryDBFList4AllTBS����
    // exception ó���ȴ�.
    IDE_ASSERT( sFailureMediaType != SMR_FAILURE_MEDIA_NONE );

    /*
       [2] �޸�/��ũ ��ü�� ������ �� �ִ� �������� LSN�� �����Ѵ�.
    */

    // [ �߿� ]
    // From Redo LSN �����ϱ�
    // �������� ���� Redo LSN�� ��������� ����Ǿ� �־�
    // �̵�� ������ From Redo LSN�� �������� �־�� �Ѵ�.
    // �ֳ��ϸ�, ������ �߰� LSN�� �����Ǹ� ����Ÿ���̽�
    // ������ ����� ���� ���� �� �ִ�.

    // get minimum RedoLSN
    getRedoLSN4SCAN( &sFromDiskRedoLSN,
                     &sToDiskRedoLSN,
                     &sFromMemRedoLSN,
                     &sToMemRedoLSN,
                     &sMinFromRedoLSN );

    if ( (sFailureMediaType & SMR_FAILURE_MEDIA_MRDB)
         == SMR_FAILURE_MEDIA_MRDB )
    {
        // [MEMORY-1]
        // �޸� ���̺����̽��� ������ �����ϱ���
        // ���� ��� ���̺����̽��� Prepare/Restore�� �����Ѵ�.
        IDE_TEST( initMediaRecovery4MemTBS() != IDE_SUCCESS );
        sMemState = 1;
    }

    /*
       [3] �޸�/��ũ �̵�� ���� ����
    */
    IDE_CALLBACK_SEND_MSG(
        "  [ RECMGR ] Restoring database consistency");

    /*
       !! media recovery ���Ŀ��� server restart recovery�� �����Ͽ�
       ������ �����Ǿ�߸� �Ѵ�.
       ���¸� �����Ѵ�.
    */
    IDE_TEST( mAnchorMgr.updateSVRStateAndFlush( SMR_SERVER_STARTED )
              != IDE_SUCCESS );

    idlOS::snprintf(sMsgBuf, SM_MAX_FILE_NAME,
                    "             Suggestion Range Of LSN : ");

    IDE_CALLBACK_SEND_MSG( sMsgBuf );

    idlOS::snprintf(sMsgBuf, SM_MAX_FILE_NAME,
                    "              From LSN <%"ID_UINT32_FMT",%"ID_UINT32_FMT"> ~ ",
                    sMinFromRedoLSN.mFileNo,
                    sMinFromRedoLSN.mOffset );

    IDE_CALLBACK_SEND_MSG( sMsgBuf );

    switch ( aRecoveryType )
    {
        case SMI_RECOVER_COMPLETE:
            {
                SM_GET_LSN( sMaxToRedoLSN, sToMemRedoLSN );

                if ( smrCompareLSN::isLT(
                         &sToMemRedoLSN,
                         &sToDiskRedoLSN ) == ID_TRUE )
                {
                    // Disk To Redo LSN�� �� ũ�ٸ�
                    SM_GET_LSN( sMaxToRedoLSN, sToDiskRedoLSN );
                }
                else
                {
                    /* nothing to do ... */
                }

                idlOS::snprintf(sMsgBuf,
                                SM_MAX_FILE_NAME,
                                "             To   LSN <%"ID_UINT32_FMT",%"ID_UINT32_FMT">",
                                sMaxToRedoLSN.mFileNo,
                                sMaxToRedoLSN.mOffset);

                IDE_CALLBACK_SEND_MSG( sMsgBuf );
                break;
            }
        case SMI_RECOVER_UNTILCANCEL:
            {
                idlOS::snprintf(sMsgBuf,
                                SM_MAX_FILE_NAME,
                                "                                       Until CANCEL" );

                IDE_CALLBACK_SEND_MSG( sMsgBuf );
                break;
            }
        case SMI_RECOVER_UNTILTIME:
            {
                sUntilTIME = (time_t)aUntilTIME;

                idlOS::localtime_r((time_t*)&sUntilTIME, &sUntil);

                idlOS::snprintf(sMsgBuf,
                                SM_MAX_FILE_NAME,
                                "                        Until Time "
                                "[%04"ID_UINT32_FMT
                                "/%02"ID_UINT32_FMT
                                "/%02"ID_UINT32_FMT
                                " %02"ID_UINT32_FMT
                                ":%02"ID_UINT32_FMT
                                ":%02"ID_UINT32_FMT" ]",
                                sUntil.tm_year + 1900,
                                sUntil.tm_mon + 1,
                                sUntil.tm_mday,
                                sUntil.tm_hour,
                                sUntil.tm_min,
                                sUntil.tm_sec);

                IDE_CALLBACK_SEND_MSG( sMsgBuf );
                break;
            }
        default:
            {
                IDE_ASSERT( 0 );
                break;
            }
    }


    // [COMMON-1]
    // ������ ���������� �α׷��ڵ带 �ǵ��ϱ� ���� RedoLSN Manager �ʱ�ȭ
    IDE_TEST( smrRedoLSNMgr::initialize( &sMinFromRedoLSN )
              != IDE_SUCCESS );
    sCommonState = 1;

    // flush�� �ϱ� ���� sdbFlushMgr�� �ʱ�ȭ�Ѵ�.
    // ������ control �ܰ������� meria recovery�� ���� flush manager�� �ʿ��ϴ�.
    IDE_TEST( sdbFlushMgr::initialize( smuProperty::getBufferFlusherCnt() )
              != IDE_SUCCESS );
    sDiskState = 2;

    idlOS::snprintf( sMsgBuf, SM_MAX_FILE_NAME,
                     "             Applying " );

    IDE_CALLBACK_SEND_SYM( sMsgBuf );

    // To Fix PR-13786, PR-14560, PR-14660
    IDE_TEST( recoverAllFailureTBS( aRecoveryType,
                                    sFailureMediaType,
                                    &sUntilTIME,
                                    &sCurRedoLSN,
                                    &sFromDiskRedoLSN,
                                    &sToDiskRedoLSN,
                                    &sFromMemRedoLSN,
                                    &sToMemRedoLSN )
              != IDE_SUCCESS );
    sDiskState = 3;

    IDE_CALLBACK_SEND_SYM("\n");

    switch ( aRecoveryType )
    {
        case SMI_RECOVER_UNTILTIME:
        case SMI_RECOVER_UNTILCANCEL:
            {
                // �ҿ��� ���� �ΰ��
                idlOS::snprintf( sMsgBuf,
                                 SM_MAX_FILE_NAME,
                                 "\n             recover at < %"ID_UINT32_FMT", %"ID_UINT32_FMT" >",
                                 sCurRedoLSN.mFileNo,
                                 sCurRedoLSN.mOffset );

                IDE_CALLBACK_SEND_SYM(sMsgBuf);

                sResetLSN = smrRedoLSNMgr::getLstCheckLogLSN();

                sMemResetLSNPtr  = &sResetLSN;
                sDiskResetLSNPtr = &sResetLSN;
                break;
            }
        case SMI_RECOVER_COMPLETE:
            {
                // ���������ÿ��� ResetLSN�� ���������
                // ������ �ʿ����.
                sMemResetLSNPtr = NULL;
                sDiskResetLSNPtr = NULL;
                break;
            }
        default:
            break;
    }

    if ( (sFailureMediaType & SMR_FAILURE_MEDIA_MRDB)
         == SMR_FAILURE_MEDIA_MRDB )
    {
        // Stable�� ��� �÷����ϰ� DIRTY PAGE�� ��� �����Ѵ�.
        IDE_TEST( flushAndRemoveDirtyPagesAllMemTBS() != IDE_SUCCESS );

        IDE_CALLBACK_SEND_MSG("  Restoring unstable checkpoint images...");
        // �����Ϸ��� �޸� ����Ÿ���� ����� REDOLSN�� �����Ѵ�.
        IDE_TEST( repairFailureChkptImageHdr( sMemResetLSNPtr )
                  != IDE_SUCCESS );
    }

    sDiskState = 2;
    if ( (sFailureMediaType & SMR_FAILURE_MEDIA_DRDB)
         == SMR_FAILURE_MEDIA_DRDB )
    {
        // ��ũ �α׷��ڵ带 ��� �ݿ��Ѵ�.
        IDE_TEST( applyHashedDiskLogRec( aStatistics) != IDE_SUCCESS );

        // �����Ϸ��� ��ũ ����Ÿ���� ����� REDOLSN�� �����Ѵ�.
        IDE_TEST( smLayerCallback::repairFailureDBFHdr( sDiskResetLSNPtr )
                  != IDE_SUCCESS );
    }
    /* Secondary Buffer ������ �ʱ�ȭ �Ѵ� */
    IDE_TEST( smLayerCallback::repairFailureSBufferHdr( aStatistics,
                                                        sDiskResetLSNPtr )
              != IDE_SUCCESS );
 
    IDE_CALLBACK_SEND_MSG("\n             Log Applied");

    switch ( aRecoveryType )
    {
        case SMI_RECOVER_UNTILTIME:
        case SMI_RECOVER_UNTILCANCEL:
            {
                IDE_ASSERT( sMemResetLSNPtr != NULL );
                IDE_ASSERT( sDiskResetLSNPtr != NULL );

                // update RESETLOGS to loganchor
                IDE_TEST( mAnchorMgr.updateResetLSN( &sResetLSN )
                          != IDE_SUCCESS );

                IDE_CALLBACK_SEND_MSG("\n             updated resetlogs.");

                // update RedoLSN to loganchor
                IDE_TEST( mAnchorMgr.updateRedoLSN( sDiskResetLSNPtr,
                                                   sMemResetLSNPtr )
                         != IDE_SUCCESS );
                break;
            }
        case SMI_RECOVER_COMPLETE:
        default:
            break;
    }

    IDE_CALLBACK_SEND_MSG(
        "  [ RECMGR ] Database media recovery successful.");

    sCommonState = 0;
    IDE_TEST( smrRedoLSNMgr::destroy() != IDE_SUCCESS );

    IDE_TEST( smrLogMgr::getLogFileMgr().closeAllLogFile() != IDE_SUCCESS );

    if ( (sFailureMediaType & SMR_FAILURE_MEDIA_MRDB)
         == SMR_FAILURE_MEDIA_MRDB )
    {
        // [MEMORY-0]
        // �޸� ���̺����̽��� ������ �����ϱ���
        // ���� ��� ���̺����̽����� ��� �����Ѵ�..
        sMemState = 0;
        IDE_TEST( finalMediaRecovery4MemTBS() != IDE_SUCCESS );
    }

    if ( (sFailureMediaType & SMR_FAILURE_MEDIA_DRDB)
         == SMR_FAILURE_MEDIA_DRDB )
    {
        // dirty page�� �������� �ʱ� ������ ��� buffer ����
        // page���� invalid ��Ų��.
        IDE_TEST( sdbBufferMgr::pageOutAll( NULL )
                  != IDE_SUCCESS );
    }

    // flush manager�� �����Ų��. sdbBufferMgr::pageOutAll()�� �ϱ����ؼ���
    // flush manager�� ���־�� �ϱ� ������ pageOutAll()���� �ڿ� �־��Ѵ�.
    sDiskState = 1;
    IDE_TEST( sdbFlushMgr::destroy() != IDE_SUCCESS );

    // [DISK-0]
    sDiskState = 0;
    IDE_TEST( smLayerCallback::destroyRedoMgr() != IDE_SUCCESS );

    mRestart = ID_FALSE;
    mMediaRecoveryPhase = ID_FALSE; /* �̵�� ���� �����߿��� TRUE */

    // fix BUG-17158
    // offline DBF�� write�� �Ұ����ϵ��� flag �� �����Ѵ�.
    sddDiskMgr::setEnableWriteToOfflineDBF( ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_log_consistency )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_ERR_LOG_CONSISTENCY, mLostLogFile ) );
    }
    IDE_EXCEPTION( err_secondary_buffer_service );
    {
        IDE_SET( ideSetErrorCode( 
                    smERR_ABORT_service_secondary_buffer_in_recv ) );
    }
    IDE_EXCEPTION( err_exist_active_trans );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_EXIST_ACTIVE_TRANS_IN_RECOV ));
    }
    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch ( sMemState )
        {
            case 1:
                IDE_ASSERT( finalMediaRecovery4MemTBS() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        switch ( sDiskState )
        {
            case 3:
                IDE_ASSERT( smLayerCallback::applyHashedLogRec( aStatistics )
                            == IDE_SUCCESS );

            case 2:
                IDE_ASSERT( sdbFlushMgr::destroy() == IDE_SUCCESS );

            case 1:
                IDE_ASSERT( smLayerCallback::removeAllRecvDBFHashNodes()
                            == IDE_SUCCESS );

                IDE_ASSERT( smLayerCallback::destroyRedoMgr() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        switch ( sCommonState )
        {
            case 1:
                IDE_ASSERT( smrRedoLSNMgr::destroy() == IDE_SUCCESS );
                IDE_ASSERT( smrLogMgr::getLogFileMgr().closeAllLogFile() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();

    mMediaRecoveryPhase = ID_FALSE; /* �̵�� ���� �����߿��� TRUE */
    mRestart = ID_FALSE;
    sddDiskMgr::setEnableWriteToOfflineDBF( ID_FALSE );

    return IDE_FAILURE;

}

/*
   �ǵ��� �αװ� common �α�Ÿ������ Ȯ���Ѵ�.

   [IN]  aCurLogPtr         - ���� �ǵ��� �α��� Ptr
   [IN]  aLogType           - ���� �ǵ��� �α��� Type
   [OUT] aIsApplyLog        - �̵��� ���뿩��

   BUG-31430 - Redo Logs should not be reflected in the complete media recovery
               have been reflected.

   aFailureMediaType�� Ȯ���Ͽ� �̹� �̵�� ���� �Ϸ�� �α״� �������� �ʵ���
   �Ѵ�.
*/
IDE_RC smrRecoveryMgr::filterCommonRedoLogType( smrLogType   aLogType,
                                                UInt         aFailureMediaType,
                                                idBool     * aIsApplyLog )
{
    idBool          sIsApplyLog;

    IDE_DASSERT( aIsApplyLog != NULL );

    switch ( aLogType )
    {
        case SMR_LT_FILE_END:
            {
                sIsApplyLog = ID_TRUE;
                break;
            }
        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_MEMTRANS_GROUPCOMMIT:
            {
                if ( (aFailureMediaType & SMR_FAILURE_MEDIA_MRDB) ==
                     SMR_FAILURE_MEDIA_MRDB )
                {
                    sIsApplyLog = ID_TRUE;
                }
                else
                {
                    sIsApplyLog = ID_FALSE;
                }
                break;
            }
        case SMR_LT_DSKTRANS_COMMIT:
            {
                if ( (aFailureMediaType & SMR_FAILURE_MEDIA_DRDB) ==
                     SMR_FAILURE_MEDIA_DRDB )
                {
                    sIsApplyLog = ID_TRUE;
                }
                else
                {
                    sIsApplyLog = ID_FALSE;
                }
                break;
            }
        default:
            {
                sIsApplyLog = ID_FALSE;
                break;
            }
    }

    *aIsApplyLog = sIsApplyLog;

    return IDE_SUCCESS;
}

/*
   �ǵ��� �αװ� ������ �޸� ����Ÿ������ ������ ���ԵǴ���
   �Ǵ��Ͽ� ���뿩�θ� ��ȯ�Ѵ�.

   [IN]  aCurLogPtr         - ���� �ǵ��� �α��� Ptr
   [IN]  aLogType           - ���� �ǵ��� �α��� Type
   [OUT] aIsApplyLog        - �̵��� ���뿩��
*/
IDE_RC smrRecoveryMgr::filterRedoLog4MemTBS(
    SChar      * aCurLogPtr,
    smrLogType   aLogType,
    idBool     * aIsApplyLog )
{
    scGRID          sGRID;
    scSpaceID       sSpaceID;
    scPageID        sPageID;
    smrUpdateLog    sUpdateLog;
    smrCMPSLog      sCMPSLog;
    smrNTALog       sNTALog;
    idBool          sIsMemTBSLog;
    idBool          sIsExistTBS;
    idBool          sIsApplyLog;

    IDE_DASSERT( aCurLogPtr  != NULL );
    IDE_DASSERT( aIsApplyLog != NULL );

    SC_MAKE_GRID( sGRID, 0, 0, 0 );

    // SMR_FAILURE_MEDIA_MRDB
    // [1] �α�Ÿ��Ȯ��
    switch ( aLogType )
    {
        case SMR_LT_TBS_UPDATE:
            {
                // MEMORY TABLESPACE UPDATE �α״� �ݿ����� �ʴ´�.
                // DISK TABLESPACE UPDATE �α״� ����������
                // �ݿ��Ѵ�. => filterRedoLogType4DiskTBS���� ó��
                // �ϴ� �� �Լ������� ������ aIsApplyLog �� ID_FALSE��
                // ��ȯ�Ѵ�.
                // �̵�� ���������� Loganchor�� ��� TableSpace ���µ�
                // �ݿ����� �ʴ´�. Loganchor�� �������� �ʴ´ٴ� �ǹ��̴�.
                sIsMemTBSLog = ID_FALSE;
                break;
            }
        case SMR_LT_UPDATE:
            {
                // Memory Update Log�� �ݿ��Ѵ�.
                idlOS::memcpy( &sUpdateLog,
                               aCurLogPtr,
                               ID_SIZEOF(smrUpdateLog) );

                SC_COPY_GRID( sUpdateLog.mGRID, sGRID );
                sIsMemTBSLog = ID_TRUE;
                break;
            }
        case SMR_LT_COMPENSATION:
            {
                idlOS::memcpy( &sCMPSLog,
                               aCurLogPtr,
                               SMR_LOGREC_SIZE(smrCMPSLog) );

                if ( (sctUpdateType)sCMPSLog.mTBSUptType
                     == SCT_UPDATE_MAXMAX_TYPE)
                {
                    sIsMemTBSLog = ID_TRUE;

                    // MEMORY UPDATE CLR �α״� �ݿ��Ѵ�.
                    SC_COPY_GRID( sCMPSLog.mGRID, sGRID );
                }
                else
                {
                    // MEMORY TABLESAPCE UPDATE CLR �α�
                    // �ݿ����� �ʴ´�.
                    sIsMemTBSLog = ID_FALSE;
                }
                break;
            }
        case SMR_LT_DIRTY_PAGE:
            {
                // �ϴ� Apply �α׷� �����ϰ�
                // redo �� �� �����Ѵ�.
                sIsMemTBSLog = ID_TRUE;
                break;
            }
        case SMR_LT_NTA:
            {
                // BUG-28709 Media recovery�� SMR_LT_NTA�� ���ؼ���
                //           redo�� �ؾ� ��
                // BUG-29434 NTA Log�� �ݿ��� Page�� Media Recovery
                //           ������� Ȯ���մϴ�.
                idlOS::memcpy( &sNTALog,
                               aCurLogPtr,
                               SMR_LOGREC_SIZE(smrNTALog));

                sIsMemTBSLog   = ID_TRUE;
                sSpaceID = sNTALog.mSpaceID;

                switch(sNTALog.mOPType)
                {
                    case SMR_OP_SMC_TABLEHEADER_ALLOC:
                    case SMR_OP_SMC_FIXED_SLOT_ALLOC:
                    case SMR_OP_CREATE_TABLE:
                        sPageID = SM_MAKE_PID(sNTALog.mData1);
                        SC_MAKE_GRID( sGRID, sSpaceID, sPageID, 0 );
                        break;
                    default :
                        // Media Recovery���� Redo����� �ƴ�
                        sIsMemTBSLog = ID_FALSE;
                        break;
                }

                break;
            }
        default:
            {
                sIsMemTBSLog = ID_FALSE;
                break;
            }
    }

    // Memory Log �� ���
    if ( sIsMemTBSLog == ID_TRUE )
    {
        if ( SC_GRID_IS_NOT_NULL( sGRID ) )
        {
            // [2] �α��� ������������ ������ ����Ÿ���Ͽ�
            // �ش��ϴ��� Ȯ��
            IDE_TEST( smmTBSMediaRecovery::findMatchFailureDBF(
                          SC_MAKE_SPACE( sGRID ),
                          SC_MAKE_PID( sGRID ),
                          &sIsExistTBS,
                          &sIsApplyLog )
                      != IDE_SUCCESS );
        }
        else
        {
            // redo �� �� �����Ѵ�.
            sIsApplyLog = ID_TRUE;
        }
    }
    else
    {
        // APPLY�� Memory Log�� �ƴ� ���
        sIsApplyLog = ID_FALSE;
    }

    *aIsApplyLog = sIsApplyLog;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   �ǵ��� �αװ� ��ũ �α�Ÿ������ Ȯ���Ѵ�.
   ���� ������ ����Ÿ������ ���������� ���� ���ԵǴ� �α�������
   sdrRedoMgr�� ���ؼ� �Ľ��Ҷ� Filtering�ȴ�.

   [IN]  aCurLogPtr         - ���� �ǵ��� �α��� Ptr
   [IN]  aLogType           - ���� �ǵ��� �α��� Type
   [OUT] aIsApplyLog        - �̵��� ���뿩��
*/
IDE_RC smrRecoveryMgr::filterRedoLogType4DiskTBS( SChar       * aCurLogPtr,
                                                  smrLogType    aLogType,
                                                  UInt          aIsNeedApplyDLT,
                                                  idBool      * aIsApplyLog )
{
    idBool          sIsApplyLog;
    smrTBSUptLog    sTBSUptLog;

    IDE_DASSERT( aCurLogPtr  != NULL );
    IDE_DASSERT( aIsApplyLog != NULL );
    IDE_DASSERT( (aIsNeedApplyDLT == SMR_DLT_REDO_EXT_DBF) ||
                 (aIsNeedApplyDLT == SMR_DLT_REDO_ALL_DLT) );

    // SMR_FAILURE_MEDIA_DRDB
    switch ( aLogType )
    {
        case SMR_LT_TBS_UPDATE:
            {
                // �̵�� ���������� Loganchor�� ��� TableSpace ���µ�
                // �ݿ����� �ʴ´�. Loganchor�� �������� �ʴ´ٴ� �ǹ��̴�.
                idlOS::memcpy( &sTBSUptLog,
                               aCurLogPtr,
                               ID_SIZEOF(smrTBSUptLog) );

                if ( sTBSUptLog.mTBSUptType ==
                     SCT_UPDATE_DRDB_EXTEND_DBF )
                {
                    // DISK TABLESPACE�� EXTEND DBF �α״�
                    // �ݿ��Ѵ�.
                    sIsApplyLog = ID_TRUE;
                }
                else
                {
                    // �� �� Ÿ���� �ݿ����� �ʴ´�.
                    sIsApplyLog = ID_FALSE;
                }
                break;
            }
        case SMR_DLT_REDOONLY:
        case SMR_DLT_UNDOABLE:
        case SMR_DLT_NTA:
        case SMR_DLT_REF_NTA:
        case SMR_DLT_COMPENSATION:
            {
                // ��� �ݿ��Ѵ�.
                if ( aIsNeedApplyDLT == SMR_DLT_REDO_ALL_DLT )
                {
                    sIsApplyLog = ID_TRUE;
                    break;
                }
                else
                {
                    sIsApplyLog = ID_FALSE;
                    break;
                }
            }
        default:
            {
                sIsApplyLog = ID_FALSE;
                break;
            }
    }

    *aIsApplyLog = sIsApplyLog;

    return IDE_SUCCESS;
}

/*
  To Fix PR-13786
  ���⵵ ������ ���Ͽ� recoverDB() �Լ�����
  �ϳ��� log file�� ���� redo �۾��� �и��Ѵ�.

*/
IDE_RC smrRecoveryMgr::recoverAllFailureTBS( smiRecoverType        aRecoveryType,
                                             UInt                  aFailureMediaType,
                                             time_t              * aUntilTIME,
                                             smLSN               * aCurRedoLSNPtr,
                                             smLSN               * aFromDiskRedoLSN,
                                             smLSN               * aToDiskRedoLSN,
                                             smLSN               * aFromMemRedoLSN,
                                             smLSN               * aToMemRedoLSN )
{
    smrLogHead        * sLogHeadPtr = NULL;
    SChar*              sLogPtr;
    UInt                sFileCount;
    smrTransCommitLog   sCommitLog;
    time_t              sCommitTIME;
    // To Fix PR-14650, PR-14660
    idBool              sIsValid       = ID_FALSE;
    void              * sCurTrans      = NULL;
    smLSN             * sCurRedoLSNPtr = NULL;
    smrLogType          sLogType;
    UInt                sFailureMediaType;
    idBool              sIsApplyLog;
    UInt                sLogSizeAtDisk = 0;
    // BUG-38503
    UInt                sIsNeedApplyDLT = SMR_DLT_REDO_NONE;

    IDE_DASSERT( aFailureMediaType != SMR_FAILURE_MEDIA_NONE );
    IDE_DASSERT( aRecoveryType     != SMI_RECOVER_RESTART );
    IDE_DASSERT( aFromDiskRedoLSN  != NULL );
    IDE_DASSERT( aToDiskRedoLSN    != NULL );
    IDE_DASSERT( aFromMemRedoLSN!= NULL );
    IDE_DASSERT( aToMemRedoLSN  != NULL );

    sFailureMediaType = aFailureMediaType;

    sFileCount = 1;

    while ( 1 )
    {
        sIsApplyLog     = ID_FALSE;
        sIsNeedApplyDLT = SMR_DLT_REDO_NONE;

        // [1] Log �ǵ�
        IDE_TEST( smrRedoLSNMgr::readLog(&sCurRedoLSNPtr,
                                        &sLogHeadPtr,
                                        &sLogPtr,
                                        &sLogSizeAtDisk,
                                        &sIsValid)
                 != IDE_SUCCESS );

        // [2] �α׷��ڵ尡 Invalid �� ��� �Ϸ����� Ȯ��
        // ���� UNTILCANCEL�� �α������� �������� ���� ��쿡
        // �ش��Ѵ�.
        if ( sIsValid == ID_FALSE )
        {
            if ( sCurRedoLSNPtr != NULL )
            {
                ideLog::log( IDE_SM_0, 
                             "Media Recovery Completed "
                             "[ %"ID_UINT32_FMT", %"ID_UINT32_FMT"]",
                             sCurRedoLSNPtr->mFileNo,
                             sCurRedoLSNPtr->mOffset );
            }

            break; /* invalid�� �α׸� �ǵ��� ��� */
        }

        mCurLogPtr     = sLogPtr;
        mCurLogHeadPtr = sLogHeadPtr;
        SM_GET_LSN( mLstRedoLSN, *sCurRedoLSNPtr );

        sLogType = smrLogHeadI::getType(sLogHeadPtr);

        // [3] �̵�� ������ �߻��� ���̺����̽��� ���� Log��
        // ���뿩�θ� Ȯ���Ѵ�.
        IDE_ASSERT( sIsApplyLog == ID_FALSE );

        /*
           redo ������ ������ ���¿��� ���õ� �α׸�
           redo �� �ϱ� ���Ѱ��̴�.

           1. From Redo LSN
           RedoLSNMgr�� ��ȯ�ϴ� �α��߿� Memory From Redo LSN
           ���� ũ�ų����� LSN�� �αװ� ��Ÿ���� ���������� ���Եȴ�.

           2. To Redo LSN
           RedoLSNMgr�� ��ȯ�ϴ� �α��߿� Memory To Redo LSN
           ���� ū LSN�� �αװ� ��Ÿ���� ���������� ���Եȴ�.  */

        // commit �αװų� file_end �α����� Ȯ���Ѵ�
        IDE_TEST( filterCommonRedoLogType( sLogType,
                                           sFailureMediaType,
                                           & sIsApplyLog )
                  != IDE_SUCCESS );

        if ( ( sFailureMediaType & SMR_FAILURE_MEDIA_MRDB )
             == SMR_FAILURE_MEDIA_MRDB )
        {
            // ������ �޸� �α� Filtering
            if ( sIsApplyLog == ID_FALSE )
            {
                IDE_TEST( filterRedoLog4MemTBS( sLogPtr,
                                                sLogType,
                                                & sIsApplyLog)
                          != IDE_SUCCESS );

                // Memory From Redo LSN ���Ͽ� ���뿩�� �Ǵ�
                if ( smrCompareLSN::isLT( sCurRedoLSNPtr,
                                          aFromMemRedoLSN )
                     == ID_TRUE )
                {
                    // ���� �α׸� �ǵ��Ѵ�.
                    sIsApplyLog = ID_FALSE;
                }
                else
                {
                    // Memory Tablespace ���������� �ش��Ѵ�.
                    // Failure ����Ÿ�������� ���θ� �˻��ؼ�
                    // ���� ���뿩�θ� �����Ѵ�.
                }

                // Memory To Redo LSN ���Ͽ� ���뿩�� �Ǵ�
                if ( smrCompareLSN::isGT( sCurRedoLSNPtr,
                                          aToMemRedoLSN )
                     == ID_TRUE )
                {
                    // �ش�α״� �ݿ����� �ʴ´�.
                    sIsApplyLog = ID_FALSE;

                    // ��� ������ �αװ� ����Ǿ���.
                    sFailureMediaType &= (~SMR_FAILURE_MEDIA_MRDB);
                }
                else
                {
                    // ���� �ǵ��� �αװ� ���� ToMemReodLSN��
                    // �������� ���� ���
                }
            }
            else
            {
                // �̹� �����Ǵ��� �� ����̴�
            }
        }
        else
        {
            // Memory TableSpace�� ��������� �ƴ� ���
        }

        if ( ( sFailureMediaType & SMR_FAILURE_MEDIA_DRDB )
             == SMR_FAILURE_MEDIA_DRDB )
        {
            // DISK �� ���ؼ��� ó���Ѵ�.
            if ( sIsApplyLog == ID_FALSE )
            {
                // Disk From Redo LSN ���Ͽ� ���뿩�� �Ǵ�
                if ( smrCompareLSN::isLT( sCurRedoLSNPtr,
                                          aFromDiskRedoLSN ) == ID_TRUE )
                {
                    sIsNeedApplyDLT = SMR_DLT_REDO_NONE;
                }
                else
                {
                    // Memory Tablespace ���������� �ش��Ѵ�.
                    // Failure ����Ÿ�������� ���θ� �˻��ؼ�
                    // ���� ���뿩�θ� �����Ѵ�.

                    // Disk To Redo LSN ���Ͽ� ���뿩�� �Ǵ�
                    if ( smrCompareLSN::isGT( sCurRedoLSNPtr, aToDiskRedoLSN )
                         == ID_TRUE )
                    {
                        // ���� �ǵ��� �αװ� aToDiskRedoLSN���� ũ�ٸ�
                        // �α� ������ ���̻� �������� �ʴ´�.

                        // BUG-38503
                        // ���� ������ ���ؼ�, SMR_LT_TBS_UPDATE Ÿ�� �� 
                        // SCT_UPDATE_DRDB_EXTEND_DBF �� ��� redo �ϵ��� �Ѵ�.
                        if ( smrCompareLSN::isGT( sCurRedoLSNPtr,
                                                  aToMemRedoLSN )
                             == ID_TRUE )
                        {
                            // �޸� TBS ���� �����̸�, Disk�� �����Ų��. 
                            sFailureMediaType  &= (~SMR_FAILURE_MEDIA_DRDB);
                            sIsNeedApplyDLT     = SMR_DLT_REDO_NONE;
                        }
                        else
                        {
                            sIsNeedApplyDLT = SMR_DLT_REDO_EXT_DBF;
                        }
                    }
                    else
                    {
                        // ���� �ǵ��� �αװ� ���� ToMemReodLSN��
                        // �������� ���� ���
                        sIsNeedApplyDLT = SMR_DLT_REDO_ALL_DLT;
                    }
                }

                if ( sIsNeedApplyDLT == SMR_DLT_REDO_NONE )
                {
                    sIsApplyLog = ID_FALSE;
                }
                else
                {
                    // ������ ��ũ �α� Filtering
                    IDE_TEST( filterRedoLogType4DiskTBS(
                            sLogPtr,
                            sLogType,
                            sIsNeedApplyDLT,
                            &sIsApplyLog ) != IDE_SUCCESS );
                }
            }
            else
            {
                // �̹� ���� �Ǵ��� �� ����̴�.
            }
        }
        else
        {
            // Disk TableSpace�� ��������� �ƴ� ���
        }

        // [4] ���� �Ϸ� ���� üũ
        if ( sFailureMediaType == SMR_FAILURE_MEDIA_NONE )
        {
            // COMPLETE ������ ��쿡 ���⼭ ������ �� �ִ�.
            // ���������� ��� scan �� ��� ���̻� redo��
            // �������� �ʴ´�.
            break;
        }

        // [5] ������ �α��ΰ�?
        if ( sIsApplyLog == ID_FALSE )
        {
            // ���� �α׸� �ǵ��Ѵ�.
            sCurRedoLSNPtr->mOffset += sLogSizeAtDisk;
            continue;
        }

        if ( (sLogType == SMR_LT_MEMTRANS_COMMIT) ||
             (sLogType == SMR_LT_DSKTRANS_COMMIT) ||
             (sLogType == SMR_LT_MEMTRANS_GROUPCOMMIT) )
        {
            // [7] Ÿ�Ӻ��̽� �ҿ���������� Ŀ�Էα׿� �����
            // �ð��� Ȯ���Ͽ� ����� ���� ���θ� �Ǵ��Ѵ�.
            if ( aRecoveryType == SMI_RECOVER_UNTILTIME )
            {
                /* BUG-47525 �Ϲ� CommitLog�� GroupCommitLog�� ����� �ٸ�����
                 * Time ������ �����ϱ� ������ Time�� Ȯ���ϴ� �����
                 * �׳� CommitLog ���·� �о �ȴ�. */
                idlOS::memcpy(&sCommitLog,
                              sLogPtr,
                              ID_SIZEOF(smrTransCommitLog));

                sCommitTIME = (time_t)sCommitLog.mTxCommitTV;

                if (sCommitTIME > *aUntilTIME)
                {
                    // Time-Based �ҿ��� ���� �Ϸ����
                    break;
                }
                else
                {
                    // commit log redo ����
                }
            }
            else
            {
                // Ÿ�Ӻ��̽� �ҿ��� ������ �ƴ� ���
                // commit log�� ���ؼ� üũ���� �ʴ´�.
            }
        }

        // Get Transaction Entry
        // �޸� slot�� TID�� �����ؾ��ϴ� ��찡 �ִ�.
        // �̵�� �����ÿ��� Transaction ��ü�� �ѱ�����,
        // Ʈ����� ���¸� Begin ��Ű�� �ʴ´�.
        // �ֳ��ϸ� Begin ���°� �ƴ� ��쿡 OID ��ϰ� Pending ����
        // ����� ���� �ʾ� active tx list�� prepare tx
        // � ������� �ʾƵ� �Ǳ� �����̴�.
        sCurTrans = smLayerCallback::getTransByTID( smrLogHeadI::getTransID( sLogHeadPtr ) );

        // [6] �����ؾ��� �α� ���ڵ带 ������Ѵ�.
        switch( sLogType )
        {
            case SMR_LT_UPDATE:
            case SMR_LT_DIRTY_PAGE:
            case SMR_LT_TBS_UPDATE:
            case SMR_LT_COMPENSATION:
            case SMR_LT_NTA:
            case SMR_DLT_REDOONLY:
            case SMR_DLT_UNDOABLE:
            case SMR_DLT_NTA:
            case SMR_DLT_REF_NTA:
            case SMR_DLT_COMPENSATION:
            case SMR_LT_FILE_END:
            case SMR_LT_MEMTRANS_COMMIT:
            case SMR_LT_DSKTRANS_COMMIT:
            case SMR_LT_MEMTRANS_GROUPCOMMIT:
                {
                    // Ʈ����ǰ� ������� �����
                    IDE_TEST( redo( sCurTrans,
                                    sCurRedoLSNPtr,
                                    &sFileCount,
                                    sLogHeadPtr,
                                    sLogPtr,
                                    sLogSizeAtDisk,
                                    ID_TRUE  /* after checkpoint */ )
                              != IDE_SUCCESS );
                    break;
                }
            default :
                {
                    // sIsApplyLog == ID_FALSE
                    // filter �������� �̹� �ɷ������Ƿ�
                    // assert �˻��Ѵ�.
                    IDE_ASSERT( 0 );
                    break;
                }
        }

        // �ؽ̵� ��ũ�α׵��� ũ���� ������
        // DISK_REDO_LOG_DECOMPRESS_BUFFER_SIZE �� �����
        // �ؽ̵� �α׵��� ��� ���ۿ� �����Ѵ�.
        IDE_TEST( checkRedoDecompLogBufferSize() != IDE_SUCCESS );
    }
    mCurLogPtr     = NULL;
    mCurLogHeadPtr = NULL;

    // To Fix PR-14560, PR-14660
    if ( sCurRedoLSNPtr != NULL )
    {
        SM_GET_LSN( *aCurRedoLSNPtr, *sCurRedoLSNPtr );
    }
    else
    {
        SM_LSN_INIT( *aCurRedoLSNPtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    mCurLogPtr     = NULL;
    mCurLogHeadPtr = NULL;

    return IDE_FAILURE;
}


/*********************************************************
 * Description:rebuildArchLogfileList�� Server Start�� Archive LogFile
 * List�� �籸���ϴ� ���̴�. checkpoint�� ������ ������ archive�Ǿ�
 * �⶧���� �������Ϻ��� archive List�� �߰��Ѵ�.
 *
 * aEndLSN - [IN] ������ Write�� logfile���� Archive Logfile List�� ��
 *              ���Ѵ�.
 * BUGBUG: �߰��Ǵ� Logfile�� �̹� Archive�Ǿ��� �� �ִ�. ��𼱰� check�ؾߵ�.
 *********************************************************/
IDE_RC smrRecoveryMgr::rebuildArchLogfileList( smLSN  *aEndLSN )
{
    UInt sLstDeleteLogFileNo;
    UInt sAddArchLogFileCount = 0;

    sLstDeleteLogFileNo = getLstDeleteLogFileNo();

    if ( sLstDeleteLogFileNo < aEndLSN->mFileNo )
    {
        sAddArchLogFileCount = sAddArchLogFileCount + aEndLSN->mFileNo - sLstDeleteLogFileNo;

        IDE_TEST( smrLogMgr::getArchiveThread().recoverArchiveLogList( sLstDeleteLogFileNo,
                                                                       aEndLSN->mFileNo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    if ( sAddArchLogFileCount != 0)
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_REBUILD_ARCHLOG_LIST,
                    sAddArchLogFileCount);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * Description: Restart Recovery�ÿ� ���� Redo LSN�� ����Ѵ�.
 *
 *    Checkpoint ���� ��
 *    BEGIN CHECKPOINT LOG ������ ������ �ϵ�
 *
 *    A. Restart Recovery�ÿ� Redo���� LSN���� ���� Recovery LSN�� ���
 *        A-1 Memory Recovery LSN ����
 *        A-2 Disk Recovery LSN ����
 *            - Disk Dirty Page Flush        [step0]
 *            - Tablespace Log Anchor ����ȭ [step1]
 *
 * Implementation
 *
 *********************************************************/
IDE_RC smrRecoveryMgr::chkptCalcRedoLSN( idvSQL       * aStatistics,
                                         smrChkptType   aChkptType,
                                         idBool         aIsFinal,
                                         smLSN        * aRedoLSN,
                                         smLSN        * aDiskRedoLSN,
                                         smLSN        * aEndLSN )
{
    smLSN   sMinLSN;
    smLSN   sDiskRedoLSN;
    smLSN   sSBufferRedoLSN; 

    //---------------------------------
    // A-1. Memory Recovery LSN �� ����
    //---------------------------------

    //---------------------------------
    // Lst LSN�� �����´�.
    //---------------------------------
    smrLogMgr::getLstLSN(aRedoLSN);

    *aDiskRedoLSN = *aRedoLSN;
    *aEndLSN      = *aDiskRedoLSN;

    /* ------------------------------------------------
     * # MRDB�� recovery LSN�� �����Ͽ� ���� checkpoint
     * �α׿� ����Ѵ�.
     * Ʈ����ǵ��� ���� �������� ����� �α��� LSN���� �����ϸ�,
     * ���ٸ�, �αװ������� End LSN���� �����Ѵ�.
     * ----------------------------------------------*/
    smLayerCallback::getMinLSNOfAllActiveTrans( &sMinLSN );

    if ( sMinLSN.mFileNo != ID_UINT_MAX )
    {

        if ( smrCompareLSN::isGT( aRedoLSN,
                                 &sMinLSN ) == ID_TRUE )

        {
            *aRedoLSN = sMinLSN;
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        /* nothing to do ... */
    }

    //---------------------------------
    // A-2. Disk Recovery LSN �� ����
    //---------------------------------
    /* ------------------------------------------------
     * ���� checkpoint �α׿� ����� DRDB�� recovery LSN�� �����Ѵ�.
     * ���۰������� dirty page flush ����Ʈ���� ���� ������
     * dirty page�� first modified LSN���� �����ϸ�, ���ٸ�
     * �αװ������� End LSN���� �����Ѵ�.
     * ----------------------------------------------*/
    if ( aChkptType == SMR_CHKPT_TYPE_BOTH )
    {
        IDE_TEST( makeBufferChkpt( aStatistics,
                                   aIsFinal,
                                   aEndLSN,
                                   aDiskRedoLSN )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���� �޸� DB üũ����Ʈ�� ��ũ ���̺����̽� �����
        // ���� ���� ��� ��ũüũ����Ʈ�� ���� ������, �α�������
        // ������ �Ǳ� ������ ����� ���õ� �α������� �������� �ʵ���
        // ����Ѵ�.

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP21);


        sdbBufferMgr::getMinRecoveryLSN( aStatistics, 
                                         &sDiskRedoLSN );

        sdsBufferMgr::getMinRecoveryLSN( aStatistics, 
                                         &sSBufferRedoLSN );

        if ( smrCompareLSN::isLT( &sDiskRedoLSN, &sSBufferRedoLSN )
             == ID_TRUE )
        {
            // PBuffer�� �ִ� RecoveryLSN(sDiskRedoLSN)�� 
            // SecondaryBuffer�� �ִ� RecoveryLSN ���� ������
            // PBuffer�� �ִ� RecoveryLSN  ����
            SM_GET_LSN( *aDiskRedoLSN, sDiskRedoLSN );
        }
        else
        {
            SM_GET_LSN( *aDiskRedoLSN, sSBufferRedoLSN );
        }

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP29,
                    aDiskRedoLSN->mFileNo,
                    aDiskRedoLSN->mOffset );
    }

    if ( (aDiskRedoLSN->mFileNo != ID_UINT_MAX) &&
         (aDiskRedoLSN->mOffset != ID_UINT_MAX))
    {
        // do nothing
    }
    else
    {
        IDE_ASSERT(( aDiskRedoLSN->mFileNo == ID_UINT_MAX) &&
                   ( aDiskRedoLSN->mOffset == ID_UINT_MAX) );
        *aDiskRedoLSN                 = *aEndLSN;
    }

    IDE_DASSERT( (aDiskRedoLSN->mFileNo != ID_UINT_MAX) &&
                 (aDiskRedoLSN->mOffset != ID_UINT_MAX) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************
 * Description: Memory Dirty Page���� Checkpoint Image�� Flush�Ѵ�.
 *
 * [OUT] aSyncLstLSN - Sync�� ������ LSN
 *
 *    Checkpoint ���� ��
 *    END CHECKPOINT LOG ������ �����ϴ� �۾�
 *
 *    C. Memory Dirty Page���� Checkpoint Image�� Flush
 *       C-1 Flush Dirty Pages              [step3]
 *       C-2 Sync Log Files
 *       C-3 Sync Memory Database           [step4]
 *
 * Implementation
 *
 *********************************************************/
IDE_RC smrRecoveryMgr::chkptFlushMemDirtyPages( smLSN * aSyncLstLSN,
                                                idBool  aIsFinal )
{
    smrWriteDPOption sWriteOption;
    ULong            sTotalDirtyPageCnt;
    ULong            sNewDirtyPageCnt;
    ULong            sDupDirtyPageCnt;
    ULong            sTotalCnt;
    ULong            sRemoveCnt;
    SLong            sSyncedLogSize;
    smLSN            sSyncedLSN;
    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * Checkpoint������ �ð��� ���� IO ��������� ����� */
    ULong            sTotalTime;    /* microsecond (���� ����) */
    ULong            sLogSyncTime;
    ULong            sDBFlushTime;
    ULong            sDBSyncTime;
    ULong            sDBWaitTime;
    ULong            sDBWriteTime;
    PDL_Time_Value   sTimevalue;
    ULong            sLogSyncBeginTime;
    ULong            sDPFlushBeginTime;
    ULong            sLastSyncTime;
    ULong            sEndTime;
    SDouble          sLogIOPerf;
    SDouble          sDBIOPerf;

    IDE_DASSERT( aSyncLstLSN != NULL );

    //---------------------------------
    // C-1. Flush Dirty Pages
    //---------------------------------

    // �޼��� ���
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP22);

        IDE_TEST( smrDPListMgr::getTotalDirtyPageCnt( & sTotalDirtyPageCnt )
                  != IDE_SUCCESS );

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP23,
                    sTotalDirtyPageCnt );
    }

    // 3) Write dirty pages on backup file
    //     3.1) Write dirty pages of last check point
    //
    // ��� Tablespace�� ���� SMM => SMR �� Dirty Page�̵�
    IDE_TEST( smrDPListMgr::moveDirtyPages4AllTBS(
                                          SCT_SS_SKIP_CHECKPOINT,
                                          & sNewDirtyPageCnt,
                                          & sDupDirtyPageCnt)
              != IDE_SUCCESS );

    // �޼��� ���
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP24,
                    sNewDirtyPageCnt);

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP25,
                    sDupDirtyPageCnt);
    }

    //---------------------------------
    // C-2. Sync Log Files
    //---------------------------------

    // fix PR-2353
    /* LOG File sync���� */
    smrLogMgr::getLstLSN( aSyncLstLSN );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_BEGIN_SYNCLSN,
                aSyncLstLSN->mFileNo,
                aSyncLstLSN->mOffset );

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * ���Ͽ����� gettimeofday�� �ð��� ����. �� �Լ��� ������ �� ����
     * �Լ�������, Checkpoint�� ȥ�� �����ϸ� �׸� ���� �Ͼ�� �ʱ� ������
     * ����ص� �����ϴ�. */
    /*==============================LogSync==============================*/
    sTimevalue        = idlOS::gettimeofday();
    sLogSyncBeginTime = 
        (time_t)sTimevalue.sec() * 1000 * 1000 + (time_t)sTimevalue.usec();
    IDE_TEST( smrLogMgr::getLFThread().getSyncedLSN( &sSyncedLSN )
              != IDE_SUCCESS );

    IDE_TEST( smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_CKP, aSyncLstLSN )
             != IDE_SUCCESS );

    sSyncedLogSize = 0;
    if ( SM_IS_LSN_INIT( sSyncedLSN ) )
    {
        /* Synced�� INIT LSN�� ���, ��Ȯ�� ����� �� �� ����. */
        sSyncedLogSize = 0;
    }
    else
    {
        sSyncedLogSize = ( ( (SInt) aSyncLstLSN->mFileNo - sSyncedLSN.mFileNo ) *
                           smuProperty::getLogFileSize() ) + 
                         ( (SInt) aSyncLstLSN->mOffset - sSyncedLSN.mOffset );
    }

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_END_SYNCLSN );

    if ( aIsFinal == ID_TRUE )
    {
        sWriteOption = SMR_WDP_FINAL_WRITE ;
    }
    else
    {
        sWriteOption = SMR_WDP_NONE ;
    }

    /*============================DirtyPageFlush============================*/
    sTimevalue        = idlOS::gettimeofday();
    sDPFlushBeginTime = 
        (time_t)sTimevalue.sec() * 1000 * 1000 + (time_t)sTimevalue.usec();

    // Dirty Page���� Disk�� Checkpoint Image�� Flush�Ѵ�.
    IDE_TEST( smrDPListMgr::writeDirtyPages4AllTBS(
                                     SCT_SS_SKIP_CHECKPOINT,
                                     & sTotalCnt,
                                     & sRemoveCnt,
                                     & sDBWaitTime,
                                     & sDBSyncTime,
                                     smmManager::getNxtStableDB, // flush target db
                                     sWriteOption ) /* Final�� True�� ���, �ͷ��� ���ƾ� ��. */
              != IDE_SUCCESS );

    // �޼��� ���
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP26,
                    sTotalCnt);

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP27,
                    sRemoveCnt);
    }

    //---------------------------------
    // C-3. Sync Memory Database
    //---------------------------------
    // �޼��� ���
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP28);
    }

    /*==========================LastDirtyPageFlush==========================*/
    sTimevalue    = idlOS::gettimeofday();
    sLastSyncTime = (time_t)sTimevalue.sec() * 1000 * 1000 + (time_t)sTimevalue.usec();

    // 4) sync db files
    // ����� ��쿡�� ��� Online Tablespace�� ���ؼ� Sync�� �����Ѵ�.
    // �̵����߿��� ��� Online/Offline TableSpace �� �̵�� ����
    // ����� �͸� Sync�� �����Ѵ�.
    IDE_TEST( smmManager::syncDB( SCT_SS_SKIP_CHECKPOINT,
                                  ID_TRUE /* syncLatch ȹ�� �ʿ� */)
              != IDE_SUCCESS );

    /*==============================End==============================*/
    sTimevalue = idlOS::gettimeofday();
    sEndTime   = (time_t)sTimevalue.sec() * 1000 * 1000 + (time_t)sTimevalue.usec();

    sTotalTime    = sEndTime - sLogSyncBeginTime;

    sLogSyncTime  = sDPFlushBeginTime - sLogSyncBeginTime;
    sDBFlushTime  = sEndTime - sDPFlushBeginTime;
    sDBSyncTime  += sEndTime - sLastSyncTime;
    if ( sDBFlushTime > ( sDBWaitTime + sDBSyncTime ) ) 
    {
        sDBWriteTime = sDBFlushTime - sDBWaitTime - sDBSyncTime;
    }
    else
    {
        sDBWriteTime = 0;
    }


    /* BUG-33142 [sm-mem-recovery] Incorrect IO stat calculation at MMDB
     * Checkpoint
     * ���� ������ ����� ���� ������ Byte�� USec�Դϴ�. �̸� MB/Sec���� ǥ���մϴ�.
     * ( Byte /1024 /1024 ) / ( USec / 1000 / 1000 )
     * ���� �� ������ �½��ϴ�.
     *
     * �ٸ� ���ϱ⸦ �տ��� �ϰ� �����⸦ �ڿ��� �ϴ� ���� ��Ȯ���� ���� ������
     *  Byte / ( USec / 1000 / 1000 )  /1024 /1024
     * �� �����մϴ�.*/

    if ( sDBFlushTime > 0 )
    {
        /* Byte/Usec ���� */
        sDBIOPerf = UINT64_TO_DOUBLE( sTotalCnt ) * SM_PAGE_SIZE / sDBFlushTime;
        /* Byte/Usec -> MByte/Sec���� ������ */
        sDBIOPerf = sDBIOPerf * 1000 * 1000 / 1024 / 1024 ;
    }
    else
    {
        sDBIOPerf = 0.0f;
    }

    if ( sLogSyncTime > 0 )
    {
        /* Byte/Usec ���� */
        sLogIOPerf = UINT64_TO_DOUBLE( sSyncedLogSize ) / sLogSyncTime ;
        /* Byte/Usec -> MByte/Sec���� ������ */
        sLogIOPerf = sDBIOPerf * 1000 * 1000 / 1024 / 1024 ;
    }
    else
    {
        sLogIOPerf = 0.0f;
    }

    ideLog::log( IDE_SM_0,
                 "==========================================================\n"
                 "SM IO STAT  - Checkpoint \n"
                 "DB SIZE      : %12"ID_UINT64_FMT" Byte "
                 "( %"ID_UINT64_FMT" Page)\n"
                 "LOG SIZE     : %12"ID_UINT64_FMT" Byte\n"
                 "TOTAL TIME   : %6"ID_UINT64_FMT" s %"ID_UINT64_FMT" us\n"
                 "LOG SYNC TIME: %6"ID_UINT64_FMT" s %"ID_UINT64_FMT" us\n"
                 "DB FLUSH TIME: %6"ID_UINT64_FMT" s %"ID_UINT64_FMT"us\n"
                 "      SYNC TIME : %6"ID_UINT64_FMT" s %"ID_UINT64_FMT" us\n"
                 "      WAIT TIME : %6"ID_UINT64_FMT" s %"ID_UINT64_FMT" us\n"
                 "      WRITE TIME: %6"ID_UINT64_FMT" s %"ID_UINT64_FMT" us\n"
                 "LOG IO PERF  : %6"ID_DOUBLE_G_FMT" MB/sec\n"
                 "DB IO PERF   : %6"ID_DOUBLE_G_FMT" MB/sec\n"
                 "=========================================================\n",
                 sTotalCnt * SM_PAGE_SIZE,  sTotalCnt,
                 sSyncedLogSize, 
                 sTotalTime   /1000/1000, sTotalTime,    
                 sLogSyncTime /1000/1000, sLogSyncTime, 
                 sDBFlushTime /1000/1000, sDBFlushTime, 
                 sDBSyncTime  /1000/1000, sDBSyncTime,  
                 sDBWaitTime  /1000/1000, sDBWaitTime,  
                 sDBWriteTime /1000/1000, sDBWriteTime, 
                 sLogIOPerf,
                 sDBIOPerf );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************
 * Description:
 *
 *    Checkpoint ���� ��
 *    END CHECKPOINT LOG ���Ŀ� �����ϴ� �۾�
 *
 *    E. After  End_CHKPT
 *        E-1. Sync Log Files            [step6]
 *        E-2. Get Remove Log File #         [step7]
 *        E-3. Update Log Anchor             [step8]
 *        E-4. Remove Log Files              [step9]
 *
 * Implementation
 *
 *********************************************************/
IDE_RC smrRecoveryMgr::chkptAfterEndChkptLog( idBool    aRemoveLogFile,
                                              idBool    aFinal,
                                              smLSN   * aBeginChkptLSN,
                                              smLSN   * aEndChkptLSN,
                                              smLSN   * aDiskRedoLSN,
                                              smLSN   * aRedoLSN,
                                              smLSN   * aSyncLstLSN,
                                              smLSN   * aDtxMinLSN )
{
    UInt        j = 0;

    smSN        sMinReplicationSN       = SM_SN_NULL;
    smLSN       sMinReplicationLSN      = {ID_UINT_MAX,ID_UINT_MAX};
    UInt        sFstFileNo;
    UInt        sEndFileNo = ID_UINT_MAX;
    UInt        sLstFileNo;
    UInt        sLstArchLogFileNo;

    SInt        sState             = 0;
    smGetMinSN sGetMinSNFunc;
    SInt        sOnlineDRDBRedoCnt;

    //---------------------------------
    // E-1. Sync Log Files
    //---------------------------------

    // 6) flush system log buffer and Wait for Log Flush

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP11);

    /* LOG File sync����*/
    smrLogMgr::getLstLSN( aSyncLstLSN );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_BEGIN_SYNCLSN,
                aSyncLstLSN->mFileNo,
                aSyncLstLSN->mOffset );

    IDE_TEST( smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_CKP, aSyncLstLSN )
              != IDE_SUCCESS );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP_END_SYNCLSN );

    /* BUG-42785 */
    sOnlineDRDBRedoCnt = getOnlineDRDBRedoCnt();
    if ( sOnlineDRDBRedoCnt != 0 )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    "OnlineRedo is performing, Skip Remove Log File(s)\n");

        /* OnlineDRDBRedo�� �����߿��� LogFile�� �������� �ȵȴ�.
         * Checkpoint�� ���� Flush�� ��� ����Ǿ����Ƿ�
         * Recovery ������� ������ ���Ͽ�
         * Log Anchor�� Chkpt ������ �������ֵ��� �Ѵ�. */
        mAnchorMgr.getFstDeleteLogFileNo(&sFstFileNo);
        mAnchorMgr.getLstDeleteLogFileNo(&sLstFileNo);

        IDE_TEST( mAnchorMgr.updateChkptAndFlush( aBeginChkptLSN,
                                                  aEndChkptLSN,
                                                  aDiskRedoLSN,
                                                  aRedoLSN,
                                                  &sFstFileNo,
                                                  &sLstFileNo ) != IDE_SUCCESS );

        IDE_RAISE( SKIP_REMOVE_LOGFILE );
    }
    else
    {
        /* �������� OnlineDRDBRedo�� �����Ƿ� Remove Log File�� �����Ѵ�. */
    }

    //---------------------------------
    // E-2. Get Remove Log File #
    //---------------------------------
    sGetMinSNFunc    = mGetMinSNFunc;    // bug-17388

    /* BUG-39675 : codesonar���� sFstFileNo �ʱ�ȭ warning�� �Ʒ��� DR enable
     * �� �� getFirstNeedLFN �Լ� ���� �߰ߵǾ� �̷����� ������ �����. 
     * ���� sFstFileNo�� �ʱ�ȭ�� �Ʒ� DR���� ������ �α����� ��ȣ�� ���ϴ�
     * �ڵ�� �����ϱ����� �������ش�. ���� ������ sFstFileNo�� �ʱ�ȭ �ڵ尡
     * �����ߴ� �α� ���� ���� �ڵ忡���� �����Ѵ�. 
     */
    mAnchorMgr.getLstDeleteLogFileNo( &sFstFileNo );

    if ( ( sGetMinSNFunc != NULL ) &&
         ( aFinal == ID_FALSE ) &&
         ( aRemoveLogFile == ID_TRUE ) )
    {    
        /*
          For Parallel Logging: Replication�� �ڽ��� Normal Start��
          Abnormal start �� �������� ù��° �α��� LSN���� ������ �ִ�.
          ������ LSN���� ���� ���� ������ �α׸���
          ���� LogFile�� ã�Ƽ� ������ �Ѵ�.
        */
        sEndFileNo = aRedoLSN->mFileNo;
        sEndFileNo =     // BUG-14898 Restart Redo FileNo (Disk)
            ((aDiskRedoLSN->mFileNo > sEndFileNo)
             ? sEndFileNo : aDiskRedoLSN->mFileNo);

        if ( getArchiveMode() == SMI_LOG_ARCHIVE )
        {
            // ����! �� �ȿ��� ������ Arhive�� �α����� ������ ���� Mutex�� ��´� 
            IDE_TEST( smrLogMgr::getArchiveThread().getLstArchLogFileNo( &sLstArchLogFileNo )
                      != IDE_SUCCESS );
        }

        /* FIT/ART/rp/Bugs/BUG-17388/BUG-17388.tc */
        IDU_FIT_POINT( "1.BUG-17388@smrRecoveryMgr::chkptAfterEndChkptLog" );

        // replication�� min sn���� ���Ѵ�.
        if ( sGetMinSNFunc( &sEndFileNo,        // bug-14898 restart redo FileNo
                            &sLstArchLogFileNo, // bug-29115 last archive log FileNo
                            &sMinReplicationSN )
             != IDE_SUCCESS )
        {
            /* BUG-40294
             * RP�� �ݹ� �Լ��� �����ϴ��� üũ����Ʈ ���з� ������ �ߴܵǾ �ȵ�
             * sGetMinSNFunc �Լ��� ������ ��� �α������� �����ϰ� üũ����Ʈ�� ����
             */
            IDE_ERRLOG( IDE_ERR_0 );
            ideLog::log( IDE_ERR_0, 
                         "Fail to get minimum LSN for replication.  " \
                         "The reason why sm log file is not removed when checkpoint is executing\n" );

            IDE_ERRLOG( IDE_SM_0 );
            ideLog::log( IDE_SM_0, 
                         "Fail to get minimum LSN for replication.  " \
                         "The reason why sm log file is not removed when checkpoint is executing\n" );

            mAnchorMgr.getFstDeleteLogFileNo( &sFstFileNo );
            mAnchorMgr.getLstDeleteLogFileNo( &sLstFileNo );

            IDE_TEST( mAnchorMgr.updateChkptAndFlush( aBeginChkptLSN,
                                                      aEndChkptLSN,
                                                      aDiskRedoLSN,
                                                      aRedoLSN,
                                                      &sFstFileNo,
                                                      &sLstFileNo ) != IDE_SUCCESS );

            IDE_RAISE( SKIP_REMOVE_LOGFILE );
        }
        else
        {
            /* nothing to do */
        }

        SM_MAKE_LSN( sMinReplicationLSN, sMinReplicationSN);
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP12);

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVERY_RECOVERYMGR_CHKP13,
                     sMinReplicationLSN.mFileNo,
                     sMinReplicationLSN.mOffset );

        /* Replication�� ���ؼ� �ʿ��� ���ϵ���
         * ù��° ������ ��ȣ�� ���Ѵ�. */
        if ( !SM_IS_LSN_MAX( sMinReplicationLSN ) )
        {
            (void)smrLogMgr::getFirstNeedLFN( sMinReplicationLSN,
                                              sFstFileNo,
                                              sEndFileNo,
                                              &sLstFileNo );
        }
        else
        {
            sLstFileNo = aRedoLSN->mFileNo;
        }

        sLstFileNo = ((aDiskRedoLSN->mFileNo > sLstFileNo)  ? sLstFileNo : aDiskRedoLSN->mFileNo);

        /* BUG-46754 */
        sLstFileNo = ( ( aDtxMinLSN->mFileNo > sLstFileNo ) ? sLstFileNo : aDtxMinLSN->mFileNo );

        if ( getArchiveMode() == SMI_LOG_ARCHIVE )
        {
            sLstFileNo = ( sLstArchLogFileNo > sLstFileNo ) ? sLstFileNo : sLstArchLogFileNo;

        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_RECOVERYMGR_CHKP14);

        mAnchorMgr.getFstDeleteLogFileNo(&sFstFileNo);
        mAnchorMgr.getLstDeleteLogFileNo(&sLstFileNo);
    }

    //---------------------------------
    // E-3. Update Log Anchor
    //---------------------------------

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_RECOVERYMGR_CHKP15);

    IDL_MEM_BARRIER;

    IDE_TEST( mAnchorMgr.updateStableNoOfAllMemTBSAndFlush()
              != IDE_SUCCESS );

    /* ���⼭ sEndLSN���� Normal Shutdown�ÿ��� Start�ÿ�
       Last LSN���� ���Ǳ⶧���� ���⼭�� �ܼ���
       ���ڸ� ä��� ���ؼ� �ѱ��..*/
    IDE_TEST( mAnchorMgr.updateChkptAndFlush( aBeginChkptLSN,
                                              aEndChkptLSN,
                                              aDiskRedoLSN,
                                              aRedoLSN,
                                              &sFstFileNo,
                                              &sLstFileNo ) != IDE_SUCCESS );

    /*  PROJ-2742 Support data integrity after fail-back on 1:1 consistent replication  */
   smrRecoveryMgr::updateLastRemovedFileNo( sLstFileNo ); 

    //---------------------------------
    // E-4. Remove Log Files
    //---------------------------------

    // remove log file that is not needed for restart recovery
    // (s_nFstFileNo <= deleted file < s_nLstFileNo)
    // To fix BUG-5071
    if ( aRemoveLogFile == ID_TRUE )
    {
        IDE_TEST( smrRecoveryMgr::lockDeleteLogFileMtx()
                  != IDE_SUCCESS );
        sState = 1;

        if (sFstFileNo != sLstFileNo)
        {
            if (j == 0)
            {
                ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                             SM_TRC_MRECOVERY_RECOVERYMGR_CHKP16);
            }
            else
            {
                /* nothing to do ... */
            }

            j++;
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_MRECOVERY_RECOVERYMGR_CHKP17,
                         sFstFileNo,
                         sLstFileNo - 1 );
            (void)smrLogMgr::getLogFileMgr().removeLogFile( sFstFileNo,
                                                            sLstFileNo,
                                                            ID_TRUE );

            // fix BUG-20241 : ������ LogFileNo�� LogAnchor ����ȭ
            IDE_TEST( mAnchorMgr.updateFstDeleteFileAndFlush()
                      != IDE_SUCCESS );

        }
        else
        {
            /* nothing to do ... */
        }

        if ( j == 0 )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_MRECOVERY_RECOVERYMGR_CHKP18 );
        }

        sState = 0;
        IDE_TEST( smrRecoveryMgr::unlockDeleteLogFileMtx()
                  != IDE_SUCCESS );
    }
    else
    {

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVERY_RECOVERYMGR_CHKP19 );
    }

    IDE_EXCEPTION_CONT( SKIP_REMOVE_LOGFILE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( smrRecoveryMgr::unlockDeleteLogFileMtx()
                        == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
  �̵�� ������ �ִ� �޸����̺����̽��� ����
  PREPARE �� LOADING�� ó���Ѵ�.

*/
IDE_RC smrRecoveryMgr::initMediaRecovery4MemTBS()
{
    // free page�� ���� page memory �Ҵ��� ���� �ʱ� ������
    // redo �ϴٰ� smmManager::getPersPagePtr ���� page��
    // �����ϰ� �Ǹ� recovery �����϶�
    // ��� page memory �� �Ҵ��� �� �ְ� ���־�� �Ѵ�.

    IDE_CALLBACK_SEND_MSG("  [ RECMGR ] Restoring corrupted memory tablespace checkpoint images");
    IDE_TEST( smmManager::prepareDB( SMM_PREPARE_OP_DBIMAGE_NEED_MEDIA_RECOVERY )
              != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("  [ RECMGR ] Loading memory tablespace checkpoint images from backup media ");
    IDE_TEST( smmManager::restoreDB( SMM_RESTORE_OP_DBIMAGE_NEED_MEDIA_RECOVERY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  �̵�� ������ �ִ� �޸����̺����̽��� ����
  PREPARE �� LOADING�� ó���Ѵ�.
*/
IDE_RC smrRecoveryMgr::finalMediaRecovery4MemTBS()
{
    IDE_CALLBACK_SEND_MSG(
        "  [ RECMGR ] Memory tablespace checkpoint image restoration complete. ");

    //BUG-34530 
    //SYS_TBS_MEM_DIC���̺����̽� �޸𸮰� �����Ǵ���
    //DicMemBase�����Ͱ� NULL�� �ʱ�ȭ ���� �ʽ��ϴ�.
    smmManager::clearCatalogPointers();

    IDE_TEST( smmTBSMediaRecovery::resetMediaFailureMemTBSNodes() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  �̵��� �Ϸ�� �޸� DirtyPage�� ��� Flush ��Ų��.
*/
IDE_RC smrRecoveryMgr::flushAndRemoveDirtyPagesAllMemTBS()
{
    ULong              sNewDirtyPageCnt;
    ULong              sDupDirtyPageCnt;
    ULong              sTotalCnt;
    ULong              sRemoveCnt;
    ULong              sWaitTime;
    ULong              sSyncTime;

    SChar              sMsgBuf[ SM_MAX_FILE_NAME ];

    // 1) ��� Tablespace�� ���� SMM => SMR �� Dirty Page�̵�
    IDE_TEST( smrDPListMgr::moveDirtyPages4AllTBS( SCT_SS_UNABLE_MEDIA_RECOVERY,
                                                   & sNewDirtyPageCnt,
                                                   & sDupDirtyPageCnt )
              != IDE_SUCCESS );

    // 2) Dirty Page���� Disk�� Checkpoint Image�� Flush�Ѵ�.
    // ��, Media Recovery �Ϸ����̹Ƿ� PID�α� ����
    IDE_TEST( smrDPListMgr::writeDirtyPages4AllTBS( SCT_SS_UNABLE_MEDIA_RECOVERY,
                                                    & sTotalCnt,
                                                    & sRemoveCnt,
                                                    & sWaitTime,
                                                    & sSyncTime,
                                                    smmManager::getCurrentDB, // flush target db
                                                    SMR_WDP_NO_PID_LOGGING )
              != IDE_SUCCESS );

    IDE_ASSERT( sRemoveCnt == 0 );

    idlOS::snprintf( sMsgBuf, SM_MAX_FILE_NAME,
                     "\n             Flush All Memory ( %d ) Dirty Pages.",
                     sTotalCnt );

    IDE_CALLBACK_SEND_SYM( sMsgBuf );

    // 3) ��� dirty page���� discard ��Ų��.
    // ������ tablespace�� dirty page list��reset tablespace���� ó���Ѵ�.
    IDE_TEST( smrDPListMgr::discardDirtyPages4AllTBS() != IDE_SUCCESS );

    // 4) sync db file
    // ����� ��쿡�� ��� Online Tablespace�� ���ؼ� Sync�� �����Ѵ�.
    // �̵����߿��� ��� Online/Offline TableSpace �� �̵�� ����
    // ����� �͸� Sync�� �����Ѵ�.
    IDE_TEST( smmManager::syncDB( SCT_SS_UNABLE_MEDIA_RECOVERY,
                                  ID_TRUE /* syncLatch ȹ�� �ʿ� */ )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  �޸� ����Ÿ������ ����� �����Ѵ�

  [IN] aResetLogsLSN - �ҿ��������� ResetLogsLSN
*/
IDE_RC smrRecoveryMgr::repairFailureChkptImageHdr( smLSN  * aResetLogsLSN )
{
    sctActRepairArgs   sRepairArgs;

    sRepairArgs.mResetLogsLSN = aResetLogsLSN;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  smmTBSMediaRecovery::doActRepairDBFHdr,
                                                  &sRepairArgs, /* Action Argument*/
                                                  SCT_ACT_MODE_NONE ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description  : �α������� ũ�⸦ ���ڷ� ���� ũ�⸸ŭ �������ϴ� �Լ�
 *
 * aLogFileName - [IN] ũ�⸦ �������� �α����� �̸�
 * aLogFileSize - [IN] �������� ũ��
 **********************************************************************/

IDE_RC smrRecoveryMgr::resizeLogFile(SChar    *aLogFileName,
                                     ULong     aLogFileSize)
{
#define                 SMR_NULL_BUFSIZE   (1024*1024)    // 1M

    iduFile             sFile;
    SChar             * sNullBuf      = NULL;
    ULong               sCurFileSize  = 0;
    ULong               sOffset       = 0;
    UInt                sWriteSize    = 0;
    UInt                sState        = 0;

    // Null buffer �Ҵ�
    /* smrRecoveryMgr_resizeLogFile_malloc_NullBuf.tc */
    IDU_FIT_POINT("smrRecoveryMgr::resizeLogFile::malloc::NullBuf");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                 SMR_NULL_BUFSIZE,
                                 (void**)&sNullBuf,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );
    sState = 1;

    idlOS::memset(sNullBuf, 0x00, SMR_NULL_BUFSIZE);

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( sFile.setFileName(aLogFileName) != IDE_SUCCESS );
    IDE_TEST( sFile.open() != IDE_SUCCESS );
    sState = 3;

    IDE_TEST( sFile.getFileSize(&sCurFileSize) != IDE_SUCCESS );
    sOffset = sCurFileSize;

    while(sOffset < aLogFileSize)
    {
        if ( (aLogFileSize - sOffset) < SMR_NULL_BUFSIZE )
        {
            sWriteSize = aLogFileSize - sOffset;
        }
        else
        {
            sWriteSize = SMR_NULL_BUFSIZE;
        }

        IDE_TEST( sFile.write( NULL, /* idvSQL* */
                               sOffset,
                               sNullBuf,
                               sWriteSize )
                  != IDE_SUCCESS );

        sOffset += sWriteSize;
    }

    IDE_ASSERT(sOffset == aLogFileSize);

    IDE_TEST( sFile.getFileSize(&sCurFileSize) != IDE_SUCCESS );
    IDE_ASSERT(sCurFileSize == aLogFileSize);

    sState = 2;
    IDE_TEST( sFile.close() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free((void**)sNullBuf) != IDE_SUCCESS );
    sNullBuf = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            IDE_ASSERT(sFile.close() == IDE_SUCCESS );

        case 2:
            IDE_ASSERT(sFile.destroy() == IDE_SUCCESS );

        case 1:
            IDE_ASSERT(iduMemMgr::free((void**)sNullBuf) == IDE_SUCCESS );

        default:
            break;
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * ���������� checkpoint �� �� �����Ǿ��ٸ�, restart �ð��� ���̰�, �α׸�
 * ���̱� ���� checkpoint�� �����Ѵ�.
 * �� �Լ��� �α׾��� ����ڰ� ������ �� ���� ���� ���� �ִ� ��쿡 checkpoint
 * ID_TRUE�� ���������� �ؼ� üũ����Ʈ�� ����ǰԲ� �Ѵ�.
 ******************************************************************************/
idBool smrRecoveryMgr::isCheckpointFlushNeeded(smLSN aLastWrittenLSN)
{
    smLSN  sEndLSN;
    idBool sResult = ID_FALSE;
    
    SM_LSN_MAX ( sEndLSN );

    if ( smrLogMgr::isAvailable() == ID_TRUE )
    {
        smrLogMgr::getLstLSN( &sEndLSN );

        if ( ( sEndLSN.mFileNo - aLastWrittenLSN.mFileNo ) >= 
             smuProperty::getCheckpointFlushMaxGap() )
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
        /* nothing to do ... */
    }

    return sResult;
}

/*
  fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline
  ����� �ùٸ��� Index Runtime Header �������� ����

  Restart REDO, UNDO���� �Ŀ� Offline ������ Disk Tablespace��
  Index Runtime Header ����

  Restart REDOAll�� ������, Dropped/Discarded TBS�� ���Ե�
  Table�� Runtime Entry�� Table�� Runitm Index Header�� �����Ѵ�.
  �׿ܿ� �������� ��찡 Offline ���̺����̽��ε�
  Offline�� �Ϸᰡ �Ǿ��ٸ� Restart Recovery�� �����Ͽ���
  Offline TBS�� �����Ͽ� Undo�� �߻����� �ʱ� ������
  ���� Refine DRDB Tables �������� Offline TBS�� ���Ե�
  Table���� Runtime Index Header���� �������� �ʾƵ� �ȴ�.
  ( �������� ����ϴ�. ) ������ ����, SKIP_UNDO ������
  Offline TBS�� skip ���� �ʱ� ������ Refine DRDB �ܰ迡��
  �����ϰ� �ִ�.

  �׷��Ƿ� UndoAll������ �Ϸ�� ���Ŀ� Offline TBS�� ���Ե�
  Table�� Runtime Index Header�� free ��Ų��.

 */
IDE_RC smrRecoveryMgr::finiOfflineTBS( idvSQL* aStatistics )
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                aStatistics,
                                sddDiskMgr::finiOfflineTBSAction,
                                NULL, /* Action Argument*/
                                SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2118 BUG Reporting
 *               Server Fatal ������ Signal Handler �� ȣ����
 *               Debugging ���� ����Լ�
 *
 *               �̹� altibase_dump.log �� lock�� ��� �����Ƿ�
 *               lock�� �����ʴ� trace ��� �Լ��� ����ؾ� �Ѵ�.
 *
 **********************************************************************/
void smrRecoveryMgr::writeDebugInfo()
{
    if ( ( isRestartRecoveryPhase() == ID_TRUE ) ||
        ( isMediaRecoveryPhase()   == ID_TRUE ) )
    {
        ideLog::log( IDE_DUMP_0,
                     "====================================================\n"
                     " Storage Manager Dump Info for Recovery\n"
                     "====================================================\n"
                     "isRestartRecovery : %"ID_UINT32_FMT"\n"
                     "isMediaRecovery   : %"ID_UINT32_FMT"\n"
                     "LstRedoLSN        : [ %"ID_UINT32_FMT" , %"ID_UINT32_FMT" ]\n"
                     "LstUndoLSN        : [ %"ID_UINT32_FMT" , %"ID_UINT32_FMT" ]\n"
                     "====================================================\n",
                     isRestartRecoveryPhase(),
                     isMediaRecoveryPhase(),
                     mLstRedoLSN.mFileNo,
                     mLstRedoLSN.mOffset,
                     mLstUndoLSN.mFileNo,
                     mLstUndoLSN.mOffset );

        if ( ( mCurLogPtr     != NULL ) &&
             ( mCurLogHeadPtr != NULL ) )
        {
            ideLog::log( IDE_DUMP_0,
                         "[ Cur Log Info ]\n"
                         "Log Flag    : %"ID_UINT32_FMT"\n"
                         "Log Type    : %"ID_UINT32_FMT"\n"
                         "Magic Num   : %"ID_UINT32_FMT"\n"
                         "Log Size    : %"ID_UINT32_FMT"\n"
                         "PrevUndoLSN [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n"
                         "TransID     : %"ID_UINT32_FMT"\n"
                         "ReplSPNum   : %"ID_UINT32_FMT"\n",
                         mCurLogHeadPtr->mFlag,
                         mCurLogHeadPtr->mType,
                         mCurLogHeadPtr->mMagic,
                         mCurLogHeadPtr->mSize,
                         mCurLogHeadPtr->mPrevUndoLSN.mFileNo,
                         mCurLogHeadPtr->mPrevUndoLSN.mOffset,
                         mCurLogHeadPtr->mTransID,
                         mCurLogHeadPtr->mReplSvPNumber );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)mCurLogHeadPtr,
                            ID_SIZEOF( smrLogHead ),
                            " Cur Log Header :\n" );

            if ( mCurLogHeadPtr->mSize > 0 )
            {
                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)mCurLogPtr,
                                mCurLogHeadPtr->mSize,
                                " Cur Log Data :\n" );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }
}


/**********************************************************************
 * PROJ-2162 RestartRiskReduction
 ***********************************************************************/

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * TOI ��ü�� �ʱ�ȭ�մϴ�.
 *
 * aObj         - [OUT] �ʱ�ȭ�� ��ü
 ***********************************************************************/
void smrRecoveryMgr::initRTOI( smrRTOI * aObj )
{
    IDE_DASSERT( aObj != NULL );

    SM_LSN_INIT( aObj->mCauseLSN );
    SC_MAKE_NULL_GRID( aObj->mGRID );
    aObj->mCauseDiskPage  = NULL;
    aObj->mCause          = SMR_RTOI_CAUSE_INIT;
    aObj->mType           = SMR_RTOI_TYPE_INIT;
    aObj->mState          = SMR_RTOI_STATE_INIT;
    aObj->mTableOID       = SM_NULL_OID;
    aObj->mIndexID        = 0;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * Log�� �޾� �м��Ͽ� �� Log�� Recovery�� ��� ��ü���� �����մϴ�.
 *
 * DML�α״� Page�� ���� Inconsistent���� �����ϰ�,
 * DDL�� Table/Index�� ���� Inconsistent���� �����մϴ�.
 *
 * aLogPtr      - [IN]  �����ϰԵ� Log ( Align �ȵ� )
 * aLogHeadPtr  - [IN]  �����ϰԵ� Log�� Head ( Align�Ǿ����� )
 * aLSN         - [IN]  �����ϰԵ� Log�� LSN
 * aRedoLogData - [IN]  DRDB Redo ���̾��� ���, ����� redoLog�� �����
 * aPagePtr     - [IN]  DRDB Redo ���̾��� ���, ��� Page�� ��
 * aIsRedo      - [IN]  Redo���ΰ�?
 * aObj         - [OUT] ������ �����
 ***********************************************************************/
void smrRecoveryMgr::prepareRTOI( void                * aLogPtr,
                                  smrLogHead          * aLogHeadPtr,
                                  smLSN               * aLSN,
                                  sdrRedoLogData      * aRedoLogData,
                                  UChar               * aPagePtr,
                                  idBool                aIsRedo,
                                  smrRTOI             * aObj )
{
    smrUpdateType        sType;
    smrOPType            sOPType;
    ULong                sData1;
    UInt                 sRefOffset;

    IDE_DASSERT( aLSN != NULL );
    IDE_DASSERT( aObj != NULL );
    IDE_DASSERT( ( ( aLogHeadPtr == NULL ) && ( aLogPtr == NULL ) ) ||
                 ( ( aLogHeadPtr != NULL ) && ( aLogPtr != NULL ) ) );

    /* �ʱ�ȭ */
    initRTOI( aObj );

    /* smrRecoveryMgr::redo���� Log�м��� ȣ���ϴ� ��� */
    /* LogHeadPtr�� �����Ǽ� �´�. */
    if ( ( aLogHeadPtr != NULL ) && ( aLogPtr != NULL ) )
    {
        switch( smrLogHeadI::getType(aLogHeadPtr) )
        {
        case SMR_LT_UPDATE:
            /* MMDB DML, DDL ����.*/
            idlOS::memcpy(
                &(aObj->mGRID),
                ( (SChar*) aLogPtr) + offsetof( smrUpdateLog, mGRID ),
                ID_SIZEOF( scGRID ) );
            idlOS::memcpy(
                &sType,
                ( (SChar*) aLogPtr) + offsetof( smrUpdateLog, mType ),
                ID_SIZEOF( smrUpdateType ) );


            /* Page �ʱ�ȭ, Table�ʱ�ȭ�� Consistency Check�� �ϸ� �ȵǴ� ���
             * �� �ִ�. �̷��� ���� ������ ����.
             *
             * Case 1) �ʱ�ȭ Log( OverwriteLog����) �� ������ �ش� ��ü��
             *    � �������� ������ ���
             * -> Check�� �ʿ䵵 ����, Recovery�����ϴ��� inconsistentFlag
             *  ���� ����. Object�� �߸��Ǿ��⿡ Inconsistent���� ����
             *  ������ �� ����. ( DONE���� �ٷ� �� )
             * Case 2) 1�� ���������, ConsistencyFlag ������ ������ ���
             *    ( CHECKED�� �ٷ� �� )
             * Case 3) Inconsistency Flag���� �����ϱ� ���� Log�� ���
             *    ( DONE ���·� ��. )
             * Case 4) Log�����δ� TargetObject�� �� �� ���� ���
             *    ( DONE ���·� ��. )
             * �� ��쿡 ���ؼ��� üũ�� ���� �ʴ´�. */
            switch( sType )
            {
            case SMR_SMC_TABLEHEADER_INIT:              /* Case 1 */
            case SMR_SMM_MEMBASE_SET_SYSTEM_SCN:
            case SMR_SMM_MEMBASE_ALLOC_PERS_LIST:
            case SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK:
            case SMR_SMM_PERS_UPDATE_LINK:
            case SMR_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK:
            case SMR_SMM_MEMBASE_INFO:
                aObj->mState = SMR_RTOI_STATE_DONE;
                break;
            case SMR_SMC_PERS_INIT_FIXED_PAGE:          /* Case 2*/
            case SMR_SMC_PERS_INIT_VAR_PAGE:
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType           = SMR_RTOI_TYPE_MEMPAGE;
                aObj->mState          = SMR_RTOI_STATE_CHECKED;
                break;
            case SMR_SMC_PERS_SET_INCONSISTENCY:
            case SMR_SMC_TABLEHEADER_SET_INCONSISTENCY: /* Case 3 */
                aObj->mState = SMR_RTOI_STATE_DONE;
                break;
            case SMR_PHYSICAL:
            case SMR_SMC_TABLEHEADER_UPDATE_ALLOCINFO:
            case SMR_SMC_TABLEHEADER_UPDATE_INDEX:      /* Case 4 */
                /* Index Create/Drop�� ����Ǵ� Physical Log
                 * ����Ǵ� Index�� ���� ������ ȹ���� �� ����,
                 * index ���� ������ ���õ� ������ ���дٺ���
                 * Inconsistent�� ������ ��ü�� ����. ���� �˻� ����*/
                aObj->mState = SMR_RTOI_STATE_DONE;
                break;
            case SMR_SMC_TABLEHEADER_UPDATE_COLUMNS:
            case SMR_SMC_TABLEHEADER_UPDATE_INFO:
            case SMR_SMC_TABLEHEADER_SET_NULLROW:
            case SMR_SMC_TABLEHEADER_UPDATE_ALL:
            case SMR_SMC_TABLEHEADER_UPDATE_FLAG:
            case SMR_SMC_TABLEHEADER_UPDATE_COLUMN_COUNT:
            case SMR_SMC_TABLEHEADER_SET_SEQUENCE:
            case SMR_SMC_TABLEHEADER_UPDATE_TABLE_SEGMENT:
            case SMR_SMC_TABLEHEADER_SET_SEGSTOATTR:
            case SMR_SMC_TABLEHEADER_SET_INSERTLIMIT:
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType  = SMR_RTOI_TYPE_TABLE;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                break;
            case SMR_SMC_INDEX_SET_FLAG:
            case SMR_SMC_INDEX_SET_SEGATTR:
            case SMR_SMC_INDEX_SET_SEGSTOATTR:
            case SMR_SMC_INDEX_SET_DROP_FLAG:
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType  = SMR_RTOI_TYPE_INDEX;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                break;
            case SMR_SMC_PERS_INIT_FIXED_ROW:
            case SMR_SMC_PERS_UPDATE_FIXED_ROW:
            case SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION:
            case SMR_SMC_PERS_SET_FIX_ROW_DROP_FLAG:
            case SMR_SMC_PERS_SET_FIX_ROW_DELETE_BIT:
            case SMR_SMC_PERS_UPDATE_VAR_ROW_HEAD:
            case SMR_SMC_PERS_UPDATE_VAR_ROW:
            case SMR_SMC_PERS_SET_VAR_ROW_FLAG:
            case SMR_SMC_PERS_SET_VAR_ROW_NXT_OID:
            case SMR_SMC_PERS_WRITE_LOB_PIECE:
            case SMR_SMC_PERS_INSERT_ROW:
            case SMR_SMC_PERS_UPDATE_INPLACE_ROW:
            case SMR_SMC_PERS_UPDATE_VERSION_ROW:
            case SMR_SMC_PERS_DELETE_VERSION_ROW:
            /* PROJ-2429 */
            case SMR_SMC_SET_CREATE_SCN:
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType  = SMR_RTOI_TYPE_MEMPAGE;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                break;
            default:
                IDE_DASSERT( 0 );
                break;
            }
            break;
        case SMR_LT_NTA:
            /* MMDB DDL, AllocSlot ����.*/
            idlOS::memcpy(
                &sData1,
                ( (SChar*) aLogPtr) + offsetof( smrNTALog, mData1 ),
                ID_SIZEOF( ULong ) );
            idlOS::memcpy(
                &sOPType,
                ( (SChar*) aLogPtr) + offsetof( smrNTALog, mOPType ),
                ID_SIZEOF( smrOPType ) );

            switch( sOPType )
            {
            case SMR_OP_NULL:
            case SMR_OP_SMC_TABLEHEADER_ALLOC:           /* Case 2 */
            case SMR_OP_CREATE_TABLE:
                SC_MAKE_GRID( aObj->mGRID,
                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                              SM_MAKE_PID( sData1 ),
                              SM_MAKE_OFFSET( sData1 ) );
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType  = SMR_RTOI_TYPE_TABLE;
                aObj->mState = SMR_RTOI_STATE_CHECKED;
                break;
            case SMR_OP_SMM_PERS_LIST_ALLOC:
            case SMR_OP_SMC_FIXED_SLOT_ALLOC:
            case SMR_OP_CREATE_INDEX:
            case SMR_OP_DROP_INDEX:
            case SMR_OP_INIT_INDEX:
            case SMR_OP_SMC_FIXED_SLOT_FREE:
            case SMR_OP_SMC_VAR_SLOT_FREE:
            case SMR_OP_ALTER_TABLE:
            case SMR_OP_SMM_CREATE_TBS:
            case SMR_OP_INSTANT_AGING_AT_ALTER_TABLE: /* Case 3 */
            case SMR_OP_DIRECT_PATH_INSERT:
                aObj->mState = SMR_RTOI_STATE_DONE;
                break;
            default:
                IDE_DASSERT( 0 );
                break;
            }
            break;
        case SMR_DLT_UNDOABLE:
            if ( aIsRedo == ID_TRUE )
            {
                /* DRDB Redo�� ���ؼ��� smrRecoveryMgr::redo�� �ƴ϶�
                 * sdrRedoMgr::applyLogRec���� �����Ѵ�. ���⼭ �ϸ�
                 * �ߺ� ������ �ǹ�����.
                 * �ٸ� Undo�� sdrRedoMgr�� DRDB������ �ƴ϶� ��������
                 * �����ϱ� ������, Undo�� �α״� ���⼭ �м��Ѵ�. */
                aObj->mState = SMR_RTOI_STATE_DONE;
            }
            else
            {
                idlOS::memcpy( &sRefOffset,
                               ( (SChar*) aLogPtr) +
                               offsetof( smrDiskLog, mRefOffset ),
                               ID_SIZEOF( UInt ) );
                idlOS::memcpy( &aObj->mGRID,
                               ( (SChar*) aLogPtr) +
                               SMR_LOGREC_SIZE(smrDiskLog) +
                               sRefOffset +
                               offsetof( sdrLogHdr,mGRID ),
                               ID_SIZEOF( scGRID ) );
                aObj->mType  = SMR_RTOI_TYPE_DISKPAGE;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
            }
            break;
        case SMR_DLT_REF_NTA:
            if ( aIsRedo == ID_TRUE )
            {
                /* �� SMR_DLT_UNDOABLE�� ���� ������ ������ */
                aObj->mState = SMR_RTOI_STATE_DONE;
            }
            else
            {
                idlOS::memcpy( &sRefOffset,
                               ( (SChar*) aLogPtr) +
                               offsetof( smrDiskRefNTALog, mRefOffset ),
                               ID_SIZEOF( UInt ) );
                idlOS::memcpy( &aObj->mGRID,
                               ( (SChar*) aLogPtr) +
                               SMR_LOGREC_SIZE(smrDiskRefNTALog) +
                               sRefOffset +
                               offsetof( sdrLogHdr,mGRID ),
                               ID_SIZEOF( scGRID ) );
                aObj->mType  = SMR_RTOI_TYPE_DISKPAGE;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
            }
            break;
        case SMR_LT_COMPENSATION:
        case SMR_LT_NULL:
        case SMR_LT_DUMMY:
        case SMR_LT_CHKPT_BEGIN:
        case SMR_LT_DIRTY_PAGE:
        case SMR_LT_CHKPT_END:
        case SMR_LT_MEMTRANS_GROUPCOMMIT:
        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_MEMTRANS_ABORT:
        case SMR_LT_DSKTRANS_COMMIT:
        case SMR_LT_DSKTRANS_ABORT:
        case SMR_LT_SAVEPOINT_SET:
        case SMR_LT_SAVEPOINT_ABORT:
        case SMR_LT_XA_PREPARE:
        case SMR_LT_TRANS_PREABORT:
        case SMR_LT_DDL:
        case SMR_LT_XA_SEGS:
        case SMR_LT_LOB_FOR_REPL:
        case SMR_LT_DDL_QUERY_STRING:
        case SMR_LT_DUMMY_COMPENSATION:
        case SMR_LT_FILE_BEGIN:
        case SMR_LT_TBS_UPDATE:
        case SMR_LT_FILE_END:
        case SMR_DLT_REDOONLY:
        case SMR_DLT_NTA:
        case SMR_DLT_COMPENSATION:
        case SMR_LT_TABLE_META:
        case SMR_LT_XA_START_REQ:
        case SMR_LT_XA_PREPARE_REQ:
        case SMR_LT_XA_END:
            aObj->mState = SMR_RTOI_STATE_DONE;
            break;
        default:
            IDE_DASSERT( 0 );
            break;
        }
    }
    else
    {
        /* nothing to do ... */
    }

    /* DRDB�� ���� �м��ϴ� ��� */
    if ( aRedoLogData != NULL )
    {
        IDE_DASSERT( aPagePtr != NULL );

        if ( aPagePtr != NULL )
        {
            aObj->mCauseDiskPage  = aPagePtr;

            IDE_DASSERT( (aRedoLogData->mOffset == SC_NULL_OFFSET) ||
                         (aRedoLogData->mSlotNum == SC_NULL_SLOTNUM) );

            if ( aRedoLogData->mOffset == SC_NULL_OFFSET )
            {
                SC_MAKE_GRID_WITH_SLOTNUM( aObj->mGRID,
                                           aRedoLogData->mSpaceID,
                                           aRedoLogData->mPageID,
                                           aRedoLogData->mSlotNum );
            }
            else
            {
                SC_MAKE_GRID( aObj->mGRID,
                              aRedoLogData->mSpaceID,
                              aRedoLogData->mPageID,
                              aRedoLogData->mOffset );
            }

            /* ���� sdrRedoMgr::checkByDiskLog�� ���� ���  */
            /* Page���� Physical Log�̳� InitPage Log���� PageConsistentFlag
             * �� �������. */
            switch( aRedoLogData->mType )
            {
            case SDR_SDP_BINARY:                  /* Case 1*/
            case SDR_SDP_PAGE_CONSISTENT:
            case SDR_SDP_INIT_PHYSICAL_PAGE:
            case SDR_SDP_WRITE_PAGEIMG:
            case SDR_SDP_WRITE_DPATH_INS_PAGE:
                aObj->mState = SMR_RTOI_STATE_DONE;
                break;
            case SDR_SDP_1BYTE:
            case SDR_SDP_2BYTE:
            case SDR_SDP_4BYTE:
            case SDR_SDP_8BYTE:
            case SDR_SDP_INIT_LOGICAL_HDR:
            case SDR_SDP_INIT_SLOT_DIRECTORY:
            case SDR_SDP_FREE_SLOT:
            case SDR_SDP_FREE_SLOT_FOR_SID:
            case SDR_SDP_RESTORE_FREESPACE_CREDIT:
            case SDR_SDP_RESET_PAGE:
            case SDR_SDPST_INIT_SEGHDR:
            case SDR_SDPST_INIT_BMP:
            case SDR_SDPST_INIT_LFBMP:
            case SDR_SDPST_INIT_EXTDIR:
            case SDR_SDPST_ADD_RANGESLOT:
            case SDR_SDPST_ADD_SLOTS:
            case SDR_SDPST_ADD_EXTDESC:
            case SDR_SDPST_ADD_EXT_TO_SEGHDR:
            case SDR_SDPST_UPDATE_WM:
            case SDR_SDPST_UPDATE_MFNL:
            case SDR_SDPST_UPDATE_PBS:
            case SDR_SDPST_UPDATE_LFBMP_4DPATH:
            case SDR_SDPSC_INIT_SEGHDR:
            case SDR_SDPSC_INIT_EXTDIR:
            case SDR_SDPSC_ADD_EXTDESC_TO_EXTDIR:
            case SDR_SDPTB_INIT_LGHDR_PAGE:
            case SDR_SDPTB_ALLOC_IN_LG:
            case SDR_SDPTB_FREE_IN_LG:
            case SDR_SDC_INSERT_ROW_PIECE:
            case SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE:
            case SDR_SDC_INSERT_ROW_PIECE_FOR_DELETEUNDO:
            case SDR_SDC_UPDATE_ROW_PIECE:
            case SDR_SDC_OVERWRITE_ROW_PIECE:
            case SDR_SDC_CHANGE_ROW_PIECE_LINK:
            case SDR_SDC_DELETE_FIRST_COLUMN_PIECE:
            case SDR_SDC_ADD_FIRST_COLUMN_PIECE:
            case SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE:
            case SDR_SDC_DELETE_ROW_PIECE:
            case SDR_SDC_LOCK_ROW:
            case SDR_SDC_PK_LOG:
            case SDR_SDC_INIT_CTL:
            case SDR_SDC_EXTEND_CTL:
            case SDR_SDC_BIND_CTS:
            case SDR_SDC_UNBIND_CTS:
            case SDR_SDC_BIND_ROW:
            case SDR_SDC_UNBIND_ROW:
            case SDR_SDC_ROW_TIMESTAMPING:
            case SDR_SDC_DATA_SELFAGING:
            case SDR_SDC_BIND_TSS:
            case SDR_SDC_UNBIND_TSS:
            case SDR_SDC_SET_INITSCN_TO_TSS:
            case SDR_SDC_INIT_TSS_PAGE:
            case SDR_SDC_INIT_UNDO_PAGE:
            case SDR_SDC_INSERT_UNDO_REC:
            case SDR_SDN_INSERT_INDEX_KEY:
            case SDR_SDN_FREE_INDEX_KEY:
            case SDR_SDN_INSERT_UNIQUE_KEY:
            case SDR_SDN_INSERT_DUP_KEY:
            case SDR_SDN_DELETE_KEY_WITH_NTA:
            case SDR_SDN_FREE_KEYS:
            case SDR_SDN_COMPACT_INDEX_PAGE:
            case SDR_SDN_KEY_STAMPING:
            case SDR_SDN_INIT_CTL:
            case SDR_SDN_EXTEND_CTL:
            case SDR_SDN_FREE_CTS:
            case SDR_SDC_LOB_UPDATE_LOBDESC:
            case SDR_SDC_LOB_INSERT_INTERNAL_KEY:
            case SDR_SDC_LOB_INSERT_LEAF_KEY:
            case SDR_SDC_LOB_UPDATE_LEAF_KEY:
            case SDR_SDC_LOB_OVERWRITE_LEAF_KEY:
            case SDR_SDC_LOB_FREE_INTERNAL_KEY:
            case SDR_SDC_LOB_FREE_LEAF_KEY:
            case SDR_SDC_LOB_WRITE_PIECE:
            case SDR_SDC_LOB_WRITE_PIECE4DML:
            case SDR_SDC_LOB_WRITE_PIECE_PREV:
            case SDR_SDC_LOB_ADD_PAGE_TO_AGINGLIST:
            case SDR_STNDR_INSERT_INDEX_KEY:
            case SDR_STNDR_UPDATE_INDEX_KEY:
            case SDR_STNDR_FREE_INDEX_KEY:
            case SDR_STNDR_INSERT_KEY:
            case SDR_STNDR_DELETE_KEY_WITH_NTA:
            case SDR_STNDR_FREE_KEYS:
            case SDR_STNDR_COMPACT_INDEX_PAGE:
            case SDR_STNDR_KEY_STAMPING:
                SM_GET_LSN( aObj->mCauseLSN, *aLSN );
                aObj->mType  = SMR_RTOI_TYPE_DISKPAGE;
                aObj->mState = SMR_RTOI_STATE_PREPARED;
                break;
            default:
                IDE_ASSERT( 0 );
                break;
            }
        }
    }

    IDE_DASSERT( aObj->mState != SMR_RTOI_STATE_INIT );
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * TargetObject�� �޾� Inconsistent���� üũ.
 *
 * aObj         - [IN] �˻��� ���
 * aConsistency - [OUT] �˻� ���
 ***********************************************************************/
void smrRecoveryMgr::checkObjectConsistency( smrRTOI * aObj,
                                             idBool  * aConsistency )
{
    idBool              sConsistency;

    IDE_DASSERT( aObj         != NULL );
    IDE_DASSERT( aConsistency != NULL );

    sConsistency = ID_TRUE;

    /* �˻��� �ʿ䰡 �ִ°�? */
    if ( ( aObj->mState == SMR_RTOI_STATE_PREPARED ) &&
         ( isSkipRedo( aObj->mGRID.mSpaceID, ID_TRUE ) == ID_FALSE ) )
    {
        aObj->mCause = SMR_RTOI_CAUSE_INIT;

        /* Property�� ���� �ش� Log, Table, Index Page�� ������ */
        if ( isIgnoreObjectByProperty( aObj ) == ID_TRUE )
        {
            aObj->mCause  = SMR_RTOI_CAUSE_PROPERTY;
            sConsistency  = ID_FALSE;

            if ( findIOL( aObj ) == ID_TRUE )
            {
                /* �̹� ������ ��� */
                aObj->mState = SMR_RTOI_STATE_DONE;
            }
            else
            {
                /* ���� ��� �ȵǾ��� */
                aObj->mState  = SMR_RTOI_STATE_CHECKED;
            }
        }
        else
        {
            sConsistency = checkObjectConsistencyInternal( aObj );
        }
    }

    (*aConsistency) = sConsistency;

    return;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * Property���� �����϶�� ������ ��ü���� Ȯ���մϴ�.
 *
 * aObj         - [IN] �˻��� ���
 ***********************************************************************/
idBool smrRecoveryMgr::isIgnoreObjectByProperty( smrRTOI * aObj )
{
    scSpaceID           sSpaceID;
    scPageID            sPageID;
    UInt                i;

    sSpaceID = aObj->mGRID.mSpaceID;
    sPageID  = aObj->mGRID.mPageID;

    if ( !SM_IS_LSN_INIT( aObj->mCauseLSN ) )
    {
        for ( i = 0 ; i < smuProperty::getSmIgnoreLog4EmergencyCount() ; i ++)
        {
            if ( ( aObj->mCauseLSN.mFileNo == smuProperty::getSmIgnoreFileNo4Emergency(i) ) &&
                 ( aObj->mCauseLSN.mOffset == smuProperty::getSmIgnoreOffset4Emergency(i) ) )
            {
                return ID_TRUE;
            }
        }
    }

    if ( aObj->mTableOID != SM_NULL_OID )
    {
        for( i = 0 ; i < smuProperty::getSmIgnoreTable4EmergencyCount() ; i ++)
        {
            if ( aObj->mTableOID == smuProperty::getSmIgnoreTable4Emergency(i) )
            {
                return ID_TRUE;
            }
        }
    }

    if ( aObj->mIndexID != 0 )
    {
        for( i = 0 ; i < smuProperty::getSmIgnoreIndex4EmergencyCount() ; i ++)
        {
            if ( aObj->mIndexID == smuProperty::getSmIgnoreIndex4Emergency(i) )
            {
                return ID_TRUE;
            }
        }
    }
    if ( ( sSpaceID != 0 ) || ( sPageID != 0 ) )
    {
        for( i = 0 ; i < smuProperty::getSmIgnorePage4EmergencyCount() ; i ++)
        {
            if ( ( ( (ULong)sSpaceID << 32 ) + sPageID ==
                  smuProperty::getSmIgnorePage4Emergency( i ) ) )
            {
                return ID_TRUE;
            }
        }
    }

    return ID_FALSE;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * TargetObject�� Inconsistency ���� üũ�ϴ� ���� �Լ�.
 *   Object�� Inconsistent Flag�� ������ ��쿡��, �� ������ �ʿ䰡
 * ���� ������ Done���·� ����.
 *   ������ Object�� �̻��� ���, Check�ؾ� �Ѵ�.
 *
 * aObj        - [IN] �˻��� ���
 ***********************************************************************/
idBool smrRecoveryMgr::checkObjectConsistencyInternal( smrRTOI * aObj )
{
    void              * sTable;
    void              * sIndex;
    UChar             * sDiskPagePtr;
    smpPersPageHeader * sMemPagePtr;
    scSpaceID           sSpaceID;
    scPageID            sPageID;
    smOID               sOID;
    smLSN               sPageLSN;
    idBool              sTrySuccess;
    idBool              sGetPage = ID_FALSE;
    smmTBSNode        * sTBSNode = NULL;
    idBool              sIsFreePage;

    /* ���� ��ü�� �����Ͽ� Ȯ���� */
    sSpaceID = aObj->mGRID.mSpaceID;
    sPageID  = aObj->mGRID.mPageID;
    sOID     = SM_MAKE_OID( aObj->mGRID.mPageID,
                            aObj->mGRID.mOffset );

    switch( aObj->mType )
    {
    case SMR_RTOI_TYPE_TABLE:
    case SMR_RTOI_TYPE_INDEX:
    case SMR_RTOI_TYPE_MEMPAGE:
        IDE_TEST_RAISE( sctTableSpaceMgr::findSpaceNodeBySpaceID( sSpaceID,
                                                                  (void**)&sTBSNode ) 
                        != IDE_SUCCESS, err_find_object_inconsistency);
        IDE_TEST_RAISE( smmExpandChunk::isFreePageID( sTBSNode,
                                                      sPageID,
                                                      &sIsFreePage )
                        != IDE_SUCCESS, err_find_object_inconsistency);
        if ( sIsFreePage == ID_TRUE )
        {
            /* Free�� Page. ������ ������ */
            aObj->mState = SMR_RTOI_STATE_CHECKED;
        }
    case SMR_RTOI_TYPE_INIT:
    case SMR_RTOI_TYPE_DISKPAGE:
        break;
    }

    if ( aObj->mState == SMR_RTOI_STATE_CHECKED )
    {
        /* ������ �̹� �˻��� */
    }
    else
    {
        switch( aObj->mType )
        {
        case SMR_RTOI_TYPE_TABLE:
            IDE_TEST_RAISE( smmManager::getOIDPtr( sSpaceID,
                                                   sOID,
                                                   (void**) &sTable )
                            != IDE_SUCCESS,
                            err_find_object_inconsistency);
            aObj->mTableOID = smLayerCallback::getTableOID( sTable );
            /* PROJ-2375 Global Meta BUG-36236, 37915 �ӽ÷� ���� */
            //IDE_DASSERT( aObj->mTableOID + SMP_SLOT_HEADER_SIZE  == sOID );

            IDE_TEST_RAISE( smLayerCallback::isTableConsistent( (void*)sTable )
                            == ID_FALSE,
                            err_inconsistent_flag );
            break;
        case SMR_RTOI_TYPE_INDEX:
            IDE_TEST_RAISE( smmManager::getOIDPtr( sSpaceID,
                                                   sOID,
                                                   (void**) &sIndex )
                            != IDE_SUCCESS,
                            err_find_object_inconsistency);
            aObj->mTableOID = smLayerCallback::getTableOIDOfIndexHeader( sIndex );
            aObj->mIndexID  = smLayerCallback::getIndexIDOfIndexHeader( sIndex );
            IDE_TEST_RAISE( smLayerCallback::getIsConsistentOfIndexHeader( sIndex )
                            == ID_FALSE,
                            err_inconsistent_flag );
            break;
        case SMR_RTOI_TYPE_MEMPAGE:
            IDE_TEST_RAISE( smmManager::getPersPagePtr( sSpaceID,
                                                        sPageID,
                                                        (void**) &sMemPagePtr )
                            != IDE_SUCCESS,
                            err_find_object_inconsistency);
            aObj->mTableOID = smLayerCallback::getTableOID4MRDBPage( sMemPagePtr );
            IDE_ASSERT( sMemPagePtr->mSelfPageID == sPageID );
            IDE_TEST_RAISE( SMP_GET_PERS_PAGE_INCONSISTENCY( sMemPagePtr )
                            == SMP_PAGEINCONSISTENCY_TRUE,
                            err_inconsistent_flag );
            break;
        case SMR_RTOI_TYPE_DISKPAGE:
            if ( aObj->mCauseDiskPage == NULL )
            {
                /* Undo �ÿ��� getPage�ؾ��� */
                IDE_TEST_RAISE( sdbBufferMgr::getPage( NULL, // idvSQL
                                                       sSpaceID,
                                                       sPageID,
                                                       SDB_S_LATCH,
                                                       SDB_WAIT_NORMAL,
                                                       SDB_SINGLE_PAGE_READ,
                                                       &sDiskPagePtr,
                                                       &sTrySuccess )
                                != IDE_SUCCESS,
                                err_find_object_inconsistency);
                IDE_TEST_RAISE( sTrySuccess == ID_FALSE,
                                err_find_object_inconsistency);
                sGetPage = ID_TRUE;
            }
            else
            {
                /* sdrRedoMgr::applyLogRecList���� ���� ����
                 * �̹� getpage�Ǿ� �ְ�, �˾Ƽ� release��  */
                sDiskPagePtr = aObj->mCauseDiskPage;
            }
            /* DRDB Consistency�� ���� ��Ȳ�̸� ������ Logging/Flush ��� 
             * ������.  �ٸ� WAL�� ���� �������� �����Ͽ� Startup�� ������ ��
             * �ִ�. mLstDRDBRedoLSN ���� UpdateLSN�� ũ�� WAL�� ���� 
             * ��Ȳ�̴�. */
            sPageLSN = sdpPhyPage::getPageLSN( sDiskPagePtr );
            IDE_TEST_RAISE( ( mDRDBConsistency == ID_FALSE ) &&
                            ( smrCompareLSN::isLT( &mLstDRDBRedoLSN, 
                                                   &sPageLSN ) ),
                            err_find_object_inconsistency);

            IDE_TEST_RAISE( smLayerCallback::isConsistentPage4DRDB( sDiskPagePtr )
                            == ID_FALSE,
                            err_inconsistent_flag );
            if ( aObj->mCauseDiskPage == NULL )
            {
                sGetPage = ID_FALSE;
                IDE_TEST_RAISE( sdbBufferMgr::releasePage( NULL, // idvSQL
                                                           sDiskPagePtr )
                                != IDE_SUCCESS,
                                err_find_object_inconsistency);
            }
            else
            {
                /* ȣ���� sdrRedoMgr���� �˾Ƽ� release */
            }
            break;
        default:
            IDE_DASSERT( 0 );
            break;
        }

        /* Check �Ϸ� */
        aObj->mState = SMR_RTOI_STATE_CHECKED;
    }

    return ID_TRUE;

    IDE_EXCEPTION( err_inconsistent_flag );
    {
        /* Inconsistent Flag�� �̹� ������ ���, �� ���� ���ص� ��. */
        aObj->mState = SMR_RTOI_STATE_DONE;
        aObj->mCause = SMR_RTOI_CAUSE_OBJECT;
    }
    IDE_EXCEPTION( err_find_object_inconsistency );
    {
        /* ���Ӱ� �߸��� ���� ã�� ���. Flag ���� �ؾ� ��*/
        aObj->mState = SMR_RTOI_STATE_CHECKED;
        aObj->mCause = SMR_RTOI_CAUSE_OBJECT;
    }
    IDE_EXCEPTION_END;

    if ( sGetPage == ID_TRUE )
    {
        (void) sdbBufferMgr::releasePage( NULL, // idvSQL
                                          sDiskPagePtr );
    }

    return ID_FALSE;

}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * startup�� ������ ��Ȳ�� ���� ��ó�մϴ�.
 *
 * aObj         - [IN] �˻��� ���
 * aisRedo      - [IN] Redo���ΰ�?
 ***********************************************************************/
IDE_RC smrRecoveryMgr::startupFailure( smrRTOI * aObj,
                                       idBool    aIsRedo )
{
    IDE_DASSERT( aObj->mCause != SMR_RTOI_CAUSE_INIT );

    /* Check�Ǿ��־�߸�, Inconsistent ������ �ǹ̰� ����. */
    if ( aObj->mState == SMR_RTOI_STATE_CHECKED )
    {
        if ( aObj->mCause == SMR_RTOI_CAUSE_PROPERTY )
        {
            /* Property�� ���� ����ó���� ���� ����ڰ� �ش� ��ü��
             * �ս��� �����Ѵٴ� ���̱⿡ Durability �ս��� ���� */
        }
        else
        {
            /* Recovery�� �����ϴ� �ս� �߻� */
            mDurability = ID_FALSE;
        }

        /* RECOVERY��å�� Normal�� ���, ���� ���н�Ŵ. */
        IDE_TEST_RAISE( ( mEmerRecovPolicy == SMR_RECOVERY_NORMAL ) &&
                        ( mDurability == ID_FALSE ),
                        err_durability );

        setObjectInconsistency( aObj, aIsRedo );
        addIOL( aObj );
    }
    else
    {
        /* Inconsistent ���� ����  */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_durability );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FAILURE_DURABILITY_AT_STARTUP ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * TargetObject�� Inconsistent�ϴٰ� ������
 * ����� Inconsistent ������, �׻� NULL TID, �� DUMMY TX�� �����.
 * CLR�� ����� Undo���� TX�� �̿��� �� ���� ����.
 *
 * aObj         - [IN] �˻��� ���
 * aisRedo      - [IN] Redo���ΰ�?
 **********************************************************************/
void smrRecoveryMgr::setObjectInconsistency( smrRTOI * aObj,
                                             idBool    aIsRedo )
{
    scSpaceID   sSpaceID;
    scPageID    sPageID;
    SChar       sBuffer[ SMR_MESSAGE_BUFFER_SIZE ];


    /* Checked�� ���¿����� ������ �ʿ䰡 ����. */
    if ( aObj->mState == SMR_RTOI_STATE_CHECKED )
    {
        if ( aIsRedo == ID_TRUE )
        {
            /* Redo���������� �ٷ� �����ϸ� Log�� ����Ǵ� �̷��.
             * ����صθ� ���߿� Redo������ �˾Ƽ� �����Ѵ�.*/
            if ( aObj->mCauseDiskPage != NULL )
            {
                /* �� DRDB Page�� ��� �ٷ� �����Ѵ�. DRDB�� Page Locality��
                 * �߿��ϱ� ������, ���ɻ� ������ �ֱ� �����̴�. */
                ((sdpPhyPageHdr*)aObj->mCauseDiskPage)->mIsConsistent = ID_FALSE;
            }

        }
        else
        {
            sSpaceID = aObj->mGRID.mSpaceID;
            sPageID  = aObj->mGRID.mPageID;

            switch( aObj->mType )
            {
            case SMR_RTOI_TYPE_TABLE:
                IDE_TEST( smLayerCallback::setTableHeaderInconsistency( aObj->mTableOID )
                          != IDE_SUCCESS );
                break;
            case SMR_RTOI_TYPE_INDEX:
                IDE_TEST( smLayerCallback::setIndexInconsistency( aObj->mTableOID,
                                                                  aObj->mIndexID )
                          != IDE_SUCCESS );
                break;
            case SMR_RTOI_TYPE_MEMPAGE:
                IDE_TEST( smLayerCallback::setPersPageInconsistency4MRDB( sSpaceID,
                                                                          sPageID )
                          != IDE_SUCCESS );
                break;
            case SMR_RTOI_TYPE_DISKPAGE:
                IDE_TEST( smLayerCallback::setPageInconsistency4DRDB( sSpaceID,
                                                                      sPageID )
                          != IDE_SUCCESS );
                break;
            default:
                IDE_DASSERT( 0 );
                break;
            }

            aObj->mState = SMR_RTOI_STATE_DONE;
        }
    }
    else
    {
        /* �̹� Flag ������ ��� */
    }

    return;

    /* Inconsistency ������ ������ ���.
     * ��·�� Startup�� �ؾ� �ϱ� ������, �����ϰ� �����Ѵ�. */

    IDE_EXCEPTION_END;

    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     SM_TRC_EMERECO_FAILED_SET_INCONSISTENCY_FLAG );

    ideLog::log( IDE_ERR_0, sBuffer );
    IDE_CALLBACK_SEND_MSG( sBuffer );
    displayRTOI( aObj );

    return;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * Undo���п� ���� RTOI�� �غ���
 *
 * aTrans       - [IN] Undo�ϴ� Transaction
 * aType        - [IN] �߸��� ��ü�� ����
 * aTableOID    - [IN] RTOI�� ����� ������ TableOID
 * aIndexID     - [IN] ������ IndexID
 * aSpaceID     - [IN] ��� �������� TablesSpaceID
 * aPageID      - [IN] ��� �������� PageID
 **********************************************************************/
void smrRecoveryMgr::prepareRTOIForUndoFailure( void        * aTrans,
                                                smrRTOIType   aType,
                                                smOID         aTableOID,
                                                UInt          aIndexID,
                                                scSpaceID     aSpaceID,
                                                scPageID      aPageID )
{
    smrRTOI * sObj;

    sObj = smLayerCallback::getRTOI4UndoFailure( aTrans );

    initRTOI( sObj );
    sObj->mCause          = SMR_RTOI_CAUSE_UNDO;
    sObj->mCauseLSN       = smLayerCallback::getCurUndoNxtLSN( aTrans );
    sObj->mCauseDiskPage  = NULL;
    sObj->mType           = aType;
    sObj->mState          = SMR_RTOI_STATE_CHECKED;
    sObj->mTableOID       = aTableOID;
    sObj->mIndexID        = aIndexID;
    sObj->mGRID.mSpaceID  = aSpaceID;
    sObj->mGRID.mPageID   = aPageID;
    sObj->mGRID.mOffset   = 0;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * Refine���з� �ش� Table�� Inconsistent ����
 *
 * aTableOID    - [IN] ������ Table
 **********************************************************************/
IDE_RC smrRecoveryMgr::refineFailureWithTable( smOID   aTableOID )
{
    smrRTOI   sObj;

    initRTOI( &sObj );
    sObj.mCause          = SMR_RTOI_CAUSE_REFINE;
    SM_SET_LSN( sObj.mCauseLSN, 0, 0 );
    sObj.mCauseDiskPage  = NULL;
    sObj.mType           = SMR_RTOI_TYPE_TABLE;
    sObj.mState          = SMR_RTOI_STATE_CHECKED;
    sObj.mTableOID       = aTableOID;
    sObj.mIndexID        = 0;

    IDE_TEST( smrRecoveryMgr::startupFailure( &sObj,
                                              ID_FALSE ) // isRedo
              != IDE_SUCCESS );

    /* IDE_SUCCESS�� Return�ϱ� ������, ide error�� û���� */
    ideClearError();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * Refine���з� �ش� Index�� Inconsistent ����
 *
 * aTableOID    - [IN] ������ Table
 * aIndex       - [IN] ������ Index
 **********************************************************************/
IDE_RC smrRecoveryMgr::refineFailureWithIndex( smOID   aTableOID,
                                               UInt    aIndexID )
{
    smrRTOI   sObj;

    initRTOI( &sObj );
    sObj.mCause          = SMR_RTOI_CAUSE_REFINE;
    SM_SET_LSN( sObj.mCauseLSN, 0, 0 );
    sObj.mCauseDiskPage  = NULL;
    sObj.mType           = SMR_RTOI_TYPE_INDEX;
    sObj.mState          = SMR_RTOI_STATE_CHECKED;
    sObj.mTableOID       = aTableOID;
    sObj.mIndexID        = aIndexID;

    IDE_TEST( smrRecoveryMgr::startupFailure( &sObj,
                                              ID_FALSE ) // isRedo
              != IDE_SUCCESS );

    /* IDE_SUCCESS�� Return�ϱ� ������, ide error�� û���� */
    ideClearError();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * IOL�� �ʱ�ȭ �մϴ�.
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::initializeIOL()
{
    mIOLHead.mNext = NULL;
    mIOLCount      = 0;

    IDE_TEST( mIOLMutex.initialize( (SChar*) "SMR_IOL_MUTEX",
                                   IDU_MUTEX_KIND_NATIVE,
                                   IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * IOL�� �߰� �մϴ�.
 *
 * aObj         - [IN] �߰��� ��ü
 **********************************************************************/
void smrRecoveryMgr::addIOL( smrRTOI * aObj )
{
    smrRTOI * sObj;
    SChar     sBuffer[ SMR_MESSAGE_BUFFER_SIZE ];

    if ( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                            ID_SIZEOF( smrRTOI ),
                            (void**)&sObj,
                            IDU_MEM_FORCE)
         == IDE_SUCCESS )
    {
        idlOS::memcpy( sObj, aObj, ID_SIZEOF( smrRTOI ) );

        /* Link Head�� Attach
         * ��� ���� Mutex�� ���� �ʿ�� ������, Parallel Recovery��
         * ����Ͽ� Mutex�� ��Ƶд� */
        (void)mIOLMutex.lock( NULL /* statistics */ );
        sObj->mNext    = mIOLHead.mNext;
        mIOLHead.mNext = sObj;
        mIOLCount ++;
        (void)mIOLMutex.unlock();

        displayRTOI( aObj );
    }
    else
    {
        /* nothing to do ...
         * �޸� �������� �����ϴ� ��Ȳ. ��Ե� ���� ������ �����ؾ� �ϸ�
         * addIOL�� �����ص� inconsistency ������ �ȵ� ���̱� ������
         * �׳� �����Ѵ�. DEBUG��� ���� */
        idlOS::snprintf( sBuffer,
                         SMR_MESSAGE_BUFFER_SIZE,
                         SM_TRC_EMERECO_FAILED_SET_INCONSISTENCY_FLAG );

        ideLog::log( IDE_ERR_0, sBuffer );
        IDE_CALLBACK_SEND_MSG( sBuffer );
        displayRTOI( aObj );

        IDE_DASSERT( 0 );
    }
}

/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * IOL�� �̹� ��ϵǾ��ִ��� �˻���
 *
 * aObj         - [IN] �˻��� ��ü
 **********************************************************************/
idBool smrRecoveryMgr::findIOL( smrRTOI * aObj )
{
    smrRTOI * sCursor;
    idBool    sRet;

    sRet = ID_FALSE;

    sCursor = mIOLHead.mNext;
    while( (sCursor != NULL ) && ( sRet == ID_FALSE ) )
    {
        if ( aObj->mType == sCursor->mType )
        {
            switch( aObj->mType )
            {
            case SMR_RTOI_TYPE_TABLE:
                if ( aObj->mTableOID == sCursor->mTableOID )
                {
                    sRet = ID_TRUE;
                }
                break;
            case SMR_RTOI_TYPE_INDEX:
                if ( aObj->mIndexID == sCursor->mIndexID )
                {
                    sRet = ID_TRUE;
                }
                break;
            case SMR_RTOI_TYPE_MEMPAGE:
            case SMR_RTOI_TYPE_DISKPAGE:
                if ( ( aObj->mGRID.mSpaceID == sCursor->mGRID.mSpaceID ) &&
                     ( aObj->mGRID.mPageID  == sCursor->mGRID.mPageID  ) )
                {
                    sRet = ID_TRUE;
                }
                break;
            default:
                IDE_DASSERT(0);
                break;
            }
        }
        else
        {
            sRet = ID_FALSE;
        }

        sCursor = sCursor->mNext;
    }

    return sRet;
}


/**********************************************************************
 * Description : PROJ-2162 RestartRiskReduction
 *
 * TOI Message�� TRC/isql�� ����մϴ�.
 *
 * aObj         - [IN] ����� ���
 **********************************************************************/
void smrRecoveryMgr::displayRTOI(  smrRTOI * aObj )
{
    SChar             sBuffer[ SMR_MESSAGE_BUFFER_SIZE ] = {0};

    if ( ( aObj->mState == SMR_RTOI_STATE_CHECKED ) ||
         ( aObj->mState == SMR_RTOI_STATE_DONE ) )
    {
        switch( aObj->mCause )
        {
        case SMR_RTOI_CAUSE_PROPERTY:
            idlOS::snprintf( sBuffer,
                             SMR_MESSAGE_BUFFER_SIZE,
                             "\n          StartupFailure: "
                             "(SRC : PROPERTY )" );
            break;
        case SMR_RTOI_CAUSE_OBJECT:
            idlOS::snprintf( sBuffer,
                             SMR_MESSAGE_BUFFER_SIZE,
                             "\n          StartupFailure: "
                             "(SRC : INCONSISTENT OBJECT )" );
            break;
        case SMR_RTOI_CAUSE_REFINE:
            idlOS::snprintf( sBuffer,
                             SMR_MESSAGE_BUFFER_SIZE,
                             "\n          StartupFailure: "
                             "(SRC : REFINE FAILURE )" );
            break;
        case SMR_RTOI_CAUSE_REDO:
        case SMR_RTOI_CAUSE_UNDO:
            idlOS::snprintf( sBuffer,
                             SMR_MESSAGE_BUFFER_SIZE,
                             "\n          StartupFailure: "
                             "(SRC : LSN=<%"ID_UINT32_FMT","
                             "%"ID_UINT32_FMT">)",
                             aObj->mCauseLSN.mFileNo,
                             aObj->mCauseLSN.mOffset );
            break;
        default:
            IDE_DASSERT(0);
            break;
        }
        switch( aObj->mType )
        {
        case SMR_RTOI_TYPE_TABLE:
            idlVA::appendFormat( sBuffer,
                                 SMR_MESSAGE_BUFFER_SIZE,
                                 "\tTABLE, TableOID=<%"ID_UINT64_FMT">",
                                 aObj->mTableOID );
            break;
        case SMR_RTOI_TYPE_INDEX:
            idlVA::appendFormat( sBuffer,
                                 SMR_MESSAGE_BUFFER_SIZE,
                                 "\tINDEX, IndexID=<%"ID_UINT32_FMT">",
                                 aObj->mIndexID );
            break;
        case SMR_RTOI_TYPE_MEMPAGE:
            idlVA::appendFormat( sBuffer,
                                 SMR_MESSAGE_BUFFER_SIZE,
                                 "\tMRDB PAGE, SPACEID=<%"ID_UINT32_FMT">,"
                                 "PID=<%"ID_UINT32_FMT">",
                                 aObj->mGRID.mSpaceID,
                                 aObj->mGRID.mPageID );
            break;
        case SMR_RTOI_TYPE_DISKPAGE:
            idlVA::appendFormat( sBuffer,
                                 SMR_MESSAGE_BUFFER_SIZE,
                                 "\tDRDB PAGE, SPACEID=<%"ID_UINT32_FMT">,"
                                 "PID=<%"ID_UINT32_FMT">",
                                 aObj->mGRID.mSpaceID,
                                 aObj->mGRID.mPageID );
            break;
        default:
            break;
        }

        ideLog::log( IDE_ERR_0, sBuffer );
        IDE_CALLBACK_SEND_MSG( sBuffer );
    }
}

/* Redo������ ����� �Ǿ����� �̷�� Inconsistency�� ������ */
void smrRecoveryMgr::applyIOLAtRedoFinish()
{
    smrRTOI * sCursor;

    sCursor = mIOLHead.mNext;
    while( sCursor != NULL )
    {
        setObjectInconsistency( sCursor,
                                ID_FALSE ); // isRedo

        sCursor = sCursor->mNext;
    }
}

/* IOL(InconsistentObjectList)�� ���� �մϴ�. */
IDE_RC smrRecoveryMgr::finalizeIOL()
{
    smrRTOI * sCursor;
    smrRTOI * sNextCursor;

    sCursor = mIOLHead.mNext;
    while( sCursor != NULL )
    {
        sNextCursor = sCursor->mNext;
        IDE_TEST( iduMemMgr::free( (void**)sCursor ) != IDE_SUCCESS );
        sCursor = sNextCursor;
    }

    mIOLHead.mNext = NULL;
    mIOLCount      = 0;
    IDE_TEST( mIOLMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* DRDB Wal�� �������� Ȯ���մϴ�. */
IDE_RC smrRecoveryMgr::checkDiskWAL()
{
    SChar    sBuffer[ SMR_MESSAGE_BUFFER_SIZE ];
    smLSN    sLstUpdatedPageLSN = sdrRedoMgr::getLastUpdatedPageLSN();
    SChar *  sFileName;

    if( smrCompareLSN::isGT( &sLstUpdatedPageLSN,
                             &mLstDRDBRedoLSN ) == ID_TRUE )
    {
        idlOS::snprintf( sBuffer,
                         SMR_MESSAGE_BUFFER_SIZE,
                         SM_TRC_EMERECO_DRDB_WAL_VIOLATION,
                         sLstUpdatedPageLSN.mFileNo,
                         sLstUpdatedPageLSN.mOffset,
                         mLstDRDBRedoLSN.mFileNo,
                         mLstDRDBRedoLSN.mOffset );
        ideLog::log( IDE_ERR_0, sBuffer );
        IDE_CALLBACK_SEND_MSG( sBuffer );

        /* BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�.
         *           �߰� ���� ��� */
        ideLog::log( IDE_ERR_0,
                     "Last Updated SpaceID: %"ID_UINT32_FMT", "
                     "PageID: %" ID_UINT32_FMT ", "
                     "FileID: %" ID_UINT32_FMT ", "
                     "FPageID: %"ID_UINT32_FMT"\n",
                     sdrRedoMgr::getLastUpdatedSpaceID(),
                     sdrRedoMgr::getLastUpdatedPageID(),
                     SD_MAKE_FID(sdrRedoMgr::getLastUpdatedPageID()),
                     SD_MAKE_FPID(sdrRedoMgr::getLastUpdatedPageID()));

        sFileName = sddDiskMgr::getFileName( sdrRedoMgr::getLastUpdatedSpaceID(),
                                             sdrRedoMgr::getLastUpdatedPageID() );

        if ( sFileName != NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "Last Updated File : %s\n",
                         sFileName );
        }

        IDE_TEST( prepare4DBInconsistencySetting() != IDE_SUCCESS );
        mDRDBConsistency = ID_FALSE;
        IDE_TEST( finish4DBInconsistencySetting() != IDE_SUCCESS );
    }
    else
    {
        mDRDBConsistency = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* MRDB Wal�� �������� Ȯ���մϴ�. */
IDE_RC smrRecoveryMgr::checkMemWAL()
{
    SChar             sBuffer[ SMR_MESSAGE_BUFFER_SIZE ];

    if ( smrCompareLSN::isGT( &mEndChkptLSN, &mLstRedoLSN )
         == ID_TRUE )
    {
        idlOS::snprintf( sBuffer,
                         SMR_MESSAGE_BUFFER_SIZE,
                         SM_TRC_EMERECO_MRDB_WAL_VIOLATION,
                         mEndChkptLSN.mFileNo,
                         mEndChkptLSN.mOffset,
                         mLstRedoLSN.mFileNo,
                         mLstRedoLSN.mOffset );
        ideLog::log( IDE_ERR_0, sBuffer );
        IDE_CALLBACK_SEND_MSG( sBuffer );

        IDE_TEST( prepare4DBInconsistencySetting() != IDE_SUCCESS );
        mMRDBConsistency = ID_FALSE;
        IDE_TEST( finish4DBInconsistencySetting() != IDE_SUCCESS );
    }
    else
    {
        mMRDBConsistency = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* DB Inconsistency ������ ���� �غ� �մϴ�.
 * Flusher�� ����ϴ�. */
IDE_RC smrRecoveryMgr::prepare4DBInconsistencySetting()
{
    UInt i;

    /* BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�.
     * EMERGENCY NORMAL���� ������ Ȯ������ �ʰ� �ٷ� ���� ����Ѵ�. */
    IDE_TEST( mEmerRecovPolicy == SMR_RECOVERY_NORMAL );

    /* Inconsistent�� DB�� ��� LogFlush�� ���´�. �׷��� mmap�� ���, Log��
     * �ڵ����� ��ϵǱ� ������ Log�� ����� ���� ���� ����. */
    IDE_TEST_RAISE( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MMAP,
                    ERR_INCONSISTENT_DB_AND_LOG_BUFFER_TYPE );

    for( i = 0 ; i < sdbFlushMgr::getFlusherCount(); i ++ )
    {
        IDE_TEST( sdbFlushMgr::turnOffFlusher( NULL /*Statistics*/,
                                               i )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_DB_AND_LOG_BUFFER_TYPE)
    {
        IDE_SET( ideSetErrorCode(
                 smERR_ABORT_ERR_INCONSISTENT_DB_AND_LOG_BUFFER_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* DB Inconsistency ������ ������ �մϴ�.
 * Flusher�� �籸���մϴ�. */
IDE_RC smrRecoveryMgr::finish4DBInconsistencySetting()
{
    UInt i;

    for( i = 0 ; i < sdbFlushMgr::getFlusherCount(); i ++ )
    {
        IDE_TEST( sdbFlushMgr::turnOnFlusher( i )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * Level 0 incremental bakcup������ �̿��� �����ͺ��̽��� �����Ѵ�.
 * 
 * aRestoreType     - [IN] ���� ����
 * aUntilTime       - [IN] �ҿ��� ���� (�ð�)
 * aUntilBackupTag  - [IN] �ҿ��� ���� (backupTag)
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreDB( smiRestoreType    aRestoreType,
                                  UInt              aUntilTime,
                                  SChar           * aUntilBackupTag )
{
    UInt            sUntilTimeBISlotIdx;           
    UInt            sStartScanBISlotIdx;
    UInt            sRestoreBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sFirstRestoreBISlotIdx;
    smriBIFileHdr * sBIFileHdr;  
    smriBISlot    * sBISlot;
    smriBISlot    * sLastRestoredBISlot;
    idBool          sSearchUntilBackupTag; 
    idBool          sIsNeedRestoreLevel1 = ID_FALSE;
    SChar           sRestoreTag[ SMI_MAX_BACKUP_TAG_NAME_LEN ];
    SChar           sBuffer[SMR_MESSAGE_BUFFER_SIZE];
    scSpaceID       sDummySpaceID = 0;
    UInt            sState = 0;

    IDE_TEST_RAISE( getBIMgrState() != SMRI_BI_MGR_INITIALIZED,
                    err_backup_info_state); 

    IDE_TEST( smriBackupInfoMgr::lock() != IDE_SUCCESS );
    sState = 1;

    /* backup info ���Ͽ��� backupinfo slot���� �о���δ�. */
    IDE_TEST( smriBackupInfoMgr::loadBISlotArea() != IDE_SUCCESS );
    sState = 2;

    idlOS::memset( sRestoreTag, 0x00, SMI_MAX_BACKUP_TAG_NAME_LEN );

    (void)smriBackupInfoMgr::getBIFileHdr( &sBIFileHdr );

    /* ���� Ÿ�Կ����� backupinfo slot�� �˻��� ������ ��ġ�� ���Ѵ�. */
    switch( aRestoreType )
    {
        case SMI_RESTORE_COMPLETE:
            {
                sStartScanBISlotIdx     = sBIFileHdr->mBISlotCnt - 1;
                sSearchUntilBackupTag   = ID_FALSE;

                break;
            }
        case SMI_RESTORE_UNTILTIME:
            {
                /* untilTime�� �ش��ϴ� backupinfo slot�� ��ġ�� ���Ѵ�. */ 
                IDE_TEST( smriBackupInfoMgr::findBISlotIdxUsingTime( 
                                                             aUntilTime, 
                                                             &sUntilTimeBISlotIdx ) 
                          != IDE_SUCCESS );

                sStartScanBISlotIdx     = sUntilTimeBISlotIdx;
                sSearchUntilBackupTag   = ID_FALSE;

                break;
            }
        case SMI_RESTORE_UNTILTAG:
            {   
                sStartScanBISlotIdx     = sBIFileHdr->mBISlotCnt - 1;
                sSearchUntilBackupTag   = ID_TRUE;

                break;
            }
        default:
            {   
                IDE_ASSERT(0);
                break;
            }
    }

    IDE_TEST_RAISE( sStartScanBISlotIdx == SMRI_BI_INVALID_SLOT_IDX,
                    there_is_no_incremental_backup );

    /* ������ ������ backupinfo slot�� idx���� ���Ѵ�. */
    IDE_TEST( smriBackupInfoMgr::getRestoreTargetSlotIdx( aUntilBackupTag,
                                                sStartScanBISlotIdx,
                                                sSearchUntilBackupTag,
                                                SMRI_BI_BACKUP_TARGET_DATABASE,
                                                sDummySpaceID,
                                                &sRestoreBISlotIdx )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRestoreBISlotIdx == SMRI_BI_INVALID_SLOT_IDX,
                    there_is_no_database_incremental_backup );

    IDE_TEST( smriBackupInfoMgr::getBISlot( sRestoreBISlotIdx, &sBISlot ) 
              != IDE_SUCCESS );

    /* ���� ����� backup tag�� �����Ѵ�. */
    idlOS::strncpy( sRestoreTag, 
                    sBISlot->mBackupTag, 
                    SMI_MAX_BACKUP_TAG_NAME_LEN );

    /* 
     * ���� ����� backup tag�� �ٸ� backup tag�� ���� backup info slot��
     * ���ö����� full backup ������ �����Ѵ�.
     */ 
    
    IDE_CALLBACK_SEND_MSG("  [ RECMGR ] restoring level 0 datafile....");
    sFirstRestoreBISlotIdx = sRestoreBISlotIdx;

    for( ; 
         sRestoreBISlotIdx < sBIFileHdr->mBISlotCnt; 
         sRestoreBISlotIdx++ )
    {
        IDE_TEST( smriBackupInfoMgr::getBISlot( sRestoreBISlotIdx, &sBISlot ) 
                  != IDE_SUCCESS );

        if ( idlOS::strncmp( sRestoreTag, 
                             sBISlot->mBackupTag, 
                             SMI_MAX_BACKUP_TAG_NAME_LEN ) != 0 )
        {
            sIsNeedRestoreLevel1 = ID_TRUE;
            break;
        }
        else
        {
            /* nothung to do */
        }

        IDE_DASSERT( ( sBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) != 0 );
        IDE_DASSERT( sBISlot->mBackupTarget == SMRI_BI_BACKUP_TARGET_DATABASE );

        IDE_TEST( restoreDataFile4Level0( sBISlot ) != IDE_SUCCESS );
    }

    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     "  \t\t backup info slots <%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT"> restored",
                     sFirstRestoreBISlotIdx,
                     sRestoreBISlotIdx - 1 );

    IDE_CALLBACK_SEND_MSG(sBuffer);

    /* BUG-37371
     * backuptag�� level 0 ������� �� �����ϵ��� �����Ȱ�� level 1������ ����
     * �ʴ´�. */
    if ( (aUntilBackupTag != NULL) &&
        (idlOS::strncmp( sRestoreTag, 
                         aUntilBackupTag, 
                         SMI_MAX_BACKUP_TAG_NAME_LEN ) == 0) )
    {
        sIsNeedRestoreLevel1 = ID_FALSE;
    }
    else
    {
        /* nothung to do */
    }
    
    /* 
     * incremental backup ������ �̿��� ������ �ʿ����� Ȯ���ϰ�
     * backup������ �̿��� �����ͺ��̽� ������ �����Ѵ�. 
     */
    if ( sIsNeedRestoreLevel1 == ID_TRUE )
    {
        IDE_DASSERT( sRestoreBISlotIdx < sBIFileHdr->mBISlotCnt );

        IDE_TEST( restoreDB4Level1( sRestoreBISlotIdx,
                                    aRestoreType, 
                                    aUntilTime, 
                                    aUntilBackupTag, 
                                    &sLastRestoredBISlot ) 
                  != IDE_SUCCESS );

    }
    else
    {
        IDE_DASSERT( sRestoreBISlotIdx <= sBIFileHdr->mBISlotCnt );
        sLastRestoredBISlot = sBISlot;
    }

    setCurrMediaTime( sLastRestoredBISlot->mEndBackupTime );
    setLastRestoredTagName( sLastRestoredBISlot->mBackupTag );

    sState = 1;
    IDE_TEST( smriBackupInfoMgr::unloadBISlotArea() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smriBackupInfoMgr::unlock() != IDE_SUCCESS );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION( there_is_no_database_incremental_backup );
    {  
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_ThereIsNoDatabaseIncrementalBackup));
    }
    IDE_EXCEPTION( there_is_no_incremental_backup );
    {  
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_ThereIsNoIncrementalBackup));
    }
    IDE_EXCEPTION( err_backup_info_state ); 
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupInfoState));
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( smriBackupInfoMgr::unloadBISlotArea() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smriBackupInfoMgr::unlock() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreTBS( scSpaceID aSpaceID ) 
{
    smriBIFileHdr * sBIFileHdr;
    smriBISlot    * sBISlot;
    SChar           sBuffer[SMR_MESSAGE_BUFFER_SIZE];
    UInt            sRestoreBISlotIdx;
    UInt            sFirstRestoreBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sLastRestoreBISlotIdx  = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sState = 0;
    
    IDE_TEST_RAISE( getBIMgrState() != SMRI_BI_MGR_INITIALIZED,
                    err_backup_info_state); 

    IDE_TEST( smriBackupInfoMgr::lock() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smriBackupInfoMgr::loadBISlotArea() != IDE_SUCCESS );
    sState = 2;

    (void)smriBackupInfoMgr::getBIFileHdr( &sBIFileHdr );

    IDE_TEST( smriBackupInfoMgr::getRestoreTargetSlotIdx( NULL,
                                        sBIFileHdr->mBISlotCnt - 1,
                                        ID_FALSE,
                                        SMRI_BI_BACKUP_TARGET_TABLESPACE,
                                        aSpaceID,
                                        &sRestoreBISlotIdx )
              != IDE_SUCCESS );
        
    IDE_TEST_RAISE( sRestoreBISlotIdx == SMRI_BI_INVALID_SLOT_IDX,
                    there_is_no_incremental_backup );

    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     "  [ RECMGR ] restoring datafile.... TBS ID<%"ID_UINT32_FMT"> ",
                     aSpaceID );

    IDE_CALLBACK_SEND_MSG(sBuffer);

    for( ; sRestoreBISlotIdx < sBIFileHdr->mBISlotCnt; sRestoreBISlotIdx++ )
    {
        IDE_TEST( smriBackupInfoMgr::getBISlot( sRestoreBISlotIdx, &sBISlot ) 
                  != IDE_SUCCESS );

        if ( sBISlot->mSpaceID == aSpaceID )
        {
            if ( ( sBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) != 0 )
            {
                if ( sFirstRestoreBISlotIdx == SMRI_BI_INVALID_SLOT_IDX )
                {
                    sFirstRestoreBISlotIdx = sRestoreBISlotIdx;
                }
                sLastRestoreBISlotIdx = sRestoreBISlotIdx;

                IDE_TEST( restoreDataFile4Level0( sBISlot ) != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } 
    }

    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     "  \t\t backup info slots <%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT"> restored",
                     sFirstRestoreBISlotIdx,
                     sLastRestoreBISlotIdx );

    IDE_CALLBACK_SEND_MSG(sBuffer);

    IDE_TEST( restoreTBS4Level1( aSpaceID, 
                                 sRestoreBISlotIdx, 
                                 sBIFileHdr->mBISlotCnt )
              != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( smriBackupInfoMgr::unloadBISlotArea() != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( smriBackupInfoMgr::unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( there_is_no_incremental_backup );
    {  
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_ThereIsNoIncrementalBackup));
    }
    IDE_EXCEPTION( err_backup_info_state ); 
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupInfoState));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( smriBackupInfoMgr::unloadBISlotArea() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smriBackupInfoMgr::unlock() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * aBISlot  - [IN] ������ ��������� backupinfo slot
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreDataFile4Level0( smriBISlot * aBISlot )
{

    SChar               sRestoreFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    SChar               sNxtStableDBFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    iduFile             sFile;
    smmChkptPathNode  * sCPathNode;
    sctTableSpaceNode * sSpaceNode;
    smmTBSNode        * sMemSpaceNode;
    sddTableSpaceNode * sDiskSpaceNode;
    smmDatabaseFile   * sDatabaseFile;
    smmDatabaseFile   * sNxtStableDatabaseFile;
    sddDataFileNode   * sDataFileNode;
    UInt                sStableDB;
    UInt                sNxtStableDB;
    UInt                sCPathCount;
    idBool              sResult = ID_FALSE;
    UInt                sState = 0;

    IDE_DASSERT( aBISlot != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aBISlot->mSpaceID,
                                                        (void **)&sSpaceNode )
              != IDE_SUCCESS );
    
    idlOS::memset( sRestoreFileName, 0x00, SMRI_BI_MAX_BACKUP_FILE_NAME_LEN );

    if ( sctTableSpaceMgr::isMemTableSpace(aBISlot->mSpaceID) == ID_TRUE )
    {
        sMemSpaceNode = (smmTBSNode *)sSpaceNode;

        sStableDB = smmManager::getCurrentDB( sMemSpaceNode );
        sNxtStableDB = smmManager::getNxtStableDB( sMemSpaceNode );
        
        IDE_TEST( smmManager::getDBFile( sMemSpaceNode,
                                         sStableDB,
                                         aBISlot->mFileNo,
                                         SMM_GETDBFILEOP_NONE,
                                         &sDatabaseFile )
                  != IDE_SUCCESS );
        
        IDE_TEST( smmManager::getDBFile( sMemSpaceNode,
                                         sNxtStableDB,
                                         aBISlot->mFileNo,
                                         SMM_GETDBFILEOP_NONE,
                                         &sNxtStableDatabaseFile )
                  != IDE_SUCCESS );

        if ( (idlOS::strncmp( sDatabaseFile->getFileName(), 
                             "\0", 
                             SMI_MAX_DATAFILE_NAME_LEN ) == 0) ||
            (idlOS::strncmp( sNxtStableDatabaseFile->getFileName(), 
                             "\0", 
                             SMI_MAX_DATAFILE_NAME_LEN ) == 0) )
        {
            IDE_TEST( smmTBSChkptPath::getChkptPathNodeCount( sMemSpaceNode,                    
                                                              &sCPathCount )
                      != IDE_SUCCESS );
 
            IDE_ASSERT( sCPathCount != 0 );
 
            IDE_TEST( smmTBSChkptPath::getChkptPathNodeByIndex( 
                                                    sMemSpaceNode,                    
                                                    aBISlot->mFileNo % sCPathCount,
                                                    &sCPathNode )
                  != IDE_SUCCESS );    
 
            IDE_ASSERT( sCPathNode != NULL );
 
            idlOS::snprintf( sRestoreFileName,
                             SMRI_BI_MAX_BACKUP_FILE_NAME_LEN,
                             "%s%c%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                             sCPathNode->mChkptPathAttr.mChkptPath,
                             IDL_FILE_SEPARATOR,
                             sMemSpaceNode->mHeader.mName,
                             sStableDB,
                             aBISlot->mFileNo );
            
            IDE_TEST( sDatabaseFile->setFileName( sRestoreFileName ) 
                      != IDE_SUCCESS );
 
            sDatabaseFile->setDir( sCPathNode->mChkptPathAttr.mChkptPath );
 
            idlOS::snprintf( sNxtStableDBFileName,
                             SMRI_BI_MAX_BACKUP_FILE_NAME_LEN,
                             "%s%c%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                             sCPathNode->mChkptPathAttr.mChkptPath,
                             IDL_FILE_SEPARATOR,
                             sMemSpaceNode->mHeader.mName,
                             sNxtStableDB,
                             aBISlot->mFileNo );
 
            IDE_TEST( sNxtStableDatabaseFile->setFileName( sNxtStableDBFileName ) 
                      != IDE_SUCCESS );
 
            sNxtStableDatabaseFile->setDir( sCPathNode->mChkptPathAttr.mChkptPath );
        }
        else
        {
            idlOS::strncpy( sRestoreFileName, 
                            sDatabaseFile->getFileName(), 
                            SM_MAX_FILE_NAME );

            idlOS::strncpy( sNxtStableDBFileName, 
                            sNxtStableDatabaseFile->getFileName(), 
                            SM_MAX_FILE_NAME );
        }

        IDE_TEST( sDatabaseFile->isNeedCreatePingPongFile( aBISlot,
                                                           &sResult ) 
                  != IDE_SUCCESS ); 
    }
    else if ( sctTableSpaceMgr::isDiskTableSpace(aBISlot->mSpaceID) == ID_TRUE )
    {
        sDiskSpaceNode = (sddTableSpaceNode *)sSpaceNode;

        sDataFileNode = sDiskSpaceNode->mFileNodeArr[ aBISlot->mFileID ];
        
        idlOS::strncpy( sRestoreFileName, 
                        sDataFileNode->mFile.getFileName(),
                        SMI_MAX_DATAFILE_NAME_LEN );
    }
    else
    {
        IDE_DASSERT(0);
    }

    if ( idf::access( sRestoreFileName, F_OK ) == 0 )
    {
        idf::unlink( sRestoreFileName );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                1,                             
                                IDU_FIO_STAT_OFF,              
                                IDV_WAIT_INDEX_NULL )          
              != IDE_SUCCESS );              
    sState = 1;

    IDE_TEST( sFile.setFileName( aBISlot->mBackupFileName ) != IDE_SUCCESS );

    IDE_TEST( sFile.open() != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( sFile.copy( NULL, sRestoreFileName ) != IDE_SUCCESS );

    if ( sResult == ID_TRUE )
    {
        IDE_TEST( sFile.copy( NULL, sNxtStableDBFileName ) != IDE_SUCCESS );
    }

    sState = 1;
    IDE_TEST( sFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sFile.destroy() == IDE_SUCCESS );
        case 0:
        default :
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * Level 1 incremental bakcup������ �̿��� �����ͺ��̽��� �����Ѵ�.
 * 
 * aRestoreStartSlotIdx     - [IN] ������ ������ backupinfo slot�� idx
 * aRestoreType             - [IN] ���� Ÿ��
 * aUntilTime               - [IN] �ҿ��� ���� (�ð�)
 * aUntilBackupTag          - [IN] �ҿ��� ���� ( backup tag )
 * aLastRestoredBISlot      - [OUT] ���������� ������ backup info slot
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreDB4Level1( UInt            aRestoreStartSlotIdx,
                                         smiRestoreType  aRestoreType,
                                         UInt            aUntilTime,
                                         SChar         * aUntilBackupTag,
                                         smriBISlot   ** aLastRestoredBISlot )
{
    UInt            sRestoreStartSlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sRestoreEndSlotIdx   = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sSlotIdx;
    smriBISlot    * sBISlot = NULL;
    SChar           sBuffer[SMR_MESSAGE_BUFFER_SIZE];

    IDE_DASSERT( aLastRestoredBISlot != NULL );

    /* ������ ������ backup info�� ���� ��� */
    IDE_TEST( smriBackupInfoMgr::calcRestoreBISlotRange4Level1( 
                                              aRestoreStartSlotIdx,
                                              aRestoreType,
                                              aUntilTime,
                                              aUntilBackupTag,
                                              &sRestoreStartSlotIdx,
                                              &sRestoreEndSlotIdx )
              != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("  [ RECMGR ] restoring level 1 datafile....");
    /* ���� ���� */
    for( sSlotIdx = sRestoreStartSlotIdx; 
         sSlotIdx <= sRestoreEndSlotIdx; 
         sSlotIdx++ )
    {
        IDE_TEST( smriBackupInfoMgr::getBISlot( sSlotIdx, &sBISlot ) 
                  != IDE_SUCCESS );

        if ( sctTableSpaceMgr::isMemTableSpace( sBISlot->mSpaceID ) == ID_TRUE )
        {
            IDE_TEST( restoreMemDataFile4Level1( sBISlot ) != IDE_SUCCESS );
        }
        else if ( sctTableSpaceMgr::isDiskTableSpace( sBISlot->mSpaceID ) == ID_TRUE )
        {
            IDE_TEST( restoreDiskDataFile4Level1( sBISlot ) != IDE_SUCCESS );
        }
        else
        {
            IDE_DASSERT(0);
        }
    }

    IDE_ERROR( sBISlot != NULL );
    
    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     "  \t\t backup info slots <%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT"> restored",
                     sRestoreStartSlotIdx,
                     sRestoreEndSlotIdx );

    IDE_CALLBACK_SEND_MSG(sBuffer);

    *aLastRestoredBISlot = sBISlot;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * aSpaceID                 - [IN] ������ SpaceID
 * aRestoreStartBISlotIdx   - [IN] ������ ������ backup info slot Idx
 * aBISlotCnt               - [IN] backup info ���Ͽ� ����� slot�� ��
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreTBS4Level1( scSpaceID aSpaceID, 
                                          UInt      aRestoreStartBISlotIdx,
                                          UInt      aBISlotCnt ) 
{
    UInt            sBISlotIdx;
    UInt            sRestoreStartSlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    UInt            sRestoreEndSlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    smriBISlot    * sBISlot;
    SChar           sBuffer[SMR_MESSAGE_BUFFER_SIZE];

    IDE_DASSERT( aRestoreStartBISlotIdx != SMRI_BI_INVALID_SLOT_IDX  );
    
    idlOS::snprintf( sBuffer,
                     SMR_MESSAGE_BUFFER_SIZE,
                     "  [ RECMGR ] restoring level 1 datafile.... TBS ID<%"ID_UINT32_FMT"> ",
                     aSpaceID );

    IDE_CALLBACK_SEND_MSG(sBuffer);

    for( sBISlotIdx = aRestoreStartBISlotIdx; 
         sBISlotIdx < aBISlotCnt; 
         sBISlotIdx++ )
    {
        IDE_TEST( smriBackupInfoMgr::getBISlot( sBISlotIdx, &sBISlot ) 
                  != IDE_SUCCESS );

        if ( sBISlot->mSpaceID == aSpaceID )
        {
            if ( sctTableSpaceMgr::isMemTableSpace( sBISlot->mSpaceID ) == ID_TRUE )
            {
                IDE_TEST( restoreMemDataFile4Level1( sBISlot ) != IDE_SUCCESS );
            }
            else if ( sctTableSpaceMgr::isDiskTableSpace( sBISlot->mSpaceID ) == ID_TRUE )
            {
                IDE_TEST( restoreDiskDataFile4Level1( sBISlot ) != IDE_SUCCESS );
            }
            else
            {
                IDE_ASSERT(0);
            }

            if ( sRestoreStartSlotIdx == SMRI_BI_INVALID_SLOT_IDX )
            {
                sRestoreStartSlotIdx = sBISlotIdx;
            }
            sRestoreEndSlotIdx = sBISlotIdx;
        }
    }

    if ( sRestoreStartSlotIdx != SMRI_BI_INVALID_SLOT_IDX )
    {
        idlOS::snprintf( sBuffer,
                         SMR_MESSAGE_BUFFER_SIZE,
                         "  \t\t backup info slots <%"ID_UINT32_FMT"> - <%"ID_UINT32_FMT"> restored",
                         sRestoreStartSlotIdx,
                         sRestoreEndSlotIdx );
    }
    else
    {
        idlOS::snprintf( sBuffer,
                         SMR_MESSAGE_BUFFER_SIZE,
                         "  \t\t No backup info slots restored");
    }

    IDE_CALLBACK_SEND_MSG(sBuffer);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * aPageID      - [IN] 
 * smriBiSlot   - [IN]
 *
 * �����Լ� - smmManager::getPageNoInFile
 * chkptImage���ϳ����� aPageID�� �� ��°�� ��ġ�ϰ��ִ°��� ���Ѵ�.
 *
 * DBFilePageCnt�� Membase�� ����Ǿ������� startup control�ܰ迡����
 * Membase�� �ε���� ���� �����̹Ƿ� �����ͺ��̽� �����߿��� 
 * DBFilePageCnt���� Ȯ�� �Ҽ� ����. incremental backup ������ �̿��� ������
 * �ϱ����� DBFilePageCnt���� backup info slot�� �����ϰ� �̸� �̿��Ͽ�
 * �����Ѵ�
 *
 **********************************************************************/
UInt smrRecoveryMgr::getPageNoInFile( UInt aPageID, smriBISlot * aBISlot)
{
    UInt          sPageNoInFile;   

    IDE_DASSERT( aBISlot != NULL );

    if ( aPageID < SMM_DATABASE_META_PAGE_CNT ) 
    {
         sPageNoInFile = aPageID;   
    }
    else
    {
        IDE_ASSERT( aBISlot->mDBFilePageCnt != 0 ); 

        sPageNoInFile = (UInt)( ( aPageID - SMM_DATABASE_META_PAGE_CNT )
                                    % aBISlot->mDBFilePageCnt );     

        if ( aPageID < (aBISlot->mDBFilePageCnt        
                            + SMM_DATABASE_META_PAGE_CNT) )                                                                                                            
        {
                sPageNoInFile += SMM_DATABASE_META_PAGE_CNT;                                                                                                           
        }
    }

    return sPageNoInFile;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * aBISlot  - [IN] �����Ϸ��� MemDataFile�� backup info slot
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreMemDataFile4Level1( smriBISlot * aBISlot )
{
    iduFile             sBackupFile;
    smmChkptImageHdr    sChkptImageHdr;
    smmTBSNode        * sMemSpaceNode;
    smmDatabaseFile   * sDatabaseFile;
    smmDatabaseFile     sNxtStableDatabaseFile;
    UChar             * sCopyBuffer;
    size_t              sReadOffset;
    size_t              sWriteOffset;
    UInt                sIBChunkCnt;
    UInt                sPageNoInFile;
    UInt                sWhichDB;
    UInt                sNxtStableDB;
    size_t              sCopySize;
    size_t              sReadSize;
    scPageID            sPageID;
    idBool              sIsCreated;
    idBool              sIsMediaFailure;
    UInt                sCPathCount;
    smmChkptPathNode  * sCPathNode;
    SChar               sRestoreFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    UInt                sState = 0;
    UInt                sMemAllocState = 0;
    UInt                sDatabaseFileState = 0;
    
    IDE_DASSERT( aBISlot != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aBISlot->mSpaceID,
                                                        (void **)&sMemSpaceNode )
              != IDE_SUCCESS );

    sWhichDB = smmManager::getCurrentDB( sMemSpaceNode );
    
    IDE_TEST( smmManager::getDBFile( sMemSpaceNode,
                                     sWhichDB,
                                     aBISlot->mFileNo,
                                     SMM_GETDBFILEOP_SEARCH_FILE, 
                                     &sDatabaseFile )
                  != IDE_SUCCESS );

    if ( idlOS::strncmp( sDatabaseFile->getFileName(),
                         "\0",
                         SMI_MAX_DATAFILE_NAME_LEN ) == 0 )
    {
        IDE_TEST( smmTBSChkptPath::getChkptPathNodeCount( sMemSpaceNode,
                                                          &sCPathCount )
                  != IDE_SUCCESS );

        IDE_ASSERT( sCPathCount != 0 );

        IDE_TEST( smmTBSChkptPath::getChkptPathNodeByIndex(
                                                sMemSpaceNode,
                                                aBISlot->mFileNo % sCPathCount,
                                                &sCPathNode )
              != IDE_SUCCESS );

        IDE_ASSERT( sCPathNode != NULL );

        idlOS::snprintf( sRestoreFileName,
                         SMRI_BI_MAX_BACKUP_FILE_NAME_LEN,
                         "%s%c%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                         sCPathNode->mChkptPathAttr.mChkptPath,
                         IDL_FILE_SEPARATOR,
                         sMemSpaceNode->mHeader.mName,
                         sWhichDB,
                         aBISlot->mFileNo );

        IDE_TEST( sDatabaseFile->setFileName( sRestoreFileName )
                  != IDE_SUCCESS );

        sDatabaseFile->setDir( sCPathNode->mChkptPathAttr.mChkptPath );
    }


    IDE_TEST( sBackupFile.initialize( IDU_MEM_SM_SMR,
                                      1,                             
                                      IDU_FIO_STAT_OFF,              
                                      IDV_WAIT_INDEX_NULL ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sBackupFile.setFileName( aBISlot->mBackupFileName ) 
              != IDE_SUCCESS );

    IDE_TEST( sBackupFile.open() != IDE_SUCCESS );
    sState = 2;

    if ( ( aBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) != 0 )
    {
        // copy target ������ open�� ���¿��� copy�ϸ� �ȵ�
        if ( sDatabaseFile->isOpen() == ID_TRUE )
        {
            IDE_TEST( sDatabaseFile->close() != IDE_SUCCESS );
        }

        IDE_TEST( sBackupFile.copy( NULL, (SChar*)sDatabaseFile->getFileName() )
                  != IDE_SUCCESS );
    }

    if ( sDatabaseFile->isOpen() == ID_FALSE )
    {
        IDE_TEST( sDatabaseFile->open() != IDE_SUCCESS );
        sDatabaseFileState = 1;
    }

    if ( ( aBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) == 0 )
    {
        IDE_TEST( sBackupFile.read( NULL,
                                    SM_DBFILE_METAHDR_PAGE_OFFSET,
                                    (void *)&sChkptImageHdr,
                                    ID_SIZEOF( smmChkptImageHdr ),
                                    &sReadSize )
                  != IDE_SUCCESS );
 
        sReadOffset = SM_DBFILE_METAHDR_PAGE_SIZE;
 
        smriBackupInfoMgr::clearDataFileHdrBI( &sChkptImageHdr.mBackupInfo );
 
        /* ChkptImageHdr���� */
        IDE_TEST( sDatabaseFile->mFile.write( NULL,
                                              SM_DBFILE_METAHDR_PAGE_OFFSET,
                                              &sChkptImageHdr,
                                              ID_SIZEOF( smmChkptImageHdr ) )
                  != IDE_SUCCESS );
 
        sCopySize = SM_PAGE_SIZE * aBISlot->mIBChunkSize;
 
        /* smrRecoveryMgr_restoreMemDataFile4Level1_calloc_CopyBuffer.tc */
        IDU_FIT_POINT("smrRecoveryMgr::restoreMemDataFile4Level1::calloc::CopyBuffer");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                     1,
                                     sCopySize,
                                     (void**)&sCopyBuffer )
                  != IDE_SUCCESS );
        sMemAllocState = 1;
 
        /* IBChunk ���� */
        for( sIBChunkCnt = 0; sIBChunkCnt < aBISlot->mIBChunkCNT; sIBChunkCnt++ )
        {
            IDE_TEST( sBackupFile.read( NULL,
                                        sReadOffset,
                                        (void *)sCopyBuffer,
                                        sCopySize,
                                        &sReadSize )
                      != IDE_SUCCESS );
 
            sPageID = SMP_GET_PERS_PAGE_ID( sCopyBuffer );
 
            sPageNoInFile = getPageNoInFile( sPageID, aBISlot);
 
            sWriteOffset = ( SM_PAGE_SIZE * sPageNoInFile ) 
                                + SM_DBFILE_METAHDR_PAGE_SIZE; 
            
            IDE_TEST( sDatabaseFile->mFile.write( NULL,
                                                  sWriteOffset,
                                                  sCopyBuffer,
                                                  sReadSize )
                      != IDE_SUCCESS );
            sReadOffset += sReadSize;
        }
 
        sMemAllocState = 0;
        IDE_TEST( iduMemMgr::free( sCopyBuffer ) != IDE_SUCCESS );
    }

    IDE_TEST( sDatabaseFile->checkValidationDBFHdr( &sChkptImageHdr,
                                                    &sIsMediaFailure ) 
              != IDE_SUCCESS );

    sNxtStableDB = smmManager::getNxtStableDB( sMemSpaceNode );
    sIsCreated   = smmManager::getCreateDBFileOnDisk( sMemSpaceNode,
                                                      sNxtStableDB,
                                                      aBISlot->mFileNo );

    if ( (sIsCreated == ID_TRUE) && ( sIsMediaFailure == ID_FALSE ) )
    {
       
        IDE_TEST( sNxtStableDatabaseFile.setFileName( 
                                        sDatabaseFile->getFileDir(),
                                        sMemSpaceNode->mHeader.mName,    
                                        sNxtStableDB,
                                        aBISlot->mFileNo )
                != IDE_SUCCESS );

        IDE_TEST( sDatabaseFile->copy( 
                            NULL, 
                            (SChar*)sNxtStableDatabaseFile.getFileName() ) 
                  != IDE_SUCCESS );
    }

    if ( sDatabaseFileState == 1 )
    {
        sDatabaseFileState = 0;
        IDE_TEST( sDatabaseFile->close() != IDE_SUCCESS );
    }

    sState = 1;
    IDE_TEST( sBackupFile.close() != IDE_SUCCESS );
                                    
    sState = 0;
    IDE_TEST( sBackupFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 2:
            IDE_ASSERT( sDatabaseFile->close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sBackupFile.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    switch( sMemAllocState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sCopyBuffer ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    switch( sDatabaseFileState )
    {
        case 1:
            IDE_ASSERT( sDatabaseFile->close() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2133 incremental backup
 *
 * aBISlot  - [IN] �����Ϸ��� DiskDataFile�� backup info slot
 *
 **********************************************************************/
IDE_RC smrRecoveryMgr::restoreDiskDataFile4Level1( smriBISlot * aBISlot ) 
{
    iduFile             sBackupFile;
    sddDataFileHdr      sDBFileHdr;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sDataFileNode = NULL;
    UChar             * sCopyBuffer;
    ULong               sReadOffset;
    ULong               sWriteOffset;
    UInt                sIBChunkCnt;
    size_t              sCopySize;
    size_t              sReadSize;
    ULong               sFileSize = 0;
    size_t              sCurrSize;
    sdpPhyPageHdr     * sPageHdr; 
    UInt                sState         = 0;
    UInt                sMemAllocState = 0;
    UInt                sDataFileState = 0;
    
    IDE_DASSERT( aBISlot != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aBISlot->mSpaceID,
                                                        (void **)&sSpaceNode )
              != IDE_SUCCESS );
    
    sDataFileNode = sSpaceNode->mFileNodeArr[ aBISlot->mFileID ];
    IDE_DASSERT( sDataFileNode != NULL );

    IDE_TEST( sBackupFile.initialize( IDU_MEM_SM_SMR,    
                                      1,                 
                                      IDU_FIO_STAT_OFF,  
                                      IDV_WAIT_INDEX_NULL ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sBackupFile.setFileName( aBISlot->mBackupFileName ) 
              != IDE_SUCCESS );

    IDE_TEST( sBackupFile.open() != IDE_SUCCESS );
    sState = 2;

    /* full backup�� incremental backup ���� */
    if ( ( aBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) != 0 )
    {
        // copy target ������ open�� ���¿��� copy�ϸ� �ȵ�
        if ( sDataFileNode->mIsOpened == ID_TRUE )
        {
            IDE_TEST( sddDiskMgr::closeDataFile( NULL,
                                                 sDataFileNode ) != IDE_SUCCESS );
        }
        IDE_ASSERT( sDataFileNode->mIsOpened == ID_FALSE );

        IDE_TEST( sBackupFile.copy( NULL, sDataFileNode->mFile.getFileName() )
                  != IDE_SUCCESS );
    }

    if ( sDataFileNode->mIsOpened == ID_FALSE )
    {
        IDE_TEST( sddDiskMgr::openDataFile( NULL,
                                            sDataFileNode ) != IDE_SUCCESS );
        sDataFileState = 1;
    }

    IDE_TEST( sDataFileNode->mFile.getFileSize(&sFileSize) != IDE_SUCCESS );

    sCurrSize = (sFileSize - SM_DBFILE_METAHDR_PAGE_SIZE) /
                 SD_PAGE_SIZE;
    
    if ( aBISlot->mOriginalFileSize < sCurrSize )
    {
        IDE_TEST( sddDataFile::truncate( sDataFileNode,
                                         aBISlot->mOriginalFileSize ) 
                  != IDE_SUCCESS );

        sddDataFile::setCurrSize( sDataFileNode, sCurrSize );
    
        if ( aBISlot->mOriginalFileSize < sDataFileNode->mInitSize )
        {
            sddDataFile::setInitSize(sDataFileNode,
                                     aBISlot->mOriginalFileSize );
        }
    }
    else
    {
        if ( aBISlot->mOriginalFileSize > sCurrSize )
        {
            sddDataFile::setCurrSize( sDataFileNode, aBISlot->mOriginalFileSize );
        }
        else
        {
            /* 
             * nothing to do 
             * ���� ũ�� ��ȭ ����         
             */
        }
    }

    if ( ( aBISlot->mBackupType & SMI_BACKUP_TYPE_FULL ) == 0 )
    {
        IDE_TEST( sBackupFile.read( NULL,
                                    SM_DBFILE_METAHDR_PAGE_OFFSET,
                                    (void *)&sDBFileHdr,
                                    ID_SIZEOF( sddDataFileHdr ),
                                    &sReadSize )
                  != IDE_SUCCESS );
 
        sReadOffset = SM_DBFILE_METAHDR_PAGE_SIZE;
 
        smriBackupInfoMgr::clearDataFileHdrBI( &sDBFileHdr.mBackupInfo );
 
        IDE_TEST( sDataFileNode->mFile.write( NULL,
                                              SM_DBFILE_METAHDR_PAGE_OFFSET,
                                              &sDBFileHdr,
                                              ID_SIZEOF( sddDataFileHdr ) )
                  != IDE_SUCCESS );
 
        sCopySize = SD_PAGE_SIZE * aBISlot->mIBChunkSize;
 
        /* smrRecoveryMgr_restoreDiskDataFile4Level1_calloc_CopyBuffer.tc */
        IDU_FIT_POINT("smrRecoveryMgr::restoreDiskDataFile4Level1::calloc::CopyBuffer");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                     1,
                                     sCopySize,
                                     (void**)&sCopyBuffer )
                  != IDE_SUCCESS );
        sMemAllocState = 1;
 
        for( sIBChunkCnt = 0; sIBChunkCnt < aBISlot->mIBChunkCNT; sIBChunkCnt++ )
        {
            IDE_TEST( sBackupFile.read( NULL,
                                        sReadOffset,
                                        (void *)sCopyBuffer,
                                        sCopySize,
                                        &sReadSize )
                    != IDE_SUCCESS );
 
            sPageHdr     = ( sdpPhyPageHdr * )sCopyBuffer;
            sWriteOffset = ( SD_MAKE_FPID(sPageHdr->mPageID) * SD_PAGE_SIZE ) + 
                             SM_DBFILE_METAHDR_PAGE_SIZE ;
 
            IDE_TEST( sDataFileNode->mFile.write( NULL,
                                                  sWriteOffset,
                                                  sCopyBuffer,
                                                  sReadSize )
                      != IDE_SUCCESS );
 
            sReadOffset += sReadSize;
        }
 
        sMemAllocState = 0;
        IDE_TEST( iduMemMgr::free( sCopyBuffer ) != IDE_SUCCESS );
 
    }
    sDataFileState = 0;
    IDE_TEST( sddDiskMgr::closeDataFile( NULL,
                                         sDataFileNode ) != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( sBackupFile.close() != IDE_SUCCESS );
                                    
    sState = 0;
    IDE_TEST( sBackupFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 2:
            IDE_ASSERT( sBackupFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sBackupFile.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    switch( sMemAllocState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sCopyBuffer ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    switch( sDataFileState )
    {
        case 1:
            IDE_ASSERT( sddDiskMgr::closeDataFile( NULL,
                                                   sDataFileNode ) 
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC smrRecoveryMgr::processPrepareReqLog( SChar              * aLogPtr,
                                             smrXaPrepareReqLog * aXaPrepareReqLog,
                                             smLSN              * aLSN )
{
    /* smrRecoveryMgr_processPrepareReqLog_ManageDtxInfoFunc.tc */
    IDU_FIT_POINT( "smrRecoveryMgr::processPrepareReqLog::ManageDtxInfoFunc" );
    IDE_DASSERT( mManageDtxInfoFunc != NULL ); 
    IDE_TEST( mManageDtxInfoFunc( &(aXaPrepareReqLog->mXID),
                                  smrLogHeadI::getTransID( &(aXaPrepareReqLog->mHead) ),
                                  aXaPrepareReqLog->mGlobalTxId,
                                  aXaPrepareReqLog->mBranchTxSize,
                                  (UChar*)aLogPtr,
                                  aLSN,
                                  NULL, //mCommitSCN
                                  SMI_DTX_PREPARE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrRecoveryMgr::processDtxLog( SChar      * aLogPtr,
                                      smrLogType   aLogType,
                                      smLSN      * aLSN,
                                      idBool     * aRedoSkip )
{
    smrXaPrepareReqLog  sXaPrepareReqLog;
    smrTransCommitLog   sCommitLog;
    smrTransAbortLog    sAbortLog;
    smrXaEndLog         sXaEndLog;

    /* BUG-47525 Group Commit */
    UInt                i;
    UInt                sGCCnt;
    smTID               sTID;
    smTID               sGlobalTID;
    SChar             * sRedoBuffer;
    smrTransGroupCommitLog sGCLog;
    SChar             * sGCCommitSCNBuffer;
    smSCN               sCommitSCN;
    UInt                sLogSize;
    idBool              sIsWithCommitSCN = ID_FALSE;

    /* PROJ-2569 xa_prepare_req�α׿� xa_end�α״� sm recovery ������ ���ʿ��ϹǷ�
     * DK���� �Ѱ��ְ� ���� �۾��� skip�Ѵ�. */
    switch ( aLogType )
    {
        case SMR_LT_XA_PREPARE_REQ:

            idlOS::memcpy( &sXaPrepareReqLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE( smrXaPrepareReqLog ) );

            IDE_TEST( processPrepareReqLog( aLogPtr + SMR_LOGREC_SIZE(smrXaPrepareReqLog),
                                            &sXaPrepareReqLog,
                                            aLSN )
                      != IDE_SUCCESS );

            *aRedoSkip = ID_TRUE;

            break;

        case SMR_LT_XA_END:

            idlOS::memcpy( &sXaEndLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE( smrXaEndLog ) );

            IDE_DASSERT( mManageDtxInfoFunc != NULL ); 
            IDE_TEST( mManageDtxInfoFunc( NULL,
                                          smrLogHeadI::getTransID( &(sXaEndLog.mHead) ),
                                          sXaEndLog.mGlobalTxId,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL, //mCommitSCN
                                          SMI_DTX_END )
                      != IDE_SUCCESS );

            *aRedoSkip = ID_TRUE;

            break;

        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_DSKTRANS_COMMIT:

            idlOS::memcpy( &sCommitLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE( smrTransCommitLog ) );

            if ( sCommitLog.mGlobalTxId != SM_INIT_GTX_ID )
            {
                IDE_DASSERT( mManageDtxInfoFunc != NULL ); 
                IDE_TEST( mManageDtxInfoFunc( NULL,
                                              smrLogHeadI::getTransID( &(sCommitLog.mHead) ),
                                              sCommitLog.mGlobalTxId,
                                              0,
                                              NULL,
                                              NULL,
                                              &sCommitLog.mCommitSCN,
                                              SMI_DTX_COMMIT )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            break;

        case SMR_LT_MEMTRANS_GROUPCOMMIT:

            idlOS::memcpy( &sGCLog,  aLogPtr, ID_SIZEOF( smrTransGroupCommitLog ) );
            sGCCnt = sGCLog.mGroupCnt;

            sRedoBuffer = aLogPtr + SMR_LOGREC_SIZE( smrTransGroupCommitLog );

            sLogSize = SMR_LOGREC_SIZE(smrTransGroupCommitLog) + 
                       ( sGCCnt * ID_SIZEOF(smTID) * 2 ) + 
                       ID_SIZEOF(smrLogTail);

            if ( smrLogHeadI::getSize(&sGCLog.mHead) > sLogSize )
            {
                /* CommitSCN �� ��������� �翬�� smSCN ũ���� ��� */
                IDE_DASSERT( ( ( smrLogHeadI::getSize(&sGCLog.mHead) - sLogSize ) % ID_SIZEOF(smSCN) ) == 0 );

                /* TID1 | GlobalTID1 | TID2 | GlobalTI2D | CommitSCN1 | CommitSCN2 | �� ������. */
                sGCCommitSCNBuffer = sRedoBuffer + ( sGCCnt * ID_SIZEOF(smTID) * 2 );

                sIsWithCommitSCN = ID_TRUE;
            }
            else
            {
                sIsWithCommitSCN = ID_FALSE;
            }

            for ( i = 0 ; i < sGCCnt ; i++ )
            {
                idlOS::memcpy( &sTID,
                               sRedoBuffer,
                               ID_SIZEOF(smTID) );
                sRedoBuffer += ID_SIZEOF(smTID);

                idlOS::memcpy( &sGlobalTID,
                               sRedoBuffer,
                               ID_SIZEOF(smTID) );
                sRedoBuffer += ID_SIZEOF(smTID);
    
                if ( sIsWithCommitSCN == ID_TRUE )
                {
                    idlOS::memcpy( &sCommitSCN,
                                   sGCCommitSCNBuffer,
                                   ID_SIZEOF(smSCN) );
                    sGCCommitSCNBuffer += ID_SIZEOF(smSCN);
                }
                else
                {
                    SM_INIT_SCN( &sCommitSCN );     
                }

                if ( sGlobalTID != SM_INIT_GTX_ID )
                {
                    IDE_DASSERT( mManageDtxInfoFunc != NULL ); 
                    IDE_TEST( mManageDtxInfoFunc( NULL,
                                                  sTID,
                                                  sGlobalTID,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  &sCommitSCN,
                                                  SMI_DTX_COMMIT )
                              != IDE_SUCCESS );
                }
            }

            break;

        case SMR_LT_MEMTRANS_ABORT:
        case SMR_LT_DSKTRANS_ABORT:

            idlOS::memcpy( &sAbortLog,
                           aLogPtr,
                           SMR_LOGREC_SIZE( smrTransAbortLog ) );

            if ( sAbortLog.mGlobalTxId != SM_INIT_GTX_ID )
            {
                IDE_DASSERT( mManageDtxInfoFunc != NULL ); 
                IDE_TEST( mManageDtxInfoFunc( NULL,
                                              smrLogHeadI::getTransID( &(sAbortLog.mHead) ),
                                              sAbortLog.mGlobalTxId,
                                              0,
                                              NULL,
                                              NULL,
                                              NULL,  //mCommitSCN
                                              SMI_DTX_ROLLBACK )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            break;

        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

