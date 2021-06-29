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
 * $Id: qmnSort.cpp 83667 2018-08-09 06:50:14Z ahra.cho $
 *
 * Description :
 *     SORT(SORT) Node
 *
 *     ������ �𵨿��� sorting ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - ORDER BY
 *         - Sort-based Grouping
 *         - Sort-based Distinction
 *         - Sort Join
 *         - Merge Join
 *         - Sort-based Left Outer Join
 *         - Sort-based Full Outer Join
 *         - Store And Search
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmoUtil.h>
#include <qmnSort.h>
#include <qcg.h>
#include <qmxResultCache.h>

IDE_RC
qmnSORT::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SORT ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    idBool sDependency;

    sDataPlan->doIt        = qmnSORT::doItDefault;
    sDataPlan->searchFirst = qmnSORT::searchDefault;
    sDataPlan->searchNext  = qmnSORT::searchDefault;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_SORT_INIT_DONE_MASK)
         == QMND_SORT_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( checkDependency( aTemplate,
                               sCodePlan,
                               sDataPlan,
                               & sDependency ) != IDE_SUCCESS );

    if ( sDependency == ID_TRUE )
    {
        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        IDE_TEST( qmcSortTemp::clear( sDataPlan->sortMgr ) != IDE_SUCCESS );

        //----------------------------------------
        // Child�� �ݺ� �����Ͽ� Temp Table�� ����
        //----------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        IDE_TEST( storeAndSort( aTemplate, sCodePlan, sDataPlan )
                  != IDE_SUCCESS);

        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        sDataPlan->depValue = sDataPlan->depTuple->modify;
    }
    else
    {
        // nothing to do
    }

    //----------------------------------------
    // �˻� �Լ� �� ���� �Լ� ����
    //----------------------------------------

    IDE_TEST( setSearchFunction( sCodePlan, sDataPlan )
              != IDE_SUCCESS );

    sDataPlan->doIt = qmnSORT::doItFirst;

    // TODO - A4 Grouping Set Integration
    /*
      sDataPlan->needUpdateCountOfSiblings = ID_TRUE;
    */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnSORT::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SORT �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SORT ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_SORT_INIT_DONE_MASK)
         == QMND_SORT_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    IDE_TEST( qmcSortTemp::getNullRow( sDataPlan->sortMgr,
                                       & sDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    IDE_DASSERT( sDataPlan->plan.myTuple->row != NULL );

    sDataPlan->plan.myTuple->modify++;

    // To Fix PR-9822
    // padNull() �Լ��� Child �� modify ���� �����Ű�� �ȴ�.
    // �̴� �籸�� ���ο� ���谡 �����Ƿ� �� ���� �����Ͽ�
    // �籸���� ���� �ʵ��� �Ѵ�.
    sDataPlan->depValue = sDataPlan->depTuple->modify;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    SORT ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSORT * sCodePlan = (qmncSORT*) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndSORT * sCacheDataPlan = NULL;

    ULong  i;
    ULong  sDiskPageCnt;
    SLong  sRecordCnt;
    idBool sIsInit      = ID_FALSE;
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

        if ( (*sDataPlan->flag & QMND_SORT_INIT_DONE_MASK)
             == QMND_SORT_INIT_DONE_TRUE )
        {
            sIsInit = ID_TRUE;
            IDE_TEST( qmcSortTemp::getDisplayInfo( sDataPlan->sortMgr,
                                                   & sDiskPageCnt,
                                                   & sRecordCnt )
                      != IDE_SUCCESS );

            if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY )
            {
                // Memory Temp Table�� ���
                // To Fix BUG-9034
                if ( ( sCodePlan->flag & QMNC_SORT_STORE_MASK )
                     == QMNC_SORT_STORE_ONLY )
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "STORE ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "STORE ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
                else
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "SORT ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "SORT ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
            }
            else
            {
                // Disk Temp Table�� ���
                // To Fix BUG-9034
                if ( ( sCodePlan->flag & QMNC_SORT_STORE_MASK )
                     == QMNC_SORT_STORE_ONLY )
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "STORE ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: %"ID_UINT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sRecordCnt,
                            sDiskPageCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "STORE ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: BLOCKED, "
                            "ACCESS: %"ID_UINT32_FMT,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
                else
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "SORT ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: %"ID_UINT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sRecordCnt,
                            sDiskPageCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "SORT ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: BLOCKED, "
                            "ACCESS: %"ID_UINT32_FMT,
                            sRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
            }
        }
        else
        {
            // �ʱ�ȭ���� ���� ���
            // To Fix BUG-9034
            if ( ( sCodePlan->flag & QMNC_SORT_STORE_MASK )
                 == QMNC_SORT_STORE_ONLY )
            {
                iduVarStringAppendFormat( aString,
                                          "STORE ( ITEM_SIZE: 0, ITEM_COUNT: 0, "
                                          "ACCESS: 0" );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "SORT ( ITEM_SIZE: 0, ITEM_COUNT: 0, "
                                          "ACCESS: 0" );
            }
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        // To Fix BUG-9034
        if ( ( sCodePlan->flag & QMNC_SORT_STORE_MASK )
             == QMNC_SORT_STORE_ONLY )
        {
            iduVarStringAppendFormat( aString,
                                      "STORE ( ITEM_SIZE: ??, ITEM_COUNT: ??, "
                                      "ACCESS: ??" );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "SORT ( ITEM_SIZE: ??, ITEM_COUNT: ??, "
                                      "ACCESS: ??" );
        }
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
            sCacheDataPlan = (qmndSORT *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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
                           sCodePlan->depTupleRowID,
                           ID_USHORT_MAX );
    }

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        if (sCodePlan->range != NULL)
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->range)
                     != IDE_SUCCESS);
        }
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT(0);

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *    �˻� �� Row�� �����ϸ�, Tuple Set�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // Option�� ���� �˻� ����
    IDE_TEST( sDataPlan->searchFirst( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
                                      aFlag ) != IDE_SUCCESS );

    // Data�� ������ ��� Tuple Set ����
    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );

        sDataPlan->doIt = qmnSORT::doItNext;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    // �ɼǿ� �´� �˻� ����
    IDE_TEST( sDataPlan->searchNext( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     aFlag ) != IDE_SUCCESS );

    // Data�� ������ ��� Tuple Set ����
    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        sDataPlan->doIt = qmnSORT::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::doItLast( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     TODO - A4 Grouping Set Integration
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::doItLast"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // set that no data found
    *aFlag = QMC_ROW_DATA_NONE;

    sDataPlan->doIt = qmnSORT::doItFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


void qmnSORT::setHitSearch( qcTemplate * aTemplate,
                            qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Probing (LEFT) Table�� ���� ó���� ������, Driving (RIGHT) Table�� ����
 *    ó���� �����ϱ� ���� ���ȴ�.
 *
 * Implementation :
 *    �˻� �Լ��� ���������� Hit �˻����� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->searchFirst = qmnSORT::searchFirstHit;
    sDataPlan->searchNext = qmnSORT::searchNextHit;
    sDataPlan->doIt = qmnSORT::doItFirst;
}


void qmnSORT::setNonHitSearch( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Probing (LEFT) Table�� ���� ó���� ������, Driving (RIGHT) Table�� ����
 *    ó���� �����ϱ� ���� ���ȴ�.
 *
 * Implementation :
 *    �˻� �Լ��� ���������� Non-Hit �˻����� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->searchFirst = qmnSORT::searchFirstNonHit;
    sDataPlan->searchNext = qmnSORT::searchNextNonHit;
    sDataPlan->doIt = qmnSORT::doItFirst;
}


IDE_RC
qmnSORT::setHitFlag( qcTemplate * aTemplate,
                     qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *     Full Outer Join�� ó����,
 *     Left Outer Join�� ó�� ���� �� ���ǿ� �����ϴ� Record��
 *     �˻��ϴ� ��� Hit Flag�� �����Ѵ�.
 *
 * Implementation :
 *     Temp Table�� �̿��Ͽ� ���� Row�� Hit Flag�� �����Ѵ�.
 *     �̹� Temp Table�� ���� Row�� ���縦 �˰� �־� ������ ���ڰ�
 *     �ʿ� ����.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::setHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    // ���� ���� Row�� Hit Flag�� ����
    IDE_TEST( qmcSortTemp::setHitFlag( sDataPlan->sortMgr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

idBool qmnSORT::isHitFlagged( qcTemplate * aTemplate,
                              qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description : 
 *     ���� Row�� Hit Flag�� �ִ��� �����Ѵ�.
 *
 * Implementation :
 *
 *     Temp Table�� �̿��Ͽ� ���� Row�� Hit Flag�� �ִ��� �����Ѵ�.
 *
 ***********************************************************************/

    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    return qmcSortTemp::isHitFlagged( sDataPlan->sortMgr );
}

IDE_RC
qmnSORT::storeCursor( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *     Merge Join��� ���� ��ġ�� Cursor�� �����ϱ� ���Ͽ� ����Ѵ�.
 *
 * Implementation :
 *     Temp Table�� Ŀ�� ���� ����� ����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::storeCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncSORT * sCodePlan = (qmncSORT *) aPlan;
    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    // ���� ��ġ�� Ŀ���� ������
    IDE_TEST( qmcSortTemp::storeCursor( sDataPlan->sortMgr,
                                        & sDataPlan->cursorInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::restoreCursor( qcTemplate * aTemplate,
                        qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *     Merge Join���� ����ϸ�,
 *     ����� Cursor ��ġ�� Ŀ���� ������Ų��.
 *
 * Implementation :
 *     Temp Table�� Ŀ�� ���� ����� ����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::restoreCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndSORT * sDataPlan =
        (qmndSORT *) (aTemplate->tmplate.data + aPlan->offset);

    // ���� ��ġ�� Ŀ���� ������ ��ġ�� ������Ŵ
    IDE_TEST( qmcSortTemp::restoreCursor( sDataPlan->sortMgr,
                                          & sDataPlan->cursorInfo )
              != IDE_SUCCESS );

    // �˻��� Row�� �̿��� Tuple Set ����
    IDE_TEST( setTupleSet( aTemplate, sDataPlan )
              != IDE_SUCCESS );

    // Ŀ���� ���� ���� ���� �Լ�
    sDataPlan->doIt = qmnSORT::doItNext;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::firstInit( qcTemplate * aTemplate,
                    qmncSORT   * aCodePlan,
                    qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    SORT node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndSORT * sCacheDataPlan = NULL;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    //---------------------------------
    // SORT ���� ������ �ʱ�ȭ
    //---------------------------------
    //
    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndSORT *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
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

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );

    IDE_TEST( initSortNode( aCodePlan, aDataPlan ) != IDE_SUCCESS );

    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    aDataPlan->depTuple = & aTemplate->tmplate.rows[aCodePlan->depTupleRowID];
    aDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_SORT_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SORT_INIT_DONE_TRUE;

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
qmnSORT::initMtrNode( qcTemplate * aTemplate,
                      qmncSORT   * aCodePlan,
                      qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ ���� ��带 �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt         sHeaderSize;
    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );

    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);
    //---------------------------------
    // ���� ������ ���� ������ �ʱ�ȭ
    //---------------------------------

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sHeaderSize = QMC_MEMSORT_TEMPHEADER_SIZE;

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
        sHeaderSize = QMC_DISKSORT_TEMPHEADER_SIZE;
    }

    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    // 1.  ���� Column�� ���� ���� ����
    // 2.  ���� Column�� �ʱ�ȭ
    // 3.  ���� Column�� offset�� ������
    // 4.  Row Size�� ���
    //     - Disk Temp Table�� ��� Row�� ���� Memory�� �Ҵ����.

    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                aCodePlan->baseTableCount )
              != IDE_SUCCESS );

    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  sHeaderSize )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSORT::initSortNode( qmncSORT   * aCodePlan,
                       qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Column�� ���� �� ���� Column�� ���� ��ġ�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::initSortNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    // ���� ���� Column�� ��ġ �˻�
    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        if ( ( sNode->myNode->flag & QMC_MTR_SORT_NEED_MASK )
             == QMC_MTR_SORT_NEED_TRUE )
        {
            break;
        }
    }

    aDataPlan->sortNode = sNode;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    switch ( aCodePlan->flag & QMNC_SORT_STORE_MASK )
    {
        case QMNC_SORT_STORE_NONE:
            IDE_DASSERT(0);
            break;
        case QMNC_SORT_STORE_ONLY:
            IDE_DASSERT( aDataPlan->sortNode == NULL );
            break;
        case QMNC_SORT_STORE_PRESERVE:
        case QMNC_SORT_STORE_SORTING:
            IDE_DASSERT( aDataPlan->sortNode != NULL );
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnSORT::initTempTable( qcTemplate * aTemplate,
                        qmncSORT   * aCodePlan,
                        qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Sort Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt       sFlag          = 0;
    qmndSORT * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    //-----------------------------
    // Flag ���� �ʱ�ȭ
    //-----------------------------

    sFlag = QMCD_SORT_TMP_INITIALIZE;

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sFlag &= ~QMCD_SORT_TMP_STORAGE_TYPE;
        sFlag |= QMCD_SORT_TMP_STORAGE_MEMORY;

        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_MEMORY );
    }
    else
    {
        sFlag &= ~QMCD_SORT_TMP_STORAGE_TYPE;
        sFlag |= QMCD_SORT_TMP_STORAGE_DISK;

        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_DISK );
    }

    /* PROJ-2201 Innovation in sorting and hashing(temp)
     * QMNC�� Flag�� QMND�� �̾���� */
    if( ( aCodePlan->flag & QMNC_SORT_SEARCH_MASK )
        == QMNC_SORT_SEARCH_SEQUENTIAL )
    {
        sFlag &= ~QMCD_SORT_TMP_SEARCH_MASK;
        sFlag |= QMCD_SORT_TMP_SEARCH_SEQUENTIAL;
    }
    else
    {
        sFlag &= ~QMCD_SORT_TMP_SEARCH_MASK;
        sFlag |= QMCD_SORT_TMP_SEARCH_RANGE;

        IDE_DASSERT( (aCodePlan->flag & QMNC_SORT_SEARCH_MASK )
                     == QMNC_SORT_SEARCH_RANGE );
    }

    //-----------------------------
    // Temp Table �ʱ�ȭ
    //-----------------------------

    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnSORT::initTempTable::qmxAlloc:sortMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                  (void **)&aDataPlan->sortMgr )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->sortNode,
                                     aCodePlan->storeRowCount,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndSORT *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnSORT::initTempTable::qrcAlloc:sortMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                               (void **)&aDataPlan->sortMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->sortNode,
                                         aCodePlan->storeRowCount,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->sortMgr = aDataPlan->sortMgr;
        }
        else
        {
            aDataPlan->sortMgr = sCacheDataPlan->sortMgr;
            IDE_TEST( qmcSortTemp::clearHitFlag( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSORT::checkDependency( qcTemplate * aTemplate,
                                 qmncSORT   * aCodePlan,
                                 qmndSORT   * aDataPlan,
                                 idBool     * aDependent )
{
/***********************************************************************
 *
 * Description :
 *    Dependent Tuple�� ��ȭ�� �ִ� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/
    idBool sDep = ID_FALSE;

    if ( aDataPlan->depValue != aDataPlan->depTuple->modify )
    {
        if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
        {
            sDep = ID_TRUE;
        }
        else
        {
            aDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
            if ( ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_STORED_MASK )
                   == QMX_RESULT_CACHE_STORED_TRUE ) &&
                ( aDataPlan->depValue == QMN_PLAN_DEFAULT_DEPENDENCY_VALUE ) )
            {
                sDep = ID_FALSE;
            }
            else
            {
                sDep = ID_TRUE;
            }
        }
    }
    else
    {
        sDep = ID_FALSE;
    }

    *aDependent = sDep;

    return IDE_SUCCESS;
}

IDE_RC
qmnSORT::storeAndSort( qcTemplate * aTemplate,
                       qmncSORT   * aCodePlan,
                       qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� �� ���� ����
 *
 * Implementation :
 *    Child�� �ݺ������� �����Ͽ� �̸� �����ϰ�,
 *    ���������� Sorting�� �����Ѵ�.
 *
 ***********************************************************************/

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    //------------------------------
    // Child Record�� ����
    //------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( addOneRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );

    }

    //------------------------------
    // ���� ����
    //------------------------------

    //  ������ ���� �ɼǿ� ���� ���� ���� ����
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_DISK )
    {
        // disk�� ��� SORTING or PRESEVED_ORDER �� ��� ������ ��
        if( ( aCodePlan->flag & QMNC_SORT_STORE_MASK )
            == QMNC_SORT_STORE_SORTING ||
            ( aCodePlan->flag & QMNC_SORT_STORE_MASK )
            == QMNC_SORT_STORE_PRESERVE )
        {
            IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // memory�� ��� SORTING �϶��� ������ ��
        if( ( aCodePlan->flag & QMNC_SORT_STORE_MASK )
            == QMNC_SORT_STORE_SORTING )
        {
            IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
        }
    }

    // PROJ-2462 Result Cache
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        *aDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_STORED_MASK;
        *aDataPlan->resultData.flag |= QMX_RESULT_CACHE_STORED_TRUE;
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
qmnSORT::addOneRow( qcTemplate * aTemplate,
                    qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� �ϳ��� Record�� �����Ͽ� �����Ѵ�.
 *
 * Implementation :
 *    1. ���� �Ҵ� : Temp Table�� �̿��Ͽ� ���� �Ҵ��� ������,
 *                   Memory Temp Table�� ��쿡�� ������ ������
 *                   �Ҵ��� �ش�.
 *    2. ���� Row�� ����
 *    3. Row�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::addOneRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDU_FIT_POINT( "qmnSORT::addOneRow::alloc::myTupleRow",
                    idERR_ABORT_InsufficientMemory );

    // ������ �Ҵ�
    IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                  & aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS);

    // ���� Row�� ����
    IDE_TEST( setMtrRow( aTemplate,
                         aDataPlan ) != IDE_SUCCESS );

    // Row�� ����
    IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                   aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnSORT::setMtrRow( qcTemplate * aTemplate,
                    qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Row�� �����Ѵ�.
 *
 * Implementation :
 *     ���� Column�� ��ȸ�ϸ�, ���� Row�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        if ( sNode->func.setMtr == qmc::setMtrByCopy )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
    }

    // BUG-46279 Disk temp ��� �� grouping �������� ������ subquery�� �����ϴ� ���
    //           ��� �� ������ �߻��մϴ�.  
    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        if ( sNode->func.setMtr != qmc::setMtrByCopy )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnSORT::setTupleSet( qcTemplate * aTemplate,
                      qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate, sNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    aDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::setSearchFunction( qmncSORT   * aCodePlan,
                            qmndSORT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     SORT ����� �˻� �ɼǿ� �����ϴ� �˻� �Լ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::setSearchFunction"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    switch ( aCodePlan->flag & QMNC_SORT_SEARCH_MASK )
    {
        case QMNC_SORT_SEARCH_SEQUENTIAL:
            aDataPlan->searchFirst = qmnSORT::searchFirstSequence;
            aDataPlan->searchNext = qmnSORT::searchNextSequence;
            break;
        case QMNC_SORT_SEARCH_RANGE:
            aDataPlan->searchFirst = qmnSORT::searchFirstRangeSearch;
            aDataPlan->searchNext = qmnSORT::searchNextRangeSearch;

            // ���ռ� �˻�
            IDE_DASSERT( aCodePlan->range != NULL );
            IDE_DASSERT( aDataPlan->sortNode != NULL );

            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchDefault( qcTemplate * /* aTemplate */,
                        qmncSORT   * /* aCodePlan */,
                        qmndSORT   * /* aDataPlan */,
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

#define IDE_FN "qmnSORT::searchDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT(0);

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchFirstSequence( qcTemplate * /* aTemplate */,
                              qmncSORT   * /* aCodePlan */,
                              qmndSORT   * aDataPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ù��° ���� �˻��� ����
 *
 * Implementation :
 *     Temp Table�� �̿��Ͽ� �˻��Ѵ�.
 *
 *     - Memory ������ ����
 *         Disk Temp Table�� ����ϴ� ��� �ش� Tuple�� �޸� ������
 *         ����� �� �ִ�.  �̸� ���� �� ���� BackUp �� �ʿ��ϴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchFirstSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ù��° ���� �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstSequence( aDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchNextSequence( qcTemplate * /* aTemplate */,
                             qmncSORT   * /* aCodePlan */,
                             qmndSORT   * aDataPlan,
                             qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� ���� �˻��� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchNextSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ���� ���� �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->sortMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchFirstRangeSearch( qcTemplate * /* aTemplate */,
                                 qmncSORT   * aCodePlan,
                                 qmndSORT   * aDataPlan,
                                 qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ù ��° Range �˻�
 *
 * Implementation :
 *     Range Predicate(DNF�� ����)�� Temp Table�� �ѱ��,
 *     �̸� �̿��Ͽ� Temp Table�� Range �˻��� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchFirstRangeSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ù��° Range �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstRange( aDataPlan->sortMgr,
                                          aCodePlan->range,
                                          & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;


    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchNextRangeSearch( qcTemplate * /* aTemplate */,
                                qmncSORT   * /* aCodePlan */,
                                qmndSORT   * aDataPlan,
                                qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻�
 *
 * Implementation :
 *    ù��° Range�˻� �� ���� Range Predicate�� �̿��Ͽ�
 *    ���� Row�� �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchNextRangeSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ���� Range �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMgr,
                                         & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnSORT::searchFirstHit( qcTemplate * /* aTemplate */,
                                qmncSORT   * /* aCodePlan */,
                                qmndSORT   * aDataPlan,
                                qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Hit �˻�
 *
 * Implementation :
 *    Inverse Sort Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Inverse Sort Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 *
 ***********************************************************************/

    void * sOrgRow;
    void * sSearchRow;

    // ù��° Hit �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstHit( aDataPlan->sortMgr,
                                        & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        IDE_TEST( qmcSortTemp::clearHitFlag( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnSORT::searchNextHit( qcTemplate * /* aTemplate */,
                               qmncSORT   * /* aCodePlan */,
                               qmndSORT   * aDataPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� Hit �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    void * sOrgRow;
    void * sSearchRow;

    // ���� Hit �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextHit( aDataPlan->sortMgr,
                                       & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        IDE_TEST( qmcSortTemp::clearHitFlag( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSORT::searchFirstNonHit( qcTemplate * /* aTemplate */,
                            qmncSORT   * /* aCodePlan */,
                            qmndSORT   * aDataPlan,
                            qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Non-Hit �˻�
 *
 * Implementation :
 *    Full Outer Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Non-Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Full Outer Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchFirstNonHit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ù��° Non-Hit �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstNonHit( aDataPlan->sortMgr,
                                           & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        IDE_TEST( qmcSortTemp::clearHitFlag( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSORT::searchNextNonHit( qcTemplate * /* aTemplate */,
                           qmncSORT   * /* aCodePlan */,
                           qmndSORT   * aDataPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� Non-Hit �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnSORT::searchNextNonHit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    // ���� Non-Hit �˻�
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextNonHit( aDataPlan->sortMgr,
                                          & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ����
    if ( sSearchRow == NULL )
    {
        IDE_TEST( qmcSortTemp::clearHitFlag( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
