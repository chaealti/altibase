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
 * $Id: smuJobThread.cpp $
 **********************************************************************/

#include <smuJobThread.h>
#include <smErrorCode.h>

UInt smuJobThread::mWaitIntvMsec = 100000; /* 100 Millisec */

IDE_RC smuJobThread::initialize( smuJobThreadFunc  aThreadFunc, 
                                 UInt              aThreadCnt,
                                 UInt              aQueueSize,
                                 smuJobThreadMgr * aThreadMgr )
{
    smuJobThread *          sThread;
    UInt                    sState = 0;
    UInt                    i = 0;
    UInt                    j;

    IDE_ASSERT( ( aQueueSize % aThreadCnt ) == 0 );
    IDE_ASSERT( aThreadCnt >= 1  );

    aThreadMgr->mThreadFunc = aThreadFunc;
    aThreadMgr->mThreadCnt  = aThreadCnt;
    aThreadMgr->mDone       = ID_FALSE;
                            
    aThreadMgr->mQueueSize  = aQueueSize;
    aThreadMgr->mJobHead    = 0;
    aThreadMgr->mJobTail    = 0;
    aThreadMgr->mWaitAddJob = 0;

    if( aThreadCnt == 1 )
    {
        /* Thread�� �ϳ���, Queue�� �̿�ġ �ʰ� ���� �����Ѵ�. */
    }
    else
    {
        SChar sConvVarName[128] = "SMU_WORKER_CONS_COND";

        IDE_TEST( aThreadMgr->mConsumeCondVar.initialize( sConvVarName ) != IDE_SUCCESS );
        sState = 1;

        /* smuJobThread_initialize_calloc_JobQueue.tc */
        IDU_FIT_POINT( "smuJobThread::initialize::calloc::JobQueue" );
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMU,
                                     aQueueSize,
                                     ID_SIZEOF( void * ),
                                     (void**)&( aThreadMgr->mJobQueue ) )
                  != IDE_SUCCESS );

        idlOS::memset( aThreadMgr->mJobQueue, 0, (size_t)aQueueSize * ID_SIZEOF( void * ) );
        sState = 2;

        /* smuJobThread_initialize_calloc_ThreadArray.tc */
        IDU_FIT_POINT( "smuJobThread::initialize::calloc::ThreadArray" );
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMU, 1,
                                     (ULong)ID_SIZEOF( smuJobThread ) * aThreadCnt,
                                     (void**)&(aThreadMgr->mThreadArray) )
                  != IDE_SUCCESS );
        sState = 3;

        /* smuJobThread_initialize_calloc_QueueLock.tc */
        IDU_FIT_POINT( "smuJobThread::initialize::QueueLock" );
        IDE_TEST( aThreadMgr->mQueueLock.initialize( "SMU_WORKER_Q_LOCK",
                                                     IDU_MUTEX_KIND_NATIVE,
                                                     IDV_WAIT_INDEX_NULL)
                  != IDE_SUCCESS);
        sState = 4;

        /* smuJobThread_initialize_calloc_WaitLock.tc */
        IDU_FIT_POINT( "smuJobThread::initialize::WaitLock" );
        IDE_TEST( aThreadMgr->mWaitLock.initialize( "SMU_WORKER_COND_LOCK",
                                                    IDU_MUTEX_KIND_POSIX,
                                                    IDV_WAIT_INDEX_NULL)
                  != IDE_SUCCESS);
        sState = 5;

        for ( ; i < aThreadMgr->mThreadCnt ; ++i )
        {
            sThread = aThreadMgr->mThreadArray + i;
            new (sThread) smuJobThread;
            sThread->mThreadMgr    = aThreadMgr;
            sThread->mJobIdx       = i;
            /* smuJobThread_initialize_start.tc */
            IDU_FIT_POINT_RAISE( "smuJobThread::initialize::start", thread_create_failed );
            IDE_TEST( aThreadMgr->mThreadArray[i].start() != IDE_SUCCESS );
        }
        sState = 6;
    }

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( thread_create_failed );
    {
       IDE_SET( ideSetErrorCode( idERR_ABORT_THR_CREATE_FAILED ) );
    }
#endif

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 6:
    case 5: /* State=5 ���� for loop ���� ���� Thread start() ���� �� ���
             * State=5 �̸鼭 �Ϻ� Thread �� start ���� �� �� �ִ�
             */
        aThreadMgr->mDone = ID_TRUE;

        for ( j = 0 ; j < i ; ++j )
        {
            IDE_ASSERT( aThreadMgr->mThreadArray[j].join() == IDE_SUCCESS );
        }

        IDE_ASSERT( aThreadMgr->mWaitLock.destroy() == IDE_SUCCESS );
    case 4:
        IDE_ASSERT( aThreadMgr->mQueueLock.destroy() == IDE_SUCCESS );
    case 3:
        IDE_ASSERT( iduMemMgr::free( aThreadMgr->mThreadArray ) 
                    == IDE_SUCCESS );
    case 2:
        IDE_ASSERT( iduMemMgr::free( aThreadMgr->mJobQueue ) 
                    == IDE_SUCCESS );
    case 1:
        IDE_ASSERT( aThreadMgr->mConsumeCondVar.destroy() == IDE_SUCCESS );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

