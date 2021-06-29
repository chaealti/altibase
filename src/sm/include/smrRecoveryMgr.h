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
 * $Id: smrRecoveryMgr.h 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

/***********************************************************************
 *
 * Description :
 *
 * �� ������ ���� �����ڿ� ���� ��� �����̴�.
 *
 * # ���
 *   1. �α׾�Ŀ ���� (����/�ʱ�ȭ/����)
 *   2. �α����� ������ ������ �� üũ����Ʈ ������ �ʱ�ȭ
 *   3. MRDB�� Dirty-Page ����
 *   4. üũ����Ʈ ����
 *   5. Restart Recovery ����
 *   6. Ʈ����� rollback ����
 *   7. **Prj-1149 : media recovery ����
 **********************************************************************/

#ifndef _O_SMR_RECOVERY_MGR_H_
#define _O_SMR_RECOVERY_MGR_H_ 1

#include <idu.h>
#include <idp.h>
#include <smrDef.h>
#include <smriDef.h>
#include <smrLogAnchorMgr.h>
#include <smrDirtyPageList.h>
#include <smrLogHeadI.h>
#include <smrCompareLSN.h>

// BUG-38503
#define SMR_DLT_REDO_NONE       (0)
#define SMR_DLT_REDO_EXT_DBF    (1)
#define SMR_DLT_REDO_ALL_DLT    (2)

class  idxMarshal;
class  smrLogFile;
class  smrLogAnchorMgr;
class  smrDirtyPageList;

class smrRecoveryMgr
{

public:

    static IDE_RC create();     // �α����� �� loganchor ���� ����
    static IDE_RC initialize(); // �ʱ�ȭ
    static IDE_RC destroy();    // ����

    static IDE_RC finalize();

    /* ����� ���� */
    static IDE_RC restart( UInt aPolicy );

    /* Ʈ����� ���� */
    static IDE_RC undoTrans( idvSQL* aStatistics,
                             void*   aTrans,
                             smLSN*  aLSN );

    /* Hashing�� Disk Log���� ���� */
    static IDE_RC applyHashedDiskLogRec( idvSQL* aStatistics );

    // PRJ-1548 User Memory Tablespace

    // �޸�/��ũ TBS �Ӽ��� �α׾�Ŀ�� �����Ѵ�.
    static IDE_RC updateTBSNodeToAnchor( sctTableSpaceNode*  aSpaceNode );
    // ��ũ ����Ÿ���� �Ӽ��� �α׾�Ŀ�� �����Ѵ�.
    static IDE_RC updateDBFNodeToAnchor( sddDataFileNode*    aFileNode );
    // �޸� ����Ÿ���� �Ӽ��� �α׾�Ŀ�� �����Ѵ�.
    static IDE_RC updateChkptImageAttrToAnchor(
                                 smmCrtDBFileInfo   * aCrtDBFileInfo,
                                 smmChkptImageAttr  * aChkptImageAttr );

    static IDE_RC updateSBufferNodeToAnchor( sdsFileNode  * aFileNode );
    // Chkpt Path �Ӽ��� Loganchor�� �����Ѵ�.
    static IDE_RC updateChkptPathToLogAnchor(
                      smmChkptPathNode * aChkptPathNode );

    // �޸�/��ũ ���̺����̽� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�.
    static IDE_RC addTBSNodeToAnchor( sctTableSpaceNode*   aSpaceNode );

    // ��ũ ����Ÿ���� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�.
    static IDE_RC addDBFNodeToAnchor( sddTableSpaceNode*   aSpaceNode,
                                      sddDataFileNode*     aFileNode );

    // �޸� üũ����Ʈ �н� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�.
    static IDE_RC addChkptPathNodeToAnchor( smmChkptPathNode * aChkptPathNode );

    // �޸� ����Ÿ���� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�.
    static IDE_RC addChkptImageAttrToAnchor(
                                 smmCrtDBFileInfo   * aCrtDBFileInfo,
                                 smmChkptImageAttr  * aChkptImageAttr );


    static IDE_RC addSBufferNodeToAnchor( sdsFileNode*   aFileNode );

    // loganchor�� tablespace ���� flush
    static IDE_RC updateAnchorAllTBS();


    static IDE_RC updateAnchorAllSB( void );
    
    // loganchor�� archive �α� ��� flush
    static IDE_RC updateArchiveMode(smiArchiveMode aArchiveMode);

    // loganchor�� TXSEG Entry ������ �ݿ��Ѵ�.
    static IDE_RC updateTXSEGEntryCnt( UInt sEntryCnt );

    /* checkpoint by buffer flush thread */
    static IDE_RC checkpointDRDB(idvSQL* aStatistics);

    /* checkpoint ���� */
    static IDE_RC checkpoint( idvSQL       * aStatistics,
                              SInt           aCheckpointReason,
                              smrChkptType   aChkptType,
                              idBool         aRemoveLogFile = ID_TRUE,
                              idBool         aFinal         = ID_FALSE );

    /* Tablespace�� Checkpoint�� �������� �˻� */
    static idBool isCheckpointable( sctTableSpaceNode * aSpaceNode );



