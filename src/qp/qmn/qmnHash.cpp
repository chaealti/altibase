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
 * $Id: qmnHash.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     HASH(HASH) Node
 *
 *     ������ �𵨿��� hashing ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Hash Join
 *         - Hash-based Left Outer Join
 *         - Hash-based Full Outer Join
 *         - Store And Search
 *
 *     HASH ���� Two Pass Hash Join� ���� ��,
 *     �������� Temp Table�� ������ �� �ִ�.
 *     ����, ��� ���� �� �˻� �� �̿� ���� ����� ����Ͽ��� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmoUtil.h>
#include <qmnHash.h>
#include <qcg.h>
#include <qmxResultCache.h>

IDE_RC
qmnHASH::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    HASH ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    UInt   i;
    idBool sDependency;

    sDataPlan->doIt = qmnHASH::doItDefault;

    sDataPlan->searchFirst = qmnHASH::searchDefault;
    sDataPlan->searchNext = qmnHASH::searchDefault;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_HASH_INIT_DONE_MASK)
         == QMND_HASH_INIT_DONE_FALSE )
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

        for ( i = 0; i < sCodePlan->tempTableCnt; i++ )
        {
            IDE_TEST( qmcHashTemp::clear( & sDataPlan->hashMgr[i] )
                      != IDE_SUCCESS );
        }

        // To Fix PR-8024
        // Mask FALSE ���� ����
        // STORE AND SEARCH�� ���� �ʱ�ȭ
        sDataPlan->isNullStore = ID_FALSE;
        sDataPlan->mtrTotalCnt = 0;

        //----------------------------------------
        // Child�� �ݺ� �����Ͽ� Temp Table�� ����
        //----------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        IDE_TEST( storeAndHashing( aTemplate,
                                   sCodePlan,
                                   sDataPlan ) != IDE_SUCCESS);

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

    switch ( sCodePlan->flag & QMNC_HASH_SEARCH_MASK )
    {
        case QMNC_HASH_SEARCH_SEQUENTIAL:
            sDataPlan->doIt = qmnHASH::doItFirst;
            break;
        case QMNC_HASH_SEARCH_RANGE:
            sDataPlan->doIt = qmnHASH::doItFirst;
            break;
        case QMNC_HASH_SEARCH_STORE_SEARCH:
            sDataPlan->doIt = qmnHASH::doItFirstStoreSearch;

            // ���ռ� �˻�
            IDE_DASSERT( (sCodePlan->flag & QMNC_HASH_STORE_MASK)
                         == QMNC_HASH_STORE_DISTINCT );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnHASH::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    HASH �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnHASH::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    HASH ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_HASH_INIT_DONE_MASK)
         == QMND_HASH_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }

    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    IDE_TEST(
        qmcHashTemp::getNullRow( & sDataPlan->hashMgr[QMN_HASH_PRIMARY_ID],
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
qmnHASH::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    HASH ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH*) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndHASH * sCacheDataPlan = NULL;
    idBool     sIsInit        = ID_FALSE;
    SLong sRecordCnt;
    SLong sTotalRecordCnt;
    ULong sPageCnt;
    ULong sTotalPageCnt;
    UInt  sBucketCnt;

    ULong  i;

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

        if ( (*sDataPlan->flag & QMND_HASH_INIT_DONE_MASK)
             == QMND_HASH_INIT_DONE_TRUE )
        {
            // ���� ���� ȹ��

            sTotalRecordCnt = 0;
            sTotalPageCnt = 0;
            sBucketCnt = 0;
            sIsInit = ID_TRUE;
            for ( i = 0; i < sCodePlan->tempTableCnt; i++ )
            {
                IDE_TEST( qmcHashTemp::getDisplayInfo( & sDataPlan->hashMgr[i],
                                                       & sPageCnt,
                                                       & sRecordCnt,
                                                       & sBucketCnt )
                          != IDE_SUCCESS );
                sTotalRecordCnt += sRecordCnt;
                sTotalPageCnt += sPageCnt;
            }

            if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY )
            {
                // To Fix PR-13136
                // Temp Table�� ������ �� �� �־�� ��.
                if ( sCodePlan->tempTableCnt == 1 )
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "BUCKET_COUNT: %"ID_UINT32_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sTotalRecordCnt,
                            sBucketCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "BUCKET_COUNT: %"ID_UINT32_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sTotalRecordCnt,
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
                            "HASH ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "BUCKET_COUNT: %"ID_UINT32_FMT
                            "[%"ID_UINT32_FMT"], "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sTotalRecordCnt,
                            sBucketCnt,
                            sCodePlan->tempTableCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "BUCKET_COUNT: %"ID_UINT32_FMT
                            "[%"ID_UINT32_FMT"], "
                            "ACCESS: %"ID_UINT32_FMT,
                            sTotalRecordCnt,
                            sBucketCnt,
                            sCodePlan->tempTableCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
            }
            else
            {
                // To Fix PR-13136
                // Temp Table�� ������ �� �� �־�� ��.
                if ( sCodePlan->tempTableCnt == 1 )
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: %"ID_UINT64_FMT", "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sTotalRecordCnt,
                            sTotalPageCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE, DISK_PAGE_COUNT ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: BLOCKED, "
                            "ACCESS: %"ID_UINT32_FMT,
                            sTotalRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
                else
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: %"ID_INT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: %"ID_UINT64_FMT
                            "[%"ID_UINT32_FMT"], "
                            "ACCESS: %"ID_UINT32_FMT,
                            sDataPlan->mtrRowSize,
                            sTotalRecordCnt,
                            sTotalPageCnt,
                            sCodePlan->tempTableCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE, DISK_PAGE_COUNT ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "HASH ( ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: BLOCKED, "
                            "ACCESS: %"ID_UINT32_FMT,
                            sTotalRecordCnt,
                            sDataPlan->plan.myTuple->modify );
                    }
                }
            }
        }
        else
        {
            // To Fix PR-13136
            // Temp Table�� ������ �� �� �־�� ��.
            if ( sCodePlan->tempTableCnt == 1 )
            {
                iduVarStringAppendFormat( aString,
                                          "HASH ( ITEM_SIZE: 0, ITEM_COUNT: 0, "
                                          "BUCKET_COUNT: %"ID_UINT32_FMT", "
                                          "ACCESS: 0",
                                          sCodePlan->bucketCnt );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "HASH ( ITEM_SIZE: 0, ITEM_COUNT: 0, "
                                          "BUCKET_COUNT: %"ID_UINT32_FMT
                                          "[%"ID_UINT32_FMT"], "
                                          "ACCESS: 0",
                                          sCodePlan->bucketCnt,
                                          sCodePlan->tempTableCnt );
            }
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        if ( sCodePlan->tempTableCnt == 1 )
        {
            iduVarStringAppendFormat( aString,
                                      "HASH ( ITEM_SIZE: ??, ITEM_COUNT: ??, "
                                      "BUCKET_COUNT: %"ID_UINT32_FMT", "
                                      "ACCESS: ??",
                                      sCodePlan->bucketCnt );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "HASH ( ITEM_SIZE: ??, ITEM_COUNT: ??, "
                                      "BUCKET_COUNT: %"ID_UINT32_FMT
                                      "[%"ID_UINT32_FMT"], "
                                      "ACCESS: ??",
                                      sCodePlan->bucketCnt,
                                      sCodePlan->tempTableCnt );
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
        // PROJ-2462 ResultCache
        if ( ( sCodePlan->componentInfo != NULL ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
               == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
               == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
        {
            sCacheDataPlan = (qmndHASH *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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
        if (sCodePlan->filter != NULL)
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
                                              sCodePlan->filter )
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
qmnHASH::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnHASH::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° ���� �Լ�
 *
 * Implementation :
 *    �ش� �˻� �Լ��� �����ϰ�, �ش� ����� Tuple Set�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    IDE_TEST( sDataPlan->searchFirst( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
                                      aFlag ) != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );

        sDataPlan->doIt = qmnHASH::doItNext;
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
qmnHASH::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻� �Լ�
 *
 * Implementation :
 *    �ش� �˻� �Լ��� �����ϰ�, �ش� ����� Tuple Set�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->searchNext( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     aFlag ) != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        sDataPlan->doIt = qmnHASH::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::doItFirstStoreSearch( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ù��° ���� �Լ�
 *
 * Implementation :
 *     0. ���� �˻��� ���� ���ϴ� Row�� �˻��Ѵ�.
 *         - Row�� ���� ����, ������� NULL ���ΰ� �Ǵܵȴ�.
 *     1. Row�� �����Ѵٸ� �״�� ����
 *     2. Row�� �������� �ʴ´ٸ� ������ ���� ��Ȳ�� ���� ��ó�Ѵ�.
 *         - 2.1 ���� Row�� ���ٸ�, ������ ������ ����
 *         - 2.2 ���� Row�� �ִٸ�,
 *             - 2.2.1 ����� NULL�̶��, NULL Padding�� �� ������ ����
 *             - 2.2.2 ���� Row�� NULL�� �����Ѵٸ�,
 *                     NULL Padding�� �� ������ ����
 *             - 2.2.3 �� ���� ���, ������ ����
 *
 *     �̿� ���� ó���� ������ ���� ���׿� ���� ó���� �����̴�.
 *
 *     Ex     1)   3 IN (1, 2, 3)      => TRUE
 *     Ex   2.1)   3 IN {}, NULL IN {} => FALSE
 *     Ex 2.2.1)   NULL IN {1,2}       => UNKNOWN
 *     Ex 2.2.2)   3 IN {1,2,NULL}     => UNKNOWN
 *     Ex 2.2.3)   3 IN {1,2}          => FALSE
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::doItFirstStoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // 0. ���� �˻�
    IDE_TEST( sDataPlan->searchFirst( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
                                      aFlag ) != IDE_SUCCESS );
    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // 1. Row�� �����ϴ� ���

        IDE_TEST( setTupleSet( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        sDataPlan->doIt = qmnHASH::doItNextStoreSearch;
    }
    else
    {
        if ( sDataPlan->mtrTotalCnt == 0 )
        {
            // 2.1 ���� Data�� ���� ���
            // Nothing To Do
        }
        else
        {
            // 2.2 ���� Data�� �ִ� ���

            if ( ( (*sDataPlan->flag & QMND_HASH_CONST_NULL_MASK)
                   == QMND_HASH_CONST_NULL_TRUE )
                 ||
                 ( sDataPlan->isNullStore == ID_TRUE ) )
            {
                // 2.2.1, 2.2.2 : ����� NULL�̰ų� ���� Row�� NULL�� ����

                IDE_TEST( qmnHASH::padNull( aTemplate, aPlan )
                          != IDE_SUCCESS );

                *aFlag &= ~QMC_ROW_DATA_MASK;
                *aFlag |= QMC_ROW_DATA_EXIST;

                sDataPlan->doIt = qmnHASH::doItNextStoreSearch;
            }
            else
            {
                // 2.2.3
                // Nothing To Do
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::doItNextStoreSearch( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ���� ���� �˻�
 *
 * Implementation :
 *     ������ ������ ����
 *     Store And Search�� ��� �� ���� �˻����� �� ����� ������ ��
 *     ������, �� ��° ���࿡�� �� ����� Ȯ���� �� �ִ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::doItNextStoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    // set that no data found
    *aFlag = QMC_ROW_DATA_NONE;

    sDataPlan->doIt = qmnHASH::doItFirstStoreSearch;

    return IDE_SUCCESS;

#undef IDE_FN
}

void qmnHASH::setHitSearch( qcTemplate * aTemplate,
                            qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Inverse Hash Join���� ���Ǹ�,
 *    Probing Left Table�� ���� ó���� ������, Driving Right Table�� ����
 *    ó���� �����ϱ� ���� ���ȴ�.
 *
 * Implementation :
 *    �˻� �Լ��� ���������� Hit �˻����� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->searchFirst = qmnHASH::searchFirstHit;
    sDataPlan->searchNext = qmnHASH::searchNextHit;
    sDataPlan->doIt = qmnHASH::doItFirst;
}

void qmnHASH::setNonHitSearch( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Full Outer Join���� ���Ǹ�,
 *    Left Outer Join�� ���� ó���� ������, Right Outer Join�� ����
 *    ó���� �����ϱ� ���� ���ȴ�.
 *
 * Implementation :
 *    �˻� �Լ��� ���������� Non-Hit �˻����� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::setNonHitSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->searchFirst = qmnHASH::searchFirstNonHit;
    sDataPlan->searchNext = qmnHASH::searchNextNonHit;
    sDataPlan->doIt = qmnHASH::doItFirst;

#undef IDE_FN
}

IDE_RC
qmnHASH::setHitFlag( qcTemplate * aTemplate,
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
 *
 *     Temp Table�� �̿��Ͽ� ���� Row�� Hit Flag�� �����Ѵ�.
 *     �̹� Temp Table�� ���� Row�� ���縦 �˰� �־� ������ ���ڰ�
 *     �ʿ� ����.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::setHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncHASH * sCodePlan = (qmncHASH *) aPlan;
    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( qmcHashTemp::setHitFlag( & sDataPlan->
                                       hashMgr[sDataPlan->currTempID] )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

idBool qmnHASH::isHitFlagged( qcTemplate * aTemplate,
                              qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2339
 *     ���� Row�� Hit Flag�� �ִ��� �����Ѵ�.
 *
 * Implementation :
 *
 *     Temp Table�� �̿��Ͽ� ���� Row�� Hit Flag�� �ִ��� �����Ѵ�.
 *
 ***********************************************************************/

    qmndHASH * sDataPlan =
        (qmndHASH *) (aTemplate->tmplate.data + aPlan->offset);

    return qmcHashTemp::isHitFlagged( & sDataPlan->
                                      hashMgr[sDataPlan->currTempID] );
}

IDE_RC
qmnHASH::firstInit( qcTemplate * aTemplate,
                    qmncHASH   * aCodePlan,
                    qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    HASH node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndHASH * sCacheDataPlan = NULL;

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndHASH *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
        aDataPlan->resultData.flag      = &aTemplate->resultCache.dataFlag[aCodePlan->planID];

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

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    if ( (aCodePlan->flag & QMNC_HASH_NULL_CHECK_MASK)
         == QMNC_HASH_NULL_CHECK_TRUE )
    {
        IDE_DASSERT( (aCodePlan->flag & QMNC_HASH_SEARCH_MASK)
                     == QMNC_HASH_SEARCH_STORE_SEARCH );
        IDE_DASSERT( (aCodePlan->flag & QMNC_HASH_STORE_MASK)
                     == QMNC_HASH_STORE_DISTINCT );
    }

    //---------------------------------
    // HASH ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );

    IDE_TEST( initHashNode( aCodePlan, aDataPlan ) != IDE_SUCCESS );

    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    aDataPlan->depTuple =
        & aTemplate->tmplate.rows[aCodePlan->depTupleRowID];
    aDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    aDataPlan->mtrTotalCnt = 0;
    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );


    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_HASH_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_HASH_INIT_DONE_TRUE;

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

#undef IDE_FN
}

IDE_RC
qmnHASH::initMtrNode( qcTemplate * aTemplate,
                      qmncHASH   * aCodePlan,
                      qmndHASH   * aDataPlan )
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

    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                aCodePlan->baseTableCount ) != IDE_SUCCESS );

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
qmnHASH::initHashNode( qmncHASH    * aCodePlan,
                       qmndHASH    * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Hashing Column�� ���� ��ġ�� ã��
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::initHashNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        if ( ( sNode->myNode->flag & QMC_MTR_HASH_NEED_MASK )
             == QMC_MTR_HASH_NEED_TRUE )
        {
            break;
        }
    }

    aDataPlan->hashNode = sNode;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aDataPlan->hashNode != NULL );

    if ( (aCodePlan->flag & QMNC_HASH_NULL_CHECK_MASK)
         == QMNC_HASH_NULL_CHECK_TRUE )
    {
        // STORE AND SEARCH�� ��� NULL CHECK�� �ʿ��� ����
        // �� �÷��� ���� Hashing�� ��츸 �����ϴ�.
        IDE_DASSERT( aDataPlan->hashNode->next == NULL );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnHASH::initTempTable( qcTemplate * aTemplate,
                        qmncHASH   * aCodePlan,
                        qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *     Hash Temp Table�� ������ŭ Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/
    UInt        i;
    UInt        sFlag;
    qmndHASH  * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    IDE_DASSERT( aCodePlan->tempTableCnt > 0 );
    IDE_DASSERT( QMN_HASH_PRIMARY_ID == 0 );

    //-----------------------------
    // Flag ���� �ʱ�ȭ
    //-----------------------------

    sFlag = QMCD_HASH_TMP_INITIALIZE;

    switch ( aCodePlan->flag & QMNC_HASH_STORE_MASK )
    {
        case QMNC_HASH_STORE_NONE:
            IDE_DASSERT(0);
            break;
        case QMNC_HASH_STORE_HASHING:
            sFlag &= ~QMCD_HASH_TMP_DISTINCT_MASK;
            sFlag |= QMCD_HASH_TMP_DISTINCT_FALSE;
            break;
        case QMNC_HASH_STORE_DISTINCT:
            sFlag &= ~QMCD_HASH_TMP_DISTINCT_MASK;
            sFlag |= QMCD_HASH_TMP_DISTINCT_TRUE;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

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
    // Bucket-based Hash Temp Table�� ������� �ʰ� Non-DISTINCT Hashing�� ���,
    // Partitioned Hashing�� �ϵ��� �Ѵ�.
    if ( ( aTemplate->memHashTempPartDisable == ID_FALSE ) &&
         ( ( sFlag & QMCD_HASH_TMP_DISTINCT_MASK ) == QMCD_HASH_TMP_DISTINCT_FALSE ) )
    {
        sFlag &= ~QMCD_HASH_TMP_HASHING_TYPE;
        sFlag |= QMCD_HASH_TMP_HASHING_PARTITIONED;
    }
    else
    {
        sFlag &= ~QMCD_HASH_TMP_HASHING_TYPE;
        sFlag |= QMCD_HASH_TMP_HASHING_BUCKET;
    }

    //-----------------------------
    // Temp Table �ʱ�ȭ
    //-----------------------------
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnHASH::initTempTable::qmxAlloc:hashMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmcdHashTemp) *
                                                  aCodePlan->tempTableCnt,
                                                  (void**) & aDataPlan->hashMgr )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->hashMgr == NULL, err_mem_alloc );

        // Primary Temp Table�� �ʱ�ȭ

        sFlag &= ~QMCD_HASH_TMP_PRIMARY_MASK;
        sFlag |= QMCD_HASH_TMP_PRIMARY_TRUE;

        IDE_TEST( qmcHashTemp::init( & aDataPlan->hashMgr[QMN_HASH_PRIMARY_ID],
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->hashNode,
                                     NULL,  // Aggregation Column����
                                     aCodePlan->bucketCnt,
                                     sFlag )
                  != IDE_SUCCESS );

        // Secondary Temp Table�� �ʱ�ȭ

        sFlag &= ~QMCD_HASH_TMP_PRIMARY_MASK;
        sFlag |= QMCD_HASH_TMP_PRIMARY_FALSE;

        for ( i = 1; i < aCodePlan->tempTableCnt; i++ )
        {
            IDE_TEST( qmcHashTemp::init( & aDataPlan->hashMgr[i],
                                         aTemplate,
                                         ID_UINT_MAX,
                                         aDataPlan->mtrNode,
                                         aDataPlan->hashNode,
                                         NULL,  // Aggregation Column����
                                         aCodePlan->bucketCnt,
                                         sFlag )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndHASH *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnHASH::initTempTable::qrcAlloc:hashMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF(qmcdHashTemp) *
                                                               aCodePlan->tempTableCnt,
                                                               (void**) & aDataPlan->hashMgr )
                      != IDE_SUCCESS );
            IDE_TEST_RAISE( aDataPlan->hashMgr == NULL, err_mem_alloc );

            // Primary Temp Table�� �ʱ�ȭ
            sFlag &= ~QMCD_HASH_TMP_PRIMARY_MASK;
            sFlag |= QMCD_HASH_TMP_PRIMARY_TRUE;

            IDE_TEST( qmcHashTemp::init( & aDataPlan->hashMgr[QMN_HASH_PRIMARY_ID],
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->hashNode,
                                         NULL,  // Aggregation Column����
                                         aCodePlan->bucketCnt,
                                         sFlag )
                      != IDE_SUCCESS );

            // Secondary Temp Table�� �ʱ�ȭ
            sFlag &= ~QMCD_HASH_TMP_PRIMARY_MASK;
            sFlag |= QMCD_HASH_TMP_PRIMARY_FALSE;

            for ( i = 1; i < aCodePlan->tempTableCnt; i++ )
            {
                IDE_TEST( qmcHashTemp::init( & aDataPlan->hashMgr[i],
                                             aTemplate,
                                             sCacheDataPlan->resultData.memoryIdx,
                                             aDataPlan->mtrNode,
                                             aDataPlan->hashNode,
                                             NULL,  // Aggregation Column����
                                             aCodePlan->bucketCnt,
                                             sFlag )
                          != IDE_SUCCESS );
            }
            sCacheDataPlan->hashMgr = aDataPlan->hashMgr;
        }
        else
        {
            aDataPlan->hashMgr = sCacheDataPlan->hashMgr;
            aDataPlan->mtrTotalCnt = sCacheDataPlan->mtrTotalCnt;
            aDataPlan->isNullStore = sCacheDataPlan->isNullStore;
            for ( i = 0; i < aCodePlan->tempTableCnt; i++ )
            {
                IDE_TEST( qmcHashTemp::clearHitFlag( & aDataPlan->hashMgr[i] )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnHASH::checkDependency( qcTemplate * aTemplate,
                                 qmncHASH   * aCodePlan,
                                 qmndHASH   * aDataPlan,
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
qmnHASH::storeAndHashing( qcTemplate * aTemplate,
                          qmncHASH   * aCodePlan,
                          qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child�� �ݺ������� �����Ͽ� Hash Temp Table�� �����Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ����Ͽ� �����Ͽ��� �Ѵ�.
 *        - ������ Temp Table�� �����Ѵ�.
 *        - Distinct Option�� ��� ���� ���� ���θ� �Ǵ��Ͽ�
 *          Memory �Ҵ� ���θ� �����Ͽ��� �Ѵ�.
 *        - NULL ���θ� �Ǵ��Ͽ��� �ϴ� ��� NULL���θ� �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::storeAndHashing"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag    sFlag = QMC_ROW_INITIALIZE;
    UInt          sHashKey;
    idBool        sInserted;
    UInt          sTempID;
    qmndHASH    * sCacheDataPlan = NULL;

    sInserted = ID_TRUE;

    //------------------------------
    // Child Record�� ����
    //------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //---------------------------------------
        // 1.  ���� Row�� ���� ���� �Ҵ�
        // 2.  ���� Row�� ����
        // 3.  NULL ������ �Ǵ�
        // 4.  Temp Table ����
        // 5.  ���� Row�� ����
        // 6.  Child ����
        //---------------------------------------

        // 1. ���� Row�� ���� ���� �Ҵ�
        if ( sInserted == ID_TRUE )
        {
            IDE_TEST(
                qmcHashTemp::alloc( & aDataPlan->hashMgr[QMN_HASH_PRIMARY_ID],
                                    & aDataPlan->plan.myTuple->row )
                != IDE_SUCCESS );
        }
        else
        {
            // ������ ������ ���� �̹� �Ҵ���� ������ ����Ѵ�.
            // ����, ������ ������ �Ҵ� ���� �ʿ䰡 ����.
        }

        // 2.  ���� Row�� ����
        IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        // 3.  NULL ������ �Ǵ�
        IDE_TEST( checkNullExist( aCodePlan, aDataPlan )
                  != IDE_SUCCESS );

        // 4.  Temp Table ����
        if ( aCodePlan->tempTableCnt > 1 )
        {
            IDE_TEST( getMtrHashKey( aDataPlan,
                                     & sHashKey )
                      != IDE_SUCCESS );
            sTempID = sHashKey % aCodePlan->tempTableCnt;
        }
        else
        {
            sTempID = QMN_HASH_PRIMARY_ID;
        }

        // 5.  ���� Row�� ����
        if ( (aCodePlan->flag & QMNC_HASH_STORE_MASK)
             == QMNC_HASH_STORE_DISTINCT )
        {
            IDE_TEST( qmcHashTemp::addDistRow( & aDataPlan->hashMgr[sTempID],
                                               & aDataPlan->plan.myTuple->row,
                                               & sInserted )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcHashTemp::addRow( & aDataPlan->hashMgr[sTempID],
                                           aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
            sInserted = ID_TRUE;
        }

        if( sInserted == ID_TRUE )
        {
            aDataPlan->mtrTotalCnt++;
        }
        else
        {
            // Nothing To Do
        }

        // 6.  Child ����
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        *aDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_STORED_MASK;
        *aDataPlan->resultData.flag |= QMX_RESULT_CACHE_STORED_TRUE;

        sCacheDataPlan = (qmndHASH *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->mtrTotalCnt = aDataPlan->mtrTotalCnt;
        sCacheDataPlan->isNullStore = aDataPlan->isNullStore;
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
qmnHASH::setMtrRow( qcTemplate * aTemplate,
                    qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Row�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setMtr( aTemplate,
                                      sNode,
                                      aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::checkNullExist( qmncHASH   * aCodePlan,
                         qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Row�� Hashing ��� Column�� NULL������ �Ǵ��Ѵ�.
 *     One Column�� ���� Store And Search�� ���ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::checkNullExist"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sIsNull;

    if ( (aCodePlan->flag & QMNC_HASH_NULL_CHECK_MASK)
         == QMNC_HASH_NULL_CHECK_TRUE )
    {
        // �ϳ��� Column���� Hashing ������� ����ȴ�.
        // �̴� �̹� ���ռ� �˻縦 ���� �����
        sIsNull =
            aDataPlan->hashNode->func.isNull( aDataPlan->hashNode,
                                              aDataPlan->plan.myTuple->row );
        if ( sIsNull == ID_TRUE )
        {
            aDataPlan->isNullStore = ID_TRUE;
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnHASH::getMtrHashKey( qmndHASH   * aDataPlan,
                        UInt       * aHashKey )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� Hash Key�� ȹ��
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::getMtrHashKey"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt sValue = mtc::hashInitialValue;
    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->hashNode; sNode != NULL; sNode = sNode->next )
    {
        sValue = sNode->func.getHash( sValue,
                                      sNode,
                                      aDataPlan->plan.myTuple->row );
    }

    *aHashKey = sValue;

    return IDE_SUCCESS;

#undef IDE_FN
}


IDE_RC
qmnHASH::getConstHashKey( qcTemplate * aTemplate,
                          qmncHASH   * aCodePlan,
                          qmndHASH   * aDataPlan,
                          UInt       * aHashKey )
{
/***********************************************************************
 *
 * Description :
 *    ��� ������ Hash Key�� ȹ��
 *
 * Implementation :
 *    ��� ������ �����ϰ�, Stack�� ���� ������ �̿��Ͽ�
 *    Hash Key���� ��´�.
 *    �ΰ������� Store And Search�� ���� ��� ������ ����
 *    NULL������ ���� �Ǵܵ� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::getConstHashKey"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool     sIsNull;
    qtcNode  * sNode;
    mtcStack * sStack;

    UInt sValue = mtc::hashInitialValue;

    sStack = aTemplate->tmplate.stack;
    for ( sNode = aCodePlan->filterConst;
          sNode != NULL;
          sNode = (qtcNode*) sNode->node.next )
    {
        IDE_TEST( qtc::calculate( sNode, aTemplate )
                  != IDE_SUCCESS );
        sValue = sStack->column->module->hash( sValue,
                                               sStack->column,
                                               sStack->value );

        // NULL �� ������ �Ǵ�
        sIsNull = sStack->column->module->isNull( sStack->column,
                                                  sStack->value );
        if ( sIsNull == ID_TRUE )
        {
            *aDataPlan->flag &= ~QMND_HASH_CONST_NULL_MASK;
            *aDataPlan->flag |= QMND_HASH_CONST_NULL_TRUE;
        }
        else
        {
            *aDataPlan->flag &= ~QMND_HASH_CONST_NULL_MASK;
            *aDataPlan->flag |= QMND_HASH_CONST_NULL_FALSE;
        }
    }

    *aHashKey = sValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::setTupleSet( qcTemplate * aTemplate,
                      qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     �˻��� ���� Row�� �̿��Ͽ� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
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
qmnHASH::setSearchFunction( qmncHASH   * aCodePlan,
                            qmndHASH   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     �˻� �ɼǿ� ���� �˻� �Լ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::setSearchFunction"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    switch ( aCodePlan->flag & QMNC_HASH_SEARCH_MASK )
    {
        case QMNC_HASH_SEARCH_SEQUENTIAL:
            aDataPlan->searchFirst = qmnHASH::searchFirstSequence;
            aDataPlan->searchNext = qmnHASH::searchNextSequence;

            IDE_DASSERT( aCodePlan->filter == NULL );
            break;
        case QMNC_HASH_SEARCH_RANGE:
            aDataPlan->searchFirst = qmnHASH::searchFirstFilter;
            aDataPlan->searchNext = qmnHASH::searchNextFilter;

            IDE_DASSERT( aCodePlan->filter != NULL );
            IDE_DASSERT( aCodePlan->filterConst != NULL );
            break;
        case QMNC_HASH_SEARCH_STORE_SEARCH:
            aDataPlan->searchFirst = qmnHASH::searchFirstFilter;
            aDataPlan->searchNext = qmnHASH::searchNextFilter;

            IDE_DASSERT( aCodePlan->filter != NULL );
            IDE_DASSERT( aCodePlan->filterConst != NULL );
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnHASH::searchDefault( qcTemplate * /* aTemplate */,
                        qmncHASH   * /* aCodePlan */,
                        qmndHASH   * /* aDataPlan */,
                        qmcRowFlag * /* aFlag */ )
{
/***********************************************************************
 *
 * Description :
 *     �� �Լ��� ȣ��Ǿ�� �ȵ�
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::searchDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::searchFirstSequence( qcTemplate * /* aTemplate */,
                              qmncHASH   * aCodePlan,
                              qmndHASH   * aDataPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° ���� �˻�
 *
 * Implementation :
 *    Temp Table�� ���� ���� ��츦 ����Ͽ� �����ϸ�,
 *    Row Pointer�� ���� �ʵ��� �Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::searchFirstSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;
    qmcRowFlag sFlag;

    sFlag = QMC_ROW_INITIALIZE;
    aDataPlan->currTempID = 0;

    // To Fix PR-8142
    // Data �������� �����Ͽ��� �ݺ� ���� �� �ùٸ� ���ᰡ
    // �����ϴ�.
    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;


    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

        IDE_TEST(
            qmcHashTemp::getFirstSequence( & aDataPlan->
                                           hashMgr[aDataPlan->currTempID],
                                           & sSearchRow )
            != IDE_SUCCESS );
        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            sFlag = QMC_ROW_DATA_NONE;
            aDataPlan->currTempID++;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::searchNextSequence( qcTemplate * /* aTemplate */,
                             qmncHASH   * aCodePlan,
                             qmndHASH   * aDataPlan,
                             qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::searchNextSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sOrgRow;
    void * sSearchRow;

    qmcRowFlag sFlag;
    idBool     sIsNextTemp;

    sFlag = QMC_ROW_INITIALIZE;
    sIsNextTemp = ID_FALSE;

    // To Fix PR-8142
    // Data �������� �����Ͽ��� �ݺ� ���� �� �ùٸ� ���ᰡ
    // �����ϴ�.
    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;

    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

        // To Fix PR-12346
        // Temp Table�� ���� Temp Table�̶��
        // Cursor�� Open�ؼ� ó���� �� �ֵ��� �ؾ� ��.
        if ( sIsNextTemp == ID_TRUE )
        {
            IDE_TEST(
                qmcHashTemp::getFirstSequence( & aDataPlan->
                                               hashMgr[aDataPlan->currTempID],
                                               & sSearchRow )
                != IDE_SUCCESS );

            sIsNextTemp = ID_FALSE;
        }
        else
        {
            IDE_TEST(
                qmcHashTemp::getNextSequence( & aDataPlan->
                                              hashMgr[aDataPlan->currTempID],
                                              & sSearchRow )
                != IDE_SUCCESS );
        }

        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            aDataPlan->currTempID++;
            sFlag = QMC_ROW_DATA_NONE;

            sIsNextTemp = ID_TRUE;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHASH::searchFirstFilter( qcTemplate * aTemplate,
                            qmncHASH   * aCodePlan,
                            qmndHASH   * aDataPlan,
                            qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° ���� �˻�
 *
 * Implementation :
 *    ������ ���� ������ �˻��Ѵ�.
 *        - ����� Hash Key �� �� �˻��� Temp Table ����
 *        - ������ Temp Table�κ��� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::searchFirstFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void    * sOrgRow;
    void    * sSearchRow;

    UInt      sHashKey;

    IDE_TEST( getConstHashKey( aTemplate, aCodePlan, aDataPlan, & sHashKey )
              != IDE_SUCCESS );

    aDataPlan->currTempID = sHashKey % aCodePlan->tempTableCnt;

    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getFirstRange( & aDataPlan->
                                          hashMgr[aDataPlan->currTempID],
                                          sHashKey,
                                          aCodePlan->filter,
                                          & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

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
qmnHASH::searchNextFilter( qcTemplate * /* aTemplate */,
                           qmncHASH   * /* aCodePlan */,
                           qmndHASH   * aDataPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHASH::searchNextFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void    * sOrgRow;
    void    * sSearchRow;

    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getNextRange( & aDataPlan->
                                         hashMgr[aDataPlan->currTempID],
                                         & sSearchRow )
              != IDE_SUCCESS );
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

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

IDE_RC qmnHASH::searchFirstHit( qcTemplate * /* aTemplate */,
                                qmncHASH   * aCodePlan,
                                qmndHASH   * aDataPlan,
                                qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Hit �˻�
 *
 * Implementation :
 *    Temp Table�� ���� ���� ��츦 ����Ͽ� �����ϸ�,
 *    Row Pointer�� ���� �ʵ��� �Ͽ��� �Ѵ�.
 *
 *    Inverse Hash Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Inverse Hash Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 ***********************************************************************/

    UInt        i;
    void      * sOrgRow;
    void      * sSearchRow;
    qmcRowFlag  sFlag;

    sFlag = QMC_ROW_INITIALIZE;
    aDataPlan->currTempID = 0;

    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;

    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
        IDE_TEST(
            qmcHashTemp::getFirstHit( & aDataPlan->
                                      hashMgr[aDataPlan->currTempID],
                                      & sSearchRow )
            != IDE_SUCCESS );
        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            sFlag = QMC_ROW_DATA_NONE;
            aDataPlan->currTempID++;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        for ( i = 0; i < aCodePlan->tempTableCnt; i++ )
        {
            IDE_TEST( qmcHashTemp::clearHitFlag( & aDataPlan->hashMgr[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnHASH::searchNextHit( qcTemplate * /* aTemplate */,
                               qmncHASH   * aCodePlan,
                               qmndHASH   * aDataPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� Hit �˻�
 *
 * Implementation :
 *    Inverse Hash Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Inverse Hash Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 *
 ***********************************************************************/

    UInt       i;
    void     * sOrgRow;
    void     * sSearchRow;
    qmcRowFlag sFlag;
    idBool     sIsNextTemp;

    sFlag = QMC_ROW_INITIALIZE;
    sIsNextTemp = ID_FALSE;

    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;

    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

        if ( sIsNextTemp == ID_TRUE )
        {
            IDE_TEST(
                qmcHashTemp::getFirstHit( & aDataPlan->
                                          hashMgr[aDataPlan->currTempID],
                                          & sSearchRow )
                != IDE_SUCCESS );

            sIsNextTemp = ID_FALSE;
        }
        else
        {
            IDE_TEST(
                qmcHashTemp::getNextHit( & aDataPlan->
                                         hashMgr[aDataPlan->currTempID],
                                         & sSearchRow )
                != IDE_SUCCESS );
        }

        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            sFlag = QMC_ROW_DATA_NONE;
            aDataPlan->currTempID++;

            sIsNextTemp = ID_TRUE;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        for ( i = 0; i < aCodePlan->tempTableCnt; i++ )
        {
            // To Fix BUG-8725
            // qmcHashTemp::clear() --> qmcHashTemp::clearHitFlag() �� ����
            IDE_TEST( qmcHashTemp::clearHitFlag( & aDataPlan->hashMgr[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnHASH::searchFirstNonHit( qcTemplate * /* aTemplate */,
                            qmncHASH   * aCodePlan,
                            qmndHASH   * aDataPlan,
                            qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Non-Hit �˻�
 *
 * Implementation :
 *    Temp Table�� ���� ���� ��츦 ����Ͽ� �����ϸ�,
 *    Row Pointer�� ���� �ʵ��� �Ͽ��� �Ѵ�.
 *
 *    Full Outer Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Non-Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Full Outer Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 ***********************************************************************/

    UInt        i;
    void      * sOrgRow;
    void      * sSearchRow;
    qmcRowFlag  sFlag;

    sFlag = QMC_ROW_INITIALIZE;
    aDataPlan->currTempID = 0;

    // To Fix PR-8142
    // Data �������� �����Ͽ��� �ݺ� ���� �� �ùٸ� ���ᰡ
    // �����ϴ�.
    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;

    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
        IDE_TEST(
            qmcHashTemp::getFirstNonHit( & aDataPlan->
                                         hashMgr[aDataPlan->currTempID],
                                         & sSearchRow )
            != IDE_SUCCESS );
        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            sFlag = QMC_ROW_DATA_NONE;
            aDataPlan->currTempID++;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        for ( i = 0; i < aCodePlan->tempTableCnt; i++ )
        {
            // To Fix BUG-8725
            // qmcHashTemp::clear() --> qmcHashTemp::clearHitFlag() �� ����
            IDE_TEST( qmcHashTemp::clearHitFlag( & aDataPlan->hashMgr[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnHASH::searchNextNonHit( qcTemplate * /* aTemplate */,
                           qmncHASH   * aCodePlan,
                           qmndHASH   * aDataPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���� Non-Hit �˻�
 *
 * Implementation :
 *    Full Outer Join�� ó���� ������ �� ���Ǵ� �Լ���
 *    Non-Hit Record�� �� �̻� ���� ���, ��� ���� Row�� Hit Flag��
 *    �ʱ�ȭ�Ѵ�.  �̴� Subquery��� Full Outer Join�� ���� ��,
 *    ���� ���� ����� ���� ���� Subquery�� ������ ������ ���� �ʵ���
 *    �ϱ� �����̴�.
 *
 ***********************************************************************/
    
    UInt       i;
    void     * sOrgRow;
    void     * sSearchRow;
    qmcRowFlag sFlag;
    idBool     sIsNextTemp;

    sFlag = QMC_ROW_INITIALIZE;
    sIsNextTemp = ID_FALSE;

    // To Fix PR-8142
    // Data �������� �����Ͽ��� �ݺ� ���� �� �ùٸ� ���ᰡ
    // �����ϴ�.
    sFlag &= ~QMC_ROW_DATA_MASK;
    sFlag |= QMC_ROW_DATA_NONE;

    // Data�� ����, �� �̻��� Temp Table�� ���� ������ ����
    while ( (aDataPlan->currTempID < aCodePlan->tempTableCnt) &&
            ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE) )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

        // BUG-34831
        // Full outer join ���� multi-pass hash join �� ���� ����� �ؾ� ��
        // ���� temp table �� �б� ���� cursor �� ����� �Ѵ�.
        if ( sIsNextTemp == ID_TRUE )
        {
            IDE_TEST(
                qmcHashTemp::getFirstNonHit( & aDataPlan->
                                             hashMgr[aDataPlan->currTempID],
                                             & sSearchRow )
                != IDE_SUCCESS );

            sIsNextTemp = ID_FALSE;
        }
        else
        {
            IDE_TEST(
                qmcHashTemp::getNextNonHit( & aDataPlan->
                                            hashMgr[aDataPlan->currTempID],
                                            & sSearchRow )
                != IDE_SUCCESS );
        }

        aDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            // ���� Temp Table�� Row�� ������ �� �ִ�.
            sFlag = QMC_ROW_DATA_NONE;
            aDataPlan->currTempID++;

            sIsNextTemp = ID_TRUE;
        }
        else
        {
            sFlag = QMC_ROW_DATA_EXIST;
        }
    }

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        for ( i = 0; i < aCodePlan->tempTableCnt; i++ )
        {
            // To Fix BUG-8725
            // qmcHashTemp::clear() --> qmcHashTemp::clearHitFlag() �� ����
            IDE_TEST( qmcHashTemp::clearHitFlag( & aDataPlan->hashMgr[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
