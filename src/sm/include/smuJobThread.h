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
 * $Id: smuJobThread.h
 *
 * TASK-6887 Improve index rebuild efficiency at server startup ���� �߰���
 *
 * Description :
 * smuWorkerThread �� Queue Interface ���� ����
 *
 * Algorithm  :
 * JobQueue�� SingleWriterMulterReader �� ����Ͽ�, ������ �й��Ѵ�.
 * Queue Pop �� Thread ���� Lock ��ȣ�Ʒ� ���������� Pop �Ѵ�
 *
 * Issue :
 *
 **********************************************************************/

#ifndef _O_SMU_JOB_THREAD_H_
#define _O_SMU_JOB_THREAD_H_ 1

#include <idu.h>
#include <idl.h>
#include <ide.h>
#include <idtBaseThread.h>

/* Example:
 *  smuJobThreadMgr sThreadMgr;
 *
 *  IDE_TEST( smuJobThread::initialize( 
 *        <������ ó���ϴ� �Լ�>,
 *        <ChildThread����>,
 *        <Queueũ��>,
 *        &sThreadMgr )
 *    != IDE_SUCCESS );
 *
 *  IDE_TEST( smuJobThread::addJob( &sThreadMgr, <��������> ) != IDE_SUCCESS );
 *  IDE_TEST( smuJobThread::finalize( &sThreadMgr ) != IDE_SUCCESS );
*/

#define SMU_JT_Q_NEXT( __X, __Q_SIZE ) ( ( __X + 1 ) % __Q_SIZE )
#define SMU_JT_Q_IS_EMPTY( __MGR )     ( __MGR->mJobHead == __MGR->mJobTail )
#define SMU_JT_Q_IS_FULL( __MGR )      \
    ( __MGR->mJobHead == SMU_JT_Q_NEXT( __MGR->mJobTail, __MGR->mQueueSize ) )

typedef void (*smuJobThreadFunc)(void * aParam );

class smuJobThread;

typedef struct smuJobThreadMgr
{
    smuJobThreadFunc   mThreadFunc;     /* ���� ó���� �Լ� */
                    
    UInt               mJobHead;        /* Queue�� Head */
    UInt               mJobTail;        /* Queue�� Tail */
    UInt               mQueueSize;      /* Queue�� ũ�� */
    iduMutex           mQueueLock;      /* Queue Lock */
    void            ** mJobQueue;       /* Job Queue. */

    UInt               mWaitAddJob;
    iduMutex           mWaitLock;       
    iduCond            mConsumeCondVar; /* Thread signals to AddJob */

    idBool             mDone;           /* ���̻� ������ ���°�? */
    UInt               mThreadCnt;      /* Thread ���� */
    smuJobThread     * mThreadArray;    /* ChildThread�� */
} smuJobThreadMgr;

class smuJobThread : public idtBaseThread
{
public:
    static UInt       mWaitIntvMsec;
    UInt              mJobIdx;          /* �ڽ��� ������ Job�� Index */
    smuJobThreadMgr * mThreadMgr;       /* �ڽ��� �����ϴ� ������ */

    smuJobThread() : idtBaseThread() {}

    static IDE_RC initialize( smuJobThreadFunc    aThreadFunc, 
                              UInt                aThreadCnt,
                              UInt                aQueueSize,
                              smuJobThreadMgr   * aThreadMgr );
    static IDE_RC finalize(   smuJobThreadMgr   * aThreadMgr );
    static IDE_RC addJob(     smuJobThreadMgr   * aThreadMgr, void * aParam );
    static void   wait(       smuJobThreadMgr   * aThreadMgr );

    virtual void run(); /* ��ӹ��� main ���� ��ƾ */
};

#endif 

