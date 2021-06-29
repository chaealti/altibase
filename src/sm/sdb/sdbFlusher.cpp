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
 
/******************************************************************************
 * Description :
 *    sdbFlusher ��ü�� �ý��� ������μ� ���� Ǯ�� dirty ���۵��� flush�ϴ�
 *    ������ �Ѵ�. flush �������� replacement flush�� checkpoint flush �ΰ�����
 *    �ִµ� replacement flush�� flush list�� �ִ� dirty ���۵��� flush�����ν�
 *    replace �߻��� victim�� ���� ã���� ���ִ� ������ �ϰ�, checkpoint flush��
 *    checkpoint list ���� dirty ���۵��� flush�����ν� redo LSN�� ���
 *    restart�� ���� ������ �ϰ� ���ִ� ������ �Ѵ�.
 *    flusher�� �ֱ������� ���鼭 �� �ΰ��� flush �۾��� �����Ѵ�.
 *    �׸��� flusher�� �����ð��� �� �� flush �۾��� �����ϴµ�,
 *    timeover�� ��� ���� �ְ� �ܺ� signal�� ���� ��� ���� �ִ�.
 *    �ܺ� signal�̶� Ʈ����� �����尡 victim�� ã�ٰ� ������ ���
 *    �߻��ȴ�.
 *
 * Implementation :
 *    flusher�� replacement flush�� checkpoint flush�� sdbFlushMgr�κ���
 *    job ���·� �޾Ƽ� �����Ѵ�. flusher�� �����ð� cond_wait�� �ϴٰ�
 *    timeover �Ǵ� signal�� �ް� ��� job�� �ϳ� ���ͼ� ó���Ѵ�.
 *    �� flusher ��ü�� ���������� IO buffer�� �����ϴµ� �̸� IOB��� �Ѵ�.
 *    IOB�� ���۸� ��ũ�� ����ϱ� ���� �����ϴ� �޸� �����̴�.
 *    �ϳ��� ���۸� flush�ϴ� ������ ������ ����.
 *     1. checkpoint list �Ǵ� flush list�κ��� flush�� ���۸� ã�´�.
 *     2. flush ������ �����ϸ� page latch�� S-mode�� ȹ���Ѵ�.
 *     3. BCBMutex�� ��� mState�� INIOB ���·� �ٲ� �� BCBMutex�� Ǭ��.
 *        �� �������� �ٸ� flusher���� �� ���۸� flush ������� ���� �� ����.
 *        S-latch�� ��� �ֱ� ������ redirty�� �� �� ����.
 *     4. �� ������ recovery LSN�� flusher�� min recovery LSN�� �ݿ��Ѵ�.
 *     5. checkpoint list���� �� ���۸� �����Ѵ�.
 *     6. IOB�� ������ ������ �����Ѵ�.
 *     7. page latch�� Ǭ��.
 *        �̶����ʹ� ���۰� redirty�� �� �ִ�. redirty�� �Ǹ�
 *        ���۴� �ٽ� checkpoint list�� �޸���. ������ redirty�� ���۴�
 *        �ٸ� flusher���� flush�� �� ����.
 *     8. IOB�� ��ũ�� ����Ѵ�.
 *     9. ������ ���¸� clean ���·� �ٲ۴�.
 *     10. replacement flush�� ��� �� ���۸� prepare list�� �ű��.
 *
 *    ���⼭ 4������ ����� min recovery LSN�� ���� ������ �ʿ䰡 �ִ�.
 *    checkpoint�� �߻��ϸ� ��ũ�� ��ϵ��� ���� dirty ���۵� �߿�
 *    ���� ���� recovery LSN�� restart redo LSN���� �����ؾ� �Ѵ�.
 *    ������ checkpoint list���� dirty ���۸� ���� ���� �� ������ recovery LSN��
 *    checkpoint�� �ݿ��� �� ���� �ȴ�. �׷��ٰ� �� ���۰� ��ũ�� ��ϵ� ��
 *    �ƴϴ�. ���� ���۰� IOB�� �����ϴ� ���� checkpoint�� checkpoint list
 *    �Ӹ��ƴ϶� IOB�� ��ϵ� ���۵���� ��� ����Ͽ� �ּҰ��� recovery LSN��
 *    �����ؾ� �Ѵ�. �׷��� ���ؼ� sdbFlusher ��ü�� mMinRecoveryLSN�̶�
 *    �ɹ� ������ �����Ѵ�. �� �ɹ��� IOB�� ����� ���۵� �߿�
 *    ���� ���� recovery LSN�� ������ �ִ�. IOB�� ���۸� ������ ������
 *    mMinRecoveryLSN�� ���Ͽ� �� ���� ���� ������ �� �ɹ��� �����Ѵ�.
 *    �׸��� IOB�� ��ũ�� ������ �Ŀ��� �� ���� �ִ밪���� �����Ѵ�.
 *    checkpoint�� �߻��ϸ� checkpoint list�� minRecovery�� �� flusher��
 *    �� mMinRecoveryLSN �߿� ���� ���� ���� restart redo LSN���� �����Ѵ�.
 ******************************************************************************/
#include <smErrorCode.h>
#include <sdbFlushMgr.h>
#include <sdbFlusher.h>
#include <sdbReq.h>
#include <smuProperty.h>
#include <sddDiskMgr.h>
#include <sdbBufferMgr.h>
#include <smrRecoveryMgr.h>
#include <sdpPhyPage.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h>
#include <sdsFlusher.h>
#include <sdsFlushMgr.h>

#define IS_USE_DELAYED_FLUSH() \
    ( mDelayedFlushListPct != 0 )
#define IS_DELAYED_FLUSH_LIST( __LIST ) \
    ( (__LIST)->getListType() == SDB_BCB_DELAYED_FLUSH_LIST )

extern "C" SInt
sdbCompareSyncFileInfo( const void* aElem1, const void* aElem2 )
{
    sdbSyncFileInfo sFileInfo1;
    sdbSyncFileInfo sFileInfo2;

    sFileInfo1 = *(sdbSyncFileInfo*)aElem1;
    sFileInfo2 = *(sdbSyncFileInfo*)aElem2;

    if ( sFileInfo1.mSpaceID > sFileInfo2.mSpaceID )
    {
        return 1;
    }
    else
    {
        if ( sFileInfo1.mSpaceID < sFileInfo2.mSpaceID )
        {
            return -1;
        }
    }

    if ( sFileInfo1.mFileID > sFileInfo2.mFileID )
    {
        return 1;
    }
    else
    {
        if ( sFileInfo1.mFileID < sFileInfo2.mFileID )
        {
            return -1;
        }
    }
    return 0;
}

sdbFlusher::sdbFlusher() : idtBaseThread()
{

}

sdbFlusher::~sdbFlusher()
{

}

/******************************************************************************
 * Description :
 *   flusher ��ü�� �ʱ�ȭ�Ѵ�. flusher thread�� start�ϱ� ���� �ݵ��
 *   �ʱ�ȭ�� �����ؾ� �Ѵ�. initialize�� ��ü�� ��� �� destroy��
 *   �ڿ��� ��������� �Ѵ�. destroy�� ��ü�� �ٽ� initialize�� ȣ���Ͽ�
 *   ������ �� �ִ�.
 *
 *  aFlusherID      - [IN]  �� ��ü�� ���� ID. flusher���� ���� ���� ID��
 *                          ������.
 *  aPageSize       - [IN]  ������ ũ��. flusher�鸶�� page size��
 *                          �ٸ��� �� �� �ִ�.
 *  aPageCount      - [IN]  flusher�� IOB ũ��μ� ������ page �����̴�.
 *  aCPListSet      - [IN]  �÷��� �ؾ� �ϴ� FlushList�� �����ִ� buffer pool
 *                          �� ���� checkpoint set
 ******************************************************************************/
