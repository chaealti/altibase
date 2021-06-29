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
 * $Id: qmnLimitSort.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     LMST(LiMit SorT) Node
 *
 *     ������ �𵨿��� sorting ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Limit Order By
 *             : SELECT * FROM T1 ORDER BY I1 LIMIT 10;
 *         - Store And Search
 *             : MIN, MAX ������ ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcuProperty.h>
#include <qmnLimitSort.h>
#include <qmxResultCache.h>

IDE_RC
qmnLMST::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    LMST ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLMST * sCodePlan = (qmncLMST *) aPlan;
    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    idBool sDependency;

    sDataPlan->doIt        = qmnLMST::doItDefault;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_LMST_INIT_DONE_MASK)
         == QMND_LMST_INIT_DONE_FALSE )
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

        sDataPlan->mtrTotalCnt = 0;
        sDataPlan->isNullStore = ID_FALSE;

        IDE_TEST( qmcSortTemp::clear( sDataPlan->sortMgr ) != IDE_SUCCESS );

        //----------------------------------------
        // �뵵�� ���� Temp Table�� ����
        //----------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        // BUG-37681
        if( sCodePlan->limitCnt != 0 )
        {
            if ( (sCodePlan->flag & QMNC_LMST_USE_MASK ) == QMNC_LMST_USE_ORDERBY )
            {
                // Limit Order By�� ���� ����
                IDE_TEST( store4OrderBy( aTemplate, sCodePlan, sDataPlan )
                        != IDE_SUCCESS );
            }
            else
            {
                // Store And Search�� ���� ����
                IDE_TEST( store4StoreSearch( aTemplate, sCodePlan, sDataPlan )
                        != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing To Do
        }

        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        sDataPlan->depValue = sDataPlan->depTuple->modify;
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------------
    // ���� �Լ� ����
    //----------------------------------------

    // BUG-37681 limit 0 �϶� ������ doit �Լ��� �����ϵ��� �Ѵ�.
    if( sCodePlan->limitCnt != 0 )
    {
        if ( (sCodePlan->flag & QMNC_LMST_USE_MASK ) == QMNC_LMST_USE_ORDERBY )
        {
            sDataPlan->doIt = qmnLMST::doItFirstOrderBy;
        }
        else
        {
            sDataPlan->doIt = qmnLMST::doItFirstStoreSearch;
        }
    }
    else
    {
        sDataPlan->doIt = qmnLMST::doItAllFalse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnLMST::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    LMST �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncLMST * sCodePlan = (qmncLMST *) aPlan;
    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnLMST::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    LMST ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN ""
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLMST * sCodePlan = (qmncLMST *) aPlan;
    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_LMST_INIT_DONE_MASK)
         == QMND_LMST_INIT_DONE_FALSE )
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
qmnLMST::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    LMST ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLMST * sCodePlan = (qmncLMST *) aPlan;
    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndLMST * sCacheDataPlan = NULL;
    idBool     sIsInit       = ID_FALSE;
    ULong  i;
    ULong  sDiskPageCnt;
    SLong  sRecordCnt;

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

        if ( (*sDataPlan->flag & QMND_LMST_INIT_DONE_MASK)
             == QMND_LMST_INIT_DONE_TRUE )
        {
            sIsInit = ID_TRUE;
            // ���� ���� ȹ��
            IDE_TEST( qmcSortTemp::getDisplayInfo( sDataPlan->sortMgr,
                                                   & sDiskPageCnt,
                                                   & sRecordCnt )
                      != IDE_SUCCESS );
            IDE_DASSERT( sDiskPageCnt == 0 );

            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                iduVarStringAppendFormat(
                    aString,
                    "LIMIT-SORT ( ITEM_SIZE: %"ID_UINT32_FMT", "
                    "ITEM_COUNT: %"ID_UINT64_FMT", "
                    "STORE_COUNT: %"ID_INT64_FMT", "
                    "ACCESS: %"ID_UINT32_FMT,
                    sDataPlan->mtrRowSize,
                    sDataPlan->mtrTotalCnt,
                    sRecordCnt,
                    sDataPlan->plan.myTuple->modify );
            }
            else
            {
                // BUG-29209
                // ITEM_SIZE ���� �������� ����
                iduVarStringAppendFormat(
                    aString,
                    "LIMIT-SORT ( ITEM_SIZE: BLOCKED, "
                    "ITEM_COUNT: %"ID_UINT64_FMT", "
                    "STORE_COUNT: %"ID_INT64_FMT", "
                    "ACCESS: %"ID_UINT32_FMT,
                    sDataPlan->mtrTotalCnt,
                    sRecordCnt,
                    sDataPlan->plan.myTuple->modify );
            }
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "LIMIT-SORT ( ITEM_SIZE: 0, "
                                      "ITEM_COUNT: 0, "
                                      "STORE_COUNT: 0, "
                                      "ACCESS: 0" );
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        iduVarStringAppendFormat( aString,
                                  "LIMIT-SORT ( ITEM_SIZE: ??, "
                                  "ITEM_COUNT: ??, "
                                  "STORE_COUNT: ??, "
                                  "ACCESS: ??" );

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
            sCacheDataPlan = (qmndLMST *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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

IDE_RC
qmnLMST::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnLMST::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnLMST::doItFirstOrderBy( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    LIMIT ORDER BY�� ���� ���� ����
 *
 * Implementation :
 *    Temp Table�� �̿��Ͽ� ���� �˻��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doItFirstOrderBy"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstSequence( sDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Data�� ������ ��� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );

        sDataPlan->doIt = qmnLMST::doItNextOrderBy;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnLMST::doItNextOrderBy( qcTemplate * aTemplate,
                          qmnPlan    * aPlan,
                          qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    LIMIT ORDER BY�� ���� ���� ����
 *
 * Implementation :
 *    Temp Table�� �̿��Ͽ� ���� �˻��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doItNextOrderBy"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextSequence( sDataPlan->sortMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Data�� ������ ��� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->doIt = qmnLMST::doItFirstOrderBy;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnLMST::doItFirstStoreSearch( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ���� ù ��° �˻��� �����Ѵ�.
 *
 * Implementation :
 *     LMST�� Store And Search�� ���� ���Ǵ� ����
 *     ������ ���� Subquery Predicate�� ���Ǵ� ����̴�.
 *     Predicate�� �ʿ�� �ϴ� Data�� ������ ������ ����.
 *     ------------------------------------------------------
 *       Predicate          |   �ʿ� Data
 *     ------------------------------------------------------
 *       >ANY, >=ANY        |  MIN Value, NULL Value
 *       <ANY, <=ANY        |  MAX Value, NULL Value
 *       >ALL, >=ALL        |  MAX Value, NULL Value
 *       <ALL, <=ALL        |  MIN Value, NULL Value
 *       =ALL               |  MIN Value, MAX Value, NULL Value
 *       !=ALL              |  MIN Value, MAX Value, NULL Value
 *     ------------------------------------------------------
 *
 *    �׷���, Predicate�� ���� �ʿ� Data�� ������ �������� �ʰ�
 *    ������ ���� ������ ���Ͽ� Data�� Return �� �� �ֵ��� �Ͽ�
 *    ���⵵�� �����.
 *        MIN Value -> MAX Value -> NULL Value -> Data ����
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doItFirstStoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    //------------------------------------
    // Left-Most �� ����
    //------------------------------------

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstSequence( sDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    if ( sSearchRow != NULL )
    {
        //------------------------------------
        // Data�� ������ ��� Tuple Set ����
        //------------------------------------

        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );

        sDataPlan->doIt = qmnLMST::doItNextStoreSearch;
    }
    else
    {
        //------------------------------------
        // Data�� ���� ��� NULL ���� ���� Ȯ��
        //------------------------------------

        if ( sDataPlan->isNullStore == ID_TRUE )
        {
            // NULL Row�� ����� ��
            *aFlag = QMC_ROW_DATA_EXIST;

            IDE_TEST( qmnLMST::padNull( aTemplate, aPlan ) != IDE_SUCCESS );

            sDataPlan->doIt = qmnLMST::doItLastStoreSearch;
        }
        else
        {
            // ���ϴ� ����� ����
            *aFlag = QMC_ROW_DATA_NONE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnLMST::doItNextStoreSearch( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ���� ���� �˻��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doItNextStoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    //------------------------------------
    // Right-Most �� ����
    //------------------------------------

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getNextSequence( sDataPlan->sortMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    if ( sSearchRow != NULL )
    {
        //------------------------------------
        // Data�� ������ ��� Tuple Set ����
        //------------------------------------

        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------
        // Data�� ���� ��� NULL ���� ���� Ȯ��
        //------------------------------------

        if ( sDataPlan->isNullStore == ID_TRUE )
        {
            // NULL Row�� ����� ��
            *aFlag = QMC_ROW_DATA_EXIST;

            IDE_TEST( qmnLMST::padNull( aTemplate, aPlan ) != IDE_SUCCESS );

            sDataPlan->doIt = qmnLMST::doItLastStoreSearch;
        }
        else
        {
            // ���ϴ� ����� ����
            *aFlag = QMC_ROW_DATA_NONE;
            sDataPlan->doIt = qmnLMST::doItFirstStoreSearch;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnLMST::doItLastStoreSearch( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ���� ������ �˻��� �����Ѵ�.
 *
 * Implementation :
 *     ������ ������ �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::doItLastStoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncLMST * sCodePlan = (qmncLMST *) aPlan;
    qmndLMST * sDataPlan =
        (qmndLMST *) (aTemplate->tmplate.data + aPlan->offset);

    *aFlag = QMC_ROW_DATA_NONE;
    sDataPlan->doIt = qmnLMST::doItFirstStoreSearch;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnLMST::doItAllFalse( qcTemplate * /*aTemplate*/,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
    qmncLMST * sCodePlan = (qmncLMST *) aPlan;

    IDE_DASSERT( sCodePlan->limitCnt == 0 );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;
}

IDE_RC
qmnLMST::firstInit( qcTemplate * aTemplate,
                    qmncLMST   * aCodePlan,
                    qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    LMST node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndLMST * sCacheDataPlan = NULL;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY );

    IDE_DASSERT( aCodePlan->limitCnt <= QMN_LMST_MAXIMUM_LIMIT_CNT );

    //---------------------------------
    // LMST ���� ������ �ʱ�ȭ
    //---------------------------------

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndLMST *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
        aDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[aCodePlan->planID];
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

    *aDataPlan->flag &= ~QMND_LMST_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_LMST_INIT_DONE_TRUE;

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
qmnLMST::initMtrNode( qcTemplate * aTemplate,
                      qmncLMST   * aCodePlan,
                      qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ ���� ��带 �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );

    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);

    if ( (aCodePlan->flag & QMNC_LMST_USE_MASK ) == QMNC_LMST_USE_ORDERBY )
    {
        // Nothing To Do
    }
    else
    {
        // Store And Search�� ���Ǵ� ��� Base Table�� �������� �ʴ´�.
        IDE_DASSERT( aCodePlan->baseTableCount == 0 );
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

    /* PROJ-2462 Result Cache */
    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    // 1.  ���� Column�� ���� ���� ����
    // 2.  ���� Column�� �ʱ�ȭ
    // 3.  ���� Column�� offset�� ������
    // 4.  Row Size�� ���

    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                aCodePlan->baseTableCount )
              != IDE_SUCCESS );

    // Memory Temp Table���� �����
    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  QMC_MEMSORT_TEMPHEADER_SIZE )
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
qmnLMST::initSortNode( qmncLMST   * aCodePlan,
                       qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ���� Column�� ���� �� ���� Column�� ���� ��ġ�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::initSortNode"
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

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    if ( (aCodePlan->flag & QMNC_LMST_USE_MASK ) == QMNC_LMST_USE_ORDERBY )
    {
        // �ݵ�� ���� Column�� �־�� �Ѵ�.
        IDE_DASSERT( aDataPlan->sortNode != NULL );
    }
    else
    {
        // Store And Search�� ���Ǵ� ���
        IDE_DASSERT( aDataPlan->sortNode == aDataPlan->mtrNode );
    }

    return IDE_SUCCESS;

#undef IDE_FN
}


IDE_RC
qmnLMST::initTempTable( qcTemplate * aTemplate,
                        qmncLMST   * aCodePlan,
                        qmndLMST   * aDataPlan )
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
    qmndLMST * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

    // Memory Temp Table���� �����
    IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK )
                 == MTC_TUPLE_STORAGE_MEMORY );

    //-----------------------------
    // Flag ���� �ʱ�ȭ
    //-----------------------------

    sFlag = QMCD_SORT_TMP_STORAGE_MEMORY;

    //-----------------------------
    // Temp Table �ʱ�ȭ
    //-----------------------------

    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnLMST::initTempTable::qmxAlloc:sortMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                  (void **)&aDataPlan->sortMgr )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->sortNode,
                                     0,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndLMST *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnLMST::initTempTable::qrcAlloc:sortMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                               (void **)&aDataPlan->sortMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->sortNode,
                                         0,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->sortMgr = aDataPlan->sortMgr;
        }
        else
        {
            aDataPlan->sortMgr     = sCacheDataPlan->sortMgr;
            aDataPlan->mtrTotalCnt = sCacheDataPlan->mtrTotalCnt;
            aDataPlan->isNullStore = sCacheDataPlan->isNullStore;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnLMST::checkDependency( qcTemplate * aTemplate,
                                 qmncLMST   * aCodePlan,
                                 qmndLMST   * aDataPlan,
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
qmnLMST::store4OrderBy( qcTemplate * aTemplate,
                        qmncLMST   * aCodePlan,
                        qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     ORDER BY�� ���� ����
 *
 * Implementation :
 *     ���� ���Ǳ��� Child�� �ݺ� �����Ͽ� ����
 *          - Data�� �������� ���� ���
 *          - Limit���� ���� ���ԵǴ� ���
 *     Sorting ����
 *     Data�� �� ������ ���
 *          - ���� ���� ���� �� ���� ����
 *          - Memory�� �׻� �����ϰ� �ǹǷ�, �߰����� �޸� �Ҵ���
 *            ���� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::store4OrderBy"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;
    qmndLMST * sCacheDataPlan = NULL;

    //------------------------------
    // Child Record�� ����
    //------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST) &&
            ( aDataPlan->mtrTotalCnt < aCodePlan->limitCnt ) )
    {
        aDataPlan->mtrTotalCnt++;

        IDE_TEST( addOneRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    //------------------------------
    // ���� ����
    //------------------------------
    // SORT�� �޸� LMST�� ���, �׻� ������ ������
    IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr ) != IDE_SUCCESS );

    //------------------------------
    // Limit Sorting ����
    //------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                      & aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            aDataPlan->mtrTotalCnt++;

            IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::shiftAndAppend( aDataPlan->sortMgr,
                                                   aDataPlan->plan.myTuple->row,
                                                   & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                                  aCodePlan->plan.left,
                                                  & sFlag ) != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2462 Result Cache
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndLMST *)(aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->mtrTotalCnt = aDataPlan->mtrTotalCnt;
        sCacheDataPlan->isNullStore = aDataPlan->isNullStore;

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

#undef IDE_FN
}


IDE_RC
qmnLMST::store4StoreSearch( qcTemplate * aTemplate,
                            qmncLMST   * aCodePlan,
                            qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Store And Search�� ���� ����
 *     MIN, MAX���� �����ϰ� NULL���� ���θ� �Ǵ��Ѵ�.
 *
 * Implementation :
 *     2�Ǹ��� ����
 *        - MIN, MAX ó���� ���� �� �Ǹ��� ����
 *        - NULL ���� ������ �˻��Ͽ� NULL�� �ƴ� ��츸 ����
 *     2�� ���� �� ���� ����
 *     �� �� �����ϴ� Column�� NULL�� �ƴ� ��쿡 ���Ͽ�
 *          - Min, Max���� ��ȯ�Ͽ� ����
 *          - Memory�� �׻� �����ϰ� �ǹǷ�, �߰����� �޸� �Ҵ���
 *            ���� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::store4StoreSearch"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;
    idBool     sIsNull;
    ULong      sRowCnt;
    qmndLMST * sCacheDataPlan = NULL;

    //------------------------------
    // ���ռ� �˻�
    //------------------------------

    IDE_DASSERT( aCodePlan->limitCnt == 2 );

    //------------------------------
    // Child Record�� ����
    //------------------------------

    IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                  & aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    sRowCnt = 0;
    while ( ((sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST) &&
            sRowCnt < aCodePlan->limitCnt )
    {
        aDataPlan->mtrTotalCnt++;

        IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        sIsNull = aDataPlan->sortNode->func.isNull( aDataPlan->sortNode,
                                                    aDataPlan->plan.myTuple->row );
        if ( sIsNull == ID_TRUE )
        {
            aDataPlan->isNullStore = ID_TRUE;
        }
        else
        {
            sRowCnt++;
            IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                           aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                          & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );
    }

    //------------------------------
    // ���� ����
    //------------------------------

    IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr ) != IDE_SUCCESS );

    //------------------------------
    // MIN-MAX ��ȯ ����
    //------------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            aDataPlan->mtrTotalCnt++;

            IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                      != IDE_SUCCESS );

            sIsNull =
                aDataPlan->sortNode->func.isNull( aDataPlan->sortNode,
                                                  aDataPlan->plan.myTuple->row );
            if ( sIsNull == ID_TRUE )
            {
                aDataPlan->isNullStore = ID_TRUE;
            }
            else
            {
                IDE_TEST(
                    qmcSortTemp::changeMinMax( aDataPlan->sortMgr,
                                               aDataPlan->plan.myTuple->row,
                                               & aDataPlan->plan.myTuple->row )
                    != IDE_SUCCESS );
            }

            IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                                  aCodePlan->plan.left,
                                                  & sFlag ) != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2462 Result Cache
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndLMST *)(aTemplate->resultCache.data + aCodePlan->plan.offset);
        sCacheDataPlan->mtrTotalCnt = aDataPlan->mtrTotalCnt;
        sCacheDataPlan->isNullStore = aDataPlan->isNullStore;

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

#undef IDE_FN
}

IDE_RC
qmnLMST::addOneRow( qcTemplate * aTemplate,
                    qmndLMST   * aDataPlan )
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

#define IDE_FN "qmnLMST::addOneRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                  & aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( setMtrRow( aTemplate,
                         aDataPlan ) != IDE_SUCCESS );

    IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                   aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnLMST::setMtrRow( qcTemplate * aTemplate,
                    qmndLMST   * aDataPlan )
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

#define IDE_FN "qmnLMST::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
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
qmnLMST::setTupleSet( qcTemplate * aTemplate,
                      qmndLMST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLMST::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate, sNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    // To Fix PR-8017
    aDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
