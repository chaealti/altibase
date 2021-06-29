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
 * $Id:$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer 
 **********************************************************************/

#include <smDef.h>
#include <smErrorCode.h>
#include <sdb.h>
#include <sds.h>
#include <sdsReq.h>


sdbFlushJob   sdsFlushMgr::mReqJobQueue[SDB_FLUSH_JOB_MAX];
UInt          sdsFlushMgr::mReqJobAddPos;
UInt          sdsFlushMgr::mReqJobGetPos;
iduMutex      sdsFlushMgr::mReqJobMutex;
iduLatch      sdsFlushMgr::mFCLatch; // flusher control latch
sdsFlusher*   sdsFlushMgr::mFlushers;
UInt          sdsFlushMgr::mMaxFlusherCount;
UInt          sdsFlushMgr::mActiveCPFlusherCount;
sdbCPListSet* sdsFlushMgr::mCPListSet;
idvTime       sdsFlushMgr::mLastFlushedTime;
idBool        sdsFlushMgr::mServiceable;
idBool        sdsFlushMgr::mInitialized;

/***********************************************************************
 * Description :  flush manager�� �ʱ�ȭ�Ѵ�.
 *  aFlusherCount   - [IN]  ������ų �÷��� ����
 ***********************************************************************/
IDE_RC sdsFlushMgr::initialize( UInt aFlusherCount )
{
    UInt    i;
    SInt    sFlusherIdx = 0;
    UInt    sIOBPageCount = smuProperty::getBufferIOBufferSize();
    idBool  sNotStarted;
    SInt    sState = 0;

    /* identify ���Ŀ� flusher�� ������ �ǹǷ� serviceable�� ���� ���� */    
    IDE_TEST_CONT( sdsBufferMgr::isServiceable() != ID_TRUE, 
                    SKIP_SUCCESS );
    mInitialized = ID_TRUE;
    mServiceable = ID_TRUE;

    mActiveCPFlusherCount = 0;
    mReqJobAddPos         = 0;
    mReqJobGetPos         = 0;
    mMaxFlusherCount      = aFlusherCount;
    mCPListSet            = sdsBufferMgr::getCPListSet();

    // job queue�� �ʱ�ȭ�Ѵ�.
    for (i = 0; i < SDB_FLUSH_JOB_MAX; i++)
    {
        initJob( &mReqJobQueue[i] );
    }

    IDE_TEST( mReqJobMutex.initialize(
              (SChar*)"SECONDARY_BUFFER_FLUSH_MANAGER_REQJOB_MUTEX",
              IDU_MUTEX_KIND_NATIVE,
              IDB_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_FLUSH_MANAGER_REQJOB )
             != IDE_SUCCESS);
    sState = 1;

    IDE_ASSERT( mFCLatch.initialize( (SChar*)"SECONDARY_BUFFER_FLUSH_LATCH" ) 
                == IDE_SUCCESS);
    sState = 2;

    // ������ flush �ð��� �����Ѵ�.
    IDV_TIME_GET( &mLastFlushedTime );

    /* TC/FIT/Limit/sm/sdb/sdsFlushMgr_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsFlushMgr::initialize::malloc",
                          ERR_INSUFFICIENT_MEMORY );

    // flusher���� �����Ѵ�.
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(sdsFlusher) * mMaxFlusherCount,
                                       (void**)&mFlushers ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );

    sState = 3;

    for( sFlusherIdx = 0 ; sFlusherIdx < (SInt)mMaxFlusherCount ; sFlusherIdx++ )
    {
        new ( &mFlushers[sFlusherIdx]) sdsFlusher();

        // ����� ��� �÷����� ���� ũ���� ������ �������
        // ���� ũ���� IOB ũ�⸦ ������.
        IDE_TEST( mFlushers[sFlusherIdx].initialize( sFlusherIdx,    /* ID */ 
                                                     SD_PAGE_SIZE,   /* pageSize  */
                                                     sIOBPageCount ) /* PageCount */
                  != IDE_SUCCESS);

        mFlushers[sFlusherIdx].start();
    }

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSUFFICIENT_MEMORY );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION_END;

    sFlusherIdx--;
    for( ; sFlusherIdx >= 0 ; sFlusherIdx-- )
    {
        IDE_ASSERT( mFlushers[sFlusherIdx].finish( NULL, &sNotStarted ) 
                    == IDE_SUCCESS );
        IDE_ASSERT( mFlushers[sFlusherIdx].destroy() == IDE_SUCCESS );
    }

    switch (sState)
    {
        case 3:
            IDE_ASSERT( iduMemMgr::free( mFlushers ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mFCLatch.destroy( ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mReqJobMutex.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/*************************************************************************
 * Description : �������� ��� flusher���� �����Ű�� �ڿ� ������Ų��.
 *************************************************************************/
IDE_RC sdsFlushMgr::destroy()
{
    UInt   i;
    idBool sNotStarted;

    IDE_TEST_CONT( mInitialized != ID_TRUE, SKIP_SUCCESS );

    for ( i = 0; i < mMaxFlusherCount; i++ )
    {
        IDE_ASSERT( mFlushers[i].finish( NULL, &sNotStarted ) 
                   == IDE_SUCCESS);
        IDE_ASSERT( mFlushers[i].destroy() == IDE_SUCCESS);
    }

    IDE_ASSERT( iduMemMgr::free( mFlushers ) == IDE_SUCCESS );
    IDE_ASSERT( mFCLatch.destroy( ) == IDE_SUCCESS );
    IDE_ASSERT( mReqJobMutex.destroy() == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;
}

/*************************************************************************
 * Description : ����� flusher�� �ٽ� �籸����Ų��.
 *  aFlusherID  - [IN]  flusher id
 *************************************************************************/
IDE_RC sdsFlushMgr::turnOnFlusher( UInt aFlusherID )
{
    IDE_TEST_CONT( mServiceable != ID_TRUE, SKIP_SUCCESS );

    IDE_DASSERT( aFlusherID < mMaxFlusherCount );

    ideLog::log( IDE_SM_0, "[PREPARE] Start flusher ID(%d)\n", aFlusherID );

    IDE_TEST( mFCLatch.lockWrite( NULL,
                                  NULL )
              != IDE_SUCCESS);

    IDE_TEST_RAISE( mFlushers[aFlusherID].isRunning() == ID_TRUE,
                    ERR_FLUSHER_RUNNING );

    IDE_TEST( mFlushers[aFlusherID].start() != IDE_SUCCESS );

    IDE_TEST( mFCLatch.unlock() != IDE_SUCCESS );

    ideLog::log( IDE_SM_0, "[SUCCESS] Start flusher ID(%d)\n", aFlusherID );

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FLUSHER_RUNNING );
    {
        ideLog::log( IDE_SM_0,
                "[WARNING] flusher ID(%d) already started\n", aFlusherID );

        IDE_SET( ideSetErrorCode(smERR_ABORT_sdsFlusherRunning, aFlusherID) );
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT( mFCLatch.unlock() == IDE_SUCCESS );

    ideLog::log( IDE_SM_0, "[FAILURE] Start flusher ID(%d)\n", aFlusherID );

    return IDE_FAILURE;
}

/*************************************************************************
 * Description : flusher �����带 �����Ų��.
 *  aStatistics - [IN]  �������
 *  aFlusherID  - [IN]  flusher id
 *************************************************************************/
IDE_RC sdsFlushMgr::turnOffFlusher( idvSQL *aStatistics, UInt aFlusherID )
{
    idBool sNotStarted;
    idBool sLatchLocked = ID_FALSE;

    IDE_TEST_CONT( mServiceable != ID_TRUE, SKIP_SUCCESS );

    IDE_DASSERT( aFlusherID < mMaxFlusherCount );

    ideLog::log( IDE_SM_0, "[PREPARE] Stop flusher ID(%d)\n", aFlusherID );

    // BUG-26476
    // checkpoint�� �߻��ؼ� checkpoint flush job�� ����Ǳ⸦ 
    // ��� �ϰ� �ִ� �����尡 ���� �� �ִ�. �׷� ��� 
    // flusher�� ���� ����ϴ� �����尡 ������ ����� �� 
    // �ֱ� ������ �̶� ����ϴ� �����尡 �������� ������ �Ѵ�. 
    IDE_ASSERT( mFCLatch.lockWrite( NULL,
                                    NULL )
                == IDE_SUCCESS);
    sLatchLocked = ID_TRUE;

    IDE_TEST( mFlushers[aFlusherID].finish( aStatistics,
                                           &sNotStarted ) 
              != IDE_SUCCESS);

    sLatchLocked = ID_FALSE;
    IDE_ASSERT( mFCLatch.unlock() == IDE_SUCCESS );

    ideLog::log( IDE_SM_0, "[SUCCESS] Stop flusher ID(%d)\n", aFlusherID );

    IDE_TEST_RAISE( sNotStarted == ID_TRUE,
                    ERROR_FLUSHER_NOT_STARTED);

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_FLUSHER_NOT_STARTED );
    {
        ideLog::log( IDE_SM_0,
                "[WARNING] flusher ID(%d) already stopped.\n", aFlusherID );

        IDE_SET( ideSetErrorCode( smERR_ABORT_sdsFlusherNotStarted,
                                  aFlusherID) );
    }
    IDE_EXCEPTION_END;

    if( sLatchLocked == ID_TRUE )
    {
        sLatchLocked = ID_FALSE;
        IDE_ASSERT( mFCLatch.unlock() == IDE_SUCCESS );
    }

    ideLog::log( IDE_SM_0, "[FAILURE] Stop flusher ID(%d)\n", aFlusherID );

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    ����ȸ�� flusher�� �ٽ� �籸����Ų��.
 ******************************************************************************/
IDE_RC sdsFlushMgr::turnOnAllflushers()
{
    UInt i;

    IDE_TEST_CONT( mServiceable != ID_TRUE, SKIP_SUCCESS );

    IDE_ASSERT( mFCLatch.lockWrite( NULL, NULL ) 
                == IDE_SUCCESS); 

    for (i = 0; i < mMaxFlusherCount ; i++)
    {
        if( mFlushers[i].isRunning() != ID_TRUE )
        {
            IDE_TEST( mFlushers[i].start() != IDE_SUCCESS );
        }
    }

    IDE_ASSERT( mFCLatch.unlock( ) == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( mFCLatch.unlock(  ) == IDE_SUCCESS );

    return IDE_FAILURE;
}

/*************************************************************************
 * Description :
 *    ��� flusher���� �����. �̹� Ȱ������ flusher�� ���ؼ���
 *    �ƹ��� �۾��� ���� �ʴ´�.
 *************************************************************************/
IDE_RC sdsFlushMgr::wakeUpAllFlushers()
{
    UInt   i;
    idBool sDummy;

    for( i = 0; i < mMaxFlusherCount; i++ )
    {
        IDE_TEST( mFlushers[i].wakeUpSleeping( &sDummy ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    Flusher���� �۾����� �۾��� ����ɶ����� ����Ѵ�.
 ******************************************************************************/
IDE_RC sdsFlushMgr::wait4AllFlusher2Do1JobDone()
{
    UInt   i;

    for( i = 0; i < mMaxFlusherCount; i++ )
    {
        mFlushers[i].wait4OneJobDone();
    }

    return IDE_SUCCESS;
}

/************************************************************************
 * Description :
 *  aStatistics       - [IN]  �������
 *  aReqFlushCount    - [IN]  flush ���� �Ǳ� ���ϴ� �ּ� ������ ����
 *  aRedoPageCount    - [IN]  Restart Recovery�ÿ� Redo�� Page ��,
 *                            �ݴ�� ���ϸ� CP List�� ���ܵ� Dirty Page�� ��
 *  aRedoLogFileCount - [IN]  Restart Recovery�ÿ� Redo�� LogFile ��,
 *                            �ݴ�� BufferPool�� ���ܵ� LSN ���� (LogFile�� ����)
 *  aJobDoneParam     - [IN]  job �ϷḦ �뺸 �ޱ⸦ ���Ҷ�, �� �Ķ���͸� �����Ѵ�.
 *  aAdded            - [OUT] job ��� ���� ����. ���� SDB_FLUSH_JOB_MAX���� ����
 *                            job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 ************************************************************************/
void sdsFlushMgr::addReqReplaceFlushJob( 
                            idvSQL                     * aStatistics,
                            sdbFlushJobDoneNotifyParam * aJobDoneParam,
                            idBool                     * aAdded,
                            idBool                     * aSkip )
{
    sdbFlushJob * sCurr;
    UInt          sExtIndex;

    IDE_ASSERT( mReqJobMutex.lock( aStatistics ) == IDE_SUCCESS );

    *aAdded = ID_FALSE;
    *aSkip = ID_FALSE;

    if( isCond4ReplaceFlush() != ID_TRUE )
    {
        *aSkip = ID_TRUE;
        IDE_CONT( SKIP_ADDJOB );
    }

    sCurr = &mReqJobQueue[mReqJobAddPos];

    if( sCurr->mType == SDB_FLUSH_JOB_NOTHING )
    {
        if( sdsBufferMgr::getTargetFlushExtentIndex( &sExtIndex ) 
            == ID_TRUE )
        {
            if( aJobDoneParam != NULL )
            {
                initJobDoneNotifyParam( aJobDoneParam );
            }

            initJob( sCurr );

            sCurr->mType          = SDB_FLUSH_JOB_REPLACEMENT_FLUSH;
            sCurr->mReqFlushCount = 1;
            sCurr->mJobDoneParam  = aJobDoneParam;
            sCurr->mFlushJobParam.mSBufferReplaceFlush.mExtentIndex = sExtIndex;

            incPos( &mReqJobAddPos );
            *aAdded = ID_TRUE;
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

    IDE_EXCEPTION_CONT( SKIP_ADDJOB );

    IDE_ASSERT( mReqJobMutex.unlock() == IDE_SUCCESS );
}

/************************************************************************
 * Description :
 *  aStatistics       - [IN]  �������
 *  aReqFlushCount    - [IN]  flush ���� �Ǳ� ���ϴ� �ּ� ������ ����
 *  aRedoPageCount    - [IN]  Restart Recovery�ÿ� Redo�� Page ��,
 *                            �ݴ�� ���ϸ� CP List�� ���ܵ� Dirty Page�� ��
 *  aRedoLogFileCount - [IN]  Restart Recovery�ÿ� Redo�� LogFile ��,
 *                            �ݴ�� BufferPool�� ���ܵ� LSN ���� (LogFile�� ����)
 *  aJobDoneParam     - [IN]  job �ϷḦ �뺸 �ޱ⸦ ���Ҷ�, �� �Ķ���͸� �����Ѵ�.
 *  aAdded            - [OUT] job ��� ���� ����. ���� SDB_FLUSH_JOB_MAX���� ����
 *                            job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 ************************************************************************/
void sdsFlushMgr::addReqCPFlushJob( 
                            idvSQL                     * aStatistics,
                            ULong                        aMinFlushCount,
                            ULong                        aRedoPageCount,
                            UInt                         aRedoLogFileCount,
                            sdbFlushJobDoneNotifyParam * aJobDoneParam,
                            idBool                     * aAdded)
{
    sdbFlushJob *sCurr;

    IDE_ASSERT( mReqJobMutex.lock( aStatistics ) == IDE_SUCCESS );

    sCurr = &mReqJobQueue[mReqJobAddPos];

    if( sCurr->mType == SDB_FLUSH_JOB_NOTHING )
    {
        if( aJobDoneParam != NULL )
        {
            initJobDoneNotifyParam( aJobDoneParam );
        }

        initJob( sCurr );

        sCurr->mType          = SDB_FLUSH_JOB_CHECKPOINT_FLUSH;
        sCurr->mReqFlushCount = aMinFlushCount;
        sCurr->mJobDoneParam  = aJobDoneParam;
        sCurr->mFlushJobParam.mChkptFlush.mRedoPageCount    = aRedoPageCount;
        sCurr->mFlushJobParam.mChkptFlush.mRedoLogFileCount = aRedoLogFileCount;

        incPos( &mReqJobAddPos );
        *aAdded = ID_TRUE;
    }
    else
    {
        *aAdded = ID_FALSE;
    }

    IDE_ASSERT( mReqJobMutex.unlock() == IDE_SUCCESS );
}

/*************************************************************************
 * Description :
 *  aStatistics - [IN]  �������
 *  aBCBQueue   - [IN]  flush�� ������ BCB�����Ͱ� ����ִ� ť
 *  aFiltFunc   - [IN]  aBCBQueue�� �ִ� BCB�� aFiltFunc���ǿ� �´� BCB�� flush
 *  aFiltObj    - [IN]  aFiltFunc�� �Ķ���ͷ� �־��ִ� ��
 *  aJobDoneParam-[IN]  job �ϷḦ �뺸 �ޱ⸦ ���Ҷ�, �� �Ķ���͸� �����Ѵ�.
 *  aAdded      - [OUT] job ��� ���� ����. ���� SDB_FLUSH_JOB_MAX���� ����
 *                      job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 ************************************************************************/
void sdsFlushMgr::addReqObjectFlushJob(
                                    idvSQL                     * aStatistics,
                                    smuQueueMgr                * aBCBQueue,
                                    sdbFiltFunc                  aFiltFunc,
                                    void                       * aFiltObj,
                                    sdbFlushJobDoneNotifyParam * aJobDoneParam,
                                    idBool                     * aAdded )
{
    sdbFlushJob *sCurr;

    IDE_ASSERT( mReqJobMutex.lock( aStatistics ) == IDE_SUCCESS );

    sCurr = &mReqJobQueue[mReqJobAddPos];

    if( sCurr->mType == SDB_FLUSH_JOB_NOTHING )
    {
        if( aJobDoneParam != NULL )
        {
            initJobDoneNotifyParam( aJobDoneParam );
        }

        initJob( sCurr );
        sCurr->mType         = SDB_FLUSH_JOB_DBOBJECT_FLUSH;
        sCurr->mJobDoneParam = aJobDoneParam;
        sCurr->mFlushJobParam.mObjectFlush.mBCBQueue = aBCBQueue;
        sCurr->mFlushJobParam.mObjectFlush.mFiltFunc = aFiltFunc;
        sCurr->mFlushJobParam.mObjectFlush.mFiltObj  = aFiltObj;

        incPos( &mReqJobAddPos );
        *aAdded = ID_TRUE;
    }
    else
    {
        *aAdded = ID_FALSE;
    }

    IDE_ASSERT( mReqJobMutex.unlock() == IDE_SUCCESS );
}

/**************************************************************************
 * Description : CPListSet�� dirty ���۵��� flush�Ѵ�.
 *  aStatistics - [IN]  �������
 *  aFlushAll   - [IN]  check point list���� ��� BCB�� flush �Ϸ��� �� ������
 *                      ID_TRUE�� �����Ѵ�.
 *************************************************************************/
IDE_RC sdsFlushMgr::flushDirtyPagesInCPList( idvSQL *aStatistics,
                                             idBool  aFlushAll )
{
    idBool                          sAdded = ID_FALSE;
    sdbFlushJobDoneNotifyParam      sJobDoneParam;
    PDL_Time_Value                  sTV;
    ULong                           sMinFlushCount;
    ULong                           sRedoPageCount;
    UInt                            sRedoLogFileCount;
    idBool                          sLatchLocked = ID_FALSE;

    // BUG-26476
    // checkpoint ����� flusher���� stop�Ǿ� ������ skip�� �ؾ� �Ѵ�.
    // ������ checkpoint�� ���� checkpoint flush�� ����ǰ� �ִ� ����
    // flush�� stop�Ǹ� �ȵȴ�.
    IDE_ASSERT( mFCLatch.lockRead( NULL,        /* idvSQL */
                                   NULL)        /* Wait Event */
                == IDE_SUCCESS );

    sLatchLocked = ID_TRUE;

    if( getActiveFlusherCount() == 0 )
    {
        IDE_CONT( SKIP_FLUSH );
    }

    if( aFlushAll == ID_FALSE )
    {
         sMinFlushCount    = smuProperty::getCheckpointFlushCount();
         sRedoPageCount    = smuProperty::getFastStartIoTarget();
         sRedoLogFileCount = smuProperty::getFastStartLogFileTarget();
    }
    else
    {
        sMinFlushCount    = mCPListSet->getTotalBCBCnt();
        sRedoPageCount    = 0; // DirtyPage�� BufferPool�� ���ܵ��� �ʴ´�.
        sRedoLogFileCount = 0;
    }

    while( sAdded == ID_FALSE )
    {
        addReqCPFlushJob( aStatistics,
                          sMinFlushCount,
                          sRedoPageCount,
                          sRedoLogFileCount,
                          &sJobDoneParam,
                          &sAdded );
        IDE_TEST( wakeUpAllFlushers() != IDE_SUCCESS );
        sTV.set( 0, 50000 );
        idlOS::sleep(sTV);
    }

    IDE_TEST( waitForJobDone( &sJobDoneParam ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_FLUSH );

    sLatchLocked = ID_FALSE;

    IDE_ASSERT( mFCLatch.unlock( ) == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sLatchLocked == ID_TRUE )
    {
       IDE_ASSERT( mFCLatch.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*************************************************************************
 * BCB �����Ͱ� ����ִ� queue�� �� BCB���� flush �ؾ� ���� ����
 * ������ �� �ִ� aFiltFunc �� aFiltObj�� ��������
 * �÷��ø� �����Ѵ�.
 *************************************************************************/
IDE_RC sdsFlushMgr::flushPagesInExtent( idvSQL  * aStatistics )
{
    sdbFlushJobDoneNotifyParam sJobDoneParam;
    PDL_Time_Value   sTV;

    idBool sAdded = ID_FALSE;
    idBool sSkip  = ID_FALSE;

    while( sAdded == ID_FALSE )
    {
        addReqReplaceFlushJob( aStatistics,
                               &sJobDoneParam,
                               &sAdded,
                               &sSkip );

        IDE_TEST_CONT( sSkip == ID_TRUE, SKIP_FLUSH )

        IDE_TEST( wakeUpAllFlushers() != IDE_SUCCESS );

        sTV.set( 0, 50000 );
        idlOS::sleep(sTV);
    }

    IDE_TEST( waitForJobDone( &sJobDoneParam ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_FLUSH );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * BCB �����Ͱ� ����ִ� queue�� �� BCB���� flush �ؾ� ���� ����
 * ������ �� �ִ� aFiltFunc �� aFiltObj�� ��������
 * �÷��ø� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCBQueue   - [IN]  flush�� ����� ��� �ִ� BCB�� �����Ͱ� �ִ� queue
 *  aFiltFunc   - [IN]  aBCBQueue�� �ִ� BCB�� aFiltFunc���ǿ� �´� BCB�� flush
 *  aFiltObj    - [IN]  aFiltFunc�� �Ķ���ͷ� �־��ִ� ��
 *************************************************************************/
IDE_RC sdsFlushMgr::flushObjectDirtyPages( idvSQL       *aStatistics,
                                           smuQueueMgr  *aBCBQueue,
                                           sdsFiltFunc   aFiltFunc,
                                           void         *aFiltObj )
{
    idBool                     sAdded = ID_FALSE;
    sdbFlushJobDoneNotifyParam sJobDoneParam;
    PDL_Time_Value             sTV;

    while( sAdded == ID_FALSE )
    {
        addReqObjectFlushJob( aStatistics,
                              aBCBQueue,
                              aFiltFunc,
                              aFiltObj,
                              &sJobDoneParam,
                              &sAdded );

        IDE_TEST( wakeUpAllFlushers() != IDE_SUCCESS );

        sTV.set( 0, 50000 );
        idlOS::sleep(sTV);
    }

    IDE_TEST( waitForJobDone(&sJobDoneParam) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : flusher�� ���� ȣ��Ǹ� flusher�� ������ ��ȯ�Ѵ�.
 *    flusher�� ������ ������ ������ ��ȣ�� �켱�����̴�.
 *      1. requested replacement flush job
 *           : Ʈ����� �����尡 victim ã�⸦ �������� ��,
 *             �ش� flush list�� flush�� ���� �۾���û�Ѵ�. �̶�
 *             ��ϵǴ� �۾��̸� addReqFlushJob()�� ���ؼ� ��ϵȴ�.
 *         requested checkpoint flush job
 *           : ���� requested replacement flush job�� ������ �켱������ ������
 *             ���� ��ϵ� ������ ó���ȴ�.
 *             �� job�� checkpoint �����尡 checkpoint ������ �ʹ� �ʾ����ٰ�
 *             �Ǵ����� ��� ��ϵǰų� ������� ����� checkpoint ��ɽ�
 *             ��ϵȴ�. addReqCPFlushJob()�� ���� ��ϵȴ�.
 *         requested  object flushjob
 *           : ���� 2����  flush job�� ������ �켱 ������ ������ 
 *             ���� ��ϵ� ������ ó�� �ȴ�. 
 *             �� job�� pageout/flush page ������ ������� ������� ��û����
 *             ��ϵȴ�. addReqObjectFlushJob()�� ���� ��ϵȴ�. 
 *      2. main replacement flush job
 *           : flusher���� �ֱ������� ���鼭 flush list�� ���̰�
 *             ���� ��ġ�� �Ѿ�� flush�ϰ� �Ǵµ� �� �۾��� ���Ѵ�.
 *             �� �۾��� Ʈ����� �����忡 ���� ��ϵǴ°� �ƴ϶�
 *             flusher�鿡 ���� ��ϵǰ� ó���ȴ�.
 *      3. system checkpoint flush job
 *           : 1, 2�� �۾��� ���� ��� system checkpoint flush�� �����Ѵ�.
 
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ������ job
 ************************************************************************/
void sdsFlushMgr::getJob( idvSQL      * aStatistics,
                          sdbFlushJob * aRetJob )
{
    initJob( aRetJob );

    getReqJob( aStatistics, aRetJob );

    if( aRetJob->mType == SDB_FLUSH_JOB_NOTHING )
    {
        getReplaceFlushJob( aRetJob );

        if( aRetJob->mType == SDB_FLUSH_JOB_NOTHING )
        {
            // checkpoint flush job�� ���Ѵ�.
            getCPFlushJob( aStatistics, aRetJob );
        }
        else
        {
            // replace flush job�� �ϳ� �����.
        }
    }
    else
    {
        // req job�� �ϳ� �����.
    }
}

/*************************************************************************
 * Description : job queue�� ��ϵ� job�� �ϳ� ��´�.
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ������ Job
 *************************************************************************/
void sdsFlushMgr::getReqJob( idvSQL      *aStatistics,
                             sdbFlushJob *aRetJob )
{
    sdbFlushJob *sJob = &mReqJobQueue[mReqJobGetPos];

    if( sJob->mType == SDB_FLUSH_JOB_NOTHING )
    {
        aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
    }
    else
    {
        IDE_ASSERT( mReqJobMutex.lock( aStatistics ) == IDE_SUCCESS );

        // lock�� ���� �ڿ� �ٽ� �޾ƿ;� �Ѵ�.
        sJob = &mReqJobQueue[mReqJobGetPos];

        if( sJob->mType == SDB_FLUSH_JOB_NOTHING )
        {
            aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
        }
        else
        {
            // req job�� �ִ�.
            *aRetJob    = *sJob;
            sJob->mType = SDB_FLUSH_JOB_NOTHING;

            // get position�� �ϳ� �����Ų��.
            incPos( &mReqJobGetPos );

            /* CheckpointFlusher �Ѱ迡 �����ߴ��� �˻� */
            limitCPFlusherCount( aStatistics,
                                 aRetJob,
                                 ID_TRUE ); /* already locked */
        }

        IDE_ASSERT( mReqJobMutex.unlock() == IDE_SUCCESS );
    }
}

/*************************************************************************
 * Description : main job�� �ϳ� ��´�. 
 *  aRetJob   - [OUT] ���ϵǴ� job
 *************************************************************************/
void sdsFlushMgr::getReplaceFlushJob( sdbFlushJob *aRetJob )
{
    UInt sExtIndex = 0;
    initJob( aRetJob );
    
    if( isCond4ReplaceFlush() == ID_TRUE )
    {   
        if( sdsBufferMgr::getTargetFlushExtentIndex( &sExtIndex ) 
            == ID_TRUE )
        {
            aRetJob->mType = SDB_FLUSH_JOB_REPLACEMENT_FLUSH;
            /* �� Extent�� flush �Ѵ� */
            aRetJob->mReqFlushCount = 1; 
            aRetJob->mFlushJobParam.mSBufferReplaceFlush.mExtentIndex = sExtIndex;
        }
        else
        {
            /* nothing to do */
        }
    }
    return;
}

/*************************************************************************
 * Description :
 *   incremental checkpoint�� �����ϴ� job�� ��´�.
 *   �� �Լ��� �����Ѵٰ� �ؼ� �׳� job�� ������ �ʴ´�. ��, � ������ ����
 *   �ؾ� �ϴµ�, �������� �Ʒ� ���� �ϳ��� ���� �� ���̴�.
 *   1. ������ checkpoint ���� �� �ð��� ���� ������
 *   2. Recovery LSN�� �ʹ� ������
 *
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ���ϵǴ� job
 *************************************************************************/
void sdsFlushMgr::getCPFlushJob( idvSQL *aStatistics, sdbFlushJob *aRetJob )
{
    smLSN          sMinLSN;
    idvTime        sCurrentTime;
    ULong          sTime;

    mCPListSet->getMinRecoveryLSN( aStatistics, &sMinLSN );

    IDV_TIME_GET( &sCurrentTime );
    sTime = IDV_TIME_DIFF_MICRO( &mLastFlushedTime, &sCurrentTime ) / 1000000;

    if ( ( smLayerCallback::isCheckpointFlushNeeded( sMinLSN ) == ID_TRUE ) ||
         ( sTime >= smuProperty::getCheckpointFlushMaxWaitSec() ) )
    {
        initJob( aRetJob );
        aRetJob->mType          = SDB_FLUSH_JOB_CHECKPOINT_FLUSH;
        aRetJob->mReqFlushCount = smuProperty::getCheckpointFlushCount();
        aRetJob->mFlushJobParam.mChkptFlush.mRedoPageCount
                                = smuProperty::getFastStartIoTarget();
        aRetJob->mFlushJobParam.mChkptFlush.mRedoLogFileCount
                                = smuProperty::getFastStartLogFileTarget();

        /* CheckpointFlusher �Ѱ迡 �����ߴ��� �˻� */
        limitCPFlusherCount( aStatistics,
                             aRetJob,
                             ID_FALSE ); /* already locked */
    }
    else
    {
        aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
    }
}

/******************************************************************************
 * Description :
 *  ����ڰ� ��û�� flush�۾� ��û�� �������� Ȯ���ϴ� ������
 *  sdbFlushJobDoneNotifyParam �� �ʱ�ȭ �Ѵ�.
 *
 *  aParam  - [IN]  �ʱ�ȭ�� ����
 ******************************************************************************/
IDE_RC sdsFlushMgr::initJobDoneNotifyParam( sdbFlushJobDoneNotifyParam *aParam )
{
    IDE_TEST( aParam->mMutex.initialize( (SChar*)"SECONDARY_BUFFER_FLUSH_JOB_COND_WAIT_MUTEX",
                                         IDU_MUTEX_KIND_POSIX,
                                         IDV_WAIT_INDEX_LATCH_FREE_OTHERS )
             != IDE_SUCCESS );

    IDE_TEST_RAISE( aParam->mCondVar.initialize((SChar*)"SECONDARY_BUFFER_FLUSH_JOB_COND_WAIT_COND") != IDE_SUCCESS, 
                    ERR_COND_INIT );

    aParam->mJobDone = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_INIT );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondInit ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * Description :
 *  �ʱ�ȭ�� sdbFlushJobDoneNotifyParam�� ��������
 *  ��û�� �۾��� �����ϱ⸦ ����Ѵ�.
 *
 *  aParam  - [IN]  ��⸦ ���� �ʿ��� ����
 *************************************************************************/
IDE_RC sdsFlushMgr::waitForJobDone( sdbFlushJobDoneNotifyParam *aParam )
{
    IDE_ASSERT(aParam->mMutex.lock(NULL) == IDE_SUCCESS);

    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while ( aParam->mJobDone == ID_FALSE )
    {
        // ����� job�� ó�� �Ϸ�ɶ����� ����Ѵ�.
        // cond_wait�� �����԰� ���ÿ� mMutex�� Ǭ��.
        IDE_TEST_RAISE( aParam->mCondVar.wait(&aParam->mMutex)
                       != IDE_SUCCESS, ERR_COND_WAIT );
    }

    IDE_ASSERT( aParam->mMutex.unlock() == IDE_SUCCESS );

    IDE_TEST_RAISE( aParam->mCondVar.destroy() != IDE_SUCCESS,
                    ERR_COND_DESTROY );

    IDE_TEST( aParam->mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_WAIT );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondWait) );
    }
    IDE_EXCEPTION( ERR_COND_DESTROY );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondDestroy) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 *    job�� ó�������� �˸���. ���� jobDone�� �������� �ʾ�����
 *    �� �Լ��ȿ��� �ƹ��� �۾��� ���� �ʴ´�.
 *    jobDone�� job�� ����� �� �ɼ����� �� �� �ִ�.
 *
 * aParam   - [IN]  �� ������ ����ϰ� �ִ� ������鿡�� job�� ���� �Ǿ�����
 *                  �˸���.
 ***************************************************************************/
void sdsFlushMgr::notifyJobDone(sdbFlushJobDoneNotifyParam *aParam)
{
    if (aParam != NULL)
    {
        IDE_ASSERT(aParam->mMutex.lock(NULL) == IDE_SUCCESS);

        IDE_ASSERT(aParam->mCondVar.signal() == IDE_SUCCESS);

        aParam->mJobDone = ID_TRUE;

        IDE_ASSERT(aParam->mMutex.unlock() == IDE_SUCCESS);
    }
}

/******************************************************************************
 * Description : Replacement Flush �� ����
 * �ش� ����� ������� �ƴϸ� ����� ���°� dirty �� flush ������� ��´�.
 ******************************************************************************/
idBool sdsFlushMgr::isCond4ReplaceFlush()
{   
    idBool sRet = ID_FALSE;

    if ( sdsBufferMgr::hasExtentMovedownDone() == ID_TRUE )
    {
        sRet = ID_TRUE;
    }

    return sRet;
}

/******************************************************************************
 * Description :
 *    flusher�� job�� ó���ϰ� ���� job�� ó���� ������ ����� �ð���
 *    ��ȯ�Ѵ�. �ý����� ��Ȳ�� ���� �������̴�. flush�Ұ� ������
 *    ��� �ð��� ª�� �Ұ� ���� ������ ��� �ð��� �������.
 ******************************************************************************/
idBool sdsFlushMgr::isBusyCondition()
{
    idBool sRet = ID_FALSE;

    if( mReqJobQueue[mReqJobGetPos].mType != SDB_FLUSH_JOB_NOTHING )
    {
        sRet = ID_TRUE;
    }

    if( isCond4ReplaceFlush() == ID_TRUE )
    {
        sRet = ID_TRUE;
    }

    return sRet;
}

/*************************************************************************
 * Description : CP Flusher������ max ������ ũ�� �ʵ��� �����Ѵ�.
 *************************************************************************/
void sdsFlushMgr::limitCPFlusherCount( idvSQL      * aStatistics, 
                                       sdbFlushJob * aRetJob,
                                       idBool        aAlreadyLocked )
{
    if( ( smuProperty::getMaxSBufferCPFlusherCnt() == 0 ) ||
        ( aRetJob->mType != SDB_FLUSH_JOB_CHECKPOINT_FLUSH ) )
    {
        /* Property�� 0�̰ų�, CheckpointFlush�� �ƴϸ� �����Ѵ�. */
    }
    else
    {
        if( aAlreadyLocked == ID_FALSE )
        {
            IDE_ASSERT( mReqJobMutex.lock( aStatistics ) == IDE_SUCCESS );
        }

        /* BUG-32664 */
        if( mActiveCPFlusherCount >= smuProperty::getMaxSBufferCPFlusherCnt() )
        {
            aRetJob->mReqFlushCount = 0;
            aRetJob->mFlushJobParam.mChkptFlush.mRedoPageCount    
                                    = ID_ULONG_MAX;
            aRetJob->mFlushJobParam.mChkptFlush.mRedoLogFileCount 
                                    = ID_UINT_MAX;
        }
        else
        {
            mActiveCPFlusherCount ++;
        }

        if( aAlreadyLocked == ID_FALSE )
        {
            IDE_ASSERT( mReqJobMutex.unlock() == IDE_SUCCESS );
        }
    }
    return;
}

/*************************************************************************
 * Description : flusher���� ���� ���� recoveryLSN�� �����Ѵ�.
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] flusher���� ���� ���� recoveryLSN�� �����Ѵ�.
 *************************************************************************/
void sdsFlushMgr::getMinRecoveryLSN( idvSQL *aStatistics,
                                     smLSN  *aRet )
{
    UInt   i;
    smLSN  sFlusherMinLSN;

    SM_LSN_MAX( *aRet );
    for (i = 0; i < mMaxFlusherCount; i++)
    {
        mFlushers[i].getMinRecoveryLSN( aStatistics, &sFlusherMinLSN );

        if ( smLayerCallback::isLSNLT( &sFlusherMinLSN, aRet ) == ID_TRUE )
        {
            SM_GET_LSN( *aRet, sFlusherMinLSN );
        }
    }
}

/******************************************************************************
 * Description : �����ϰ� �ִ� flusher�� ���� ��ȯ�Ѵ�
 ******************************************************************************/
UInt sdsFlushMgr::getActiveFlusherCount()
{
    UInt i;
    UInt sActiveCount = 0;

    for( i = 0 ; i < mMaxFlusherCount ; i++ )
    {
        if( mFlushers[ i ].isRunning() == ID_TRUE )
        {
            sActiveCount++;
        }
    }
    return sActiveCount;
}

