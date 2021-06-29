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
   sdbFlusher.cpp�� �⺻���� ���� ���� �̹Ƿ�
   sdbFlusher�� ������ ����� �� ���Ͽ��� ���� ����!!! 
 **********************************************************************/

#include <smDef.h>
#include <smErrorCode.h>
#include <sdbDef.h>
#include <smrRecoveryMgr.h>
#include <sds.h>
#include <sdsReq.h>


extern "C" SInt
sdsCompareFileInfo( const void* aElem1, const void* aElem2 )
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

sdsFlusher::sdsFlusher() : idtBaseThread()
{

}

sdsFlusher::~sdsFlusher()
{

}

/************************************************************************
 * Description : flusher ��ü�� �ʱ�ȭ�Ѵ�. 
 *  aFlusherID      - [IN]  �� ��ü�� ���� ID. 
 *  aPageSize       - [IN]  ������ ũ��. 
 *  aIOBPageCount   - [IN]  flusher�� IOB ũ��μ� ������ page �����̴�.
 *  aCPListSet      - [IN]  �÷��� �ؾ� �ϴ� FlushList�� �����ִ� buffer pool
 *                          �� ���� checkpoint set
 ************************************************************************/
IDE_RC sdsFlusher::initialize ( UInt  aFlusherID,
                                UInt  aPageSize,
                                UInt  aIOBPageCount )
{
    SChar   sTmpName[128];
    UInt    i;
    SInt    sState = 0;

    mFlusherID    = aFlusherID;
    mPageSize     = aPageSize;   /* SD_PAGE_SIZE */
    mIOBPageCount = aIOBPageCount;
    mIOBPos       = 0;
    mFinish       = ID_FALSE;
    mStarted      = ID_FALSE;
    mWaitTime     = smuProperty::getDefaultFlusherWaitSec();
    mCPListSet    = sdsBufferMgr::getCPListSet();
    mBufferArea   = sdsBufferMgr::getSBufferArea(); 
    mSBufferFile  = sdsBufferMgr::getSBufferFile();
    mMeta         = sdsBufferMgr::getSBufferMeta();
 
    SM_LSN_MAX( mMinRecoveryLSN );
    SM_LSN_INIT( mMaxPageLSN );

    // ��� ������ ���� session ���� �ʱ�ȭ
    // BUG-21155 : current session�� �ʱ�ȭ
    idvManager::initSession(&mCurrSess, 0 /* unused */, NULL /* unused */);

    idvManager::initSQL( &mStatistics,
                         &mCurrSess,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         IDV_OWNER_FLUSHER);

    idlOS::memset( sTmpName, 0, 128);
    idlOS::snprintf( sTmpName,
                     128,
                     "SECONDARY_BUFFER_FLUSHER_MIN_RECOVERY_LSN_MUTEX_%"ID_UINT32_FMT,
                     aFlusherID );

    IDE_TEST( mMinRecoveryLSNMutex.initialize( 
           sTmpName,
           IDU_MUTEX_KIND_NATIVE,
           IDB_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_FLUSHER_MIN_RECOVERY_LSN)
           != IDE_SUCCESS);
    sState = 1;

    idlOS::memset( sTmpName, 0, 128 );
    idlOS::snprintf( sTmpName,
                     128,
                     "SECONDARY_BUFFER_FLUSHER_COND_WAIT_MUTEX_%"ID_UINT32_FMT,
                     aFlusherID);

    IDE_TEST( mRunningMutex.initialize( sTmpName,
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_LATCH_FREE_OTHERS )
             != IDE_SUCCESS);
    sState = 2;

    /* TC/FIT/Limit/sm/sdsFlusher_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsFlusher::initialize::malloc1", 
                          ERR_INSUFFICIENT_MEMORY );

    // IOB �ʱ�ȭ
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       mPageSize * (mIOBPageCount + 1),
                                       (void**)&mIOBSpace ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 3;

    idlOS::memset((void*)mIOBSpace, 0, mPageSize * (mIOBPageCount + 1));

    mIOB = (UChar*)idlOS::align( (void*)mIOBSpace, mPageSize );

    /* TC/FIT/Limit/sm/sdsFlusher_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsFlusher::initialize::malloc2", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(UChar*) * mIOBPageCount,
                                       (void**)&mIOBPtr ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 4;

    /* TC/FIT/Limit/sm/sdsFlusher_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsFlusher::initialize::malloc3", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(sdsBCB*) * mIOBPageCount,
                                       (void**)&mIOBBCBArray ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 5;


    /* TC/FIT/Limit/sm/sdsFlusher_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsFlusher::initialize::malloc4", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF( sdbSyncFileInfo ) * mIOBPageCount,
                                       (void**)&mFileInfoArray ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 6;
    // condition variable �ʱ�ȭ
    // flusher�� ���� ���� ���⿡ ��⸦ �Ѵ�.
    idlOS::snprintf( sTmpName,
                     ID_SIZEOF( sTmpName ),
                     "SECONDARY_BUFFER_FLUSHER_COND_%"ID_UINT32_FMT,
                     aFlusherID);

    IDE_TEST_RAISE( mCondVar.initialize(sTmpName) != IDE_SUCCESS,
                    ERR_COND_VAR_INIT);
    sState = 7;

    for (i = 0; i < mIOBPageCount; i++)
    {
        mIOBPtr[i] = mIOB + (mPageSize * i);
    }

    IDE_TEST( mDWFile.create( mFlusherID, 
                              mPageSize, 
                              mIOBPageCount, 
                              SD_LAYER_SECONDARY_BUFFER )
              != IDE_SUCCESS);
    sState = 8; 

    mFlusherStat.initialize( mFlusherID );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_VAR_INIT );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondInit) );
    }
    IDE_EXCEPTION( ERR_INSUFFICIENT_MEMORY );
    {
        IDE_SET( ideSetErrorCode(idERR_ABORT_InsufficientMemory) );
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 8:
            IDE_ASSERT( mDWFile.destroy() == IDE_SUCCESS );
        case 7:
            IDE_ASSERT( mCondVar.destroy() == IDE_SUCCESS );
        case 6:
            IDE_ASSERT( iduMemMgr::free( mFileInfoArray ) == IDE_SUCCESS );
        case 5:
            IDE_ASSERT( iduMemMgr::free( mIOBBCBArray ) == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( iduMemMgr::free( mIOBPtr ) == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( iduMemMgr::free( mIOBSpace ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mRunningMutex.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mMinRecoveryLSNMutex.destroy() == IDE_SUCCESS );
        default:
           break;
    }

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : flusher ��ü
 ****************************************************************************/
IDE_RC sdsFlusher::destroy()
{
    IDE_ASSERT( mDWFile.destroy() == IDE_SUCCESS );

    IDE_TEST_RAISE( mCondVar.destroy() != IDE_SUCCESS, ERR_COND_DESTROY );

    IDE_ASSERT( iduMemMgr::free( mFileInfoArray ) == IDE_SUCCESS );
    mFileInfoArray = NULL;

    IDE_ASSERT( iduMemMgr::free( mIOBBCBArray ) == IDE_SUCCESS );
    mIOBBCBArray = NULL;

    IDE_ASSERT( iduMemMgr::free( mIOBPtr ) == IDE_SUCCESS );
    mIOBPtr = NULL;

    IDE_ASSERT( iduMemMgr::free( mIOBSpace ) == IDE_SUCCESS );
    mIOBSpace = NULL;

    IDE_ASSERT( mRunningMutex.destroy() == IDE_SUCCESS );

    IDE_ASSERT( mMinRecoveryLSNMutex.destroy() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_DESTROY );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondDestroy ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 ****************************************************************************/
IDE_RC sdsFlusher::start()
{
    mFinish = ID_FALSE;

    IDE_TEST(idtBaseThread::start() != IDE_SUCCESS);

    IDE_TEST(idtBaseThread::waitToStart() != IDE_SUCCESS);

    mFlusherStat.applyStart();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 *    flusher �������� ������ ���������� �����Ų��.
 *  aStatistics - [IN]  �������
 *  aNotStarted - [OUT] flusher�� ���� ���۵��� �ʾҴٸ� ID_TRUE�� ����. �� �ܿ�
 *                      ID_FALSE�� ����
 ****************************************************************************/
IDE_RC sdsFlusher::finish( idvSQL *aStatistics, idBool *aNotStarted )
{
    IDE_ASSERT( mRunningMutex.lock(aStatistics) == IDE_SUCCESS );

    if( mStarted == ID_FALSE )
    {
        IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
        *aNotStarted = ID_TRUE;
    }
    else
    {
        mFinish      = ID_TRUE;
        mStarted     = ID_FALSE;
        *aNotStarted = ID_FALSE;

        IDE_TEST_RAISE( mCondVar.signal() != 0, ERR_COND_SIGNAL );

        IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );

        IDE_TEST_RAISE( join() != IDE_SUCCESS,  ERR_THR_JOIN );

        mFlusherStat.applyFinish();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_THR_JOIN );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_Systhrjoin ) );
    }
    IDE_EXCEPTION( ERR_COND_SIGNAL);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondSignal ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : flusher �����尡 start�Ǹ� �Ҹ��� �Լ��̴�.
 ****************************************************************************/