IDE_RC smuJobThread::finalize( smuJobThreadMgr * aThreadMgr )
{
    UInt           sState = 6;
    UInt           i = 0;

    if( aThreadMgr->mThreadCnt == 1 )
    {
        /* Thread�� �ϳ���, ���ٸ� ��ü�� �������� �ʴ´�. */
    }
    else
    {
        wait( aThreadMgr );

        sState = 5;
        aThreadMgr->mDone = ID_TRUE;

        for ( ; i < aThreadMgr->mThreadCnt ; ++i )
        {
            IDE_TEST_RAISE( aThreadMgr->mThreadArray[i].join() != IDE_SUCCESS,
                            thr_join_error );
        }

        IDE_DASSERT( aThreadMgr->mQueueLock.lock(NULL) == IDE_SUCCESS );
        IDE_DASSERT( SMU_JT_Q_IS_EMPTY( aThreadMgr ) == ID_TRUE );
        IDE_DASSERT( aThreadMgr->mQueueLock.unlock() == IDE_SUCCESS );

        sState = 4;
        IDE_TEST( aThreadMgr->mWaitLock.destroy() != IDE_SUCCESS );

        sState = 3;
        IDE_TEST( aThreadMgr->mQueueLock.destroy() != IDE_SUCCESS );

        sState = 2;
        IDE_TEST( iduMemMgr::free( aThreadMgr->mThreadArray ) != IDE_SUCCESS );

        sState = 1;
        IDE_TEST( iduMemMgr::free( aThreadMgr->mJobQueue ) != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( aThreadMgr->mConsumeCondVar.destroy() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( thr_join_error );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_Systhrjoin ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 6:
    case 5: /* State=5 ���� for loop ���� ���� Thread join() ���� �� ���
             * State=5 �̸鼭 �Ϻ� Thread �� run ���� �� �� �ִ�
             * i ��° Thread join �� ���������Ƿ� i �������� �ٽ� join �Ѵ�
             */
        aThreadMgr->mDone = ID_TRUE;
        for ( ++i ; i < aThreadMgr->mThreadCnt ; ++i )
        {
            IDE_ASSERT( aThreadMgr->mThreadArray[i].join() == IDE_SUCCESS );
        }

        IDE_ASSERT( aThreadMgr->mWaitLock.destroy() == IDE_SUCCESS );
    case 4:
        IDE_ASSERT( aThreadMgr->mQueueLock.destroy() == IDE_SUCCESS );
    case 3:
        IDE_ASSERT( iduMemMgr::free( aThreadMgr->mThreadArray ) 
                    == IDE_SUCCESS );
    case 2:
        IDE_ASSERT( iduMemMgr::free( aThreadMgr->mJobQueue ) == IDE_SUCCESS );
    case 1:
        IDE_ASSERT( aThreadMgr->mConsumeCondVar.destroy() == IDE_SUCCESS );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

IDE_RC smuJobThread::addJob( smuJobThreadMgr * aThreadMgr,
                             void            * aParam )
{
    if( aThreadMgr->mThreadCnt == 1 )
    {
        /* Thread�� 1���̸� ���� �����Ѵ�. */
        aThreadMgr->mThreadFunc( (void*)aParam );
    }
    else
    {
        do {
            IDE_ASSERT( aThreadMgr->mQueueLock.lock( NULL ) == IDE_SUCCESS );

            if ( SMU_JT_Q_IS_FULL( aThreadMgr ) == ID_FALSE )
            {

                aThreadMgr->mJobQueue[ aThreadMgr->mJobTail ] = aParam;
                aThreadMgr->mJobTail
                        = SMU_JT_Q_NEXT( aThreadMgr->mJobTail,
                                         aThreadMgr->mQueueSize );

                IDE_ASSERT( aThreadMgr->mQueueLock.unlock() == IDE_SUCCESS );
                break;
            }

            IDE_ASSERT( aThreadMgr->mQueueLock.unlock() == IDE_SUCCESS );

            PDL_Time_Value sTVIntv;
            PDL_Time_Value sTVWait;

            sTVIntv.set( 0, mWaitIntvMsec );
            sTVWait = idlOS::gettimeofday();
            sTVWait += sTVIntv;

            IDE_ASSERT( aThreadMgr->mWaitLock.lock( NULL ) == IDE_SUCCESS );

            ++aThreadMgr->mWaitAddJob;
            if ( aThreadMgr->mConsumeCondVar.timedwait( &aThreadMgr->mWaitLock,
                                                        &sTVWait,
                                                        IDU_IGNORE_TIMEDOUT )
                    == IDE_SUCCESS )
            {
                --aThreadMgr->mWaitAddJob;

                IDE_ASSERT( aThreadMgr->mWaitLock.unlock() == IDE_SUCCESS );
            }
            else
            {
                /* timedwait ���� �� sleep���� ������ ����Ѵ�. */
                --aThreadMgr->mWaitAddJob;

                IDE_ASSERT( aThreadMgr->mWaitLock.unlock() == IDE_SUCCESS );
                IDE_DASSERT( sTVIntv.msec() == mWaitIntvMsec );
                idlOS::sleep( sTVIntv );
            }
        } while ( ID_TRUE );
    }

    return IDE_SUCCESS;
}

void smuJobThread::wait( smuJobThreadMgr * aThreadMgr )
{
    idBool           sRemainJob;

    if( aThreadMgr->mThreadCnt == 1 )
    {
        /* Thread�� 1����, ������ �� ���ߴ�. �ڽ��� �����ϱ� ������.  */
    }
    else
    {
        /* Queue�� ��� �������, �ؾ��� ���� ���� ��.
         * �ֳ��ϸ� addJob�� �ϴ� MainThread�� �� �Լ��� ȣ���ϴϱ� */
        do {
            IDE_ASSERT( aThreadMgr->mQueueLock.lock( NULL ) == IDE_SUCCESS );

            sRemainJob = (idBool)( SMU_JT_Q_IS_EMPTY( aThreadMgr ) == ID_FALSE );

            IDE_ASSERT( aThreadMgr->mQueueLock.unlock() == IDE_SUCCESS );

            if ( sRemainJob == ID_TRUE )
            {
                PDL_Time_Value sTV;

                sTV.set( 0, mWaitIntvMsec );
                idlOS::sleep( sTV );
            }
        } while ( sRemainJob == ID_TRUE );
    }
}

void smuJobThread::run()
{
    void           * sParam;
    PDL_Time_Value   sTV;

    sTV.set( 0, mWaitIntvMsec );

#ifdef DEBUG
    PDL_Time_Value   sTvWait;

    sTvWait.set( 0, 0 );
#endif

    do
    {
        sParam = NULL;

        IDE_ASSERT( mThreadMgr->mQueueLock.lock(NULL) == IDE_SUCCESS );

        if ( SMU_JT_Q_IS_EMPTY( mThreadMgr ) == ID_FALSE )
        {
            sParam = mThreadMgr->mJobQueue[ mThreadMgr->mJobHead ];
            mThreadMgr->mJobQueue[ mThreadMgr->mJobHead ] = NULL;
            mThreadMgr->mJobHead
                    = SMU_JT_Q_NEXT( mThreadMgr->mJobHead,
                                     mThreadMgr->mQueueSize );
        }

        IDE_ASSERT( mThreadMgr->mQueueLock.unlock() == IDE_SUCCESS );

        if( sParam != NULL )
        {
            mThreadMgr->mThreadFunc( sParam );

            /* CPU Register�� �ƴ� Memory���� Ȯ���غ��� Lock �ȿ��� Ȯ���Ѵ� */
            volatile UInt *sWaitAddJob = &(mThreadMgr->mWaitAddJob);
            if ( *sWaitAddJob > 0 )
            {
                IDE_ASSERT( mThreadMgr->mWaitLock.lock(NULL) == IDE_SUCCESS );
                if ( *sWaitAddJob > 0 )
                {
                    (void)mThreadMgr->mConsumeCondVar.signal();
                }
                IDE_ASSERT( mThreadMgr->mWaitLock.unlock() == IDE_SUCCESS );
            }
        }
        else
        {
            /* Queue Empty �� ��쿡�� Thread ���� ����(mDone)�� Ȯ���Ѵ�.
             * Thread ���� ������ ��� �۾��� �Ϸ� �ؾ��Ѵ�
             */
            if ( mThreadMgr->mDone == ID_TRUE )
            {
                break;
            }

#ifdef DEBUG
            sTvWait += sTV;
            ideLog::log( IDE_SM_0,
                         "[ID-%"ID_UINT32_FMT"] "
                         "Job Thread Queue Empty",
                         mJobIdx );
#endif

            idlOS::sleep( sTV );
        }
    }
    while ( ID_TRUE );
           
#ifdef DEBUG
    if ( sTvWait.msec() >= 1 )
    {
        ideLog::log( IDE_SM_0,
                     "[ID-%"ID_UINT32_FMT"] [MSEC-%"ID_UINT64_FMT"] "
                     "Job Thread Wait Statistic",
                     mJobIdx, sTvWait.msec() );
    }
#endif
}

