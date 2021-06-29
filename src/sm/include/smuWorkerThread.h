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
 * $Id: smuWorkerThread.h
 *
 * PROJ-2162 RestartRiskReduction ���� �߰���
 *
 * Description :
 * Thread�� ���� Wrapping class.
 * ���� ChildThread���� ���� �й��� �� �ִ� ���� ������ Ŭ����
 *
 * Algorithm  :
 * JobQueue�� SingleWriterMulterReader �� ����Ͽ�, ������ �й��Ѵ�.
 *
 * ChildThread�� 4�� �ִٰ� ������,
 * ThreadMgr�� Job�� Queue�� 1��,2��,3��,4��,5�� ���� Slot�� ����ϸ�,
 * 0��  - 0,4,8,12
 * 1��  - 1,5,9,13
 * 2��  - 2,6,10,14
 * 3��  - 3,7,11,15
 * �� �������� ���� �����Ѵ�.
 *
 * ���� �� Job�� �ҿ�ð��� ����, Ư�� ClientThread�� ó���� �ʾ�����
 * ������ ��Ÿ�� ���� �ִ�.
 *
 * Issue :
 * 1) Queue�� ���ؼ��� DirtyWrite/DirtyRead�� �����Ѵ�. Pointer����
 *    �̱� ������, ���� �������� �� ��ü�� Atomic�ϱ� ������ �����ϴ�.
 * 2) ������ ABA Problem�� �߻����� ���, �ٸ� Core�� CacheMiss��
 *    �˷����� �ʴ� ������ �߻��� �� �����Ƿ�, �ش� ��쿡��
 *    volatile �� ����Ѵ�.
 * 3) ThreadCnt�� 1�� ���, ThreadMgr�� addJob �Լ����� ���� Job��
 *    ó���Ѵ�. ������ ��� ThreadMgr�� ����� �������� �ʾ� Hang��
 *    ������ ��츦 �����, ThreadCnt�� 1�̸� SingleThreadó��
 *    �����ϵ��� �ϱ� �����̴�. 
 *
 **********************************************************************/

#ifndef _O_SMU_WORKER_THREAD_H_
#define _O_SMU_WORKER_THREAD_H_ 1

#include <idu.h>
#include <ide.h>
#include <idtBaseThread.h>

/* Example:
 *  smuWorkerThreadMgr     sThreadMgr;
 *
 *  IDE_TEST( smuWorkerThread::initialize( 
 *        <������ ó���ϴ� �Լ�>,
 *        <ChildThread����>,
 *        <Queueũ��>,
 *        &sThreadMgr )
 *    != IDE_SUCCESS );
 *
 *  IDE_TEST( smuWorkerThread::addJob( &sThreadMgr, <��������> ) != IDE_SUCCESS );
 *  IDE_TEST( smuWorkerThread::finalize( &sThreadMgr ) != IDE_SUCCESS );
*/

typedef void (*smuWorkerThreadFunc)(void * aParam );

class smuWorkerThread;

typedef struct smuWorkerThreadMgr
{
    smuWorkerThreadFunc    mThreadFunc;    /* ���� ó���� �Լ� */
    UInt                   mThreadCnt;     /* Thread ���� */
    idBool                 mDone;          /* ���̻� ������ ���°�? */
                    
    void                ** mJobQueue;      /* Job Queue. */
    UInt                   mJobTail;       /* Queue�� Tail */
    UInt                   mQueueSize;     /* Queue�� ũ�� */

    smuWorkerThread      * mThreadArray;   /* ChildThread�� */
} smuWorkerThreadMgr;

class smuWorkerThread : public idtBaseThread
{
public:
    UInt                 mJobIdx;          /* �ڽ��� ������ Job�� Index */
    smuWorkerThreadMgr * mThreadMgr;       /* �ڽ��� �����ϴ� ������ */

    smuWorkerThread() : idtBaseThread() {}

    static IDE_RC initialize( smuWorkerThreadFunc    aThreadFunc, 
                              UInt             aThreadCnt,
                              UInt             aQueueSize,
                              smuWorkerThreadMgr   * aThreadMgr );
    static IDE_RC finalize( smuWorkerThreadMgr * aThreadMgr );
    static IDE_RC addJob( smuWorkerThreadMgr * aThreadMgr, void * aParam );
    static void   wait( smuWorkerThreadMgr * aThreadMgr );

    virtual void run(); /* ��ӹ��� main ���� ��ƾ */
};

#endif 

