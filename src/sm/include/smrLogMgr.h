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
 * $Id: smrLogMgr.h 88191 2020-07-27 03:08:54Z mason.lee $
 *
 * Description :
 *
 * �αװ����� ��� ���� �Դϴ�. 
 *
 * �α״� �� �������� ��� �ڶ󳪴�, Durable�� ��������̴�.
 * ������ Durable�� ��ü�� ���������� ����ϴ� Disk�� ���,
 * �� �뷮�� �����Ǿ� �־, �α׸� ������ �ڶ󳪰� ����� ���� ����.
 *
 * �׷��� �������� �������� �α����ϵ��� �������� �ϳ��� �α׷�
 * ����� �� �ֵ��� ������ �ϴµ�,
 * �� �� �ʿ��� �������� �α������� smrLogFile �� ǥ���Ѵ�.
 * �������� �α����ϵ��� ����Ʈ�� �����ϴ� ������ smrLogFileMgr�� ����ϰ�,
 * �α������� Durable�� �Ӽ��� ������Ű�� ������ smrLFThread�� ����Ѵ�.
 * �������� �������� �α����ϵ���
 * �ϳ��� ������ �α׷� �߻�ȭ �ϴ� ������ smrLogMgr�� ����Ѵ�.
 *
 * # ���
 * 1. �α׷��ڵ� Ÿ�Ժ� �α�
 * 2. Logging ���� ����
 * 3. Durability ���� �� Logging ������ ���� �α� ó��
 * 4. �α����� ��ȯ
 * 5. Synced Last LSN �� Last LSN ����
 *
 * #  �����ϴ� Thread��
 *
 * 1. �α����� Prepare ������ - smrLogFileMgr
 *    STARTUP  :startupLogPrepareThread
 *    SHUTDOWN :shutdown
 *
 * 2. �α����� Sync ������ - smrLFThread
 *    STARTUP  :initialize
 *    SHUTDOWN :shutdown
 *
 * 3. �α����� Archive ������ - smrArchThread
 *    STARTUP  :startupLogArchiveThread
 *    SHUTDOWN :shutdown
 *
 *    ����ڰ� ��������� ���۽�ų �� :
 *              startupLogArchiveThread
 *
 *    ����ڰ� ��������� ������ų �� :
 *              shutdownLogArchiveThread
 * 
 **********************************************************************/

#ifndef _O_SMR_LOG_MGR_H_
#define _O_SMR_LOG_MGR_H_ 1

#include <idu.h>
#include <idp.h>
#include <iduMemoryHandle.h>

#include <sctDef.h>
#include <smrDef.h>
#include <smrLogFile.h>
#include <smrLogFileMgr.h>
#include <smrLFThread.h>
#include <smrArchThread.h>
#include <smrLogHeadI.h>
#include <smrCompResPool.h>
#include <smrLogComp.h>             /* BUG-35392 */
#include <smrUCSNChkThread.h>       /* BUG-35392 */


#define SMR_CHECK_LSN_UPDATE_GROUP   (32) // FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1 �� ��� dummyLog �� �߻��Ҽ� ����.
                                          // 32�� trans�� ��Ƽ� �α׸� ������� Ʈ������� ���� ����Ѵ�. 
                                          // Group Count=(������� Ʈ������� ��)�� 0�� Group�� �˻����� �ʰ� skip. 
 

struct smuDynArrayBase;

class smrLogMgr
{
public:

    static IDE_RC initialize();
    static IDE_RC destroy();
    
    static IDE_RC initializeStatic(); 
    static IDE_RC destroyStatic();

    // createDB �� ȣ�� 
    // �α����ϵ��� ���ʷ� �����Ѵ�.
    static IDE_RC create();
    // �α� ���� �Ŵ����� ���� ��������� ���� 
    static IDE_RC shutdown();

/***********************************************************************
 *
 * Description : �α�Ÿ������ �α��� �޸�/��ũ ���ÿ��θ� ��ȯ�Ѵ�.
 *
 * [IN] aLogType : �α�Ÿ��
 *
 ***********************************************************************/
    static inline idBool isDiskLogType( smrLogType aLogType )
    {
        if ( (aLogType == SMR_DLT_REDOONLY)       ||
             (aLogType == SMR_DLT_UNDOABLE)       ||
             (aLogType == SMR_DLT_NTA)            ||
             (aLogType == SMR_DLT_REF_NTA)        ||
             (aLogType == SMR_DLT_COMPENSATION)   ||
             (aLogType == SMR_LT_DSKTRANS_COMMIT) ||
             (aLogType == SMR_LT_DSKTRANS_ABORT) )
        {
            return ID_TRUE;
        }

        return ID_FALSE;
    };

    // MRDB�� operation NTA �α� ���
    static IDE_RC writeNTALogRec(idvSQL*   aStatistics,
                                 void*      aTrans,
                                 smLSN*     aLSN,
                                 scSpaceID  aSpaceID = 0,
                                 smrOPType  aOPType = SMR_OP_NULL,
                                 vULong     aData1 = 0,
                                 vULong     aData2 = 0);

    static IDE_RC writeCMPSLogRec( idvSQL*       aStatistics,
                                   void*         aTrans,
                                   smrLogType    aType,
                                   smLSN*        aPrvUndoLSN,
                                   smrUpdateLog* aUpdateLog,
                                   SChar*        aBeforeImagePtr );

    // savepoint ���� �α� ���
    static IDE_RC writeSetSvpLog( idvSQL*   aStatistics,
                                  void*         aTrans,
                                  const SChar*  aSVPName );

    // savepoint ���� �α� ���
    static IDE_RC writeAbortSvpLog( idvSQL*   aStatistics,
                                    void*          aTrans,
                                    const SChar*   aSVPName );

    // DB ���� �߰��� ���õ� �α�
    static IDE_RC writeDbFileChangeLog( idvSQL*   aStatistics,
                                        void * aTrans,
                                        SInt   aDBFileNo,
                                        SInt   aCurDBFileCount,
                                        SInt   aAddedDBFileCount );