    /* prepare Ʈ����ǿ� ���� table lock�� ���� */
    static IDE_RC acquireLockForInDoubt();

    // �αװ������� ���� ��ȯ
    static inline idBool isFinish()  { return mFinish;  } /* �αװ������� ���� ��ȯ*/
    static inline idBool isRestart() { return mRestart; } /* restart ������ ���� */
    static inline idBool isRedo() { return mRedo; }       /* redo ������ ���� */

    // ���� Recovery������ ���θ� �����Ѵ�.
    static inline idBool isRestartRecoveryPhase() { return mRestartRecoveryPhase; } /* Restart Recovery ���������� ���� */
    static inline idBool isMediaRecoveryPhase()   { return mMediaRecoveryPhase; }   /* �̵�� ���� ���� �� ���� */

    static inline idBool isVerifyIndexIntegrityLevel2();

    // PR-14912
    static inline idBool isRefineDRDBIdx() { return mRefineDRDBIdx; }

    /* ���۷α� ���� ��ȯ */
    static inline idBool isBeginLog( smrLogHead* aLogHead );

    /* BUG-26482 ��� �Լ��� CommitLog ��� ���ķ� �и��Ͽ� ȣ���մϴ�. */
    static void setCallbackFunction(
                    smGetMinSN                   aGetMinSN,
                    smIsReplCompleteBeforeCommit aIsReplCompleteBeforeCommitFunc,
                    smIsReplCompleteAfterCommit  aIsReplCompleteAfterCommitFunc,
                    smCopyToRPLogBuf             aCopyToRPLogBufFunc,
                    smSendXLog                   aSendXLogFunc,
                    smIsReplWaitGlobalTxAfterPrepare aIsReplWaitGlobalTxAfterPrepareFunc );

    static inline smLSN  getIdxSMOLSN()       { return mIdxSMOLSN;}
    static inline idBool isABShutDown()       { return mABShutDown; }

    static inline smrLogAnchorMgr *getLogAnchorMgr() { return &mAnchorMgr; }

    static UInt getLstDeleteLogFileNo();

    /* PROJ-2102 */
    static void getEndLSN(smLSN* aLSN)
    { 
        mAnchorMgr.getEndLSN( aLSN );
    }

    static smiArchiveMode getArchiveMode();

    /************************************
    * PROJ-2133 incremental backup begin
    ************************************/
    static idBool isCTMgrEnabled();
 
    static idBool isCreatingCTFile();

    static smriBIMgrState getBIMgrState();

    static SChar* getCTFileName();
    static SChar* getBIFileName();

    static smLSN getCTFileLastFlushLSNFromLogAnchor();
    static smLSN getBIFileLastBackupLSNFromLogAnchor();
    static smLSN getBIFileBeforeBackupLSNFromLogAnchor();

    static IDE_RC changeIncrementalBackupDir( SChar * aNewBackupDir );

    static SChar *  getIncrementalBackupDirFromLogAnchor();

    static IDE_RC updateCTFileAttrToLogAnchor(                        
                                      SChar          * aFileName,    
                                      smriCTMgrState * aCTMgrState,  
                                      smLSN          * aFlushLSN );

    static IDE_RC updateBIFileAttrToLogAnchor(                        
                                      SChar          * aFileName,    
                                      smriBIMgrState * aBIMgrState,  
                                      smLSN          * aBackupLSN,
                                      SChar          * aBackupDir,
                                      UInt           * aDeleteArchLogFile );

    static IDE_RC getDataFileDescSlotIDFromLogAncho4ChkptImage(     
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID);

    static IDE_RC getDataFileDescSlotIDFromLogAncho4DBF(     
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID);
    /************************************
    * PROJ-2133 incremental backup end
    ************************************/

    static UInt getTXSEGEntryCnt();
    static UInt getSmVersionIDFromLogAnchor();
    static void getDiskRedoLSNFromLogAnchor(smLSN* aDiskRedoLSN);

    // prj-1149 checkpoint ����
    static IDE_RC makeBufferChkpt( idvSQL      * aStatistics,
                                   idBool        aIsFinal,
                                   smLSN       * aEndLSN,
                                   smLSN       * aOldestLSN );

    //* Sync Replication extension * //
    /* BUG-26482 ��� �Լ��� CommitLog ��� ���ķ� �и��Ͽ� ȣ���մϴ�. */
    static smIsReplCompleteBeforeCommit mIsReplCompleteBeforeCommitFunc; // Sync LSN wait for (Before Write CommitLog)
    static smIsReplCompleteAfterCommit  mIsReplCompleteAfterCommitFunc;  // Sync LSN wait for (After Write CommitLog)

    /* PRJ-2747 Global Tx Consistent */
    static smIsReplWaitGlobalTxAfterPrepare mIsReplWaitGlobalTxAfterPrepareFunc;

