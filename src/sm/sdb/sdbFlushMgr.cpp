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
 * $$Id:$
 **********************************************************************/
/******************************************************************************
 * Description :
 *    sdbFlushMgr�� static class�μ� flusher���� ����, �����ϸ�
 *    flush ��, waiting time, flush job scheduling ���� ����Ѵ�.
 *    flush manager�� �ý��ۿ��� �ϳ��� �����ϸ�, ��� flusher����
 *    �������� �Ҹ�, ������ ����, ����� ���� ����� �����Ѵ�.
 *    �ý����� ��Ȳ������ flush���� �����ϸ� flusher���� waiting time��
 *    �������� �����Ѵ�.
 *    ���� Ʈ����� ��������� victim�� ã�ٰ� list warp�� �ϱ�����
 *    flush job�� ��Ͻ����ֵ����ϴ� �������̽��� �����Ѵ�.
 *    ��ϵ� replacement flush job�� ������ flush manager�� flush list��
 *    ���̸� ���� replacement flush�� �� ������, checkpoint flush�� �� ������
 *    ������ �� flusher���� flush �۾��� �Ҵ��Ѵ�.
 *
 *    �� ó�� �ý����� �ֱ������� �����ϴ� üũ����Ʈ�� system checkpoint��� �ϰ�
 *    ������� �Է¿� ���� üũ����Ʈ�� user checkpoint��� �Ѵ�.
 *
 *    flush job�� �켱 ������ ������ ����.
 *      1. Ʈ����� �����忡 ���� ��ϵ� replacement flush job �Ǵ�
 *         checkpoint �����忡 ���� ��ϵ� full checkpoint flush job
 *      2. flush list�� ���� ���� �̻��� �Ǿ��� �� �ֱ������� ó���Ǵ�
 *         replacement flush job
 *      3. checkpoint flush job
 *
 * Implementation :
 *    flush manager�� flusher�鿡�� sdbFlushJob�̶�� ����ü�� �Ű���
 *    flush job�� �Ҵ��Ѵ�.
 *    flush manager�� ���� 1�� job���� ��û�ޱ� ���� ���������� job queue��
 *    �����Ѵ�. �̶� ���Ǵ� �ɹ��� mReqJobQueue�̴�.
 *    job queue�� array�� �����Ǹ� mReqJobAddPos ��ġ�� �ְ�,
 *    mReqJobGetPos���� job�� ��´�. job queue�� mReqJobMutex�� ����
 *    ���ü��� ����ȴ�.
 *    SDB_FLUSH_JOB_MAX���� ���� job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 ******************************************************************************/
#include <sdbFlushMgr.h>
#include <smErrorCode.h>
#include <sdbReq.h>
#include <sdbBufferMgr.h>
#include <smrRecoveryMgr.h>

iduMutex       sdbFlushMgr::mReqJobMutex;
sdbFlushJob    sdbFlushMgr::mReqJobQueue[SDB_FLUSH_JOB_MAX];
UInt           sdbFlushMgr::mReqJobAddPos;
UInt           sdbFlushMgr::mReqJobGetPos;
iduLatch       sdbFlushMgr::mFCLatch;
sdbFlusher*    sdbFlushMgr::mFlushers;
UInt           sdbFlushMgr::mFlusherCount;
idvTime        sdbFlushMgr::mLastFlushedTime;
sdbCPListSet  *sdbFlushMgr::mCPListSet;

/******************************************************************************
 * Description :
 *    flush manager�� �ʱ�ȭ�Ѵ�.
 *    flusher�� ������ ���ڷ� �޴´�.
 *    �ʱ�ȭ �����߿� flusher ��ü���� �����Ѵ�.
 *
 *  aFlusherCount   - [IN]  ������ų �÷��� ����
 ******************************************************************************/