    /* ------------------------------------------------
     * DRDB�� �α׸� �α����Ͽ� ���
     * writeDiskLogRec �Լ����� ȣ��Ǵ� �Լ�
     * - �α��� �α��� �����͸� �޴°� ��� smuDynBuffer
     * �����͸� �޾Ƽ� ó��
     * - �α��� BeginLSN�� EndLSN�� ������
     * ----------------------------------------------*/
    static IDE_RC writeDiskLogRec( idvSQL           *aStatistics,
                                   void             *aTrans,
                                   smLSN            *aPrvLSN,
                                   smuDynArrayBase  *aMtxLogBuffer,
                                   UInt              aWriteOption,
                                   UInt              aLogAttr,
                                   UInt              aContType,
                                   UInt              aRefOffset,
                                   smOID             aTableOID,
                                   UInt              aRedoType,
                                   smLSN*            aBeginLSN,
                                   smLSN*            aEndLSN );

    static IDE_RC writeDiskNTALogRec( idvSQL*           aStatistics,
                                      void*             aTrans,
                                      smuDynArrayBase*  aMtxLogBuffer,
                                      UInt              aWriteOption,
                                      UInt              aOPType,
                                      smLSN*            aPPrevLSN,
                                      scSpaceID         aSpaceID,
                                      ULong           * aArrData,
                                      UInt              aDataCount,
                                      smLSN*            aBeginLSN,
                                      smLSN*            aEndLSN,
                                      smOID             aTableOID );

    static IDE_RC writeDiskRefNTALogRec( idvSQL*           aStatistics,
                                         void*             aTrans,
                                         smuDynArrayBase*  aMtxLogBuffer,
                                         UInt              aWriteOption,
                                         UInt              aOPType,
                                         UInt              aRefOffset, 
                                         smLSN*            aPPrevLSN,
                                         scSpaceID         aSpaceID,
                                         smLSN*            aBeginLSN,
                                         smLSN*            aEndLSN,
                                         smOID             aTableOID );

    static IDE_RC writeDiskDummyLogRec( idvSQL*           aStatistics,
                                        smuDynArrayBase*  aMtxLogBuffer,
                                        UInt              aWriteOption,
                                        UInt              aContType,
                                        UInt              aRedoType,
                                        smOID             aTableOID,
                                        smLSN*            aBeginLSN,
                                        smLSN*            aEndLSN );

    static IDE_RC writeDiskCMPSLogRec( idvSQL*           aStatistics,
                                       void*             aTrans,
                                       smuDynArrayBase*  aMtxLogBuffer,
                                       UInt              aWriteOption,
                                       smLSN*            aPrevLSN,
                                       smLSN*            aBeginLSN,
                                       smLSN*            aEndLSN,
                                       smOID             aTableOID );

    static IDE_RC writeCMPSLogRec4TBSUpt( idvSQL*           aStatistics,
                                          void*             aTrans,
                                          smLSN*            aPrevLSN,
                                          smrTBSUptLog*     aUpdateLog,
                                          SChar*            aBeforeImagePtr );

    /* BUG-9640 */
    static IDE_RC writeTBSUptLogRec( idvSQL*           aStatistics,
                                     void*             aTrans,
                                     smuDynArrayBase*  aLogBuffer,
                                     scSpaceID         aSpaceID,
                                     UInt              aFileID,
                                     sctUpdateType     aTBSUptType,
                                     UInt              aAImgSize,
                                     UInt              aBImgSize,
                                     smLSN*            aBeginLSN );

    /* prj-1149*/
    static IDE_RC writeDiskPILogRec( idvSQL*           aStatistics,
                                     UChar*            aBuffer,
                                     scGRID            aPageGRID );

    // PROJ-1362 Internal LOB
    static IDE_RC writeLobCursorOpenLogRec(idvSQL*         aStatistics,
                                            void*           aTrans,
                                            smrLobOpType    aLobOp,
                                            smOID           aTable,
                                            UInt            aLobColID,
                                            smLobLocator    aLobLocator,
                                            const void*     aPKInfo );

    static IDE_RC writeLobCursorCloseLogRec( idvSQL*        aStatistics,
                                             void*          aTrans,
                                             smLobLocator   aLobLocator,
                                             smOID          aTableOID );

    static IDE_RC writeLobPrepare4WriteLogRec(idvSQL*       aStatistics,
                                              void*         aTrans,
                                              smLobLocator  aLobLocator,
                                              UInt          aOffset,
                                              UInt          aOldSize,
                                              UInt          aNewSize,
                                              smOID         aTableOID );

    static IDE_RC writeLobFinish2WriteLogRec( idvSQL*       aStatistics,
                                              void*         aTrans,
                                              smLobLocator  aLobLocator,
                                              smOID         aTableOID );

    // PROJ-2047 Strengthening LOB
    static IDE_RC writeLobEraseLogRec( idvSQL       * aStatistics,
                                       void         * aTrans,
                                       smLobLocator   aLobLocator,
                                       ULong          aOffset,
                                       ULong          aSize,
                                       smOID          aTableOID );

    static IDE_RC writeLobTrimLogRec( idvSQL        * aStatistics,
                                      void*           aTrans,
                                      smLobLocator    aLobLocator,
                                      ULong           aOffset,
                                      smOID           aTableOID );

    // PROJ-1665
    // Direct-Path Insert�� ����� Page ��ü�� logging �ϴ� �Լ�
    static IDE_RC writeDPathPageLogRec( idvSQL * aStatistics,
                                        UChar  * aBuffer,
                                        scGRID   aPageGRID,
                                        smLSN  * aEndLSN );

    // PROJ-1867 write Page Image Log
    static IDE_RC writePageImgLogRec( idvSQL     * aStatistics,
                                      UChar      * aBuffer,
                                      scGRID       aPageGRID,
                                      sdrLogType   aLogType,
                                      smLSN      * aEndLSN );

    static IDE_RC writePageConsistentLogRec( idvSQL        * aStatistics,
                                             scSpaceID       aSpaceID,
                                             scPageID        aPageID,
                                             UChar           aIsPageConsistent );