    /*PROJ-1670 RP LOG Buffer*/
    static smCopyToRPLogBuf   mCopyToRPLogBufFunc;
    /* PROJ-2453 Eager Replication performance enhancement */ 
    static smSendXLog   mSendXLogFunc;
    /* Log File������ mDeleteLogFileMutex�� ��ƾ� �Ѵ�. �ֳ��ϸ� Replication��
       LogFile�� Scaning �ϸ鼭 �ڽ��� ������ ������ ��ġ�� ã���� log������ �����Ǵ�
       ���� �����ϱ� ���� ������ mDeleteLogFileMutex�� ���ؼ� lock�� ��� ����
       Replication�� ������ �� ù��° ������ ã�� �� ��ƾ� �Ѵ�.*/
    static inline IDE_RC lockDeleteLogFileMtx()
                                     { return mDeleteLogFileMutex.lock( NULL  ); }
    static inline IDE_RC unlockDeleteLogFileMtx()  
                                           { return mDeleteLogFileMutex.unlock();}
    static inline void getLstDeleteLogFileNo(UInt *aFileNo) 
                                    { mAnchorMgr.getLstDeleteLogFileNo(aFileNo); }
    /*proj-1608 recovery from replication*/
    static inline smLSN getReplRecoveryLSN() 
                                       { return mAnchorMgr.getReplRecoveryLSN(); }
    static inline IDE_RC setReplRecoveryLSN( smLSN aReplRecoveryLSN )
                  { return mAnchorMgr.updateReplRecoveryLSN( aReplRecoveryLSN ); }
    // Decompress Log Buffer ũ�Ⱑ
    // ������Ƽ�� ������ ������ Ŭ ��� Hashing�� Disk Log���� ����
    static IDE_RC checkRedoDecompLogBufferSize() ;

    /////////////////////////////////////////////////////////////////
    // ALTER TABLESPACE ONLINE/OFFLINE���� �Լ�

    // ALTER TABLESPACE OFFLINE�� ����
    // ��� Tablespace�� ��� Dirty Page�� Flush
    static IDE_RC flushDirtyPages4AllTBS();

    // Ư�� Tablespace�� Dirty Page�� ������ Ȯ���Ѵ�.
    static IDE_RC assertNoDirtyPagesInTBS( smmTBSNode * aTBSNode );

    /////////////////////////////////////////////////////////////////
    // �̵�� ���� ���� �Լ�

    /* �߰��� ���� �α������� �ִ��� �˻� */
    static IDE_RC identifyLogFiles();

    /* �̵����� �ʿ����� �˻� */
    static IDE_RC identifyDatabase( UInt aActionFlag );

    // PRJ-1149
    static IDE_RC recoverDB(idvSQL*           aStatistics,
                            smiRecoverType    aRecvType,
                            UInt              aUntilTIME);

    // incomplete media recovery ���� �α����� reset ����
    static IDE_RC resetLogFiles();

    // resetlogfile �������� �α������� �����Ѵ�.
    static IDE_RC deleteLogFiles( SChar   * aDirPath,
                                  UInt      aBeginLogFileNo );

    static idBool isCheckpointFlushNeeded(smLSN aLastWrittenLSN);

    // SKIP�� REDO�α����� ���θ� �����Ѵ�.
    static idBool isSkipRedo( scSpaceID aSpaceID,
                              idBool    aUsingTBSAttr = ID_FALSE);

    static void writeDebugInfo();

    /* DRDB Log�� �м��Ͽ� DRDB RedoLog�� ���� ��ġ�� ���̸�
     * ��ȯ�Ѵ�. */
    static void getDRDBRedoBuffer( smrLogType    aLogType,
                                   UInt          aLogTotalSize,
                                   SChar       * aLogPtr,
                                   UInt        * aRedoLogSize,
                                   SChar      ** aRedoBuffer );

    /*****************************************************************
     * PROJ-2162 RestartRiskReduction
     *
     * �ֿ� �Լ���
     *****************************************************************/
    /* Log�� �޾� �м��Ͽ� �� Log�� Recovery�� ��� ��ü���� �����մϴ�. */
    static void prepareRTOI( void                * aLogPtr,
                             smrLogHead          * aLogHeadPtr,
                             smLSN               * aLSN,
                             sdrRedoLogData      * aRedoLogData,
                             UChar               * aPagePtr,
                             idBool                aIsRedo,
                             smrRTOI             * aObj );

    /* TargetObject�� �޾� Inconsistent���� üũ. */
    static void checkObjectConsistency( smrRTOI * aObj,
                                        idBool  * aConsistency);
    /* Recovery�� ������ ��Ȳ�Դϴ�. */
    static IDE_RC startupFailure( smrRTOI * aObj,
                                  idBool    aIsRedo );

    /* TargetObject�� Inconsistent�ϴٰ� ������ */
    static void setObjectInconsistency( smrRTOI * aObj,
                                        idBool    aIsRedo );

    /*****************************************************************
     * PROJ-2162 RestartRiskReduction
     *
     * get/set/check �Լ���
     *****************************************************************/
    /* RTOI ��ü�� �ʱ�ȭ�Ѵ�. */
    static void initRTOI( smrRTOI * aObj );

