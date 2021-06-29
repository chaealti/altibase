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
 * $Id: smrLFThread.h 82426 2018-03-09 05:12:27Z emlee $
 **********************************************************************/

#ifndef _O_SMR_LF_THREAD_H_
#define _O_SMR_LF_THREAD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <smrDef.h>
#include <smrArchThread.h>
#include <smrLogFile.h>
#include <idvTime.h>

/* Log Fulsh Thread
 *
 * �ֱ������� ����� ���� Flush�� �α����ϵ��� Flush�Ѵ�.
 *
 * Ȥ��, �ܺ� ������ ��û�� ���� �α����ϵ��� Flush�Ѵ�.
 *
 */

class smrLFThread : public idtBaseThread
{
//For Operation    
public:
    /* Sync��� �α����� ����Ʈ�� �α������� �߰�
     */
    IDE_RC addSyncLogFile( smrLogFile    * aLogFile );

    /* Sync��� �α����� ����Ʈ���� �α������� ����
     */
    IDE_RC removeSyncLogFile( smrLogFile    * aLogFile );

    /* aFileNo, aOffset���� �αװ� sync�Ǿ����� �����Ѵ�.
     * 
     * 1. Commit Transaction�� Durability�� �����ϱ� ���� ȣ��
     * 2. Log sync�� �����ϱ� ���� LogSwitch �������� ȣ��
     * 3. ���� �����ڿ� ���� ȣ��Ǹ�, �⺻���� ������
     *    noWaitForLogSync �� ����.
     */
    IDE_RC syncOrWait4SyncLogToLSN( smrSyncByWho  aSyncWho,
                                    UInt          aFileNo,
                                    UInt          aOffset,
                                    UInt        * aSyncedLFCnt );

    /* aFileNo, aOffset���� �αװ� sync�Ǿ����� �����Ѵ�.
     *
     * �̹� sync�Ǿ����� �ѹ��� check �� �ٷ� �α׸� sync�Ѵ�.
     */
    IDE_RC syncLogToDiskByGroup( smrSyncByWho aWhoSync,
                                 UInt         aFileNo,
                                 UInt         aOffset,
                                 idBool     * aIsSyncLogToLSN,
                                 UInt       * aSyncedLFCnt );

    // Ư�� LSN���� �αװ� sync�Ǿ����� Ȯ���Ѵ�.
    IDE_RC isSynced( UInt    aFileNo,
                     UInt    aOffset,
                     idBool* aIsSynced) ;
    
    // ������� sync�� LSN�� �����Ѵ�.
    // mSyncLSN �� ���� thread-safe�� getter
    IDE_RC getSyncedLSN( smLSN *aLSN );

    // ������� sync�� ��ġ�� mSyncLSN �� �����Ѵ�.
    // mSyncLSN �� ���� thread-safe�� setter
    IDE_RC setSyncedLSN( UInt    aFileNo,
                         UInt    aOffset );

    // ���̺� ��� ������ ���������� ���θ� �����Ѵ�.
    IDE_RC setRemoveFlag( idBool aRemove );
    
    virtual void run();
    IDE_RC shutdown();

    /* �ν��Ͻ��� �ʱ�ȭ�ϰ� �α� Flush �����带 ������
     */
    IDE_RC initialize( smrLogFileMgr   * aLogFileMgr,
                       smrArchThread   * aArchThread );
    
    IDE_RC destroy();
   
    void dump();

    // V$LFG �� ������ Group Commit ���ġ
    // Commit�Ϸ��� Ʈ����ǵ��� Log�� Flush�Ǳ⸦ ��ٸ� Ƚ��
    inline UInt getGroupCommitWaitCount();
    // Commit�Ϸ��� Ʈ����ǵ��� Flush�Ϸ��� Log�� ��ġ��
    // �̹� Log�� Flush�� ������ �Ǹ�Ǿ� �������� Ƚ��
    inline UInt getGroupCommitAlreadySyncCount();
    // Commit�Ϸ��� Ʈ����ǵ��� ������ Log �� Flush�� Ƚ��
    inline UInt getGroupCommitRealSyncCount();

    smrLFThread();
    virtual ~smrLFThread();
public:

    inline IDE_RC lockListMtx()
    { return mMtxList.lock( NULL ); };
    inline IDE_RC unlockListMtx()
    { return mMtxList.unlock(); };

    inline IDE_RC lockThreadMtx()
    { return mMtxThread.lock( NULL ); };
    inline IDE_RC unlockThreadMtx()
    { return mMtxThread.unlock(); };

    inline IDE_RC lockSyncLSNMtx()
    { return mMtxSyncLSN.lock( NULL ); };
    inline IDE_RC unlockSyncLSNMtx()
    { return mMtxSyncLSN.unlock(); };