void sdsFlusher::run()
{
    sdbFlushJob                      sJob;
    sdbSBufferReplaceFlushJobParam * sReplaceJobParam;
    sdbObjectFlushJobParam         * sObjJobParam;
    sdbChkptFlushJobParam          * sChkptJobParam;

    PDL_Time_Value sTimeValue;
    time_t         sBeforeWait;
    time_t         sAfterWait;
    UInt           sWaitSec;
    ULong          sFlushedCount = 0;
    idBool         sMutexLocked  = ID_FALSE;
    IDE_RC         sRC;
    
    IDE_ASSERT( mRunningMutex.lock( NULL ) == IDE_SUCCESS );
    sMutexLocked = ID_TRUE;

    mStarted = ID_TRUE;
    while ( mFinish == ID_FALSE )
    {
        sFlushedCount = 0;
        sdsFlushMgr::getJob( &mStatistics, &sJob );
        switch( sJob.mType )
        {
           case SDB_FLUSH_JOB_REPLACEMENT_FLUSH:
                mFlusherStat.applyReplaceFlushJob();
                sReplaceJobParam = &sJob.mFlushJobParam.mSBufferReplaceFlush;
                IDE_TEST( flushForReplacement( &mStatistics,
                                               sJob.mReqFlushCount,
                                               sReplaceJobParam->mExtentIndex,
                                               &sFlushedCount )
                          != IDE_SUCCESS );
                mFlusherStat.applyReplaceFlushJobDone();
                break;
            case SDB_FLUSH_JOB_CHECKPOINT_FLUSH:
                mFlusherStat.applyCheckpointFlushJob();
                sChkptJobParam = &sJob.mFlushJobParam.mChkptFlush;
                IDE_TEST( flushForCheckpoint( &mStatistics,
                                              sJob.mReqFlushCount,
                                              sChkptJobParam->mRedoPageCount,
                                              sChkptJobParam->mRedoLogFileCount,
                                              &sFlushedCount )
                          != IDE_SUCCESS );
                mFlusherStat.applyCheckpointFlushJobDone();
                break;
            case SDB_FLUSH_JOB_DBOBJECT_FLUSH:
                mFlusherStat.applyObjectFlushJob();
                sObjJobParam = &sJob.mFlushJobParam.mObjectFlush;
                IDE_TEST( flushForObject( &mStatistics,
                                          sObjJobParam->mFiltFunc,
                                          sObjJobParam->mFiltObj,
                                          sObjJobParam->mBCBQueue,
                                          &sFlushedCount )
                          != IDE_SUCCESS );
                mFlusherStat.applyObjectFlushJobDone();
                break;
            default:
                break;
        }

        if( sJob.mType != SDB_FLUSH_JOB_NOTHING ) 
        {
            sdsFlushMgr::updateLastFlushedTime();
            sdsFlushMgr::notifyJobDone( sJob.mJobDoneParam );
        }

        if( sJob.mType == SDB_FLUSH_JOB_REPLACEMENT_FLUSH )
        {   
            /* flush ���� Extent�� sdbFlusher�� movedown�Ҽ��ֵ��� ���º��� */ 
            mBufferArea->changeStateFlushDone( sReplaceJobParam->mExtentIndex );
        } 

        sWaitSec = getWaitInterval( sFlushedCount );
        if( sWaitSec == 0 )
        {
            IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
            IDE_ASSERT( mRunningMutex.lock( NULL ) == IDE_SUCCESS );
            continue;
        }

        sBeforeWait = idlOS::time( NULL );
        // sdsFlushMgr�κ��� �󸶳� ���� �˾ƿͼ� time value�� �����Ѵ�.
        sTimeValue.set( sBeforeWait + sWaitSec );
        sRC = mCondVar.timedwait(&mRunningMutex, &sTimeValue);
        sAfterWait = idlOS::time( NULL );

        mFlusherStat.applyTotalSleepSec( sAfterWait - sBeforeWait );

        if( sRC == IDE_SUCCESS )
        {
            // cond_signal�� �ް� ��� ���
            mFlusherStat.applyWakeUpsBySignal();
        }
        else
        {
            if(mCondVar.isTimedOut() == ID_TRUE)
            {
                // timeout�� �Ǿ� ��� ���
                mFlusherStat.applyWakeUpsByTimeout();
            }
            else
            {
                // �׿��� ��쿡�� ������ ó���� �Ѵ�.
                ideLog::log( IDE_SERVER_0, 
                             "Secondary Flusher Dead [RC:%d, ERRNO:%d]", 
                             sRC, errno );
                IDE_RAISE( ERR_COND_WAIT );
            }
        }
    }

    sMutexLocked = ID_FALSE;
    IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );

    return;

    IDE_EXCEPTION( ERR_COND_WAIT );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondWait ) );
    }
    IDE_EXCEPTION_END;

    if( sMutexLocked == ID_TRUE )
    {
        IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
    }

    mStarted = ID_FALSE;
    mFinish = ID_TRUE;

    IDE_ASSERT( 0 );

    return;
}