    /* DRDB Wal�� �������� Ȯ���մϴ�. */
    static IDE_RC checkDiskWAL();

    /* MRDB Wal�� �������� Ȯ���մϴ�. */
    static IDE_RC checkMemWAL();

    /* DB Inconsistency ������ ���� �غ� �մϴ�.
     * Flusher�� ����ϴ�. */
    static IDE_RC prepare4DBInconsistencySetting();

    /* DB Inconsistency ������ ������ �մϴ�.
     * Flusher�� �籸���մϴ�. */
    static IDE_RC finish4DBInconsistencySetting();

    /* Idempotent�� Log���� Ȯ���մϴ�. */
    static idBool isIdempotentLog( smrLogType sLogType );

    /* Property���� �����϶�� �� ��ü���� ������ */
    static idBool isIgnoreObjectByProperty( smrRTOI * aObj);

    /* RTOI�� Inconsistency ���� üũ�ϴ� ���� �Լ� */
    static idBool checkObjectConsistencyInternal( smrRTOI * aObj );

    /* Emergency Startup ���� �� ��ȸ�� ���� get �Լ� */
    static smrRTOI * getIOLHead()            { return &mIOLHead; }
    static idBool    getDBDurability()       { return mDurability; }
    static idBool    getDRDBConsistency()    { return mDRDBConsistency; }
    static idBool    getMRDBConsistency()    { return mMRDBConsistency; }
    static idBool    getConsistency()
    {
        if ( ( mDRDBConsistency == ID_TRUE ) &&
             ( mMRDBConsistency == ID_TRUE ) )
        {
            return ID_TRUE;
        }
        return ID_FALSE;
    }
    static smrEmergencyRecoveryPolicy getEmerRecovPolicy()
        { return mEmerRecovPolicy; }

    /*****************************************************************
     * PROJ-2162 RestartRiskReduction
     *
     * Undo/Refine �� ���� ���� �Լ���� ���и� ó���ϴ� ���
     *****************************************************************/

    /* Undo���п� ���� RTOI�� �غ��� */
    static void prepareRTOIForUndoFailure( void        * aTrans,
                                           smrRTOIType   aType,
                                           smOID         aTableOID,
                                           UInt          aIndexID,
                                           scSpaceID     aSpaceID,
                                           scPageID      aPageID );

    /* Refine���з� �ش� Table�� Inconsistent ���� */
    static IDE_RC refineFailureWithTable( smOID   aTableOID );

    /* Refine���з� �ش� Index�� Inconsistent ���� */
    static IDE_RC refineFailureWithIndex( smOID   aTableOID,
                                          UInt    aIndexID );

    /*****************************************************************
     * PROJ-2162 RestartRiskReduction
     *
     * IOL(InconsistentObjectList)�� �ٷ�� �Լ���
     *****************************************************************/
    /* IOL�� �ʱ�ȭ �մϴ�. */
    static IDE_RC initializeIOL();

    /* IOL�� �߰� �մϴ�. */
    static void addIOL( smrRTOI * aObj );

    /* �̹� ��ϵ� ��ü�� �ִ���, IOL���� ã���ϴ�. */
    static idBool findIOL( smrRTOI * aObj);

    /* RTOI Message�� TRC/isql�� ����մϴ�. */
    static void displayRTOI( smrRTOI * aObj );

    /* Redo������ ����� �Ǿ����� �̷�� Inconsistency�� ������ */
    static void applyIOLAtRedoFinish();

    /* IOL�� ���� �մϴ�. */
    static IDE_RC finalizeIOL();
    /*****************************************************************
     * PROJ-2162 RestartRiskReduction End
     *****************************************************************/

    
    /************************************
    * PROJ-2133 incremental backup begin
    ************************************/
    static IDE_RC restoreDB( smiRestoreType    aRestoreType,
                             UInt              aUntilTIME,  
                             SChar           * aUntilBackupTag );

    static IDE_RC restoreTBS( scSpaceID aSpaceID );


    static IDE_RC restoreDataFile4Level0( smriBISlot * aBISlot );

    static IDE_RC restoreDB4Level1( UInt            aRestoreStartSlotIdx, 
                                    smiRestoreType  aRestoreType,
                                    UInt            aUntilTime,
                                    SChar         * aUntilBackupTag,
                                    smriBISlot   ** aLastRestoredBISlot );

    static IDE_RC restoreTBS4Level1( scSpaceID aSpaceID,
                                     UInt      aRestoreStartBISlotIdx,
                                     UInt      aBISlotCnt );

    static UInt getPageNoInFile( UInt aPageID, smriBISlot * aBISlot);

    static IDE_RC restoreMemDataFile4Level1( smriBISlot * aBISlot );

    static IDE_RC restoreDiskDataFile4Level1( smriBISlot * aBISlot );

    static UInt getCurrMediaTime()
        { return mCurrMediaTime; }

    static void setCurrMediaTime ( UInt aRestoreCompleteTime )
        { mCurrMediaTime = aRestoreCompleteTime; }