    inline void wait4MultiplexLogFileSwitch( smrLogFile * aLogFile );

private:
    IDE_RC closeLogFile( smrLogFile *aLogFile );
    IDE_RC waitForCV( UInt     aFileNo );
    IDE_RC wakeupWaiterForSync();

    IDE_RC syncToLSN( smrSyncByWho aWhoSync,
                      idBool       aSyncLstPage,
                      UInt         aFileNo,
                      UInt         aOffset,
                      UInt        *aSyncLFCnt );

//For Member
private:
    // �� �α� Flush �����尡 Flush�ϴ� �α����ϵ���
    // �����ϴ� �α����� ������.
    // �ϳ��� �α׸� Flush�� ���� �ش� �α� ������ close�ؾ� �ϴµ�,
    // �ٷ� �� �α����� �����ڰ� �α������� close�� ����Ѵ�.
    smrLogFileMgr    * mLogFileMgr;

    // �α� Flush thread�� ������ �����ϱ� ���� Condition value
    // mMtxThread�� �Բ� ���ȴ�.
    iduCond            mCV;
    PDL_Time_Value     mTV;

    // �α����� Flush �����带 �����ؾ� ������ ����
    // �� �÷��װ� ���õǸ� run()�Լ��� ���� �������ͼ� ����Ǿ�� �Ѵ�.
    idBool             mFinish;

    // mSyncLogFileList �� ���ü��� �����ϱ� ���� ���Ǵ� Mutex.
    iduMutex           mMtxList;

    // �α� Flush thread�� ������ �����ϱ� ���� Mutex.
    // mCV, mTV�� �Բ� ���ȴ�.
    iduMutex           mMtxThread;

    // 32��Ʈ���� 64��Ʈ �������� mLSN��
    // atomic�ϰ� Read/Write�ϱ� ���� ����ϴ� Mutex
    iduMutex           mMtxSyncLSN;
    iduCond            mSyncWaitCV;

    // ������� Flush�� LSN.
    smrLSN4Union       mLstLSN;

    // sync ����� �α������� ����Ʈ.
    smrLogFile         mSyncLogFileList;

    // ���������� Log Sync�� �� �ð�
    idvTime            mLastSyncTime;

    // V$LFG �� ������ Group Commit ���ġ
    // Commit�Ϸ��� Ʈ����ǵ��� Log�� Flush�Ǳ⸦ ��ٸ� Ƚ��
    UInt               mGCWaitCount;
    // Commit�Ϸ��� Ʈ����ǵ��� Flush�Ϸ��� Log�� ��ġ��
    // �̹� Log�� Flush�� ������ �Ǹ�Ǿ� �������� Ƚ��
    UInt               mGCAlreadySyncCount;
    // Commit�Ϸ��� Ʈ����ǵ��� ������ Log �� Flush�� Ƚ��
    UInt               mGCRealSyncCount;

    // Archive Thread Ptr
    smrArchThread*     mArchThread;

    // Log�� Sync�� �Ǳ⸦ ����ϴ� Thread�� ����
    UInt               mThreadCntWaitForSync;
};


// V$LFG �� ������ Group Commit ���ġ
// Commit�Ϸ��� Ʈ����ǵ��� Log�� Flush�Ǳ⸦ ��ٸ� Ƚ��
UInt smrLFThread::getGroupCommitWaitCount()
{
    return mGCWaitCount;
}


// Commit�Ϸ��� Ʈ����ǵ��� Flush�Ϸ��� Log�� ��ġ��
// �̹� Log�� Flush�� ������ �Ǹ�Ǿ� �������� Ƚ��
UInt smrLFThread::getGroupCommitAlreadySyncCount()
{
    return mGCAlreadySyncCount;
}


// Commit�Ϸ��� Ʈ����ǵ��� ������ Log �� Flush�� Ƚ��
UInt smrLFThread::getGroupCommitRealSyncCount()
{
    return mGCRealSyncCount;
}

/* mSyncLogFileList ��ũ�� ����Ʈ�� �۵����
 * ( mTBFileList �� �����ϰ� ����� )
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

/*
 * �α����� ����ȭ�� SMR_LT_FILE_END���� �Ϸ�Ǿ� ���� ����ȭ �α����� switch��
 * ����ɶ� ���� ����Ѵ�.
 *
 * aLogFile - [IN] ���� �α�����
 */ 
inline void smrLFThread::wait4MultiplexLogFileSwitch( smrLogFile * aLogFile )
{
    PDL_Time_Value    sTV;    
    UInt              sMultiplexIdx;

    sTV.set( 0, 1 );

    for( sMultiplexIdx = 0; 
         sMultiplexIdx < smrLogMultiplexThread::mMultiplexCnt;
         sMultiplexIdx++ )
    {
        while( aLogFile->mMultiplexSwitch[ sMultiplexIdx ] == ID_FALSE )
        {
            idlOS::sleep( sTV );
        }
    }
}

#endif /* _O_SMR_LF_THREAD_H_ */
