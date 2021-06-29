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
 * $Id: qmnSetIntersect.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     SITS(Set IntTerSection) Node
 *
 *     ������ �𵨿��� hash-based set intersection ������
 *     �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnSetIntersect.h>
#include <qmxResultCache.h>

IDE_RC
qmnSITS::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SITS ����� �ʱ�ȭ
 *
 * Implementation :
 *    Left Dependent�� �߻��� ��� ����� ������ ��� ������ ��.
 *    Right Dependent�� �߻��� ��� ����� ������ �״�� ����� ��
 *    ������, Hit Flag�� clear�Ͽ� right�� ������� �� �ִ�.
 *
 ***********************************************************************/
    qmncSITS * sCodePlan = (qmncSITS *) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS *) (aTemplate->tmplate.data + aPlan->offset);

    idBool sLeftDependency;
    idBool sRightDependency;
    idBool sIsSkip = ID_FALSE;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnSITS::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_SITS_INIT_DONE_MASK)
         == QMND_SITS_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( checkLeftDependency( sDataPlan,
                                   &sLeftDependency ) != IDE_SUCCESS );

    if ( sLeftDependency == ID_TRUE )
    {
        /* PROJ-2462 Result Cache */
        if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
        {
            sIsSkip = ID_FALSE;
        }
        else
        {
            sDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[sCodePlan->planID];
            if ( ( ( *sDataPlan->resultData.flag & QMX_RESULT_CACHE_STORED_MASK )
                   == QMX_RESULT_CACHE_STORED_TRUE ) &&
                 ( sDataPlan->leftDepValue == QMN_PLAN_DEFAULT_DEPENDENCY_VALUE ) )
            {
                sIsSkip = ID_TRUE;
            }
            else
            {
                sIsSkip = ID_FALSE;
            }
        }

        if ( sIsSkip == ID_FALSE )
        {
            //----------------------------------------
            // Left Dependent Row�� ����� ���
            // ���� Row�� �籸���Ѵ�.
            //----------------------------------------

            // 1. Temp Table Clear
            IDE_TEST( qmcHashTemp::clear( sDataPlan->hashMgr )
                      != IDE_SUCCESS );

            // 2. Left �����Ͽ� ����
            IDE_TEST( storeLeft( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );
            // 3. Right �����Ͽ� intersected row ����
            IDE_TEST( setIntersectedRows( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );

            /* PROJ-2462 Result Cache */
            if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
                 == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
            {
                *sDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_STORED_MASK;
                *sDataPlan->resultData.flag |= QMX_RESULT_CACHE_STORED_TRUE;
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
    }
    else
    {
        IDE_TEST( checkRightDependency( sDataPlan,
                                        & sRightDependency ) != IDE_SUCCESS );
        if ( sRightDependency == ID_TRUE )
        {
            //----------------------------------------
            // Right Dependent Row�� ����� ���
            // Intersected Row�� �籸���Ѵ�.
            //----------------------------------------

            // 1. Hit Flag ����
            // 2. Right�� �����Ͽ� intersected row ����
            IDE_TEST( qmcHashTemp::clearHitFlag( sDataPlan->hashMgr )
                      != IDE_SUCCESS );

            IDE_TEST( setIntersectedRows( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------------
    // ���� �Լ� ����
    //----------------------------------------

    if ( ( sCodePlan->flag & QMNC_SITS_IN_TOP_MASK )
         == QMNC_SITS_IN_TOP_FALSE )
    {
        sDataPlan->doIt = qmnSITS::doItFirstIndependent;
    }
    else
    {
        if ( sIsSkip == ID_TRUE )
        {
            sDataPlan->doIt = qmnSITS::doItFirstIndependent;
        }
        else
        {
            sDataPlan->doIt = qmnSITS::doItFirst;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnSITS::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SITS �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *    SITS�� ���� ���� �׻� VIEW�̴�.
 *    ����, ����� ������ ��� VIEW���� ó���� �� �ֵ���
 *    �� ����� Stack�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSITS * sCodePlan = (qmncSITS *) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS*) (aTemplate->tmplate.data + aPlan->offset);

    qmdMtrNode * sNode;
    void       * sRow;

    mtcStack   * sStack;
    SInt         sRemain;

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    //-----------------------------------
    // ���� VIEW ��带 ���� Stack ����
    //-----------------------------------

    sStack  = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        sRow = sDataPlan->plan.myTuple->row;

        for ( sNode = sDataPlan->mtrNode;
              sNode != NULL;
              sNode = sNode->next,
                  aTemplate->tmplate.stack++,
                  aTemplate->tmplate.stackRemain-- )
        {
            IDE_TEST_RAISE(aTemplate->tmplate.stackRemain < 1,
                           ERR_STACK_OVERFLOW);

            aTemplate->tmplate.stack->value =
                (void*)( (UChar*)sRow + sNode->dstColumn->column.offset);
            aTemplate->tmplate.stack->column = sNode->dstColumn;
        }

        aTemplate->tmplate.stack = sStack;
        aTemplate->tmplate.stackRemain = sRemain;

    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

        aTemplate->tmplate.stack = sStack;
        aTemplate->tmplate.stackRemain = sRemain;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::padNull( qcTemplate * /* aTemplate */,
                  qmnPlan    * /* aPlan */)
{
/***********************************************************************
 *
 * Description :
 *    ȣ��Ǿ�� �ȵ�.
 *    ���� Node�� �ݵ�� VIEW����̸�,
 *    View�� �ڽ��� Null Row���� �����ϱ� �����̴�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSITS * sCodePlan = (qmncSITS*) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndSITS * sCacheDataPlan = NULL;
    idBool     sIsInit       = ID_FALSE;

    SLong sRecordCnt;
    ULong sPageCnt;
    UInt  sBucketCnt;

    ULong i;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // ���� ���� ���
    //----------------------------

    if ( aMode == QMN_DISPLAY_ALL )
    {
        //----------------------------
        // explain plan = on; �� ���
        //----------------------------

        if ( (*sDataPlan->flag & QMND_SITS_INIT_DONE_MASK)
             == QMND_SITS_INIT_DONE_TRUE )
        {
            sIsInit = ID_TRUE;
            // ���� ���� ȹ��
            IDE_TEST( qmcHashTemp::getDisplayInfo( sDataPlan->hashMgr,
                                                   & sPageCnt,
                                                   & sRecordCnt,
                                                   & sBucketCnt )
                      != IDE_SUCCESS );

            if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY )
            {
                if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                {
                    iduVarStringAppendFormat(
                        aString,
                        "SET-INTERSECT ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "BUCKET_COUNT: %"ID_UINT32_FMT", "
                        "ACCESS: %"ID_UINT32_FMT,
                        sDataPlan->mtrRowSize,
                        sRecordCnt,
                        sBucketCnt,
                        sDataPlan->plan.myTuple->modify );
                }
                else
                {
                    // BUG-29209
                    // ITEM_SIZE ���� �������� ����
                    iduVarStringAppendFormat(
                        aString,
                        "SET-INTERSECT ( "
                        "ITEM_SIZE: BLOCKED, "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "BUCKET_COUNT: %"ID_UINT32_FMT", "
                        "ACCESS: %"ID_UINT32_FMT,
                        sRecordCnt,
                        sBucketCnt,
                        sDataPlan->plan.myTuple->modify );
                }
            }
            else
            {
                if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                {
                    iduVarStringAppendFormat(
                        aString,
                        "SET-INTERSECT ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "DISK_PAGE_COUNT: %"ID_UINT64_FMT", "
                        "ACCESS: %"ID_UINT32_FMT,
                        sDataPlan->mtrRowSize,
                        sRecordCnt,
                        sPageCnt,
                        sDataPlan->plan.myTuple->modify );
                }
                else
                {
                    // BUG-29209
                    // ITEM_SIZE, DISK_PAGE_COUNT ���� �������� ����
                    iduVarStringAppendFormat(
                        aString,
                        "SET-INTERSECT ( "
                        "ITEM_SIZE: BLOCKED, "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "DISK_PAGE_COUNT: BLOCKED, "
                        "ACCESS: %"ID_UINT32_FMT,
                        sRecordCnt,
                        sDataPlan->plan.myTuple->modify );
                }
            }
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "SET-INTERSECT ( "
                                      "ITEM_SIZE: 0, "
                                      "ITEM_COUNT: 0, "
                                      "BUCKET_COUNT: %"ID_UINT32_FMT", "
                                      "ACCESS: 0",
                                      sCodePlan->bucketCnt );

        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        iduVarStringAppendFormat( aString,
                                  "SET-INTERSECT ( "
                                  "ITEM_SIZE: ??, "
                                  "ITEM_COUNT: ??, "
                                  "BUCKET_COUNT: %"ID_UINT32_FMT", "
                                  "ACCESS: ??",
                                  sCodePlan->bucketCnt );
    }

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    /* PROJ-2462 Result Cache */
    if ( QCU_TRCLOG_DETAIL_RESULTCACHE == 1 )
    {
        if ( ( sCodePlan->componentInfo != NULL ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
               == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
               == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
        {
            qmn::printResultCacheRef( aString,
                                      aDepth,
                                      sCodePlan->componentInfo );
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
    //----------------------------
    // PROJ-1473 mtrNode info 
    //----------------------------

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        if ( ( sCodePlan->componentInfo != NULL ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
               == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
               == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
        {
            sCacheDataPlan = (qmndSITS *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
            qmn::printResultCacheInfo( aString,
                                     aDepth,
                                     aMode,
                                     sIsInit,
                                     &sCacheDataPlan->resultData );
        }
        else
        {
            /* Nothing to do */
        }
        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->myNode,
                           "myNode",
                           sCodePlan->myNode->dstNode->node.table,
                           sCodePlan->leftDepTupleRowID,
                           sCodePlan->rightDepTupleRowID );
    }

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
   
    //----------------------------
    // Child Plan ���� ���
    //----------------------------

    IDE_TEST( aPlan->left->printPlan( aTemplate,
                                      aPlan->left,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    IDE_TEST( aPlan->right->printPlan( aTemplate,
                                       aPlan->right,
                                       aDepth + 1,
                                       aString,
                                       aMode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::doItDefault( qcTemplate * /* Template */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */ )
{
/***********************************************************************
 *
 * Description :
 *    ȣ��Ǿ�� �ȵ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Top Query���� ���Ǵ� ���, ���� ���� �Լ�
 *
 * Implementation :
 *    Right Child�� �ʱ�ȭ�� �� Intersected Row�� �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSITS * sCodePlan = (qmncSITS *) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    IDE_TEST( aPlan->right->init(aTemplate, aPlan->right )
              != IDE_SUCCESS );

    sDataPlan->doIt = qmnSITS::doItNext;
    IDE_TEST( qmnSITS::doItNext( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Top Query���� ���Ǵ� ���, ���� ���� �Լ�
 *
 * Implementation :
 *    Right�� ���� ���� Row�� �����ϰ�,
 *    Hash Temp Table�� �̿��Ͽ� intersected row�� �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndSITS * sDataPlan =
        (qmndSITS *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;

    void * sSearchRow;

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    idBool     sIsSetMtrRow;

    //------------------------------
    // Right ����
    //------------------------------

    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
              != IDE_SUCCESS );

    // To Fix PR-8060
    sSearchRow = NULL;

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        //------------------------------
        // Right�� ���� ���� Row ����
        //------------------------------

        // jhseong, PR-10107
        sDataPlan->plan.myTuple->row = sDataPlan->temporaryRightRow;

        // PR-24190
        // select i1(varchar(30)) from t1 minus select i1(varchar(250)) from t2;
        // ����� ���� ����������        
        IDE_TEST( setRightChildMtrRow( aTemplate, sDataPlan, &sIsSetMtrRow )
                  != IDE_SUCCESS );

        sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
        
        if( sIsSetMtrRow == ID_TRUE )
        {
            //------------------------------
            // Hash Temp Table�� �̿��� intersected row �˻�
            //------------------------------

            IDE_TEST( qmcHashTemp::getSameRowAndNonHit( sDataPlan->hashMgr,
                                                        sDataPlan->plan.myTuple->row,
                                                        & sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sSearchRow = NULL;            
        }

        sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;
        
        if ( sSearchRow != NULL )
        {
            break;
        }
        else
        {
            //------------------------------
            // Right�� ���� ���� Row ����
            //------------------------------

            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
                      != IDE_SUCCESS );
        }
    }

    if ( sSearchRow != NULL )
    {
        //------------------------------
        // �ٽ� ���õ��� �ʵ��� Hit Flag Setting
        //------------------------------

        IDE_TEST( qmcHashTemp::setHitFlag( sDataPlan->hashMgr )
                  != IDE_SUCCESS );

        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;

        sDataPlan->plan.myTuple->modify++;

        // PR-9055 fixed
        // ������ �о�� ���� hit flag�� �����ؾ� �ϴ� ���̶��
        // �����Ŀ� �� �޸𸮿� ����� �ʰ� ���� ������ �о�;� �ϹǷ�
        // sOrgRow�� �ٽ� �������ش�.

// 10107, kbjung->jhseong        sDataPlan->plan.myTuple->row = sOrgRow;

    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;

        // Top Query���� ���Ǵ� ����̹Ƿ�
        // �ٽ� ȣ��� ���� ����.
        sDataPlan->doIt = qmnSITS::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnSITS::doItFirstIndependent( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Sub Query���� ���Ǵ� ���, ���� ���� �Լ�
 *
 * Implementation :
 *    �̹� Intersected Row�� ��� �����Ǿ� ������,
 *    Hash Temp Table�� �̿��Ͽ� �̸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doItFirstIndependent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSITS * sCodePlan = (qmncSITS *) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    //--------------------------------
    // Intersected Row ����
    //--------------------------------

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getFirstHit( sDataPlan->hashMgr,
                                        & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    if ( sSearchRow != NULL )
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;

        sDataPlan->plan.myTuple->modify++;

        sDataPlan->doIt = qmnSITS::doItNextIndependent;
    }
    else
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::doItNextIndependent( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Sub Query���� ���Ǵ� ���, ���� ���� �Լ�
 *
 * Implementation :
 *    �̹� Intersected Row�� ��� �����Ǿ� ������,
 *    Hash Temp Table�� �̿��Ͽ� �̸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::doItNextIndependent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSITS * sCodePlan = (qmncSITS *) aPlan;
    qmndSITS * sDataPlan =
        (qmndSITS *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    // Intersected Row ����

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getNextHit( sDataPlan->hashMgr,
                                       & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    if ( sSearchRow != NULL )
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;

        sDataPlan->plan.myTuple->modify++;
    }
    else
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_NONE;

        sDataPlan->doIt = qmnSITS::doItFirstIndependent;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnSITS::firstInit( qcTemplate * aTemplate,
                    qmncSITS   * aCodePlan,
                    qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndSITS * sCacheDataPlan = NULL;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    //---------------------------------
    // SITS ���� ������ �ʱ�ȭ
    //---------------------------------
    //
    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndSITS *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
        aDataPlan->resultData.flag     = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
        if ( qmxResultCache::initResultCache( aTemplate,
                                              aCodePlan->componentInfo,
                                              &sCacheDataPlan->resultData )
             != IDE_SUCCESS )
        {
            *aDataPlan->flag &= ~QMN_PLAN_RESULT_CACHE_EXIST_MASK;
            *aDataPlan->flag |= QMN_PLAN_RESULT_CACHE_EXIST_FALSE;
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

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // To Fix PR-8060
    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    aDataPlan->leftDepTuple =
        & aTemplate->tmplate.rows[aCodePlan->leftDepTupleRowID];
    aDataPlan->leftDepValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    aDataPlan->rightDepTuple =
        & aTemplate->tmplate.rows[aCodePlan->rightDepTupleRowID];
    aDataPlan->rightDepValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_SITS_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SITS_INIT_DONE_TRUE;

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        *aDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_INIT_DONE_MASK;
        *aDataPlan->resultData.flag |= QMX_RESULT_CACHE_INIT_DONE_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSITS::initMtrNode( qcTemplate * aTemplate,
                      qmncSITS   * aCodePlan,
                      qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ ���� ��带 �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt        sHeaderSize = 0;
    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );

    //---------------------------------
    // ���� ������ ���� ������ �ʱ�ȭ
    //---------------------------------

    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sHeaderSize = QMC_MEMHASH_TEMPHEADER_SIZE;

        /* PROJ-2462 Result Cache */
        if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
        {
            aDataPlan->mtrNode = ( qmdMtrNode * )( aTemplate->resultCache.data +
                                                   aCodePlan->mtrNodeOffset );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        sHeaderSize = QMC_DISKHASH_TEMPHEADER_SIZE;
    }

    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    // 1.  ���� Column�� ���� ���� ����
    // 2.  ���� Column�� �ʱ�ȭ
    // 3.  ���� Column�� offset�� ������
    // 4.  Row Size�� ���
    //     - Disk Temp Table�� ��� Row�� ���� Memory�� �Ҵ����.

    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode, aDataPlan->mtrNode )
              != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                0 ) != IDE_SUCCESS );

    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  sHeaderSize ) != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnSITS::initTempTable( qcTemplate * aTemplate,
                        qmncSITS   * aCodePlan,
                        qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt       sFlag          = 0;
    qmndSITS * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    // ��� Column�� Hashing ����̴�.
    IDE_DASSERT( (aDataPlan->mtrNode->flag & QMC_MTR_HASH_NEED_MASK )
                 == QMC_MTR_HASH_NEED_TRUE );

    //-----------------------------
    // Flag ���� �ʱ�ȭ
    //-----------------------------

    sFlag = QMCD_HASH_TMP_DISTINCT_TRUE | QMCD_HASH_TMP_PRIMARY_TRUE;

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sFlag &= ~QMCD_HASH_TMP_STORAGE_TYPE;
        sFlag |= QMCD_HASH_TMP_STORAGE_MEMORY;

        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_MEMORY );
    }
    else
    {
        sFlag &= ~QMCD_HASH_TMP_STORAGE_TYPE;
        sFlag |= QMCD_HASH_TMP_STORAGE_DISK;

        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_DISK );
    }
    
    // PROJ-2553
    // DISTINCT Hashing�� Bucket List Hashing ����� ��� �Ѵ�.
    sFlag &= ~QMCD_HASH_TMP_HASHING_TYPE;
    sFlag |= QMCD_HASH_TMP_HASHING_BUCKET;

    // BUG-31997: When using temporary tables by RID, RID refers to
    // the invalid row.
    /* QMNC_SITS_IN_TOP_TRUE �϶��� temp table �� ������ �������� �ʰ� 
     * 1 row �� insert �ϰ� ���� �÷����� insert �� rid �� �״�� �����ϰ� �ȴ�.
     * ������ hash temp table �� index�� ��������� rid �� ����ɼ� �ִ�.
     * ���� SM ���� rid �� �������� �ʵ��� ��û�� �ؾ� �Ѵ�. */
    if ( (aCodePlan->flag & QMNC_SITS_IN_TOP_MASK) == QMNC_SITS_IN_TOP_TRUE )
    {
        if ( (aCodePlan->plan.flag & QMN_PLAN_TEMP_FIXED_RID_MASK)
             == QMN_PLAN_TEMP_FIXED_RID_TRUE )
        {
            sFlag &= ~QMCD_HASH_TMP_FIXED_RID_MASK;
            sFlag |= QMCD_HASH_TMP_FIXED_RID_TRUE;
        }
    }

    //-----------------------------
    // Temp Table �ʱ�ȭ
    //-----------------------------
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnSITS::initTempTable::qmxAlloc:hashMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                  (void **)&aDataPlan->hashMgr )
                  != IDE_SUCCESS );
        IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->mtrNode,
                                     NULL,  // Aggregation Column����
                                     aCodePlan->bucketCnt,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndSITS *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnSITS::initTempTable::qrcAlloc:hashMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                               (void **)&aDataPlan->hashMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->mtrNode,
                                         NULL,  // Aggregation Column����
                                         aCodePlan->bucketCnt,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->hashMgr = aDataPlan->hashMgr;
        }
        else
        {
            aDataPlan->hashMgr = sCacheDataPlan->hashMgr;
            aDataPlan->temporaryRightRow = sCacheDataPlan->temporaryRightRow;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSITS::checkLeftDependency( qmndSITS   * aDataPlan,
                              idBool     * aDependent )
{
/***********************************************************************
 *
 * Description :
 *    Left Dependent Tuple�� ���� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( aDataPlan->leftDepValue != aDataPlan->leftDepTuple->modify )
    {
        *aDependent = ID_TRUE;
    }
    else
    {
        *aDependent = ID_FALSE;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmnSITS::checkRightDependency( qmndSITS   * aDataPlan,
                               idBool     * aDependent )
{
/***********************************************************************
 *
 * Description :
 *     Right Dependent Tuple�� ���� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::checkRightDependency"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( aDataPlan->rightDepValue != aDataPlan->rightDepTuple->modify )
    {
        *aDependent = ID_TRUE;
    }
    else
    {
        *aDependent = ID_FALSE;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnSITS::storeLeft( qcTemplate * aTemplate,
                    qmncSITS   * aCodePlan,
                    qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Left�� �����Ͽ� distinct hashing���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::storeLeft"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;
    idBool     sInserted;
    qmndSITS * sCacheDataPlan = NULL;

    //---------------------------------------
    // Left Child ����
    //---------------------------------------

    IDE_TEST( aCodePlan->plan.left->init( aTemplate,
                                          aCodePlan->plan.left )
              != IDE_SUCCESS);

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    //---------------------------------------
    // �ݺ� �����Ͽ� Temp Table ����
    //---------------------------------------

    sInserted = ID_TRUE;

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //---------------------------------------
        // 1.  ���� Row�� ���� ���� �Ҵ�
        //---------------------------------------

        if ( sInserted == ID_TRUE )
        {
            IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                          & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            // ������ ������ ���� �̹� �Ҵ���� ������ ����Ѵ�.
            // ����, ������ ������ �Ҵ� ���� �ʿ䰡 ����.
        }

        //---------------------------------------
        // 2.  ���� Row�� ����
        //---------------------------------------

        IDE_TEST( setMtrRow( aTemplate,
                             aDataPlan ) != IDE_SUCCESS );

        //---------------------------------------
        // 3.  ���� Row�� ����
        //---------------------------------------

        IDE_TEST( qmcHashTemp::addDistRow( aDataPlan->hashMgr,
                                           & aDataPlan->plan.myTuple->row,
                                           & sInserted )
                  != IDE_SUCCESS );

        //---------------------------------------
        // 4.  Left Child ����
        //---------------------------------------

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );

    }

    //---------------------------------------
    // Right ó���� ���� �޸� ���� Ȯ��
    //---------------------------------------

    if ( sInserted == ID_TRUE )
    {
        IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                      & aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

    }
    else
    {
        // �̹� ������ ���� ����
    }

    aDataPlan->leftDepValue = aDataPlan->leftDepTuple->modify;
    // jhseong, PR-10107
    aDataPlan->temporaryRightRow = aDataPlan->plan.myTuple->row;

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndSITS *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->temporaryRightRow = aDataPlan->temporaryRightRow;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;


#undef IDE_FN
}

IDE_RC
qmnSITS::setRightChildMtrRow( qcTemplate * aTemplate,
                              qmndSITS   * aDataPlan,
                              idBool     * aIsSetMtrRow )
{
/***********************************************************************
 *
 * Description :
 *    Right Child�� ����� ���� Row�� ����
 *
 * Implementation :
 *    ���� ��尡 �ݵ�� PROJ ����̱� ������ Stack�� ���� �����
 *    ����Ͽ� ���� Row�� �����Ѵ�.
 *
 * BUG-24190
 * select i1(varchar(30)) from t1 minus select i1(varchar(250)) from t2;
 * ����� ���� ����������.
 *
 *  right child�� actualsize�� ū ��� skip �Ѵ�. 
 *
 *                [ SET-INTERSECT ] ( �÷������� : 30 )
 *                         |
 *           -----------------------------
 *           |                            |
 *         [ T1 : varchar(30) ]    [ T2 : varchar(250) ]
 *
 ***********************************************************************/

    qmdMtrNode * sNode;

    mtcStack   * sStack;
    SInt         sRemain;

    UInt         sActualSize;    

    sStack = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    *aIsSetMtrRow = ID_TRUE;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next,
              aTemplate->tmplate.stack++,
              aTemplate->tmplate.stackRemain-- )
    {
        IDE_TEST_RAISE(aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW);

        sActualSize = aTemplate->tmplate.stack->column->module->actualSize(
            aTemplate->tmplate.stack->column,
            aTemplate->tmplate.stack->value );

        if ( sActualSize > aDataPlan->plan.myTuple->columns[sNode->dstNode->node.column].column.size )
        {
            *aIsSetMtrRow = ID_FALSE;
            break;            
        }
        else
        {
            // Nothing To Do 
        }
    }

    aTemplate->tmplate.stack = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    if( *aIsSetMtrRow == ID_TRUE )
    {        
        IDE_TEST( setMtrRow( aTemplate,
                             aDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }   

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

        // fix BUG-33678
        aTemplate->tmplate.stack = sStack;
        aTemplate->tmplate.stackRemain = sRemain;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSITS::setMtrRow( qcTemplate * aTemplate,
                    qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child�� ����� ���� Row�� ����
 *
 * Implementation :
 *    ���� ��尡 �ݵ�� PROJ ����̱� ������ Stack�� ���� �����
 *    ����Ͽ� ���� Row�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    mtcStack * sStack;
    SInt       sRemain;
    void *     sRow;

    sStack = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    sRow = aDataPlan->plan.myTuple->row;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next,
              aTemplate->tmplate.stack++,
              aTemplate->tmplate.stackRemain-- )
    {
        IDE_TEST_RAISE(aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW);
        idlOS::memcpy(
            (SChar*) sRow + sNode->dstColumn->column.offset,
            (SChar*) aTemplate->tmplate.stack->value,
            aTemplate->tmplate.stack->column->module->actualSize(
                aTemplate->tmplate.stack->column,
                aTemplate->tmplate.stack->value ) );
    }

    aTemplate->tmplate.stack = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

        // fix BUG-33678
        aTemplate->tmplate.stack = sStack;
        aTemplate->tmplate.stackRemain = sRemain;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSITS::setIntersectedRows( qcTemplate * aTemplate,
                             qmncSITS   * aCodePlan,
                             qmndSITS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Subquery���� ���Ǵ� ��� Intersected Row���� ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSITS::setIntersectedRows"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    if ( ( aCodePlan->flag & QMNC_SITS_IN_TOP_MASK )
         == QMNC_SITS_IN_TOP_FALSE )
    {
        //---------------------------------------
        // Right�� �ݺ� �����Ͽ� Insersected Row���� ����
        //---------------------------------------

        IDE_TEST( qmnSITS::doItFirst( aTemplate,
                                      (qmnPlan*) aCodePlan,
                                      & sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( qmnSITS::doItNext( aTemplate,
                                         (qmnPlan*) aCodePlan,
                                         & sFlag )
                      != IDE_SUCCESS );
        }

        // Dependenct Value ����
        aDataPlan->rightDepValue = aDataPlan->rightDepTuple->modify;
    }
    else
    {
        // Top Query���� ���Ǵ� ��� �ʱ�ȭ ��������
        // intersected row�� �������� �ʰ�
        // ���� �������� �ٷ� intersected row�� �����Ѵ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