/****************************************************************************
 * Description :
 *    flush list�� ���� flush�۾��� �����Ѵ�.
 *  aStatistics     - [IN]  �� list �̵��� mutex ȹ���� �ʿ��ϱ� ������
 *                          ��������� �ʿ��ϴ�.
 *  aReqFlushCount  - [IN]  ����ڰ� ��û�� flush����
 *                          X-latch�� �����ִ� ���۵��� �ű� LRU list
 *  aRetFlushedCount- [OUT] ���� flush�� page ����
 ***************************************************************************/
IDE_RC sdsFlusher::flushForReplacement( idvSQL         * aStatistics,
                                        ULong            aReqFlushCount,
                                        ULong            aExtentIndex,
                                        ULong          * aRetFlushedCount)
{
    sdsBCB        * sSBCB;
    UInt            sFlushCount = 0;
    UInt            sReqFlushCount = 0;
    UInt            sSBCBIndex;   

    IDE_ERROR( aReqFlushCount > 0 );

    *aRetFlushedCount = 0;
    /* �� Extent�� flush �Ѵ�. �� Extent�� IOBPage ���� ��ŭ��  �������� ���� */
    sReqFlushCount = aReqFlushCount*mIOBPageCount;
    
    sSBCBIndex = aExtentIndex * mIOBPageCount ;

    // sReqFlushCount��ŭ flush�Ѵ�.
    while( sReqFlushCount-- )
    {
        sSBCB = mBufferArea->getBCB( sSBCBIndex );
        sSBCBIndex++;

        IDE_ASSERT( sSBCB != NULL )

       // BCB state �˻縦 �ϱ� ���� BCBMutex�� ȹ���Ѵ�.
        sSBCB->lockBCBMutex( aStatistics );

        if( (sSBCB->mState == SDS_SBCB_OLD ) ||
            (sSBCB->mState == SDS_SBCB_INIOB ) ||
            (sSBCB->mState == SDS_SBCB_CLEAN ) ||
            (sSBCB->mState == SDS_SBCB_FREE) )
        {
            // REDIRTY, INIOB, CLEAN�� ���۴�  skip
            sSBCB->unlockBCBMutex();

            mFlusherStat.applyReplaceSkipPages();
            continue;
        }
        else
        {
            IDE_ASSERT( sSBCB->mState == SDS_SBCB_DIRTY );
        }

        // flush ������ ��� �����ߴ�.
        // ���� flush�� ����.
        sSBCB->mState = SDS_SBCB_INIOB;
        sSBCB->unlockBCBMutex();

        if( SM_IS_LSN_INIT(sSBCB->mRecoveryLSN) )
        {
            // temp page�� ��� double write�� �� �ʿ䰡 ���⶧����
            // IOB�� ������ �� �ٷ� �� �������� disk�� ������.
            IDE_TEST( flushTempPage( aStatistics,
                                     sSBCB )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics,
                                 sSBCB )
                      != IDE_SUCCESS);
        }
        mFlusherStat.applyReplaceFlushPages();

        sFlushCount++;
    }

    // IOB�� ���� ���������� ��ũ�� ������.
    IDE_TEST( writeIOB( aStatistics ) != IDE_SUCCESS );

    *aRetFlushedCount = sFlushCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 *    CPListSet�� ���۵��� recovery LSN�� ���� ������ flush�� �����Ѵ�.
 *
 *  aStatistics         - [IN]  �������
 *  aMinFlushCount      - [IN]  flush�� �ּ� Page ��
 *  aRedoDirtyPageCnt   - [IN]  Restart Recovery�ÿ� Redo�� Page ��,
 *                              �ݴ�� ���ϸ� CP List�� ���ܵ� Dirty Page�� ��
 *  aRedoLogFileCount   - [IN]  Restart Recovery�ÿ� redo�� LogFile ��
 *  aRetFlushedCount    - [OUT] ���� flush�� page ����
 ****************************************************************************/