    static SChar * getLastRestoredTagName()
        { return mLastRestoredTagName; }
    
    static void setLastRestoredTagName( SChar * aTagName )
        { idlOS::strcpy( mLastRestoredTagName, aTagName ); }

    static void initLastRestoredTagName()
        { 
            idlOS::memset( mLastRestoredTagName, 
                           0x00, 
                           SMI_MAX_BACKUP_TAG_NAME_LEN ); 
        }
    /************************************
    * PROJ-2133 incremental backup end
    ************************************/

    // Memory Dirty Page���� Checkpoint Image�� Flush�Ѵ�.
    // END CHKPT LOG ������ �����ϴ� �۾�
    // PROJ-1923 private -> public ���� ����
    static IDE_RC chkptFlushMemDirtyPages( smLSN          * aSyncLstLSN,
                                           idBool           aIsFinal );
    /* PROJ-2569 */
    static inline void set2PCCallback( smGetDtxMinLSN aGetDtxMinLSN,
                                       smManageDtxInfo aManageDtxInfo );

    static smGetDtxMinLSN    mGetDtxMinLSNFunc;
    static smManageDtxInfo   mManageDtxInfoFunc;
    static IDE_RC processPrepareReqLog( SChar * aLogPtr,
                                        smrXaPrepareReqLog * aXaPrepareReqLog,
                                        smLSN * aLSN );
    static IDE_RC processDtxLog( SChar      * aLogPtr,
                                 smrLogType   aLogType,
                                 smLSN      * aLSN,
                                 idBool     * aRedoSkip );

    /* BUG-42785 OnlineDRDBRedo�� LogFileDeleteThread�� ���ü� ����
     * mOnlineDRDBRedoCnt ���������� ���� */
    static SInt getOnlineDRDBRedoCnt()
    {
        return idCore::acpAtomicGet32( &mOnlineDRDBRedoCnt );
    }
    static IDE_RC incOnlineDRDBRedoCnt()
    {
        SInt sOnlineDRDBRedoCnt;

        sOnlineDRDBRedoCnt = idCore::acpAtomicInc32( &mOnlineDRDBRedoCnt );

        IDE_TEST( sOnlineDRDBRedoCnt > ID_SINT_MAX );

        return IDE_SUCCESS;

        IDE_EXCEPTION_END;
        
        return IDE_FAILURE;
    }
    static IDE_RC decOnlineDRDBRedoCnt()
    {
        SInt sOnlineDRDBRedoCnt;

        sOnlineDRDBRedoCnt = idCore::acpAtomicDec32( &mOnlineDRDBRedoCnt );

        IDE_TEST( sOnlineDRDBRedoCnt < 0 );
        
        return IDE_SUCCESS;
        
        IDE_EXCEPTION_END;

        return IDE_FAILURE;
    }

    static void getLastRemovedFileNo( UInt *aFailNo )
    {
        *aFailNo = mLastRemovedFileNo;
    }

private:

    static IDE_RC applyDskLogInstantly4ActiveTrans( void        * aCurTrans,
                                                    SChar       * aLogPtr,
                                                    smrLogHead  * aLogHeadPtr,
                                                    smLSN       * aCurRedoLSNPtr );

    // �α׾�Ŀ�� resetloglsn�� �����Ǿ� �ִ��� Ȯ���Ѵ�.
    static IDE_RC checkResetLogLSN( UInt aActionFlag );

    /* ������� ���ϸ�� ���� */
    static IDE_RC makeMediaRecoveryDBFList4AllTBS(
                                smiRecoverType    aRecoveryType,
                                UInt            * aFailureMediaType,
                                smLSN           * aFromDiskRedoLSN,
                                smLSN           * aToDiskRedoLSN,
                                smLSN           * aFromMemRedoLSN,
                                smLSN           * aToMemRedoLSN );

    // PROJ-1867 ��� �Ǿ��� DBFile���� MustRedoToLSN���� �����´�.
    static IDE_RC getMaxMustRedoToLSN( idvSQL     * aStatistics,
                                       smLSN      * aMustRedoToLSN,
                                       SChar     ** sDBFileName );

    // �޸� ����Ÿ������ ����� �����Ѵ�
    static IDE_RC repairFailureChkptImageHdr( smLSN  * aResetLogsLSN );

    // �޸� ���̺����̽��� �̵����� ����
    // ���̺����̽��� �ε��Ѵ�.
    static IDE_RC initMediaRecovery4MemTBS();

    // �޸� ���̺����̽��� �̵����� ����
    // ���̺����̽��� �޸������Ѵ�.
    static IDE_RC finalMediaRecovery4MemTBS();

    // �̵��� �Ϸ�� �޸� Dirty Pages�� ��� Flush ��Ų��.
    static IDE_RC flushAndRemoveDirtyPagesAllMemTBS();

    // To Fix PR-13786
    static IDE_RC recoverAllFailureTBS( smiRecoverType       aRecoveryType,
                                        UInt                 aFailureMediaType,
                                        time_t             * aUntilTIME,
                                        smLSN              * aCurRedoLSNPtr,
                                        smLSN              * aFromDiskRedoLSN,
                                        smLSN              * aToDiskRedoLSN,
                                        smLSN              * aFromMemRedoLSN,
                                        smLSN              * aToMemRedoLSN );