    // �α����Ͽ� �α� ���
    static IDE_RC writeLog( idvSQL  * aStatistics,
                            void    * aTrans,
                            SChar   * aStrLog,
                            smLSN   * aPPrvLSN,
                            smLSN   * aBeginLSN,
                            smLSN   * aEndLSN,
                            smOID     aTableOID );

    /* ����� �α������� ��� ������ Ǯ� ���ڵ带 �о�´�. */
    static IDE_RC readLog( iduMemoryHandle    * aDecompBufferHandle,
                           smLSN              * aLSN,
                           idBool               aIsCloseLogFile,
                           smrLogFile        ** aLogFile,
                           smrLogHead         * aLogHeadPtr,
                           SChar             ** aLogPtr,
                           idBool             * aIsValid,
                           UInt               * aLogSizeAtDisk);
    
    /* ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ�Ѵ�. */
    static IDE_RC readLog4RP( smLSN              * aLSN,
                              idBool               aIsCloseLogFile,
                              smrLogFile        ** aLogFile,
                              smrLogHead         * aLogHeadPtr,
                              SChar             ** aLogPtr,
                              idBool             * aIsValid,
                              UInt               * aLogSizeAtDisk );

    // Ư�� LSN�� log record�� �ش� log record�� ���� �α� ������ �����Ѵ�.
    static IDE_RC readLogInternal( iduMemoryHandle  * aDecompBufferHandle,
                                   smLSN            * aLSN,
                                   idBool             aIsCloseLogFile,
                                   smrLogFile      ** aLogFile,
                                   smrLogHead       * aLogHead,
                                   SChar           ** aLogPtr,
                                   UInt             * aLogSizeAtDisk );

    static IDE_RC readLogInternal4RP( smLSN            * aLSN,
                                      idBool             aIsCloseLogFile,
                                      smrLogFile      ** aLogFile,
                                      smrLogHead       * aLogHead,
                                      SChar           ** aLogPtr,
                                      UInt             * aLogSizeAtDisk );

    // Ư�� �α������� ù��° �α��� Head�� �д´�.
    static IDE_RC readFirstLogHeadFromDisk( UInt   aFileNo,
                                            smrLogHead *aLogHead );
    
    /*
      ��� LogFile�� �����ؼ� aMinLSN���� ���� LSN�� ������ �α׸�
      ù��°�� ������ LogFile No�� ���ؼ� aNeedFirstFileNo�� �־��ش�.
    */
    static IDE_RC getFirstNeedLFN( smLSN          aMinLSN,
                                   const UInt     aFirstFileNo,
                                   const UInt     aEndFileNo,
                                   UInt         * aNeedFirstFileNo );

    // ������ LSN���� Sync�Ѵ�.
    static IDE_RC syncToLstLSN( smrSyncByWho   aWhoSyncLog );

    // �� Log File Group�� �α� ���ϵ��� ����Ǵ� ��θ� ����
    static inline const SChar * getLogPath()
    {
        return mLogPath;
    };

    // �� Log File Group�� �α׵��� ��ī�̺�� ��θ� ����
    static inline const SChar * getArchivePath()
    {
        return mArchivePath;
    };

    static inline smrLogFileMgr &getLogFileMgr()
    {
        return mLogFileMgr;
    };

    static inline smrLFThread &getLFThread()
    {
        return mLFThread;
    };

    static inline smrArchThread &getArchiveThread()
    {
        return mArchiveThread;
    };

    static inline UInt getCurOffset()
    {
        return mCurLogFile->mOffset;
    }
   
    /* �α������� ���� ���� ���ü� ��� ���� lock()/unlock() */
    static inline IDE_RC lock() 
    { 
        return mMutex.lock( NULL ); 
    }
    static inline IDE_RC unlock() 
    { 
        return mMutex.unlock(); 
    }

    /* LogMgr�� initialize �Ǿ����� Ȯ���Ѵ�. */
    static inline idBool isAvailable()
    {
        return mAvailable;
    }

public:
    /********************  Group Commit ���� ********************/
    // Active Transaction�� Update Transaction�� ���� �����Ѵ�.
    static inline UInt getUpdateTxCount()
    {
        return mUpdateTxCount ;
    };

    // Active Transaction�� Update Transaction�� ����
    // �ϳ� ������Ų��.
    // �ݵ�� mMutex�� ���� ����(lock()ȣ��)���� ȣ��Ǿ�� �Ѵ�.
    static  inline void incUpdateTxCount()
    {
        IDE_DASSERT( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY );

        idCore::acpAtomicInc32( &mUpdateTxCount );
        // Update Transaction ���� �ý����� �ִ� Transaction�� ���� ���� �� ����.
        IDE_DASSERT( mUpdateTxCount <= smuProperty::getTransTblSize() );
    };

    // Active Transaction�� Update Transaction�� ����
    // �ϳ� ���ҽ�Ų��.
    // �ݵ�� mMutex�� ���� ����(lock()ȣ��)���� ȣ��Ǿ�� �Ѵ�.
    static inline void decUpdateTxCount()
    {
        IDE_DASSERT( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY );

        IDE_DASSERT ( mUpdateTxCount > 0 );

        idCore::acpAtomicDec32( &mUpdateTxCount ); 
    };

public:
    /******************** �α� ���� �Ŵ��� ********************/
    // �α������� ������ switch��Ű�� archiving 
    static IDE_RC switchLogFileByForce();
    // interval�� ���� checkpoint ������ switch count �ʱ�ȭ 
    static IDE_RC clearLogSwitchCount();
    
     /***********************************************************************
     * Description : startupLogPrepareThread �� ������ �۾� 
                     �� createDB�� ȣ��, LSN is 0
     **********************************************************************/
    static inline IDE_RC startupLogPrepareThread()
    {
        smLSN  sLstLSN;
        SM_LSN_INIT( sLstLSN );
 
        IDE_TEST( mLogFileMgr.startAndWait( &sLstLSN,
                                            0,        /* aLstFileNo */
                                            ID_FALSE, /* aIsRecovery */  
                                            &mCurLogFile )
                  != IDE_SUCCESS );

        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    };

