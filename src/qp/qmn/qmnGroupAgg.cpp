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
 * $Id: qmnGroupAgg.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     GRAG(GRoup AGgregation) Node
 *
 *     ������ �𵨿��� Hash-based Grouping ������ �����ϴ� Plan Node �̴�.
 *
 *     Aggregation�� Group By�� ���¿� ���� ������ ���� ������ �Ѵ�.
 *         - Grouping Only
 *         - Aggregation Only
 *         - Group Aggregation
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnGroupAgg.h>
#include <qmxResultCache.h>

IDE_RC
qmnGRAG::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    GRAG ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);
    qmndGRAG * sCacheDataPlan = NULL;
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    // PROJ-2444 Parallel Aggreagtion
    sDataPlan->plan.mTID = QMN_PLAN_INIT_THREAD_ID;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_GRAG_INIT_DONE_MASK)
         == QMND_GRAG_INIT_DONE_FALSE )
    {
        if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
        {
            sCacheDataPlan = (qmndGRAG *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
            sCacheDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[sCodePlan->planID];
            sDataPlan->resultData.flag     = &aTemplate->resultCache.dataFlag[sCodePlan->planID];
            if ( qmxResultCache::initResultCache( aTemplate,
                                                  sCodePlan->componentInfo,
                                                  &sCacheDataPlan->resultData )
                 != IDE_SUCCESS )
            {
                *sDataPlan->flag &= ~QMN_PLAN_RESULT_CACHE_EXIST_MASK;
                *sDataPlan->flag |= QMN_PLAN_RESULT_CACHE_EXIST_FALSE;
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
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);
    }
    else
    {
        if ( ( *sDataPlan->resultData.flag & QMX_RESULT_CACHE_STORED_MASK )
             == QMX_RESULT_CACHE_STORED_FALSE )
        {
            IDE_TEST( aPlan->left->init( aTemplate,
                                         aPlan->left ) != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }
        *sDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_INIT_DONE_MASK;
        *sDataPlan->resultData.flag |= QMX_RESULT_CACHE_INIT_DONE_TRUE;
    }

    sDataPlan->doIt = qmnGRAG::doItFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnGRAG::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    GRAG �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    GRAG ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_GRAG_INIT_DONE_MASK)
         == QMND_GRAG_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }

    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    IDE_TEST(
        qmcHashTemp::getNullRow( sDataPlan->hashMgr,
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
qmnGRAG::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    GRAG ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRAG * sCodePlan = (qmncGRAG*) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndGRAG * sCacheDataPlan = NULL;
    idBool     sIsInit        = ID_FALSE;
    ULong  i;
    ULong  sPageCnt;
    SLong  sRecordCnt;
    UInt   sBucketCnt;

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

        if ( (*sDataPlan->flag & QMND_GRAG_INIT_DONE_MASK)
             == QMND_GRAG_INIT_DONE_TRUE )
        {
            // ���� ���� ȹ��
            sIsInit = ID_TRUE;
            if ( sDataPlan->groupNode != NULL )
            {
                IDE_TEST( qmcHashTemp::getDisplayInfo( sDataPlan->hashMgr,
                                                       & sPageCnt,
                                                       & sRecordCnt,
                                                       & sBucketCnt )
                          != IDE_SUCCESS );
            }
            else
            {
                sPageCnt = 0;
                sRecordCnt = 1;
                sBucketCnt = 1;
            }

            if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY )
            {
                if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                {
                    iduVarStringAppendFormat(
                        aString,
                        "GROUP-AGGREGATION ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "GROUP_COUNT: %"ID_INT64_FMT", "
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
                        "GROUP-AGGREGATION ( "
                        "ITEM_SIZE: BLOCKED, "
                        "GROUP_COUNT: %"ID_INT64_FMT", "
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
                        "GROUP-AGGREGATION ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "GROUP_COUNT: %"ID_INT64_FMT", "
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
                        "GROUP-AGGREGATION ( "
                        "ITEM_SIZE: BLOCKED, "
                        "GROUP_COUNT: %"ID_INT64_FMT", "
                        "DISK_PAGE_COUNT: BLOCKED, "
                        "ACCESS: %"ID_UINT32_FMT,
                        sRecordCnt,
                        sDataPlan->plan.myTuple->modify );
                }
            }
        }
        else
        {
            iduVarStringAppendFormat(
                aString,
                "GROUP-AGGREGATION ( ITEM_SIZE: 0, "
                "GROUP_COUNT: 0, "
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
                                  "GROUP-AGGREGATION ( ITEM_SIZE: ??, "
                                  "GROUP_COUNT: ??, "
                                  "BUCKET_COUNT: %"ID_UINT32_FMT", "
                                  "ACCESS: ??",
                                  sCodePlan->bucketCnt );
    }

    //----------------------------
    // Thread ID
    //----------------------------
    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( ( (*sDataPlan->flag & QMND_GRAG_INIT_DONE_MASK)
               == QMND_GRAG_INIT_DONE_TRUE ) &&
             ( sDataPlan->plan.mTID != QMN_PLAN_INIT_THREAD_ID ) )
        {
            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                iduVarStringAppendFormat( aString,
                                          ", TID: %"ID_UINT32_FMT"",
                                          sDataPlan->plan.mTID );
            }
            else
            {
                iduVarStringAppend( aString, ", TID: BLOCKED" );
            }
        }
        else
        {
            // Parallel execution �� �ƴ� ��� ����� �����Ѵ�.
        }
    }
    else
    {
        // Planonly �� ��� ����� �����Ѵ�.
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
    //-----------------------------

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        if ( ( sCodePlan->componentInfo != NULL ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
               == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
               == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
        {
            sCacheDataPlan = (qmndGRAG *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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

IDE_RC qmnGRAG::readyIt( qcTemplate * aTemplate,
                         qmnPlan    * aPlan,
                         UInt         aTID )
{
    qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    UInt       sModifyCnt;

    // ----------------
    // TID ����
    // ----------------
    sDataPlan->plan.mTID = aTID;

    if ( sDataPlan->plan.mTID != QMN_PLAN_INIT_THREAD_ID )
    {
        // PROJ-2444
        // ���ο� ���ø��� �°� mtrNode �ʱ�ȭ, temp table �ʱ�ȭ���� �۾��� �ٽ� ���־�� �Ѵ�.
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );

        // �ڽ� �÷��� readyIt �Լ��� ȣ���Ѵ�.
        IDE_TEST( aPlan->left->readyIt( aTemplate,
                                        aPlan->left,
                                        aTID ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // ���� ACCESS count
    sModifyCnt = sDataPlan->plan.myTuple->modify;

    // ----------------
    // Tuple ��ġ�� ����
    // ----------------
    sDataPlan->plan.myTuple = &aTemplate->tmplate.rows[sCodePlan->myNode->dstNode->node.table];

    // ACCESS count ����
    sDataPlan->plan.myTuple->modify = sModifyCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnGRAG::storeTempTable( qcTemplate * aTemplate,
                                qmncGRAG   * aCodePlan,
                                qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    GARG ��忡�� temp table �� �̿��Ͽ� grouping, aggr �۾��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool sDependency;
    qmndGRAG * sCacheDataPlan = NULL;

    IDE_TEST( checkDependency( aTemplate,
                               aCodePlan,
                               aDataPlan,
                               &sDependency ) != IDE_SUCCESS );

    if ( sDependency == ID_TRUE )
    {
        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        IDE_TEST( qmcHashTemp::clear( aDataPlan->hashMgr ) != IDE_SUCCESS );

        //----------------------------------------
        // Child�� �ݺ� �����Ͽ� Temp Table�� ����
        //----------------------------------------

        // PROJ-2444
        // parallel plan �϶� MERGE �ܰ�� ������ �Լ��� �����Ǿ� �ִ�.
        if ( (aCodePlan->flag & QMNC_GRAG_PARALLEL_STEP_MASK) == QMNC_GRAG_PARALLEL_STEP_MERGE )
        {
            if ( aDataPlan->groupNode == NULL )
            {
                IDE_TEST( aggrOnlyMerge( aTemplate,
                                         aCodePlan,
                                         aDataPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( aDataPlan->aggrNode == NULL )
                {
                    IDE_TEST( groupOnlyMerge( aTemplate,
                                              aCodePlan,
                                              aDataPlan )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( groupAggrMerge( aTemplate,
                                              aCodePlan,
                                              aDataPlan )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            if ( aDataPlan->groupNode == NULL )
            {
                //----------------------------------
                // GROUP BY�� ���� ���
                //----------------------------------

                IDE_DASSERT( aDataPlan->aggrNode != NULL );

                IDE_TEST( aggregationOnly( aTemplate,
                                           aCodePlan,
                                           aDataPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( aDataPlan->aggrNode == NULL )
                {
                    //----------------------------------
                    // Aggregation�� ���� ���
                    //----------------------------------

                    IDE_TEST( groupingOnly( aTemplate,
                                            aCodePlan,
                                            aDataPlan ) != IDE_SUCCESS );
                }
                else
                {
                    //----------------------------------
                    // GROUP BY�� Aggregation�� ��� �ִ� ���
                    //----------------------------------

                    IDE_TEST( groupAggregation( aTemplate,
                                                aCodePlan,
                                                aDataPlan ) != IDE_SUCCESS);
                }
            }
        }

        /* PROJ-2462 Result Cache */
        if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
        {
            sCacheDataPlan = (qmndGRAG *) (aTemplate->resultCache.data +
                                          aCodePlan->plan.offset);
            sCacheDataPlan->isNoData = aDataPlan->isNoData;
            *aDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_STORED_MASK;
            *aDataPlan->resultData.flag |= QMX_RESULT_CACHE_STORED_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        aDataPlan->depValue = aDataPlan->depTuple->modify;
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------------
    // ���� �Լ� ����
    //----------------------------------------

    if ( aDataPlan->isNoData == ID_FALSE )
    {
        aDataPlan->doIt = qmnGRAG::doItFirst;
    }
    else
    {
        // GROUP BY�� �ְ� Record�� ���� ���
        aDataPlan->doIt = qmnGRAG::doItNoData;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnGRAG::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */ )
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ù��° ���� �Լ�
 *
 * Implementation :
 *    Temp Table�κ��� �˻��� �����ϰ�, �ش� ����� Tuple Set�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // PROJ-2444
    // parallel �÷��� �ƴҶ� �����ؾ� �Ѵ�.
    if ( sDataPlan->plan.mTID == QMN_PLAN_INIT_THREAD_ID )
    {
        IDE_TEST(readyIt(aTemplate, aPlan, QMN_PLAN_INIT_THREAD_ID) != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2444
    // readyIt �Լ����� temp table �۾��� �����ϸ� �ȵȴ�.
    // ������ readyIt �Լ��� parallel �ϰ� ������� �ʴ´�.
    // ������ �Լ��� �и��Ͽ� �����Ѵ�.
    IDE_TEST( storeTempTable( aTemplate, sCodePlan, sDataPlan ) != IDE_SUCCESS );

    if ( sDataPlan->groupNode != NULL )
    {
        sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
        IDE_TEST( qmcHashTemp::getFirstSequence( sDataPlan->hashMgr,
                                                 & sSearchRow )
                  != IDE_SUCCESS );
        sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

        if ( sSearchRow == NULL )
        {
            *aFlag = QMC_ROW_DATA_NONE;
        }
        else
        {
            *aFlag = QMC_ROW_DATA_EXIST;
            IDE_TEST( setTupleSet( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnGRAG::doItNext;
        }
    }
    else
    {
        if ( sDataPlan->isNoData == ID_FALSE )
        {
            // Group By�� ���� ���� �ϳ��� Record���� �����Ѵ�.
            // ������ ���� Row�� �� ����� �ȴ�.
            // ����, ���� ���� �Լ��� Data ������ ������ �� �ִ�
            // �Լ��� �����Ѵ�.
            *aFlag = QMC_ROW_DATA_EXIST;
            IDE_TEST( setTupleSet( aTemplate, sCodePlan, sDataPlan )
                    != IDE_SUCCESS );

            sDataPlan->doIt = qmnGRAG::doItNoData;
        }
        else
        {
            // set that no data found
            *aFlag = QMC_ROW_DATA_NONE;

            sDataPlan->doIt = qmnGRAG::doItFirst;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnGRAG::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *    Temp Table�κ��� �˻��� �����ϰ�, �ش� ����� Tuple Set�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getNextSequence( sDataPlan->hashMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->doIt = qmnGRAG::doItFirst;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;
        IDE_TEST( setTupleSet( aTemplate, sCodePlan, sDataPlan )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::doItNoData( qcTemplate * aTemplate,
                     qmnPlan    * aPlan,
                     qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Only������ ���� ���� �Լ�
 *
 * Implementation :
 *    ��� ������ �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::doItNoData"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncGRAG * sCodePlan = (qmncGRAG *) aPlan;
    qmndGRAG * sDataPlan =
        (qmndGRAG *) (aTemplate->tmplate.data + aPlan->offset);

    // set that no data found
    *aFlag = QMC_ROW_DATA_NONE;

    sDataPlan->doIt = qmnGRAG::doItFirst;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnGRAG::firstInit( qcTemplate * aTemplate,
                    qmncGRAG   * aCodePlan,
                    qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    GRAG node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    //---------------------------------
    // GRAG ���� ������ �ʱ�ȭ
    //---------------------------------
    // 1. ���� Column�� �ʱ�ȭ
    // 2. Aggregation Column�� �ʱ�ȭ
    // 3. Grouping Column ������ ��ġ ����
    // 4. Tuple ��ġ ����

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );

    if ( aDataPlan->aggrNodeCnt > 0 )
    {
        IDE_TEST( initAggrNode( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->aggrNode = NULL;
    }

    IDE_TEST( initGroupNode( aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    aDataPlan->depTuple =
        & aTemplate->tmplate.rows[aCodePlan->depTupleRowID];
    aDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;
    aDataPlan->isNoData = ID_FALSE;

    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_GRAG_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_GRAG_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::initMtrNode( qcTemplate * aTemplate,
                      qmncGRAG   * aCodePlan,
                      qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmcMtrNode * sNode;
    UInt         sHeaderSize;
    UShort       i;

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
        sHeaderSize = QMC_MEMHASH_TEMPHEADER_SIZE;
    }
    else
    {
        sHeaderSize = QMC_DISKHASH_TEMPHEADER_SIZE;
    }

    //----------------------------------
    // Aggregation ������ ����
    //----------------------------------

    // PROJ-1473
    for( sNode = aCodePlan->myNode, i = 0;
         i < aCodePlan->baseTableCount;
         sNode = sNode->next, i++ )
    {
        // Nothing To Do 
    }  

    for ( sNode = sNode,
              aDataPlan->aggrNodeCnt  = 0;
          sNode != NULL;
          sNode = sNode->next )
    {
        if ( ( sNode->flag & QMC_MTR_GROUPING_MASK )
             == QMC_MTR_GROUPING_FALSE )
        {
            // aggregation node should not use converted source node
            //   e.g) SUM( MIN(I1) )
            //        MIN(I1)'s converted node is not aggregation node
            aDataPlan->aggrNodeCnt++;
        }
        else
        {
            break;
        }
    }

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

    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    // ���� Column�� ���� ���� ����
    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    // ���� Column�� �ʱ�ȭ
    // Aggregation�� ���� �� Conversion���� �����ؼ��� �ȵ�
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                (UShort)aCodePlan->baseTableCount +
                                aDataPlan->aggrNodeCnt )
              != IDE_SUCCESS );

    // ���� Column�� offset�� ������.
    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  sHeaderSize )
              != IDE_SUCCESS );

    // Row Size�� ���
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnGRAG::initAggrNode( qcTemplate * aTemplate,
                       qmncGRAG   * aCodePlan,
                       qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� �ʱ�ȭ
 *
 * Implementation :
 *    Aggregation Column�� �ʱ�ȭ�ϰ�,
 *    Distinct Aggregation�� ��� �ش� Distinct Node�� ã�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::initAggrNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt          i;
    UShort        sBaseCnt;    
    qmcMtrNode  * sNode;
    qmdMtrNode  * sAggrNode;
    qmdMtrNode  * sPrevNode;
    UInt          sHeaderSize = 0;

    //-----------------------------------------------
    // ���ռ� �˻�
    //-----------------------------------------------

    IDE_DASSERT( aCodePlan->aggrNodeOffset > 0 );

    //-----------------------------------------------
    // Aggregation Node�� ���� ������ �����ϰ� �ʱ�ȭ
    //-----------------------------------------------

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        aDataPlan->aggrNode = ( qmdMtrNode * )( aTemplate->resultCache.data +
                                                aCodePlan->aggrNodeOffset );
    }
    else
    {
        aDataPlan->aggrNode =
            (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->aggrNodeOffset);
    }

    for( sBaseCnt = 0,
             sNode = aCodePlan->myNode;
         sBaseCnt < aCodePlan->baseTableCount;
         sNode = sNode->next,
             sBaseCnt++ )
    {
        // Nothing To Do
    }

    // Aggregation Column�� ���� ���� ����
    for ( i = 0,
              sNode = sNode,
              sAggrNode = aDataPlan->aggrNode,
              sPrevNode = NULL;
          i < aDataPlan->aggrNodeCnt;
          i++, sNode = sNode->next, sAggrNode++ )
    {
        sAggrNode->myNode = sNode;
        sAggrNode->srcNode = NULL;
        sAggrNode->next = NULL;

        if ( sPrevNode != NULL )
        {
            sPrevNode->next = sAggrNode;
        }

        sPrevNode = sAggrNode;
    }

    // aggregation node should not use converted source node
    //   e.g) SUM( MIN(I1) )
    //        MIN(I1)'s converted node is not aggregation node
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                (qmdMtrNode*) aDataPlan->aggrNode,
                                (UShort)aDataPlan->aggrNodeCnt )
              != IDE_SUCCESS );

    // Aggregation Column�� offset�� ������.
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sHeaderSize = QMC_MEMHASH_TEMPHEADER_SIZE;
    }
    else
    {
        sHeaderSize = QMC_DISKHASH_TEMPHEADER_SIZE;
    }

    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  sHeaderSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::initGroupNode( qmncGRAG   * aCodePlan,
                        qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Column�� ��ġ�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::initGroupNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    qmdMtrNode * sMtrNode;

    for ( i = 0, sMtrNode = aDataPlan->mtrNode;
          i < ( aCodePlan->baseTableCount + aDataPlan->aggrNodeCnt );
          i++, sMtrNode = sMtrNode->next ) ;

    aDataPlan->groupNode = sMtrNode;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnGRAG::initTempTable( qcTemplate * aTemplate,
                        qmncGRAG   * aCodePlan,
                        qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt        sFlag;
    qmndGRAG  * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    //-----------------------------
    // Flag ���� �ʱ�ȭ
    //-----------------------------

    sFlag = QMCD_HASH_TMP_PRIMARY_TRUE | QMCD_HASH_TMP_DISTINCT_TRUE;

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

    //-----------------------------
    // Temp Table �ʱ�ȭ
    //-----------------------------
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnGRAG::initTempTable::qmxAlloc:hashMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                  (void **)&aDataPlan->hashMgr )
                  != IDE_SUCCESS );
        IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->groupNode,
                                     aDataPlan->aggrNode,
                                     aCodePlan->bucketCnt,
                                     sFlag )
                  != IDE_SUCCESS );

        if ( aDataPlan->groupNode == NULL )
        {
            IDU_FIT_POINT( "qmnGRAG::initTempTable::qmxAlloc:myTuple->row",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                          &aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {

            /* Nothing to do */
        }
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndGRAG *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnGRAG::initTempTable::qrcAlloc:hashMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                               (void **)&aDataPlan->hashMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->groupNode,
                                         aDataPlan->aggrNode,
                                         aCodePlan->bucketCnt,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->hashMgr = aDataPlan->hashMgr;

            if ( aDataPlan->groupNode == NULL )
            {
                IDU_FIT_POINT( "qmnGRAG::initTempTable::qrcAlloc:myTuple->row",
                               idERR_ABORT_InsufficientMemory );
                IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                              &aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );
                sCacheDataPlan->resultRow = aDataPlan->plan.myTuple->row;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            aDataPlan->hashMgr = sCacheDataPlan->hashMgr;
            aDataPlan->plan.myTuple->row = sCacheDataPlan->resultRow;
            aDataPlan->isNoData = sCacheDataPlan->isNoData;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnGRAG::checkDependency( qcTemplate * aTemplate,
                                 qmncGRAG   * aCodePlan,
                                 qmndGRAG   * aDataPlan,
                                 idBool     * aDependent )
{
/***********************************************************************
 *
 * Description :
 *    Dependent Tuple�� ���� ���� �˻�
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
qmnGRAG::aggregationOnly( qcTemplate * aTemplate,
                          qmncGRAG   * aCodePlan,
                          qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY�� ���� ���� �ϳ��� Row���� �����ϰ�,
 *     Aggregation�� �����Ѵ�.
 *
 * Implementation :
 *     �� ���� �޸𸮸� �Ҵ� �ް�, Temp Table�� ������� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::aggregationOnly"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    // Aggregation Column�� �ʱ�ȭ
    IDE_TEST( aggrInit( aTemplate, aDataPlan ) != IDE_SUCCESS );

    //-------------------------------------
    // Child�� �ݺ� ����� Aggregation
    //-------------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //-----------------------------------
        // PROJ-1473
        // ������忡�� ������ �÷� ����.
        //-----------------------------------       
        IDE_TEST( setBaseColumnRow( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );        
        
        IDE_TEST( aggrDoIt( aTemplate, aDataPlan ) != IDE_SUCCESS );

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    //----------------------------------------
    // Aggregation Column�� ������ ����
    //----------------------------------------

    // PROJ-2444
    // AGGR �ܰ迡���� aggrFini �� �����ϸ� �ȵȴ�.
    if ( (aCodePlan->flag & QMNC_GRAG_PARALLEL_STEP_MASK) != QMNC_GRAG_PARALLEL_STEP_AGGR )
    {
        IDE_TEST( aggrFini( aTemplate, aDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::groupingOnly( qcTemplate * aTemplate,
                       qmncGRAG   * aCodePlan,
                       qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY�� �����ϰ� Aggregation�� ���� ��쿡 ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::groupingOnly"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sInserted;

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    // doIt left child
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    //-----------------------------------
    // Result Set�� ���� ���� ����
    //-----------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        aDataPlan->isNoData = ID_FALSE;
    }
    else
    {
        // GRAG�� ���� ����� �ϳ��� ���� ����̴�.
        aDataPlan->isNoData = ID_TRUE;
    }

    sInserted = ID_TRUE;

    //-----------------------------------
    // Child�� �ݺ� ������ ���� Grouping ����
    //-----------------------------------

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //-----------------------------------
        // Memory ������ �Ҵ�
        //-----------------------------------

        if ( sInserted == ID_TRUE )
        {
            IDU_FIT_POINT( "qmnGRAG::groupingOnly::alloc::DataPlan",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                          & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            // ������ ������ �����,
            // Memory ������ �״�� ���� �ִ�.
            // Nothing To Do
        }

        //-----------------------------------
        // PROJ-1473
        // ������忡�� ������ �÷� ����.
        //-----------------------------------
        
        IDE_TEST( setBaseColumnRow( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );        

        //-----------------------------------
        // ���� Row�� ���� �� ����
        //-----------------------------------

        IDE_TEST( setGroupRow( aTemplate,
                               aDataPlan ) != IDE_SUCCESS );

        IDE_TEST( qmcHashTemp::addDistRow( aDataPlan->hashMgr,
                                           & aDataPlan->plan.myTuple->row,
                                           & sInserted )
                  != IDE_SUCCESS );

        //-----------------------------------
        // Child Plan ����
        //-----------------------------------

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::groupAggregation( qcTemplate * aTemplate,
                           qmncGRAG   * aCodePlan,
                           qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::groupAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void   * sOrgRow;
    void   * sSearchRow;
    void   * sAllocRow;

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    //-----------------------------------
    // Result Set�� ���� ���� ����
    //-----------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        aDataPlan->isNoData = ID_FALSE;
    }
    else
    {
        // GRAG�� ���� ����� �ϳ��� ���� ����̴�.
        aDataPlan->isNoData = ID_TRUE;
    }

    sSearchRow = NULL;
    sAllocRow = NULL;

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //-----------------------------------
        // Memory ������ �Ҵ�
        //-----------------------------------

        if ( sSearchRow == NULL )
        {
            IDU_FIT_POINT( "qmnGRAG::groupAggregation::alloc::DataPlan",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                          & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
            sAllocRow = aDataPlan->plan.myTuple->row;
        }
        else
        {
            // ������ ������ �����,
            // Memory ������ �״�� ���� �ִ�.
            aDataPlan->plan.myTuple->row = sAllocRow;
        }

        //-----------------------------------
        // PROJ-1473
        // ������忡�� ������ �÷� ����.
        //-----------------------------------
        
        IDE_TEST( setBaseColumnRow( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );        

        //-----------------------------------
        // To Fix PR-8213
        // ���� Group�� �˻�
        //-----------------------------------

        IDE_TEST( setGroupRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        sSearchRow = aDataPlan->plan.myTuple->row;
        IDE_TEST( qmcHashTemp::getSameGroup( aDataPlan->hashMgr,
                                             & aDataPlan->plan.myTuple->row,
                                             & sSearchRow )
                  != IDE_SUCCESS );
        
        //-----------------------------------
        // Aggregation ó��
        //-----------------------------------

        if ( sSearchRow == NULL )
        {
            // ���� Group�� ���� ���� ���� Row�� �����Ͽ�
            // ���ο� Group�� �����Ѵ�.
            IDE_TEST( aggrInit( aTemplate, aDataPlan )
                      != IDE_SUCCESS );

            IDE_TEST( aggrDoIt( aTemplate, aDataPlan )
                      != IDE_SUCCESS );
            
            IDE_TEST( qmcHashTemp::addNewGroup( aDataPlan->hashMgr,
                                                aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->plan.myTuple->row = sSearchRow;
            
            // �̹� ���� Group�� �����ϴ� ���� Aggregation ����
            IDE_TEST( aggrDoIt( aTemplate, aDataPlan )
                      != IDE_SUCCESS );

            sOrgRow = aDataPlan->plan.myTuple->row;
            IDE_TEST( qmcHashTemp::updateAggr( aDataPlan->hashMgr )
                      != IDE_SUCCESS );
            aDataPlan->plan.myTuple->row = sOrgRow;
        }

        //-----------------------------------
        // Child Plan ����
        //-----------------------------------

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    // PROJ-2444
    // AGGR �ܰ迡���� aggrFini �� �����ϸ� �ȵȴ�.
    if ( (aCodePlan->flag & QMNC_GRAG_PARALLEL_STEP_MASK) != QMNC_GRAG_PARALLEL_STEP_AGGR )
    {
        //--------------------------------------
        // To-Fix PR-8415
        // ��� Group�� ���Ͽ� Aggregation �ϷḦ �����Ͽ��� �Ѵ�.
        // Disk Temp Table�� ��� Memory ���� Aggregation���� ������ ���
        // ������ ORDER BY���� �����ϸ� Disk �󿡴� ���� Aggregation�����
        // �������� �ʾ� ��Ȯ�� ����� ������ �� ����.
        //--------------------------------------

        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
        IDE_TEST( qmcHashTemp::getFirstGroup( aDataPlan->hashMgr,
                                              & sSearchRow )
                != IDE_SUCCESS );
        aDataPlan->plan.myTuple->row =
            ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while ( sSearchRow != NULL )
        {
            //----------------------------------------
            // Aggregation Column�� ������ ����
            //----------------------------------------

            IDE_TEST( aggrFini( aTemplate, aDataPlan ) != IDE_SUCCESS );

            sOrgRow = aDataPlan->plan.myTuple->row;
            IDE_TEST( qmcHashTemp::updateFiniAggr( aDataPlan->hashMgr )
                    != IDE_SUCCESS );
            aDataPlan->plan.myTuple->row = sOrgRow;

            //----------------------------------------
            // ���ο� Group ȹ��
            //----------------------------------------

            sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
            IDE_TEST( qmcHashTemp::getNextGroup( aDataPlan->hashMgr,
                                                 & sSearchRow )
                    != IDE_SUCCESS );
            aDataPlan->plan.myTuple->row =
                ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
IDE_RC qmnGRAG::aggrOnlyMerge( qcTemplate * aTemplate,
                               qmncGRAG   * aCodePlan,
                               qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2444 Parallel Aggreagtion
 *               aggr �� ����Ҷ�
 *               parallel plan �� merge �ܰ踦 �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void        * sChildRow         = NULL;
    qmnPlan     * sChildPlan        = aCodePlan->plan.left;
    qmndPlan    * sChildDataPlan    = (qmndPlan*)(aTemplate->tmplate.data + sChildPlan->offset);
    qmcRowFlag    sFlag             = QMC_ROW_INITIALIZE;

    IDE_TEST( aggrInit( aTemplate, aDataPlan ) != IDE_SUCCESS );

    //-------------------------------------
    // Child�� �ݺ� ����� Aggregation
    //-------------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          &sFlag ) != IDE_SUCCESS );

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        sChildRow = sChildDataPlan->myTuple->row;

        IDE_TEST( aggrMerge( aTemplate,
                             aDataPlan,
                             sChildRow )
                  != IDE_SUCCESS );

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    //----------------------------------------
    // Aggregation Column�� ������ ����
    //----------------------------------------

    IDE_TEST( aggrFini( aTemplate, aDataPlan ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnGRAG::groupOnlyMerge( qcTemplate * aTemplate,
                                qmncGRAG   * aCodePlan,
                                qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2444 Parallel Aggreagtion
 *               group by �� ����Ҷ�
 *               parallel plan �� merge �ܰ踦 �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void        * sChildRow         = NULL;
    qmnPlan     * sChildPlan        = aCodePlan->plan.left;
    qmndPlan    * sChildDataPlan    = (qmndPlan*)(aTemplate->tmplate.data + sChildPlan->offset);
    idBool        sInserted         = ID_TRUE;
    qmcRowFlag    sFlag             = QMC_ROW_INITIALIZE;

    // doIt left child
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          &sFlag ) != IDE_SUCCESS );

    //-----------------------------------
    // Result Set�� ���� ���� ����
    //-----------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        aDataPlan->isNoData = ID_FALSE;
    }
    else
    {
        // GRAG�� ���� ����� �ϳ��� ���� ����̴�.
        aDataPlan->isNoData = ID_TRUE;
    }

    //-----------------------------------
    // Child�� �ݺ� ������ ���� Grouping ����
    //-----------------------------------

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //-----------------------------------
        // ���� Row�� ���� �� ����
        //-----------------------------------

        // disk temp �� ������ aDataPlan->plan.myTuple->row �� �۾��� �Ѵ�.
        sChildRow = sChildDataPlan->myTuple->row;
        aDataPlan->plan.myTuple->row = sChildRow;

        IDE_TEST( qmcHashTemp::addDistRow( aDataPlan->hashMgr,
                                           &sChildRow,  // �˻��� row
                                           &sInserted )
                  != IDE_SUCCESS );

        //-----------------------------------
        // Child Plan ����
        //-----------------------------------

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              &sFlag ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnGRAG::groupAggrMerge( qcTemplate * aTemplate,
                                qmncGRAG   * aCodePlan,
                                qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2444 Parallel Aggreagtion
 *               Aggr �Լ��� group by �� ����Ҷ�
 *               parallel plan �� merge �ܰ踦 �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void        * sBufferRow        = NULL;
    void        * sSearchRow        = NULL;
    void        * sChildRow         = NULL;
    qmnPlan     * sChildPlan        = aCodePlan->plan.left;
    qmndPlan    * sChildDataPlan    = (qmndPlan*)(aTemplate->tmplate.data + sChildPlan->offset);
    qmcRowFlag    sFlag             = QMC_ROW_INITIALIZE;

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    //-----------------------------------
    // Result Set�� ���� ���� ����
    //-----------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        aDataPlan->isNoData = ID_FALSE;
    }
    else
    {
        // GRAG�� ���� ����� �ϳ��� ���� ����̴�.
        aDataPlan->isNoData = ID_TRUE;
    }

    sBufferRow = aDataPlan->plan.myTuple->row;

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // sChildRow �� ���� GRAG ����� mtr ������ �����ϴ�.
        sChildRow  = sChildDataPlan->myTuple->row;
        sSearchRow = sBufferRow;

        // �޸� temp �� sSearchRow �� �ּҰ��� �����ϰ�
        // ��ũ temp �� *(sSearchRow) �� ��ġ�� ���� �����Ѵ�.
        IDE_TEST( qmcHashTemp::getSameGroup( aDataPlan->hashMgr,
                                             &sChildRow,    // �˻��� row
                                             &sSearchRow )  // �����
                  != IDE_SUCCESS );

        //-----------------------------------
        // Aggregation ó��
        //-----------------------------------

        if ( sSearchRow == NULL )
        {
            //-----------------------------------
            // PROJ-2527 WITHIN GROUP AGGR
            // Memory �Ҵ�
            // parent�� function data �� child function data merge�Ѵ�.
            //-----------------------------------
            
            // ���� Group�� ���� ���� ���� Row�� �����Ͽ�
            // ���ο� Group�� �����Ѵ�.
            // �޸� temp �� �̹� insert �� �Ϸ�� ����
            // ��ũ temp �� aDataPlan->plan.myTuple->row �� ���� insert �Ѵ�.
            IDE_TEST( qmcHashTemp::alloc( aDataPlan->hashMgr,
                                          &aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            // copy group keys
            idlOS::memcpy( aDataPlan->plan.myTuple->row,
                           sChildRow,
                           aDataPlan->plan.myTuple->rowOffset );
            
            IDE_TEST( aggrInit( aTemplate, aDataPlan )
                      != IDE_SUCCESS );
            
            IDE_TEST( aggrMerge( aTemplate,
                                 aDataPlan,
                                 sChildRow )
                      != IDE_SUCCESS );

            IDE_TEST( qmcHashTemp::addNewGroup( aDataPlan->hashMgr,
                                                aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            // �Ʒ� �۾����� aDataPlan->plan.myTuple->row �� �̿��Ѵ�.
            aDataPlan->plan.myTuple->row = sSearchRow;
            
            IDE_TEST( aggrMerge( aTemplate,
                                 aDataPlan,
                                 sChildRow )
                      != IDE_SUCCESS );

            IDE_TEST( qmcHashTemp::updateAggr( aDataPlan->hashMgr )
                      != IDE_SUCCESS );
        }

        //-----------------------------------
        // Child Plan ����
        //-----------------------------------

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              &sFlag ) != IDE_SUCCESS );
    }

    //----------------------------------------
    // Aggregation Column�� ������ ����
    //----------------------------------------

    sSearchRow = aDataPlan->plan.myTuple->row;

    IDE_TEST( qmcHashTemp::getFirstGroup( aDataPlan->hashMgr,
                                          &sSearchRow )
              != IDE_SUCCESS );

    while ( sSearchRow != NULL )
    {
        aDataPlan->plan.myTuple->row = sSearchRow;

        IDE_TEST( aggrFini( aTemplate, aDataPlan ) != IDE_SUCCESS );

        IDE_TEST( qmcHashTemp::updateFiniAggr( aDataPlan->hashMgr )
                  != IDE_SUCCESS );

        //----------------------------------------
        // ���ο� Group ȹ��
        //----------------------------------------

        IDE_TEST( qmcHashTemp::getNextGroup( aDataPlan->hashMgr,
                                             & sSearchRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnGRAG::setBaseColumnRow( qcTemplate * aTemplate,
                           qmncGRAG   * aCodePlan,
                           qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Column�� ���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort       i;    
    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode, i = 0;
          ( sNode != NULL ) && 
              ( i < aCodePlan->baseTableCount );
          sNode = sNode->next, i++ )
    {
        IDE_TEST( sNode->func.setMtr( aTemplate,
                                      sNode,
                                      aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnGRAG::setGroupRow( qcTemplate * aTemplate,
                      qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Column�� ���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::setGroupRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // idBool       sExist;
    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->groupNode;
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
qmnGRAG::aggrInit( qcTemplate * aTemplate,
                   qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::aggrInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( qtc::initialize( sNode->myNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnGRAG::aggrDoIt( qcTemplate * aTemplate,
                   qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� ���� ����
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::aggrDoIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( qtc::aggregate( sNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnGRAG::aggrMerge( qcTemplate * aTemplate,
                    qmndGRAG   * aDataPlan,
                    void       * aSrcRow )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� ���� ����
 * Implementation :
 *
 ***********************************************************************/

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( aTemplate->tmplate.
                             rows[sNode->dstNode->node.table].
                             execute[sNode->dstNode->node.column].
             merge( (mtcNode*)sNode->dstNode,
                    aTemplate->tmplate.stack,
                    aTemplate->tmplate.stackRemain,
                    aSrcRow,
                    &aTemplate->tmplate ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnGRAG::aggrFini( qcTemplate * aTemplate,
                   qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� ������
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::aggrFini"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( qtc::finalize( sNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRAG::setTupleSet( qcTemplate * aTemplate,
                      qmncGRAG   * aCodePlan,
                      qmndGRAG   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Row�� �̿��Ͽ� Tuple Set�� ����
 *
 * Implementation :
 *     Grouping Column�� Tuple Set�� �����ϰ�
 *     Aggregation Column�� ���� ������ ������ �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRAG::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort       i;    
    qmdMtrNode * sNode;

    //----------------------------------------
    // PROJ-1473
    // ������忡�� �����ؾ� �� �����÷��� ����
    //----------------------------------------

    for ( sNode = aDataPlan->mtrNode, i = 0;
          ( sNode != NULL ) &&
              ( i < aCodePlan->baseTableCount );
          sNode = sNode->next, i++ )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate, sNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    //----------------------------------------
    // Grouping Column�� Tuple Set ����
    //----------------------------------------

    for ( sNode = aDataPlan->groupNode;
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