    // �̵����� �ǵ��� Log LSN�� scan�� ������ ��´�
    static void getRedoLSN4SCAN( smLSN * aFromDiskRedoLSN,
                                 smLSN * aToDiskRedoLSN,
                                 smLSN * aFromMemRedoLSN,
                                 smLSN * aToMemRedoLSN,
                                 smLSN * aMinFromRedoLSN );

    // �ǵ��� �αװ� common �α�Ÿ������ Ȯ���Ѵ�.
    static IDE_RC filterCommonRedoLogType( smrLogType   aLogType,
                                           UInt         aFailureMediaType,
                                           idBool     * aIsApplyLog );

    // �ǵ��� �αװ� ������ �޸� ����Ÿ������ ������ ���ԵǴ���
    // �Ǵ��Ͽ� ���뿩�θ� ��ȯ�Ѵ�.
    static IDE_RC filterRedoLog4MemTBS( SChar      * aCurLogPtr,
                                        smrLogType   aLogType,
                                        idBool     * aIsApplyLog );

    // �ǵ��� �αװ� ��ũ �α�Ÿ�������� �켱Ȯ���ϰ� ���뿩�θ�
    // �Ǵ��Ͽ� ��ȯ�Ѵ�.
    static IDE_RC filterRedoLogType4DiskTBS( SChar      * aCurLogPtr,
                                             smrLogType   aLogType,
                                             UInt         aIsNeedApplyDLT,
                                             idBool     * aIsApplyLog );

    //   fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline
    static IDE_RC finiOfflineTBS( idvSQL * aStatistics );

    /* ------------------------------------------------
     * !!] system restart ���� �Լ�
     * ----------------------------------------------*/
    static IDE_RC initRestart(idBool* aIsNeedRecovery);
    static IDE_RC restartNormal();   /* ������ �ʿ���� restart */
    static IDE_RC restartRecovery(); /* ������ �ʿ��� restart */
    static void   finalRestart();    /* restart ���� ������ */

    static IDE_RC redo( void        * aCurTrans,
                        smLSN       * aCurLSN,
                        UInt        * aFileCount,
                        smrLogHead  * aLogHead,
                        SChar       * aLogPtr,
                        UInt          aLogSizeAtDisk,
                        idBool        aAfterChkpt );

    static IDE_RC addActiveTrans( smrLogHead    * aLogHeadPtr,
                                  SChar         * aLogPtr,
                                  smTID           aTID,
                                  smLSN         * aCurRedoLSNPtr,
                                  void         ** aCurTrans );

    // To Fix PR-13786

    static IDE_RC redo_FILE_END( smLSN      * aCurLSN,
                                 UInt       * aFileCount );


    /* restart recovery ������ ����� ���� ���� */
    static IDE_RC redoAll(idvSQL* aStatistics);

    /* restart recovery ������ undoall pass ���� */
    static IDE_RC undoAll(idvSQL* aStatistics);

    /* Ʈ����� öȸ */
    static IDE_RC undo( idvSQL*       aStatistics,
                        void*         aTrans,
                        smrLogFile**  aLogFilePtr);

    static IDE_RC rebuildArchLogfileList( smLSN  *aEndLSN );

    /* ------------------------------------------------
     * To Fix PR-13786 Cyclomatic Number
     * CHECKPOINT ���� �Լ�
     * ----------------------------------------------*/
    // Checkpoint ���� �Լ� - ���� Checkpoint�� �����Ѵ�.
    static IDE_RC checkpointInternal( idvSQL*      aStatistics,
                                      smrChkptType aChkptType,
                                      idBool       aRemoveLogFile,
                                      idBool       aFinal );

    // Checkpoint�� �� �ϰ� �Ǿ������� altibase_sm.log�� �����.
    static IDE_RC logCheckpointReason( SInt aCheckpointReason );

    // Checkpoint ��� Message�� �α��Ѵ�.
    static IDE_RC logCheckpointSummary( smLSN   aBeginChkptLSN,
                                        smLSN   aEndChkptLSN,
                                        smLSN * aRedoLSN,
                                        smLSN   aDiskRedoLSN );

    // Begin Checkpoint Log�� ����Ѵ�.
    static IDE_RC writeBeginChkptLog( idvSQL* aStatistics,
                                      smLSN * aRedoLSN,
                                      smLSN   aDiskRedoLSN,
                                      smLSN   aEndLSN,
                                      smLSN * aBeginChkptLSN,
                                      smLSN * aDtxMinLSN,
                                      idBool  aFinal );


    // End Checkpoint Log�� ����Ѵ�.
    static IDE_RC writeEndChkptLog( idvSQL* aStatistics,
                                    smLSN * aEndChkptLSN );