     /***********************************************************************
     * Description : Prepare �����带 Start��Ŵ.
     *               aLstLSN�� �ش��ϴ� ������ 
                     mCurLogFile�� �����ϰ� LstLSN���� ������. 
                      
     * aLstLSN    - [IN] Lst LSN
     * aLstFileNo - [IN] ���������� ������ �α����� ��ȣ
     * aIsRecovery- [IN] recovery ���� 
     **********************************************************************/
    static inline IDE_RC startupLogPrepareThread( smLSN   * aLstLSN, 
                                                  UInt      aLstFileNo, 
                                                  idBool    aIsRecovery )
    {
        IDE_TEST( mLogFileMgr.startAndWait( aLstLSN,
                                            aLstFileNo,
                                            aIsRecovery,
                                            &mCurLogFile )
                  != IDE_SUCCESS );

        IDE_DASSERT ( mCurLogFile->mOffset == aLstLSN->mOffset );
        IDE_DASSERT ( mCurLogFile->mFileNo == aLstLSN->mFileNo );

        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    };

    /***********************************************************************
     * Description : aFileNo�� ����Ű�� LogFile�� Open�Ѵ�.
     *               aLogFilePtr�� Open�� Logfile Pointer�� Setting���ش�.
     *
     * aFileNo     - [IN] open�� LogFile No
     * aIsWrite    - [IN] open�� logfile�� ���� write�� �Ѵٸ� ID_TRUE,
     *                    �ƴϸ� ID_FALSE
     * aLogFilePtr - [OUT] open�� logfile�� ����Ų��.
     **********************************************************************/
    static inline IDE_RC openLogFile( UInt           aFileNo,
                                      idBool         aIsWrite,
                                      smrLogFile   **aLogFilePtr )
    {
        IDE_TEST( mLogFileMgr.open( aFileNo,
                                    aIsWrite,
                                    aLogFilePtr )
                  != IDE_SUCCESS);
        
        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    };

    /* aLogFile�� Close�Ѵ�. */
    static inline IDE_RC closeLogFile(smrLogFile *aLogFile)
    {
        IDE_DASSERT( aLogFile != NULL );

        IDE_TEST( mLogFileMgr.close( aLogFile ) != IDE_SUCCESS );

        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    }

    // aLSN�� ����Ű�� �α������� ù��° Log �� Head�� �д´�
    static IDE_RC readFirstLogHead( smLSN      *aLSN,
                                    smrLogHead *aLogHead);

public:
    /******************** Archive ������ ********************/
    // Archive �����带 startup��Ų��.
    static inline IDE_RC startupLogArchiveThread()
    {
        IDE_TEST( mArchiveThread.startThread() != IDE_SUCCESS );

        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    }

    // Archive �����带 shutdown��Ų��.
    static inline IDE_RC shutdownLogArchiveThread()
    {
        IDE_TEST( mArchiveThread.shutdown() != IDE_SUCCESS );
        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    }

public:
    /******************** Log Flush Thread ********************/
    // Log Flush Thread�� aLSNToSync�� ������ LSN���� Sync����.
    static IDE_RC syncLFThread( smrSyncByWho    aWhoSync,
                                smLSN         * aLSNToSync );

    // sync�� log file�� ù ��° �α� �� ���� ���� ���� �����´�.
    static IDE_RC getSyncedMinFirstLogLSN( smLSN *aLSN );

    /* aLSNToSync���� �αװ� sync�Ǿ����� �����Ѵ�.
     *  
     * ���� �����ڿ� ���� ȣ��Ǹ�, �⺻���� ������
     * noWaitForLogSync �� ����.
     */
    static inline IDE_RC sync4BufferFlush( smLSN        * aLSNToSync,
                                           UInt         * aSyncedLFCnt )
    {

        IDE_TEST( mLFThread.syncOrWait4SyncLogToLSN( SMR_LOG_SYNC_BY_BFT,
                                                     aLSNToSync->mFileNo,
                                                     aLSNToSync->mOffset, 
                                                     aSyncedLFCnt )
                  != IDE_SUCCESS);

        return IDE_SUCCESS;
        IDE_EXCEPTION_END;
        return IDE_FAILURE;
    }

    static void writeDebugInfo();

public:
    /********************  FAST UNLOCK LOG ALLOC MUTEX ********************/
    /* BUG-35392 
     * Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� LSN */
    static inline void getUncompletedLstWriteLSN( smLSN   * aLSN )
    {
        IDE_DASSERT( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE );
        smrLSN4Union  sLstWriteLSN;

        sLstWriteLSN.mSync = mUncompletedLSN.mLstWriteLSN.mSync;

        SM_GET_LSN( *aLSN,
                    sLstWriteLSN.mLSN );
    };

#if 0
    /* BUG-35392 */
    static inline void getLstWriteLogLSN( smLSN   * aLstWriteLSN )
    {
        if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
        {
            /* Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� LSN */
            (void)getUncompletedLstWriteLSN( aLstWriteLSN );
        }
        else
        {
            /*  ���������� ����� �α� ���ڵ��� LSN */
            getLstWriteLSN( aLstWriteLSN );
        }
    };
#endif

    /* BUG-35392 
     * Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� Last offset �޾ƿ´�. */
    static inline void getUncompletedLstLSN( smLSN   * aUncompletedLSN )
    {
        smrLSN4Union sLstLSN;

        IDE_DASSERT( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE );

        sLstLSN.mSync = mUncompletedLSN.mLstLSN.mSync;

        SM_GET_LSN( *aUncompletedLSN,
                    sLstLSN.mLSN );
    };

    /* BUG-35392
     * logfile�� ����� Log�� ������ Offset�� �޾ƿ´�.
     */
    static inline void getLstLogOffset( smLSN  * aValidLSN )
    {
        if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
        {
            /* Dummy Log�� �������� �ʴ� ������ �α� ���ڵ��� Last offset */
            getUncompletedLstLSN( aValidLSN );
        }
        else
        {
            /* ���������� ����� �α� ���ڵ��� Last Offset�� ��ȯ */
            getLstLSN( aValidLSN );
        }
    };

   /* ������ LSN ���� sync�� �Ϸ�Ǳ⸦ ����Ѵ�. */
    static void waitLogSyncToLSN( smLSN  * aLSNToSync,
                                  UInt     aSyncWaitMin,
                                  UInt     aSyncWaitMax );