IDE_RC sdbFlusher::initialize(UInt          aFlusherID,
                              UInt          aPageSize,
                              UInt          aPageCount,
                              sdbCPListSet *aCPListSet)
{
    SChar   sMutexName[128];
    UInt    i;
    SInt    sState = 0;

    mFlusherID    = aFlusherID;
    mPageSize     = aPageSize;
    mIOBPageCount = aPageCount;
    mCPListSet    = aCPListSet;
    mIOBPos       = 0;
    mFinish       = ID_FALSE;
    mStarted      = ID_FALSE;
    mWaitTime     = smuProperty::getDefaultFlusherWaitSec();
    mPool         = sdbBufferMgr::getPool();
    mServiceable  = sdsBufferMgr::isServiceable();

    SM_LSN_MAX(mMinRecoveryLSN);
    SM_LSN_INIT(mMaxPageLSN);

    // ��� ������ ���� private session ���� �ʱ�ȭ
    idvManager::initSession(&mOldSess, 0 /* unused */, NULL /* unused */);

    // BUG-21155 : current session�� �ʱ�ȭ
    idvManager::initSession(&mCurrSess, 0 /* unused */, NULL /* unused */);

    idvManager::initSQL(&mStatistics,
                        &mCurrSess,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        IDV_OWNER_FLUSHER);

    // mutex �ʱ�ȭ
    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName,
                    128,
                    "BUFFER_FLUSHER_MIN_RECOVERY_LSN_MUTEX_%"ID_UINT32_FMT,
                    aFlusherID);

    IDE_TEST(mMinRecoveryLSNMutex.initialize(sMutexName,
                                  IDU_MUTEX_KIND_NATIVE,
                                  IDB_WAIT_INDEX_LATCH_FREE_DRDB_BUFFER_FLUSHER_MIN_RECOVERY_LSN)
             != IDE_SUCCESS);
    sState = 1;

    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName,
                    128,
                    "BUFFER_FLUSHER_COND_WAIT_MUTEX_%"ID_UINT32_FMT,
                    aFlusherID);

    IDE_TEST(mRunningMutex.initialize(sMutexName,
                                    IDU_MUTEX_KIND_POSIX,
                                    IDV_WAIT_INDEX_LATCH_FREE_OTHERS)
             != IDE_SUCCESS);
    sState = 2;

    /* TC/FIT/Limit/sm/sdbFlusher_initialize_malloc1.sql */
    IDU_FIT_POINT_RAISE( "sdbFlusher::initialize::malloc1",
                          insufficient_memory );

    // IOB �ʱ�ȭ
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)mPageSize * (mIOBPageCount + 1),
                                     (void**)&mIOBSpace) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 3;

    idlOS::memset((void*)mIOBSpace, 0, mPageSize * (mIOBPageCount + 1));

    mIOB = (UChar*)idlOS::align((void*)mIOBSpace, mPageSize);

    /* TC/FIT/Limit/sm/sdbFlusher_initialize_malloc2.sql */
    IDU_FIT_POINT_RAISE( "sdbFlusher::initialize::malloc2",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF(UChar*) * mIOBPageCount,
                                     (void**)&mIOBPtr) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 4;

    /* TC/FIT/Limit/sm/sdbFlusher_initialize_malloc3.sql */
    IDU_FIT_POINT_RAISE( "sdbFlusher::initialize::malloc3",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF(sdbBCB*) * mIOBPageCount,
                                     (void**)&mIOBBCBArray) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 5;

    /* TC/FIT/Limit/sm/sdbFlusher_initialize_malloc4.sql */
    IDU_FIT_POINT_RAISE( "sdbFlusher::initialize::malloc4",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDB,
                                       (ULong)ID_SIZEOF(sdbSyncFileInfo) * mIOBPageCount,
                                       (void**)&mArrSyncFileInfo ) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 6;

    // condition variable �ʱ�ȭ
    // flusher�� ���� ���� ���⿡ ��⸦ �Ѵ�.
    idlOS::snprintf(sMutexName,
                    ID_SIZEOF(sMutexName),
                    "BUFFER_FLUSHER_COND_%"ID_UINT32_FMT,
                    aFlusherID);

    IDE_TEST_RAISE(mCondVar.initialize(sMutexName) != IDE_SUCCESS,
                   err_cond_var_init);

    for (i = 0; i < mIOBPageCount; i++)
    {
        mIOBPtr[i] = mIOB + mPageSize * i;
    }

    IDE_TEST(mDWFile.create(mFlusherID, 
                            mPageSize, 
                            mIOBPageCount,
                            SD_LAYER_BUFFER_POOL)
             != IDE_SUCCESS);

    mStat.initialize(mFlusherID);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch (sState)
    {
        case 6:
            IDE_ASSERT( iduMemMgr::free( mArrSyncFileInfo ) == IDE_SUCCESS );
        case 5:
            IDE_ASSERT(iduMemMgr::free(mIOBBCBArray) == IDE_SUCCESS);
        case 4:
            IDE_ASSERT(iduMemMgr::free(mIOBPtr) == IDE_SUCCESS);
        case 3:
            IDE_ASSERT(iduMemMgr::free(mIOBSpace) == IDE_SUCCESS);
        case 2:
            IDE_ASSERT(mRunningMutex.destroy() == IDE_SUCCESS);
        case 1:
            IDE_ASSERT(mMinRecoveryLSNMutex.destroy() == IDE_SUCCESS);
        default:
           break;
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    flusher ��ü�� ������ �ִ� �޸�, mutex, condition variable����
 *    �����Ѵ�. destroy�� ��ü�� �ٽ� initialize�� ȣ�������ν� ������ ��
 *    �ִ�.
 ******************************************************************************/
IDE_RC sdbFlusher::destroy()
{
    IDE_ASSERT(mDWFile.destroy() == IDE_SUCCESS);

    IDE_ASSERT( iduMemMgr::free( mArrSyncFileInfo ) == IDE_SUCCESS );
    mArrSyncFileInfo = NULL;

    IDE_ASSERT(iduMemMgr::free(mIOBBCBArray) == IDE_SUCCESS);
    mIOBBCBArray = NULL;

    IDE_ASSERT(iduMemMgr::free(mIOBPtr) == IDE_SUCCESS);
    mIOBPtr = NULL;

    IDE_ASSERT(iduMemMgr::free(mIOBSpace) == IDE_SUCCESS);
    mIOBSpace = NULL;

    IDE_ASSERT(mRunningMutex.destroy() == IDE_SUCCESS);

    IDE_ASSERT(mMinRecoveryLSNMutex.destroy() == IDE_SUCCESS);

    IDE_TEST_RAISE(mCondVar.destroy() != IDE_SUCCESS, err_cond_destroy);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    flusher �������� ������ ���������� �����Ų��. thread�� ����� ������
 *    ���ϵ��� �ʴ´�.
 *
 *  aStatistics - [IN]  �������
 *  aNotStarted - [OUT] flusher�� ���� ���۵��� �ʾҴٸ� ID_TRUE�� ����. �� �ܿ�
 *                      ID_FALSE�� ����
 ******************************************************************************/
IDE_RC sdbFlusher::finish(idvSQL *aStatistics, idBool *aNotStarted)
{
    IDE_ASSERT(mRunningMutex.lock(aStatistics) == IDE_SUCCESS);
    if (mStarted == ID_FALSE)
    {
        IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);
        *aNotStarted = ID_TRUE;
    }
    else
    {
        mFinish = ID_TRUE;
        mStarted = ID_FALSE;
        *aNotStarted = ID_FALSE;

        IDE_TEST_RAISE(mCondVar.signal() != IDE_SUCCESS, err_cond_signal);

        IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);

        IDE_TEST_RAISE(join() != 0, err_thr_join);

        mStat.applyFinish();
    }

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

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : ���� �ϰ� �ִ� �۾��� �ִٸ� �۾����� �۾��� ����Ǵ� ���� ���
 *               ����. ���� ���ٸ� �ٷ� �����Ѵ�.
 ******************************************************************************/
void sdbFlusher::wait4OneJobDone()
{
    IDE_ASSERT( mRunningMutex.lock( NULL ) == IDE_SUCCESS );
    IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
}

/******************************************************************************
 * Description :
 *    �� flusher�� ���� ������ �����. �۾����̸� �ƹ��� ������ ���� �ʴ´�.
 *    ���������� ������ aWaken���� ��ȯ�ȴ�.
 * Implementation :
 *    flusher�� �۾����̾����� mRunningMutex�� ��µ� ������ ���̴�.
 *
 *  aWaken  - [OUT] flusher�� ���� �־ �����ٸ� ID_TRUE��,
 *                  flusher�� �۾����̾����� ID_FALSE�� ��ȯ�Ѵ�.
 ******************************************************************************/
IDE_RC sdbFlusher::wakeUpOnlyIfSleeping(idBool *aWaken)
{
    idBool sLocked;

    IDE_ASSERT(mRunningMutex.trylock(sLocked) == IDE_SUCCESS);

    if (sLocked == ID_TRUE)
    {
        IDE_TEST_RAISE(mCondVar.signal() != 0,
                       err_cond_signal);

        IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);

        *aWaken = ID_TRUE;
    }
    else
    {
        *aWaken = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
        IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbFlusher::start()
{
    mFinish = ID_FALSE;
    IDE_TEST(idtBaseThread::start() != IDE_SUCCESS);

    IDE_TEST(idtBaseThread::waitToStart() != IDE_SUCCESS);

    mStat.applyStart();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    flusher �����尡 start�Ǹ� �Ҹ��� �Լ��̴�. flusher�� run����
 *    ���� ������ ���� flusher�� finish()�ϱ� ������ �� �۾��� ��ӵȴ�.
 *    sdbFlushMgr�κ��� job�� ���� ó���ϰ� �����ð� cond_wait�Ѵ�.
 *
 * Implementation :
 *    run()�� ȣ��Ǹ� �Ǹ��� mFinish�� ID_FALSE�� �����Ѵ�. mFinish��
 *    finish()�� ȣ��Ǿ��� ���� ID_TRUE�� ���õǸ�, �ٽ� flusher�� start�ϸ�
 *    run()�� ȣ��Ǹ鼭 mFinish�� ID_FALSE�� ���õȴ�.
 ******************************************************************************/
void sdbFlusher::run()
{
    PDL_Time_Value           sTimeValue;
    sdbFlushJob              sJob;
    IDE_RC                   sRC;
    time_t                   sBeforeWait;
    time_t                   sAfterWait;
    UInt                     sWaitSec;
    ULong                    sFlushedCount = 0;
    idBool                   sMutexLocked  = ID_FALSE;
    sdbReplaceFlushJobParam *sReplaceJobParam;
    sdbObjectFlushJobParam  *sObjJobParam;
    sdbChkptFlushJobParam   *sChkptJobParam;

    IDE_ASSERT(mRunningMutex.lock(NULL) == IDE_SUCCESS);
    sMutexLocked = ID_TRUE;

    mStarted = ID_TRUE;
    while (mFinish == ID_FALSE)
    {
        sFlushedCount = 0;
        sdbFlushMgr::getJob(&mStatistics, &sJob);
        switch (sJob.mType)
        {
            case SDB_FLUSH_JOB_REPLACEMENT_FLUSH:
                mStat.applyReplaceFlushJob();
                if ( IS_USE_DELAYED_FLUSH() == ID_TRUE )
                {
                    IDE_TEST( delayedFlushForReplacement( &mStatistics,
                                                          &sJob,
                                                          &sFlushedCount )
                              != IDE_SUCCESS );
                }
                else
                {
                    sReplaceJobParam = &sJob.mFlushJobParam.mReplaceFlush;
                    IDE_TEST( flushForReplacement( &mStatistics,
                                                   sJob.mReqFlushCount,
                                                   sReplaceJobParam->mFlushList,
                                                   sReplaceJobParam->mLRUList,
                                                   &sFlushedCount )
                              != IDE_SUCCESS );
                }
                mStat.applyReplaceFlushJobDone();
                break;
            case SDB_FLUSH_JOB_CHECKPOINT_FLUSH:
                mStat.applyCheckpointFlushJob();
                sChkptJobParam = &sJob.mFlushJobParam.mChkptFlush;
                IDE_TEST(flushForCheckpoint( &mStatistics,
                                             sJob.mReqFlushCount,
                                             sChkptJobParam->mRedoPageCount,
                                             sChkptJobParam->mRedoLogFileCount,
                                             sChkptJobParam->mCheckpointType, 
                                             &sFlushedCount )
                         != IDE_SUCCESS);
                mStat.applyCheckpointFlushJobDone();
                break;
            case SDB_FLUSH_JOB_DBOBJECT_FLUSH:
                mStat.applyObjectFlushJob();
                sObjJobParam = &sJob.mFlushJobParam.mObjectFlush;
                IDE_TEST(flushDBObjectBCB(&mStatistics,
                                          sObjJobParam->mFiltFunc,
                                          sObjJobParam->mFiltObj,
                                          sObjJobParam->mBCBQueue,
                                          &sFlushedCount)
                         != IDE_SUCCESS);
                mStat.applyObjectFlushJobDone();
                break;
            default:
                break;
        }

        if (sJob.mType != SDB_FLUSH_JOB_NOTHING)
        {
            sdbFlushMgr::updateLastFlushedTime();
            sdbFlushMgr::notifyJobDone(sJob.mJobDoneParam);
        }

        sWaitSec = getWaitInterval(sFlushedCount);
        if (sWaitSec == 0)
        {
            IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);
            IDE_ASSERT(mRunningMutex.lock(NULL) == IDE_SUCCESS);
            continue;
        }

        sBeforeWait = idlOS::time(NULL);
        // sdbFlushMgr�κ��� �󸶳� ���� �˾ƿͼ� time value�� �����Ѵ�.
        sTimeValue.set(sBeforeWait + sWaitSec);
        /* Timed out ���� */
        sRC = mCondVar.timedwait(&mRunningMutex, &sTimeValue, IDU_IGNORE_TIMEDOUT);
        sAfterWait = idlOS::time(NULL);

        mStat.applyTotalSleepSec(sAfterWait - sBeforeWait);

        if (sRC == IDE_SUCCESS)
        {
            if(mCondVar.isTimedOut() == ID_TRUE)
            {
                mStat.applyWakeUpsByTimeout();
            }
            else
            {
                // cond_signal�� �ް� ��� ���
                mStat.applyWakeUpsBySignal();
            }
        }
        else
        {
            // �׿��� ��쿡�� ������ ó���� �Ѵ�.
            ideLog::log( IDE_SM_0, "Flusher Dead [RC:%d, ERRNO:%d]", sRC, errno );
            IDE_RAISE(err_cond_wait);
        }
    }

    sMutexLocked = ID_FALSE;
    IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);

    return;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    if (sMutexLocked == ID_TRUE)
    {
        IDE_ASSERT(mRunningMutex.unlock() == IDE_SUCCESS);
    }

    mStarted = ID_FALSE;
    mFinish = ID_TRUE;

    IDE_ASSERT( 0 );

    return;
}

/******************************************************************************
 * Description :
 *   PROJ-2669
 *    Delayed Flush ����� ���� Normal/Delayed flush list�� ������ ���� flush �Ѵ�.
 *    Normal/Delayed Flush Job �� ������ Ȯ���ϰ�
 *    Normal Flush -> Delayed Flush ������ Flush ������ �����Ѵ�.
 *
 *  aStatistics     - [IN]  �������
 *  aNormalFlushJob - [IN]  Normal flush �� ���� ����
 *  aRetFlushedCount- [OUT] ���� flush�� page ����
 ******************************************************************************/
IDE_RC sdbFlusher::delayedFlushForReplacement( idvSQL                  *aStatistics,
                                               sdbFlushJob             *aNormalFlushJob,
                                               ULong                   *aRetFlushedCount)
{
    sdbFlushJob              sDelayedFlushJob;
    ULong                    sNormalFlushedCount;
    ULong                    sDelayedFlushedCount;
    sdbReplaceFlushJobParam *sNormalJobParam;
    sdbReplaceFlushJobParam *sDelayedJobParam;
    UInt                     sFlushListLength;

    *aRetFlushedCount = 0;
    sNormalFlushedCount = 0;
    sDelayedFlushedCount = 0;

    sNormalJobParam = &aNormalFlushJob->mFlushJobParam.mReplaceFlush;

    /* PROJ-2669
     * ���� Flusher�� Flush ���� ���� ��İ� �����ϰ� ����Ѵ�.
     * (AS-IS: Flush List�� ���̸� �ִ� Flush ���� �����Ѵ�. - BUG-22386 ����) */

    /* 1. Flush ��� Flush List�� Total Length(Normal + Delayed) �� Ȯ���Ѵ�.  */
    sFlushListLength = ( aNormalFlushJob->mReqFlushCount == SDB_FLUSH_COUNT_UNLIMITED )
        ? mPool->getFlushListLength( sNormalJobParam->mFlushList->getID() )
        : aNormalFlushJob->mReqFlushCount;

    /* 2. Delayed Flush�� �����Ѵ�.                                            */
    sdbFlushMgr::getDelayedFlushJob( aNormalFlushJob, &sDelayedFlushJob );
    sDelayedJobParam = &sDelayedFlushJob.mFlushJobParam.mReplaceFlush;

    if ( sDelayedFlushJob.mType != SDB_FLUSH_JOB_NOTHING )
    {
        /* TC/FIT/Limit/sm/sdb/sdbFlusher_delayedFlushForReplacement_delayedFlush.sql */
        IDU_FIT_POINT( "sdbFlusher::delayedFlushForReplacement::delayedFlush" );

        IDE_TEST( flushForReplacement( aStatistics,
                                       sDelayedFlushJob.mReqFlushCount,
                                       sDelayedJobParam->mFlushList,
                                       sDelayedJobParam->mLRUList,
                                       &sDelayedFlushedCount)
                  != IDE_SUCCESS );

        *aRetFlushedCount += sDelayedFlushedCount;
    }
    else
    {
        /* nothing to do */
    }

    /* 3. 1���� ����� Total Length�� Normal Flush�� ������ ������ ũ�ٸ�
     * Total Length - Delayed Flush ���� ��ŭ�� �� Flush �ؾ� �Ѵ�.             */
    if ( sFlushListLength > sDelayedFlushedCount )
    {
        aNormalFlushJob->mReqFlushCount = sFlushListLength - sDelayedFlushedCount;
    }
    else
    {
        aNormalFlushJob->mType = SDB_FLUSH_JOB_NOTHING;
    }


    /* 4. Normal Flush �� �����Ѵ�.
     * flushForReplacement() �Լ� ���ο��� Flush List ������ŭ Flush �Ѵ�.     */
    if ( aNormalFlushJob->mType != SDB_FLUSH_JOB_NOTHING )
    {
        IDE_TEST( flushForReplacement( aStatistics,
                    aNormalFlushJob->mReqFlushCount,
                    sNormalJobParam->mFlushList,
                    sNormalJobParam->mLRUList,
                    &sNormalFlushedCount)
                != IDE_SUCCESS );

        *aRetFlushedCount  += sNormalFlushedCount;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    flush list�� ���� flush�۾��� �����Ѵ�.
 *    flush list�� BCB�� �߿� ���� ������ �����ϸ� flush�� �����Ѵ�.
 *     1. touch count�� BUFFER_HOT_TOUCH_COUNT�� ���� �ʴ°�
 *        -> ���� ���ϸ� LRU mid point�� �ִ´�.
 *     2. S-latch�� ȹ���� �� �վ�� �Ѵ�.
 *        -> ���� ���ϸ� LRU mid point�� �ִ´�.
 *     3. state�� dirty�̾�� �Ѵ�.
 *        -> INIOB, REDIRTY�� �͵��� skip, CLEAN�� �͵��� prepare list�� �ű��.
 *    ���� flush ������ �� ���� ��ܿ� �ִ� Implementation �ּ��� ������ ��.
 *    flush�� ���۵��� ��� prepare list�� �ű��.
 *
 *  aStatistics     - [IN]  �� list �̵��� mutex ȹ���� �ʿ��ϱ� ������
 *                          ��������� �ʿ��ϴ�.
 *  aReqFlushCount  - [IN]  ����ڰ� ��û�� flush����
 *  aFlushList      - [IN]  flush�� flush list
 *  aLRUList        - [IN]  touch count�� BUFFER_HOT_TOUCH_COUNT �̻��̰ų�
 *                          X-latch�� �����ִ� ���۵��� �ű� LRU list
 *  aRetFlushedCount- [OUT] ���� flush�� page ����
 ******************************************************************************/
IDE_RC sdbFlusher::flushForReplacement(idvSQL         * aStatistics,
                                       ULong            aReqFlushCount,
                                       sdbFlushList   * aFlushList,
                                       sdbLRUList     * aLRUList,
                                       ULong          * aRetFlushedCount)
{
    idBool          sLocked;
    sdbFlushList  * sDelayedFlushList   = mPool->getDelayedFlushList( aFlushList->getID() );
    sdbBCB        * sBCB;
    sdsBCB        * sSBCB;
    ULong           sFlushCount         = 0;
    idBool          sMoveToSBuffer      = ID_FALSE;
    idBool          sIsUnderMaxLength;
    idBool          sIsHotBCB;
    idBool          sIsDelayedFlushList;

    IDE_DASSERT(aFlushList != NULL);
    IDE_DASSERT(aReqFlushCount > 0);

    *aRetFlushedCount = 0;

    sIsDelayedFlushList = (idBool)IS_DELAYED_FLUSH_LIST( aFlushList );

    // flush list�� Ž���ϱ� ���ؼ� beginExploring()�� �Ѵ�.
    aFlushList->beginExploring(aStatistics);

    // BUG-22386 flush list�� dirty page�� �߰��Ǵ� �ӵ��� list��
    // dirty page�� flush�ϴ� �ӵ����� �� ����, flusher�� �ϳ���
    // flush list�� ����� ���� �۾����� �Ѿ�� ���ϴ� ������
    // �ֽ��ϴ�. �׷��� ������ ���� ��ŭ�� flush�ϰ� flush���߿�
    // �߰��� page�� flush���� �ʽ��ϴ�.
    if ( aReqFlushCount > aFlushList->getPartialLength() )
    {
        aReqFlushCount = aFlushList->getPartialLength();
    }
    else
    {
        /* nothing to do */
    }

     
    // aReqFlushCount��ŭ flush�Ѵ�.
    while (sFlushCount < aReqFlushCount)
    {
        // flush list���� �ϳ��� BCB�� ����.
        // �̶� ����Ʈ���� �������� �ʴ´�.
        sBCB = aFlushList->getNext(aStatistics);

        if (sBCB == NULL)
        {
            // ���̻� flush�� BCB�� �����Ƿ� ������ ����������.
            break;
        }


        if ( ( sIsDelayedFlushList == ID_FALSE ) &&
             ( mPool->isHotBCB( sBCB ) == ID_TRUE ) )
        {
            // touch count�� BUFFER_HOT_TOUCH_COUNT �̻��̰ų�
            // X-latch�� �����ִ� ���۵��� ������ LRU mid point�� �ű��.
            aFlushList->remove(aStatistics, sBCB);
            aLRUList->insertBCB2BehindMid(aStatistics, sBCB);

            mStat.applyReplaceSkipPages();
            continue;
        }
        else
        {
            /* Nothing to do */
        }


        if ( IS_USE_DELAYED_FLUSH() == ID_TRUE )
        {
            /* PROJ-2669 Flsuh now or Delay Case
             *        | sIsUnderMaxLength | sIsHotBCB | Doing what
             * -------+-------------------+-----------+------------------------
             * CASE 1 |         1         |     1     | Delayed Flush List Add
             * CASE 2 |         1         |     0     | Flush
             * CASE 3 |         0         |     1     | Flush
             * CASE 4 |         0         |     0     | Flush
             */

            sIsUnderMaxLength = sDelayedFlushList->isUnderMaxLength();
            sIsHotBCB = isHotBCBByFlusher( sBCB );

            /* CASE 3 only for statistics */
            if ( ( sIsUnderMaxLength == ID_FALSE ) && ( sIsHotBCB == ID_TRUE ) )
            {
                if ( sIsDelayedFlushList == ID_FALSE )
                {
                    mStat.applyReplacementOverflowDelayedPages();
                }
                else
                {
                    /* No statistics */
                }
            }
            else
            {
                /* Nothing to do */
            }

            /* CASE 1 */
            if ( ( sIsUnderMaxLength == ID_TRUE ) &&
                 ( sIsHotBCB == ID_TRUE ) )
            {
                /* Touch ������ �ʱ�ȭ �Ͽ�
                 * ������ flush ���� ������ Touch�� �ִ� ��쿡��
                 * �ٽ� Delayed flush �ǵ��� �Ѵ�.                  */
                sBCB->mTouchCnt = 1;
                IDV_TIME_INIT( &sBCB->mLastTouchedTime );

                if ( sIsDelayedFlushList == ID_FALSE )
                {
                    aFlushList->remove( aStatistics, sBCB );
                    sDelayedFlushList->add( aStatistics, sBCB );
                    mStat.applyReplaceAddDelayedPages();
                }
                else
                {
                    mStat.applyReplaceSkipDelayedPages();
                }
                continue;
            }
            else
            {
                /* Nothing to do */
            }

            /* CASE 2, 4
             * ���̻� Delayed Flush �� ���ؼ� �ؾ� �� ���� ����.
             * ���� �۾��� ��� �����Ѵ�.                        */
        }
        else
        {
            /* Nothing to do */
        }

        // S-latch�� try�غ���.
        // �����ϸ� X-latch�� �̹� �����ִٴ� �ǹ��̴�.
        // �̰�쿡�� LRU mid point�� �ִ´�.
        sBCB->tryLockPageSLatch( &sLocked );
        if ( sLocked == ID_FALSE )
        {
            aFlushList->remove( aStatistics, sBCB );
            aLRUList->insertBCB2BehindMid( aStatistics, sBCB );

            mStat.applyReplaceSkipPages();
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        // BCB state �˻縦 �ϱ� ���� BCBMutex�� ȹ���Ѵ�.
        sBCB->lockBCBMutex(aStatistics);

        if ((sBCB->mState == SDB_BCB_REDIRTY) ||
            (sBCB->mState == SDB_BCB_INIOB)   ||
            (sBCB->mState == SDB_BCB_FREE))
        {
            // REDIRTY, INIOB�� ���۴� skip, prepare list��
            // �ű��.

            sBCB->unlockBCBMutex();
            sBCB->unlockPageLatch();

            if( sBCB->mState == SDB_BCB_FREE )
            {
                aFlushList->remove(aStatistics, sBCB);
                mPool->addBCB2PrepareLst(aStatistics, sBCB);
            }
            mStat.applyReplaceSkipPages();
            continue;
        }
        else
        {
           IDE_ASSERT( ( sBCB->mState == SDB_BCB_DIRTY ) ||
                       ( sBCB->mState == SDB_BCB_CLEAN ) );

            if( needToSkipBCB( sBCB ) == ID_TRUE )
            {
                /* Secondary Buffer�� movedown ����� �ƴ�
                   CLEAN page��  prepare list��  �ű��. */
                sBCB->unlockBCBMutex();
                sBCB->unlockPageLatch();

                aFlushList->remove(aStatistics, sBCB);
                mPool->addBCB2PrepareLst(aStatistics, sBCB);

                mStat.applyReplaceSkipPages();
                continue;
            }
            else 
            {
                /* nothing to do */
            }
        }

        // flush ������ ��� �����ߴ�.
        // ���� flush�� ����.
        sBCB->mPrevState = sBCB->mState;
        sBCB->mState     = SDB_BCB_INIOB;
        //����� SBCB�� �ִٸ� delink
        sSBCB = sBCB->mSBCB;
        if( sSBCB != NULL )
        { 
            /* delink �۾��� lock���� ����ǹǷ�
               victim �߻����� ���� ���� ������ ������ �ִ�.
               pageID ���� �ٸ��ٸ� �̹� free �� ��Ȳ�ϼ� �ִ�. */
            if (( sBCB->mSpaceID == sSBCB->mSpaceID ) &&
                ( sBCB->mPageID  == sSBCB->mPageID ))
            {
                (void)sdsBufferMgr::removeBCB( aStatistics,
                                               sSBCB );
            }
            else
            {
                /* ���⼭ sSBCB �� �����ϴ� ������
                   secondary buffer�� �������� �ִ� old page�� �̸������ϱ� �����ε�
                   pageID ���� �ٸ��ٸ� (victim ������ ����) �ٸ� �������̹Ƿ�
                   ��������� �ƴϴ�. */
            }
        }
        else
        {
            /* nothing to do */
        }

        sBCB->unlockBCBMutex();

        aFlushList->remove(aStatistics, sBCB);
    
        if( mServiceable == ID_TRUE )
        {
            sMoveToSBuffer = ID_TRUE;
        }

        if( SM_IS_LSN_INIT(sBCB->mRecoveryLSN) ) 
        {
            if( sMoveToSBuffer == ID_TRUE ) 
            {
                /* secondary Buffer �� ����Ҷ���
                   temppage�� secondary Buffer �� �����Ѵ�. */
                IDE_TEST( copyTempPage( aStatistics,
                                        sBCB,
                                        ID_TRUE,         // move to prepare
                                        sMoveToSBuffer ) // move to Secondary
                          != IDE_SUCCESS);
            }
            else 
            {
                // temp page�� ��� double write�� �� �ʿ䰡 ���⶧����
                // IOB�� ������ �� �ٷ� �� �������� disk�� ������.
                IDE_TEST( flushTempPage( aStatistics,
                                         sBCB,
                                         ID_TRUE )  // move to prepare
                          != IDE_SUCCESS);
            }
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics,
                                 sBCB,
                                 ID_TRUE,         // move to prepare
                                 sMoveToSBuffer ) // move to Secondary
                      != IDE_SUCCESS);
        }
        mStat.applyReplaceFlushPages();

        sFlushCount++;
    }

    // IOB�� ���� ���������� ��ũ�� ������.
    IDE_TEST( writeIOB( aStatistics,
                        ID_TRUE, // move to prepare
                        sMoveToSBuffer ) // move to Secondary buffer     
             != IDE_SUCCESS);

    // flush Ž���� �������� �˸���.
    aFlushList->endExploring(aStatistics);

    *aRetFlushedCount = sFlushCount;

    /* PROJ-2669 Delayed Flush Statistics */
    if ( sIsDelayedFlushList == ID_TRUE )
    {
        mStat.applyTotalFlushDelayedPages( sFlushCount );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    CPListSet�� ���۵��� recovery LSN�� ���� ������ flush�� �����Ѵ�.
 *    flush ������ DIRTY �����̱⸸ �ϸ� �ȴ�.
 *    DIRTY�� �ƴ� BCB�� skip�Ѵ�.
 *    replacement flush������ S-latch�� try������ ���⼭��
 *    ���������� ��⸦ �Ѵ�. checkpoint flush�� ���Ұ� ����,
 *    ������� flush�ϴ°� ȿ�����̱� �����̴�.
 *
 *    aReqFlushCount �̻�, aReqFlushLSN ����,
 *    Chkpt list pages �� aMaxSkipPageCnt ����,
 *    ������ ������ ��� �����Ҷ����� flush������
 *    �۾� ���۽� CPList�� ������ Page�� LSN�� �̸� Ȯ���Ͽ�
 *    �� �̻��� flush���� �ʴ´�.
 *
 *  aStatistics         - [IN]  �������
 *  aMinFlushCount      - [IN]  flush�� �ּ� Page ��
 *  aRedoDirtyPageCnt   - [IN]  Restart Recovery�ÿ� Redo�� Page ��,
 *                              �ݴ�� ���ϸ� CP List�� ���ܵ� Dirty Page�� ��
 *  aRedoLogFileCount   - [IN]  Restart Recovery�ÿ� redo�� LogFile ��
 *  aRetFlushedCount    - [OUT] ���� flush�� page ����
 ******************************************************************************/
IDE_RC sdbFlusher::flushForCheckpoint( idvSQL          * aStatistics,
                                       ULong             aMinFlushCount,
                                       ULong             aRedoPageCount,
                                       UInt              aRedoLogFileCount,
                                       sdbCheckpointType aCheckpointType,
                                       ULong           * aRetFlushedCount )
{
    sdbBCB        * sBCB;
    sdsBCB        * sSBCB;
    ULong           sFlushCount     = 0;
    ULong           sReqFlushCount  = 0;
    ULong           sNotDirtyCount  = 0;
    ULong           sLockFailCount  = 0;
    smLSN           sReqFlushLSN;
    smLSN           sMaxFlushLSN;
    idBool          sLocked;
    UShort          sActiveFlusherCount;

    IDE_ASSERT( aRetFlushedCount != NULL );

    *aRetFlushedCount = 0;

    /* checkpoint flush�� �ִ� flush�� �� �ִ� LSN�� ��´�.
     * �� LSN�̻��� flush���� �ʴ´�. */
    mCPListSet->getMaxRecoveryLSN(aStatistics, &sMaxFlushLSN);

    /* last LSN�� �����´�, Disk�Ӹ� �ƴ϶� Memory �����ؼ� ����
     * ������ LSN�̴�. �׷��� LogFile�� ���� �� �� �ִ�. */
    smLayerCallback::getLstLSN( &sReqFlushLSN );

    if( sReqFlushLSN.mFileNo >= aRedoLogFileCount )
    {
        sReqFlushLSN.mFileNo -= aRedoLogFileCount ;
    }
    else
    {
        SM_LSN_INIT( sReqFlushLSN );
    }

    /* flush ��û�� page�� ���� ����Ѵ�. */
    sReqFlushCount = mCPListSet->getTotalBCBCnt();

    if( sReqFlushCount > aRedoPageCount )
    {
        /* CP list�� Dirty Page���� Restart Recovery�ÿ�
         * Redo �� Page ���� ���������Ƿ� �� ���̸� flush�Ѵ�. */
        sReqFlushCount -= aRedoPageCount ;

        /* ���� flusher�� ���ÿ� �۵� ���̹Ƿ�
         * flush�� page�� �۵����� flusher���� 1/n�� ������. */
        sActiveFlusherCount = sdbFlushMgr::getActiveFlusherCount();
        IDE_ASSERT( sActiveFlusherCount > 0 );
        sReqFlushCount /= sActiveFlusherCount;

        if( aMinFlushCount > sReqFlushCount )
        {
            sReqFlushCount = aMinFlushCount;
        }
    }
    else
    {
        /* CP list�� Dirty Page���� Restart Recovery�ÿ�
         * Redo �� Page ���� �۴�. MinFlushCount ��ŭ�� flush�Ѵ�. */
        sReqFlushCount = aMinFlushCount;
    }

    /* CPListSet���� recovery LSN�� ���� ���� BCB�� ����.
     * ������ �� BCB�� ���� ���� CPListSet���� ���� ���� �ִ�. (!!!) */
    sBCB = (sdbBCB *)mCPListSet->getMin();

    while (sBCB != NULL)
    {
        if ( smLayerCallback::isLSNLT( &sMaxFlushLSN,
                                       &sBCB->mRecoveryLSN )
             == ID_TRUE )
        {
            break;
        }

        /* BUG-40138
         * flusher�� ���� checkpoint flush�� ����Ǵ� ����
         * �ٸ� �켱 ������ ���� job�� �����ؾ��ϴ� ������ �˻��Ѵ�.
         * checkpoint thread�� ���� ��û�� checkpoint flush�� �ش� ���� �ʴ´�.*/
        if ( ((sFlushCount % smuProperty::getFlusherBusyConditionCheckInterval()) == 0) &&
             (aCheckpointType == SDB_CHECKPOINT_BY_FLUSH_THREAD) &&
             (sdbFlushMgr::isBusyCondition() == ID_TRUE) )
        {
            break;
        }

        if ( ( sFlushCount >= sReqFlushCount ) &&
             ( smLayerCallback::isLSNGT( &sBCB->mRecoveryLSN,
                                         &sReqFlushLSN ) == ID_TRUE) )
        {
            /* sReqFlushCount �̻� flush�Ͽ���
             * sReqFlushLSN���� flush�Ͽ����� flush �۾��� �ߴ��Ѵ�. */
            break;
        }

        /* ���� Flush �κ� */
        sBCB->tryLockPageSLatch(&sLocked);

        if (sLocked == ID_FALSE)
        {
            sLockFailCount++;
            if (sLockFailCount > mCPListSet->getListCount())
            {
                break; /* checkpoint flush�� �ߴ��Ѵ�. */
            }
            sBCB = (sdbBCB *)mCPListSet->getNextOf( sBCB );
            mStat.applyCheckpointSkipPages();
            continue;
        }
        sLockFailCount = 0;

        sBCB->lockBCBMutex(aStatistics);

        if (sBCB->mState != SDB_BCB_DIRTY)
        {
            /* DIRTY�� �ƴ� ���۴� skip�Ѵ�.
             * ���� BCB�� �����Ѵ�. */
            sBCB->unlockBCBMutex();
            sBCB->unlockPageLatch();

            /* ��� checkpoint list�� ù��° BCB�� dirty�� �ƴ� ���
             * ���ѷ����� ���� �� �ִ�.
             * �÷��� �ڽ��� InIOB �ΰ����� �ִ� �������� ���� ��ũ��
             * �ݿ����� ���� ���¿���, redirty�� �Ǵ� ��Ȳ�� ������ ����,
             * ��� ���⼭ ���� ������ ���� �ȴ�. �� ��Ȳ�� �����ϱ� ����
             * list ��ȸ�� list������ŭ �����Ͽ�����, �ڽ��� ������ �ִ� IOB��
             * ��� ���۸� write�� �� ��� flush�� �����Ѵ�.
             * ������ ���� ��Ȳ�� ���� �߻����� �ʵ��� �Ǿ��ִ�.
             * �ֳ��ϸ�, flush����� �Ǵ� BCB�� flush�� ���۵Ǿ������� LSN����
             * mRecvLSN�� ���� BCB�̱� �����̴�.
             * ���� InIOB�Ǿ��ִ� ���Ŀ� redirty�Ǿ��ٸ�, �� BCB�� mRecvLSN��
             * flush�� ���۵Ǿ������� LSN���� ũ�⶧���̴�. */
            sNotDirtyCount++;
            if (sNotDirtyCount > mCPListSet->getListCount())
            {
                IDE_TEST( writeIOB( aStatistics,
                                    ID_FALSE,  /* MOVE TO PREPARE */
                                    ID_FALSE ) /* move to Secondary buffer */
                         != IDE_SUCCESS);
                sNotDirtyCount = 0;
            }
            /* nextBCB()�� ���ؼ� ���� BCB���� �����ϴ� ����
             * CPListSet���� ���� �� �ִ�.
             * ���� nextBCB()�� ���ڷ� ���� sBCB����
             * �� �������� CPList���� ���� �� �ִ�.
             * ���� �׷� ����� nextBCB�� minBCB�� ������ ���̴�. */
            sBCB = (sdbBCB *)mCPListSet->getNextOf( sBCB );
            mStat.applyCheckpointSkipPages();
            continue;
        }
        sNotDirtyCount = 0;

        /* �ٸ� flusher���� flush�� ���ϰ� �ϱ� ����
         * INIOB���·� �����Ѵ�. */
        sBCB->mState = SDB_BCB_INIOB;
        //����� SBCB�� �ִٸ� delink
        sSBCB = sBCB->mSBCB;
        if( sSBCB != NULL )
        { 
            /* delink �۾��� lock���� ����ǹǷ�
               victim �߻����� ���� ���� ������ ������ �ִ�.
               pageID ���� �ٸ��ٸ� �̹� free �� ��Ȳ�ϼ� �ִ�. */
            if( (sBCB->mSpaceID == sSBCB->mSpaceID ) &&
                (sBCB->mPageID == sSBCB->mPageID ) )
            {
                (void)sdsBufferMgr::removeBCB( aStatistics,
                                               sSBCB );
            }
            else
            {
                /* ���⼭ sSBCB �� �����ϴ� ������
                   checkpoint�� ���� secondary buffer�� ������ �ʰ� disk�� ������ �̹�����
                   sSBCB�� �ִ� �������� ���� ���� ���ϰ� �ϱ� ���� �ε�
                   pageID ���� �ٸ��ٸ� (victim ������ ����) �ٸ� �������̹Ƿ�
                   ��������� �ƴϴ�. */
            }
        }
        else
        {
            /* nothing to do */
        }

        sBCB->unlockBCBMutex();

        if (SM_IS_LSN_INIT(sBCB->mRecoveryLSN))
        {
            /* TEMP PAGE�� check point ����Ʈ�� �޷� ������ �ȵȴ�.
             * �׷����� ������ ������ �ʱ� ������, release�ÿ� �߰ߵǴ���
             * ������ �ʵ��� �Ѵ�.
             * TEMP PAGE�� �ƴϴ���, mRecoveryLSN�� init���̶�� ����
             * recovery�� ������� ��������� ���̴�.
             * recovery�� ������� �������� checkpoint�� ���� �ʾƵ� �ȴ�. */

            IDE_DASSERT( 0 );
            IDE_TEST( flushTempPage( aStatistics,
                                     sBCB,
                                     ID_FALSE )  /* move to prepare */
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics,
                                 sBCB,
                                 ID_FALSE,   /* prepare list�� ��ȯ���� �ʴ´�. */
                                 ID_FALSE )  /* move to Secondary buffer */ 
                     != IDE_SUCCESS);

        }
        mStat.applyCheckpointFlushPages();

        sFlushCount++;

        /* ���� flush�� BCB�� ���Ѵ�. */
        sBCB = (sdbBCB*)mCPListSet->getMin();
    }

    *aRetFlushedCount = sFlushCount;

    /* IOB�� ���� �ִ� ���۵��� ��� disk�� ����Ѵ�. */
    IDE_TEST( writeIOB( aStatistics,
                        ID_FALSE,  /* move to prepare */
                        ID_FALSE ) /* move to Secondary buffer */
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  aBCBQueue�� ����ִ� BCB�� ������� flush�� �����Ѵ�.
 *  �̶�, Queue�� ���� ��� BCB�߿� aFiltFunc������ �����ϴ� BCB�� ���ؼ���
 *  flush�� �����Ѵ�.
 *
 *  ���� ����! aBCBQueue�� �ִ� BCB�� ���� Ǯ�� � ��ġ���� ���� �� �ִ�.
 *
 *  aStatistics     - [IN]  �������.
 *  aFiltFunc       - [IN]  �÷��� ���� ���� ���� �Լ�.
 *  aFiltObj        - [IN]  aFiltFunc�� �Ѱ��� ����.
 *  aBCBQueue       - [IN]  �÷��� �ؾ��� BCB���� �����Ͱ� ����ִ� ť.
 *  aRetFlushedCount- [OUT] ���� �÷��� �� ������ ����.
 ******************************************************************************/
IDE_RC sdbFlusher::flushDBObjectBCB( idvSQL           * aStatistics,
                                     sdbFiltFunc        aFiltFunc,
                                     void             * aFiltObj,
                                     smuQueueMgr      * aBCBQueue,
                                     ULong            * aRetFlushedCount )
{
    sdbBCB        * sBCB        = NULL;
    sdsBCB        * sSBCB       = NULL;
    idBool          sEmpty      = ID_FALSE;
    idBool          sIsSuccess  = ID_FALSE;
    PDL_Time_Value  sTV;
    ULong           sRetFlushedCount = 0;

    while (1)
    {
        IDE_ASSERT( aBCBQueue->dequeue( ID_FALSE, //mutex ���� �ʴ´�.
                                        (void*)&sBCB,
                                        &sEmpty)
                    == IDE_SUCCESS );

        if ( sEmpty == ID_TRUE )
        {
            // ���̻� flush�� BCB�� �����Ƿ� ������ ����������.
            break;
        }
retry:
        sBCB->tryLockPageSLatch(&sIsSuccess);
        if( sIsSuccess == ID_FALSE )
        {
            // lock�� ���� ���ߴٴ� ���� ������ �����ϰ� �ִٴ°�!!
            // sBCB�� aFiltFunc�� ������Ű�� ���ϸ� �÷��ø� ���� �ʴ´�.
            if( aFiltFunc( sBCB, aFiltObj ) == ID_TRUE )
            {
                // ������ ������ �����Ѵٸ� latch�� ���� �� ������ ���� ����Ѵ�.
                sBCB->lockPageSLatch(aStatistics);
            }
            else
            {
                mStat.applyObjectSkipPages();
                continue;
            }
        }

        // BCB state �˻縦 �ϱ� ���� BCBMutex�� ȹ���Ѵ�.
        sBCB->lockBCBMutex(aStatistics);

        if ((sBCB->mState == SDB_BCB_CLEAN) ||
            (sBCB->mState == SDB_BCB_FREE))
        {
            sBCB->unlockBCBMutex();
            sBCB->unlockPageLatch();
            mStat.applyObjectSkipPages();
            continue;
        }

        if( aFiltFunc( sBCB, aFiltObj ) == ID_FALSE )
        {
            // BCB�� aFiltFunc������ �������� ���ϸ� �÷��� ���� �ʴ´�.
            sBCB->unlockBCBMutex();
            sBCB->unlockPageLatch();
            mStat.applyObjectSkipPages();
            continue;
        }

        // BUG-21135 flusher�� flush�۾��� �Ϸ��ϱ� ���ؼ���
        // INIOB������ BCB���°� ����Ǳ� ��ٷ��� �մϴ�.
        if( (sBCB->mState == SDB_BCB_REDIRTY ) ||
            (sBCB->mState == SDB_BCB_INIOB))
        {
            sBCB->unlockBCBMutex();
            sBCB->unlockPageLatch();
            // redirty�� ��� dirty�� �ɶ����� ��ٷȴٰ��ؾ���
            // dirty�� �Ǹ� flush�ؾ���
            // 50000�� ������ ����.. �ʹ� ������ cpu�ð��� ���� ��Ƹ԰�,
            // �ʹ� ũ�� ���ð��� ����� �� �ִ�.
            sTV.set(0, 50000);
            idlOS::sleep(sTV);
            goto retry;
        }
        // flush ������ ��� �����ߴ�.
        // ���� flush�� ����.
        sBCB->mState = SDB_BCB_INIOB;
        //����� SBCB�� �ִٸ� delink
        sSBCB = sBCB->mSBCB;
        if( sSBCB != NULL )
        { 
            /* delink �۾��� lock���� ����ǹǷ�
               victim �߻����� ���� ���� ������ ������ �ִ�.
               pageID ���� �ٸ��ٸ� �̹� free �� ��Ȳ�ϼ� �ִ�. */
            if( (sBCB->mSpaceID == sSBCB->mSpaceID ) &&
                (sBCB->mPageID == sSBCB->mPageID ) )
            {
                (void)sdsBufferMgr::removeBCB( aStatistics,
                                               sSBCB );
            }
            else
            {
                /* ���⼭ sSBCB �� �����ϴ� ������
                   flush�۾����� ���� secondary buffer�� ������ �ʰ� disk�� ������ �̹�����
                   sSBCB�� �ִ� �������� ���� ���� ���ϰ� �ϱ� ���� �ε�
                   pageID ���� �ٸ��ٸ� (victim ������ ����) �ٸ� �������̹Ƿ�
                   ��������� �ƴϴ�. */
            }
        }
        else
        {
            /* nothing to do */
        }

        sBCB->unlockBCBMutex();

        if (SM_IS_LSN_INIT(sBCB->mRecoveryLSN))
        {
            // temp page�� ��� double write�� �� �ʿ䰡 ���⶧����
            // IOB�� ������ �� �ٷ� �� �������� disk�� ������.
            IDE_TEST( flushTempPage( aStatistics,
                                     sBCB,
                                     ID_FALSE ) // move to prepare
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics,
                                 sBCB,
                                 ID_FALSE,  // prepare list�� ��ȯ���� �ʴ´�.
                                 ID_FALSE ) // move to Secondary buffer     
                     != IDE_SUCCESS);
        }
        mStat.applyObjectFlushPages();
        sRetFlushedCount++;
    }

    // IOB�� ��ũ�� ����Ѵ�.
    IDE_TEST( writeIOB( aStatistics,
                        ID_FALSE,  // move to prepare
                        ID_FALSE ) // move to Secondary buffer                
             != IDE_SUCCESS);

    *aRetFlushedCount = sRetFlushedCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRetFlushedCount = sRetFlushedCount;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    temp page�� ��� flushTempPage ��� �� �Լ��� ����Ѵ�.
 *    flushTempPage�� �ϳ��� IOB�� �ٷ� ��������
      copyTempPage�� copyTOIOB�� ����ϰ� ����
 *    no logging �̱� ������ log flush�� �ʿ����.
 *
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  flush�� BCB
 *  aMoveToPrepare  - [IN]  flush���Ŀ� BCB�� prepare list�� �ű��� ����
 *                          ID_TRUE�̸� flush ���� BCB�� prepare list�� �ű��.
 ******************************************************************************/
IDE_RC sdbFlusher::copyTempPage( idvSQL           * aStatistics,
                                  sdbBCB          * aBCB,
                                  idBool            aMoveToPrepare,
                                  idBool            aMoveToSBuffer )
{
    smLSN      sDummyLSN;
    UChar     *sIOBPtr;
    idvTime    sBeginTime;
    idvTime    sEndTime;
    ULong      sCalcChecksumTime;

    IDE_ASSERT( aBCB->mCPListNo == SDB_CP_LIST_NONE );

    sIOBPtr = mIOBPtr[mIOBPos];

    idlOS::memcpy(sIOBPtr, aBCB->mFrame, mPageSize);
    mIOBBCBArray[mIOBPos] = aBCB;

    aBCB->unlockPageLatch();

    SM_LSN_INIT( sDummyLSN );

    /* BUG-22271: Flusher�� Page�� Disk�� ������ Page�� Chechsum�� Page
     *            Latch�� ��� �ϰ� �ֽ��ϴ�. */
    IDV_TIME_GET(&sBeginTime);
    smLayerCallback::setPageLSN( sIOBPtr, &sDummyLSN );
    smLayerCallback::calcAndSetCheckSum( sIOBPtr );
    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * Checksum ����ϴµ� �ɸ� �ð� ������. */
    mStat.applyTotalCalcChecksumTimeUSec( sCalcChecksumTime );

    mIOBPos++;
    mStat.applyINIOBCount( mIOBPos );

    // IOB�� ����á���� IOB�� ��ũ�� ������.
    if( mIOBPos == mIOBPageCount )
    {
        IDE_TEST( writeIOB( aStatistics, aMoveToPrepare, aMoveToSBuffer )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    temp page�� ��� copyToIOB()��� �� �Լ��� ����Ѵ�.
 *    �� �Լ��� IOB�� ���۸� ������ �� �� IOB�� �ٷ� ��ũ�� ����Ѵ�.
 *    copyToIOB()�� IOB�� ��á�� ���� disk�� ��ϵ�����
 *    flushPage()�� �ϳ��� IOB�� �ٷ� disk�� ����Ѵ�.
 *    no logging �̱� ������ log flush�� �ʿ����.
 *
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  flush�� BCB
 *  aMoveToPrepare  - [IN]  flush���Ŀ� BCB�� prepare list�� �ű��� ����
 *                          ID_TRUE�̸� flush ���� BCB�� prepare list�� �ű��.
 ******************************************************************************/
IDE_RC sdbFlusher::flushTempPage( idvSQL           * aStatistics,
                                  sdbBCB           * aBCB,
                                  idBool             aMoveToPrepare )
{
    SInt       sWaitEventState = 0;
    idvWeArgs  sWeArgs;
    smLSN      sDummyLSN;
    UChar     *sIOBPtr;
    idvTime    sBeginTime;
    idvTime    sEndTime;
    ULong      sTempWriteTime;
    ULong      sCalcChecksumTime;

    IDE_ASSERT( aBCB->mCPListNo == SDB_CP_LIST_NONE );
    IDE_ASSERT( smLayerCallback::isTempTableSpace( aBCB->mSpaceID )
                == ID_TRUE );

    sIOBPtr = mIOBPtr[mIOBPos];

    idlOS::memcpy(sIOBPtr, aBCB->mFrame, mPageSize);

    aBCB->unlockPageLatch();

    SM_LSN_INIT( sDummyLSN );

    /* BUG-22271: Flusher�� Page�� Disk�� ������ Page�� Chechsum�� Page
     *            Latch�� ��� �ϰ� �ֽ��ϴ�. */
    IDV_TIME_GET(&sBeginTime);
    smLayerCallback::setPageLSN( sIOBPtr, &sDummyLSN );
    smLayerCallback::calcAndSetCheckSum( sIOBPtr );

    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    IDV_WEARGS_SET( &sWeArgs, IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_WRITE,
                    0, /* WaitParam1 */
                    0, /* WaitParam2 */
                    0  /* WaitParam3 */ );

    IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );

    sWaitEventState = 1;

    IDV_TIME_GET(&sBeginTime);

    mStat.applyIOBegin();

    IDE_TEST( sddDiskMgr::write( aStatistics,
                                 aBCB->mSpaceID,
                                 aBCB->mPageID,
                                 mIOBPtr[mIOBPos] )
              != IDE_SUCCESS);

    mStat.applyIODone();

    IDV_TIME_GET(&sEndTime);
    sTempWriteTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    aBCB->mWriteCount++;

    sWaitEventState = 0;
    IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );

    aBCB->lockBCBMutex(aStatistics);

    if (aBCB->mState == SDB_BCB_INIOB)
    {
        aBCB->mState = SDB_BCB_CLEAN;
    }
    else
    {
        IDE_DASSERT(aBCB->mState == SDB_BCB_REDIRTY);
        aBCB->mState = SDB_BCB_DIRTY;
    }
    aBCB->unlockBCBMutex();

    if (aMoveToPrepare == ID_TRUE)
    {
        mPool->addBCB2PrepareLst(aStatistics, aBCB);
    }

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * ���� TempPage�� �ѹ��� �� Page�� Write��. */
    mStat.applyTotalFlushTempPages( 1 );
    mStat.applyTotalTempWriteTimeUSec( sTempWriteTime );
    mStat.applyTotalCalcChecksumTimeUSec( sCalcChecksumTime );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sWaitEventState == 1)
    {
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    BCB�� frame ������ IOB�� �����Ѵ�. IOB�� �������� IOB�� disk�� ������.
 *    BCB�� frame�� IOB�� ����� �� IOB�� minRecoveryLSN�� ���ŵȴ�.
 *    ���� ����� �� CPListSet������ ���ŵȴ�.
 *
 * ����!!
 *    CPListSet���� �����ϱ� ���� �ݵ�� IOB�� minRecoveryLSN�� ����
 *    �����ؾ� �Ѵ�.
 *    ���� CPListSet���� �����ϰ� IOB�� minRecoveryLSN�� �����Ѵٸ�, �׵���
 *    üũ����Ʈ �����尡 �߸��� minRecoveryLSN�� ������ �� �ִ�.
 *    �׸���, üũ����Ʈ �����尡 minRecoveryLSN�� �������� ������ �� ���Ѿ�
 *    �ϴµ�, ���� CPListSset���� minRecoveryLSN�� ��������, �״�����
 *    flusher�� InIOB���� minRecoveryLSN�� �����;� �Ѵ�.
 *    ���� ������ �Ųٷ� �Ѵٸ�, minRecoveryLSN�� ��ĥ ���� �ִ�.
 *
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  BCB
 *  aMoveToPrepare  - [IN]  flush���Ŀ� BCB�� prepare list�� �ű��� ����
 *                          ID_TRUE�̸� flush ���� BCB�� prepare list�� �ű��.
 ******************************************************************************/
IDE_RC sdbFlusher::copyToIOB( idvSQL          * aStatistics,
                              sdbBCB          * aBCB,
                              idBool            aMoveToPrepare,
                              idBool            aMoveToSBuffer )
{
    UChar    * sIOBPtr;
    smLSN      sPageLSN;
    scGRID     sPageGRID;
    idvTime    sBeginTime;
    idvTime    sEndTime;
    ULong      sCalcChecksumTime;

    IDE_ASSERT( mIOBPos < mIOBPageCount );
    IDE_ASSERT( aBCB->mCPListNo != SDB_CP_LIST_NONE );

    /* PROJ-2162 RestartRiskReduction
     * Consistency ���� ������, Flush�� ���´�.  */
    if( ( smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
        ( smuProperty::getCrashTolerance() != 2 ) )
    {
        /* Checkpoint Linst���� BCB�� �����ϰ� Mutex��
         * Ǯ���ֱ⸸ �Ѵ�. Flush�� IOB ����� ���� */
        mCPListSet->remove( aStatistics, aBCB );
        aBCB->unlockPageLatch();
    }
    else
    {
        // IOB�� minRecoveryLSN�� �����ϱ� ���ؼ� mMinRecoveryLSNMutex�� ȹ���Ѵ�.
        IDE_ASSERT( mMinRecoveryLSNMutex.lock(aStatistics) == IDE_SUCCESS );
    
        if ( smLayerCallback::isLSNLT( &aBCB->mRecoveryLSN, 
                                       &mMinRecoveryLSN )
             == ID_TRUE)
        {
            SM_GET_LSN( mMinRecoveryLSN, aBCB->mRecoveryLSN );
        }

        IDE_ASSERT( mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS );

        mCPListSet->remove( aStatistics, aBCB );

        sPageLSN = smLayerCallback::getPageLSN( aBCB->mFrame );

        sIOBPtr = mIOBPtr[ mIOBPos ];

        idlOS::memcpy( sIOBPtr, aBCB->mFrame, mPageSize );

        mIOBBCBArray[mIOBPos] = aBCB;

        // IOB�� ��ϵ� BCB���� pageLSN�߿� ���� ū LSN�� �����Ѵ�.
        // �� ���� ���߿� WAL�� ��Ű�� ���� ���ȴ�.
        if ( smLayerCallback::isLSNGT( &sPageLSN, &mMaxPageLSN )
             == ID_TRUE )
        {
            SM_GET_LSN( mMaxPageLSN, sPageLSN );
        }
        
        if( aMoveToSBuffer != ID_TRUE )
        {   
            SM_LSN_INIT( aBCB->mRecoveryLSN );
        }

        smLayerCallback::setPageLSN( sIOBPtr, &sPageLSN );

        /* BUG-23269 [5.3.1 SD] backup���� page image log ������ �߸��Ǿ�
         *           Restart Recovery ����
         * Disk TBS�� Backup ���¶�� DWB�� �����ϴ� �������� Latch Ǯ������
         * ������ Image �α׸� ����Ѵ�. �ֳ��ϸ�, DWB�� Copy�� ���Ŀ��� �ش�
         * Page�� ��� ����Ǿ�������, copy �� ������ ������ �α��� ���� ���Ŀ�
         * �߻��� ���� �ֱ� ������ ��ӵǴ� ��������� �����Ĺ����� �ִ�. */
        if ( smLayerCallback::isBackupingTBS( aBCB->mSpaceID ) 
             == ID_TRUE )
        {
            SC_MAKE_GRID( sPageGRID, 
                          aBCB->mSpaceID, 
                          aBCB->mPageID, 
                          0 );
            IDE_TEST( smLayerCallback::writeDiskPILogRec( aStatistics,
                                                          sIOBPtr,
                                                          sPageGRID )
                      != IDE_SUCCESS );
        }

        // IOB�� �����ϸ� S-latch�� Ǭ��.
        // �� �������� �� ���۴� �ٽ� DIRTY�� �� �� �ִ�.
        // �� �ٽ� CPListSet�� �Ŵ޸� �� �ִ�.
        aBCB->unlockPageLatch();

        /* BUG-22271: Flusher�� Page�� Disk�� ������ Page�� Checksum�� Page
         * Latch�� ��� �ϰ� �ֽ��ϴ�. ���ü� ����� ���ؼ� Latch�� Ǯ���Ѵ�. */
        IDV_TIME_GET(&sBeginTime);
        smLayerCallback::calcAndSetCheckSum( sIOBPtr );
        IDV_TIME_GET(&sEndTime);
        sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

        /* BUG-32670    [sm-disk-resource] add IO Stat information 
         * for analyzing storage performance.
         * Checksum ����ϴµ� �ɸ� �ð� ������. */
        mStat.applyTotalCalcChecksumTimeUSec( sCalcChecksumTime );

        mIOBPos++;
        mStat.applyINIOBCount( mIOBPos );

        // IOB�� ����á���� IOB�� ��ũ�� ������.
        if( mIOBPos == mIOBPageCount )
        {
            IDE_TEST( writeIOB( aStatistics, aMoveToPrepare, aMoveToSBuffer )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    IOB�� ��ũ�� ������. ���������� double write ����� ����� ���� �ְ�
 *    ���� ���� �ִ�. �׸��� disk�� ���������� �ݵ�� WAL�� ��Ű�� ����
 *    log�� ���� flush�Ѵ�.
 *    IOB�� ��������� �ƹ��� �۾��� ���� �ʴ´�.
 *    �׸��� IOB�� ��� ���� �Ŀ��� minRecoveryLSN�� MAX������ �ʱ�ȭ�Ѵ�.
 *
 *  aStatistics     - [IN]  �������
 *  aMoveToPrepare  - [IN]  flush���Ŀ� BCB�� prepare list�� �ű��� ����
 *                          ID_TRUE�̸� flush ���� BCB�� prepare list�� �ű��.
 *  aMoveToSBuffer  - [IN]  flushForReplacement���� ȣ��Ȱ�� TRUE.
 ******************************************************************************/
IDE_RC sdbFlusher::writeIOB( idvSQL           * aStatistics, 
                             idBool             aMoveToPrepare,
                             idBool             aMoveToSBuffer )
{
    UInt            i;
    UInt            sDummySyncedLFCnt; // ���߿� ������.
    idvWeArgs       sWeArgs;
    SInt            sWaitEventState = 0;
    sdbBCB        * sBCB;
    idvTime         sBeginTime;
    idvTime         sEndTime;
    ULong           sLogSyncTime;
    ULong           sDWTime;
    ULong           sWriteTime;
    ULong           sSyncTime;
    /* PROJ-2102 Secondary Buffer */
    UInt            sExtentIndex = 0;
    sdsBufferArea * sBufferArea;
    sdsBCB        * sSBCB = NULL;    

    if (mIOBPos > 0)
    {
        IDE_DASSERT( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) ||
                     ( smuProperty::getCrashTolerance() == 2 ) );

        if( needToUseDWBuffer( aMoveToSBuffer ) == ID_TRUE ) 
        {                                                  
            IDV_TIME_GET(&sBeginTime);

            mStat.applyIOBegin();
            // double write ����� ����ϴ� ���
            // IOB ��ü�� DWFile�� �ѹ� ����Ѵ�.
            IDE_TEST( mDWFile.write( aStatistics,
                                     mIOB,
                                     mIOBPos )
                      != IDE_SUCCESS);
            mStat.applyIODone();

            IDV_TIME_GET(&sEndTime);
            sDWTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
        }
        else
        {
            // double write�� ������� ���� ��쿡��
            // ó���ؾ��ϴ� Ư���� �۾��� ����.
            sDWTime = 0;
        }

        if( aMoveToSBuffer == ID_TRUE )
        {
            sBufferArea = sdsBufferMgr::getSBufferArea();

            if( sBufferArea->getTargetMoveDownIndex( aStatistics, &sExtentIndex )
                != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0,
                        "CAN NOT FIND SECONDARY BUFFER EXTENT \n ");
                sdsBufferMgr::setUnserviceable();
            }
            else 
            {
                /* nothing to do */   
            }
        }
        else 
        {
            /* */
        }

        // WAL
        /* BUG-45148 DW ���� Log Wait ���� ����
         * DWFile�� �̿��ϴ� ���� DataFile�� Inconsistent �� �� ���̴�.
         * WAL�� DataFile�� Flush�� �Ͼ�� ������ ��������ȴ�.
         * DWFile�� �������� ���� Log Flusher�� �̹� �ʿ��� Log�� Flush �Ҽ� �����Ƿ�
         * Log Wait �ð��� �پ�� �� �ִ�. */
        IDV_TIME_GET(&sBeginTime);
        IDE_TEST( smLayerCallback::sync4BufferFlush( &mMaxPageLSN,
                                                     &sDummySyncedLFCnt )
                  != IDE_SUCCESS );
        IDV_TIME_GET(&sEndTime);
        sLogSyncTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);


        IDV_WEARGS_SET( &sWeArgs, IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_WRITE,
                        0, /* WaitParam1 */
                        0, /* WaitParam2 */
                        0  /* WaitParam3 */ );

        sWriteTime = 0;

        for (i = 0; i < mIOBPos; i++)
        {
            IDV_TIME_GET(&sBeginTime);
            IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );

            sWaitEventState = 1;

            mStat.applyIOBegin();

            sBCB = mIOBBCBArray[i];

            if( needToMovedownSBuffer( aMoveToSBuffer, sBCB ) == ID_TRUE )
            { 
                IDE_TEST( sdsBufferMgr::moveDownPage( aStatistics,
                                                      sBCB,  
                                                      mIOBPtr[i],
                                                      sExtentIndex,/*Extent Idx */
                                                      i,           /*frame Idx */  
                                                      &sSBCB )
                          != IDE_SUCCESS ); 

                IDE_ASSERT( sSBCB != NULL );
                sBCB->mSBCB = sSBCB;
            }
            else   
            {
                IDE_TEST( sddDiskMgr::write( aStatistics,
                                             sBCB->mSpaceID,
                                             sBCB->mPageID,
                                             mIOBPtr[i])
                          != IDE_SUCCESS );

                sBCB->mSBCB = NULL;
            }

            mStat.applyIODone();

            sBCB->mWriteCount++;

            mArrSyncFileInfo[i].mSpaceID = sBCB->mSpaceID;
            mArrSyncFileInfo[i].mFileID  = SD_MAKE_FID( sBCB->mPageID );

            sWaitEventState = 0;
            IDV_END_WAIT_EVENT(aStatistics, &sWeArgs);
            IDV_TIME_GET(&sEndTime);
            sWriteTime += IDV_TIME_DIFF_MICRO( &sBeginTime, 
                                               &sEndTime);

            // BCB ���¸� clean�� �����Ѵ�.
            sBCB->lockBCBMutex( aStatistics );

            if( sBCB->mState == SDB_BCB_INIOB )
            {
                sBCB->mState = SDB_BCB_CLEAN;
                sBCB->mPrevState = SDB_BCB_INIOB;

                /* checkpoint flush ���̸� SecondaryBuffer �� �������ش�. */
                if( aMoveToSBuffer != ID_TRUE )
                { 
                    sSBCB = sBCB->mSBCB;
                    if( sSBCB != NULL )
                    { 
#ifdef DEBUG
                        /* IOB ���� ���� ��������������� ������
                           ����׿����� ��� ó�� */
                        IDE_RAISE( ERROR_INVALID_BCD )
#endif
                        sdsBufferMgr::removeBCB( aStatistics, 
                                                 sSBCB ); 
                        sBCB->mSBCB = NULL;
                    }
                }
            }  
            else 
            {
                IDE_DASSERT( sBCB->mState == SDB_BCB_REDIRTY );

                sBCB->mState = SDB_BCB_DIRTY;
                sBCB->mPrevState = SDB_BCB_REDIRTY;

                /* checkpoint flush ���̸� SecondaryBuffer �� �������ش�. */
                if( aMoveToSBuffer != ID_TRUE )
                { 
                    sSBCB = sBCB->mSBCB;
                    if( sSBCB != NULL )
                    { 
#ifdef DEBUG
                        /* IOB ���� ���� ��������������� ������
                           ����׿����� ��� ó�� */
                        IDE_RAISE( ERROR_INVALID_BCD )
#endif
                        sdsBufferMgr::removeBCB( aStatistics, 
                                                 sBCB->mSBCB ); 

                        sBCB->mSBCB = NULL;
                    }
                }
            }

            sBCB->unlockBCBMutex();

            if( aMoveToPrepare == ID_TRUE )
            {
                mPool->addBCB2PrepareLst( aStatistics, sBCB );
            }
        }

        if( aMoveToSBuffer == ID_TRUE )
        {
            /* movedown �� Extent�� sdsFlusher�� flush�Ҽ��ֵ��� ���¸� �����Ѵ�.*/
            sBufferArea->changeStateMovedownDone( sExtentIndex );
        }

        /* BUG-23752: [SD] Buffer���� Buffer Page�� Disk�� ������ Write�ϰ� ����
         * fsync�� ȣ���ϰ� ���� �ʽ��ϴ�.
         *
         * WritePage�� �߻��� TBS�� ���ؼ� fsync�� ȣ���Ѵ�. */
        if( needToSyncTBS( aMoveToSBuffer ) == ID_TRUE )
        {
            IDV_TIME_GET(&sBeginTime);
            IDE_TEST( syncAllFile4Flush( aStatistics ) != IDE_SUCCESS );
            IDV_TIME_GET(&sEndTime);

            sSyncTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
        }
        else 
        {
            sSyncTime = 0;
        }
        
        mStat.applyTotalFlushPages( mIOBPos );

        // IOB�� ��� disk�� �������Ƿ� IOB�� �ʱ�ȭ�Ѵ�.
        mIOBPos = 0;
        mStat.applyINIOBCount( mIOBPos );

        SM_LSN_INIT( mMaxPageLSN );

        IDE_ASSERT( mMinRecoveryLSNMutex.lock(aStatistics) == IDE_SUCCESS );
        SM_LSN_MAX( mMinRecoveryLSN );
        IDE_ASSERT( mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS );

        /* BUG-32670    [sm-disk-resource] add IO Stat information 
         * for analyzing storage performance.
         * Flush�� �ɸ� �ð����� ������. */
        mStat.applyTotalLogSyncTimeUSec( sLogSyncTime );
        mStat.applyTotalWriteTimeUSec( sWriteTime );
        mStat.applyTotalSyncTimeUSec( sSyncTime );
        mStat.applyTotalDWTimeUSec( sDWTime );
    }
    else
    {
        // IOB�� ��������Ƿ� �Ұ� ����.
    }

    return IDE_SUCCESS;

#ifdef DEBUG
    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                "invalid bcb :"
                "spaseID :%u\n"
                "pageID  :%u\n"
                "state   :%u\n"
                "CPListNo:%u\n"
                "HashTableNo:%u\n",
                sSBCB->mSpaceID,
                sSBCB->mPageID,
                sSBCB->mState,
                sSBCB->mCPListNo,
                sSBCB->mHashBucketNo );

        /* IOB�� �������� ��������� �Ѵ�. */
        IDE_DASSERT( 0 );
    }
#endif
    IDE_EXCEPTION_END;

    if (sWaitEventState == 1)
    {
        IDV_END_WAIT_EVENT(aStatistics, &sWeArgs);
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : mArrSyncFileInfo�� �ִ� File���� Sort���Ŀ�
 *               File �� �ѹ��� Sync�Ѵ�.
 *
 *  aStatistics     - [IN] �������
 ******************************************************************************/
IDE_RC sdbFlusher::syncAllFile4Flush( idvSQL * aStatistics )
{
    UInt            i;
    sdbSyncFileInfo sLstSyncFileInfo = { SC_NULL_SPACEID, 0 };

    idlOS::qsort( (void*)mArrSyncFileInfo,
                  mIOBPos,
                  ID_SIZEOF( sdbSyncFileInfo ),
                  sdbCompareSyncFileInfo );

    for( i = 1 ; i < mIOBPos ; i++ )
    {
        /* ���� File�� ���ؼ� �ι� Sync���� �ʰ� �ѹ��� �Ѵ�. */
        /* drop file�� ��� flusher job�� ����Ѵ�.
         * �׷��Ƿ� flusher������ drop file�� ������� �ʾƵ� �ȴ�.*/
        if (( sLstSyncFileInfo.mSpaceID != mArrSyncFileInfo[i].mSpaceID ) ||
            ( sLstSyncFileInfo.mFileID  != mArrSyncFileInfo[i].mFileID ))
        {
            IDE_ASSERT( mArrSyncFileInfo[i].mSpaceID != SC_NULL_SPACEID );

            sLstSyncFileInfo = mArrSyncFileInfo[i];

            IDE_TEST( sddDiskMgr::syncFile( aStatistics,
                                            sLstSyncFileInfo.mSpaceID,
                                            sLstSyncFileInfo.mFileID )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *    IOB�� ����� BCB�� �� recoveryLSN�� ���� ���� ���� ��´�.
 *    �� ���� checkpoint�� ���ȴ�.
 *    restart redo LSN�� �� ���� CPListSet�� minRecoveryLSN�� ���� ������
 *    �����ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] �ּ� recoveryLSN
 ******************************************************************************/
void sdbFlusher::getMinRecoveryLSN(idvSQL *aStatistics,
                                   smLSN  *aRet)
{
    IDE_ASSERT(mMinRecoveryLSNMutex.lock(aStatistics) == IDE_SUCCESS);

    SM_GET_LSN(*aRet, mMinRecoveryLSN);

    IDE_ASSERT(mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS);
}

/******************************************************************************
 * Description :
 *    ��� ������ �ý��ۿ� �ݿ��Ѵ�.
 ******************************************************************************/
void sdbFlusher::applyStatisticsToSystem()
{
    idvManager::applyStatisticsToSystem(&mCurrSess, &mOldSess);
}

/******************************************************************************
 * Description :
 *  �ϳ��� �۾��� ����ģ flusher�� ����� �� �ð��� ������ �ش�.
 *
 *  aFlushedCount   - [IN]  flusher�� �ٷ��� flush�� ������ ����
 ******************************************************************************/
UInt sdbFlusher::getWaitInterval(ULong aFlushedCount)
{
    if (sdbFlushMgr::isBusyCondition() == ID_TRUE)
    {
        mWaitTime = 0;
    }
    else
    {
        if (aFlushedCount > 0)
        {
            mWaitTime = smuProperty::getDefaultFlusherWaitSec();
        }
        else
        {
            // ������ �Ѱǵ� flush���� �ʾҴٸ�, ǫ ����.
            if (mWaitTime < smuProperty::getMaxFlusherWaitSec())
            {
                mWaitTime++;
            }
        }
    }
    mStat.applyLastSleepSec(mWaitTime);

    return mWaitTime;
}

/* PROJ-2102 Secondary Buffer */
/******************************************************************************
 * Description :  SecondaryBuffer�� ����Ҷ��� DW�� ������� ������ �ִ�.
 * DW�� ����ϴ��� �Ǵ��ϴ� ����
 * 1. DW property�� �����Ǿ�� �ϰ�
 * 2. Secondary Buffer�� ��� ����
 * 3. Secondary Buffer����ص� cache type�� clean page ��
 * 4. Secondary Buffer����ص� flush for replacement �� �ƴϸ� Disk�� ���
 *    dirty page�� DW�� disk�� �����.
 ******************************************************************************/
idBool sdbFlusher::needToUseDWBuffer( idBool aMoveToSBuffer )
{
    sdsSBufferType sSBufferType; 
    idBool sRet = ID_FALSE;
         
    sSBufferType = (sdsSBufferType)sdsBufferMgr::getSBufferType();

    /* case 1 */
    if( smuProperty::getUseDWBuffer() == ID_TRUE )
    {
        /* case 2  */
        if( mServiceable == ID_FALSE )
        {
            sRet = ID_TRUE;
        }
        else
        {   
            if( aMoveToSBuffer == ID_TRUE )
            {    
                 /* BUGBUG 
                  * CLEAN PAGE�� ���� DW�� ������.
                  * �̹����� Secondary Buffer �� ���������� ���� ������ �ذ��ϱ�����
                  * IOB�� �ι� �δ� ����� ����ؾ� �Ѵ�.
                  */

                 /* case 3 */
                if( sSBufferType == SDS_FLUSH_PAGE_CLEAN )
                {
                    sRet = ID_TRUE;
                }
                else 
                {
                    /* nothing to do */
                }
            }         
            else 
            {   /* case 4 */
                sRet = ID_TRUE;
            }
        }
    }
    else 
    {
        /* DW�� ������� �ʴ´ٸ� ����Ұ� ����.
         * nothing to do */
    }
    return sRet;
}

/******************************************************************************
 * Description : �ش� �������� Secondary Buffer�� ��� �ϴ� ���������� �Ǵ�
 ******************************************************************************/
idBool sdbFlusher::needToMovedownSBuffer( idBool aMoveToSBuffer, sdbBCB  * aBCB )
{
    sdsSBufferType sSBufferType; 

    idBool sRet = ID_FALSE;

    sSBufferType = (sdsSBufferType)sdsBufferMgr::getSBufferType();

    if( (mServiceable == ID_TRUE) && (aMoveToSBuffer == ID_TRUE) )
    {
        switch( sSBufferType )
        {   
            case SDS_FLUSH_PAGE_ALL:
                if( (aBCB->mPrevState == SDB_BCB_DIRTY) || 
                    (aBCB->mPrevState == SDB_BCB_CLEAN) )
                {
                    sRet = ID_TRUE;
                }
                else 
                {
                    /* nothing to do */
                }
                break;

            case SDS_FLUSH_PAGE_DIRTY:
                if( aBCB->mPrevState == SDB_BCB_DIRTY )
                {
                    sRet = ID_TRUE;
                }
                else 
                {
                    /* nothing to do */
                }
                break;

            case SDS_FLUSH_PAGE_CLEAN:
                if( aBCB->mPrevState == SDB_BCB_CLEAN )
                {
                    sRet = ID_TRUE;
                }
                else 
                {
                    /* nothing to do */
                }
                break;  

            default:
                ideLog::log( IDE_ERR_0,
                        "Unknown  Secondary Buffer Type:%u\n",
                        sSBufferType );
                IDE_DASSERT( 0 );
                break;
        }
    }
    else 
    {
        /* nothing to do */
    }
    return sRet;
}

idBool sdbFlusher::needToSkipBCB( sdbBCB  * aBCB )
{
    idBool sRet = ID_FALSE;

   // case 1: Secondary Buffer �� ������� �ʴ� ��� clean page ����..
   if( mServiceable != ID_TRUE )  
    {
        if( aBCB->mState == SDB_BCB_CLEAN )
        {
             sRet = ID_TRUE;
        }
        else 
        {
            /* nothing to do */
        }
    }
    else // Secondary Buffer ���
    {  
    /*                             FSB               HDD
     * cacheType �� ALL �϶� CLEAN + DIRTY   /        -
     *              CLEAN    CLEAN           /      DIRTY
     *              DIRTY    DIRTY           /        -      <--
     * case 2:  cacheType ��  DIRTY �� clean page��  ���� ��Ų��.
     */
        if( (sdsBufferMgr::getSBufferType() == SDS_FLUSH_PAGE_DIRTY) &&   
            (aBCB->mState == SDB_BCB_CLEAN) ) 
        {
             sRet = ID_TRUE;
        }
        else 
        {
            /* nothing to do */
        }
    } 
    return sRet;
}

idBool sdbFlusher::needToSyncTBS( idBool aMoveToSBuffer )
{
    sdsSBufferType sSBufferType; 

    idBool sRet = ID_TRUE;

    sSBufferType = (sdsSBufferType)sdsBufferMgr::getSBufferType();

    if( ( mServiceable == ID_TRUE ) &&
        ( aMoveToSBuffer == ID_TRUE ) )
    {
        switch( sSBufferType )
        {   
            case SDS_FLUSH_PAGE_ALL:
            case SDS_FLUSH_PAGE_DIRTY:
                sRet = ID_FALSE;
                break;

            case SDS_FLUSH_PAGE_CLEAN:
                break;  

            default:
                ideLog::log( IDE_ERR_0,
                        "Unknown  Secondary Buffer Type:%u\n",
                        sSBufferType );
                IDE_DASSERT( 0 );
                break;
        }
    }
    return sRet;
} 

void sdbFlusher::setDelayedFlushProperty( UInt aDelayedFlushListPct,
                                          UInt aDelayedFlushProtectionTimeMsec )
{
    mDelayedFlushListPct            = aDelayedFlushListPct;
    mDelayedFlushProtectionTimeUsec = aDelayedFlushProtectionTimeMsec * 1000;
}