    // Restart Recovery�ÿ� Redo���� LSN���� ���� Recovery LSN�� ���
    // BEGIN CHKPT LOG ������ �����ϴ� �۾�
    static IDE_RC chkptCalcRedoLSN( idvSQL*            aStatistics,
                                    smrChkptType       aChkptType,
                                    idBool             aIsFinal,
                                    smLSN            * aRedoLSN,
                                    smLSN            * aDiskRedoLSN,
                                    smLSN            * aEndLSN );

    // ��� ���̺����̽��� ����Ÿ���� ��Ÿ����� RedoLSN ����
    static IDE_RC chkptSetRedoLSNOnDBFiles( idvSQL* aStatistics,
                                            smLSN * aRedoLSN,
                                            smLSN   aDiskRedoLSN );

    // END CHKPT LOG ���Ŀ� �����ϴ� �۾�
    static IDE_RC chkptAfterEndChkptLog( idBool             aRemoveLogFile,
                                         idBool             aFinal,
                                         smLSN            * aBeginChkptLSN,
                                         smLSN            * aEndChkptLSN,
                                         smLSN            * aDiskRedoLSN,
                                         smLSN            * aRedoLSN,
                                         smLSN            * aSyncLstLSN,
                                         smLSN            * aDtxMinLSN );

    // BUG-20229
    static IDE_RC resizeLogFile(SChar    *aLogFileName,
                                ULong     aLogFileSize);

    static idBool existDiskSpace4LogFile(void);

    static void updateLastRemovedFileNo( UInt aFileNo )
    {
        mLastRemovedFileNo = aFileNo ;
    }

private:

    static smrLogAnchorMgr    mAnchorMgr;  /* loganchor ������ */
    static smrDirtyPageList   mDPPList;    /* dirty page list */
    static sdbFlusher         mFlusher;
    static idBool             mFinish;     /* �αװ������� �������� */
    static idBool             mABShutDown; /* ������ ������ ���� */

    /* restart ������ ���� */
    static idBool             mRestart;

    /* redo ������ ���� */
    static idBool             mRedo;

    /* �̵�� ���� ���� �� ���� */
    static idBool             mMediaRecoveryPhase;

    /* Restart Recovery ���������� ���� */
    static idBool             mRestartRecoveryPhase;

    // PR-14912
    static idBool             mRefineDRDBIdx;

    static UInt               mLogSwitch;
    /* Replication�� ���ؼ� �ʿ��� �α� ���ڵ���� ����
       ���� SN���� �����´�. */
    static smGetMinSN         mGetMinSNFunc;

    /* for index smo */
    static smLSN              mIdxSMOLSN;

    /* Log File Delete�� ��� Mutex */
    static iduMutex           mDeleteLogFileMutex;

    /* BUG-42785 OnlineDRDBRedo�� LogFileDeleteThread�� ���ü� ����
     * mOnlineDRDBRedoCnt�� ���� OnlineDRDBRedo�� �����ϰ� �ִ� 
     * Ʈ������� ���� ��Ÿ����
     * OnlineDRDBRedo�� ����Ǵ� ������ LogFile�� �������� �ȵȴ�.
     * ���� mOnlineDRDBRedoCnt�� 0�϶��� 
     * LogFileDeleteThread�� �����Ͽ��� �Ѵ�. */
    static SInt               mOnlineDRDBRedoCnt;

    // PROJ-2118 BUG Reporting - Debug Info for Fatal
    static smLSN         mLstRedoLSN;
    static smLSN         mLstUndoLSN;
    static SChar      *  mCurLogPtr;
    static smrLogHead *  mCurLogHeadPtr;


    /*****************************************************************
     * PROJ-2162 RestartRiskReduction Begin
     *
     * EmergencyStartup�� ���� ��å �� �������Դϴ�.
     *****************************************************************/

    /* ��� ���� ��å ( 0 ~ 2 ) */
    static smrEmergencyRecoveryPolicy mEmerRecovPolicy;

    static idBool mDurability;             // Durability�� �ٸ���?
    static idBool mDRDBConsistency;        // DRDB�� Consistent�Ѱ�?
    static idBool mMRDBConsistency;        // MRDB�� Consistent�Ѱ�?
    static idBool mLogFileContinuity;      // LogFile�� �������ΰ�?
    static SChar  mLostLogFile[SM_MAX_FILE_NAME]; // ���� LogFile�̸�

    /* EndCheckpoint Log�� ��ġ */
    static smLSN  mEndChkptLSN;           // MMDB WAL Check�� ����

    static smLSN mLstDRDBRedoLSN;         // DRDB WAL Check�� ����

    /* Inconsistent�� Object���� ��Ƶδ� ��. MemMge�� �̿��� �Ҵ��� */
    static smrRTOI  mIOLHead; /* Inconsistent Object List */
    static iduMutex mIOLMutex;/* IOL Attach�� Mutex */
    static UInt     mIOLCount;/* ������� �߰��� IOL Node ���� */
    /*****************************************************************
     * PROJ-2162 RestartRiskReduction End
     *****************************************************************/

    /* PROJ-2133 incremental backup */
    static SChar  mLastRestoredTagName[ SMI_MAX_BACKUP_TAG_NAME_LEN ];
    static UInt   mCurrMediaTime;

