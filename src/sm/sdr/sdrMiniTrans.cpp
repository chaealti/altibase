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
 * $Id: sdrMiniTrans.cpp 89495 2020-12-14 05:19:22Z emlee $
 *
 * Description :
 *
 * �� ������ mini-transaction�� ���� ���� �����̴�.
 *
 *
 * # Mtx�� logging ���� flush list ���
 *
 * MTX ����Ȳ  �α� ���          flush list ��� ����
 *-----------------------------------------------------------
 * temp table      no                  X latch page
 * ----------------------------------------------------------
 * redo ��         no                  X latch page
 * ----------------------------------------------------------
 * normal          yes                 X latch page ��
 *                                     begin, end LSN �� ������
 * ----------------------------------------------------------
 * normal          no   -> �̷� ����� mtx�� ������ �ʴ´�.
 *
 **********************************************************************/


#include <ide.h>

#include <smxTrans.h>
#include <smrLogMgr.h>
#include <sdb.h>

#include <sct.h>
#include <sdrReq.h>
#include <sdrMiniTrans.h>
#include <sdrMtxStack.h>
#include <sdptbDef.h>
#include <sdnIndexCTL.h>


/* --------------------------------------------------------------------
 * sdrMtxLatchMode�� ���� release�Լ�
 * ----------------------------------------------------------------- */
sdrMtxLatchItemApplyInfo sdrMiniTrans::mItemVector[SDR_MTX_RELEASE_FUNC_NUM] =
{
    // page x latch
    {
        &sdbBufferMgr::releasePage,
        &sdbBufferMgr::releasePageForRollback,
        &sdbBCB::isSamePageID,
        &sdbBCB::dump,
        SDR_MTX_BCB,
        "SDR_MTX_PAGE_X"
    },
    // page s latch
    {
        &sdbBufferMgr::releasePage,
        &sdbBufferMgr::releasePageForRollback,
        &sdbBCB::isSamePageID,
        &sdbBCB::dump,
        SDR_MTX_BCB,
        "SDR_MTX_PAGE_S"
    },
    // page no latch
    {
        &sdbBufferMgr::releasePage,
        &sdbBufferMgr::releasePageForRollback,
        &sdbBCB::isSamePageID,
        &sdbBCB::dump,
        SDR_MTX_BCB,
        "SDR_MTX_PAGE_NO"
    },
    // latch x
    {
		// wrapper
        &sdrMiniTrans::unlock,
        &sdrMiniTrans::unlock,
        &iduLatch::isLatchSame,
        &sdrMiniTrans::dump,
        SDR_MTX_LATCH,
        "SDR_MTX_LATCH_X"
    },
    // latch s
    {
		// wrapper
        &sdrMiniTrans::unlock,
        &sdrMiniTrans::unlock,
        &iduLatch::isLatchSame,
        &sdrMiniTrans::dump,
        SDR_MTX_LATCH,
        "SDR_MTX_LATCH_S"
    },
    // mutex
    {
        &iduMutex::unlock,
        &iduMutex::unlock,
        &iduMutex::isMutexSame,
        &iduMutex::dump,
        SDR_MTX_MUTEX,
        "SDR_MTX_MUTEX_X"
    }
};

/*PROJ-2162 RestartRiskReduction */
#if defined(DEBUG)
UInt sdrMiniTrans::mMtxRollbackTestSeq = 0;
#endif

/***********************************************************************
 * Description : sdrMiniTrans �ʱ�ȭ
 *
 * ����dynamic ���۴� �ý��� �����ÿ� ���� �ʱ�ȭ�� �Ǿ�� �ϸ�,
 * �Ʒ��� ���� latch ������ latch release ���� �ʱ�ȭ�� �ʿ��ϴ�.
 * + page latch�̸�  -> sdbBufferMgr::releasePage
 * + iduLatch�̸�    -> iduLatch::unlock
 * + iduMutex�̸�    -> object.unlock()
 *
 * sdrMtx �ڷᱸ���� �ʱ�ȭ�Ѵ�.
 * mtx ������ �ʱ�ȭ�ϰ�, Ʈ������� �α׹��۸� �ʱ�ȭ�Ѵ�.
 * mtx �������̽��� �׻� start, commit ������ ȣ��Ǿ�� �Ѵ�.
 * dml�� ���� �α� �ۼ��� ���� replication ������ ���� ��������
 * �ʱ�ȭ �Ѵ�.
 *
 * - 1st. code design
 *   + stack�� �ʱ�ȭ �Ѵ�.
 *   + log mode�� set�Ѵ�. -> setLoggingMode
 *   + trans�� set�Ѵ�.
 *   + if (logging mode ����̸�)
 *        log�� write�ϱ� ���� array�� initialize�Ѵ�.
 *
 * - 2nd. code design
 *   + stack�� �ʱ�ȭ �Ѵ�.      -> sdrMtxStack::initiailze
 *   + mtx �α� ��带 �����Ѵ�. -> sdrMiniTrans::setMtxMode
 *   + Ʈ������� �����Ѵ�.
 *   + undoable ���θ� �����Ѵ�.
 *   + modified�� �ʱ�ȭ�Ѵ�.
 *   + begin LSN, end LSN �ʱ�ȭ�Ѵ�.
 **********************************************************************/
IDE_RC sdrMiniTrans::initialize( idvSQL*       aStatistics,
                                 sdrMtx*       aMtx,
                                 void*         aTrans,
                                 sdrMtxLogMode aLogMode,
                                 idBool        aUndoable,
                                 UInt          aDLogAttr ) // disk log�� attr, smrDef ����
{
    UInt sState = 0;

    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMtxStack::initialize( &aMtx->mLatchStack )
              != IDE_SUCCESS );
    sState=1;

    IDE_TEST( sdrMtxStack::initialize( &aMtx->mXLatchPageStack )
              != IDE_SUCCESS );
    sState=2;

    aMtx->mStatSQL  = aStatistics;

    if( aLogMode == SDR_MTX_LOGGING )
    {

        // dynamic ���۸� �ʱ�ȭ�Ѵ�. �� �÷��װ� �����Ǹ� mtx����
        // fix �ϰ� �ִ� page�鿡 ���ؼ� unfix �ÿ� BCB��
        // LSN�� �����ǰ�, �� �Ŀ� page flush ��������
        // physical header�� mUpdateLSN�� �����Ѵ�.
        // ����, mtx�� SDR_MTX_NOLOGGING ���� �ʱ�ȭ��
        // �Ǿ��ٸ�, �� �Լ��� ȣ������ �ʴ´�.
        IDE_TEST( smuDynArray::initialize( &aMtx->mLogBuffer )
                  != IDE_SUCCESS );
        sState=3;
    }

    SMU_LIST_INIT_BASE(&(aMtx->mPendingJob));

    aMtx->mTrans     = aTrans;

    SM_LSN_INIT( aMtx->mBeginLSN );
    SM_LSN_INIT( aMtx->mEndLSN );

    aMtx->mOpType    = 0;
    aMtx->mDataCount = 0;

    aMtx->mDLogAttr  = 0;

    aMtx->mDLogAttr  = aDLogAttr;

    SM_LSN_INIT(aMtx->mPPrevLSN);

    aMtx->mTableOID  = SM_NULL_OID;
    aMtx->mRefOffset = 0;

    // mtx�� �����ϴ� tablespace�̴�.
    aMtx->mAccSpaceID = 0;
    aMtx->mLogCnt     = 0;

    /* Flag�� �ʱ�ȭ �Ѵ�. */
    aMtx->mFlag  = SDR_MTX_DEFAULT_INIT ;

    // TASK-2398 Log Compress
    //
    // �α� ���࿩�� (�⺻�� : ���� �ǽ� )
    //   ���� �α׸� �������� �ʵ��� ������ Tablespace�� ����
    //   �α׸� ����ϰԵǸ� �� Flag�� ID_FALSE�� �ٲٰ� �ȴ�.

    /* Logging ���� ������ */
    aMtx->mFlag &= ~SDR_MTX_LOGGING_MASK;
    if( aLogMode == SDR_MTX_LOGGING )
    {
        aMtx->mFlag |= SDR_MTX_LOGGING_ON;
    }
    else
    {
        aMtx->mFlag |= SDR_MTX_LOGGING_OFF;
    }

    /* Mtx ���� �� Property�� �ٲ� �� �ֱ� ������, �̸� �����ص�. */
    aMtx->mFlag &= ~SDR_MTX_STARTUP_BUG_DETECTOR_MASK;
    if( smuProperty::getSmEnableStartupBugDetector() == 1 )
    {
        aMtx->mFlag |= SDR_MTX_STARTUP_BUG_DETECTOR_ON;
    }
    else
    {
        aMtx->mFlag |= SDR_MTX_STARTUP_BUG_DETECTOR_OFF;
    }

    /* Undo���ɿ��θ� ������ */
    aMtx->mFlag &= ~SDR_MTX_UNDOABLE_MASK;
    if( aUndoable == ID_TRUE )
    {
        aMtx->mFlag |= SDR_MTX_UNDOABLE_ON;
    }
    else
    {
        aMtx->mFlag |= SDR_MTX_UNDOABLE_OFF;
    }


    /* BUG-32579 The MiniTransaction commit should not be used in
     * exception handling area. */
    aMtx->mRemainLogRecSize = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            IDE_ASSERT( smuDynArray::destroy( &aMtx->mLogBuffer )
                        == IDE_SUCCESS );

        case 2:
            IDE_ASSERT( sdrMtxStack::destroy( &aMtx->mXLatchPageStack )
                        == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( sdrMtxStack::destroy( &aMtx->mLatchStack )
                        == IDE_SUCCESS );

        default:
            break;
    }

    return IDE_FAILURE;

}

/* Mtx StartInfo�� �����Ѵ�. */
void sdrMiniTrans::makeStartInfo( sdrMtx* aMtx,
                                  sdrMtxStartInfo * aStartInfo )
{
    IDE_ASSERT( aMtx != NULL );
    IDE_ASSERT( aStartInfo != NULL );

    aStartInfo->mTrans   = getTrans( aMtx );
    aStartInfo->mLogMode = getLogMode( aMtx );
    return;
}


/***********************************************************************
 * Description : sdrMiniTrans ����
 **********************************************************************/
IDE_RC sdrMiniTrans::destroy( sdrMtx * aMtx )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    IDE_TEST( destroyPendingJob( aMtx ) 
              != IDE_SUCCESS );

    IDE_TEST( sdrMtxStack::destroy( &aMtx->mLatchStack )
              != IDE_SUCCESS );

    IDE_TEST( sdrMtxStack::destroy( &aMtx->mXLatchPageStack )
              != IDE_SUCCESS );

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        IDE_TEST( smuDynArray::destroy( &aMtx->mLogBuffer )
                  != IDE_SUCCESS );
    }

    aMtx->mFlag        = SDR_MTX_DEFAULT_DESTROY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}



/***********************************************************************
 * Description : mtx�� �ʱ�ȭ �� ����
 *
 * initialize redirect
 **********************************************************************/