IDE_RC sdsFlusher::flushForCheckpoint( idvSQL      * aStatistics,
                                       ULong         aMinFlushCount,
                                       ULong         aRedoPageCount,
                                       UInt          aRedoLogFileCount,
                                       ULong       * aRetFlushedCount )
{
    sdsBCB  * sSBCB;
    ULong     sFlushCount    = 0;
    ULong     sReqFlushCount = 0;
    ULong     sNotDirtyCount = 0;
    smLSN     sReqFlushLSN;
    smLSN     sMaxFlushLSN;
    UShort    sActiveFlusherCount;

    IDE_ASSERT( aRetFlushedCount != NULL );

    *aRetFlushedCount = 0;
    // checkpoint list���� ���� ū recoveryLSN -> �ִ� flush �Ҽ� �ִ� LSN
    mCPListSet->getMaxRecoveryLSN( aStatistics, &sMaxFlushLSN );

    //DISK�� ������ LSN���� ���Ѵ�.
    smLayerCallback::getLstLSN( &sReqFlushLSN );

    if ( sReqFlushLSN.mFileNo >= aRedoLogFileCount )
    {
        sReqFlushLSN.mFileNo -= aRedoLogFileCount;
    }
    else
    {
        SM_LSN_INIT( sReqFlushLSN );
    }

    // flush ��û�� page�� ���� ����Ѵ�.
    sReqFlushCount = mCPListSet->getTotalBCBCnt();

    if( sReqFlushCount > aRedoPageCount )
    {
        // CP list�� Dirty Page���� Restart Recovery�ÿ�
        // Redo �� Page ���� ���������Ƿ� �� ���̸� flush�Ѵ�.
        sReqFlushCount -= aRedoPageCount;

        // flush�� page�� �۵����� flusher��(n)�� 1/n�� ������.
        sActiveFlusherCount = sdsFlushMgr::getActiveFlusherCount();
        IDE_ASSERT( sActiveFlusherCount > 0 );
        sReqFlushCount /= sActiveFlusherCount;

        if( aMinFlushCount > sReqFlushCount )
        {
            sReqFlushCount = aMinFlushCount;
        }
    }
    else
    {
        // CP list�� Dirty Page���� Restart Recovery�ÿ�
        // Redo �� Page ���� �۴�. MinFlushCount ��ŭ�� flush�Ѵ�.
        sReqFlushCount = aMinFlushCount;
    }
    // CPListSet���� recovery LSN�� ���� ���� BCB�� ����.
    sSBCB = (sdsBCB*)mCPListSet->getMin();

    while( sSBCB != NULL )
    {
        if ( smLayerCallback::isLSNLT( &sMaxFlushLSN,
                                       &sSBCB->mRecoveryLSN )
             == ID_TRUE)
        {
            // RecoveryLSN�� �ִ� flush �Ϸ��� LSN���� ũ�Ƿ� ��. 
            break;
        }

        if ( ( sFlushCount >= sReqFlushCount ) &&
             ( smLayerCallback::isLSNGT( &sSBCB->mRecoveryLSN, &sReqFlushLSN ) 
               == ID_TRUE) )
        {
            // sReqFlushCount �̻� flush�Ͽ���
            // sReqFlushLSN���� flush�Ͽ����� flush �۾��� �ߴ��Ѵ�.
            break;
        }

        sSBCB->lockBCBMutex( aStatistics );

        if( sSBCB->mState != SDS_SBCB_DIRTY )
        {
            // DIRTY�� �ƴ� ���۴� skip�Ѵ�.
            // ���� BCB�� �����Ѵ�.
            sSBCB->unlockBCBMutex();

            sNotDirtyCount++;
            if( sNotDirtyCount > mCPListSet->getListCount() )
            {
                IDE_TEST( writeIOB( aStatistics ) != IDE_SUCCESS );
                sNotDirtyCount = 0;
            }
            // nextBCB()�� ���ؼ� ���� BCB���� �����ϴ� ����
            // CPListSet���� ���� �� �ִ�.
            // ���� nextBCB()�� ���ڷ� ���� sSBCB����
            // �� �������� CPList���� ���� �� �ִ�.
            // ���� �׷� ����� nextBCB�� minBCB�� ������ ���̴�.
            sSBCB = (sdsBCB*)mCPListSet->getNextOf( sSBCB );
            mFlusherStat.applyCheckpointSkipPages();
            continue;
        }
        sNotDirtyCount = 0;
        // �ٸ� flusher���� flush�� ���ϰ� �ϱ� ����
        // INIOB���·� �����Ѵ�.
        sSBCB->mState = SDS_SBCB_INIOB;
        sSBCB->unlockBCBMutex();

        if( SM_IS_LSN_INIT( sSBCB->mRecoveryLSN ) )
        {
            // TEMP PAGE�� check point ����Ʈ�� �޷� ������ �ȵȴ�.
            // �׷����� ������ ������ �ʱ� ������, release�ÿ� �߰ߵǴ���
            // ������ �ʵ��� �Ѵ�.
            // TEMP PAGE�� �ƴϴ���, mRecoveryLSN�� init���̶�� ����
            // recovery�� ������� ��������� ���̴�.
            // recovery�� ������� �������� checkpoint�� ���� �ʾƵ� �ȴ�.
            IDE_DASSERT( 0 );
            IDE_TEST( flushTempPage( aStatistics, sSBCB ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics, sSBCB ) != IDE_SUCCESS );
        }
        mFlusherStat.applyCheckpointFlushPages();

        sFlushCount++;

        // ���� flush�� BCB�� ���Ѵ�.
        sSBCB = (sdsBCB*)mCPListSet->getMin();
    }

    *aRetFlushedCount = sFlushCount;

    // IOB�� ���� �ִ� ���۵��� ��� disk�� ����Ѵ�.
    IDE_TEST( writeIOB( aStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 *  aBCBQueue�� ����ִ� BCB�� ������� flush�� �����Ѵ�.
 *  aStatistics     - [IN]  �������.
 *  aFiltFunc       - [IN]  �÷��� ���� ���� ���� �Լ�.
 *  aFiltObj        - [IN]  aFiltFunc�� �Ѱ��� ����.
 *  aBCBQueue       - [IN]  �÷��� �ؾ��� BCB���� �����Ͱ� ����ִ� ť.
 *  aRetFlushedCount- [OUT] ���� �÷��� �� ������ ����.
 ****************************************************************************/
IDE_RC sdsFlusher::flushForObject( idvSQL       * aStatistics,
                                   sdsFiltFunc    aFiltFunc,
                                   void         * aFiltObj,
                                   smuQueueMgr  * aBCBQueue,
                                   ULong        * aRetFlushedCount )
{
    sdsBCB          * sSBCB  = NULL;
    idBool            sEmpty = ID_FALSE;
    PDL_Time_Value    sTV;
    ULong             sRetFlushedCount = 0;

    while (1)
    {
        IDE_ASSERT( aBCBQueue->dequeue( ID_FALSE, //mutex ���� �ʴ´�.
                                        (void*)&sSBCB,
                                        &sEmpty )
                    == IDE_SUCCESS );

        if ( sEmpty == ID_TRUE )
        {
            // ���̻� flush�� BCB�� �����Ƿ� ������ ����������.
            break;
        }
retry:
        if( aFiltFunc( sSBCB, aFiltObj ) != ID_TRUE )
        {
            mFlusherStat.applyObjectSkipPages();
            continue;
        }
        
        // BCB state �˻縦 �ϱ� ���� BCBMutex�� ȹ���Ѵ�.
        sSBCB->lockBCBMutex(aStatistics);

        if( (sSBCB->mState == SDS_SBCB_CLEAN) ||
            (sSBCB->mState == SDS_SBCB_FREE) )
        {
            sSBCB->unlockBCBMutex();
            mFlusherStat.applyObjectSkipPages();
            continue;
        }

        if( aFiltFunc( sSBCB, aFiltObj ) == ID_FALSE )
        {
            // BCB�� aFiltFunc������ �������� ���ϸ� �÷��� ���� �ʴ´�.
            sSBCB->unlockBCBMutex();
            mFlusherStat.applyObjectSkipPages();
            continue;
        }

        // BUG-21135 flusher�� flush�۾��� �Ϸ��ϱ� ���ؼ���
        // INIOB������ BCB���°� ����Ǳ� ��ٷ��� �մϴ�.
        if( (sSBCB->mState == SDS_SBCB_INIOB) ||
            (sSBCB->mState == SDS_SBCB_OLD ) )
        {
            sSBCB->unlockBCBMutex();
            sTV.set(0, 50000);
            idlOS::sleep(sTV);
            goto retry;
        }
        // flush ������ ��� �����ߴ�.
        // ���� flush�� ����.
        sSBCB->mState = SDS_SBCB_INIOB;
        sSBCB->unlockBCBMutex();

        if( SM_IS_LSN_INIT(sSBCB->mRecoveryLSN) )
        {
            // temp page�� ��� double write�� �� �ʿ䰡 ���⶧����
            // IOB�� ������ �� �ٷ� �� �������� disk�� ������.
            IDE_TEST( flushTempPage( aStatistics, sSBCB )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( copyToIOB( aStatistics, sSBCB )
                      != IDE_SUCCESS );
        }
        mFlusherStat.applyObjectFlushPages();
        sRetFlushedCount++;
    }

    // IOB�� ��ũ�� ����Ѵ�.
    IDE_TEST( writeIOB( aStatistics )
              != IDE_SUCCESS);

    *aRetFlushedCount = sRetFlushedCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRetFlushedCount = sRetFlushedCount;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 *    temp page�� ��� copyToIOB()��� �� �Լ��� ����Ѵ�.
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  flush�� BCB
 ****************************************************************************/
IDE_RC sdsFlusher::flushTempPage( idvSQL    * aStatistics,
                                  sdsBCB    * aSBCB )
{
    SInt        sWaitEventState = 0;
    idvWeArgs   sWeArgs;
    UChar     * sIOBPtr;
    idvTime     sBeginTime;
    idvTime     sEndTime;
    ULong       sTempWriteTime=0;

    IDE_ASSERT( aSBCB->mCPListNo == SDB_CP_LIST_NONE );
    IDE_ASSERT( smLayerCallback::isTempTableSpace( aSBCB->mSpaceID )
                == ID_TRUE );

    sIOBPtr = mIOBPtr[mIOBPos];

    aSBCB->lockBCBMutex( aStatistics );

    if( aSBCB->mState == SDS_SBCB_INIOB )
    {
        /* PBuffer�� �ִ� ���¸� ���� �����ϰų� PBuffer ���¸� �ٲ�� �ϳ� 
           Mutex ��� �ϴµ� ������ �ִµ�.  ���� ����. */
        IDE_TEST_RAISE( mSBufferFile->open( aStatistics ) != IDE_SUCCESS,
                        ERR_READ_SECONDARY_BUFFER );

        IDE_TEST_RAISE( mSBufferFile->read( aStatistics,
                               aSBCB->mSBCBID,      /* who */
                               mMeta->getFrameOffset( aSBCB->mSBCBID ),
                               1,                   /* page counter */
                               sIOBPtr )            /* where */
                        != IDE_SUCCESS,
                        ERR_READ_SECONDARY_BUFFER );

        /* BUG-22271 */
        IDV_WEARGS_SET( &sWeArgs, IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_WRITE,
                        0, /* WaitParam1 */
                        0, /* WaitParam2 */
                        0  /* WaitParam3 */ );

        IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );
        sWaitEventState = 1;

        IDV_TIME_GET(&sBeginTime);

        mFlusherStat.applyIOBegin();
        
        IDE_TEST( sddDiskMgr::write( aStatistics,
                                     aSBCB->mSpaceID, 
                                     aSBCB->mPageID, 
                                     mIOBPtr[mIOBPos] )
                  != IDE_SUCCESS );

        aSBCB->mState = SDS_SBCB_CLEAN;
        aSBCB->unlockBCBMutex();

        mFlusherStat.applyIODone();

        IDV_TIME_GET( &sEndTime );
        sTempWriteTime = IDV_TIME_DIFF_MICRO( &sBeginTime, &sEndTime );

        sWaitEventState = 0;
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );

        /* BUG-32670    [sm-disk-resource] add IO Stat information
         * for analyzing storage performance.
         * ���� TempPage�� �ѹ��� �� Page�� Write��. */
        mFlusherStat.applyTotalFlushTempPages( 1 );
        mFlusherStat.applyTotalTempWriteTimeUSec( sTempWriteTime );
    }
    else 
    {
        sdsBufferMgr::makeFreeBCBForce( aStatistics,
                                        aSBCB ); 
        aSBCB->unlockBCBMutex();
    } 

    /* BUG-32670    [sm-disk-resource] add IO Stat information
     * for analyzing storage performance.
     * ���� TempPage�� �ѹ��� �� Page�� Write��. */
    mFlusherStat.applyTotalFlushTempPages( 1 );
    mFlusherStat.applyTotalTempWriteTimeUSec( sTempWriteTime );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_READ_SECONDARY_BUFFER );
    {
        sdsBufferMgr::setUnserviceable();
        IDE_SET( ideSetErrorCode( smERR_FATAL_PageReadStopped ) );
    }
    IDE_EXCEPTION_END;

    if( sWaitEventState == 1 )
    {
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
    }

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : BCB�� frame ������ IOB�� �����Ѵ�.
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  BCB
 ****************************************************************************/
IDE_RC sdsFlusher::copyToIOB( idvSQL  * aStatistics,
                              sdsBCB  * aSBCB )
{
    UChar     * sIOBPtr;
    smLSN       sPageLSN;
 
    IDE_ASSERT( mIOBPos < mIOBPageCount );
    IDE_ASSERT( aSBCB->mCPListNo != SDB_CP_LIST_NONE );

    /* PROJ-2162 RestartRiskReduction
     * Consistency ���� ������, Flush�� ���´�.  */
    if( ( smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
        ( smuProperty::getCrashTolerance() != 2 ) )
    {
        /* Checkpoint Linst���� BCB�� �����ϰ� Mutex��
         * Ǯ���ֱ⸸ �Ѵ�. Flush�� IOB ����� ���� */
        mCPListSet->remove( aStatistics, aSBCB );
    }
    else
    {
        // IOB�� minRecoveryLSN�� �����ϱ� ���ؼ� mMinRecoveryLSNMutex�� ȹ���Ѵ�.
        IDE_ASSERT( mMinRecoveryLSNMutex.lock(aStatistics) == IDE_SUCCESS );

        if ( smLayerCallback::isLSNLT( &aSBCB->mRecoveryLSN, 
                                       &mMinRecoveryLSN )
             == ID_TRUE)
        {
            SM_GET_LSN( mMinRecoveryLSN, aSBCB->mRecoveryLSN );
        }

        IDE_ASSERT( mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS );

        mCPListSet->remove( aStatistics, aSBCB );

        sPageLSN = aSBCB->mPageLSN;

        sIOBPtr = mIOBPtr[ mIOBPos ];

        /* ���⼭  PBuffer�� ���¸� ���� PBuffer�� ���¸� �ٲٰų� �����ؾ��ϴµ� 
          mutex ��°� ������ �Ǵµ� �ϴ�. ���� ����. */

        IDE_TEST_RAISE( mSBufferFile->open( aStatistics ) != IDE_SUCCESS,
                        ERR_READ_SECONDARY_BUFFER );

        IDE_TEST_RAISE( mSBufferFile->read( aStatistics,
                                            aSBCB->mSBCBID,     /* who */
                                            mMeta->getFrameOffset( aSBCB->mSBCBID ),
                                            1,                  /* page counter */
                                            sIOBPtr )           /* where */
                        != IDE_SUCCESS,
                        ERR_READ_SECONDARY_BUFFER );

        mIOBBCBArray[mIOBPos] = aSBCB;

        // IOB�� ��ϵ� BCB���� pageLSN�߿� ���� ū LSN�� �����Ѵ�.
        // �� ���� ���߿� WAL�� ��Ű�� ���� ���ȴ�.
        if ( smLayerCallback::isLSNGT( &sPageLSN, &mMaxPageLSN )
             == ID_TRUE )
        {
            SM_GET_LSN( mMaxPageLSN, sPageLSN );
        }

        SM_LSN_INIT( aSBCB->mRecoveryLSN );
        smLayerCallback::calcAndSetCheckSum( sIOBPtr );

        mIOBPos++;
        mFlusherStat.applyINIOBCount( mIOBPos );

        // IOB�� ����á���� IOB�� ��ũ�� ������.
        if( mIOBPos == mIOBPageCount )
        {
            IDE_TEST( writeIOB( aStatistics ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_READ_SECONDARY_BUFFER );
    {
        sdsBufferMgr::setUnserviceable();
        IDE_SET( ideSetErrorCode( smERR_FATAL_PageReadStopped ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :
 *    IOB�� ��ũ�� ������.
 *  aStatistics     - [IN]  �������
 ****************************************************************************/
IDE_RC sdsFlusher::writeIOB( idvSQL *aStatistics )
{
    UInt        i;
    UInt        sDummySyncedLFCnt; // ���߿� ������.
    idvWeArgs   sWeArgs;
    SInt        sWaitEventState = 0;
    sdsBCB    * sSBCB;
   
    idvTime     sBeginTime;
    idvTime     sEndTime;
    ULong       sDWTime;
    ULong       sWriteTime;
    ULong       sSyncTime;
    SInt        sErrState   = 0;
    UInt        sTBSSyncCnt = 0;

    if( mIOBPos > 0 )
    {
        IDE_DASSERT( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) ||
                     ( smuProperty::getCrashTolerance() == 2 ) );
        sErrState = 1;
        // WAL
        // ������ ������ ����.;;;;
        IDE_TEST( smLayerCallback::sync4BufferFlush( &mMaxPageLSN,
                                                     &sDummySyncedLFCnt )
                  != IDE_SUCCESS);
        sErrState = 2;

        if ( smuProperty::getUseDWBuffer() == ID_TRUE )
        {
            IDV_TIME_GET( &sBeginTime );

            mFlusherStat.applyIOBegin();
            IDE_TEST( mDWFile.write( aStatistics,
                                     mIOB,
                                     mIOBPos )
                     != IDE_SUCCESS );
            mFlusherStat.applyIODone();
            sErrState = 3;

            IDV_TIME_GET( &sEndTime );
            sDWTime = IDV_TIME_DIFF_MICRO( &sBeginTime, &sEndTime );
        }
        else
        {
            sDWTime = 0;
        }
        IDV_WEARGS_SET( &sWeArgs, IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_WRITE,
                        0, /* WaitParam1 */
                        0, /* WaitParam2 */
                        0  /* WaitParam3 */ );

        sWriteTime = 0;

        for ( i = 0; i < mIOBPos; i++ )
        {
            IDV_TIME_GET(&sBeginTime);
            IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );
            sWaitEventState = 1;

            mFlusherStat.applyIOBegin();

            sSBCB = mIOBBCBArray[i];

            sSBCB->lockBCBMutex( aStatistics );

            if( sSBCB->mState == SDS_SBCB_INIOB )
            {
                sSBCB->mState = SDS_SBCB_CLEAN;

                sSBCB->unlockBCBMutex();

                mFileInfoArray[sTBSSyncCnt].mSpaceID = sSBCB->mSpaceID;
                mFileInfoArray[sTBSSyncCnt].mFileID  = SD_MAKE_FID( sSBCB->mPageID );
                sTBSSyncCnt++;

                IDE_TEST( sddDiskMgr::write( aStatistics,
                                             sSBCB->mSpaceID,  
                                             sSBCB->mPageID,
                                             mIOBPtr[i])
                          != IDE_SUCCESS);

                mFlusherStat.applyIODone();
                sErrState = 4;
            }
            else 
            {
                sdsBufferMgr::makeFreeBCBForce( aStatistics,
                                                sSBCB ); 

                sSBCB->unlockBCBMutex();
            }

            sWaitEventState = 0;
            IDV_END_WAIT_EVENT(aStatistics, &sWeArgs);
            IDV_TIME_GET(&sEndTime);
            sWriteTime += IDV_TIME_DIFF_MICRO( &sBeginTime,
                                               &sEndTime);
        }

        /* BUG-23752 */
        IDV_TIME_GET( &sBeginTime );
        IDE_TEST( syncAllFile4Flush( aStatistics,
                                     sTBSSyncCnt ) != IDE_SUCCESS );
        sErrState = 5;

        IDV_TIME_GET(&sEndTime);
        sSyncTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

        mFlusherStat.applyTotalFlushPages( mIOBPos );

        // IOB�� ��� disk�� �������Ƿ� IOB�� �ʱ�ȭ�Ѵ�.
        mIOBPos = 0;
        mFlusherStat.applyINIOBCount( mIOBPos );

        SM_LSN_INIT( mMaxPageLSN );

        IDE_ASSERT( mMinRecoveryLSNMutex.lock(aStatistics) == IDE_SUCCESS );
        SM_LSN_MAX( mMinRecoveryLSN );
        IDE_ASSERT( mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS );

        /* BUG-32670    [sm-disk-resource] add IO Stat information
         * for analyzing storage performance.
         * Flush�� �ɸ� �ð����� ������. */
        mFlusherStat.applyTotalWriteTimeUSec( sWriteTime );
        mFlusherStat.applyTotalSyncTimeUSec( sSyncTime );
        mFlusherStat.applyTotalDWTimeUSec( sDWTime );
    }
    else
    {
        // IOB�� ��������Ƿ� �Ұ� ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( smERR_FATAL_PageFlushStopped, sErrState ) );

    if( sWaitEventState == 1 )
    {
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
    }

    return IDE_FAILURE;
}

/****************************************************************************
 * Description :  mFileInfoArray�� �ִ� File���� Sort���Ŀ�  Sync�Ѵ�.
 *
 *  aStatistics     - [IN] �������
 *  aSyncArrCnt     - [IN] Sync �� File Info �� ��
 ****************************************************************************/ 
IDE_RC sdsFlusher::syncAllFile4Flush( idvSQL * aStatistics,
                                      UInt     aSyncArrCnt )
{
    UInt            i;
    sdbSyncFileInfo sLstSyncFileInfo = { SC_NULL_SPACEID, 0 };

    idlOS::qsort( (void*)mFileInfoArray,
                  aSyncArrCnt,
                  ID_SIZEOF( sdbSyncFileInfo ),
                  sdsCompareFileInfo );

    for( i = 1 ; i < aSyncArrCnt ; i++ )
    {
        /* ���� File�� ���ؼ� �ι� Sync���� �ʰ� �ѹ��� �Ѵ�. */
        /* drop file�� ��� flusher job�� ����Ѵ�.
         * �׷��Ƿ� flusher������ drop file�� ������� �ʾƵ� �ȴ�.*/
        if (( sLstSyncFileInfo.mSpaceID != mFileInfoArray[i].mSpaceID ) ||
            ( sLstSyncFileInfo.mFileID  != mFileInfoArray[i].mFileID ))
        {
            IDE_ASSERT( mFileInfoArray[i].mSpaceID != SC_NULL_SPACEID );

            sLstSyncFileInfo = mFileInfoArray[i];

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

/****************************************************************************
 * Description : ���� �ϰ� �ִ� �۾��� �ִٸ� �۾����� �۾��� ����Ǵ� ���� ���
 *               ����. ���� ���ٸ� �ٷ� �����Ѵ�.
 ****************************************************************************/
void sdsFlusher::wait4OneJobDone()
{
    IDE_ASSERT( mRunningMutex.lock( NULL ) == IDE_SUCCESS );
    IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
}

/****************************************************************************
 * Description :
 *    �� flusher�� ���� ������ �����. �۾����̸� �ƹ��� ������ ���� �ʴ´�.
 *    ���������� ������ aWaken���� ��ȯ�ȴ�.
 * Implementation :
 *    flusher�� �۾����̾����� mRunningMutex�� ��µ� ������ ���̴�.
 *
 *  aWaken  - [OUT] flusher�� ���� �־ �����ٸ� ID_TRUE��,
 *                  flusher�� �۾����̾����� ID_FALSE�� ��ȯ�Ѵ�.
 ****************************************************************************/
IDE_RC sdsFlusher::wakeUpSleeping( idBool *aWaken )
{
    idBool sLocked;

    IDE_ASSERT( mRunningMutex.trylock(sLocked) == IDE_SUCCESS );

    if( sLocked == ID_TRUE )
    {
        IDE_TEST_RAISE( mCondVar.signal() != IDE_SUCCESS,
                        ERR_COND_SIGNAL );

        IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );

        *aWaken = ID_TRUE;
    }
    else
    {
        *aWaken = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_SIGNAL );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondSignal) );
        IDE_ASSERT( mRunningMutex.unlock() == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************************
 * Description :
 *    IOB�� ����� BCB�� �� recoveryLSN�� ���� ���� ���� ��´�.
 *    �� ���� checkpoint�� ���ȴ�.
 *    restart redo LSN�� �� ���� CPListSet�� minRecoveryLSN�� ���� ������
 *    �����ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] �ּ� recoveryLSN
 ****************************************************************************/
void sdsFlusher::getMinRecoveryLSN( idvSQL *aStatistics,
                                    smLSN  *aRet )
{
    IDE_ASSERT( mMinRecoveryLSNMutex.lock(aStatistics ) == IDE_SUCCESS );

    SM_GET_LSN( *aRet, mMinRecoveryLSN );

    IDE_ASSERT( mMinRecoveryLSNMutex.unlock() == IDE_SUCCESS );
}

/*****************************************************************************
 * Description :
 *  �ϳ��� �۾��� ����ģ flusher�� ����� �� �ð��� ������ �ش�.
 *
 *  aFlushedCount   - [IN]  flusher�� �ٷ��� flush�� ������ ����
 ****************************************************************************/
UInt sdsFlusher::getWaitInterval( ULong aFlushedCount )
{
    if( sdsFlushMgr::isBusyCondition() == ID_TRUE )
    {
        mWaitTime = 0;
    }
    else
    {
        if( aFlushedCount > 0 )
        {
            mWaitTime = smuProperty::getDefaultFlusherWaitSec();
        }
        else
        {
            // ������ �Ѱǵ� flush���� �ʾҴٸ�, ǫ ����.
            if( mWaitTime < smuProperty::getMaxFlusherWaitSec() )
            {
                mWaitTime++;
            }
        }
    }
    mFlusherStat.applyLastSleepSec(mWaitTime);

    return mWaitTime;
}