    static smLSN    mSkipRedoLSN;
  
    /* PROJ-2742 Support data integrity after fail-back on 1:1 consistent replication */
    static UInt     mLastRemovedFileNo;
};

/***********************************************************************
 * Description : Ʈ������� ù��° �α� ���� ��ȯ
 **********************************************************************/
inline idBool smrRecoveryMgr::isBeginLog(smrLogHead * aLogHead)
{

    IDE_DASSERT( aLogHead != NULL );

    if ( ( smrLogHeadI::getFlag(aLogHead) & SMR_LOG_BEGINTRANS_MASK ) ==
         SMR_LOG_BEGINTRANS_OK)
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }

}

/***********************************************************************
 *
 * Description : Restart Recovery �������� ActiveƮ����ǰ� ������ �ε���
 *               ���Ἲ���� ���θ� ��ȯ�Ѵ�.
 *
 **********************************************************************/
idBool smrRecoveryMgr::isVerifyIndexIntegrityLevel2()
{
    if ( ( isRestartRecoveryPhase() == ID_TRUE ) &&
         ( smuProperty::getCheckDiskIndexIntegrity() 
           == SMU_CHECK_DSKINDEX_INTEGRITY_LEVEL2 ) )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

/***********************************************************************
 * Description : Idempotent�� ������ ������ �ϴ� Log�ΰ�?
 **********************************************************************/
inline idBool smrRecoveryMgr::isIdempotentLog( smrLogType sLogType )
{
    idBool sRet = ID_FALSE;
    switch( sLogType )
    {
    case SMR_LT_UPDATE:
    case SMR_LT_NTA:
    case SMR_LT_COMPENSATION:
        sRet = ID_TRUE;
        break;
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
    case SMR_DLT_UNDOABLE:
    case SMR_DLT_NTA:
    case SMR_DLT_COMPENSATION:
    case SMR_DLT_REF_NTA:
    case SMR_LT_TABLE_META:
        sRet = ID_FALSE;
        break;
    default:
        IDE_DASSERT(0);
        break;
    }

    return sRet;
}

/***********************************************************************
 * Description : DRDB Log�� �м��Ͽ� DRDB RedoLog�� ���� ��ġ�� ���̸�
 *               ��ȯ�Ѵ�.
 **********************************************************************/
inline void smrRecoveryMgr::getDRDBRedoBuffer( smrLogType    aLogType,
                                               UInt          aLogTotalSize,
                                               SChar       * aLogPtr,
                                               UInt        * aRedoLogSize,
                                               SChar      ** aRedoBuffer )
{
    switch( aLogType )
    {
    case SMR_DLT_REDOONLY:
    case SMR_DLT_UNDOABLE:
        (*aRedoBuffer) = aLogPtr + SMR_LOGREC_SIZE( smrDiskLog );
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrDiskLog, mRedoLogSize ),
                       ID_SIZEOF( UInt ) );
        break;
    case SMR_DLT_NTA:
        (*aRedoBuffer) = aLogPtr + SMR_LOGREC_SIZE( smrDiskNTALog );
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrDiskNTALog, mRedoLogSize ),
                       ID_SIZEOF( UInt ) );
        break;
    case SMR_DLT_REF_NTA:
        (*aRedoBuffer) = aLogPtr + SMR_LOGREC_SIZE( smrDiskRefNTALog );
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrDiskRefNTALog, mRedoLogSize ),
                       ID_SIZEOF( UInt ) );
        break;
    case SMR_DLT_COMPENSATION:
        (*aRedoBuffer) = aLogPtr + SMR_LOGREC_SIZE( smrDiskCMPSLog );
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrDiskCMPSLog, mRedoLogSize ),
                       ID_SIZEOF( UInt ) );
        break;
    case SMR_LT_DSKTRANS_COMMIT:
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrTransCommitLog, mDskRedoSize ),
                       ID_SIZEOF( UInt ) );
        (*aRedoBuffer) = aLogPtr + aLogTotalSize
                         - (*aRedoLogSize) - ID_SIZEOF(smrLogTail);
        break;
    case SMR_LT_DSKTRANS_ABORT:
        (*aRedoBuffer) = aLogPtr + SMR_LOGREC_SIZE( smrTransAbortLog );
        idlOS::memcpy( aRedoLogSize,
                       aLogPtr + offsetof( smrTransAbortLog, mDskRedoSize ),
                       ID_SIZEOF( UInt ) );
        break;
    default:
        /* DRDB�� �ƴϰų�, �Ұ� ���� */
        (*aRedoLogSize) = 0;
        break;
    }
}

inline void smrRecoveryMgr::set2PCCallback( smGetDtxMinLSN aGetDtxMinLSN,
                                            smManageDtxInfo aManageDtxInfo )
{
    mGetDtxMinLSNFunc  = aGetDtxMinLSN;
    mManageDtxInfoFunc = aManageDtxInfo;
}

#endif /* _O_SMR_RECOVERY_MGR_H_ */