IDE_RC sdbFlushMgr::initialize(UInt aFlusherCount)
{
    UInt   i = 0;
    UInt   sFlusherIdx = 0;
    idBool sNotStarted; 
    SInt   sState = 0; 

    mReqJobAddPos   = 0;
    mReqJobGetPos   = 0;
    mFlusherCount   = aFlusherCount;
    mCPListSet      = sdbBufferMgr::getPool()->getCPListSet();

    // job queue�� �ʱ�ȭ�Ѵ�.
    for (i = 0; i < SDB_FLUSH_JOB_MAX; i++)
    {
        initJob(&mReqJobQueue[i]);
    }

    IDE_TEST(mReqJobMutex.initialize(
                 (SChar*)"BUFFER_FLUSH_MANAGER_REQJOB_MUTEX",
                 IDU_MUTEX_KIND_NATIVE,
                 IDB_WAIT_INDEX_LATCH_FREE_DRDB_BUFFER_FLUSH_MANAGER_REQJOB)
             != IDE_SUCCESS);
    sState = 1;

    IDE_ASSERT( mFCLatch.initialize( (SChar*)"BUFFER_FLUSH_CONTROL_LATCH") 
               == IDE_SUCCESS); 
    sState = 2; 

    // ������ flush �ð��� �����Ѵ�.
    IDV_TIME_GET(&mLastFlushedTime);

    /* TC/FIT/Limit/sm/sdb/sdbFlushMgr_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbFlushMgr::initialize::malloc",
                          insufficient_memory );

    // flusher���� �����Ѵ�.
    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                      (ULong)ID_SIZEOF(sdbFlusher) * aFlusherCount,
                                      (void**)&mFlushers) != IDE_SUCCESS,
                    insufficient_memory );
                    
    sState = 3;

    for( sFlusherIdx = 0; sFlusherIdx < aFlusherCount; sFlusherIdx++ )
    {
        new (&mFlushers[sFlusherIdx]) sdbFlusher();

        // ����� ��� �÷����� ���� ũ���� ������ �������
        // ���� ũ���� IOB ũ�⸦ ������.
        IDE_TEST(mFlushers[sFlusherIdx].initialize(
                    sFlusherIdx,
                    SD_PAGE_SIZE,
                    smuProperty::getBufferIOBufferSize(),
                    mCPListSet)
                != IDE_SUCCESS);

        mFlushers[sFlusherIdx].setDelayedFlushProperty( smuProperty::getDelayedFlushListPct(),
                                                        smuProperty::getDelayedFlushProtectionTimeMsec() );

        mFlushers[sFlusherIdx].start();
    }

    sState = 4;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch (sState) 
    { 
        case 4: 
        case 3: 
            for ( ; sFlusherIdx > 0; sFlusherIdx-- ) 
            { 
                IDE_ASSERT(mFlushers[sFlusherIdx-1].finish(NULL, &sNotStarted) 
                        == IDE_SUCCESS); 
                IDE_ASSERT(mFlushers[sFlusherIdx-1].destroy() == IDE_SUCCESS); 
            } 

            IDE_ASSERT(iduMemMgr::free(mFlushers) == IDE_SUCCESS); 
        case 2: 
            IDE_ASSERT(mFCLatch.destroy() == IDE_SUCCESS); 
        case 1: 
            IDE_ASSERT(mReqJobMutex.destroy() == IDE_SUCCESS); 
        case 0: 
        default: 
            break; 
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    ��� flusher���� ������Ų��.
 ******************************************************************************/
void sdbFlushMgr::startUpFlushers()
{
    UInt i;

    for (i = 0; i < mFlusherCount; i++)
    {
        mFlushers[i].start();
    }
}

/******************************************************************************
 * Description :
 *    �������� ��� flusher���� �����Ű�� �ڿ� ������Ų��.
 *
 ******************************************************************************/
IDE_RC sdbFlushMgr::destroy()
{
    UInt   i;
    idBool sNotStarted;

    for (i = 0; i < mFlusherCount; i++)
    {
        IDE_TEST(mFlushers[i].finish(NULL, &sNotStarted) != IDE_SUCCESS);
        IDE_TEST(mFlushers[i].destroy() != IDE_SUCCESS);
    }

    IDE_TEST(iduMemMgr::free(mFlushers) != IDE_SUCCESS);

    IDE_ASSERT(mFCLatch.destroy() == IDE_SUCCESS);

    IDE_ASSERT(mReqJobMutex.destroy() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    ��� flusher���� �����. �̹� Ȱ������ flusher�� ���ؼ���
 *    �ƹ��� �۾��� ���� �ʴ´�.
 *
 ******************************************************************************/
IDE_RC sdbFlushMgr::wakeUpAllFlushers()
{
    UInt   i;
    idBool sDummy;

    for (i = 0; i < mFlusherCount; i++)
    {
        IDE_TEST(mFlushers[i].wakeUpOnlyIfSleeping(&sDummy) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    Flusher���� �۾����� �۾��� ����ɶ����� ����Ѵ�. ���� �ִ� Flusher����
 *    �����Ѵ�.
 *
 ******************************************************************************/
IDE_RC sdbFlushMgr::wait4AllFlusher2Do1JobDone()
{
    UInt   i;

    for (i = 0; i < mFlusherCount; i++)
    {
        mFlushers[i].wait4OneJobDone();
    }

    return IDE_SUCCESS;
}

/******************************************************************************
 * Description :
 *  flusher���� ���� ���� recoveryLSN�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] flusher���� ���� ���� recoveryLSN�� �����Ѵ�.
 ******************************************************************************/
void sdbFlushMgr::getMinRecoveryLSN(idvSQL *aStatistics,
                                    smLSN  *aRet)
{
    UInt   i;
    smLSN  sFlusherMinLSN;

    SM_LSN_MAX(*aRet);
    for (i = 0; i < mFlusherCount; i++)
    {
        mFlushers[i].getMinRecoveryLSN(aStatistics, &sFlusherMinLSN);
        if ( smLayerCallback::isLSNLT( &sFlusherMinLSN, aRet ) == ID_TRUE )
        {
            SM_GET_LSN(*aRet, sFlusherMinLSN);
        }
    }
}

/******************************************************************************
 * Description :
 *    job queue�� ��ϵ� job�� �ϳ� ��´�.
 *
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ������ Job
 ******************************************************************************/
void sdbFlushMgr::getReqJob(idvSQL      *aStatistics,
                            sdbFlushJob *aRetJob)
{
    sdbFlushJob *sJob = &mReqJobQueue[mReqJobGetPos];

    if (sJob->mType == SDB_FLUSH_JOB_NOTHING)
    {
        aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
    }
    else
    {
        IDE_ASSERT(mReqJobMutex.lock(aStatistics) == IDE_SUCCESS);

        // lock�� ���� �ڿ� �ٽ� �޾ƿ;� �Ѵ�.
        sJob = &mReqJobQueue[mReqJobGetPos];

        if (sJob->mType == SDB_FLUSH_JOB_NOTHING)
        {
            aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
        }
        else
        {
            // req job�� �ִ�.
            *aRetJob    = *sJob;
            sJob->mType = SDB_FLUSH_JOB_NOTHING;

            // get position�� �ϳ� �����Ų��.
            incPos(&mReqJobGetPos);
        }

        IDE_ASSERT(mReqJobMutex.unlock() == IDE_SUCCESS);
    }
}

/*****************************************************************************
 * Description :
 *    flusher�� ���� ȣ��Ǹ� flusher�� ������ ��ȯ�Ѵ�.
 *    flusher�� ������ ������ ������ ��ȣ�� �켱�����̴�.
 *    1.    requested  object flushjob
 *             �� job�� pageout/flush page ������ ������� ������� ��û����
 *             ��ϵȴ�. addReqObjectFlushJob()�� ���� ��ϵȴ�. 
 *         requested checkpoint flush job
 *           : ���� requested object flush job�� ������ �켱������ ������
 *             ���� ��ϵ� ������ ó���ȴ�.
 *             �� job�� checkpoint �����尡 checkpoint ������ �ʹ� �ʾ����ٰ�
 *             �Ǵ����� ��� ��ϵǰų� ������� ����� checkpoint ��ɽ�
 *             ��ϵȴ�. addReqCPFlushJob()�� ���� ��ϵȴ�.
 *      2. main replacement flush job
 *           : flusher���� �ֱ������� ���鼭 flush list�� ���̰�
 *             ���� ��ġ�� �Ѿ�� flush�ϰ� �Ǵµ� �� �۾��� ���Ѵ�.
 *             �� �۾��� Ʈ����� �����忡 ���� ��ϵǴ°� �ƴ϶�
 *             flusher�鿡 ���� ��ϵǰ� ó���ȴ�.
 *      3. system checkpoint flush job
 *           : 1, 2�� �۾��� ���� ��� system checkpoint flush�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ������ job
 *****************************************************************************/
void sdbFlushMgr::getJob(idvSQL      *aStatistics,
                         sdbFlushJob *aRetJob)
{
    getReqJob(aStatistics, aRetJob);

    if (aRetJob->mType == SDB_FLUSH_JOB_NOTHING)
    {
        getReplaceFlushJob(aRetJob);

        if (aRetJob->mType == SDB_FLUSH_JOB_NOTHING)
        {
            // checkpoint flush job�� ���Ѵ�.
            getCPFlushJob(aStatistics, aRetJob);
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

/******************************************************************************
 * Description :
 *    PROJ-2669
 *      Delayed Flush List �� BCB�� ���� ��� Job�� ��ȯ�Ѵ�.
 *      Flush ��� Flush list �� Delayed Flush list �� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ���ϵǴ� job
 ******************************************************************************/
void sdbFlushMgr::getDelayedFlushJob( sdbFlushJob *aColdJob,
                                      sdbFlushJob *aRetJob)
{
    sdbFlushList  *sNormalFlushList;
    sdbFlushList  *sDelayedFlushList;
    sdbBufferPool *sPool;
    ULong          sFlushCount;

    initJob(aRetJob);

    sPool             = sdbBufferMgr::getPool();
    sNormalFlushList  = aColdJob->mFlushJobParam.mReplaceFlush.mFlushList;
    sDelayedFlushList = sPool->getDelayedFlushList( sNormalFlushList->getID() );
    sFlushCount       = sDelayedFlushList->getPartialLength();

    if ( sFlushCount != 0 )
    {
        aRetJob->mType          = SDB_FLUSH_JOB_REPLACEMENT_FLUSH;
        aRetJob->mReqFlushCount = SDB_FLUSH_COUNT_UNLIMITED;
        aRetJob->mFlushJobParam.mReplaceFlush.mLRUList
                                = sPool->getMinLengthLRUList();
        aRetJob->mFlushJobParam.mReplaceFlush.mFlushList
                                = sDelayedFlushList;
    }
    else
    {
        /* nothing to do */
    }
}

/*****************************************************************************
 * Description :
 *   �� �Լ��� checkpoint thread�� recovery LSN�� �ʹ� ��������
 *   ���ŵ��� �ʾ� checkpoint flush�� �� �ʿ䰡 �ִٰ� �Ǵ��� ���
 *   ȣ��ȴ�.
 *   jobDone�� �ɼ����� �� �� �ִµ� �� ���� �����ϸ� job�� �Ϸ�Ǿ��� ��
 *   � ó���� ������ �� �ִ�. job �ϷḦ �뺸���� �ʿ������
 *   �� ���ڿ� NULL�� �����ϸ� �ȴ�.
 *
 *  aStatistics       - [IN]  �������
 *  aReqFlushCount    - [IN]  flush ���� �Ǳ� ���ϴ� �ּ� ������ ����
 *  aRedoPageCount    - [IN]  Restart Recovery�ÿ� Redo�� Page ��,
 *                            �ݴ�� ���ϸ� CP List�� ���ܵ� Dirty Page�� ��
 *  aRedoLogFileCount - [IN]  Restart Recovery�ÿ� Redo�� LogFile ��,
 *                            �ݴ�� BufferPool�� ���ܵ� LSN ���� (LogFile�� ����)
 *  aJobDoneParam     - [IN]  job �ϷḦ �뺸 �ޱ⸦ ���Ҷ�, �� �Ķ���͸� �����Ѵ�.
 *  aAdded            - [OUT] job ��� ���� ����. ���� SDB_FLUSH_JOB_MAX���� ����
 *                            job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 *****************************************************************************/
void sdbFlushMgr::addReqCPFlushJob(
        idvSQL                     * aStatistics,
        ULong                        aMinFlushCount,
        ULong                        aRedoPageCount,
        UInt                         aRedoLogFileCount,
        sdbFlushJobDoneNotifyParam * aJobDoneParam,
        idBool                     * aAdded)
{
    sdbFlushJob *sCurr;

    IDE_ASSERT(mReqJobMutex.lock(aStatistics) == IDE_SUCCESS);

    sCurr = &mReqJobQueue[mReqJobAddPos];

    if (sCurr->mType == SDB_FLUSH_JOB_NOTHING)
    {
        if (aJobDoneParam != NULL)
        {
            initJobDoneNotifyParam(aJobDoneParam);
        }

        initJob(sCurr);
        sCurr->mType          = SDB_FLUSH_JOB_CHECKPOINT_FLUSH;
        sCurr->mReqFlushCount = aMinFlushCount;
        sCurr->mJobDoneParam  = aJobDoneParam;
        sCurr->mFlushJobParam.mChkptFlush.mRedoPageCount    = aRedoPageCount;
        sCurr->mFlushJobParam.mChkptFlush.mRedoLogFileCount = aRedoLogFileCount;
        sCurr->mFlushJobParam.mChkptFlush.mCheckpointType   = SDB_CHECKPOINT_BY_CHKPT_THREAD;

        incPos(&mReqJobAddPos);
        *aAdded = ID_TRUE;
    }
    else
    {
        *aAdded = ID_FALSE;
    }

    IDE_ASSERT(mReqJobMutex.unlock() == IDE_SUCCESS);
}

/*****************************************************************************
 * Description :
 *   �� �Լ��� tablespace �Ǵ� table�� drop �Ǵ� offline�Ǿ��� ��,
 *   �� tablespace�� table�� ���ؼ� flush �۾��� �� �� ���ȴ�.
 *   jobDone�� �ɼ����� �� �� �ִµ� �� ���� �����ϸ� job�� �Ϸ�Ǿ��� ��
 *   � ó���� ������ �� �ִ�. job �ϷḦ �뺸���� �ʿ������
 *   �� ���ڿ� NULL�� �����ϸ� �ȴ�.
 *
 * Implementation:
 *
 *  aStatistics - [IN]  �������
 *  aBCBQueue   - [IN]  flush�� ������ BCB�����Ͱ� ����ִ� ť
 *  aFiltFunc   - [IN]  aBCBQueue�� �ִ� BCB�� aFiltFunc���ǿ� �´� BCB�� flush
 *  aFiltObj    - [IN]  aFiltFunc�� �Ķ���ͷ� �־��ִ� ��
 *  aJobDoneParam-[IN]  job �ϷḦ �뺸 �ޱ⸦ ���Ҷ�, �� �Ķ���͸� �����Ѵ�.
 *  aAdded      - [OUT] job ��� ���� ����. ���� SDB_FLUSH_JOB_MAX���� ����
 *                      job�� ����Ϸ��� �õ��ϸ� ����� �����Ѵ�.
 *****************************************************************************/
void sdbFlushMgr::addReqDBObjectFlushJob(
        idvSQL                     *aStatistics,
        smuQueueMgr                *aBCBQueue,
        sdbFiltFunc                 aFiltFunc,
        void                       *aFiltObj,
        sdbFlushJobDoneNotifyParam *aJobDoneParam,
        idBool                     *aAdded)
{
    sdbFlushJob *sCurr;

    IDE_ASSERT(mReqJobMutex.lock(aStatistics) == IDE_SUCCESS);

    sCurr = &mReqJobQueue[mReqJobAddPos];
    if (sCurr->mType == SDB_FLUSH_JOB_NOTHING)
    {
        if (aJobDoneParam != NULL)
        {
            initJobDoneNotifyParam(aJobDoneParam);
        }

        initJob(sCurr);
        sCurr->mType         = SDB_FLUSH_JOB_DBOBJECT_FLUSH;
        sCurr->mJobDoneParam = aJobDoneParam;
        sCurr->mFlushJobParam.mObjectFlush.mBCBQueue = aBCBQueue;
        sCurr->mFlushJobParam.mObjectFlush.mFiltFunc = aFiltFunc;
        sCurr->mFlushJobParam.mObjectFlush.mFiltObj  = aFiltObj;

        incPos(&mReqJobAddPos);
        *aAdded = ID_TRUE;
    }
    else
    {
        *aAdded = ID_FALSE;
    }

    IDE_ASSERT(mReqJobMutex.unlock() == IDE_SUCCESS);
}

/******************************************************************************
 * Description :
 *    main job�� �ϳ� ��´�. main job�̶� flush list�� ���� ���̰� �ʰ��Ǹ�
 *    flush ������� �����ϰ� replacement flush job�� �����Ͽ� ��ȯ�Ѵ�.
 *
 * Implementation :
 *    pool�� flush list�� �߿� ���� �� list�� ���� flush ���̸� �ʰ��ϴ���
 *    �˻��Ͽ� �ʰ��Ǹ� flush job���� ������ �� ��ȯ�Ѵ�.
 *    �� �˰��� ���ϸ� ���ÿ� ���� flusher���� �� flush list�� ���ؼ���
 *    flush�� ���� �ִ�. ������ flush list�� ���̰� �پ��� flusher����
 *    �ٸ� flush list�� ���� �۾��� ������ ���̴�.
 *    �� flush list�� ���ؼ� �������� flusher�� flush �۾��� �����ص�
 *    ������ �������� �ʰԲ� ����Ǿ� �ִ�.
 *
 *    aRetJob   - [OUT] ���ϵǴ� job
 ******************************************************************************/
void sdbFlushMgr::getReplaceFlushJob(sdbFlushJob *aRetJob)
{
    sdbFlushList  *sFlushList;
    sdbBufferPool *sPool;
    
    sPool = sdbBufferMgr::getPool();
    sFlushList = sPool->getMaxLengthFlushList();

    initJob(aRetJob);

    /* BUG-24303 Dirty �������� ��� Wait���� Checkpoint Flush Job�� ���� �߻�
     *
     * Replacement Flush�� �߻��ϴ� ������ �ٽ� �������ڴ� ���̴�.
     *
     * ������ Busy�ϴٰ� �Ǵ��ϴ� ���� HIGH_FLUSH_PCT �̻��� ���
     * �� ����Ͽ��µ�, �̰ͻӸ� �ƴ϶� LOW_FLUSH_PCT �̻� LOW_RREPARE_PCT
     * ���� �� ����̴�. ��, �� ������ Replacement Flush�� �߻��� �� �ִ� ���ǰ�
     * ��ġ�̴�.
     *
     * ������ ������ Replacement Flush�� �߻��� �� �ִ� ���ǿ��� Busy�ϴٰ� �Ǵ�
     * �ϰ�� �����ʰ� Flush Job�� �߻���Ű�µ� �̶�, ���ǰ˻� �������� 
     * Replacement Flush�� �߻����� �ʰ�, ��� Checkpoint List Flush���� 
     * �����Ͽ� �߻��ϹǷ� �ؼ� CPU�� �ִ�� ����ϴ� ������ �߻��Ѵ�.
     * ( ChkptList Flush�� ���ؼ� Flush List ���̰� �پ���� �ʴ� ����
     * Flush���� Dirty ���°� �����Ǿ Flush List�� ������ �� �ֱ� �����̴�. )
     *
     * �׷��Ƿ�, getJob�� Replacement Flush Job�� �����ϵ��� �ؼ� Flush
     * List�� clean BCB�� prepare list�� �̵�����, Busy ������ �������� �ʵ���
     * �ؾ��Ѵ�.
     */
    if ( isCond4ReplaceFlush() == ID_TRUE )
    {
        aRetJob->mType          = SDB_FLUSH_JOB_REPLACEMENT_FLUSH;
        aRetJob->mReqFlushCount = SDB_FLUSH_COUNT_UNLIMITED;
        aRetJob->mFlushJobParam.mReplaceFlush.mFlushList
                                = sFlushList;
        aRetJob->mFlushJobParam.mReplaceFlush.mLRUList
                                = sPool->getMinLengthLRUList();
    }
    else
    {
        aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
    }
}


/******************************************************************************
 * Description :
 *   incremental checkpoint�� �����ϴ� job�� ��´�.
 *   �� �Լ��� �����Ѵٰ� �ؼ� �׳� job�� ������ �ʴ´�. ��, � ������ ����
 *   �ؾ� �ϴµ�, �������� �Ʒ� ���� �ϳ��� ���� �� ���̴�.
 *   1. ������ checkpoint ���� �� �ð��� ���� ������
 *   2. Recovery LSN�� �ʹ� ������
 *
 *  aStatistics - [IN]  �������
 *  aRetJob     - [OUT] ���ϵǴ� job
 ******************************************************************************/
void sdbFlushMgr::getCPFlushJob(idvSQL *aStatistics, sdbFlushJob *aRetJob)
{
    smLSN       sMinLSN;
    idvTime     sCurrentTime;
    ULong       sTime;

    mCPListSet->getMinRecoveryLSN(aStatistics, &sMinLSN);

    IDV_TIME_GET(&sCurrentTime);
    sTime = IDV_TIME_DIFF_MICRO(&mLastFlushedTime, &sCurrentTime) / 1000000;

    if ( ( smLayerCallback::isCheckpointFlushNeeded(sMinLSN) == ID_TRUE ) ||
         ( sTime >= smuProperty::getCheckpointFlushMaxWaitSec() ) )
    {
        initJob(aRetJob);
        aRetJob->mType          = SDB_FLUSH_JOB_CHECKPOINT_FLUSH;
        aRetJob->mReqFlushCount = smuProperty::getCheckpointFlushCount();
        aRetJob->mFlushJobParam.mChkptFlush.mRedoPageCount
                                = smuProperty::getFastStartIoTarget();
        aRetJob->mFlushJobParam.mChkptFlush.mRedoLogFileCount
                                = smuProperty::getFastStartLogFileTarget();
        aRetJob->mFlushJobParam.mChkptFlush.mCheckpointType 
                                = SDB_CHECKPOINT_BY_FLUSH_THREAD;
    }
    else
    {
        aRetJob->mType = SDB_FLUSH_JOB_NOTHING;
    }
}

/******************************************************************************
 * Description :
 *    flusher �����带 �����Ų��. �� flusher �ϳ��� ������Ų��.
 *    �����常 ������ �� �ڿ��� �������� �ʴ´�.
 *    ������ flusher�� turnOnFlusher()�� �ٽ� ������ų �� �ִ�.
 *
 *  aStatistics - [IN]  �������
 *  aFlusherID  - [IN]  flusher id
 ******************************************************************************/
IDE_RC sdbFlushMgr::turnOffFlusher(idvSQL *aStatistics, UInt aFlusherID)
{
    idBool sNotStarted;
    idBool sLatchLocked = ID_FALSE;

    IDE_DASSERT(aFlusherID < mFlusherCount);

    ideLog::log( IDE_SM_0, "[PREPARE] Stop flusher ID(%d)\n", aFlusherID );

    // BUG-26476 
    // checkpoint�� �߻��ؼ� checkpoint flush job�� ����Ǳ⸦ 
    // ��� �ϰ� �ִ� �����尡 ���� �� �ִ�. �׷� ��� 
    // flusher�� ���� ����ϴ� �����尡 ������ ����� �� 
    // �ֱ� ������ �̶� ����ϴ� �����尡 �������� ������ �Ѵ�. 

    IDE_ASSERT(mFCLatch.lockWrite(NULL, 
                                  NULL) 
               == IDE_SUCCESS); 
    sLatchLocked = ID_TRUE;

    IDE_TEST(mFlushers[aFlusherID].finish(aStatistics,
                                          &sNotStarted) != IDE_SUCCESS);

    sLatchLocked = ID_FALSE;
    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);

    ideLog::log( IDE_SM_0, "[SUCCESS] Stop flusher ID(%d)\n", aFlusherID );

    IDE_TEST_RAISE(sNotStarted == ID_TRUE,
                   ERROR_FLUSHER_NOT_STARTED);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERROR_FLUSHER_NOT_STARTED);
    {
        ideLog::log( IDE_SM_0, 
                "[WARNING] flusher ID(%d) already stopped.\n", aFlusherID );

        IDE_SET(ideSetErrorCode(smERR_ABORT_sdbFlusherNotStarted,
                                aFlusherID));
    }
    IDE_EXCEPTION_END;

    if (sLatchLocked == ID_TRUE)
    {
        sLatchLocked = ID_FALSE;
        IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);
    }

    ideLog::log( IDE_SM_0, "[FAILURE] Stop flusher ID(%d)\n", aFlusherID );

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    ����� flusher�� �ٽ� �籸����Ų��.
 *
 *  aFlusherID  - [IN]  flusher id
 ******************************************************************************/
IDE_RC sdbFlushMgr::turnOnFlusher(UInt aFlusherID)
{
    IDE_DASSERT(aFlusherID < mFlusherCount);

    ideLog::log( IDE_SM_0, "[PREPARE] Start flusher ID(%d)\n", aFlusherID );

    IDE_ASSERT(mFCLatch.lockWrite(NULL,
                                  NULL)
               == IDE_SUCCESS);

    IDE_TEST_RAISE(mFlushers[aFlusherID].isRunning() == ID_TRUE,
                   ERROR_FLUSHER_RUNNING);

    IDE_TEST(mFlushers[aFlusherID].start() != IDE_SUCCESS);

    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);

    ideLog::log( IDE_SM_0, "[SUCCESS] Start flusher ID(%d)\n", aFlusherID );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERROR_FLUSHER_RUNNING);
    {
        ideLog::log( IDE_SM_0, 
                "[WARNING] flusher ID(%d) already started\n", aFlusherID );

        IDE_SET(ideSetErrorCode(smERR_ABORT_sdbFlusherRunning, aFlusherID));
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);

    ideLog::log( IDE_SM_0, "[FAILURE] Start flusher ID(%d)\n", aFlusherID );

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    ����ȸ�� flusher�� �ٽ� �籸����Ų��.
 *
 ******************************************************************************/
IDE_RC sdbFlushMgr::turnOnAllflushers()
{
    UInt i;

    IDE_ASSERT(mFCLatch.lockWrite(NULL, 
                                  NULL) 
               == IDE_SUCCESS); 

    for (i = 0; i < mFlusherCount; i++)
    {
        if( mFlushers[i].isRunning() != ID_TRUE )
	{
	   IDE_TEST(mFlushers[i].start() != IDE_SUCCESS);
	}
    }

    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS);

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  ����ڰ� ��û�� flush�۾� ��û�� �������� Ȯ���ϴ� ������
 *  sdbFlushJobDoneNotifyParam �� �ʱ�ȭ �Ѵ�.
 *
 *  aParam  - [IN]  �ʱ�ȭ�� ����
 ******************************************************************************/
IDE_RC sdbFlushMgr::initJobDoneNotifyParam(sdbFlushJobDoneNotifyParam *aParam)
{
    IDE_TEST(aParam->mMutex.initialize((SChar*)"BUFFER_FLUSH_JOB_COND_WAIT_MUTEX",
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_LATCH_FREE_OTHERS)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(aParam->mCondVar.initialize((SChar*)"BUFFER_FLUSH_JOB_COND_WAIT_COND") != IDE_SUCCESS, err_cond_init);

    aParam->mJobDone = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  �ʱ�ȭ�� sdbFlushJobDoneNotifyParam�� ��������
 *  ��û�� �۾��� �����ϱ⸦ ����Ѵ�.
 *
 *  aParam  - [IN]  ��⸦ ���� �ʿ��� ����
 ******************************************************************************/
IDE_RC sdbFlushMgr::waitForJobDone(sdbFlushJobDoneNotifyParam *aParam)
{
    IDE_ASSERT(aParam->mMutex.lock(NULL) == IDE_SUCCESS);

    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while( aParam->mJobDone == ID_FALSE )
    {
        /* ����� job�� ó�� �Ϸ�ɶ����� ����Ѵ�.
         * cond_wait�� �����԰� ���ÿ� mMutex�� Ǭ��. */
        IDE_TEST_RAISE(aParam->mCondVar.wait(&(aParam->mMutex))
                       != IDE_SUCCESS, err_cond_wait);
    }

    IDE_ASSERT(aParam->mMutex.unlock() == IDE_SUCCESS);

    IDE_TEST_RAISE(aParam->mCondVar.destroy() != IDE_SUCCESS,
                   err_cond_destroy);

    IDE_TEST(aParam->mMutex.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION(err_cond_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    CPListSet�� dirty ���۵��� flush�Ѵ�.
 *    flush�� �Ϸ�� ������ �� �Լ��� ���������� �ʴ´�.
 *
 * Implementation :
 *    �� �Լ��� �ǿܷ� �����ϴ�. checkpoint flush job�� ����ϰ�
 *    �� job�� ó�� �Ϸ�� ������ ��ٸ��� ����, mutex�� �����ϰ�
 *    condition variable�� �����Ͽ� cond_wait�� �ϰԵȴ�.
 *    �� ��� ���¿� �ִٰ� flusher�� �̶� ��û�� job�� ó�� �Ϸ��ϸ�
 *    notifyJobDone()�� ȣ���ϰ� �Ǵµ� �׷��� cond_signal()�� ȣ���ϰ� �Ǿ�
 *    cond_wait���� Ǯ������ �ǰ� �׷��� ��μ� �� �Լ��� ���������� �ȴ�.
 *
 *  aStatistics     - [IN]  �������
 *  aFlushAll       - [IN]  check point list���� ��� BCB�� flush �Ϸ���
 *                          �� ������ ID_TRUE�� �����Ѵ�.
 ******************************************************************************/
IDE_RC sdbFlushMgr::flushDirtyPagesInCPList(idvSQL    * aStatistics,
                                            idBool      aFlushAll,
                                            idBool      aIsChkptThread )
{
    idBool                      sAdded          = ID_FALSE;
    sdbFlushJobDoneNotifyParam  sJobDoneParam;
    PDL_Time_Value              sTV;
    ULong                       sMinFlushCount;
    ULong                       sRedoPageCount;
    UInt                        sRedoLogFileCount;
    ULong                       sRetFlushedCount;
    idBool                      sLatchLocked    = ID_FALSE; 

    /* BUG-26476 
     * checkpoint ����� flusher���� stop�Ǿ� ������ skip�� �ؾ� �Ѵ�. 
     * ������ checkpoint�� ���� checkpoint flush�� ����ǰ� �ִ� ���� 
     * flush�� stop�Ǹ� �ȵȴ�. */
    IDE_ASSERT(mFCLatch.lockRead(NULL, 
                                 NULL) 
               == IDE_SUCCESS); 
    sLatchLocked = ID_TRUE; 

    if ( getActiveFlusherCount() == 0 )
    { 
        IDE_CONT(SKIP_FLUSH); 
    } 

    if (aFlushAll == ID_FALSE)
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

    if ( (aIsChkptThread == ID_TRUE) &&
         (smuProperty::getCheckpointFlushResponsibility() == 1) )
    {
        IDE_TEST( smLayerCallback::flushForCheckpoint( aStatistics,
                                                       sMinFlushCount,
                                                       sRedoPageCount,
                                                       sRedoLogFileCount,
                                                       &sRetFlushedCount )
                  != IDE_SUCCESS );

    }
    else
    {
        while (sAdded == ID_FALSE)
        {
            addReqCPFlushJob(aStatistics,
                             sMinFlushCount,
                             sRedoPageCount,
                             sRedoLogFileCount,
                             &sJobDoneParam,
                             &sAdded);
            IDE_TEST(wakeUpAllFlushers() != IDE_SUCCESS);
            sTV.set(0, 50000);
            idlOS::sleep(sTV);
        }
 
        IDE_TEST(waitForJobDone(&sJobDoneParam) != IDE_SUCCESS);
    }
    IDE_EXCEPTION_CONT(SKIP_FLUSH); 

    sLatchLocked = ID_FALSE; 
    IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sLatchLocked == ID_TRUE) 
    { 
        IDE_ASSERT(mFCLatch.unlock() == IDE_SUCCESS); 
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * BCB �����Ͱ� ����ִ� queue�� �� BCB���� flush �ؾ� ���� ����
 * ������ �� �ִ� aFiltFunc �� aFiltObj�� ��������
 * �÷��ø� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCBQueue   - [IN]  flush�� ����� ��� �ִ� BCB�� �����Ͱ� �ִ� queue
 *  aFiltFunc   - [IN]  aBCBQueue�� �ִ� BCB�� aFiltFunc���ǿ� �´� BCB�� flush
 *  aFiltObj    - [IN]  aFiltFunc�� �Ķ���ͷ� �־��ִ� ��
 ******************************************************************************/
IDE_RC sdbFlushMgr::flushObjectDirtyPages(idvSQL       *aStatistics,
                                          smuQueueMgr  *aBCBQueue,
                                          sdbFiltFunc   aFiltFunc,
                                          void         *aFiltObj)
{
    idBool                     sAdded = ID_FALSE;
    sdbFlushJobDoneNotifyParam sJobDoneParam;
    PDL_Time_Value             sTV;

    while (sAdded == ID_FALSE)
    {
        addReqDBObjectFlushJob(aStatistics,
                               aBCBQueue,
                               aFiltFunc,
                               aFiltObj,
                               &sJobDoneParam,
                               &sAdded);
        IDE_TEST(wakeUpAllFlushers() != IDE_SUCCESS);
        sTV.set(0, 50000);
        idlOS::sleep(sTV);
    }

    IDE_TEST(waitForJobDone(&sJobDoneParam) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    job�� ó�������� �˸���. ���� jobDone�� �������� �ʾ�����
 *    �� �Լ��ȿ��� �ƹ��� �۾��� ���� �ʴ´�.
 *    jobDone�� job�� ����� �� �ɼ����� �� �� �ִ�.
 *
 * aParam   - [IN]  �� ������ ����ϰ� �ִ� ������鿡�� job�� ���� �Ǿ�����
 *                  �˸���.
 ******************************************************************************/
void sdbFlushMgr::notifyJobDone(sdbFlushJobDoneNotifyParam *aParam)
{
    if (aParam != NULL)
    {
        IDE_ASSERT(aParam->mMutex.lock(NULL) == IDE_SUCCESS);

        IDE_ASSERT(aParam->mCondVar.signal() == 0);

        aParam->mJobDone = ID_TRUE;

        IDE_ASSERT(aParam->mMutex.unlock() == IDE_SUCCESS);
    }
}

/******************************************************************************
 * Description :
 *    flush list�� ���̰� �� �Լ��� ���ϰ� �̻��̵Ǹ�, ���� ���� BCB�� ����
 *    flush�� �����Ѵ�.
 *    �ڼ��� �뵵�� sdbFlushMgr::isCond4ReplaceFlush �� ���� �˼� �ִ�.
 ******************************************************************************/
ULong sdbFlushMgr::getLowFlushLstLen4FF()
{
    sdbBufferPool *sPool;
    ULong          sPoolSizePerFlushList;
    ULong          sRet;

    sPool = sdbBufferMgr::getPool();

    sPoolSizePerFlushList = sPool->getPoolSize() / sPool->getFlushListCount();
    // LOW_FLUSH_PCT 1%
    sRet = (sPoolSizePerFlushList * smuProperty::getLowFlushPCT()) / 100;

    return (sRet == 0) ? 1 : sRet;
}

/******************************************************************************
 * Description :
 *    flush list�� ���̰� �� �Լ��� ���ϰ� �̻��̵Ǹ� ���� ���� BCB�� ����
 *    flush�� �����Ѵ�.
 *    �ڼ��� �뵵�� sdbFlushMgr::isCond4ReplaceFlush�� ���� �˼� �ִ�.
 ******************************************************************************/
ULong sdbFlushMgr::getHighFlushLstLen4FF()
{
    sdbBufferPool  *sPool;
    ULong           sPoolSizePerFlushList;
    ULong           sRet;

    sPool = sdbBufferMgr::getPool();
    sPoolSizePerFlushList = sPool->getPoolSize() / sPool->getFlushListCount();
    // HIGH_FLUSH_PCT 5%
    sRet = (sPoolSizePerFlushList * smuProperty::getHighFlushPCT()) / 100;

    return (sRet == 0) ? 1 : sRet;
}

/******************************************************************************
 * Description :
 *    prepare list�� ���̰� �� ���� ���ϰ� �Ǹ� ���� ���� BCB�� ����
 *    flush�� �����Ѵ�.
 *    �ڼ��� �뵵�� sdbFlushMgr::isCond4ReplaceFlush�� ���� �� �� �ִ�.
 ******************************************************************************/
ULong sdbFlushMgr:: getLowPrepareLstLen4FF()
{
    sdbBufferPool  *sPool;
    ULong           sPoolSizePerPrepareList;

    sPool = sdbBufferMgr::getPool();
    sPoolSizePerPrepareList = sPool->getPoolSize() / sPool->getPrepareListCount();
    // LOW_PREPARE_PCT 1%
    return (sPoolSizePerPrepareList * smuProperty::getLowPreparePCT()) / 100;
}

/******************************************************************************
 *
 * Description : Replacement Flush �� ����
 *
 * RF�� �߻��� �� �ִ� ������ ������ ����.
 *
 * ù��°, �ִ������ Flush List�� ���̰� ��ü���� ��� HIGH_FLUSH_PCT(�⺻��5%)
 *        �̻��� ��츦 �����Ѵ�.
 *
 * �ι�°, �ּұ����� Prepared List�� ���̰� ��ü���� ��� LOW_PREPARE_PCT(�⺻��1%) ����
 *        �̸鼭 �ִ������ Flush List�� ���̰� ��ü���� ��� LOW_FLUSH_PCT(�⺻��1%) �̻�
 *        �� ��츦 �����Ѵ�.
 *
 ******************************************************************************/
idBool sdbFlushMgr::isCond4ReplaceFlush()
{
    sdbBufferPool  *sPool;
    ULong           sPrepareLength;
    ULong           sTotalLength;
    ULong           sNormalFlushLength;
    sdbFlushList   *sFlushList;

    sPool = sdbBufferMgr::getPool();

    /* PROJ-2669
     * ���� Flush List �� Delayed Flush List �� �߰��Ǿ����Ƿ�
     * Delayed/Normal ������ �����ִ� �Լ��� ����ؾ� �Ѵ�.      */
    sFlushList = sPool->getMaxLengthFlushList();

    sNormalFlushLength = sFlushList->getPartialLength();
    sTotalLength = sPool->getFlushListLength( sFlushList->getID() );

    /* PROJ-2669
     * Normal Flush List �� Length==0 �̶��
     * ���� ���Եǰ� �ִ� Dirty Page�� �����Ƿ�
     * Replacement Flush Condition ���� �������� �ʾƵ� �ȴ�. */
    if ( sNormalFlushLength != 0 )
    {
        if ( sTotalLength > getHighFlushLstLen4FF() )
        {
            return ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        sPrepareLength = sPool->getMinLengthPrepareList()->getLength();

        if ( ( sPrepareLength < getLowPrepareLstLen4FF() ) &&
             ( sTotalLength > getLowFlushLstLen4FF() ) )
        {
            return ID_TRUE;
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

    return ID_FALSE;
}

/******************************************************************************
 * Description :
 *    flusher�� job�� ó���ϰ� ���� job�� ó���� ������ ����� �ð���
 *    ��ȯ�Ѵ�. �ý����� ��Ȳ�� ���� �������̴�. flush�Ұ� ������
 *    ��� �ð��� ª�� �Ұ� ���� ������ ��� �ð��� �������.
 *
 ******************************************************************************/
idBool sdbFlushMgr::isBusyCondition()
{
    idBool sRet = ID_FALSE;

    if (mReqJobQueue[mReqJobGetPos].mType != SDB_FLUSH_JOB_NOTHING)
    {
        sRet = ID_TRUE;
    }

    if ( isCond4ReplaceFlush() == ID_TRUE )
    {
        sRet = ID_TRUE;
    }

    return sRet;
}

/******************************************************************************
 * Abstraction : ���� �������� flusher�� ���� ��ȯ�Ѵ�.
 ******************************************************************************/
UInt sdbFlushMgr::getActiveFlusherCount()
{
    UInt i;
    UInt sActiveCount = 0;

    for( i = 0 ; i < mFlusherCount ; i++ )
    {
        if( mFlushers[ i ].isRunning() == ID_TRUE )
        {
            sActiveCount++;
        }
    }
    return sActiveCount;
}

/******************************************************************************
 * Description :
 *  PROJ-2669
 *   Delayed Flush ��ɿ� ���õ� ������Ƽ ���� �Լ�
 ******************************************************************************/
void sdbFlushMgr::setDelayedFlushProperty( UInt aDelayedFlushListPct,
                                           UInt aDelayedFlushProtectionTimeMsec )
{
    UInt i;

    for ( i = 0 ; i < mFlusherCount ; i++ )
    {
        mFlushers[ i ].setDelayedFlushProperty( aDelayedFlushListPct,
                                                aDelayedFlushProtectionTimeMsec );
    }
}