IDE_RC sdrMiniTrans::begin( idvSQL *      aStatistics,
                            sdrMtx*       aMtx,
                            void*         aTrans,
                            sdrMtxLogMode aLogMode,
                            idBool        aUndoable,
                            UInt          aDLogAttr )
{

    IDE_TEST( initialize( aStatistics,
                          aMtx,
                          aTrans,
                          aLogMode,
                          aUndoable,
                          aDLogAttr )
               != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : mtx�� �ʱ�ȭ �� ����
 *
 * �ٸ� mtx�κ��� trans, logmode ������ ���
 * ���ο� trans�� �����Ѵ�.
 * latch, stack�� �����ϴ� ���� �ƴ�.
 **********************************************************************/
IDE_RC sdrMiniTrans::begin(idvSQL*          aStatistics,
                           sdrMtx*          aMtx,
                           sdrMtxStartInfo* aStartInfo,
                           idBool           aUndoable,
                           UInt             aDLogAttr)
{


    IDE_TEST( initialize( aStatistics,
                          aMtx,
                          aStartInfo->mTrans,
                          aStartInfo->mLogMode,
                          aUndoable,
                          aDLogAttr)
               != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : mtx�� commit�Ѵ�.
 *
 * mtx�� ���� Ʈ����� �α׹��ۿ� �ۼ��� �α׵��� �α����Ͽ� write�ϰ�,
 * ��� Item�� ���� latch item�� �����Ͽ� fix�ߴ� �������
 * unfix�ϴ� ���� �۾��� �Ѵ�. item�� �޸� ������ �޸� ������
 * ���� å���̴�.
 *
 * 1) BEGIN-LSN : mtx�� �α׸� ���� �α׿� �� �� ���� LSN
 * �� LSN�� recovery �ÿ� ������ LSN�� �����ϱ� ���Ͽ� ���ȴ�.
 * releasePage �ÿ� �� ���� BCB�� firstModifiedLSN�� ��������.
 *
 * 2) END-LSN : mtx�� �α׸� ���� �α׿� �� �� end LSN
 * �α� tail�� ������ LSN�� ����Ų��. releasePage �ÿ� �� ���� BCB��
 * lastModifiedLSN�� ��������. flush�� lastModifiedLSN������ �α׸�
 * �������̴�.
 *
 * redo�� �����Ҷ��� ��α����� ���������
 * redo�� ������ ����ƴ����� �������� ǥ���ؾ� �Ѵ�.
 * endLSN���ڴ� ���� redo �������̶�� NULL�� �ƴ� ���� set�Ǹ�,
 * �̸� flush�ÿ� �������� write�� �� �ֵ��� aMtx�� set�� �־�� �Ѵ�.
 * �⺻���� NULL
 *
 *
 * - 2st. code design
 *   + if ( logging mode��� )
 *     �α׸� ������ write �ϰ� Begin LSN, End LSN�� ��´�.
 *     ��쿡 ���� ������ ���� �αװ������� interface�� ȣ���Ѵ�.
 *     : redo �α׿� undo record�� redo-undo �α��϶�
 *       - smrLogMgr::writeDiskLogRec
 *     : MRDB�� �����ִ� �α��� �ʿ��� NTA �α��� ���
 *       - smrLogMgr::writeDiskNTALogRec
 *
 *   + while(��� ������ Item�� ����)
 *     �� Item�� pop�Ѵ�.-> pop
 *     item�� �����Ѵ�. -> releaseLatchItem
 *
 *   + if( logging mode�϶� )
 *        ��� �ڿ��� �����Ѵ�.
 *     -> sdrMtxStack::destroy
 **********************************************************************/
IDE_RC sdrMiniTrans::commit( sdrMtx   * aMtx,
                             UInt       aContType,
                             smLSN    * aEndLSN,   /* in */
                             UInt       aRedoType )
{

    sdrMtxLatchItem  *sItem;
    sdrMtxStackInfo   sMtxDPStack;
    UInt              sWriteLogOption;
    SInt              sState   = 0;
    UInt              sAttrLog = 0;
    smLSN            *sPrvLSNPtr;


    /* BUG-32579 The MiniTransaction commit should not be used in
     * exception handling area.
     * 
     * �κ������� �αװ� �������� �ȵȴ�. */
    IDE_ASSERT( aMtx->mRemainLogRecSize == 0 );

    sState = 1;

    if( ( aMtx->mFlag & SDR_MTX_LOG_SHOULD_COMPRESS_MASK ) ==
        SDR_MTX_LOG_SHOULD_COMPRESS_ON )
    {
        sWriteLogOption = SDR_REQ_DISK_LOG_WRITE_OP_COMPRESS_TRUE;
    }
    else
    {
        sWriteLogOption = SDR_REQ_DISK_LOG_WRITE_OP_COMPRESS_FALSE;
    }

    if ( aEndLSN != NULL )
    {
        aMtx->mEndLSN = *aEndLSN;
    }

    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    /* BUG-19122: Restart Recovery LSN�� �߸� ������ �� �ֽ��ϴ�.
     *
     * XLatch�� ���� �������� �α��ϱ����� Dirty�� ����Ѵ�. */
    if( sdrMtxStack::isEmpty( &aMtx->mXLatchPageStack ) != ID_TRUE)
    {
        IDE_TEST( sdrMtxStack::initialize( &sMtxDPStack )
                  != IDE_SUCCESS );
        sState = 2;
    }

    while( sdrMtxStack::isEmpty( &aMtx->mXLatchPageStack ) != ID_TRUE )
    {
        sItem = sdrMtxStack::pop( &( aMtx->mXLatchPageStack ) );

        IDE_TEST( sdbBufferMgr::setDirtyPage( (UChar*)( sItem->mObject ),
                                              aMtx )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMtxStack::push( &sMtxDPStack, sItem ) != IDE_SUCCESS );
    }

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        sAttrLog = SM_DLOG_ATTR_WRITELOG_MASK & aMtx->mDLogAttr;

        switch( sAttrLog )
        {
            case SM_DLOG_ATTR_TRANS_LOGBUFF:

                IDE_ASSERT( aMtx->mTrans != NULL );

                /* BUG-29262 TSS �Ҵ翡 ������ Ʈ������� COMMIT �α׸� ����ؾ� �մϴ�.
                 * Transaction log buffer�� ��ϵ� �αװ� ������ flush �ؾ��Ѵ�. */
                if( ((smxTrans*)aMtx->mTrans)->isLogWritten() == ID_TRUE )
                {
                    IDE_TEST( smrLogMgr::writeLog( aMtx->mStatSQL,
                                                   aMtx->mTrans,
                                                   ((smxTrans*)aMtx->mTrans)->getLogBuffer(),
                                                   NULL, // Prev LSN Ptr
                                                   &aMtx->mBeginLSN,
                                                   &aMtx->mEndLSN,
                                                   aMtx->mTableOID ) 
                               != IDE_SUCCESS);
                }
                break;
            case SM_DLOG_ATTR_MTX_LOGBUFF : /* xxx �Լ��� ������ */

                sAttrLog = SM_DLOG_ATTR_LOGTYPE_MASK & aMtx->mDLogAttr;

                // BUG-25128 �α� �Ȱ��� ��� CLR �α״� ��ϵǾ�� ��
                if( (isLogWritten(aMtx) == ID_TRUE) || (sAttrLog == SM_DLOG_ATTR_CLR) )
                {
                    switch( sAttrLog )
                    {
                        case SM_DLOG_ATTR_NTA:
                            IDE_TEST( smLayerCallback::writeDiskNTALogRec( aMtx->mStatSQL,
                                                                           aMtx->mTrans,
                                                                           &aMtx->mLogBuffer,
                                                                           sWriteLogOption,
                                                                           aMtx->mOpType,
                                                                           &(aMtx->mPPrevLSN),
                                                                           aMtx->mAccSpaceID,
                                                                           aMtx->mData,
                                                                           aMtx->mDataCount,
                                                                           &(aMtx->mBeginLSN),
                                                                           &(aMtx->mEndLSN),
                                                                           aMtx->mTableOID )
                                      != IDE_SUCCESS );
                            break;

                        case SM_DLOG_ATTR_REF_NTA:
                            IDE_TEST( smLayerCallback::writeDiskRefNTALogRec( aMtx->mStatSQL,
                                                                              aMtx->mTrans,
                                                                              &aMtx->mLogBuffer,
                                                                              sWriteLogOption,
                                                                              aMtx->mOpType,
                                                                              aMtx->mRefOffset,
                                                                              &(aMtx->mPPrevLSN),
                                                                              aMtx->mAccSpaceID,
                                                                              &(aMtx->mBeginLSN),
                                                                              &(aMtx->mEndLSN),
                                                                              aMtx->mTableOID )
                                      != IDE_SUCCESS );
                            break;

                        case SM_DLOG_ATTR_CLR:
                            IDE_TEST( smLayerCallback::writeDiskCMPSLogRec( aMtx->mStatSQL,
                                                                            aMtx->mTrans,
                                                                            &aMtx->mLogBuffer,
                                                                            sWriteLogOption,
                                                                            &(aMtx->mPPrevLSN),
                                                                            &(aMtx->mBeginLSN),
                                                                            &(aMtx->mEndLSN),
                                                                            aMtx->mTableOID )
                                      != IDE_SUCCESS );
                            break;

                        case SM_DLOG_ATTR_REDOONLY:
                        case SM_DLOG_ATTR_UNDOABLE:
                            if ( SM_IS_LSN_INIT( aMtx->mPPrevLSN ) )
                            {
                                sPrvLSNPtr = NULL;
                            }
                            else
                            {
                                sPrvLSNPtr = &aMtx->mPPrevLSN;
                            }

                            IDE_TEST( smLayerCallback::writeDiskLogRec( aMtx->mStatSQL,
                                                                        aMtx->mTrans,
                                                                        sPrvLSNPtr,
                                                                        &aMtx->mLogBuffer,
                                                                        sWriteLogOption,
                                                                        aMtx->mDLogAttr,
                                                                        aContType,
                                                                        aMtx->mRefOffset,
                                                                        aMtx->mTableOID,
                                                                        aRedoType,
                                                                        &(aMtx->mBeginLSN),
                                                                        &(aMtx->mEndLSN) )
                                      != IDE_SUCCESS );
                            break;
                        case SM_DLOG_ATTR_DUMMY:
                            IDE_TEST( smLayerCallback::writeDiskDummyLogRec( aMtx->mStatSQL,
                                                                             &aMtx->mLogBuffer,
                                                                             sWriteLogOption,
                                                                             aContType,
                                                                             aRedoType,
                                                                             aMtx->mTableOID,
                                                                             &aMtx->mBeginLSN,
                                                                             &aMtx->mEndLSN )
                                      != IDE_SUCCESS );
                            break;
                        default:
                            IDE_ASSERT( 0 );
                            break;
                    }
                }
                break;

            default:
                break;
        }
    }

    /* PROJ-2162 RestartRiskReduction
     * Minitransaction���� Log�� �ϳ� �� ��, �� Log�� ����
     * Redo�� ServiceThread�� ���� ����� �������� �˻��Ѵ�. */
    if( SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON ( aMtx->mFlag ) )
    {
        if( ( getLogMode(aMtx) == SDR_MTX_LOGGING ) &&
            ( isLogWritten(aMtx) == ID_TRUE ) &&
            ( smuDynArray::getSize( &aMtx->mLogBuffer ) > 0 ) &&
            ( isNologgingPersistent(aMtx) == ID_FALSE ) ) /*IndexBUBuild*/
        {
            if( sState < 2 )
            {
                /* ���°� 2���� ���� ����, DirtyPage�� ���ٰ� �ǴܵǾ�
                 * DPStack�� �ʱ�ȭ ���� ���� ����.
                 * ������ ���� ������, Dirty �ȵǾ� �־ Logging ������
                 * Dirtypage�� �������� �ϴ� ��ŭ, ���⼭ '�����'��
                 * �ʱ�ȭ�� ����� �Ѵ�. */
                IDE_TEST( sdrMtxStack::initialize( &sMtxDPStack )
                          != IDE_SUCCESS );
                sState = 2;
            }
            else
            {
                /* nothing to do ... */
            }

            IDE_ASSERT( validateDRDBRedo( aMtx,
                                          &sMtxDPStack,
                                          &aMtx->mLogBuffer ) == IDE_SUCCESS );
        }
    }

    /* PROJ-2162 RestartRiskReduction
     * Consistency ���� ������, PageLSN�� �������� �ʴ´�. */
    if( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) ||
        ( smuProperty::getCrashTolerance() == 2 ) )
    {
        /* DirtyPage�� PageLSN���� */
        if ( sState == 2 )
        {
            while( sdrMtxStack::isEmpty( &sMtxDPStack ) != ID_TRUE )
            {
                sItem = sdrMtxStack::pop( &sMtxDPStack );

                sdbBufferMgr::setPageLSN( sItem->mObject, aMtx );
                if ( SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON ( aMtx->mFlag ) )
                {
                    IDE_TEST( smuUtility::freeAlignedBuf( &(sItem->mPageCopy) )
                              != IDE_SUCCESS );
                }
            }
        }
    }

    /* ���� Latch�� Ǭ��. */
    while( sdrMtxStack::isEmpty( &aMtx->mLatchStack ) != ID_TRUE )
    {
        sItem = sdrMtxStack::pop( &( aMtx->mLatchStack ) );
        IDE_TEST( releaseLatchItem( aMtx, sItem ) != IDE_SUCCESS );
    }

    /* BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�.
     * Mtx�� Commit�ɶ� SpaceCache�� Free�� Extent�� ��ȯ�մϴ�. */ 
    IDE_TEST( executePendingJob( aMtx,
                                 ID_TRUE)  //aDoCommitOp
              != IDE_SUCCESS );

    if( aEndLSN != NULL )
    {
        SM_SET_LSN(*aEndLSN,
                   aMtx->mEndLSN.mFileNo,
                   aMtx->mEndLSN.mOffset);
    }

    if ( sState == 2 )
    {
        sState = 1;
        IDE_TEST( sdrMtxStack::destroy( &sMtxDPStack )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( destroy(aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-31006 - Debug information must be recorded when an exception 
     *             occurs from mini transaction commit or rollback.
     * mini transaction commit/rollback ���� �� ���н� ����� ���� ����ϰ�
     * ������ ���δ�. */

    /* dump sdrMtx */
    if ( sState != 0 )
    {
        dumpHex( aMtx );
        dump( aMtx, SDR_MTX_DUMP_DETAILED );
    }

    /* dump variable */
    ideLog::log( IDE_SERVER_0, "[ Dump variable ]" );
    ideLog::log( IDE_SERVER_0,
                 "ContType: %"ID_UINT32_FMT", "
                 "EndLSN: (%"ID_UINT32_FMT", %"ID_UINT32_FMT")",
                 "RedoType: %"ID_UINT32_FMT", "
                 "WriteLogOption: %"ID_UINT32_FMT", "
                 "State: %"ID_INT32_FMT", "
                 "AttrLog: %"ID_UINT32_FMT"",
                 aContType,
                 ( aEndLSN != NULL ) ? aEndLSN->mFileNo : 0,
                 ( aEndLSN != NULL ) ? aEndLSN->mOffset : 0,
                 aRedoType,
                 sWriteLogOption,
                 sState,
                 sAttrLog );

    if( sState == 2 )
    {
        /* dump sMtxDPStack */
        ideLog::log( IDE_SERVER_0, "[ Hex Dump MtxDPStack ]" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sMtxDPStack,
                        ID_SIZEOF(sdrMtxStackInfo) );

        IDE_ASSERT( sdrMtxStack::destroy( &sMtxDPStack )
                    == IDE_SUCCESS );
    }

    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : NOLOGGING Attribute ����
 * NOLOGGING mini-transaction�� Nologging Persistent attribute�� ����
 * Logging�� �������� Persistent�� �������� (TempPage�� �ƴ� ������).
 **********************************************************************/
void sdrMiniTrans::setNologgingPersistent( sdrMtx* aMtx )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    aMtx->mFlag &= ~SDR_MTX_NOLOGGING_ATTR_PERSISTENT_MASK;
    aMtx->mFlag |=  SDR_MTX_NOLOGGING_ATTR_PERSISTENT_ON;

    return;

}

/***********************************************************************
 * Description : NTA �α�
 **********************************************************************/
void sdrMiniTrans::setNTA( sdrMtx   * aMtx,
                           scSpaceID  aSpaceID,
                           UInt       aOpType,
                           smLSN*     aPPrevLSN,
                           ULong    * aArrData,
                           UInt       aDataCount )
{
    UInt   sLoop;

    aMtx->mAccSpaceID = aSpaceID;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_ASSERT( (aMtx->mDLogAttr & SM_DLOG_ATTR_LOGTYPE_MASK)
                 == SM_DLOG_ATTR_REDOONLY );
    IDE_ASSERT( aDataCount <= SM_DISK_NTALOG_DATA_COUNT );

    aMtx->mDLogAttr  |= SM_DLOG_ATTR_NTA;
    aMtx->mOpType     = aOpType;
    aMtx->mDataCount  = aDataCount;

    for ( sLoop = 0; sLoop < aDataCount; sLoop++ )
    {
        aMtx->mData[ sLoop ] = aArrData [ sLoop ];
    }

    SM_SET_LSN(aMtx->mPPrevLSN,
               aPPrevLSN->mFileNo,
               aPPrevLSN->mOffset);
}

/***********************************************************************
 * Description : Index NTA �α�
 **********************************************************************/
void sdrMiniTrans::setRefNTA( sdrMtx   * aMtx,
                              scSpaceID  aSpaceID,
                              UInt       aOpType,
                              smLSN*     aPPrevLSN )
{
    IDE_ASSERT( aMtx->mRefOffset != 0 );
    
    aMtx->mAccSpaceID = aSpaceID;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_ASSERT( (aMtx->mDLogAttr & SM_DLOG_ATTR_LOGTYPE_MASK)
                 == SM_DLOG_ATTR_REDOONLY );

    aMtx->mDLogAttr  |= SM_DLOG_ATTR_REF_NTA;
    aMtx->mOpType     = aOpType;

    SM_SET_LSN(aMtx->mPPrevLSN,
               aPPrevLSN->mFileNo,
               aPPrevLSN->mOffset);
}

void sdrMiniTrans::setNullNTA( sdrMtx   * aMtx,
                               scSpaceID  aSpaceID,
                               smLSN*     aPPrevLSN )
{
    aMtx->mAccSpaceID = aSpaceID;

    SM_SET_LSN(aMtx->mPPrevLSN,
               aPPrevLSN->mFileNo,
               aPPrevLSN->mOffset);
}

/***********************************************************************
 * Description : Ư�� ���꿡 ���� CLR �α� ����
 **********************************************************************/
void sdrMiniTrans::setCLR( sdrMtx* aMtx,
                           smLSN*  aPPrevLSN )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( (aMtx->mDLogAttr & SM_DLOG_ATTR_LOGTYPE_MASK)
                 == SM_DLOG_ATTR_REDOONLY );

    aMtx->mDLogAttr |= SM_DLOG_ATTR_CLR;

    SM_SET_LSN(aMtx->mPPrevLSN,
               aPPrevLSN->mFileNo,
               aPPrevLSN->mOffset);

    return;
}

/***********************************************************************
 * Description : DML���� undo/redo �α� ��ġ ���
 * ref offset�� �ϳ��� smrDiskLog�� ��ϵ� �������� disk �α׵� ��
 * replication sender�� �����Ͽ� �α׸� �ǵ��ϰų� transaction undo�ÿ�
 * �ǵ��ϴ� DML���� redo/undo �αװ� ��ϵ� ��ġ�� �ǹ��Ѵ�.
 **********************************************************************/
void sdrMiniTrans::setRefOffset( sdrMtx* aMtx,
                                 smOID   aTableOID )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_ASSERT( ( aMtx->mFlag & SDR_MTX_REFOFFSET_MASK ) 
                == SDR_MTX_REFOFFSET_OFF );

    aMtx->mRefOffset      = smuDynArray::getSize(&aMtx->mLogBuffer);
    aMtx->mTableOID       = aTableOID;
    aMtx->mFlag          |= SDR_MTX_REFOFFSET_ON;

    return;
}

/***********************************************************************
 *
 * Description :
 *
 * minitrans�� begin lsn�� return
 *
 * Implementation :
 *
 **********************************************************************/
void* sdrMiniTrans::getTrans( sdrMtx     * aMtx )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    return aMtx->mTrans;
}


/***********************************************************************
 *
 * Description :
 *
 * minitrans�� begin lsn�� return
 *
 * Implementation :
 *
 **********************************************************************/
void* sdrMiniTrans::getMtxTrans( void * aMtx )
{
    if ( aMtx != NULL )
    {
        IDE_DASSERT( validate((sdrMtx*)aMtx) == ID_TRUE );

        return ((sdrMtx*)aMtx)->mTrans;
    }

    return (void*)NULL;
}


/***********************************************************************
 * Description : mtx �α� ��带 ��´�.
 **********************************************************************/
sdrMtxLogMode sdrMiniTrans::getLogMode(sdrMtx*        aMtx)
{
    sdrMtxLogMode  sRet = SDR_MTX_LOGGING;

    if( ( aMtx->mFlag & SDR_MTX_LOGGING_MASK ) == SDR_MTX_LOGGING_ON )
    {
        sRet = SDR_MTX_LOGGING;
    }
    else
    {
        sRet = SDR_MTX_NOLOGGING;
    }

    return sRet;
}

/***********************************************************************
 * Description : disk log�� ���� attribute�� ��´�..
 **********************************************************************/
UInt sdrMiniTrans::getDLogAttr( sdrMtx * aMtx )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    return aMtx->mDLogAttr;
}

