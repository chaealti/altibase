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
 * $Id: smrLFThread.cpp 82426 2018-03-09 05:12:27Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <smr.h>
#include <smrReq.h>
#include <smxTransMgr.h> /* BUG-35392 */

smrLFThread::smrLFThread() : idtBaseThread()
{
}

smrLFThread::~smrLFThread()
{
}

/* �ν��Ͻ��� �ʱ�ȭ�ϰ� �α� Flush �����带 ������
 *
 * �� �α� Flush �����尡 Flush�ϴ� �α����ϵ���
 * �����ϴ� �α����� ������.
 */
IDE_RC smrLFThread::initialize( smrLogFileMgr   * aLogFileMgr,
                                smrArchThread   * aArchThread)
{
    IDE_DASSERT( aLogFileMgr != NULL );

    mLogFileMgr = aLogFileMgr;
    mArchThread = aArchThread;
    
    mFinish     = ID_FALSE;
    
    // mSyncLogFileList �� head,tail := NULL
    mSyncLogFileList.mSyncPrvLogFile = &mSyncLogFileList;
    mSyncLogFileList.mSyncNxtLogFile = &mSyncLogFileList;

    IDE_TEST(mMtxList.initialize((SChar*)"LOG_FILE_SYNC_LIST_MUTEX",
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);
    
    IDE_TEST(mMtxThread.initialize((SChar*)"LOG_FLUSH_THREAD_MUTEX",
                                   IDU_MUTEX_KIND_POSIX,
                                   IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);

    /* fix BUG-17602 Ư�� LSN�� sync�Ǳ⸦ ����ϴ� ��� ���� sync�� �Ϸ�ɶ�
     * ���� sync ���θ� Ȯ���� �� ����
     * 64bit������ SyncWait Mutex�� ����.
     * 32bit������ SyncedLSN ���� �� �ǵ��� SyncWait Mutex�� ���� */
    IDE_TEST(mMtxSyncLSN.initialize((SChar*)"SYNC_LSN_MUTEX",
                                    IDU_MUTEX_KIND_POSIX,
                                    IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);

    mLstLSN.mLSN.mFileNo = 0;
    mLstLSN.mLSN.mOffset = 0;

    // ���������� Sync�� �ð��� ���� �ð����� �ʱ�ȭ
    IDV_TIME_GET(&mLastSyncTime);

    mThreadCntWaitForSync = 0;
    
    // V$LFG �� ������ Group Commit ���ġ �ʱ�ȭ
    mGCWaitCount = 0;
    mGCAlreadySyncCount = 0;
    mGCRealSyncCount = 0;
    
    IDE_TEST_RAISE(mCV.initialize((SChar *)"LOG_FLUSH_THREAD_COND")
                   != IDE_SUCCESS, err_cond_var_init);

    IDE_TEST_RAISE(mSyncWaitCV.initialize((SChar *)"SYNC_LSN_COND")
                   != IDE_SUCCESS, err_cond_var_init);

    IDE_TEST(start() != IDE_SUCCESS);
    IDE_TEST(waitToStart(0) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLFThread::destroy()
{
    IDE_TEST(mMtxList.destroy() != IDE_SUCCESS);
    IDE_TEST(mMtxThread.destroy() != IDE_SUCCESS);

    IDE_TEST(mMtxSyncLSN.destroy() != IDE_SUCCESS);

    IDE_TEST_RAISE(mSyncWaitCV.destroy() != IDE_SUCCESS, err_cond_var_destroy);
 
    IDE_TEST_RAISE(mCV.destroy() != IDE_SUCCESS, err_cond_var_destroy);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Sync��� �α����� ����Ʈ�� �α������� �߰�
*/

IDE_RC smrLFThread::addSyncLogFile(smrLogFile*   aLogFile)
{
    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    // �߰��Ϸ��� �α������� ���� := ��ũ�� ����Ʈ�� tail
    aLogFile->mSyncPrvLogFile = mSyncLogFileList.mSyncPrvLogFile;
    // �߰��Ϸ��� �α������� ���� := NULL
    aLogFile->mSyncNxtLogFile = &mSyncLogFileList;

    // ��ũ�� ����Ʈ�� tail.next := �߰��Ϸ��� �α�����
    mSyncLogFileList.mSyncPrvLogFile->mSyncNxtLogFile = aLogFile;
    // ��ũ�� ����Ʈ�� tail := �߰� �Ϸ��� �α�����
    mSyncLogFileList.mSyncPrvLogFile = aLogFile;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Sync��� �α����� ����Ʈ���� �α������� ����
 */
IDE_RC smrLFThread::removeSyncLogFile(smrLogFile*  aLogFile)
{
    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    /*     LF2���� ������ ��ũ�� ����Ʈ
     *
     *         (1) ->       (2) ->
     *     LF1         LF2          LF3
     *         <- (3)       <- (4)
     *
     *---------------------------------------
     *     LF2 ���� ���� ��ũ�� ����Ʈ
     *
     *         (1) ->                       
     *     LF1         LF3             
     *         <- (4)                  
     *
     *---------------------------------------
     *     ���ŵǾ� ������ ���� LF2
     *
     *                        (2)->
     *       NULL         LF2         NULL
     *             <- (3)
     */
    
    // (1) := (2)
    aLogFile->mSyncPrvLogFile->mSyncNxtLogFile = aLogFile->mSyncNxtLogFile;
    // (4) := (3)
    aLogFile->mSyncNxtLogFile->mSyncPrvLogFile = aLogFile->mSyncPrvLogFile;
    // (3) := NULL
    aLogFile->mSyncPrvLogFile = NULL;
    // (2) := NULL
    aLogFile->mSyncNxtLogFile = NULL;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLFThread::getSyncedLSN( smLSN *aLSN )
{
    smrLSN4Union   sTmpLSN;

#ifndef COMPILE_64BIT     
    // BUGBUG lock/unlock���̿� ���� �߻����� �����Ƿ�,
    // sState���� �ڵ� �����ص� ������.
    SInt    sState = 0;


    IDE_TEST( lockSyncLSNMtx() != IDE_SUCCESS );
    sState = 1;
#endif
    // 64��Ʈ�� ��� atomic �ϰ� mLSN�� �ѹ� �о�� ��
    // aLSN�� ������� �����Ѵ�.
    ID_SERIAL_BEGIN( sTmpLSN.mSync = mLstLSN.mSync );

    ID_SERIAL_END( SM_SET_LSN( *aLSN,
                               sTmpLSN.mLSN.mFileNo,
                               sTmpLSN.mLSN.mOffset) );

#ifndef COMPILE_64BIT    
    sState = 0;
    IDE_TEST( unlockSyncLSNMtx() != IDE_SUCCESS );
#endif
    return IDE_SUCCESS;

#ifndef COMPILE_64BIT
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( unlockSyncLSNMtx() == IDE_SUCCESS );
        IDE_POP();
    }
    
    return IDE_FAILURE;
#endif
    
}

/* Ư�� LSN���� �αװ� Flush�Ǿ����� Ȯ���Ѵ�.
 *
 * aIsSynced - [OUT] �ش� LSN���� sync�� �Ǿ����� ����.
 */
IDE_RC smrLFThread::isSynced( UInt    aFileNo,
                              UInt    aOffset,
                              idBool* aIsSynced )
{
    smrLSN4Union sTmpLSN;
    UInt         sFileNo = ID_UINT_MAX;
    UInt         sOffset = ID_UINT_MAX;
#ifndef COMPILE_64BIT
    SInt         sState = 0;
#endif

    *aIsSynced = ID_FALSE;
        
#ifndef COMPILE_64BIT
    IDE_TEST(lockSyncLSNMtx() != IDE_SUCCESS);
    sState = 1;
#endif

    ID_SERIAL_BEGIN( sTmpLSN.mSync = mLstLSN.mSync );
    ID_SERIAL_EXEC(  sFileNo = sTmpLSN.mLSN.mFileNo, 1 );
    ID_SERIAL_END(   sOffset = sTmpLSN.mLSN.mOffset );
    
    // aFileNo,aFileOffset���� �αװ� �̹� Flush�Ǿ����� üũ
    if ( aFileNo < sFileNo )
    {
        *aIsSynced = ID_TRUE;
    }
    else
    {
        if ( (aFileNo == sFileNo) && (aOffset <= sOffset) )
        {
            *aIsSynced = ID_TRUE;
        }
    }

    
#ifndef COMPILE_64BIT
    sState = 0;
    IDE_TEST(unlockSyncLSNMtx() != IDE_SUCCESS);
#endif
    
    return IDE_SUCCESS;
    
#ifndef COMPILE_64BIT
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( unlockSyncLSNMtx() == IDE_SUCCESS );
        IDE_POP();
    }
    
    return IDE_FAILURE;
#endif
}

/* ������� sync�� ��ġ�� mLSN �� �����Ѵ�.
 *
 * �켱, mLSN�� �о �̹� sync�� ��ġ�� Ȯ���ϰ�
 * �̺��� �� ū LSN�� ��쿡�� mLSN�� ������ �ǽ��Ѵ�.
 */
IDE_RC smrLFThread::setSyncedLSN(UInt    aFileNo,
                                 UInt    aOffset)
{
    smrLSN4Union sTmpLSN;

    sTmpLSN.mLSN.mFileNo = aFileNo;
    sTmpLSN.mLSN.mOffset = aOffset;
    
#ifndef COMPILE_64BIT            
    IDE_TEST(lockSyncLSNMtx() != IDE_SUCCESS);
#endif

    if ( smrCompareLSN::isLT( &mLstLSN.mLSN, &sTmpLSN.mLSN ) )
    {
        mLstLSN.mSync = sTmpLSN.mSync;
    }
    else
    {
        /* nothing to do */
    }
    
#ifndef COMPILE_64BIT            
    IDE_TEST(unlockSyncLSNMtx() != IDE_SUCCESS);
#endif
    
    return IDE_SUCCESS;

#ifndef COMPILE_64BIT
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#endif
}

IDE_RC smrLFThread::waitForCV( UInt aWaitUS )
{
    IDE_RC rc;
    UInt   sState = 0;
    PDL_Time_Value sUntilWaitTV;
    PDL_Time_Value sWaitTV;

    // USec ������ Timed Wait ����.
    sWaitTV.set(0, aWaitUS );

    sUntilWaitTV.set( idlOS::time(0) );

    sUntilWaitTV += sWaitTV;

    IDE_TEST( lockSyncLSNMtx() != IDE_SUCCESS );
    sState = 1;

    mThreadCntWaitForSync++;

    rc = mSyncWaitCV.timedwait(&mMtxSyncLSN, &sUntilWaitTV, IDU_IGNORE_TIMEDOUT);

    mThreadCntWaitForSync--;

    IDE_TEST_RAISE( rc != IDE_SUCCESS, err_cond_wait );

    sState = 0;
    IDE_TEST( unlockSyncLSNMtx() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_wait );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState == 1 )
        {
            IDE_ASSERT( unlockSyncLSNMtx() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/* mSyncLogFileList ��ũ�� ����Ʈ�� �۵����
 * ( mTBFileList �� �����ϰ� ����� )
 * 
 * 
 * 1. ��ũ�� ����Ʈ�� ����� smrLogFile ��ü(mSyncLogFileList)��
 *    ���� �α������� �����ϴµ� ������ �ʴ´�.
 *
 * 2. mSyncLogFileList ���� prv, nxt �����ͷ� ��ũ�� ����Ʈ ����
 *    �α����ϵ��� ����Ű�� �뵵�θ� ���� ���̴�..
 *
 * 3. nxt�����ʹ� ��ũ�� ����Ʈ�� head �� ����Ų��.
 *
 * 4. prv�����ʹ� ��ũ�� ����Ʈ�� tail �� ����Ų��.
 *
 * 5. ��ũ�� ����Ʈ�� ����� mSyncLogFileList�� �ּ�(&mSyncLogFileList)��
 *    nxt, prv�����Ϳ� �Ҵ�� ���, ��ũ�� ����Ʈ���� NULL�������� ������ �Ѵ�.
 *
 */
IDE_RC smrLFThread::wakeupWaiterForSync()
{
    SInt sState = 0;

    // Sync�� ��ٸ��� Thread�� ���� ��쿡�� �����.
    if ( mThreadCntWaitForSync != 0 )
    {
        IDE_TEST( lockSyncLSNMtx() != IDE_SUCCESS );
        sState = 1;

        if ( mThreadCntWaitForSync != 0 )
        {
            IDE_TEST_RAISE(mSyncWaitCV.broadcast()
                           != IDE_SUCCESS, err_cond_signal );
        }

        sState = 0;
        IDE_TEST( unlockSyncLSNMtx() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( unlockSyncLSNMtx() == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/* aFileNo, aOffset���� �αװ� sync�Ǿ����� �����Ѵ�.
 *
 * 1. Commit Transaction�� Durability�� �����ϱ� ���� ȣ��
 * 2. Log sync�� �����ϱ� ���� LogSwitch �������� ȣ��
 * 3. ���� �����ڿ� ���� ȣ��Ǹ�, �⺻���� ������
 *    noWaitForLogSync �� ����.
 */
IDE_RC smrLFThread::syncOrWait4SyncLogToLSN( smrSyncByWho  aSyncWho,
                                             UInt          aFileNo,
                                             UInt          aOffset, 
                                             UInt        * aSyncedLFCnt )
{
    idBool  sSynced;
    idBool  sLocked;
    UInt    sState      = 0;
    smLSN   sSyncLSN;

    IDE_ASSERT( aFileNo != ID_UINT_MAX );
    IDE_ASSERT( aOffset != ID_UINT_MAX );

    SM_SET_LSN( sSyncLSN,
                aFileNo,
                aOffset );

    /* BUG-35392
     * ������ LSN ���� sync�� �Ϸ�Ǳ⸦ ����Ѵ�. */
    smrLogMgr::waitLogSyncToLSN( &sSyncLSN,
                                 smuProperty::getLFGMgrSyncWaitMin(),
                                 smuProperty::getLFGMgrSyncWaitMax() );

    // LSN�� sync�� �ȵ� ���
    while ( 1 )
    {
        IDE_TEST( isSynced( aFileNo, aOffset, &sSynced ) != IDE_SUCCESS );

        if ( sSynced == ID_TRUE )
        {
            break;
        }

        IDE_TEST( mMtxThread.trylock( sLocked ) != IDE_SUCCESS );

        if ( sLocked == ID_TRUE )
        {
            // �ڽ��� ���� LSN�� Sync�ؾ��ϴ� ���
            sState = 1;

            IDE_TEST( syncLogToDiskByGroup( aSyncWho,
                                            aFileNo,
                                            aOffset,
                                            &sSynced,
                                            aSyncedLFCnt )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( unlockThreadMtx() != IDE_SUCCESS );

            // sync�Ǿ��ٸ� �Ϸ��Ѵ�.
            if ( sSynced == ID_TRUE )
            {
                break;
            }
        }

        // �ٸ� Thread�� Sync�� �����ϰ� �ִ� ���
        // Cond_timewait�ϴٰ� Signal�� �ްų�,
        // Ư�� �ð��� �귯�� �ٽ��Ϲ� Sync�Ǿ�����
        // Ȯ���Ѵ�.
        IDE_TEST( waitForCV( smuProperty::getLFGGroupCommitRetryUSec() )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState == 1 )
        {
            IDE_ASSERT( unlockThreadMtx() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC smrLFThread::syncToLSN( smrSyncByWho aWhoSync,
                               idBool       aSyncLstPageInLstLF,
                               UInt         aFileNo,
                               UInt         aOffset,
                               UInt       * aSyncLFCnt )
{
    UInt          sState        = 0;
    smrLogFile  * sCurLogFile;
    smrLogFile  * sNxtLogFile;
    UInt          sSyncOffset;
    UInt          sCurFileNo    = 0;
    UInt          sFstSyncLFNo  = 0;
    UInt          sSyncedLFCnt  = 0;
    idBool        sIsSwitchLF;
    idBool        sSyncLstPage;
    smLSN         sSyncLSN;

    /* BUG-35392 */
    if ( ( aFileNo == ID_UINT_MAX ) || ( aOffset == ID_UINT_MAX ))
    {
        smrLogMgr::getLstLSN( &sSyncLSN );
    }
    else
    {
        SM_SET_LSN( sSyncLSN,
                    aFileNo,
                    aOffset );
    }

    /* BUG-35392
     * ������ LSN ���� sync�� �Ϸ�Ǳ⸦ ����Ѵ�. */
    smrLogMgr::waitLogSyncToLSN( &sSyncLSN,
                                 smuProperty::getLFThrSyncWaitMin(),
                                 smuProperty::getLFThrSyncWaitMax() );

    // ���� sync���� ���� ����̴�.
    // �α����� �ϳ��� sync �ǽ�.
    IDE_TEST( lockListMtx() != IDE_SUCCESS );
    sState = 1;

    sCurLogFile = mSyncLogFileList.mSyncNxtLogFile;

    sState = 0;
    IDE_TEST( unlockListMtx() != IDE_SUCCESS);

    /* 
     * PROJ-2232 log multiplex
     * ����ȭ �α׿� ���� WAL�� �����°��� �����ϱ� ���� 
     * �α� ����ȭ �����带 �����.
     */ 
    IDE_TEST( smrLogMultiplexThread::wakeUpSyncThread(
                                            smrLogFileMgr::mSyncThread,
                                            aWhoSync,
                                            aSyncLstPageInLstLF,
                                            sSyncLSN.mFileNo,
                                            sSyncLSN.mOffset )
              != IDE_SUCCESS );

    /* BUG-39953 [PROJ-2506] Insure++ Warning
     * - ���� ������ ����Ʈ ������� �˻��� �� �����մϴ�.
     * - �Ǵ�, mSyncLogFileList �ʱ�ȭ ��, mFileNo�� �ʱ�ȭ�ϴ� ����� �ֽ��ϴ�.
     */
    if ( sCurLogFile != &mSyncLogFileList )
    {
        sFstSyncLFNo = sCurLogFile->mFileNo;
    }
    else
    {
        /* Nothing to do */
    }

    while (sCurLogFile != &mSyncLogFileList)
    {
        sNxtLogFile = sCurLogFile->mSyncNxtLogFile;
        sCurFileNo  = sCurLogFile->mFileNo;

        if ( sCurLogFile->getEndLogFlush() == ID_FALSE )
        {
            sIsSwitchLF = sCurLogFile->mSwitch;

            // sSyncLSN.mFileNo�� ���� sync���� ���� �α����Ϲ�ȣ��
            // mSyncLogFileList�� ������� �� �ۿ� ����.
            // sSyncLSN.mFileNo�� ��ġ�ϴ� �α������� ������ ��������
            // �ش� �α����ϵ��� ��� sync�Ѵ�.
            if ( sSyncLSN.mFileNo == sCurFileNo )
            {
                // Ư�� ��ġ���� sync�� ��û ���� ��쿡��
                // ������ LogFile�� ����̴�.
                sSyncOffset  = sSyncLSN.mOffset;
                sSyncLstPage = aSyncLstPageInLstLF;
            }
            else
            {
                // 1. aFileNo�� UInt Max�� ���
                // 2. Ư�� ��ġ���� sync�� ��û �޾Ҵµ�
                //    ���� �������� ���� ���
                // BUG-35392
                // ������ ������ �����̶� �ϴ��� switch �Ǿ�����
                // ����� �Դµ� ������ Log File�� switch �Ǿ�����
                // �׳� mOffset���� sync �ϸ� �Ǳ� �����̴�.
                // ������ BUG-28856  ������ Log File�� �ƴϴ���
                // ���� log Copy�� �Ϸ� ���� �ʾ��� �� �ִ�.
                sSyncOffset  = sCurLogFile->mOffset;
                sSyncLstPage = ID_TRUE;
            }

            // ���� ������ isSync�� ȣ���Ͽ� ���� �α� Flush �����尡
            // �ش� �α����ϵ��� �ѹ� �� sync�ϰ� mSyncLogFileList ���� �����ش�.
            // �α� Flush �����尡 �α����ϵ��� �ѹ��� �� sync�ϵ���
            // smrLogFile.syncLog�� ������ ȣ���Ͽ���
            // smrLogFile.syncLog������ �ѹ��� ��ũ�ϵ��� �����Ǿ� �ִ�.
            IDE_TEST( sCurLogFile->syncLog( sSyncLstPage,
                                            sSyncOffset )
                      != IDE_SUCCESS );

            // mLSN �� ������� sync�� LSN ����.
            IDE_TEST( setSyncedLSN( sCurLogFile->mFileNo,
                                    sCurLogFile->mSyncOffset )
                      != IDE_SUCCESS );

            // Log Sync�� ��ٸ��� Thread���� �����.
            IDE_TEST( wakeupWaiterForSync() != IDE_SUCCESS );

            if ( ( sSyncedLFCnt % 100 == 0 ) && ( sSyncedLFCnt != 0 )  )
            {
                // �ѹ� ��� ���� Sync�� ������ �α����� ������
                // 100���̻��� ��쿡 �޽����� �����.
                ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                             SM_TRC_MRECOVERY_LFTHREAD_INFO_FOR_SYNC_LOGFILE,
                             sFstSyncLFNo,
                             sCurFileNo,
                             sCurFileNo - sFstSyncLFNo + 1 );

                sFstSyncLFNo += 100;
            }

            if ( sIsSwitchLF == ID_TRUE )
            {
                /* BUG-35392 */
                if ( sCurLogFile->isAllLogSynced() == ID_TRUE )
                {
                    sSyncedLFCnt++;

                    /* �� �̻� �α����Ͽ� ��ũ�� ����� �αװ� ����
                     * ��� ��ũ�� �ݿ��� �Ǿ���. */
                    sCurLogFile->setEndLogFlush( ID_TRUE );
                }
                else
                {
                    /* ������ ������ �ƴ����� Log Copy�� �Ϸ� ���� ���� ��� */
                    break;
                }
            }
            else
            {
                /* ���� ����� ���� ���� ������ ������ ��� */
                break;
            }
        }

        if ( ( sCurLogFile->getEndLogFlush() == ID_TRUE ) &&
             ( aWhoSync == SMR_LOG_SYNC_BY_LFT ) )
        {
            /* PROJ-2232 log �α� ����ȭ�� �Ϸ� �ɶ����� ����Ѵ�. */ 
            wait4MultiplexLogFileSwitch( sCurLogFile );
            /* Sync Thread���� logfile�� close �Ѵ�. */
            IDE_TEST( closeLogFile( sCurLogFile ) != IDE_SUCCESS );
        }

        if ( sSyncLSN.mFileNo == sCurFileNo )
        {
            break;
        }

        sCurLogFile = sNxtLogFile;
    }

    if ( ( sSyncedLFCnt % 100 != 0 ) && ( sSyncedLFCnt > 100 ) )
    {
        // �ѹ� ��� ���� Sync�� ������ �α����� ������
        // 100���̻��� ��쿡 �޽����� �����. 
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_MRECOVERY_LFTHREAD_INFO_FOR_SYNC_LOGFILE,
                     sFstSyncLFNo,
                     sCurFileNo,
                     sCurFileNo - sFstSyncLFNo + 1 );
    }

    if ( aSyncLFCnt != NULL )
    {
        *aSyncLFCnt = sSyncedLFCnt;
    }

    /* PROJ-2232 ����ȭ�αװ� ���Ϸ� �ݿ��ɶ����� ����Ѵ�. */
    IDE_TEST( smrLogMultiplexThread::wait( smrLogFileMgr::mSyncThread ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch (sState )
    {
        case 1:
            IDE_ASSERT( unlockListMtx() == IDE_SUCCESS);
        default:
            break;
    }

    return IDE_FAILURE;
}

/* aFileNo, aOffset���� �αװ� sync�Ǿ����� �����Ѵ�.
 *
 * [ ���ǻ��� ]
 * mMtxThread �� ȹ��� ���¿��� ȣ��Ǿ�� �Ѵ�.
 */
IDE_RC smrLFThread::syncLogToDiskByGroup( smrSyncByWho aWhoSync,
                                          UInt         aFileNo,
                                          UInt         aOffset,
                                          idBool     * aIsSyncLogToLSN,
                                          UInt       * aSyncedLFCnt )
{
    idBool         sSynced = ID_FALSE;
    idvTime        sCurTime;
    ULong          sTimeDiff;
    
    // �� �ӿ��� �α� Flush �����带 ����� �Ǵµ�,
    // ������ �α� Flush ������ ��ġ�� ��ұ� ������,
    // �� ��ġ�� Ǯ����� �α� Flush ������� ��ٷ��� �Ѵ�.
    IDE_TEST( isSynced( aFileNo,
                        aOffset,
                        &sSynced ) != IDE_SUCCESS);

    if ( aWhoSync == SMR_LOG_SYNC_BY_TRX )
    {
        if ( sSynced == ID_TRUE )
        {
            mGCAlreadySyncCount ++;
        }
        else
        {
            // LFG_GROUP_COMMIT_UPDATE_TX_COUNT == 0 �̸�
            // Group Commit�� Disable��Ų��.
            if ( (smuProperty::getLFGGroupCommitUpdateTxCount() != 0 ) &&
                // �� LFG�� Update Transaction ����
                // LFG_GROUP_COMMIT_UPDATE_TX_COUNT���� Ŭ ��
                // Group Commit�� ���۽�Ų��.
                ( smrLogMgr::getUpdateTxCount() >=
                  smuProperty::getLFGGroupCommitUpdateTxCount() ) )
            {
                IDV_TIME_GET(&sCurTime);
                sTimeDiff = IDV_TIME_DIFF_MICRO(&mLastSyncTime,
                                                &sCurTime);

                // ���� ������ Sync�� ���ķ� ���� �ð���
                // LFG_GROUP_COMMIT_INTERVAL_USEC ��ŭ ������ �ʾҴٸ�
                // �α����� Sync�� ������Ų��.
                if ( sTimeDiff < smuProperty::getLFGGroupCommitIntervalUSec() ) 
                {
                    mGCWaitCount++;
                    *aIsSyncLogToLSN = ID_FALSE;

                    return IDE_SUCCESS;
                }
            }

            mGCRealSyncCount ++;
        }
    }

    if ( sSynced == ID_FALSE )
    {
        IDV_TIME_GET( &mLastSyncTime );

        IDE_TEST( syncToLSN( SMR_LOG_SYNC_BY_TRX,
                             ID_TRUE,
                             aFileNo,
                             aOffset,
                             aSyncedLFCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // �̹� sync�Ǿ��ٸ� �� �̻� ������ ����.
    }

    *aIsSyncLogToLSN  = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �α� Flush �������� run�Լ�
 * �ֱ�������, Ȥ�� ������� ��û�� ���� ����� 
 * �α������� Flush�ϰ� ������ Flush�� �α�������
 * Sync��� �α����� ����Ʈ���� �����Ѵ�.
 */
void smrLFThread::run()
{
    IDE_RC         rc;
    UInt           sState = 0;
    PDL_Time_Value sCurTimeValue;
    PDL_Time_Value sFixTimeValue;
    UInt           sSyncedLFCnt    = 0;
    
  startPos:
    sState=0;
    
    IDE_TEST( lockThreadMtx() != IDE_SUCCESS );
    sState = 1;

    sFixTimeValue.set(smuProperty::getSyncIntervalSec(),
                      smuProperty::getSyncIntervalMSec() * 1000);
    
    while ( mFinish == ID_FALSE ) 
    {
        sSyncedLFCnt = 0;

        sCurTimeValue = idlOS::gettimeofday();

        sCurTimeValue += sFixTimeValue;

        sState = 0;
        rc = mCV.timedwait(&mMtxThread, &sCurTimeValue, IDU_IGNORE_TIMEDOUT);
        sState = 1;

        if ( mFinish == ID_TRUE )
        {
            break;
        }

        if ( smuProperty::isRunLogFlushThread() == SMU_THREAD_OFF )
        {
            // To Fix PR-14783
            // System Thread�� �۾��� �������� �ʵ��� �Ѵ�.
            continue;
        }
        else
        {
            // Go Go 
        }
        
        IDE_TEST_RAISE( rc != IDE_SUCCESS, err_cond_wait );

        // �α������� Flush�ϰ� ������ Flush�� �α�������
        // Sync��� �α����� ����Ʈ���� �����Ѵ�.
        IDE_TEST( syncToLSN( SMR_LOG_SYNC_BY_LFT,
                             ID_FALSE,
                             ID_UINT_MAX,
                             ID_UINT_MAX,
                             &sSyncedLFCnt ) != IDE_SUCCESS );

        if ( sSyncedLFCnt > 100 )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                         SM_TRC_MRECOVERY_LFTHREAD_TOTAL_SYNCED_LOGFILE_COUNT,
                         sSyncedLFCnt );
        }
    }

    // ������ �����ϱ� ���� �α������� �ٽ��ѹ� Flush�Ѵ�.
    //
    // BUGBUG �� �ڵ�� �Բ� ������ mFinish == ID_TRUE üũ�Ͽ� break�ϴ� �ڵ�
    // ���� ���� �ڵ�� ������ ����.

    IDE_TEST( syncToLSN( SMR_LOG_SYNC_BY_LFT,
                         ID_TRUE,
                         ID_UINT_MAX,
                         ID_UINT_MAX,
                         &sSyncedLFCnt )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlockThreadMtx() != IDE_SUCCESS );
    
    return;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        (void)unlockThreadMtx();
        IDE_POP();
    }

    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                SM_TRC_MRECOVERY_LFTHREAD_INSUFFICIENT_MEMORY_WARNNING);

    goto startPos;
}

/* �α� Flush �����带 �����Ų��.
 */
IDE_RC smrLFThread::shutdown()
{
    UInt           sState = 0;

    // �����尡 ���ڰ� �ִٴ� ���� �����ϱ� ���� ������ ��ġ�� ����
    IDE_TEST(lockThreadMtx() != IDE_SUCCESS);
    sState = 1;

    // ������ �����ϵ��� �÷��� ����
    mFinish = ID_TRUE;

    // �����带 �����.
    IDE_TEST_RAISE(mCV.signal() != IDE_SUCCESS, err_cond_signal);

    // �����尡 ��� �� �ֵ��� ������ ��ġ ����
    sState = 0;
    IDE_TEST( unlockThreadMtx() != IDE_SUCCESS );

    IDE_TEST_RAISE(join() != IDE_SUCCESS, err_thr_join);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));   
    }
    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0)
    {
        IDE_PUSH();
        (void)unlockThreadMtx();
        IDE_POP();
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α� ������ close�Ѵ�.
 *
 * 1. sync logfile list���� ����
 * 2. Archive Mode�̸� Archive List�� �߰�
 * 3. logfile close��û.
 *
 * aLogFile - [IN] logfile pointer
 *
 **********************************************************************/
IDE_RC smrLFThread::closeLogFile( smrLogFile *aLogFile )
{
    // ������ ��ä�� Flush�����Ƿ� sync�� �α����� ����Ʈ���� ����.
    IDE_TEST( removeSyncLogFile( aLogFile )
              != IDE_SUCCESS);

    // ��ī�̺� ����� ��, ��ī�̺� �α׷� �߰�.
    if (smrRecoveryMgr::getArchiveMode()
        == SMI_LOG_ARCHIVE)
    {
        IDE_TEST(mArchThread->addArchLogFile( aLogFile->mFileNo )
                 != IDE_SUCCESS);
    }

    IDE_DASSERT( mLogFileMgr != NULL );
            
    IDE_TEST( mLogFileMgr->close(aLogFile)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

void smrLFThread::dump()
{
    smrLogFile *sCurLogFile;
    smrLogFile *sNxtLogFile;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_LFTHREAD_SYNC_LOGFILE_LIST);
    sCurLogFile = mSyncLogFileList.mSyncNxtLogFile;
    while(sCurLogFile != &mSyncLogFileList)
    {
        sNxtLogFile = sCurLogFile->mSyncNxtLogFile;
        
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_LFTHREAD_SYNC_LOGFILE_NO,
                    sCurLogFile->mFileNo); 
        sCurLogFile = sNxtLogFile;
    }
}