    static void rebuildMinUCSN();

    // Dummy �α׸� ����Ѵ�. 
    static IDE_RC writeDummyLog();
 
    /***********************************************************************
     * Description : ���������� ����� �α� ���ڵ��� "Last Offset"�� �����Ѵ�. 
     *               �д� ���� ���ÿ� �����ϳ�, ���ÿ� ���°��� �ȵȴ�.
     *               allocMutex�� ��ȣ�ϰ� ����Ѵ�.
     *               allocMutex�� ��ȣ�Ǳ⿡ �׻� �� ū ���� �;� �Ѵ�.
     *
     *   logfile0 �� 20���� �αװ� ��ϵǾ��ٸ� 
     *   +---------------------------------------------
     *   + [FileNo, offset] | .....   [0,10] | [0,20] |        
     *   +----------------------------------------------
     *                                      (A)      (B) 
     *   (A) : mLstWriteLSN    (B) : mLstLSN
     *   �� �Լ���  (B) �� ������ 
     *
     *   aFileNo  - [IN] Log File Number
     *   aOffset  - [IN] Log�� ���������� ����� �α׷��ڵ��� "Last Offset"
     ***********************************************************************/
    static inline void setLstLSN( UInt   aFileNo,
                                  UInt   aOffset )
    {
        /* BUG-32137 [sm-disk-recovery] The setDirty operation in DRDB causes 
         * contention of LOG_ALLOCATION_MUTEX.
         * ������ smrUncompletedLogInfo�� �̿��� �����Ѵ�.
         * 64Bit�� ��� Atomic�ϰ� ����Ǳ� ������ �������,
         * 32Bit �� �������� �ʴ´�!.*/
        smrLSN4Union sLstLSN;

        ID_SERIAL_BEGIN( SM_SET_LSN( sLstLSN.mLSN,
                                     aFileNo,
                                     aOffset ) );        

        ID_SERIAL_END( mLstLSN.mSync = sLstLSN.mSync );
        
    }

    /***********************************************************************
     * Description : ���������� ����� �α� ���ڵ��� "Last Offset"�� ��ȯ�Ѵ�.  
     *               flush ���� ������ ������ LSN�� ���ؾ� �Ҷ� ��� 
     *   aLstLSN - [OUT] ������ LSN�� "Last Offset"
     ***********************************************************************/
    static inline void getLstLSN( smLSN  * aLSN )
    {
        smrLSN4Union sLstLSN ;

        ID_SERIAL_BEGIN( sLstLSN.mSync = mLstLSN.mSync );

        ID_SERIAL_END( SM_GET_LSN( *aLSN,
                                    sLstLSN.mLSN ) );
    }

    /***********************************************************************
     * Description : ���������� ����� �α� ���ڵ��� LSN�� �����Ѵ�
     *               DR, RP ��� ������ ����� �α� ���ڵ带 ã�� ���� ��� 
     *
     *   logfile0 �� 20���� �αװ� ��ϵǾ��ٸ� 
     *   +---------------------------------------------
     *   + [FileNo, offset] | .....   [0,10] | [0,20] |        
     *   +----------------------------------------------
     *                                      (A)      (B) 
     *   (A) : mLstWriteLSN    (B) : mLstLSN
     *   �� �Լ��� (A) �� ������
     *
     *   aLstLSN  - [IN]  Log�� ���������� ����� �α׷��ڵ��� LSN
     ***********************************************************************/
    static inline void setLstWriteLSN( smLSN  aLSN )
    {
         /* ������ smrUncompletedLogInfo�� �̿��� �����Ѵ�.
         * 64Bit�� ��� Atomic�ϰ� ����Ǳ� ������ �������,
         * 32Bit �� �������� �ʴ´�!.*/
        smrLSN4Union sLstWriteLSN;

        ID_SERIAL_BEGIN( SM_SET_LSN( sLstWriteLSN.mLSN,
                                     aLSN.mFileNo,
                                     aLSN.mOffset ) );        

        ID_SERIAL_END( mLstWriteLSN.mSync = sLstWriteLSN.mSync );
    }

    /***********************************************************************
     * Description : ���������� ����� �α� ���ڵ��� LSN�� �����´�  
     *
     *   aLstLSN - [OUT] ���������� ����� �α� ���ڵ��� LSN
     ***********************************************************************/
    static inline void getLstWriteLSN( smLSN *aLSN )
    {
        smrLSN4Union sLstWriteLSN;

        ID_SERIAL_BEGIN( sLstWriteLSN.mSync = mLstWriteLSN.mSync );

        ID_SERIAL_END( SM_GET_LSN( *aLSN,
                                   sLstWriteLSN.mLSN ) );
    }

public: // for request function

    // Disk�α��� Log ���� ���θ� �����Ѵ�
    static IDE_RC decideLogComp( UInt aDiskLogWriteOption,
                                 smrLogHead * aLogHead );

    /* SMR_OP_NULL Ÿ���� NTA �α� ��� */
    static IDE_RC writeNullNTALogRec( idvSQL* aStatistics,
                                      void*   aTrans,
                                      smLSN*  aLSN );

    /* SMR_OP_SMM_PERS_LIST_ALLOC Ÿ���� NTA �α� ��� */
    static IDE_RC writeAllocPersListNTALogRec( idvSQL*    aStatistics,
                                               void     * aTrans,
                                               smLSN    * aLSN,
                                               scSpaceID  aSpaceID,
                                               scPageID   aFirstPID,
                                               scPageID   aLastPID );

    static IDE_RC writeCreateTbsNTALogRec( idvSQL*    aStatistics,
                                           void     * aTrans,
                                           smLSN    * aLSN,
                                           scSpaceID  aSpaceID);

    // Table/Index/Sequence��
    // Create/Alter/Drop DDL�� ���� Query String�� �α��Ѵ�.
    static IDE_RC writeDDLStmtTextLog( idvSQL         * aStatistics,
                                       void           * aTrans,
                                       smrDDLStmtMeta * aDDLStmtMeta,
                                       SChar          * aStmtText,
                                       SInt             aStmtTextLen );

private:
    static IDE_RC writeLobOpLogRec( idvSQL*           aStatistics,
                                    void*             aTrans,
                                    smLobLocator      aLobLocator,
                                    smrLobOpType      aLobOp,
                                    smOID             aTableOID );
    