/***********************************************************************
 * Description : �α� ���ڵ带 �ʱ�ȭ�Ѵ�.
 *
 * �α� Ÿ�԰� rid�� ����ϰ�, ����Ŀ� �α� ������ offset��
 * ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 * log hdr�� �����Ѵ�.
 * log hdr�� write�Ѵ�.
 *
 **********************************************************************/
IDE_RC sdrMiniTrans::initLogRec( sdrMtx*        aMtx,
                                 scGRID         aWriteGRID,
                                 UInt           aLength,
                                 sdrLogType     aType )
{
    sdrLogHdr   sLogHdr;
    scSpaceID   sSpaceID;

    /* BUG-32579 The MiniTransaction commit should not be used in
     * exception handling area. 
     *
     * �κ������� �αװ� �������� �ȵȴ�. */

    IDE_ASSERT( aMtx->mRemainLogRecSize == 0 );

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( getLogMode(aMtx) == SDR_MTX_LOGGING );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace(SC_MAKE_SPACE(aWriteGRID))
                 == ID_TRUE );

    sLogHdr.mGRID   = aWriteGRID;
    sLogHdr.mLength = aLength;
    sLogHdr.mType   = aType;

    sSpaceID = SC_MAKE_SPACE(aWriteGRID);

    if ( aMtx->mLogCnt == 0 )
    {
        aMtx->mAccSpaceID = sSpaceID;
    }
    else
    {
        // undo �� Ȯ���ϸ� ���� Ȯ������ ����. ���� ������ �ְ�..
        if ( sctTableSpaceMgr::isUndoTableSpace( sSpaceID ) == ID_FALSE )
        {
            IDE_ASSERT( sSpaceID == aMtx->mAccSpaceID );
            IDE_ASSERT( sctTableSpaceMgr::isDiskTableSpace(sSpaceID) == ID_TRUE ); 
        }
    }

    // TASK-2398 Log Compression
    // Tablespace�� Log Compression�� ���� �ʵ��� ������ ���
    // �α� ������ ���� �ʵ��� ����
    IDE_TEST( checkTBSLogCompAttr(aMtx, sSpaceID) != IDE_SUCCESS );

    aMtx->mLogCnt++;

    aMtx->mRemainLogRecSize = ID_SIZEOF(sdrLogHdr) + aLength;
        
    IDE_TEST( write( aMtx, &sLogHdr, (UInt)ID_SIZEOF(sdrLogHdr) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� Log Compression�� ���� �ʵ��� ������ ���
   �α� ������ ���� �ʵ��� ����

   [IN] aMtx     - Mini Transaction
   [IN] aSpaceID - Mini Transaction�� �α��Ϸ��� �����Ͱ� ����
                   Tablespace�� ID
*/
IDE_RC sdrMiniTrans::checkTBSLogCompAttr( sdrMtx*      aMtx,
                                          scSpaceID    aSpaceID )
{
    idBool     sShouldCompress = ID_TRUE;

    IDE_DASSERT( aMtx != NULL );

    // ������ �ϵ��� �����Ǿ� �ִ� ��쿡�� üũ
    if( ( aMtx->mFlag & SDR_MTX_LOG_SHOULD_COMPRESS_MASK ) ==
        SDR_MTX_LOG_SHOULD_COMPRESS_ON )
    {
        // ù��° ������ Tablespace�̰ų�
        // ù��° ������ Tablespace�� �ٸ� Tablespace���
        // Tablespace�� �α� ���� ���� üũ
        if ( (aMtx->mLogCnt == 0) ||
             (aMtx->mAccSpaceID != aSpaceID ) )
        {
            IDE_TEST( sctTableSpaceMgr::getSpaceLogCompFlag(
                          aSpaceID,
                          & sShouldCompress )
                      != IDE_SUCCESS );

            aMtx->mFlag &= ~SDR_MTX_LOG_SHOULD_COMPRESS_MASK;
            if( sShouldCompress == ID_TRUE )
            {
                aMtx->mFlag |= SDR_MTX_LOG_SHOULD_COMPRESS_ON;
            }
            else
            {
                aMtx->mFlag |= SDR_MTX_LOG_SHOULD_COMPRESS_OFF;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * RID ����
 * multiple column�� redo log�� write�� �� ���ȴ�.
 **********************************************************************/
IDE_RC sdrMiniTrans::writeLogRec( sdrMtx*      aMtx,
                                  scGRID       aWriteGRID,
                                  void*        aValue,
                                  UInt         aLength,
                                  sdrLogType   aLogType )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( SC_GRID_IS_NOT_NULL(aWriteGRID) );

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        IDE_TEST( initLogRec( aMtx,
                              aWriteGRID,
                              aLength,
                              aLogType )
                  != IDE_SUCCESS );


        if( aValue != NULL )
        {
            IDE_DASSERT( aLength > 0 );

            IDE_TEST( write( aMtx, aValue, aLength )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page ����κп� ���� �α׸� ����Ѵ�.
 *
 * �α� header �� body�� ���ÿ� mtx �α� ���ۿ� write
 * write �ÿ��� �ش� ���� �����ӿ� x-latch�� mtx stack��
 * �־�� �Ѵ�.
 *
 * - 2nd. code design
 *   + �α� Ÿ�԰� RID�� �ʱ�ȭ�Ѵ�.  -> sdrMiniTrans::initLogRec
 *   + value�� log buffer�� write�Ѵ� -> sdrMiniTrans::write
 **********************************************************************/
IDE_RC sdrMiniTrans::writeLogRec( sdrMtx*      aMtx,
                                  UChar*       aWritePtr,
                                  void*        aValue,
                                  UInt         aLength,
                                  sdrLogType   aLogType )
{
    idBool    sIsTablePageType = ID_FALSE;
    scGRID    sGRID;
    sdRID     sRID;
    scSpaceID sSpaceID;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( aWritePtr != NULL );

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        if ( (aMtx->mDLogAttr & SM_DLOG_ATTR_EXCEPT_MASK)
             == SM_DLOG_ATTR_EXCEPT_INSERT_APPEND_PAGE )
        {
            sIsTablePageType = smLayerCallback::isTablePageType( aWritePtr );

            if( sIsTablePageType == ID_TRUE )
            {
                //----------------------------
                // PROJ-1566
                // insert�� append ������� ����Ǵ� ���,
                // consistent log�� �����ϰ� page�� ���Ͽ� logging ���� ����
                // page flush �� ��, page ��ü�� ���Ͽ� logging �ϵ��� �Ǿ�����
                //----------------------------

                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                            "write page log in DPath-Insert "
                            "aLogType %"ID_UINT32_FMT" sIsTablePageType %"ID_UINT32_FMT"",
                            aLogType,  sIsTablePageType );

                IDE_ASSERT( 0 );
            }
        }
        // table page type�� �ƴϸ� ( extent or segment ),
        // logging  ����

        sRID     = smLayerCallback::getRIDFromPagePtr( aWritePtr );
        sSpaceID = smLayerCallback::getSpaceID( smLayerCallback::getPageStartPtr( aWritePtr ) );

        SC_MAKE_GRID( sGRID,
                      sSpaceID,
                      SD_MAKE_PID(sRID),
                      SD_MAKE_OFFSET(sRID));

        IDE_TEST( writeLogRec(aMtx,
                              sGRID,
                              aValue,
                              aLength,
                              aLogType )
                  != IDE_SUCCESS );
    }
    else
    {
        // SDR_MTX_NOLOGGING
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : page�� ���� ����� �Բ� 1, 2, 4, 8 byte �α׵� ����Ѵ�.
 *
 * logtype�� �� byte�� �´� ���ڷ� �Ѿ�´�. ��, logtype�� value��
 * ũ��ε� ���ȴ�. SDR_1BYTE -> 1, SDR_2BYTE->2, SDR_4BYTE->4,
 * SDR_8BYTE->8 �̷� ������ ���ǵȴ�.
 *
 * ���� : �� �Լ��� page�� ���� �����Ѵ�.
 *
 * - 2st. code design
 *   + !!) Ư�� page�� ���� dest �����Ϳ� ���� �α�Ÿ�Կ� ����
 *     Ư�� ���̸�ŭ memcpy�Ѵ�.
 *   + type�� RID�� �����Ѵ�      -> sdrMiniTrans::initLogRec
 *   + value�� write�Ѵ�.         -> sdrMiniTrans::write
 *   + page�� value�� write�Ѵ�.  -> idlOS::memcpy
 **********************************************************************/
IDE_RC sdrMiniTrans::writeNBytes( void*           aMtx,
                                  UChar*          aWritePtr,
                                  void*           aValue,
                                  UInt            aLogType )
{
    idBool     sIsTablePageType = ID_FALSE;
    UInt       sLength = aLogType;
    sdrMtx    *sMtx = (sdrMtx *)aMtx;
    scGRID     sGRID;
    sdRID      sRID;
    scSpaceID  sSpaceID;


    IDE_DASSERT( validate(sMtx ) == ID_TRUE );
    IDE_DASSERT( aWritePtr != NULL );
    IDE_DASSERT( aValue != NULL );
    IDE_DASSERT( aLogType == SDR_SDP_1BYTE ||
                 aLogType == SDR_SDP_2BYTE ||
                 aLogType == SDR_SDP_4BYTE ||
                 aLogType == SDR_SDP_8BYTE );

    if( getLogMode(sMtx) == SDR_MTX_LOGGING )
    {
        if ( (sMtx->mDLogAttr & SM_DLOG_ATTR_EXCEPT_MASK)
             == SM_DLOG_ATTR_EXCEPT_INSERT_APPEND_PAGE )
        {
             sIsTablePageType = smLayerCallback::isTablePageType( aWritePtr );
        }
        else
        {
             sIsTablePageType = ID_FALSE;
        }

        if ( sIsTablePageType == ID_FALSE )
        {
            sRID     = smLayerCallback::getRIDFromPagePtr( aWritePtr );
            sSpaceID = smLayerCallback::getSpaceID( smLayerCallback::getPageStartPtr( aWritePtr ) );

            SC_MAKE_GRID( sGRID,
                          sSpaceID,
                          SD_MAKE_PID(sRID),
                          SD_MAKE_OFFSET(sRID));

            IDE_TEST( initLogRec(sMtx,
                                 sGRID,
                                 sLength,
                                 (sdrLogType)aLogType )
                  != IDE_SUCCESS );

            IDE_TEST( write(sMtx,
                            aValue,
                            sLength) != IDE_SUCCESS );
        }
        else
        {
            //----------------------------
            // PROJ-1566
            // insert�� append ������� ����Ǵ� ���,
            // page�� ���Ͽ� logging ���� ����
            // page flush �� ��, page ��ü�� ���Ͽ� logging �ϵ��� �Ǿ�����
            //----------------------------
        }
    }
    else
    {
        // SDR_MTX_NOLOGGING
    }
    
    //BUG-23471 : overlap in memcpy
    if(aWritePtr != aValue)
    {
        idlOS::memcpy( aWritePtr, aValue, sLength );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : log�� write�Ѵ�.
 *
 * mtx�� dynamic �α� ���ۿ� Ư�� ������ value�� ����Ѵ�.
 *
 * - 2nd. code design
 *   + Ư�� ũ���� value�� mtx �α� ���ۿ� ����Ѵ�. -> smuDynBuffer::store
 **********************************************************************/
IDE_RC sdrMiniTrans::write(sdrMtx*        aMtx,
                           void*          aValue,
                           UInt           aLength)
{

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( aValue != NULL );

#if defined(DEBUG)
    /* PROJ-2162 RestartRiskReduction
     * __SM_MTX_ROLLBACK_TEST ���� �����Ǹ� (0�� �ƴϸ�) �� ����
     * MtxLogWrite�ø��� �����Ѵ�. �׸��� �� Property���� �����ϸ�
     * ������ ����ó���Ѵ�. */
    if( smuProperty::getSmMtxRollbackTest() > 0 )
    {
        mMtxRollbackTestSeq ++;
        if( mMtxRollbackTestSeq >= smuProperty::getSmMtxRollbackTest() )
        {
            mMtxRollbackTestSeq = 0;
            smuProperty::resetSmMtxRollbackTest();

            ideLog::logCallStack( IDE_SM_0 );
            IDE_RAISE( ABORT_INTERNAL_ERROR );
        }
        else
        {
            /* nothing to do... */
        }
    }
#endif

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        /* �ʱ� ������ ũ�⸸ŭ�� Log�� ������ �� */
        if( aMtx->mRemainLogRecSize < aLength )
        {
            ideLog::log( IDE_DUMP_0,
                        "ESTIMATED SIZE : %"ID_UINT32_FMT"\n"
                        "VALUE          : %"ID_UINT32_FMT"",
                        aMtx->mRemainLogRecSize,
                        aLength );
            dumpHex( aMtx );
            dump( aMtx, SDR_MTX_DUMP_DETAILED );
            IDE_ASSERT( 0 );
        }
        aMtx->mRemainLogRecSize -= aLength;

        /* �޸� ���� �ܿ��� ������ ������ ������,
         * �޸� ������ ���� ���ܵ� �Ͼ ���ɼ��� ���� ����. */
        IDE_ERROR( smuDynArray::store( &aMtx->mLogBuffer,
                                       aValue,
                                       aLength )
                   == IDE_SUCCESS );
    }

    return IDE_SUCCESS;
#if defined(DEBUG)
    IDE_EXCEPTION( ABORT_INTERNAL_ERROR )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
    }
#endif
    IDE_EXCEPTION_END;

    if( ( aMtx->mFlag & SDR_MTX_UNDOABLE_MASK ) == SDR_MTX_UNDOABLE_OFF )
    {
        ideLog::logMem( IDE_DUMP_0, 
                        (UChar*)aValue, 
                        aLength,
                        "LENGTH : %"ID_UINT32_FMT"",
                        aLength );
        dumpHex( aMtx );
        dump( aMtx, SDR_MTX_DUMP_DETAILED );

        /* PROJ-2162 RestartRiskReduction
         * Undo �Ұ����� �����̹Ƿ� ���������� ��Ų��. */
        IDE_ASSERT( 0 );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : mtx stack�� latch item(object�� ��ġ ���)�� �ִ´�.
 *
 * - 1st. code design
 *   + latch stack�� latch item�� �ִ´�. -> sdrMtxStack::push
 **********************************************************************/
IDE_RC sdrMiniTrans::push(void*         aMtx,
                          void*         aObject,
                          UInt          aLatchMode)
{
    sdrMtxLatchItem sPushItem;
    sdrMtx *sMtx = (sdrMtx *)aMtx;
    sdrMtxLatchMode sLatchMode = (sdrMtxLatchMode)aLatchMode;

    IDE_DASSERT( validate(sMtx) == ID_TRUE );
    IDE_DASSERT( aObject != NULL );
    IDE_DASSERT( validate(sMtx) == ID_TRUE );

    sPushItem.mObject    = aObject;
    sPushItem.mLatchMode = sLatchMode;

    IDE_TEST( sdrMtxStack::push( &sMtx->mLatchStack,
                                 &sPushItem )
              != IDE_SUCCESS );

    if( aLatchMode == SDR_MTX_PAGE_X )
    {
        IDE_TEST( setDirtyPage( aMtx,
                                ((sdbBCB*)aObject)->mFrame )
                  != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC sdrMiniTrans::pushPage(void*         aMtx,
                              void*         aObject,
                              UInt          aLatchMode)
{
    return push( aMtx,
                 sdbBufferMgr::getBCBFromPagePtr( (UChar*)aObject ),
                 aLatchMode );
}

IDE_RC sdrMiniTrans::setDirtyPage( void*    aMtx,
                                   UChar*   aPagePtr )
{
    sdrMtxLatchItem sPushItem;
    sdrMtx *sMtx = (sdrMtx *)aMtx;

    sPushItem.mObject = sdbBufferMgr::getBCBFromPagePtr( aPagePtr );
    sPushItem.mLatchMode = SDR_MTX_PAGE_X;

    /* �ߺ����� */
    if ( existObject( &sMtx->mXLatchPageStack,
                      sPushItem.mObject, 
                      SDR_MTX_BCB ) == NULL )
    {
        /* Property ������������, ���纻�� ������ */
        if ( SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON( sMtx->mFlag ) )
        {
            IDE_TEST( smuUtility::allocAlignedBuf( IDU_MEM_SM_SDR,
                                                   SD_PAGE_SIZE,
                                                   SD_PAGE_SIZE,
                                                   &sPushItem.mPageCopy )
                      != IDE_SUCCESS );

            idlOS::memcpy( sPushItem.mPageCopy.mAlignPtr,
                           aPagePtr,
                           SD_PAGE_SIZE );
        }

        IDE_TEST( sdrMtxStack::push( &sMtx->mXLatchPageStack,
                                     &sPushItem )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/****************************************************************************
 * Description : Mini-transaction�� Commit(�Ǵ� Rollback)�ÿ� ����� �ϵ�
 *               �� ����Ѵ�.
 *
 * BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
 *
 * aMtx         [in] PendingJob�� �� ��� Mini-transaction 
 * aIsCommitJob [in] Commit�� ����� ���ΰ�?(T), Rollback�� ����� ���ΰ�(F)
 * aFreeData    [in] Destroy�� data�� Free���� ���ΰ�?
 * aPendingFunc [in] ���Ŀ� ������ ���� ������ �Լ� ������
 * aData        [in] ���� ������ aPendingFunc���� ���޵Ǵ� ������.
 ***************************************************************************/
IDE_RC sdrMiniTrans::addPendingJob( void              * aMtx,
                                    idBool              aIsCommitJob,
                                    sdrMtxPendingFunc   aPendingFunc,
                                    void              * aData )
{
    sdrMtx           * sMtx = (sdrMtx *)aMtx;
    sdrMtxPendingJob * sPendingJob; 
    smuList          * sNewNode;
    UInt               sState = 0;

    IDE_DASSERT( sMtx         != NULL );
    IDE_DASSERT( aPendingFunc != NULL );

    /* TC/Server/LimitEnv/sm/sdr/sdrMiniTrans_addPendingJob_calloc1.sql */
    IDU_FIT_POINT_RAISE( "sdrMiniTrans::addPendingJob::calloc1",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SDR, 
                                       1, 
                                       ID_SIZEOF(smuList),
                                       (void**)&sNewNode ) 
                    != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    SMU_LIST_INIT_BASE(sNewNode);

    /* TC/Server/LimitEnv/sm/sdr/sdrMiniTrans_addPendingJob_calloc2.sql */
    IDU_FIT_POINT_RAISE( "sdrMiniTrans::addPendingJob::calloc2",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SDR,
                                       1,
                                       ID_SIZEOF(sdrMtxPendingJob),
                                       (void**)&sPendingJob) 
                    != IDE_SUCCESS,
                    insufficient_memory );
    sState = 2;

    sPendingJob->mIsCommitJob = aIsCommitJob;
    sPendingJob->mPendingFunc = aPendingFunc;
    sPendingJob->mData        = aData;

    sNewNode->mData = sPendingJob;
    
    SMU_LIST_ADD_LAST(&(sMtx->mPendingJob), sNewNode );
    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sState )
    {
        case 2 :
            IDE_ASSERT( iduMemMgr::free(sPendingJob) == IDE_SUCCESS );
        case 1 :
            IDE_ASSERT( iduMemMgr::free(sNewNode) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : Mini-transaction�� Commit(�Ǵ� Rollback)�ÿ� ����� �ϵ�
 *               ��, ��Ͻÿ� ���� ���� �Լ��� ȣ���Ͽ� �����Ѵ�.
 *
 * BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
 *
 * aMtx         [in] PendingJob�� �޸� Mini-transaction 
 * aDoCommitJob [in] Commit�����ΰ�? (T), Rollback�����ΰ�?(F)
 ***************************************************************************/
IDE_RC sdrMiniTrans::executePendingJob(void   * aMtx,
                                       idBool   aDoCommitJob)
{
    sdrMtx           * sMtx = (sdrMtx *)aMtx;
    smuList          * sOpNode;
    smuList          * sBaseNode;
    sdrMtxPendingJob * sPendingJob; 

    sBaseNode = &(sMtx->mPendingJob);

    for ( sOpNode = SMU_LIST_GET_FIRST(sBaseNode);
          sOpNode != sBaseNode;
          sOpNode = SMU_LIST_GET_NEXT(sOpNode) )
    {
        sPendingJob = (sdrMtxPendingJob*)sOpNode->mData;

        if( sPendingJob->mIsCommitJob == aDoCommitJob )
        {
            IDE_TEST( sPendingJob->mPendingFunc( sPendingJob->mData )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : PendingJob ����Ʈ �� ����ü, Data�� ���� �����Ѵ�.
 *
 * BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
 *
 * aMtx         [in] PendingJob�� ���� ��� Mini-transaction 
 ***************************************************************************/
IDE_RC sdrMiniTrans::destroyPendingJob( void * aMtx )
{
    sdrMtx           * sMtx = (sdrMtx *)aMtx;
    smuList          * sOpNode;
    smuList          * sBaseNode;
    smuList          * sNextNode;
    sdrMtxPendingJob * sPendingJob;
    UInt               sState = 0;

    sBaseNode = &(sMtx->mPendingJob);

    for ( sOpNode = SMU_LIST_GET_FIRST(sBaseNode);
          sOpNode != sBaseNode;
          sOpNode = sNextNode )
    {
        sNextNode = SMU_LIST_GET_NEXT(sOpNode);
        SMU_LIST_DELETE(sOpNode);

        sPendingJob = (sdrMtxPendingJob*)sOpNode->mData;

        sState = 1;
        IDE_TEST(iduMemMgr::free(sPendingJob) != IDE_SUCCESS);

        sState = 2;
        IDE_TEST(iduMemMgr::free(sOpNode) != IDE_SUCCESS);
        sState = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUGBUG
     * �� ���� �� ���� PendingJob List�� ������ ����̴�.
     *
     * ����� ����Ʈ�� ����Ǿ� �ִµ� �̹� �������� Free�ؼ� �߻��� �����
     * ���� �ְ�,
     * ����Ʈ�� �����־, ������ ��ü�� Free�ϴ� ���� ���� �ִ�.
     *
     * ���� ��� ����Ʈ�� �����ϸ鼭 Free�� ������ �� ���� ��Ȳ�̸�, �̿�
     * ���� �޸� Leak�� �߻��� �� ������, ���� ������ ����. */

    switch( sState )
    {
    case 0:
        break;
    case 1:
        ideLog::log( IDE_SERVER_0, "Invalid pendingJob structure -%"ID_POINTER_FMT"\n\
     mIsCommitJob -%"ID_UINT32_FMT" \n\
     mPendingFunc -%"ID_UINT32_FMT" \n\
     mData        -%"ID_UINT32_FMT" \n ",
                     sPendingJob,
                     sPendingJob->mIsCommitJob ,
                     sPendingJob->mPendingFunc ,
                     sPendingJob->mData );
        break;
    case 2:
        ideLog::log( IDE_SERVER_0, "Invalid pendingJob list -%"ID_POINTER_FMT" "
                     "prev-%"ID_POINTER_FMT" next-%"ID_POINTER_FMT"\n",
                     sOpNode,
                     SMU_LIST_GET_PREV(sOpNode),
                     SMU_LIST_GET_NEXT(sOpNode) );
        break;

    }
    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

idBool sdrMiniTrans::needMtxRollback( sdrMtx * aMtx )
{
    idBool sRet = ID_FALSE;

    if ( ( getLogMode(aMtx) == SDR_MTX_LOGGING )     &&
         ( isNologgingPersistent(aMtx) == ID_FALSE ) &&  /*IndexBUBuild*/
         ( ( aMtx->mFlag & SDR_MTX_IGNORE_UNDO_MASK ) 
           == SDR_MTX_IGNORE_UNDO_OFF ) )
    {
        /* ��ϵ� Log�� �ְų�,
         * LogRec�� ������ �غ��߰ų�,
         * XLatch�� ���� �������� ������ Rollback�� �ʿ��մϴ�. */
        if ( ( isLogWritten(aMtx) == ID_TRUE ) ||
             ( aMtx->mRemainLogRecSize > 0 )   || 
             ( sdrMtxStack::isEmpty(&aMtx->mXLatchPageStack) == ID_FALSE ) )
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = ID_FALSE;
        }
    }
    else
    {
        /* Nologging�̰� Ignroe���� �ǹ� �����ϴ�. */
        sRet = ID_FALSE;
    }

    return sRet;
}

/***********************************************************************
 * Description :
 *
 * ��� undo log buffer�� ������ �ݿ��Ѵ�.
 * ��� latch�� �����Ѵ�.
 * �� �Լ��� begin - commit ������ abort�� ���Ͽ�
 * exception �� �߻����� �� ȣ��ȴ�.
 *
 * ��α��϶��� �ƿ� �� �Լ��� ���� �� ����.
 * ���� ��α��� ��쿡�� abort�� �Ͼ���� �ȵȴ�.
 *
 * Implementation :
 *
 * undo Log Buffer
 *
 * while( ��� stack item�� ���� )
 *     pop
 *     release
 *
 *
 *
 **********************************************************************/
IDE_RC sdrMiniTrans::rollback( sdrMtx * aMtx )
{
    idvSQL                  sDummyStatistics;
    UInt                    sLogSize        = 0;
    SInt                    sParseOffset    = 0;
    sdrMtxLatchItem        *sItem;
    sdbBCB                 *sTargetBCB;
    idBool                  sSuccess;
    UInt                    sState          = 0;
    ULong                   sSPIDArray[ SDR_MAX_MTX_STACK_SIZE ];
    UInt                    sSPIDCount = 0;
    smuAlignBuf             sPagePtr;
    sdbBCB                * sBCB;
    UInt                    i;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    IDE_TEST_CONT( ( aMtx->mFlag & SDR_MTX_INITIALIZED_MASK ) ==
                   SDR_MTX_INITIALIZED_OFF, SKIP );
    sState = 1;

    if ( needMtxRollback( aMtx ) == ID_TRUE )
    {
        if( ( ( aMtx->mFlag & SDR_MTX_UNDOABLE_MASK ) == SDR_MTX_UNDOABLE_OFF ) &&
            ( ( smrRecoveryMgr::isRestart() == ID_FALSE ) ) )
        {
            dumpHex( aMtx );
            dump( aMtx, SDR_MTX_DUMP_DETAILED );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* CanUndo�� False��, Restart ���������� �����Ѵ�.
             * Restart ���������� Rollback�� �����ϸ�
             * ����� �����ϱ�. */
        }

        /* OnlineRedo�� ���� Undo�� */
        idlOS::memset( &sDummyStatistics, 0, ID_SIZEOF( idvSQL ) );

        while( sdrMtxStack::isEmpty(&aMtx->mXLatchPageStack) != ID_TRUE )
        {
            sItem = sdrMtxStack::pop( &( aMtx->mXLatchPageStack ) );

            sTargetBCB = (sdbBCB*)sItem->mObject ;

#if defined(DEBUG)
            /* MtxRollback�� ���� �߻����� �ʴ� ���̴�. ���� �߻���
             * ��� �̻��� �־����� ����ϱ� ���� ������ �����. */
            ideLog::logCallStack( IDE_SM_0 );
            ideLog::log( IDE_SM_0,
                         "-----------------------------------------------"
                         "sdrMiniTrans::rollback "
                         "SPACEID:%"ID_UINT32_FMT""
                         ",PAGEID:%"ID_UINT32_FMT""
                         "-----------------------------------------------",
                         sTargetBCB->mSpaceID,
                         sTargetBCB->mPageID );
#endif
            IDE_TEST( smuUtility::allocAlignedBuf( IDU_MEM_SM_SDR,
                                                   SD_PAGE_SIZE,
                                                   SD_PAGE_SIZE,
                                                   &sPagePtr )
                      != IDE_SUCCESS );

            sTargetBCB->lockBCBMutex( &sDummyStatistics );
            IDE_TEST( sdrRedoMgr::generatePageUsingOnlineRedo( 
                                                    &sDummyStatistics,
                                                    sTargetBCB->mSpaceID,
                                                    sTargetBCB->mPageID,
                                                    ID_TRUE, // aReadFromDisk
                                                    sPagePtr.mAlignPtr,
                                                    &sSuccess )
                      != IDE_SUCCESS );
            idlOS::memcpy( sTargetBCB->mFrame,
                           sPagePtr.mAlignPtr,
                           SD_PAGE_SIZE );
            IDE_TEST( smuUtility::freeAlignedBuf( &sPagePtr )
                      != IDE_SUCCESS );

            /* Frame�� BCB ������ ������ */
            sdbBufferMgr::getPool()->setFrameInfoAfterRecoverPage( 
                                                        sTargetBCB,
                                                        ID_TRUE );  // check to online tablespace
            sTargetBCB->unlockBCBMutex();
        }
    }
    else
    {
        /* Page�� ���� Rollback�� ���� ���� ���, ���� ���� �ʾƵ�
         * �Ǵ��� ������.
         * �� �̶� Logging�� Persistent, �� Logging�� �ǹ��ִ� ��
         * �쿩�� �Ѵ�.*/
        if ( ( SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON (aMtx->mFlag) ) &&
             ( getLogMode(aMtx) == SDR_MTX_LOGGING )              &&
             ( isNologgingPersistent(aMtx) == ID_FALSE ) )
        {
            idlOS::memset( &sDummyStatistics, 0, ID_SIZEOF( idvSQL ) );
            for( i = 0 ; i < aMtx->mXLatchPageStack.mCurrSize ; i ++ )
            {
                sBCB = (sdbBCB*)(aMtx->mXLatchPageStack.mArray[ i ].mObject);

                IDE_TEST( validateDRDBRedoInternal( &sDummyStatistics,
                                                    aMtx,
                                                    &(aMtx->mXLatchPageStack),
                                                    (ULong*)sSPIDArray,
                                                    &sSPIDCount,
                                                    sBCB->mSpaceID,
                                                    sBCB->mPageID )
                          != IDE_SUCCESS );
            }
        }
        /* XLatchPageStack ���� */
        while( sdrMtxStack::isEmpty(&aMtx->mXLatchPageStack) != ID_TRUE )
        {
            sItem = sdrMtxStack::pop( &( aMtx->mXLatchPageStack ) );

        }
    }

    while( sdrMtxStack::isEmpty(&aMtx->mLatchStack) != ID_TRUE )
    {
        sItem = sdrMtxStack::pop( &( aMtx->mLatchStack ) );
        IDE_TEST( releaseLatchItem4Rollback( aMtx,
                                             sItem )
                  != IDE_SUCCESS );
    }

    /* BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�.
     * Mtx�� Commit�ɶ� SpaceCache�� Free�� Extent�� ��ȯ�մϴ�. */ 
    IDE_TEST( executePendingJob( aMtx,
                                 ID_FALSE)  //aDoCommitOp
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( destroy(aMtx) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-31006 - Debug information must be recorded when an exception occurs
     *             from mini transaction commit or rollback.
     * mini transaction commit/rollback ���� �� ���н� ����� ���� ����ϰ�
     * ������ ���δ�. */

    /* dump sdrMtx */
    if ( sState != 0 )
    {
        dumpHex( aMtx );
        dump( aMtx, SDR_MTX_DUMP_DETAILED );
    }

    /* dump variable */
    ideLog::log( IDE_SERVER_0, "[ Dump variable ]" );
    ideLog::log( IDE_SERVER_0,
                 "LogSize: %"ID_UINT32_FMT", "
                 "ParseOffset: %"ID_INT32_FMT", "
                 "State: %"ID_UINT32_FMT"",
                 sLogSize,
                 sParseOffset,
                 sState );

    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *
 * save point���� latch�� �����Ѵ�. page�� �����Ǿ��� ���
 * latch�� �����ؼ��� �ȵȴ�. �� savepoint�� ��ȿȭ�ȴ�.
 * �̸� log buffer�� ũ��� �����Ѵ�.
 *
 * Implementation :
 * if( save point�� �α� ũ�� != ���� �α� ũ�� )
 *     return false
 *
 * if( savepoint�� ũ�� > ���� stack�� ũ�� )
 *     return false
 *
 *
 * while( savepoint + 1������ stack index�� ���� )
 *     pop
 *     release
 *
 *
 *
 **********************************************************************/
IDE_RC sdrMiniTrans::releaseLatchToSP( sdrMtx       *aMtx,
                                       sdrSavePoint *aSP )
{

    sdrMtxLatchItem *sItem;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( aSP != NULL );

    IDE_DASSERT( ( (getLogMode(aMtx) == SDR_MTX_LOGGING) &&
                   (aSP->mLogSize == smuDynArray::getSize(&aMtx->mLogBuffer))) ||
                 ( (getLogMode(aMtx) == SDR_MTX_NOLOGGING) &&
                   (aSP->mLogSize == 0) ) );

    while( aSP->mStackIndex < sdrMtxStack::getCurrSize(&aMtx->mLatchStack) )
    {
        sItem = sdrMtxStack::pop( &( aMtx->mLatchStack ) );

        IDE_TEST( releaseLatchItem4Rollback( aMtx,
                                             sItem )
                  != IDE_SUCCESS );
    }

    while( aSP->mXLatchStackIndex < sdrMtxStack::getCurrSize(&aMtx->mXLatchPageStack) )
    {
        sItem = sdrMtxStack::pop( &( aMtx->mXLatchPageStack ) );
        if ( SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON(aMtx->mFlag) )
        {
            IDE_TEST( smuUtility::freeAlignedBuf( &(sItem->mPageCopy) )
                      != IDE_SUCCESS );
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 *
 * savepoint�� �����Ѵ�.
 *
 **********************************************************************/
void sdrMiniTrans::setSavePoint( sdrMtx       *aMtx,
                                 sdrSavePoint *aSP )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT(aSP != NULL);

    aSP->mStackIndex = sdrMtxStack::getCurrSize( &aMtx->mLatchStack );

    aSP->mXLatchStackIndex = sdrMtxStack::getCurrSize(
                                            &aMtx->mXLatchPageStack );

    if( getLogMode(aMtx) == SDR_MTX_LOGGING )
    {
        aSP->mLogSize = smuDynArray::getSize(&aMtx->mLogBuffer);
    }
    else
    {
        aSP->mLogSize = 0;
    }

}


/***********************************************************************
 * Description : mtx ������ �� item�� release �۾��� �����Ѵ�.
 *
 * release�� �� item�� ������ ���ʷ� commit �ÿ� ���־�� �� �۾��� ���Ѵ�.
 * ���� ���, latch�� �����ϰ�, fix count�� ���ҽ�Ű�� ���� �۾��� �ִ�.
 *
 * item�� ������ �ִ� ������Ʈ�� sdbBCB, iduLatch, iduMutex ��� 1����
 * �̻��� latch ������ ���� �� ������, ������ ���� release �ؾ� �Ѵ�.
 *
 * sdbBCB�� ���� sdb�� page release �Լ��� ȣ���Ѵ�.
 * iduLatch�� iduMutex�� ��쿡�� latch, mutex�� ������ �ָ� �ȴ�.
 * �̸� ���� �� Ÿ���� release function�� �ʱ�ȭ �� ��
 * ȣ���Ѵ�.
 *
 * object            latch mode     release function
 * ----------------------------------------------------------
 * Page               No latch    sdbBufferMgr::releasePage
 * Page               S latch     sdbBufferMgr::releasePage
 * Page               X latch     sdbBufferMgr::releasePage
 * iduLatch           S latch     iduLatch::unlock
 * iduLatch           X latch     iduLatch::unlock
 * iduMutex           X lock      Object.unlock
 *
 * - 1st. code design
 *   + if(object�� null) !!ASSERT
 *   + if( object�� Page�� ���� ���̸� )
 *     - BCB�� release�Ѵ�. ->sdbBufferMgr::releasePage
 *   else if ( object�� iduLatch�� ���� ���̸� )
 *     - latch�� �����Ѵ�.->iduLatch::unlock
 *   else ( object�� iduMutex�� ���� ���̸�)
 *     - mutex�� �����Ѵ�.->object.unlock()
 *
 * - 2nd. code design
 *   + if(object�� null) assert
 *   + latch item Ÿ�Կ� ���� release �Լ� �����͸� ȣ���Ѵ�.
 **********************************************************************/
IDE_RC sdrMiniTrans::releaseLatchItem(sdrMtx*           aMtx,
                                      sdrMtxLatchItem*  aItem)
{

    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( aItem != NULL );

    IDE_TEST( mItemVector[aItem->mLatchMode].mReleaseFunc(
                    aItem->mObject,
                    aItem->mLatchMode,
                    aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Descripition : mtx�� Stack�� Ư�� page ������ ��ȯ
 *
 * xLatch ���ÿ� Ư�� page ID�� �ش��ϴ� BCB�� ������ ���,
 * �ش� page frame�� ���� �����͸� ��ȯ.
 * ������ NULL
 *
 * �Ϲ� Stack���� �ٸ�. �ֳ��ϸ� Logging�� ����, FixPage�� �ٸ� MTX�� �ϰ�
 * �ٸ� MTX���� SetDrity�� �� ���̽��� �ֱ� ����. ���� XLatchStack��
 * ã�ƾ� ��.
 *
 ***********************************************************************/
UChar* sdrMiniTrans::getPagePtrFromPageID( sdrMtx       * aMtx,
                                           scSpaceID      aSpaceID,
                                           scPageID       aPageID )

{
    sdbBCB            sBCB;
    sdrMtxLatchItem * sFindItem;
    UChar           * sRetPagePtr = NULL;
    sdrMtxStackInfo * aLatchStack = NULL;

    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    aLatchStack = &aMtx->mLatchStack;

    sBCB.mSpaceID = aSpaceID;
    sBCB.mPageID = aPageID;

    sFindItem = existObject( aLatchStack,
                             &sBCB,
                             SDR_MTX_BCB);

    if( sFindItem != NULL )
    {
        sRetPagePtr = ((sdbBCB *)(sFindItem->mObject))->mFrame;
    }
    
    return sRetPagePtr;
}

/***********************************************************************
 * Descripition :
 *
 * releaseToSP Ȥ�� rollback�ÿ� item�� release�Ѵ�.
 * �ٸ� object�� �׳� releae�� ���� unlock�� �����ϳ�,
 * page�� ��� flush list�� �� �ʿ���� latch�� �����ϴ� ���� �ٸ���.
 *
 *
 ***********************************************************************/
IDE_RC sdrMiniTrans::releaseLatchItem4Rollback(sdrMtx*           aMtx,
                                               sdrMtxLatchItem*  aItem)
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );
    IDE_DASSERT( aItem != NULL );

    IDE_TEST( mItemVector[aItem->mLatchMode].mReleaseFunc4Rollback(
                    aItem->mObject,
                    aItem->mLatchMode,
                    aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ÿ� ������Ʈ�� �ִ��� Ȯ���Ѵ�.
 * ã���� stack�� latch item��, ��ã���� NULL�� �����Ѵ�.
 *
 * - 1st code design
 *   + ���ÿ� �ش� �������� �ִ��� Ȯ���Ѵ�. -> sdrMtxStack::find()
 **********************************************************************/
sdrMtxLatchItem* sdrMiniTrans::existObject( sdrMtxStackInfo       * aLatchStack,
                                            void                  * aObject,
                                            sdrMtxItemSourceType    aType )
{
    sdrMtxLatchItem    sSrcItem;
    sdrMtxLatchItem  * sFindItem;

    IDE_DASSERT( aObject != NULL )
  
    sSrcItem.mObject = aObject;

    sFindItem = sdrMtxStack::find( aLatchStack,
                                   &sSrcItem,
                                   aType,
                                   mItemVector );

    return sFindItem;
}

#if 0
/***********************************************************************
 *
 * Description :
 *
 * minitrans�� begin lsn�� return
 *
 * Implementation :
 *
 **********************************************************************/
smLSN sdrMiniTrans::getBeginLSN( void     * aMtx )
{

    IDE_DASSERT( validate((sdrMtx *)aMtx) == ID_TRUE );

    return ((sdrMtx *)aMtx)->mBeginLSN;

}
#endif


/***********************************************************************
 *
 * Description :
 *
 * minitrans�� end lsn�� return
 *
 * Implementation :
 *
 **********************************************************************/
smLSN sdrMiniTrans::getEndLSN( void     * aMtx )
{

    IDE_DASSERT( validate((sdrMtx*)aMtx) == ID_TRUE );

    return ((sdrMtx *)aMtx)->mEndLSN;

}

/***********************************************************************
 *
 * Description :
 *
 * minitrans�� Log�� write�� ���� �ִ��� �˻��Ѵ�.
 *
 * Implementation :
 *
 **********************************************************************/
idBool sdrMiniTrans::isLogWritten( void     * aMtx )
{
    sdrMtx *sMtx = (sdrMtx*)aMtx;

    IDE_DASSERT( validate(sMtx) == ID_TRUE );

    if( ( smuDynArray::getSize( &sMtx->mLogBuffer ) > 0 ) ||
        ( sMtx->mOpType != SDR_OP_INVALID ) )
    {
        IDE_ASSERT( sMtx->mAccSpaceID != 0 );
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/***********************************************************************
 *
 * Description :
 *
 * minitrans �α� ��尡 logging�̳�
 *
 * Implementation :
 *
 **********************************************************************/
idBool sdrMiniTrans::isModeLogging( void     * aMtx )
{

    IDE_DASSERT( validate((sdrMtx*)aMtx) == ID_TRUE );

    return ( getLogMode((sdrMtx *)aMtx) == SDR_MTX_LOGGING ?
             ID_TRUE : ID_FALSE );

}

/***********************************************************************
 *
 * Description :
 *
 * minitrans �α� ��尡 no logging�̳�
 *
 * Implementation :
 *
 **********************************************************************/
idBool sdrMiniTrans::isModeNoLogging( void     * aMtx )
{

    IDE_DASSERT( validate((sdrMtx*)aMtx) == ID_TRUE );

    return ( getLogMode((sdrMtx *)aMtx) == SDR_MTX_NOLOGGING ?
             ID_TRUE : ID_FALSE );

}

/***********************************************************************
 *
 * Description :
 *
 * minitrans�� ���� ������ dump�Ѵ�.
 *
 * Implementation :
 *
 * mtx struct�� ��� hex dump�Ѵ�.
 **********************************************************************/
void sdrMiniTrans::dumpHex( sdrMtx  * aMtx )
{
    UChar   * sDumpBuf;
    UInt      sDumpBufSize;

    /* sdrMtx Hexa dump */
    ideLog::log( IDE_SERVER_0, "[ Hex Dump MiniTrans ]" );
    ideLog::logMem( IDE_SERVER_0, (UChar*)aMtx, ID_SIZEOF(sdrMtx) );

    /* dump�� �޸� ũ�� ��� */
    sDumpBufSize = smuDynArray::getSize( &aMtx->mLogBuffer );

    if( sDumpBufSize > 0 )
    {
        if ( iduMemMgr::malloc( IDU_MEM_SM_SDR,
                                sDumpBufSize,
                                (void**)&sDumpBuf,
                                IDU_MEM_FORCE) == IDE_SUCCESS )
        {
            /* sdrMtx->mLogBuffer Hexa dump */
            ideLog::log( IDE_SERVER_0, "[ Hex Dump LogBuffer ]" );

            smuDynArray::load( &aMtx->mLogBuffer, 
                               (void*)sDumpBuf, 
                               sDumpBufSize );

            ideLog::logMem( IDE_SERVER_0,
                            sDumpBuf,
                            smuDynArray::getSize(&aMtx->mLogBuffer) );

            IDE_ASSERT( iduMemMgr::free( sDumpBuf ) == IDE_SUCCESS );
        }
    }
}

/***********************************************************************
 *
 * Description :
 *
 * minitrans�� ���� ������ dump�Ѵ�.
 *
 * Implementation :
 *
 * mtx struct�� ��� ǥ���Ѵ�.
 **********************************************************************/
void sdrMiniTrans::dump( sdrMtx          *aMtx,
                         sdrMtxDumpMode   aDumpMode )
{
    IDE_DASSERT( validate(aMtx) == ID_TRUE );

    if( aMtx->mTrans != NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "----------- MiniTrans Begin ----------\n" 
                     "Transaction ID\t: %"ID_UINT32_FMT"\n"
                     "TableSpace ID\t: %"ID_UINT32_FMT"\n"
                     "Flag\t: %"ID_UINT32_FMT"\n"
                     "Used Log Buffer Size\t: %"ID_UINT32_FMT"\n"
                     "Begin LSN\t: %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "End LSN\t: %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "Dis Log Attr\t: %"ID_xINT32_FMT"\n",
                     smLayerCallback::getTransID( aMtx->mTrans ),
                     aMtx->mAccSpaceID,
                     aMtx->mFlag,
                     smuDynArray::getSize(&aMtx->mLogBuffer),
                     aMtx->mBeginLSN.mFileNo,
                     aMtx->mBeginLSN.mOffset,
                     aMtx->mEndLSN.mFileNo,
                     aMtx->mEndLSN.mOffset,
                     aMtx->mDLogAttr );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "----------- MiniTrans Begin ----------\n" 
                     "Transaction ID\t: <NONE>\n"
                     "TableSpace ID\t: %"ID_UINT32_FMT"\n"
                     "Flag\t: %"ID_UINT32_FMT"\n"
                     "Used Log Buffer Size\t: %"ID_UINT32_FMT"\n"
                     "Begin LSN\t: %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "End LSN\t: %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "Dis Log Attr\t: %"ID_xINT32_FMT"\n",
                     aMtx->mAccSpaceID,
                     aMtx->mFlag,
                     smuDynArray::getSize(&aMtx->mLogBuffer),
                     aMtx->mBeginLSN.mFileNo,
                     aMtx->mBeginLSN.mOffset,
                     aMtx->mEndLSN.mFileNo,
                     aMtx->mEndLSN.mOffset,
                     aMtx->mDLogAttr );
    }

    if (aMtx->mOpType != 0)
    {
        ideLog::log( IDE_SERVER_0,
                     "Operational Log Type \t: %"ID_UINT32_FMT"\n"
                     "Prev Prev LSN For NTA\t: %"ID_UINT32_FMT",%"ID_UINT32_FMT"\n"
                     "RID For NTA\t: %"ID_UINT32_FMT",%"ID_UINT32_FMT"\n",
                     aMtx->mOpType,
                     aMtx->mPPrevLSN.mFileNo,
                     aMtx->mPPrevLSN.mOffset ,
                     SD_MAKE_PID(aMtx->mData[0]),
                     SD_MAKE_OFFSET(aMtx->mData[0]) );
    }

    sdrMtxStack::dump( &aMtx->mLatchStack, mItemVector, aDumpMode );

    ideLog::log( IDE_SERVER_0, 
                         "----------- MiniTrans End ----------" );
}

/***********************************************************************
 *
 * Description :
 *
 * validate
 *
 * Implementation :
 *
 **********************************************************************/
#if defined(DEBUG)
idBool sdrMiniTrans::validate( sdrMtx    * aMtx )
{
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( sdrMtxStack::validate( &aMtx->mLatchStack ) == ID_TRUE );

    return ID_TRUE;
}
#endif
/***********************************************************************

 Description :

 Implementation



 **********************************************************************/
idBool sdrMiniTrans::isEmpty( sdrMtx * aMtx )
{
    IDE_DASSERT( aMtx != NULL );

    return sdrMtxStack::isEmpty(&(aMtx->mLatchStack));
}

/***********************************************************************
 * Description : NOLOGGING Persistent �Ǵ�
 * NOLOGGING mini-transaction�� persistent ���� ���θ� return
 **********************************************************************/
idBool sdrMiniTrans::isNologgingPersistent( void * aMtx )
{
    sdrMtx     *sMtx;

    IDE_DASSERT( aMtx != NULL );

    sMtx = (sdrMtx*)aMtx;

    return ((sMtx->mFlag & SDR_MTX_NOLOGGING_ATTR_PERSISTENT_MASK) ==
            SDR_MTX_NOLOGGING_ATTR_PERSISTENT_ON)? ID_TRUE : ID_FALSE;
}

/***********************************************************************
 * PROJ-2162 RestartRiskReduction
 *
 * Description : Mtx Commit���� DRDB Redo����� ������
 *  
 * aMtx          - [IN] ��������� Mtx
 * aMtxStack     - [IN] MiniTransaction�� Dirtypage�� ��Ƶ� Stack
 * aLogBuffer    - [IN] �̹��� ����� Log��
 **********************************************************************/
IDE_RC sdrMiniTrans::validateDRDBRedo( sdrMtx          * aMtx,
                                       sdrMtxStackInfo * aMtxStack,
                                       smuDynArrayBase * aLogBuffer )
{
    idvSQL             sDummyStatistics;
    SChar            * sRedoLogBuffer = NULL;
    UInt               sRedoLogBufferSize;
    sdrRedoLogData   * sHead;
    sdrRedoLogData   * sData;
    ULong              sSPIDArray[ SDR_MAX_MTX_STACK_SIZE ];
    UInt               sSPIDCount = 0;
    sdbBCB           * sBCB;
    UInt               sState = 0;
    UInt               i;

    idlOS::memset( &sDummyStatistics, 0, ID_SIZEOF( idvSQL ) );

    /*********************************************************************
     * 1) MTX���� ����� Log���� �м��Ͽ�, ���ŵ� ���������� ã��
     *********************************************************************/
    
    /* ��ϵ� Log�� ������ */
    sRedoLogBufferSize = smuDynArray::getSize( aLogBuffer);

    /* sdrMiniTrans_validateDRDBRedo_malloc_RedoLogBuffer.tc */
    IDU_FIT_POINT("sdrMiniTrans::validateDRDBRedo::malloc::RedoLogBuffer");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDR,
                                 sRedoLogBufferSize,
                                 (void**)&sRedoLogBuffer,
                                 IDU_MEM_FORCE) 
              != IDE_SUCCESS );
    smuDynArray::load( aLogBuffer, 
                       sRedoLogBuffer,
                       sRedoLogBufferSize );
    sState = 1;

    IDE_TEST( sdrRedoMgr::generateRedoLogDataList( 0, // dummy TID
                                                   sRedoLogBuffer,
                                                   sRedoLogBufferSize,
                                                   &aMtx->mBeginLSN,
                                                   &aMtx->mEndLSN,
                                                   (void**)&sHead )
              != IDE_SUCCESS );

    /* ���� ���� */
    sData = sHead;
    do
    {
        IDE_TEST( validateDRDBRedoInternal( &sDummyStatistics,
                                            aMtx,
                                            aMtxStack,
                                            (ULong*)sSPIDArray,
                                            &sSPIDCount,
                                            sData->mSpaceID,
                                            sData->mPageID )
                  != IDE_SUCCESS );

        sData = (sdrRedoLogData*)SMU_LIST_GET_NEXT( &(sData->mNode4RedoLog) )->mData;
    }    
    while( sData != sHead );

    IDE_TEST( sdrRedoMgr::freeRedoLogDataList( (void*)sData ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sRedoLogBuffer ) != IDE_SUCCESS );

    /*********************************************************************
     * 2) Log�� �ȳ��Ҵµ�, SetDirty�� ���������� ����
     *********************************************************************/
    for( i = 0 ; i < aMtxStack->mCurrSize ; i ++ )
    {
        sBCB = (sdbBCB*)(aMtxStack->mArray[ i ].mObject);

        IDE_TEST( validateDRDBRedoInternal( &sDummyStatistics,
                                            aMtx,
                                            aMtxStack,
                                            (ULong*)sSPIDArray,
                                            &sSPIDCount,
                                            sBCB->mSpaceID,
                                            sBCB->mPageID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        IDE_ASSERT( iduMemMgr::free( sRedoLogBuffer ) == IDE_SUCCESS );
    default:
        break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-2162 RestartRiskReduction
 *
 * Description : ������ ������ ������.
 * 
 * <<State���� ó��. ����. ���⼭ ���ܰ� �߻��ϸ� �����ų���̱� ����>>
 *
 * aStatistics    - [IN] Dummy �������
 * aMtx           - [IN] ��������� Mtx
 * aMtxStack      - [IN] MiniTransaction�� Dirtypage�� ��Ƶ� Stack
 * aSPIDArray     - [IN] �ߺ� �˻縦 ���� ���� Array
 * aSPIDCount     - [IN] �ߺ� �˻縦 ���� ���� Array�� ��ü ����
 * aSpaceID       - [IN] ������ ��� Page�� SpaceID
 * aPageID        - [IN] ������ ��� Page�� PageID
 **********************************************************************/

IDE_RC sdrMiniTrans::validateDRDBRedoInternal( 
                                            idvSQL          * aStatistics,
                                            sdrMtx          * aMtx,
                                            sdrMtxStackInfo * aMtxStack,
                                            ULong           * aSPIDArray,
                                            UInt            * aSPIDCount,
                                            scSpaceID         aSpaceID,
                                            scPageID          aPageID )
{
    UChar            * sBufferPage;
    UChar            * sRecovePage;
    sdbBCB             sBCB;
    sdrMtxLatchItem  * sFindItem;
    smuAlignBuf        sPagePtr;
    idBool             sReadFromDisk;
    idBool             sSuccess;
    sdpPageType        sPageType;
    ULong              sSPID;
    UInt               i;

    /****************************************************************
     * 1) �ߺ� üũ 
     ****************************************************************/
    sSPID = ( (ULong)aSpaceID << 32 ) + aPageID;
    for( i = 0 ; i < (*aSPIDCount) ; i ++  )
    {
        if( aSPIDArray[ i ] == sSPID )
        {
            break;
        }
    }
    IDE_TEST_CONT( i != (*aSPIDCount), SKIP );
    aSPIDArray[ (*aSPIDCount) ++ ] = sSPID;

    /****************************************************************
     * 2) ������ �� Page ��������
     ****************************************************************/
    sBCB.mSpaceID = aSpaceID;
    sBCB.mPageID  = aPageID;
    sFindItem     = existObject( aMtxStack,
                                 &sBCB,
                                 SDR_MTX_BCB );

    /* Log�� ����ߴµ� setDirty�� ���� ���� ������.
       ���⼭ ���ܴ� ������ ASSERT �� ������ ����ó������. */
    IDE_ERROR_MSG( sFindItem != NULL,
                   "[ERROR] - The FindItem is not found." );

    /* Frame �����صα�. */
    IDE_ASSERT( smuUtility::allocAlignedBuf( IDU_MEM_SM_SDR,
                                             SD_PAGE_SIZE,
                                             SD_PAGE_SIZE,
                                             &sPagePtr )
                == IDE_SUCCESS );
    sBufferPage = sPagePtr.mAlignPtr;
    idlOS::memcpy( sBufferPage, 
                   ((sdbBCB *)(sFindItem->mObject))->mFrame,
                   SD_PAGE_SIZE );

    sRecovePage = sFindItem->mPageCopy.mAlignPtr;
    IDE_ASSERT( sRecovePage != NULL );

    if ( existObject( &aMtx->mLatchStack,
                      &sBCB,
                      SDR_MTX_BCB )
         == NULL )
    {
        /* Latch�� ���� ���� ���� �ΰ��� Mtx�� ���� CreatePage������,
         * Latch�� �ٸ� Mtx���� ���, ���⼭�� SetDrity�ϰ� Logging��
         * �� ����̴�. 
         * �� ��쿡�� �������� ������ �� setDirty�� �ϴ°�찡 ���Ƽ�,
         * Copy������ Redo�ߴٰ��� ���� ��쿡 ������ ����� ������, 
         * DiskFile�κ��� Page�� �д´�.*/
        sReadFromDisk = ID_TRUE;
    }
    else
    {
        sReadFromDisk = ID_FALSE;
    }

    /****************************************************************
     * 3) �����ұ� ����?
     ****************************************************************/
    sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sBufferPage );

    /* BUG-32546 [sm-disk-index] The logging image of DRDB index meta page
     * can be different to page image.
     * �� ���� �ذ� �� Skip�� Ǯ��. */
    IDE_TEST_CONT( sPageType == SDP_PAGE_INDEX_META_BTREE, SKIP );
    IDE_TEST_CONT( sPageType == SDP_PAGE_INDEX_META_RTREE, SKIP );

    /* Restart Recovery �������� AgerbleSCN�� �����Ǿ� ���� �ʱ� ������
     * getCommitSCN�� ���� DelayedStamping�� ���Ѵ�. ���� Stamping
     * �� �ʿ��� ���� ������ ���� �ʴ´�. */
    IDE_TEST_CONT( ( smrRecoveryMgr::isRestart() == ID_TRUE ) &&
                    ( ( sPageType == SDP_PAGE_INDEX_BTREE ) ||
                      ( sPageType == SDP_PAGE_INDEX_RTREE ) ||
                      ( sPageType == SDP_PAGE_DATA ) ),
                    SKIP );

    if( SM_IS_LSN_INIT( sdpPhyPage::getPageLSN( sBufferPage ) ) )
    {
        /* CreatePage���� �� ��� �ۿ� ����. ���� �� ����
         * RecoverdPage�� BCB�� ���� Frame�� ������ �ֱ� ������
         * Page ������ ��Ȯ���� �����Ƿ� �����ϸ� �ȵȴ�.
         * sdbBufferPool::createPage ���� */
        sSuccess = ID_FALSE;
    }
    else
    {
        IDE_TEST( sdrRedoMgr::generatePageUsingOnlineRedo( 
                                                aStatistics,
                                                aSpaceID,
                                                aPageID,
                                                sReadFromDisk,
                                                sRecovePage,
                                                &sSuccess )
                 != IDE_SUCCESS );
    }

    if( sSuccess == ID_TRUE )
    {
        IDE_TEST( sPageType 
                  != sdpPhyPage::getPageType( (sdpPhyPageHdr*)sRecovePage ) );

        /* Logging���� �����ϴ� FastStamping�� ���� �Ȱ��� �����ش�. */
        if( sPageType == SDP_PAGE_DATA )
        {
            IDE_TEST( sdcTableCTL::stampingAll4RedoValidation(aStatistics,
                                                              sRecovePage,
                                                              sBufferPage )
                      != IDE_SUCCESS );
        }
        if( ( sPageType == SDP_PAGE_INDEX_BTREE ) ||
            ( sPageType == SDP_PAGE_INDEX_RTREE ) )
        {
            IDE_TEST( sdnIndexCTL::stampingAll4RedoValidation(aStatistics,
                                                              sRecovePage,
                                                              sBufferPage )
                      != IDE_SUCCESS );
        }
        IDE_TEST( idlOS::memcmp( sRecovePage  + ID_SIZEOF( sdbFrameHdr ),
                                 sBufferPage  + ID_SIZEOF( sdbFrameHdr ),
                                 SD_PAGE_SIZE - ID_SIZEOF( sdbFrameHdr )
                                              - ID_SIZEOF( sdpPageFooter )
                               ) != 0 );
    }

    IDE_TEST( smuUtility::freeAlignedBuf( &sPagePtr )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "SpaceID: %"ID_UINT32_FMT"\n"
                 "PageID : %"ID_UINT32_FMT"\n",
                 aSpaceID,
                 aPageID );

    dumpHex( aMtx );
    dump( aMtx, SDR_MTX_DUMP_DETAILED );
    /* SetDirty�� �ȵ� ���. Page ���� ��� ���� ���� �����Ŵ */
    IDE_ASSERT( sFindItem != NULL );

    sdpPhyPage::tracePage( IDE_SERVER_0,
                           sRecovePage,
                           "RedoPage:");
    sdpPhyPage::tracePage( IDE_SERVER_0,
                           sBufferPage,
                           "BuffPage:");
    /* PageType�� �޶� �ȵ� ���. */
    IDE_ASSERT( sdpPhyPage::getPageType( (sdpPhyPageHdr*)sRecovePage ) ==
                sdpPhyPage::getPageType( (sdpPhyPageHdr*)sBufferPage ) );
    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :  BUG-32579 The MiniTransaction commit should not be used in
 * exception handling area.
 ***************************************************************************/
idBool sdrMiniTrans::isRemainLogRec( void * aMtx )
{
    sdrMtx     *sMtx;
    idBool      sRet = ID_FALSE;

    IDE_DASSERT( aMtx != NULL );

    sMtx = (sdrMtx*)aMtx;

    if( sMtx->mRemainLogRecSize == 0 )
    {
        sRet = ID_FALSE;
    }
    else
    {
        sRet = ID_TRUE;
    }

    return sRet;
}
