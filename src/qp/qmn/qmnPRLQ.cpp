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
 * $Id: qmnPRLQ.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmnPRLQ.h>
#include <qmnScan.h>
#include <qmnGroupAgg.h>
#include <qcg.h>
#include <qmcThr.h>

/***********************************************************************
 *
 * Description :
 *    Parallel Queue ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::init( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
    qmncPRLQ    * sCodePlan = (qmncPRLQ *) aPlan;
    qmndPRLQ    * sDataPlan =
        (qmndPRLQ *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnPRLQ::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_PRLQ_INIT_DONE_MASK)
         == QMND_PRLQ_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sDataPlan) != IDE_SUCCESS );
    }

    // init child node
    if( aPlan->left != NULL )
    {
        sDataPlan->mCurrentNode = aPlan->left;
        IDE_TEST( sDataPlan->mCurrentNode->init( aTemplate,
                                                 sDataPlan->mCurrentNode )
                  != IDE_SUCCESS);
    }
    else
    {
        sDataPlan->mCurrentNode = NULL;
    }

    /* sDataPlan->mThrArg �� ���� firstInit ���� �ʱ�ȭ �Ǿ��� */
    sDataPlan->doIt         = qmnPRLQ::doItFirst;
    sDataPlan->mAllRowRead  = 0;
    sDataPlan->mThrObj     = NULL;
    sDataPlan->plan.mTID    = QMN_PLAN_INIT_THREAD_ID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    PRLQ node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::firstInit( qcTemplate * aTemplate,
                           qmndPRLQ   * aDataPlan )
{
    iduMemory * sMemory = NULL;
    iduMemory * sPRLQMemory = NULL;
    iduMemory * sPRLQCacheMemory = NULL;
    qcTemplate* sTemplate = NULL;
    SInt        sStackSize;

    sMemory = aTemplate->stmt->qmxMem;

    // ���ռ� �˻�
    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aDataPlan != NULL );

    //---------------------------------
    // PRLQ Queue �ʱ�ȭ
    //---------------------------------
    aDataPlan->mQueue.mHead = 0;
    aDataPlan->mQueue.mTail = 0;
    aDataPlan->mQueue.mSize = QCU_PARALLEL_QUERY_QUEUE_SIZE;

    IDU_FIT_POINT( "qmnPRLQ::firstInit::alloc::mRecord", 
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(sMemory->alloc(ID_SIZEOF(qmnPRLQRecord) *
                            aDataPlan->mQueue.mSize,
                            (void**)&(aDataPlan->mQueue.mRecord))
             != IDE_SUCCESS);

    //---------------------------------
    // Thread arguemnt �Ҵ� �� �ʱ�ȭ
    //---------------------------------
    IDU_FIT_POINT( "qmnPRLQ::firstInit::alloc::mThrArg", 
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( sMemory->alloc( ID_SIZEOF(qmnPRLQThrArg),
                              (void**)&(aDataPlan->mThrArg) )
              != IDE_SUCCESS);

    aDataPlan->mThrArg->mChildPlan  = NULL;
    aDataPlan->mThrArg->mPRLQQueue  = &aDataPlan->mQueue;
    aDataPlan->mThrArg->mAllRowRead = &aDataPlan->mAllRowRead;

    /*
     * --------------------------------------------------------------
     * BUG-38294 print wrong subquery plan when execute parallel query
     *
     * initialize PRLQ template
     * - alloc statement
     * - alloc memory manager
     * - alloc stack
     * --------------------------------------------------------------
     */
    sTemplate = &aDataPlan->mTemplate;

    IDU_FIT_POINT( "qmnPRLQ::firstInit::alloc::Template",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(sMemory->alloc(ID_SIZEOF(qcStatement),
                            (void**)&sTemplate->stmt)
             != IDE_SUCCESS);

    /* worker thread ���� ����� QMX, QXC memory ���� */
    IDE_TEST( qcg::allocPRLQExecutionMemory( aTemplate->stmt,
                                             &sPRLQMemory,
                                             &sPRLQCacheMemory )     // PROJ-2452
              != IDE_SUCCESS);

    /*
     * ���� ������ �޸𸮸� ����� worker thread �� template �� �����Ѵ�.
     * ���ο��� statement ����� �� QMX, QXC �޸� ���� � ��� ó���Ѵ�.
     */
    IDE_TEST( qcg::allocAndCloneTemplate4Parallel( sPRLQMemory,
                                                   sPRLQCacheMemory, // PROJ-2452
                                                   aTemplate,
                                                   sTemplate )
              != IDE_SUCCESS );

    // PROJ-2527 WITHIN GROUP AGGR
    // mtcTemplate�� finalize�� ȣ���ϰ� �Ǿ���. �̸� ���ؼ�
    // clone�� template�� ����Ѵ�.
    IDE_TEST( qcg::addPRLQChildTemplate( aTemplate->stmt,
                                         sTemplate )
              != IDE_SUCCESS );
    
    sStackSize = aTemplate->tmplate.stackCount;

    /* worker thread �� stack ���� */
    IDU_FIT_POINT( "qmnPRLQ::firstInit::alloc::Stack",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(sPRLQMemory->alloc(ID_SIZEOF(mtcStack) * sStackSize,
                                (void**)&(sTemplate->tmplate.stackBuffer))
             != IDE_SUCCESS);

    sTemplate->tmplate.stack       = sTemplate->tmplate.stackBuffer;
    sTemplate->tmplate.stackCount  = sStackSize;
    sTemplate->tmplate.stackRemain = sStackSize;

    aDataPlan->mTemplateCloned = ID_FALSE;

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------
    *aDataPlan->flag &= ~QMND_PRLQ_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_PRLQ_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    PRLQ�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::doIt( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * aFlag )
{
    qmncPRLQ * sCodePlan = (qmncPRLQ*)aPlan;
    qmndPRLQ * sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);
    UInt       sThrStatus;

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    if ( ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE ) &&
         ( ( *aFlag & QMC_ROW_QUEUE_EMPTY_MASK ) == QMC_ROW_QUEUE_EMPTY_TRUE ) &&
         ( sDataPlan->mThrObj != NULL ) )
    {
        if (sCodePlan->mParallelType == QMNC_PRLQ_PARALLEL_TYPE_SCAN)
        {
            /*
             * non-partition �� ���
             * ������ doIt ���� thread ��ȯ
             */
            sThrStatus = acpAtomicGet32(&sDataPlan->mThrObj->mStatus);
            while (sThrStatus == QMC_THR_STATUS_RUN)
            {
                idlOS::thr_yield();
                sThrStatus = acpAtomicGet32(&sDataPlan->mThrObj->mStatus);
            }
            qmcThrReturn(aTemplate->stmt->mThrMgr, sDataPlan->mThrObj);
        }
        else
        {
            /*
             * partition �� ��� (PPCRD �Ʒ� PRLQ �� �ִ� ���)
             *
             * PRLQ �� child �� 1 �� �̻��� ��� thread �� �����ϱ� ����
             * ���⼭ thread ��ȯ���� �ʴ´�.
             * ��� PRLQ �� ����ϴ� ���(multi children �� ���� ���)��
             * PRLQ �� thread �� ��ȯ�ؾ� �Ѵ�. (qmnPRLQ::returnThread)
             */
        }

        // Thread �� ����� �� �����Ƿ� �ʱ�ȭ ���ش�.
        acpAtomicSet32(&sDataPlan->mAllRowRead, 0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::padNull( qcTemplate* aTemplate, qmnPlan* aPlan )
{
    qmncPRLQ * sCodePlan = (qmncPRLQ *) aPlan;
    qmndPRLQ * sDataPlan =
        (qmndPRLQ *) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_PRLQ_INIT_DONE_MASK)
         == QMND_PRLQ_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_DASSERT( sDataPlan->mCurrentNode != NULL );

    // Child Plan�� ���Ͽ� Null Padding����
    return sDataPlan->mCurrentNode->padNull(aTemplate, sDataPlan->mCurrentNode);

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ------------------------------------------------------------------
 * PROJ-2402 Parallel Table Scan
 *
 * doItFirst() ���� �����ϴ� �۾� �պκ��� ����
 * thread �� ���۽�Ű�� ���� SCAN �� startIt �� �Ϸ�ɶ����� ���
 * ------------------------------------------------------------------
 */
IDE_RC qmnPRLQ::startIt(qcTemplate* aTemplate, qmnPlan* aPlan, UInt* aTID)
{
    qmndPRLQ * sDataPlan;
    qmnPlan  * sChildCodePlan;
    iduMemory* sMemory;
    idBool     sIsSuccess;
    UInt       i;

    sDataPlan      = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);
    sChildCodePlan = (qmnPlan *)sDataPlan->mCurrentNode;
    sMemory        = aTemplate->stmt->qmxMem;

    //---------------------------------
    // Template ���� (BUG-38294)
    //---------------------------------
    // ���� �� worker thread ���� ����� template �� clone �Ѵ�.

    if (sDataPlan->mTemplateCloned == ID_FALSE)
    {
        IDE_TEST(qcg::cloneTemplate4Parallel(sMemory,
                                             aTemplate,
                                             &sDataPlan->mTemplate)
                 != IDE_SUCCESS);
        sDataPlan->mTemplateCloned = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }

    /*
     * ----------------------------------------------------------------
     * Thead manager �� ���� thread ����
     * ----------------------------------------------------------------
     */

    sDataPlan->mThrArg->mChildPlan = sChildCodePlan;
    sDataPlan->mThrArg->mTemplate  = &sDataPlan->mTemplate;
    sDataPlan->mThrObj            = NULL;

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT ȯ���� ����ؼ� Child ���� �ÿ� Memory, Disk Type�� �Ǵ��ϰ� ������ �����ϵ��� �Ѵ�.
     */
    if ( sDataPlan->mExistDisk == ID_TRUE )
    {
        IDU_FIT_POINT( "qmnPRLQ::startIt::alloc::mRowBuffer",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->alloc(ID_SIZEOF(void*) * sDataPlan->mQueue.mSize,
                                (void**)&sDataPlan->mThrArg->mRowBuffer)
                 != IDE_SUCCESS);

        for (i = 0; i < sDataPlan->mQueue.mSize; i++)
        {
            // qmc::setRowSize ����ó�� cralloc �ؾ� �Ѵ�.
            // �׷��� ������ �Ϻ� type �� readRow �� �߸��� ���� ���� �� �ִ�.
            IDU_FIT_POINT( "qmnPRLQ::startIt::cralloc::mRowBufferItem",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( sMemory->cralloc( sDataPlan->mDiskRowOffset,
                                        (void **) & (sDataPlan->mThrArg->mRowBuffer[i]) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDU_FIT_POINT("qmnPRLQ::startIt::qmcThrGet::Disk::ERR_THREAD");
    IDE_TEST(qmcThrGet(aTemplate->stmt->mThrMgr,
                       runChildEnqueue,
                       (void*)sDataPlan->mThrArg,
                       &sDataPlan->mThrObj)
             != IDE_SUCCESS);

    if (sDataPlan->mThrObj != NULL)
    {
        // Thread ȹ�濡 ����
        IDE_TEST(qmcThrWakeup(sDataPlan->mThrObj, &sIsSuccess) != IDE_SUCCESS);
        IDE_TEST_RAISE(sIsSuccess == ID_FALSE, ERR_IN_THREAD);

        /* PPCRD or PSCRD */
        sDataPlan->doIt = qmnPRLQ::doItNextDequeue;

        // Plan ����� ���� thread id �� �����Ѵ�.
        sDataPlan->plan.mTID = sDataPlan->mThrObj->mID;

        /* child node (SCAN) �� startIt �� ���������� ��� */
        while (acpAtomicGet32(&sDataPlan->mAllRowRead) == 0)
        {
            idlOS::thr_yield();
        }
    }
    else
    {
        // Thread ȹ�濡 ����
        sDataPlan->doIt = qmnPRLQ::doItNextSerial;

        // Plan ����� ���� thread id �� �����Ѵ�.
        // Thread �� �Ҵ���� ���ϸ� TID �� QMN_PLAN_INIT_THREAD_ID
        sDataPlan->plan.mTID = QMN_PLAN_INIT_THREAD_ID;
    }

    if (aTID != NULL)
    {
        *aTID = sDataPlan->plan.mTID;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_IN_THREAD)
    {
        while (sDataPlan->mThrObj->mErrorCode == QMC_THR_ERROR_CODE_NONE)
        {
            idlOS::thr_yield();
        }
        IDE_SET(ideSetErrorCodeAndMsg(sDataPlan->mThrObj->mErrorCode,
                                      sDataPlan->mThrObj->mErrorMsg));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    PRLQ ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::printPlan( qcTemplate   * aTemplate,
                           qmnPlan      * aPlan,
                           ULong          aDepth,
                           iduVarString * aString,
                           qmnDisplay     aMode )
{
    qmncPRLQ  * sCodePlan;
    qmndPRLQ  * sDataPlan;
    qcTemplate* sTemplate;
    UInt        sTID;

    sCodePlan = (qmncPRLQ*)aPlan;
    sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = &aTemplate->planFlag[sCodePlan->planID];

    //----------------------------
    // Display ��ġ ����
    //----------------------------
    qmn::printSpaceDepth(aString, aDepth);
    iduVarStringAppendLength( aString, "PARALLEL-QUEUE", 14 );

    if ((*sDataPlan->flag & QMND_PRLQ_INIT_DONE_MASK) ==
        QMND_PRLQ_INIT_DONE_TRUE)
    {
        sTID = sDataPlan->plan.mTID;
    }
    else
    {
        sTID = 0;
    }

    if (sTID == 0)
    {
        /*
         * data plan �ʱ�ȭ�� �ȵǾ��ְų� serial �� ����� ���
         * ���� ���� template �� original template
         */
        sTemplate = aTemplate;
    }
    else
    {
        sTemplate = &sDataPlan->mTemplate;
    }

    //----------------------------
    // Thread ID
    //----------------------------
    if( aMode == QMN_DISPLAY_ALL )
    {
        if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
        {
            iduVarStringAppendFormat( aString,
                                      " ( TID: %"ID_UINT32_FMT" )",
                                      sTID );
        }
        else
        {
            iduVarStringAppend( aString, " ( TID: BLOCKED )" );
        }
    }

    iduVarStringAppendLength( aString, "\n", 1 );

    //----------------------------
    // Operator�� ��� ���� ���
    //----------------------------
    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    sCodePlan->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( aPlan->left != NULL )
    {
        IDE_TEST( aPlan->left->printPlan( sTemplate,
                                          aPlan->left,
                                          aDepth + 1,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // Multi children �� ������ PRLQ (PPCRD ������ �� ���)
        // ������� �ʴ´�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPRLQ::doItDefault( qcTemplate * /* aTemplate */,
                             qmnPlan    * /* aPlan */,
                             qmcRowFlag * /* aFlag */ )
{
    /* �� �Լ��� ����Ǹ� �ȵ�. */
    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    PRLQ �� ���� ���� �Լ�
 *    Child�� �����ϰ� �뵵�� �´� ���� �Լ��� �����Ѵ�.
 *    ���� ���õ� Row�� �ݵ�� ���� ���� ���޵Ǳ� ������
 *    �뵵�� �´� ó���� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::doItFirst( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
    qmndPRLQ* sDataPlan;

    sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST(startIt(aTemplate, aPlan, NULL) != IDE_SUCCESS);

    // Queue ���� ù��° row �� �����´�.
    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *    PRLQ �� doIt ���� �Լ�
 *    Thread �� ���� ������ �� ���ȴ�.
 *
 * Implementation :
 *    ���� ����� doIt �Լ��� ȣ���Ѵ�.
 *
 ***********************************************************************/
IDE_RC qmnPRLQ::doItNextSerial( qcTemplate* aTemplate,
                                qmnPlan   * aPlan,
                                qmcRowFlag* aFlag )
{
    qmndPRLQ * sDataPlan        = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);
    qmndPlan * sChildDataPlan   = (qmndPlan*)(aTemplate->tmplate.data + sDataPlan->mCurrentNode->offset);

    IDE_DASSERT( sDataPlan->mCurrentNode != NULL );

    // Child�� ����
    IDE_TEST( sDataPlan->mCurrentNode->doIt( aTemplate,
                                             sDataPlan->mCurrentNode,
                                             aFlag ) != IDE_SUCCESS );

    if( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnPRLQ::doItFirst;
    }
    else
    {
        // PROJ-2444
        // PRLQ �� ������ ���� �÷����� PRLQ �� �����͸� ���� �д´�.
        sDataPlan->mRid = sChildDataPlan->myTuple->rid;
        sDataPlan->mRow = sChildDataPlan->myTuple->row;
    }

    // PPCRD ���� serial ������ ��� �ٷ� �ٸ� partition �� �����ϵ���
    // flag �� �����Ѵ�.
    *aFlag &= ~QMC_ROW_QUEUE_EMPTY_MASK;
    *aFlag |= QMC_ROW_QUEUE_EMPTY_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ------------------------------------------------------------------
 * PPCRD or PSCRD �Ʒ� PRLQ �� �������� doIt
 *
 * ���� ��尡 ���� �����ϰ� ���ڵ带 �������� �ϱ� ����
 * ���߿� queue �� ������� ��� ������� �ʰ�
 * DATA_NONE + QUEUE_EMPTY �� ��ȯ�Ѵ�.
 * ------------------------------------------------------------------
 */
IDE_RC qmnPRLQ::doItNextDequeue( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag )
{
    qmndPRLQ * sDataPlan;
    UInt       sSleep;
    UInt       sAllRowRead;

    sDataPlan = (qmndPRLQ *) (aTemplate->tmplate.data + aPlan->offset);

    sSleep = 1;

    while (1)
    {
        // ���� Record�� �д´�.
        if (dequeue( &sDataPlan->mQueue,
                     &sDataPlan->mRid,
                     &sDataPlan->mRow ) == ID_TRUE)
        {
            // queue ���� row �� �Ѱ� �����Դ�.
            *aFlag = QMC_ROW_DATA_EXIST;
            break;
        }
        else
        {
            // nothing to do
        }

        sAllRowRead = sDataPlan->mAllRowRead;

        if (sAllRowRead == 0)
        {
            // mAllRowRead �� 0: ���� ��� initialize �� �̰ų�
            // doItFirst ���� �� queue �� ��������� ���� row ��
            // �а� �ִ� ���̹Ƿ� ��� ��õ��Ѵ�.
            // loop �� ��� ����.
            acpSleepUsec(sSleep);
            sSleep = (sSleep < QCU_PARALLEL_QUERY_QUEUE_SLEEP_MAX) ?
                (sSleep << 1) : 1;
        }
        else if (sAllRowRead == 1)
        {
            // mAllRowRead �� 1: row �� �а� �ִ� ��
            // queue �� ��������� ���� row �� �а� �ִ� ���̴�.
            // ���� PPCRD ���� ���� partition �� �е��� ����� �����Ѵ�.
            *aFlag = QMC_ROW_DATA_NONE | QMC_ROW_QUEUE_EMPTY_FALSE;
            break;
        }
        else if (sAllRowRead == 2)
        {
            // BUG-40598 �����Ϸ��� ���� ����� Ʋ��
            // acpAtomicGet32 �� �̿��Ͽ� �ѹ��� üũ��
            sAllRowRead = acpAtomicGet32( &sDataPlan->mAllRowRead );

            if ( sAllRowRead != 2 )
            {
                continue;
            }
            else
            {
                // nothing to do
            }

            /*
             * ������ dequeue ����������,
             * ���������� enqueue �� �ǰ� AllRowRead = 2 �� �Ǿ������� �ִ�.
             * �ٽ� Ȯ���ؾ���
             */
            if (dequeue( &sDataPlan->mQueue,
                         &sDataPlan->mRid,
                         &sDataPlan->mRow ) == ID_TRUE)
            {
                // queue ���� row �� �Ѱ� �����Դ�.
                *aFlag = QMC_ROW_DATA_EXIST;
                break;
            }
            else
            {
                // mAllRowRead �� 2: row �� �о���
                // row �� �� �о���, queue �� ����ִ� �����̴�.
                // ���̻� ���� row �� ����.
                *aFlag = QMC_ROW_DATA_NONE | QMC_ROW_QUEUE_EMPTY_TRUE;
                break;
            }
        }
        else
        {
            // Thread ���� ���� �߻�
            IDE_RAISE(ERR_IN_THREAD);
        }
    }

    if ( ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE ) &&
         ( (*aFlag & QMC_ROW_QUEUE_EMPTY_MASK) == QMC_ROW_QUEUE_EMPTY_TRUE ) )
    {
        // record�� ���� ���
        // ���� ������ ���� ����� �Լ��� ������.
        sDataPlan->doIt = qmnPRLQ::doItResume;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_IN_THREAD)
    {
        while (sDataPlan->mThrObj->mErrorCode == QMC_THR_ERROR_CODE_NONE)
        {
            idlOS::thr_yield();
        }
        IDE_SET(ideSetErrorCodeAndMsg(sDataPlan->mThrObj->mErrorCode,
                                      sDataPlan->mThrObj->mErrorMsg));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *    PRLQ �� ������� ���� �� ù ���� �Լ�
 *    Thread �� ������ ���� ��带 �����ϰ� �����ϵ��� �Ѵ�.
 *
 *    partition �ϳ� ó���Ҷ����� threadGet ���� �ʱ� ����
 *    PRLQ �� �ϳ��� scan �� ������ ��ȯ���� �ʰ� �ٸ� scan �� �����Ѵ�.
 *
 *    PPCRD �Ʒ��� ���� assign �ȵ� �ٸ� scan ���� ��ü�ɶ� ȣ��ȴ�.
 *    partition table �� ��츸 �ش�
 ***********************************************************************/
IDE_RC qmnPRLQ::doItResume( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag )
{
    qmndPRLQ * sDataPlan;
    qmnPlan  * sChildCodePlan;
    idBool     sIsSuccess;

    sDataPlan       = (qmndPRLQ*)(aTemplate->tmplate.data
                                  + aPlan->offset);
    sChildCodePlan  = (qmnPlan *)sDataPlan->mCurrentNode;

    IDE_DASSERT(sDataPlan->mThrObj != NULL);

    sDataPlan->doIt = qmnPRLQ::doItNextDequeue;

    /*
     * ----------------------------------------------------------------
     * thead �� ���ο� ���� ��带 ����, �����Ѵ�.
     * ----------------------------------------------------------------
     */

    // Template, Queue, AllRowRead, row buffer ���� �״�� ����Ѵ�.
    // ���� ��带 �����Ѵ�.
    sDataPlan->mThrArg->mChildPlan = sChildCodePlan;

    IDE_TEST(qmcThrWakeup(sDataPlan->mThrObj, &sIsSuccess) != IDE_SUCCESS);
    IDE_TEST_RAISE(sIsSuccess == ID_FALSE, ERR_IN_THREAD);

    // Queue ���� ù��° row �� �����´�.
    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_IN_THREAD)
    {
        while (sDataPlan->mThrObj->mErrorCode == QMC_THR_ERROR_CODE_NONE)
        {
            idlOS::thr_yield();
        }
        IDE_SET(ideSetErrorCodeAndMsg(sDataPlan->mThrObj->mErrorCode,
                                      sDataPlan->mThrObj->mErrorMsg));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPRLQ::runChildEnqueue( qmcThrObj * aThrObj )
{
    qcTemplate      * sTemplate;
    qmcRowFlag        sFlag = QMC_ROW_INITIALIZE;
    qmnPRLQThrArg   * sPRLQThrArg;

    qmnPlan         * sChildPlan;
    qmndPlan        * sChildDataPlan;
    qmnPRLQQueue    * sParallelQueue;
    void            * sOldChildTupleRow = NULL;
    UInt              sSleep;
    UInt              sQueueIdx;

    sPRLQThrArg    = (qmnPRLQThrArg*)aThrObj->mPrivateArg;
    // BUG-38294
    // Argument �� template �� ���޹޾� ����Ѵ�.
    sTemplate      = sPRLQThrArg->mTemplate;
    sChildPlan     = sPRLQThrArg->mChildPlan;
    sParallelQueue = sPRLQThrArg->mPRLQQueue;

    IDE_DASSERT( sChildPlan != NULL );

    sChildDataPlan = (qmndPlan*)(sTemplate->tmplate.data +
                                 sChildPlan->offset);

    // PROJ-2444 Parallel Aggreagtion
    IDE_TEST( sChildPlan->readyIt( sTemplate,
                                   sChildPlan,
                                   aThrObj->mID )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT ȯ���� ����ؼ� Child ���� �ÿ� Memory, Disk Type�� �Ǵ��ϰ� ������ �����ϵ��� �Ѵ�.
     */
    if ( ( sChildPlan->flag & QMN_PLAN_STORAGE_MASK ) == QMN_PLAN_STORAGE_DISK )
    {
        sOldChildTupleRow = sChildDataPlan->myTuple->row;

        // �ڽ� �÷��� myTuple->row �� �����͸� �̿��Ͽ�
        // rowBuffer �� �����Ѵ�.
        sChildDataPlan->myTuple->row = sPRLQThrArg->mRowBuffer[0];

        sQueueIdx = 0;
    }
    else
    {
        /* Nothing to do */
    }

    sSleep    = 1;

    (void)acpAtomicSet32(sPRLQThrArg->mAllRowRead, 1);

    // Child�� ����
    IDE_TEST( sChildPlan->doIt( sTemplate,
                                sChildPlan,
                                &sFlag )
              != IDE_SUCCESS );

    while (aThrObj->mStopFlag == ID_FALSE)
    {
        if ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST)
        {
            while (aThrObj->mStopFlag == ID_FALSE)
            {
                if (enqueue( sParallelQueue,
                             sChildDataPlan->myTuple->rid,
                             sChildDataPlan->myTuple->row) == ID_TRUE)
                {
                    /* PROJ-2464 hybrid partitioned table ����
                     *  - HPT ȯ���� ����ؼ� Child ���� �ÿ� Memory, Disk Type�� �Ǵ��ϰ� ������ �����ϵ��� �Ѵ�.
                     */
                    if ( ( sChildPlan->flag & QMN_PLAN_STORAGE_MASK ) == QMN_PLAN_STORAGE_DISK )
                    {
                        /* row buffer �ٲ�ġ�� */
                        sQueueIdx = ((sQueueIdx + 1) < sParallelQueue->mSize) ?
                            (sQueueIdx + 1) : 0;

                        // PROJ-2444 Parallel Aggreagtion
                        sChildDataPlan->myTuple->row = sPRLQThrArg->mRowBuffer[sQueueIdx];
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    sSleep = 1;
                    break;
                }
                else
                {
                    /* enqueue fail => wait for a while */

                    acpSleepUsec(sSleep);
                    sSleep = (sSleep < QCU_PARALLEL_QUERY_QUEUE_SLEEP_MAX) ?
                        (sSleep << 1) : 1;
                }
            }
        }
        else
        {
            // ��� row read �Ϸ�
            acpAtomicSet32(sPRLQThrArg->mAllRowRead, 2);
            break;
        }

        // Child�� ����
        IDE_TEST( sChildPlan->doIt( sTemplate,
                                    sChildPlan,
                                    &sFlag )
                  != IDE_SUCCESS );
    }

    if ( ( sChildPlan->flag & QMN_PLAN_STORAGE_MASK ) == QMN_PLAN_STORAGE_DISK )
    {
        sChildDataPlan->myTuple->row = sOldChildTupleRow;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // Set error code to thread argument
    aThrObj->mErrorCode = ideGetErrorCode();
    aThrObj->mErrorMsg  = ideGetErrorMsg();
    (void)acpAtomicSet32(sPRLQThrArg->mAllRowRead, 3);

    return IDE_FAILURE;
}

/*
 * ------------------------------------------------------------------
 * PPCRD �κ��� ȣ���
 * PRLQ �� �����ؾ� �� SCAN �� ����
 * ------------------------------------------------------------------
 */
void qmnPRLQ::setChildNode( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,         /* PRLQ */
                            qmnPlan    * aChildPlan )   /* SCAN */
{
    qmndPRLQ * sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);

    // 1. ���� ��� ����
    sDataPlan->mCurrentNode = aChildPlan;
}

/***********************************************************************
 *
 * Description :
 *    PRLQ �� thread �� ��ȯ�Ѵ�.
 *    ���� PRLQ �� ��� ����� �������� PPCRD �κ��� ȣ��ȴ�.
 *
 * Implementation :
 *    Thread �� wait ���°� �� ������ ��ٸ� �� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
void qmnPRLQ::returnThread( qcTemplate * aTemplate, qmnPlan * aPlan )
{
    qmndPRLQ* sDataPlan;

    sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);

    IDE_DASSERT( sDataPlan->mThrObj != NULL );
    IDE_DASSERT(((qmncPRLQ*)aPlan)->mParallelType ==
                QMNC_PRLQ_PARALLEL_TYPE_PARTITION)

    while (acpAtomicGet32(&sDataPlan->mThrObj->mStatus) ==
           QMC_THR_STATUS_RUN)
    {
        idlOS::thr_yield();
    }

    qmcThrReturn(aTemplate->stmt->mThrMgr, sDataPlan->mThrObj);

    acpAtomicSet32(&sDataPlan->mAllRowRead, 0);
}

/*
 * ------------------------------------------------------------------
 * BUG-38024 print wrong subquery plan when execute parallel query
 *
 * return the PRLQ's own template
 * if data plan is not initialized, return the original template
 * this function is called by qmnPPCRD::printPlan()
 * ------------------------------------------------------------------
 */
qcTemplate* qmnPRLQ::getTemplate(qcTemplate* aTemplate, qmnPlan* aPlan)
{
    qmncPRLQ  * sCodePlan;
    qmndPRLQ  * sDataPlan;
    qcTemplate* sResult;

    sCodePlan = (qmncPRLQ*)aPlan;
    sDataPlan = (qmndPRLQ*)(aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = &aTemplate->planFlag[sCodePlan->planID];

    if ((*sDataPlan->flag & QMND_PRLQ_INIT_DONE_MASK) ==
        QMND_PRLQ_INIT_DONE_FALSE)
    {
        sResult = aTemplate;
    }
    else
    {
        if (sDataPlan->mTemplateCloned == ID_FALSE)
        {
            sResult = aTemplate;
        }
        else
        {
            if (sDataPlan->plan.mTID == QMN_PLAN_INIT_THREAD_ID)
            {
                /* serial execution */
                sResult = aTemplate;
            }
            else
            {
                sResult = &sDataPlan->mTemplate;
            }
        }
    }
    return sResult;
}

/***********************************************************************
 *
 * Description : PROJ-2464 hybrid partitioned table ����
 *
 *    PRLQ node�� Data ������ �߰� ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
void qmnPRLQ::setPRLQInfo( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,         /* PRLQ */
                           idBool       aExistDisk,      /* INFO 1 */
                           UInt         aDiskRowOffset ) /* INFO 2 */
{
    qmndPRLQ * sDataPlan = (qmndPRLQ *)(aTemplate->tmplate.data + aPlan->offset);

    /* INFO 1 */
    sDataPlan->mExistDisk = aExistDisk;

    /* INFO 2 */
    /* BUG-43403 mExistDisk�� ������ ��, Disk Row Offset�� �����Ѵ�. */
    sDataPlan->mDiskRowOffset = aDiskRowOffset;
}