    static UInt   getMaxLogOffset() { return mMaxLogOffset; };

    // File Begin Log�� �����Ѵ�.
    static void initializeFileBeginLog
                           ( smrFileBeginLog * aFileBeginLog );
    // File End Log�� �����Ѵ�.
    static void initializeFileEndLog
                           ( smrFileEndLog * aFileEndLog );

    // SMR_LT_FILE_BEGIN �α׸� ����Ѵ�.
    static void writeFileBeginLog();
    
    // SMR_LT_FILE_END �α׸� ����Ѵ�.
    static void writeFileEndLog();

    // ���� ���ҽ��� �����´�
    static IDE_RC allocCompRes( void        * aTrans,
                                smrCompRes ** aCompRes );

    // ���� ���ҽ��� �ݳ��Ѵ�.
    static IDE_RC freeCompRes( void       * aTrans,
                               smrCompRes * aCompRes );

    // �������� ���� �����α׸� Replication Log Buffer�� ����
    static void copyLogToReplBuffer( idvSQL * aStatistics,
                                     SChar  * aRawLog,
                                     UInt     aRawLogSize,
                                     smLSN    aLSN );

    // Log�� ���� Mutex�� ���� ���·� �α� ��� 
    static IDE_RC lockAndWriteLog( idvSQL   * aStatistics,
                                   void     * aTrans,
                                   SChar    * aRawOrCompLog,
                                   UInt       aRawOrCompLogSize,
                                   SChar    * aRawLog4Repl,
                                   UInt       aRawLogSize4Repl,
                                   smLSN    * aBeginLSN,
                                   smLSN    * aEndLSN,
                                   idBool   * aIsLogFileSwitched );

    // Log�� ���� Mutex�� ���� ���·� �α� ��� 
    static IDE_RC lockAndWriteLog4FastUnlock( idvSQL   * aStatistics,
                                              void     * aTrans,
                                              SChar    * aRawOrCompLog,
                                              UInt       aRawOrCompLogSize,
                                              SChar    * aRawLog4Repl,
                                              UInt       aRawLogSize4Repl,
                                              smLSN    * aBeginLSN,
                                              smLSN    * aEndLSN,
                                              idBool   * aIsLogFileSwitched );


    // �α� ������� ������ �۾� ó��
    static void onBeforeWriteLog( void     * aTrans,
                                  SChar    * aStrLog,
                                  smLSN    * aPPrvLSN );
    

    
    // �α� ����Ŀ� ������ �۾��� ó��
    static void onAfterWriteLog( idvSQL     * aStatistics,
                                 void       * aTrans,
                                 smrLogHead * aLogHead,
                                 smLSN        aLSN,
                                 UInt         aWrittenLogSize );

    // �α��� ������ ������ ��� ���� �ǽ�
    static IDE_RC tryLogCompression( smrCompRes         * aCompRes,
                                     SChar              * aRawLog,
                                     UInt                 aRawLogSize,
                                     SChar             ** aLogToWrite,
                                     UInt               * aLogSizeToWrite,
                                     smOID                aTableOID );

    
 
    /* �α� header�� previous undo LSN�� �����Ѵ�.
     * writeLog ���� ����Ѵ�.  
     */
    static void setLogHdrPrevLSN( void*       aTrans, 
                                  smrLogHead* aLogHead,
                                  smLSN*      aPPrvLSN );
    
    /* �α� ����� ���� Ȯ��
     * writeLog ���� ����Ѵ�.
     *
     * aLogSize           - [IN]  ���� ����Ϸ��� �α� ���ڵ��� ũ��
     * aIsLogFileSwitched - [OUT] aLogSize��ŭ ����Ҹ��� ������ Ȯ���ϴ� �߿�
     *                            �α����� Switch�� �߻��ߴ����� ����
     */
    static IDE_RC reserveLogSpace( UInt     aLogSize,
                                   idBool * aIsLogFileSwitched );


    // ���ۿ� ��ϵ� �α��� smrLogTail�� �ǵ��Ͽ�,
    // �α��� validation �˻縦 �Ѵ�.
    static void validateLogRec( SChar * aStrLog );

    /* Transaction�� Fst, Lst Log LSN�� �����Ѵ�. */
    static void updateTransLSNInfo( idvSQL * aStatistics,
                                    void   * aTrans,
                                    smLSN  * aLSN,
                                    UInt     aLogSize );

//    // Check LogDir Exist
    static IDE_RC checkLogDirExist();

    /******************************************************************************
     * ����/����� �α��� Head�� SN�� �����Ѵ�.
     *
     * [IN] aRawOrCompLog - ����/����� �α�
     * [IN] aLogSN - �α׿� ����� SN
     *****************************************************************************/

    // ����/����� �α��� Head�� SN�� �����Ѵ�.
    static inline void setLogLSN( SChar  * aRawOrCompLog,
                                  smLSN    aLogLSN )
    {
        if ( smrLogComp::isCompressedLog( aRawOrCompLog ) == ID_TRUE )
        {
            smrLogComp::setLogLSN( aRawOrCompLog, aLogLSN );
        }
        else
        {
            /* LSN���� �α� ����� �����Ѵ�. */
            smrLogHeadI::setLSN( (smrLogHead*)aRawOrCompLog, aLogLSN );
        }
    }

    /******************************************************************************
     * ����/����� �α��� Head�� MAGIC���� �����Ѵ�.
     *
     * [IN] aRawOrCompLog - ����/����� �α�
     * [IN] aLSN - �α��� LSN
     *****************************************************************************/
    static inline void setLogMagic( SChar * aRawOrCompLog,
                                    smLSN * aLSN )
    {
        smMagic sLogMagicValue = smrLogFile::makeMagicNumber( aLSN->mFileNo,
                                                              aLSN->mOffset );

        if ( smrLogComp::isCompressedLog( aRawOrCompLog ) == ID_TRUE )
        {
            smrLogComp::setLogMagic( aRawOrCompLog,
                                     sLogMagicValue );
        }
        else
        {
            /* ���߿� �α׸� ������ Log�� Validity check�� ���� �αװ� ��ϵǴ�
             * ���Ϲ�ȣ�� �α׷��ڵ��� ���ϳ� Offset�� �̿��Ͽ�
             * Magic Number�� �����صд�. */
            smrLogHeadI::setMagic( (smrLogHead *)aRawOrCompLog,
                                   sLogMagicValue );
        }
    }


private:
    /********************  Group Commit ���� ********************/
    // Transaction�� �α׸� ����� �� 
    // Update Transaction�� ���� �������Ѿ� �ϴ��� üũ�Ѵ�.
    static inline void checkIncreaseUpdateTxCount( void       * aTrans );

    // Transaction�� �α׸� ����� �� 
    // Update Transaction�� ���� ���ҽ��Ѿ� �ϴ��� üũ�Ѵ�.
    static inline void checkDecreaseUpdateTxCount( void       * aTrans,
                                                   smrLogHead * aLogHead );
    
private:
    /********************  FAST UNLOCK LOG ALLOC MUTEX ********************/

    inline static void incCount( UInt aSlotID )
    {
        UInt sIdx = aSlotID / SMR_CHECK_LSN_UPDATE_GROUP;

        idCore::acpAtomicInc32( &mFstChkLSNUpdateCnt[sIdx] );
    }

    inline static void decCount( UInt aSlotID )
    {
        UInt sIdx = aSlotID / SMR_CHECK_LSN_UPDATE_GROUP;
        
        idCore::acpAtomicDec32( &mFstChkLSNUpdateCnt[sIdx] );
    }

    /* BUG-35392 
     * ���̷α׸� �������� �ʴ� LstlSN, LstWriteLSN�� ���ϱ� ���� ���
     * �ش� ���̷αװ� �ۼ��Ǳ� �� LstlSN, LstWriteLSN �� �����Ѵ�. */
    static inline void setFstCheckLSN( UInt aSlotID )
    {
        smrUncompletedLogInfo     * sFstChkLSN;

        IDE_DASSERT( aSlotID < mFstChkLSNArrSize );

        sFstChkLSN = &mFstChkLSNArr[ aSlotID ];

        IDE_DASSERT( SM_IS_SYNC_LSN_MAX( sFstChkLSN->mLstLSN.mSync ) );
        IDE_DASSERT( SM_IS_SYNC_LSN_MAX( sFstChkLSN->mLstWriteLSN.mSync ) );

        sFstChkLSN->mLstLSN.mSync      = mLstLSN.mSync;
        sFstChkLSN->mLstWriteLSN.mSync = mLstWriteLSN.mSync;
        
        incCount( aSlotID );
    }

    /* BUG-35392 */
    static inline void unsetFstCheckLSN( UInt aSlotID )
    {
        static smrUncompletedLogInfo       sMaxSyncLSN;
        smrUncompletedLogInfo            * sFstChkLSN;

        IDE_DASSERT( aSlotID < mFstChkLSNArrSize );
        
        decCount( aSlotID );

        SM_SYNC_LSN_MAX( sMaxSyncLSN.mLstLSN.mSync );
        SM_SYNC_LSN_MAX( sMaxSyncLSN.mLstWriteLSN.mSync );

        sFstChkLSN = &mFstChkLSNArr[ aSlotID ];

        IDE_DASSERT( !(SM_IS_SYNC_LSN_MAX( sFstChkLSN->mLstLSN.mSync )) );
        IDE_DASSERT( !(SM_IS_SYNC_LSN_MAX( sFstChkLSN->mLstWriteLSN.mSync )) );

        sFstChkLSN->mLstLSN.mSync      = sMaxSyncLSN.mLstLSN.mSync;
        sFstChkLSN->mLstWriteLSN.mSync = sMaxSyncLSN.mLstWriteLSN.mSync;
    }

private:
/******************** �α� ���� �Ŵ��� ********************/
    // �α������� Switch�� ������ �Ҹ����.
    // �α����� Switch Count�� 1 ������Ű��
    // üũ����Ʈ�� �����ؾ� �� ���� ���θ� �����Ѵ�.
    static IDE_RC onLogFileSwitched();

    static inline IDE_RC lockLogSwitchCount()
    { 
        return mLogSwitchCountMutex.lock( NULL ); 
    }
    static inline IDE_RC unlockLogSwitchCount()
    { 
        return mLogSwitchCountMutex.unlock(); 
    }

private:

    //For Logging Mode
    static iduMutex           mMtxLoggingMode;
    static UInt               mMaxLogOffset;

    /* Transaction�� NULL�� ������ ��쿡 ���Ǵ�
       �α� ���� ���ҽ� Ǯ
       
       Pool ���ٽ� Mutex��� ������ ª�� ������
       Contention�� ������ �� �ִ� �����̴�.
    */
    static smrCompResPool       mCompResPool;
    
  
    // �α������� ���� Write�� �ϳ��� �����常 ������ �����ϴ�.
    // �α������� ���� ���� ���ü� ��� ���� Mutex
    static iduMutex             mMutex;

    // �� �α����� �׷���� open�� �α����ϵ��� �����ϴ�
    // �α����� ������
    // �α������� ����ϱ� ���� �̸� �غ��� �δ� prepare �������̱⵵ �ϴ�.
    static smrLogFileMgr        mLogFileMgr;

    // �� �α����� �׷쿡 ���� �α����ϵ��� Flush�� ����ϴ�
    // �α����� Flush ������
    static smrLFThread          mLFThread;

    // �� �α����� �׷쿡 ���� �α����ϵ��� ��ī�̺� ��Ű��
    // �α����� ��ī�̺� ������
    static smrArchThread        mArchiveThread;

    // �α� ���ϵ��� ����� �α� ���丮
    static const SChar        * mLogPath ;

    // ��ī�̺� �αװ� ����� ���丮
    // Log File Group�� �ϳ��� unique�� ��ī�̺� ���丮�� �ʿ��ϴ�.
    static const SChar        * mArchivePath ;

    // �� �α����Ͽ� �α� ���ڵ���� ����� ������.
    static smrLogFile         * mCurLogFile;

     /*   logfile0 �� 20���� �αװ� ��ϵǾ��ٸ� 
     *   +---------------------------------------------
     *   + [FileNo, offset] | .....   [0,10] | [0,20] |        
     *   +----------------------------------------------
     *                                      (A)      (B) 
     *   (A) : mLstWriteLSN    (B) : mLstLSN
     */

    // ������ LSN
    // ���̸� �����ؼ� mCurLogFile���� �α� ���ڵ尡 ��ϵ� ������ offset
    // mCurLogFile���� ���� �α� ���ڵ带 ����� ��ġ.
    static smrLSN4Union             mLstLSN;

    // ���������� Write�� LSN��
    // ���̸� �����ؼ� mCurLogFile�� �α� ���ڵ尡 ��ϵ� LSN
    static smrLSN4Union             mLstWriteLSN;  
 
    // ���̸� �������� ���� LstLSN, LstWriteLSN
    static smrUncompletedLogInfo    mUncompletedLSN;

    // ��� �α����Ͽ� �� ó������ ��ϵǴ� File Begin Log�̴�.
    // �� �α׷��ڵ��� �뵵�� ���ؼ��� smrDef.h�� �����Ѵ�.
    static smrFileBeginLog          mFileBeginLog;
    
    // �ϳ��� �α������� �� ���� �� �α������� �� �������� ����ϴ�
    // File End Log�̴�.
    static smrFileEndLog            mFileEndLog;
    
    /********************  FAST UNLOCK LOG ALLOC MUTEX ********************/
    static iduMutex                 mMutex4NullTrans;

    static smrUncompletedLogInfo  * mFstChkLSNArr;

    static UInt                     mFstChkLSNArrSize;
    
    static UInt                   * mFstChkLSNUpdateCnt;

private:
/********************  Group Commit ���� ********************/
    // Active Transaction�� Update Transaction�� ��
    // �� ���� LFG_GROUP_COMMIT_UPDATE_TX_COUNT ������Ƽ���� Ŭ ������
    // �׷�Ŀ���� �����Ѵ�.
    //
    // �� ������ ���ü� ����� �α����� ���� ���ؽ��� mMutex ���� ó���Ѵ�.
    static UInt                 mUpdateTxCount;

/******************** �α� ���� �Ŵ��� ********************/
    // �ϳ��� �α����� �׷� ���� �α������� Switch�� ������ 1�� ������Ų��.
    // �� ���� smuProperty::getChkptIntervalInLog() ��ŭ �����ϸ�
    // üũ����Ʈ�� �����ϰ� ���� 0���� �����Ѵ�.
    static UInt                 mLogSwitchCount;

    // mLogSwitchCount ������ ���� ���ü� ��� ���� Mutex
    static iduMutex             mLogSwitchCountMutex;

    // DebugInfo ��
    static idBool               mAvailable;  

    static smrUCSNChkThread     mUCSNChkThread; /* BUG-35392 */
};

/************************************************************************
  PROJ-1527 Log Optimization

  1~8����Ʈ�� ������ ����� memcpy���� byte assign���� �����ϴ� ����
  ������ �� ����.
  ( �������� �������� assign instruction���� ���� ����Ǳ� ���� )

  �α� ���ۿ� �����͸� ���� �� �� ������ ���� ��ũ�θ� ����ϵ��� �Ѵ�.
  ( inline�Լ��� ó���ϸ� �α� ������ �ּҸ� ������Ű�� �κ� ó���� ����
    SChar ** �� ��� �ϴµ�, �̷��� �� ��� memcpy���� �� ��������.
    �̷��� ������ inline�Լ��� ���� �ʰ� ��ũ�θ� ����Ͽ���. )

 ************************************************************************/
#define SMR_LOG_APPEND_1( aDest, aSrc )    \
{                                          \
    IDE_DASSERT( ID_SIZEOF( aSrc ) == 1 ); \
    (aDest)[0] = aSrc;                     \
    (aDest)   += 1;                        \
}

#define SMR_LOG_APPEND_2( aDest, aSrc )   \
{                                         \
    IDE_DASSERT( ID_SIZEOF( aSrc ) == 2 );\
    (aDest)[0] = ((SChar*)(&aSrc))[0]; \
    (aDest)[1] = ((SChar*)(&aSrc))[1]; \
    (aDest)   += 2;                       \
}

#define SMR_LOG_APPEND_4( aDest, aSrc )   \
{                                         \
    IDE_DASSERT( ID_SIZEOF( aSrc ) == 4 );\
    (aDest)[0] = ((SChar*)(&aSrc))[0]; \
    (aDest)[1] = ((SChar*)(&aSrc))[1]; \
    (aDest)[2] = ((SChar*)(&aSrc))[2]; \
    (aDest)[3] = ((SChar*)(&aSrc))[3]; \
    (aDest)   += 4;                       \
}

#define SMR_LOG_APPEND_8( aDest, aSrc )   \
{                                         \
    IDE_DASSERT( ID_SIZEOF( aSrc ) == 8 );\
    (aDest)[0] = ((SChar*)(&aSrc))[0]; \
    (aDest)[1] = ((SChar*)(&aSrc))[1]; \
    (aDest)[2] = ((SChar*)(&aSrc))[2]; \
    (aDest)[3] = ((SChar*)(&aSrc))[3]; \
    (aDest)[4] = ((SChar*)(&aSrc))[4]; \
    (aDest)[5] = ((SChar*)(&aSrc))[5]; \
    (aDest)[6] = ((SChar*)(&aSrc))[6]; \
    (aDest)[7] = ((SChar*)(&aSrc))[7]; \
    (aDest)   += 8;                       \
}


#if defined(COMPILE_64BIT)
#   define SMR_LOG_APPEND_vULONG SMR_LOG_APPEND_8
#else
#   define SMR_LOG_APPEND_vULONG SMR_LOG_APPEND_4
#endif


#endif /* _O_SMR_LOG_MGR_H_ */

