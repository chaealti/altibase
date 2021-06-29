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
 * $Id: qmnWindowSort.cpp 29304 2008-11-14 08:17:42Z jakim $
 *
 * Description :
 *    WNST(WiNdow SorT) Node
 *
 * ��� ���� :
 *    ���� �ǹ̸� ������ ���� �ٸ� �ܾ �����ϸ� �Ʒ��� ����.
 *    - Analytic Funtion = Window Function
 *    - Analytic Clause = Window Clause = Over Clause
 *
 * ��� :
 *    WNST(Window Sort)
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmoUtil.h>
#include <qmnWindowSort.h>
#include <qcg.h>
#include <qmxResultCache.h>

extern mtfModule mtfRowNumber;
extern mtfModule mtfRowNumberLimit;
extern mtfModule mtfLagIgnoreNulls;
extern mtfModule mtfLeadIgnoreNulls;
extern mtfModule mtfRatioToReport;

IDE_RC
qmnWNST::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    WNST ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmncWNST * sCodePlan = (qmncWNST *) aPlan;
    qmndWNST       * sDataPlan =
        (qmndWNST *) (aTemplate->tmplate.data + aPlan->offset);
    idBool           sIsSkip = ID_FALSE;
    SLong            sNumber;
    UInt             sCodePlanFlag   = 0;
   
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnWNST::doItDefault;

    sCodePlanFlag = sCodePlan->flag;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_WNST_INIT_DONE_MASK)
         == QMND_WNST_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit( aTemplate,
                             sCodePlan,
                             sDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------------
    // Dependency�� �˻��Ͽ� �� ���� ���� ����
    //----------------------------------------
    if( sDataPlan->depValue != sDataPlan->depTuple->modify )
    {
        // Sort Manager ����
        if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
             == QMN_PLAN_STORAGE_DISK )
        {
            // ��ũ ��Ʈ ������ ���
            // ó�� ����� sortMgr�� sortMgrForDisk�� ����Ű�� ����
            // ������ �ݺ� ����� (firstInit)�� ������� �����Ƿ�
            // �̸� ����ؿ� ���� �ʱ�ȭ
            sDataPlan->sortMgr = sDataPlan->sortMgrForDisk;
        }
        else
        {
            /* PROJ-2462 Result Cache */
            if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
                 == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
            {
                sIsSkip = ID_FALSE;
            }
            else
            {
                /* PROJ-2462 Result Cache */
                sDataPlan->resultData.flag = &aTemplate->resultCache.dataFlag[sCodePlan->planID];
                if ( ( *sDataPlan->resultData.flag & QMX_RESULT_CACHE_STORED_MASK )
                     == QMX_RESULT_CACHE_STORED_TRUE )
                {
                    sIsSkip = ID_TRUE;
                }
                else
                {
                    sIsSkip = ID_FALSE;
                }
            }
        }

        if ( sIsSkip == ID_FALSE )
        {
            //----------------------------------------
            // Temp Table ���� �� �ʱ�ȭ
            //----------------------------------------
            IDE_TEST( qmcSortTemp::clear( sDataPlan->sortMgr )
                      != IDE_SUCCESS );

            //----------------------------------------
            // 1. Child�� �ݺ� �����Ͽ� Temp Table�� Insert
            //----------------------------------------
            IDE_TEST( sCodePlan->plan.left->init( aTemplate,
                                                  sCodePlan->plan.left )
                      != IDE_SUCCESS);

            /* BUG-40354 pushed rank */
            if ( ( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
                   == QMNC_WNST_STORE_LIMIT_SORTING ) ||
                 ( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
                   == QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER ) )
            {
                // BUG-48905 (BUG-40409 ����) 1024���� ū ��� ������ ���ϵȴ�.
                IDE_TEST( getMinLimitValue( aTemplate,
                                            sDataPlan->wndNode[0],
                                            & sNumber )
                          != IDE_SUCCESS );

                if ( sNumber <= QMN_WNST_PUSH_RANK_MAX )
                {
                    IDE_TEST( insertLimitedRowsFromChild( aTemplate,
                                                          sCodePlan,
                                                          sDataPlan,
                                                          sNumber )
                              != IDE_SUCCESS );
                }
                else
                {
                    // 1024�ʰ��ϸ� ������ �����ϰ� ����
                    IDE_TEST( insertRowsFromChild( aTemplate,
                                                   sCodePlan,
                                                   sDataPlan )
                              != IDE_SUCCESS );

                    // performAnalyticFunctions���� sorting�� �� �� �ֵ���
                    if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
                        == QMNC_WNST_STORE_LIMIT_SORTING )
                    {
                        sCodePlanFlag &= ~QMNC_WNST_STORE_MASK;
                        sCodePlanFlag |= QMNC_WNST_STORE_SORTING;
                    }
                    else
                    {
                        sCodePlanFlag &= ~QMNC_WNST_STORE_MASK;
                        sCodePlanFlag |= QMNC_WNST_STORE_PRESERVED_ORDER;
                    }
                }
            }
            else
            {
                IDE_TEST( insertRowsFromChild( aTemplate,
                                               sCodePlan,
                                               sDataPlan )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( performAnalyticFunctions( aTemplate,
                                            sCodePlan,
                                            sDataPlan,
                                            sCodePlanFlag )
                  != IDE_SUCCESS );

        // Temp Table ���� �� �ʱ�ȭ
        sDataPlan->depValue = sDataPlan->depTuple->modify;
    }
    else
    {
        // Nothing To Do
    }

    // doIt �Լ� ����
    sDataPlan->doIt = qmnWNST::doItFirst;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    WNST �� doIt �Լ�
 *
 * Implementation :
 *    Analytic Function ���� ����� ���������� tuple�� ������
 *
 ***********************************************************************/
    qmndWNST * sDataPlan =
        (qmndWNST *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate,
                               aPlan,
                               aFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    WNST ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/
    qmncWNST * sCodePlan = (qmncWNST *) aPlan;
    qmndWNST * sDataPlan =
        (qmndWNST *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_WNST_INIT_DONE_MASK)
         == QMND_WNST_INIT_DONE_FALSE )
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
}

IDE_RC
qmnWNST::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    WNST ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmncWNST * sCodePlan = (const qmncWNST*) aPlan;
    qmndWNST       * sDataPlan = (qmndWNST*)
        (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndWNST       * sCacheDataPlan = NULL;
    idBool           sIsInit       = ID_FALSE;

    ULong        i;
    ULong        sDiskPageCnt;
    SLong        sRecordCnt;
    UInt         sSortCount = 0;

    //----------------------------
    // SORT COUNT ����
    //----------------------------
    if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
        == QMNC_WNST_STORE_SORTING )
    {
        // �Ϲ����� ���
        sSortCount = sCodePlan->sortKeyCnt;
    }
    else if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
        == QMNC_WNST_STORE_LIMIT_SORTING )
    {
        /* pushed rank�� ��� */
        IDE_DASSERT( sCodePlan->sortKeyCnt == 1 );
        sSortCount = sCodePlan->sortKeyCnt;
    }
    else if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
             == QMNC_WNST_STORE_PRESERVED_ORDER )
    {
        // PRESERVED ORDER�� ������ ���
        sSortCount = sCodePlan->sortKeyCnt - 1;
    }
    else if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
             == QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER )
    {
        /* pushed rank�� ��� */
        IDE_DASSERT( sCodePlan->sortKeyCnt == 1 );
    }
    else if( ( sCodePlan->flag & QMNC_WNST_STORE_MASK )
             == QMNC_WNST_STORE_ONLY )
    {
        // �� OVER()���� ������ ����Ű
        IDE_DASSERT( sCodePlan->sortKeyCnt == 1 );
    }
    else
    {
        // ������� ���� �÷��� ����
        IDE_DASSERT(0);
    }
    
    //----------------------------
    // Display ��ġ ���� (�鿩����)
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString, " " );
    }

    //----------------------------
    // ���� ���� ���
    //----------------------------

    if ( aMode == QMN_DISPLAY_ALL )
    {
        //----------------------------
        // explain plan = on; �� ���
        //----------------------------

        if ( (*sDataPlan->flag & QMND_WNST_INIT_DONE_MASK)
             == QMND_WNST_INIT_DONE_TRUE )
        {
            sIsInit = ID_TRUE;
            // �ʱ�ȭ �� ���

            // BUBBUG
            // -> ������ Disk�� ��쵵 �������� Sort Mgr���� �����͸� �ű�� ����,
            // clear()�� ������ ���ε�, �� ��쵵 ������ ��ȯ���� �ʴ°�?
            // ��� Sort Temp�� ���� ������ ���ؾ� ���� �����ؾ� ��
            // Sort Temp Table�� ���� record, page ���� ������
            IDE_TEST( qmcSortTemp::getDisplayInfo( sDataPlan->sortMgr,
                                                   & sDiskPageCnt,
                                                   & sRecordCnt )
                      != IDE_SUCCESS );
            
            // Memory/Disk�� �����Ͽ� �����
            if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                 == QMN_PLAN_STORAGE_MEMORY )
            {
                if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                {
                    iduVarStringAppendFormat(
                        aString,
                        "WINDOW SORT ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "ACCESS: %"ID_UINT32_FMT", "
                        "SORT_COUNT: %"ID_UINT32_FMT,
                        sDataPlan->mtrRowSize,
                        sRecordCnt,
                        sDataPlan->plan.myTuple->modify,
                        sSortCount );
                }
                else
                {
                    // BUG-29209
                    // ITEM_SIZE ���� �������� ����
                    iduVarStringAppendFormat(
                        aString,
                        "WINDOW SORT ( "
                        "ITEM_SIZE: BLOCKED, "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "ACCESS: %"ID_UINT32_FMT", "
                        "SORT_COUNT: %"ID_UINT32_FMT,
                        sRecordCnt,
                        sDataPlan->plan.myTuple->modify,
                        sSortCount );
                }
            }
            else
            {
                if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                {
                    iduVarStringAppendFormat(
                        aString,
                        "WINDOW SORT ( "
                        "ITEM_SIZE: %"ID_UINT32_FMT", "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "DISK_PAGE_COUNT: %"ID_UINT64_FMT", "
                        "ACCESS: %"ID_UINT32_FMT", "
                        "SORT_COUNT: %"ID_UINT32_FMT,
                        sDataPlan->mtrRowSize,
                        sRecordCnt,
                        sDiskPageCnt,
                        sDataPlan->plan.myTuple->modify,
                        sSortCount );
                }
                else
                {
                    // BUG-29209
                    // ITEM_SIZE, DISK_PAGE_COUNT ���� �������� ����
                    iduVarStringAppendFormat(
                        aString,
                        "WINDOW SORT ( "
                        "ITEM_SIZE: BLOCKED, "
                        "ITEM_COUNT: %"ID_INT64_FMT", "
                        "DISK_PAGE_COUNT: BLOCKED, "
                        "ACCESS: %"ID_UINT32_FMT", "
                        "SORT_COUNT: %"ID_UINT32_FMT,
                        sRecordCnt,
                        sDataPlan->plan.myTuple->modify,
                        sSortCount );
                }
            }
            
        }
        else
        {
            // �ʱ�ȭ ���� ���� ���
            iduVarStringAppendFormat( aString,
                                      "WINDOW SORT ( ITEM_SIZE: 0, "
                                      "ITEM_COUNT: 0, ACCESS: 0, "
                                      "SORT_COUNT: %"ID_UINT32_FMT,
                                      sSortCount );
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------
        iduVarStringAppendFormat( aString,
                                  "WINDOW SORT ( ITEM_SIZE: ??, "
                                  "ITEM_COUNT: ??, ACCESS: ??, "
                                  "SORT_COUNT: %"ID_UINT32_FMT,
                                  sSortCount );
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
    // Materialize Node Info
    //----------------------------
    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        if ( ( sCodePlan->componentInfo != NULL ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
               == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
             ( ( aTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
               == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
        {
            sCacheDataPlan = (qmndWNST *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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

        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->aggrNode,
                           "aggrNode",
                           sCodePlan->myNode->dstNode->node.table,
                           sCodePlan->depTupleRowID,
                           ID_USHORT_MAX );
    }
    else
    {
        // TRCLOG_DETAIL_MTRNODE = 0 �� ���
        // �ƹ� �͵� ������� �ʴ´�.
    }
    
    //----------------------------
    // Predicate Info
    //----------------------------
    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        // TRCLOG_DETAIL_PREDICATE = 1 �� ���
        // ���� Ű�� �� ���� Ű�� analytic clause ������ ����Ѵ�
        for ( i = 0; i < aDepth+1; i++ )
        {
            iduVarStringAppend( aString, " " );
        }

        iduVarStringAppend( aString, "[ ANALYTIC FUNCTION INFO ]\n" );
        
        // �м� �Լ� ���� ���
        IDE_TEST( printAnalyticFunctionInfo( aTemplate,
                                             sCodePlan,
                                             sDataPlan,
                                             aDepth+1,
                                             aString,
                                             aMode )
                  != IDE_SUCCESS );
    }
    else
    {
        // TRCLOG_DETAIL_PREDICATE = 0 �� ���
        // �ƹ��͵� ������� �ʴ´�
    }

    //----------------------------
    // Operator�� ��� ���� ���
    //----------------------------
    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    aPlan->resultDesc )
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
}

IDE_RC
qmnWNST::doItDefault( qcTemplate * /* aTemplate */,
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
    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndWNST * sDataPlan =
        (qmndWNST *) (aTemplate->tmplate.data + aPlan->offset);
    
    void       * sOrgRow;
    void       * sSearchRow;

    // ����: �˻��ϴ� ��嵵 �ְ�, �ƴ� ��嵵 �ִµ�, �ؾ��ϳ�?
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );
    
    // ù��° ���� �˻�
    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcSortTemp::getFirstSequence( sDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;
    
    // Row ���� ���� ���� �� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        // Data�� ������ ��� Tuple Set ����
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan->mtrNode,
                               sDataPlan->plan.myTuple->row ) != IDE_SUCCESS );
        
        sDataPlan->plan.myTuple->modify++;

        sDataPlan->doIt = qmnWNST::doItNext;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
        
        sDataPlan->doIt = qmnWNST::doItFirst;
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻��� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndWNST * sDataPlan =
        (qmndWNST *) (aTemplate->tmplate.data + aPlan->offset);
    void     * sOrgRow;
    void     * sSearchRow;
    
    // ���� �˻�
    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    
    IDE_TEST( qmcSortTemp::getNextSequence( sDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // Row ���� ���� ���� �� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        // Data�� ������ ��� Tuple Set ����
        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan->mtrNode,
                               sDataPlan->plan.myTuple->row ) != IDE_SUCCESS );
        
        sDataPlan->plan.myTuple->modify++;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
        
        sDataPlan->doIt = qmnWNST::doItFirst;

    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::firstInit( qcTemplate     * aTemplate,
                    const qmncWNST * aCodePlan,
                    qmndWNST       * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Wnst Node�� Data Plan �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    UChar const * sDataArea = aTemplate->tmplate.data;
    qmndWNST    * sCacheDataPlan = NULL;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------    
    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );
    IDE_DASSERT( aCodePlan->distNodeOffset > 0 );
    IDE_DASSERT( aCodePlan->aggrNodeOffset > 0 );    
    IDE_DASSERT( aCodePlan->sortNodeOffset > 0 );
    IDE_DASSERT( aCodePlan->wndNodeOffset > 0 );
    IDE_DASSERT( aCodePlan->sortMgrOffset > 0 );

    //---------------------------------
    // Data Plan�� Data ���� �ּ� �Ҵ�
    //---------------------------------
    aDataPlan->mtrNode  = (qmdMtrNode*)  (sDataArea + aCodePlan->mtrNodeOffset);
    aDataPlan->distNode = (qmdDistNode*) (sDataArea + aCodePlan->distNodeOffset);
    aDataPlan->aggrNode = (qmdAggrNode*) (sDataArea + aCodePlan->aggrNodeOffset);    
    aDataPlan->sortNode = (qmdMtrNode**) (sDataArea + aCodePlan->sortNodeOffset);
    aDataPlan->wndNode  = (qmdWndNode**) (sDataArea + aCodePlan->wndNodeOffset);
    
    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndWNST *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
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

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_DISK  )
    {
        // DISK �� ���, sortMtrForDisk ���� ����
        aDataPlan->sortMgr        = (qmcdSortTemp*)
            (sDataArea + aCodePlan->sortMgrOffset);
        aDataPlan->sortMgrForDisk = (qmcdSortTemp*)
            (sDataArea + aCodePlan->sortMgrOffset);
    }
    else
    {
        // MEMORY�� ���
        aDataPlan->sortMgr        = (qmcdSortTemp*)
            (sDataArea + aCodePlan->sortMgrOffset);
        aDataPlan->sortMgrForDisk = NULL;
    }

    //---------------------------------
    // Data Plan ���� �ʱ�ȭ
    //---------------------------------
    
    // ���� Į�� ������ (Materialize ���) �ʱ�ȭ
    IDE_TEST( initMtrNode( aTemplate,
                           aCodePlan,
                           aDataPlan )
              != IDE_SUCCESS );

    // �ʱ�ȭ�� mtrNode ������ �̿��Ͽ� ���õ� �ٸ� ���� �ʱ�ȭ
    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );
    aDataPlan->plan.myTuple    = aDataPlan->mtrNode->dstTuple;
    aDataPlan->depTuple   = & aTemplate->tmplate.rows[aCodePlan->depTupleRowID];
    aDataPlan->depValue   = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;
    
    // Analytic Function ���ھ� DISTINCT�� �ִ� ��� ���� ����
    if( aCodePlan->distNode != NULL )
    {
        IDE_TEST( initDistNode( aTemplate,
                                aCodePlan,
                                aDataPlan->distNode,
                                & aDataPlan->distNodeCnt )
                  != IDE_SUCCESS );    
    }
    else
    {
        aDataPlan->distNodeCnt = 0;
        aDataPlan->distNode = NULL;
    }

    // Reporting Aggregation�� ó���ϴ� Į��(�߰���) ������ ����
    IDE_TEST( initAggrNode( aTemplate,
                            aCodePlan->aggrNode,
                            aDataPlan->distNode,
                            aDataPlan->distNodeCnt,
                            aDataPlan->aggrNode)
              != IDE_SUCCESS );
    
    // init sort node
    IDE_TEST( initSortNode( aCodePlan,
                            aDataPlan,
                            aDataPlan->sortNode )
              != IDE_SUCCESS );
    
    // init wnd node
    IDE_TEST( initWndNode( aCodePlan,
                           aDataPlan,
                           aDataPlan->wndNode )
              != IDE_SUCCESS );

    
    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------
    IDE_TEST( initTempTable( aTemplate,
                             aCodePlan,
                             aDataPlan,
                             aDataPlan->sortMgr )
               != IDE_SUCCESS );
    

    // ���� �ٸ� Partition�� �����ϱ� ���� DataPlan->mtrRow[2]�� �޸� ���� �Ҵ�
    IDE_TEST( allocMtrRow( aTemplate,
                           aCodePlan,
                           aDataPlan,
                           aDataPlan->mtrRow )
              != IDE_SUCCESS );

    
    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_WNST_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_WNST_INIT_DONE_TRUE;

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
qmnWNST::initMtrNode( qcTemplate     * aTemplate,
                      const qmncWNST * aCodePlan,
                      qmndWNST       * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Į�� ������ (Materialize ���) �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt        sHeaderSize = 0;

    // Memory/Disk���ο� ���� ������ temp table header size ����
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sHeaderSize = QMC_MEMSORT_TEMPHEADER_SIZE;

        /* PROJ-2462 Result Cache */
        if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
        {
            aDataPlan->mtrNode  = ( qmdMtrNode * )( aTemplate->resultCache.data +
                                                    aCodePlan->mtrNodeOffset );
            aDataPlan->sortNode = ( qmdMtrNode** )( aTemplate->resultCache.data +
                                                    aCodePlan->sortNodeOffset);
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
    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    // 2.  ���� Column�� �ʱ�ȭ
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                aCodePlan->baseTableCount )
              != IDE_SUCCESS );

    // 3.  ���� Column�� offset�� ������
    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  sHeaderSize )
              != IDE_SUCCESS );

    // 4.  Row Size�� ���
    //     - Disk Temp Table�� ��� Row�� ���� Memory�� �Ҵ����.
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initDistNode( qcTemplate     * aTemplate,
                       const qmncWNST * aCodePlan,
                       qmdDistNode    * aDistNode,
                       UInt           * aDistNodeCnt )
{
/***********************************************************************
 *
 * Description :
 *    Analytic Function ���ھ� DISTINCT�� �ִ� ��� ���� ����
 *
 * Implementation :
 *    �ٸ� ���� Column�� �޸� Distinct Argument Column������
 *    ���� ���� ���� ������ �������� �ʴ´�.
 *    �̴� �� Column������ ������ Tuple�� ����ϸ�, ���� ���� ����
 *    ���踦 ���� ���� �Ӵ��� Hash Temp Table�� ���� ���� ����
 *    ����ϱ� ���ؼ��̴�.
 *
 ***********************************************************************/
    const qmcMtrNode * sCodeNode;
    qmdDistNode      * sDistNode;
    UInt               sDistNodeCnt = 0;
    UInt               sFlag;
    UInt               sHeaderSize;
    UInt               i;
     
    //------------------------------------------------------
    // Distinct ���� Column�� �⺻ ���� ����
    // Distinct Node�� ���������� ���� ������ ���� ó���Ǹ�,
    // ���� Distinct Node���� ���� ������ �������� �ʴ´�.
    //------------------------------------------------------

    for( sCodeNode = aCodePlan->distNode,
             sDistNode = aDistNode;
         sCodeNode != NULL;
         sCodeNode = sCodeNode->next,
             sDistNode++,
             sDistNodeCnt++ )
    {
        sDistNode->myNode  = (qmcMtrNode*)sCodeNode;
        sDistNode->srcNode = NULL;
        sDistNode->next    = NULL;
    }

    *aDistNodeCnt = sDistNodeCnt;
    
    //------------------------------------------------------------
    // [Hash Temp Table�� ���� ���� ����]
    // Distinct Column�� ���� ��ü��/ Memory �Ǵ� Disk�� �� �ִ�. ��
    // ������ plan.flag�� �̿��Ͽ� �Ǻ��ϸ�, �ش� distinct column��
    // �����ϱ� ���� Tuple Set���� ������ ���� ��ü�� ����ϰ� �־��
    // �Ѵ�.  �̿� ���� ���ռ� �˻�� Hash Temp Table���� �˻��ϰ�
    // �ȴ�.
    //------------------------------------------------------------

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sFlag =
            QMCD_HASH_TMP_STORAGE_MEMORY |
            QMCD_HASH_TMP_DISTINCT_TRUE | QMCD_HASH_TMP_PRIMARY_TRUE;

        sHeaderSize = QMC_MEMHASH_TEMPHEADER_SIZE;
    }
    else
    {
        sFlag =
            QMCD_HASH_TMP_STORAGE_DISK |
            QMCD_HASH_TMP_DISTINCT_TRUE | QMCD_HASH_TMP_PRIMARY_TRUE;

        sHeaderSize = QMC_DISKHASH_TEMPHEADER_SIZE;
    }

    // PROJ-2553
    // DISTINCT Hashing�� Bucket List Hashing ����� ��� �Ѵ�.
    sFlag &= ~QMCD_HASH_TMP_HASHING_TYPE;
    sFlag |= QMCD_HASH_TMP_HASHING_BUCKET;

    //----------------------------------------------------------
    // ���� Distinct ���� Column�� �ʱ�ȭ
    //----------------------------------------------------------

    for ( i = 0, sDistNode = aDistNode;
          i < sDistNodeCnt;
          i++, sDistNode++ )
    {
        //---------------------------------------------------
        // 1. Dist Column�� ���� ���� �ʱ�ȭ
        // 2. Dist Column�� offset������
        // 3. Disk Temp Table�� ����ϴ� ��� memory ������ �Ҵ������,
        //    Dist Node�� �� ������ ��� �����Ͽ��� �Ѵ�.
        //    Memory Temp Table�� ����ϴ� ��� ������ ������ �Ҵ� ����
        //    �ʴ´�.
        // 4. Dist Column�� ���� Hash Temp Table�� �ʱ�ȭ�Ѵ�.
        //---------------------------------------------------

        IDE_TEST( qmc::initMtrNode( aTemplate,
                                    (qmdMtrNode*) sDistNode,
                                    0 )
                  != IDE_SUCCESS );

        IDE_TEST( qmc::refineOffsets( (qmdMtrNode*) sDistNode,
                                      sHeaderSize )
                  != IDE_SUCCESS );

        IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                                   & aTemplate->tmplate,
                                   sDistNode->dstNode->node.table )
                  != IDE_SUCCESS );

        // Disk Temp Table�� ����ϴ� �����
        // �� ������ ���� �ʵ��� �ؾ� �Ѵ�.
        sDistNode->mtrRow = sDistNode->dstTuple->row;
        sDistNode->isDistinct = ID_TRUE;

        IDE_TEST( qmcHashTemp::init( & sDistNode->hashMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     (qmdMtrNode*) sDistNode,  // ���� ���
                                     (qmdMtrNode*) sDistNode,  // �� ���
                                     NULL,
                                     sDistNode->myNode->bucketCnt,
                                     sFlag )
                  != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initAggrNode( qcTemplate        * aTemplate,
                       const qmcMtrNode  * aCodeNode,
                       const qmdDistNode * aDistNode,
                       const UInt          aDistNodeCnt,
                       qmdAggrNode       * aAggrNode )
{
/***********************************************************************
 *
 * Description :
 *    Reporting Aggregation�� ó���ϴ� Į��(�߰���) ������ ����
 *
 * Implementation :
 *
 ***********************************************************************/
    iduMemory         * sMemory;
    const qmcMtrNode  * sCodeNode;
    const qmdDistNode * sDistNode;
    qmdAggrNode       * sAggrNode;
    UInt                sAggrNodeCnt = 0;
    UInt                i;
    UInt                sAggrMtrRowSize;
    UInt                sAggrTupleRowID;
    
    sMemory = aTemplate->stmt->qmxMem;
    
    //-----------------------------------------------
    // ���ռ� �˻�
    //-----------------------------------------------

    //-----------------------------------------------
    // Aggregation Node�� ���� ������ �����ϰ� �ʱ�ȭ
    // �ʱ�ȭ�ϸ鼭 aggrNode�� ���� ���
    //-----------------------------------------------
    for( sCodeNode = aCodeNode,
             sAggrNode = aAggrNode;
         sCodeNode != NULL;
         sCodeNode = sCodeNode->next,
             sAggrNode = sAggrNode->next )
    {
        sAggrNode->myNode = (qmcMtrNode*)sCodeNode;
        sAggrNode->srcNode = NULL;
        sAggrNode->next = sAggrNode + 1;

        // Aggregation Node�� ���� ���
        sAggrNodeCnt++;
        
        if( sCodeNode->next == NULL )
        {
            sAggrNode->next = NULL;            
        }
    }

    IDE_DASSERT( sAggrNodeCnt != 0 );
    
    // aggregation node should not use converted source node
    //   e.g) SUM( MIN(I1) )
    //        MIN(I1)'s converted node is not aggregation node
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                (qmdMtrNode*)aAggrNode,
                                (UShort)sAggrNodeCnt )
              != IDE_SUCCESS );
    
    // Aggregation Column �� offset�� ������
    IDE_TEST( qmc::refineOffsets( (qmdMtrNode*)aAggrNode,
                                  0 ) // ������ header�� �ʿ� ����
              != IDE_SUCCESS );

    // aggrNode�� ���� tupelID
    sAggrTupleRowID = aAggrNode->dstNode->node.table;
        
    // set row size (�ʿ��� �޸� �� Ʃ�� �Ҵ�)
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               (UShort)sAggrTupleRowID )
              != IDE_SUCCESS );

    // aggrNode�� aggregation �߰� ��� ������ ���� ���� �Ҵ�
    sAggrMtrRowSize = qmc::getMtrRowSize( (qmdMtrNode*)aAggrNode );

    IDE_TEST( sMemory->cralloc( sAggrMtrRowSize,
                              (void**)&(aTemplate->tmplate.rows[sAggrTupleRowID].row))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aTemplate->tmplate.rows[sAggrTupleRowID].row == NULL,
                    err_mem_alloc );
    
    //-----------------------------------------------
    // Distinct Aggregation�� ��� �ش� Distinct Node��
    // ã�� �����Ѵ�.
    //-----------------------------------------------
    for( sAggrNode = aAggrNode;
         sAggrNode != NULL;
         sAggrNode = sAggrNode->next)
    {
        if( sAggrNode->myNode->myDist != NULL )
        {
            // Distinct Aggregation�� ���
            for( i = 0, sDistNode = aDistNode;
                 i < aDistNodeCnt;
                 i++, sDistNode++ )
            {
                if( sDistNode->myNode == sAggrNode->myNode->myDist )
                {
                    sAggrNode->myDist = (qmdDistNode*)sDistNode;
                    break;
                }
                else
                {
                    // do nothing
                }
            }
            
        }
        else
        {
            // �Ϲ� Aggregation�� ���
            sAggrNode->myDist = NULL;
        }            
        
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_mem_alloc )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::initSortNode( const qmncWNST * aCodePlan,
                       const qmndWNST * aDataPlan,
                       qmdMtrNode    ** aSortNode )
{
/***********************************************************************
 *
 * Description :
 *    ��� ����Ű�� ������ ����
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmcMtrNode * sCodeNode;
    qmdMtrNode       * sSortNode;
    const UInt         sSortKeyCnt = aCodePlan->sortKeyCnt;
    UInt               i;

    // sSortNode ��ġ �ʱ�ȭ
    // �պκп� �� ����Ű�� ���� qmdMtrNode*�� ������ ������ ������ ��ġ��
    // ����Ű�� ���� Į�� ������ ����� ���� �ּ�
    sSortNode = (qmdMtrNode*)(aSortNode + sSortKeyCnt);
    
    for( i=0; i < sSortKeyCnt; i++ )
    {
        // ���� ����Ű�� �ش��ϴ� Code Plan ���� ����
        sCodeNode = aCodePlan->sortNode[i];
        
        // ���� ����Ű�� ���� Į���� ����
        if( sCodeNode != NULL )
        {
            aSortNode[i] = sSortNode;

            //---------------------------------
            // ����Ű�� ���� Column�� �ʱ�ȭ
            //---------------------------------
            IDE_TEST( initCopiedMtrNode( aDataPlan,
                                         sCodeNode,
                                         sSortNode )
                      != IDE_SUCCESS );            

            // sSortNode �� ����
            // ������ ����Ű���� ����� ��ġ�� ����
            while( sSortNode->next != NULL )
            {
                sSortNode++;
            }
            sSortNode++;
            
        }
        else
        {
            // OVER���� ���� ����Ű�� NULL�� ����ų �� ����
            aSortNode[i] = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::initCopiedMtrNode( const qmndWNST   * aDataPlan,
                            const qmcMtrNode * aCodeNode,
                            qmdMtrNode       * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    �����Ͽ� ����ϴ� ���� Į�� ������ (Materialize ���) �ʱ�ȭ
 *    ���Ǵ� ��: ����Ű�� Į��, PARTITION BY
 *
 * Implementation :
 *
 *    �̹� initMtrNode���� �ʱ�ȭ�� ������ �ٸ� next�� ������
 *    ���� ǥ���Ͽ� �ʱ�ȭ�ϴ� ����. ���� ������ �ʱ�ȭ�� ����� ����.
 *    1. ���� Į���� ���� ���� ����
 *    2. ���� Į���� (next�� ������ ������) �˻��Ͽ� ����
 *
 ***********************************************************************/
    qmdMtrNode    * sColumnNode;
    qmdMtrNode    * sFindNode;
    qmdMtrNode    * sNextNode;

    idBool          sIsMatched;

    
    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------
    
    // 1.  ���� Column�� ���� ���� ����
    IDE_TEST( qmc::linkMtrNode( aCodeNode,
                                aDataNode ) != IDE_SUCCESS );


    // 2. �̹� �����ϴ� ��带 �˻��Ͽ� ����
    for( sColumnNode = aDataNode;
         sColumnNode != NULL;
         sColumnNode = sColumnNode->next )
    {
        sIsMatched = ID_FALSE;
        
        // mtrNode���� ������ ��带 �˻��ϴ� �κ�
        for( sFindNode = aDataPlan->mtrNode;
             sFindNode != NULL;
             sFindNode = sFindNode->next )
        {
            // �˻��ϴ� ������ �ᱹ ����Ǿ� ������ �����̹Ƿ�
            // myNode�� next�� ������ ������ ��ġ�ؾ� ��
            if( ( sColumnNode->myNode->srcNode == sFindNode->myNode->srcNode ) &&
                ( sColumnNode->myNode->dstNode == sFindNode->myNode->dstNode ) )
            {
                if ( (sColumnNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                     == QMC_MTR_SORT_ORDER_FIXED_FALSE )
                {
                    if ( (sFindNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                         == QMC_MTR_SORT_ORDER_FIXED_FALSE )
                    {
                        sIsMatched = ID_TRUE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    IDE_DASSERT( (sColumnNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                                 == QMC_MTR_SORT_ORDER_FIXED_TRUE );
                    
                    if ( (sFindNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                         == QMC_MTR_SORT_ORDER_FIXED_TRUE )
                    {
                        if ( (sColumnNode->myNode->flag & QMC_MTR_SORT_ORDER_MASK)
                             == (sFindNode->myNode->flag & QMC_MTR_SORT_ORDER_MASK) )
                        {
                            sIsMatched = ID_TRUE;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sIsMatched == ID_TRUE )
                {
                    // base table�� ���� �ȵ�
                    // ���� base table�� myNode������ ��ġ�Ѵٸ� ����
                    IDE_DASSERT( ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_MEMORY_TABLE) &&
                                 ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_DISK_TABLE) );
                    
                    sIsMatched = ID_TRUE;
                    break;
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
        }

        // DataPlan->mtrNode�� �׻� ��ġ�ϴ� Į���� �־�� ��
        IDE_TEST_RAISE( sIsMatched == ID_FALSE, ERR_COLUMN_NOT_FOUND );

        // ���� ��带 �����ϱ� ���� next ���� ����
        sNextNode = sColumnNode->next;
        
        // qmdMtrNode ����
        *sColumnNode = *sFindNode;

        // next�� ����� ����
        sColumnNode->next = sNextNode;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::initCopiedMtrNode",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initCopiedAggrNode( const qmndWNST   * aDataPlan,
                             const qmcMtrNode * aCodeNode,
                             qmdAggrNode      * aAggrNode )
{
/***********************************************************************
 *
 * Description :
 *    �����Ͽ� ����ϴ� ���� Į�� ������ (Materialize ���) �ʱ�ȭ
 *    ���Ǵ� ��: ����Ű�� Į��, PARTITION BY, AGGREGATION RESULT
 *
 * Implementation :
 *
 *    �̹� initMtrNode���� �ʱ�ȭ�� ������ �ٸ� next�� ������
 *    ���� ǥ���Ͽ� �ʱ�ȭ�ϴ� ����. ���� ������ �ʱ�ȭ�� ����� ����.
 *    1. ���� Į���� ���� ���� ����
 *    2. ���� Į���� (next�� ������ ������) �˻��Ͽ� ����
 *
 ***********************************************************************/
    const qmcMtrNode  * sCodeNode;
    qmdAggrNode       * sAggrNode;
    
    qmdAggrNode    * sFindNode;
    qmdAggrNode    * sNextNode;

    idBool          sIsMatched;

    
    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------
    
    // 1.  ���� Column�� ���� ���� ����
    for( sCodeNode = aCodeNode,
             sAggrNode = aAggrNode;
         sCodeNode != NULL;
         sCodeNode = sCodeNode->next,
             sAggrNode = sAggrNode->next)
    {
        sAggrNode->myNode = (qmcMtrNode*) sCodeNode;
        sAggrNode->srcNode = NULL;
        sAggrNode->next = sAggrNode + 1;

        if( sCodeNode->next == NULL )
        {
            sAggrNode->next = NULL;
        }
    }

    // 2. �̹� �����ϴ� ��带 �˻��Ͽ� ����
    for( sAggrNode = aAggrNode;
         sAggrNode != NULL;
         sAggrNode = sAggrNode->next )
    {
        sIsMatched = ID_FALSE;
        
        // DataPlan->aggrNode���� ������ ��带 �˻��ϴ� �κ�
        for( sFindNode = aDataPlan->aggrNode;
             sFindNode != NULL;
             sFindNode = sFindNode->next )
        {
            // �˻��ϴ� ������ �ᱹ ����Ǿ� ������ �����̹Ƿ�
            // myNode�� next�� ������ ������ ��ġ�ؾ� ��
            if( ( sAggrNode->myNode->srcNode == sFindNode->myNode->srcNode ) &&
                ( sAggrNode->myNode->dstNode == sFindNode->myNode->dstNode ) )
            {
                // aggrNode���� base table�� ���� �ȵ�
                IDE_DASSERT( ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_MEMORY_TABLE) &&
                             ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_DISK_TABLE) );
                
                sIsMatched = ID_TRUE;
                break;
            }
            else
            {
                // Nothing To Do                 
            }
        }

        // DataPlan->mtrNode�� �׻� ��ġ�ϴ� Į���� �־�� ��
        IDE_TEST_RAISE( sIsMatched == ID_FALSE, ERR_COLUMN_NOT_FOUND );

        // ���� ��带 �����ϱ� ���� next ���� ����
        sNextNode = sAggrNode->next;
        
        // qmdMtrNode ����
        *sAggrNode = *sFindNode;

        // netxt�� ����� ����
        sAggrNode->next = sNextNode;
    }
    
    return IDE_SUCCESS;
 
    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::initCopiedAggrNode",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnWNST::initAggrResultMtrNode(const qmndWNST   * aDataPlan,
                                      const qmcMtrNode * aCodeNode,
                                      qmdMtrNode       * aDataNode)
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Result ���� Į�� ���� �ʱ�ȭ
 *    ���Ǵ� ��: analytic result node
 *
 * Implementation :
 *
 *    �̹� initMtrNode���� �ʱ�ȭ�� ������ �ٸ� next�� ������
 *    ���� ǥ���Ͽ� �ʱ�ȭ�ϴ� ����. ���� ������ �ʱ�ȭ�� ����� ����.
 *    1. ���� Į���� ���� ���� ����
 *    2. ���� Į���� (next�� ������ ������) �˻��Ͽ� ����
 *
 ***********************************************************************/

    qmdMtrNode    * sDataNode;
    qmdMtrNode    * sFindNode;
    qmdMtrNode    * sNextNode;
    
    idBool          sIsMatched;

    
    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------
    
    // 1.  ���� Column�� ���� ���� ����
    IDE_TEST( qmc::linkMtrNode( aCodeNode,
                                aDataNode ) != IDE_SUCCESS );


    // 2. �̹� �����ϴ� ��带 �˻��Ͽ� ����
    for( sDataNode = aDataNode;
         sDataNode != NULL;
         sDataNode = sDataNode->next )
    {
        sIsMatched = ID_FALSE;
        
        // mtrNode���� ������ ��带 �˻��ϴ� �κ�
        for( sFindNode = aDataPlan->mtrNode;
             sFindNode != NULL;
             sFindNode = sFindNode->next )
        {
            // �˻��ϴ� ������ �ᱹ ����Ǿ� ������ �����̹Ƿ�
            // myNode�� next�� ������ ������ ��ġ�ؾ� ��
            if( ( sDataNode->myNode->srcNode == sFindNode->myNode->srcNode ) &&
                ( sDataNode->myNode->dstNode == sFindNode->myNode->dstNode ) )
            {
                // base table�� ���� �ȵ�
                // ���� base table�� myNode������ ��ġ�Ѵٸ� ����
                IDE_DASSERT( ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_MEMORY_TABLE) &&
                             ((sFindNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_DISK_TABLE) );
                
                sIsMatched = ID_TRUE;
                break;
            }
            else
            {
                // Nothing To Do                 
            }
        }

        // DataPlan->mtrNode�� �׻� ��ġ�ϴ� Į���� �־�� ��
        IDE_TEST_RAISE( sIsMatched == ID_FALSE, ERR_COLUMN_NOT_FOUND );
        
        // ���� ��带 �����ϱ� ���� next ���� ����
        sNextNode = sDataNode->next;        

        *sDataNode = *sFindNode;

        // BUG-31210 
        sDataNode->flag &= ~QMC_MTR_ANAL_FUNC_RESULT_OF_WND_NODE_MASK;
        sDataNode->flag |= QMC_MTR_ANAL_FUNC_RESULT_OF_WND_NODE_TRUE;
        IDE_TEST( qmc::setFunctionPointer( sDataNode ) != IDE_SUCCESS );

        // next�� ����� ����
        sDataNode->next = sNextNode;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::initAggrResultMtrNode",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initWndNode( const qmncWNST    * aCodePlan,
                      const qmndWNST    * aDataPlan,
                      qmdWndNode       ** aWndNode )
{
/***********************************************************************
 *
 * Description :
 *    Window Clause (Analytic Clause) ������ ��� qmdWndNode�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmcWndNode  * sCodeWndNode;
    qmdWndNode        * sDataWndNode;
    qmdMtrNode        * sMtrNode;
    UInt                sSortKeyCnt = aCodePlan->sortKeyCnt;
    UInt                i;

    const qmdWndNode  * sNodeBase;   // ��尡 ����� ���� ��ġ
    const void        * sNextNode;   // ���� ��尡 ����� ��ġ
    const qmcMtrNode  * sNode;       // ��� Ž���� ���� �ӽ� ����
    
    
    // sWNdNodeBase ��ġ �ʱ�ȭ
    // �� �κп� �� Clause�� ���� qmdWndNode*�� ������ ������ ������ ��ġ��
    // wndNode�� ���� ������ ����� ���� �ּ�
    // ���� ��� �Ʒ��� ���� ������ �����
    // [wndNode*][wndNode*][wndNode*]
    // [wndNode][overColumnNodes...][aggrNodes...][aggrResultNodes...]
    // [wndNode][overColumnNodes...][aggrNodes...][aggrResultNodes...]
    sNodeBase = (qmdWndNode*)(aWndNode + sSortKeyCnt);
    sNextNode = (void*)sNodeBase;

    // ���� ����Ű�� �����ϴ� Clause�� next�� ����Ǿ� ����
    for( i = 0;
         i < sSortKeyCnt;
         i++ )
    {
        // ���� sDataWndNode�� ��ġ ����
        sDataWndNode = (qmdWndNode*)sNextNode;

        // ���� data wnd node ��ġ�� ����
        aWndNode[i]  = sDataWndNode;

        // ����Ű�� �����ϴ� Wnd Node ����
        for( sCodeWndNode = aCodePlan->wndNode[i];
             sCodeWndNode != NULL;
             sCodeWndNode = sCodeWndNode->next,
                 sDataWndNode = sDataWndNode->next )
        {
            // ���� ��� ��ġ ����
            sNextNode    = (UChar*)sNextNode + idlOS::align8( ID_SIZEOF(qmdWndNode) );
            
            //-----------------------------------------------    
            // initOverColumnNode
            //-----------------------------------------------

            if( sCodeWndNode->overColumnNode != NULL )
            {
                // PARTITION BY�� �����ϴ� ���

                // overColumnNode��ġ �Ҵ�
                sDataWndNode->overColumnNode = (qmdMtrNode*)sNextNode;
                sDataWndNode->orderByColumnNode = NULL;

                // overColumnNode�� ����� Į�� ���� ����Ͽ� ���� ��尡 ����� ��ġ ����
                for( sNode = sCodeWndNode->overColumnNode;
                     sNode != NULL;
                     sNode = sNode->next )
                {
                    sNextNode = (UChar*)sNextNode + idlOS::align8( ID_SIZEOF(qmdMtrNode) );
                }
            
                // initOverColumnNode
                IDE_TEST( initCopiedMtrNode( aDataPlan,
                                             sCodeWndNode->overColumnNode,
                                             sDataWndNode->overColumnNode )
                          != IDE_SUCCESS );
                for ( sMtrNode = sDataWndNode->overColumnNode;
                      sMtrNode != NULL;
                      sMtrNode = sMtrNode->next )
                {
                    if ( (sMtrNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                         == QMC_MTR_SORT_ORDER_FIXED_TRUE )
                    {
                        sDataWndNode->orderByColumnNode = sMtrNode;
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            else
            {
                // �� OVER()���� ���� wndNode�� ���
                sDataWndNode->overColumnNode  = NULL;
            }            
            
            //-----------------------------------------------    
            // initAggrNode
            //-----------------------------------------------

            // aggrNode��ġ �Ҵ�
            sDataWndNode->aggrNode = (qmdAggrNode*)sNextNode;

            // aggrNode�� ����� Į���� ���� ����Ͽ� ���� ��尡 ����� ��ġ ����
            for( sNode = sCodeWndNode->aggrNode;
                 sNode != NULL;
                 sNode = sNode->next )
            {
                sNextNode = (UChar*)sNextNode + idlOS::align8( ID_SIZEOF(qmdAggrNode) );
            }
            
            // initAggrNode
            IDE_TEST( initCopiedAggrNode( aDataPlan,
                                          sCodeWndNode->aggrNode,
                                          sDataWndNode->aggrNode )
                      != IDE_SUCCESS );
            
            //-----------------------------------------------    
            // initAggrResultMtrNode
            //-----------------------------------------------
            
            // aggrResultNode��ġ �Ҵ�
            sDataWndNode->aggrResultNode = (qmdMtrNode*)sNextNode;

            // aggrResultNode�� ����� Į���� ���� ����Ͽ� ���� ��尡 ����� ��ġ ����
            for( sNode = sCodeWndNode->aggrResultNode;
                 sNode != NULL;
                 sNode = sNode->next )
            {
                sNextNode = (UChar*)sNextNode + idlOS::align8( ID_SIZEOF(qmdMtrNode) );
            }
            
            // initAggrResultNode
            IDE_TEST( initAggrResultMtrNode( aDataPlan,
                                             sCodeWndNode->aggrResultNode,
                                             sDataWndNode->aggrResultNode )
                      != IDE_SUCCESS );
            
            //-----------------------------------------------    
            // initExecMethod
            //-----------------------------------------------

            sDataWndNode->execMethod = sCodeWndNode->execMethod;
            sDataWndNode->window     = (qmcWndWindow *)&sCodeWndNode->window;

            if( sCodeWndNode->next == NULL )
            {
                // ������ ���� next�� ����
                sDataWndNode->next = NULL;
            }
            else
            {
                // WndNode->next ��ġ ����
                sDataWndNode->next = (qmdWndNode*)sNextNode;
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initTempTable( qcTemplate      * aTemplate,
                        const qmncWNST  * aCodePlan,
                        qmndWNST        * aDataPlan,
                        qmcdSortTemp    * aSortMgr )
{
/***********************************************************************
 *
 * Description :
 *    Sort Temp Table�� �ʱ�ȭ
 *
 * Implementation :
 *    Disk�� ��� ��� ����Ű�� ���� ������ Sort Manager�� �̸� �ʱ�ȭ
 *
 ***********************************************************************/
    UInt        sFlag;
    qmndWNST  * sCacheDataPlan = NULL;
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
        
        //-----------------------------
        // Temp Table �ʱ�ȭ
        //-----------------------------

        if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
        {
            // Memory Sort Temp Table�� ���, ���� Ű�� �����ϸ�
            // �ݺ� ������ �����ϹǷ� ù��° ����Ű�� ���ؼ��� �ʱ�ȭ�� ������
            IDE_TEST( qmcSortTemp::init( aSortMgr,
                                         aTemplate,
                                         ID_UINT_MAX,
                                         aDataPlan->mtrNode,
                                         aDataPlan->sortNode[0],
                                         0,
                                         sFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-2462 Result Cache */
            sCacheDataPlan = (qmndWNST *) (aTemplate->resultCache.data +
                                          aCodePlan->plan.offset);
            if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
                 == QMX_RESULT_CACHE_INIT_DONE_FALSE )
            {
                aDataPlan->sortMgr = (qmcdSortTemp*)(aTemplate->resultCache.data +
                                                     aCodePlan->sortMgrOffset);
                IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                             aTemplate,
                                             sCacheDataPlan->resultData.memoryIdx,
                                             aDataPlan->mtrNode,
                                             aDataPlan->sortNode[0],
                                             0,
                                             sFlag )
                          != IDE_SUCCESS );
                sCacheDataPlan->sortMgr = aDataPlan->sortMgr;
            }
            else
            {
                aDataPlan->sortMgr = sCacheDataPlan->sortMgr;
                IDE_TEST( qmcSortTemp::setSortNode( aDataPlan->sortMgr,
                                                    aDataPlan->sortNode[0] )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        sFlag &= ~QMCD_SORT_TMP_STORAGE_TYPE;
        sFlag |= QMCD_SORT_TMP_STORAGE_DISK;

        /* PROJ-2201 
         * ������, BackwardScan���� �Ϸ��� RangeFlag�� ����� */
        sFlag &= ~QMCD_SORT_TMP_SEARCH_MASK;
        sFlag |= QMCD_SORT_TMP_SEARCH_RANGE;

        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_DISK );
        
        //-----------------------------
        // Temp Table �ʱ�ȭ
        //-----------------------------

        // Disk Sort Temp Table�� ���,
        // ��� ���� Ű�� ���� �̸� �ʱ�ȭ�� ������
        IDE_TEST( qmcSortTemp::init( aSortMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->sortNode[0],
                                     aCodePlan->storeRowCount,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::setMtrRow( qcTemplate     * aTemplate,
                    qmndWNST       * aDataPlan )
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
}

IDE_RC
qmnWNST::setTupleSet( qcTemplate   * aTemplate,
                      qmdMtrNode   * aMtrNode,
                      void         * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmdMtrNode * sNode;

    for ( sNode = aMtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate,
                                        sNode,
                                        aRow )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::performAnalyticFunctions( qcTemplate     * aTemplate,
                                   const qmncWNST * aCodePlan,
                                   qmndWNST       * aDataPlan,
                                   UInt             aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ��� Analytic Function�� �����Ͽ� Temp Table�� �����Ѵ�
 *
 * Implementation :
 *    1. Child�� �ݺ� �����Ͽ� Temp Table�� Insert
 *    2. ù ��° ���� Ű�� ���� sort() ����
 *    3. Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
 *    4. ���� ����Ű�� �� �̻��̶�� �Ʒ��� �ݺ�
 *    4.1. ����Ű�� ����
 *    4.2. ����� ����Ű�� ���� �ٽ� ������ ����
 *    4.3. Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
 *
 ***********************************************************************/
    UInt         i;

    // Sort Manager ����
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_DISK )
    {
        // ��ũ ��Ʈ ������ ���
        // ó�� ����� sortMgr�� sortMgrForDisk�� ����Ű�� ����
        // ������ �ݺ� ����� (firstInit)�� ������� �����Ƿ�
        // �̸� ����ؿ� ���� �ʱ�ȭ
        aDataPlan->sortMgr = aDataPlan->sortMgrForDisk;
    }
    else
    {
        // �޸��� ��� ������ ����
    }

    
    //----------------------------------------
    // 2. ù ��° ���� Ű�� ���� sort() ����
    //----------------------------------------
    if( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
        == QMN_PLAN_STORAGE_DISK )
    {
        // disk�� ��� SORTING or PRESEVED_ORDER �� ��� ������ ��
        if( ( ( aFlag & QMNC_WNST_STORE_MASK )
              == QMNC_WNST_STORE_SORTING ) ||
            ( ( aFlag & QMNC_WNST_STORE_MASK )
              == QMNC_WNST_STORE_PRESERVED_ORDER ) )
        {
            IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        // memory�� ��� SORTING �϶��� ������ ��
        if( ( aFlag & QMNC_WNST_STORE_MASK )
               == QMNC_WNST_STORE_SORTING )
        {
            IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2462 Result Cache
            if ( ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
                   == QMN_PLAN_RESULT_CACHE_EXIST_TRUE ) &&
                 ( ( aFlag & QMNC_WNST_STORE_MASK )
                     == QMNC_WNST_STORE_PRESERVED_ORDER ) )
            {
                if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_USE_PRESERVED_ORDER_MASK )
                     == QMX_RESULT_CACHE_USE_PRESERVED_ORDER_TRUE )
                {
                    IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( aCodePlan->sortKeyCnt > 1 )
                    {
                        *aDataPlan->resultData.flag &= ~QMX_RESULT_CACHE_USE_PRESERVED_ORDER_MASK;
                        *aDataPlan->resultData.flag |= QMX_RESULT_CACHE_USE_PRESERVED_ORDER_TRUE;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    //----------------------------------------
    // 3. Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
    //----------------------------------------
    IDE_TEST( aggregateAndUpdate( aTemplate,
                                  aDataPlan,
                                  aDataPlan->wndNode[0] )
              != IDE_SUCCESS);

    
    //----------------------------------------
    // 4. ���� ����Ű�� �� �̻��̶�� �Ʒ��� �ݺ�
    //----------------------------------------
    for( i = 1;
         i < aCodePlan->sortKeyCnt;
         i++ )
    {
        // 4.1. ����Ű�� ����
        IDE_TEST( qmcSortTemp::setSortNode( aDataPlan->sortMgr,
                                            aDataPlan->sortNode[i] )
                  != IDE_SUCCESS );

        // 4.2. ����� ����Ű�� ���� �ٽ� ������ ����
        // �� ��° ���� ����Ű�� PRESERVED ORDER�� ������� �����Ƿ� ������ ����
        IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        // 4.3. Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
        IDE_TEST( aggregateAndUpdate( aTemplate,
                                      aDataPlan,
                                      aDataPlan->wndNode[i] )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::insertRowsFromChild( qcTemplate     * aTemplate,
                              const qmncWNST * aCodePlan,
                              qmndWNST       * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child�� �ݺ� �����Ͽ� Temp Table�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcRowFlag   sFlag = QMC_ROW_INITIALIZE;
    
    //------------------------------
    // Child Record�� ����
    //------------------------------

    // aggrNode�� �ʱ� ���� ����
    // aggregation���� execution ���� init�� �ٷ� finalize�ϸ�
    // grouping�� NULL�� ����� ���� ������
    
    

    // Child ����
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag )
              != IDE_SUCCESS );
    
    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // ������ �Ҵ�
        IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                      & aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        // ���� Row�� ����
        IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );
        
        // Row�� ����
        IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                       aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        // Child ����
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag )
                  != IDE_SUCCESS );
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
qmnWNST::insertLimitedRowsFromChild( qcTemplate     * aTemplate,
                                     const qmncWNST * aCodePlan,
                                     qmndWNST       * aDataPlan,
                                     SLong            aLimitNum )
{
/***********************************************************************
 *
 * Description :
 *    Child�� �ݺ� �����Ͽ� ���� n���� rocord�� ���� memory temp�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    qmcRowFlag   sFlag = QMC_ROW_INITIALIZE;
    SLong        sCount = 0;

    IDE_TEST_RAISE( aCodePlan->sortKeyCnt != 1,
                    ERR_INVALID_KEY_COUNT );
    
    //------------------------------
    // Child Record�� ����
    //------------------------------

    // Child ����
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );
    
    while ( ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST ) &&
            ( sCount < aLimitNum ) )
    {
        sCount++;
        
        // ������ �Ҵ�
        IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                      & aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        // ���� Row�� ����
        IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );
        
        // Row�� ����
        IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                       aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        // Child ����
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag )
                  != IDE_SUCCESS );
    }
    
    //------------------------------
    // ���� ����
    //------------------------------

    if ( ( aCodePlan->flag & QMNC_WNST_STORE_MASK )
         == QMNC_WNST_STORE_LIMIT_SORTING )
    {
        IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        //------------------------------
        // Limit Sorting ����
        //------------------------------

        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            // ������ �Ҵ�
            IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                          & aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                // ���� Row�� ����
                IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                          != IDE_SUCCESS );

                IDE_TEST( qmcSortTemp::shiftAndAppend( aDataPlan->sortMgr,
                                                       aDataPlan->plan.myTuple->row,
                                                       & aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );

                IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                                      aCodePlan->plan.left,
                                                      & sFlag )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER */
        
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_KEY_COUNT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::insertLimitedRowsFromChild",
                                  "Invalid key count" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::aggregateAndUpdate( qcTemplate       * aTemplate,
                             qmndWNST         * aDataPlan,
                             const qmdWndNode * aWndNode )
{
/***********************************************************************
 *
 * Description :
 *    Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmdWndNode    * sWndNode;

    for( sWndNode = aWndNode;
         sWndNode != NULL;
         sWndNode = sWndNode->next )
    {
        switch ( sWndNode->execMethod )
        {
            case QMC_WND_EXEC_PARTITION_ORDER_UPDATE:
            {
                // partition by�� order by�� �Բ� �ִ� ���
                IDE_TEST( partitionOrderByAggregation( aTemplate,
                                                       aDataPlan,
                                                       sWndNode->overColumnNode,
                                                       sWndNode->aggrNode,
                                                       sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;   
            }

            case QMC_WND_EXEC_PARTITION_UPDATE:
            {
                // partition by�� �ִ� ���
                IDE_TEST( partitionAggregation( aTemplate,
                                                aDataPlan,
                                                sWndNode->overColumnNode,
                                                sWndNode->aggrNode,
                                                sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }

            case QMC_WND_EXEC_ORDER_UPDATE:
            {
                // order by�� �ִ� ���
                IDE_TEST( orderByAggregation( aTemplate,
                                            aDataPlan,
                                            sWndNode->overColumnNode,
                                            sWndNode->aggrNode,
                                            sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }

            case QMC_WND_EXEC_AGGR_UPDATE:
            {
                // �� over���� ���
                IDE_TEST( aggregationOnly( aTemplate,
                                           aDataPlan,
                                           sWndNode->aggrNode,
                                           sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }

            case QMC_WND_EXEC_PARTITION_ORDER_WINDOW_UPDATE:
            {
                IDE_TEST( windowAggregation( aTemplate,
                                             aDataPlan,
                                             sWndNode->window,
                                             sWndNode->overColumnNode,
                                             sWndNode->orderByColumnNode,
                                             sWndNode->aggrNode,
                                             sWndNode->aggrResultNode,
                                             ID_TRUE )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_ORDER_WINDOW_UPDATE:
            {
                IDE_TEST( windowAggregation( aTemplate,
                                             aDataPlan,
                                             sWndNode->window,
                                             sWndNode->overColumnNode,
                                             sWndNode->orderByColumnNode,
                                             sWndNode->aggrNode,
                                             sWndNode->aggrResultNode,
                                             ID_FALSE )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LAG:
            {
                IDE_TEST( partitionOrderByLagAggr( aTemplate,
                                                   aDataPlan,
                                                   sWndNode->overColumnNode,
                                                   sWndNode->aggrNode,
                                                   sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LEAD:
            {
                IDE_TEST( partitionOrderByLeadAggr( aTemplate,
                                                    aDataPlan,
                                                    sWndNode->overColumnNode,
                                                    sWndNode->aggrNode,
                                                    sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_ORDER_UPDATE_LAG:
            {
                IDE_TEST( orderByLagAggr( aTemplate,
                                          aDataPlan,
                                          sWndNode->aggrNode,
                                          sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_ORDER_UPDATE_LEAD:
            {
                IDE_TEST( orderByLeadAggr( aTemplate,
                                           aDataPlan,
                                           sWndNode->aggrNode,
                                           sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_PARTITION_ORDER_UPDATE_NTILE:
            {
                IDE_TEST( partitionOrderByNtileAggr( aTemplate,
                                                     aDataPlan,
                                                     sWndNode->overColumnNode,
                                                     sWndNode->aggrNode,
                                                     sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            case QMC_WND_EXEC_ORDER_UPDATE_NTILE:
            {
                IDE_TEST( orderByNtileAggr( aTemplate,
                                            aDataPlan,
                                            sWndNode->aggrNode,
                                            sWndNode->aggrResultNode )
                          != IDE_SUCCESS );
                break;
            }
            default:
            {
                IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                break;
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_EXEC_METHOD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::aggregateAndUpdate",
                                  "Invalid exec method" ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmnWNST::aggregationOnly( qcTemplate        * aTemplate,
                          qmndWNST          * aDataPlan,
                          const qmdAggrNode * aAggrNode,
                          const qmdMtrNode  * aAggrResultNode )
{
/***********************************************************************
 *
 * Description :
 *    ��Ƽ���� �������� ���� ��� ��ü�� ���� aggregation�� �����ϰ�,
 *    �� (����) ����� Sort Temp�� �ݿ���
 *
 * Implementation :
 *    1. ���� ��Ƽ�ǿ� ���� aggregation ����
 *    2. Aggregation ����� Sort Temp�� �ݿ� (update)
 *
 ***********************************************************************/
    qmcRowFlag         sFlag = QMC_ROW_INITIALIZE;
    qmdMtrNode       * sNode;
    mtcRankValueType   sRankValue;
    
    //---------------------------------
    // set update columns
    //---------------------------------
    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );


    //----------------------------------------
    // 1. ���� ��Ƽ�ǿ� ���� aggregation ����
    //----------------------------------------    

    //---------------------------------
    // ù ��° ���ڵ带 ������
    //---------------------------------

    // ���� row ����
    aDataPlan->mtrRowIdx = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate,
                              aDataPlan,
                              & sFlag )
              != IDE_SUCCESS );

    if( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //---------------------------------
        // clearDistNode
        //---------------------------------
        IDE_TEST( clearDistNode( aDataPlan->distNode,
                                 aDataPlan->distNodeCnt )
                  != IDE_SUCCESS );
        
        //---------------------------------
        // initAggregation
        //---------------------------------
        IDE_TEST( initAggregation( aTemplate,
                                   aAggrNode )
                  != IDE_SUCCESS );

        sRankValue = MTC_RANK_VALUE_FIRST;
        
        do
        {
            //---------------------------------
            // execAggregation
            //---------------------------------
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       (void*)&sRankValue,
                                       aDataPlan->distNode,
                                       aDataPlan->distNodeCnt )
                      != IDE_SUCCESS );

            //---------------------------------
            // ���� ���ڵ带 ������
            //---------------------------------

            // ���� row ����
            aDataPlan->mtrRowIdx = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            
            IDE_TEST( getNextRecord( aTemplate,
                                     aDataPlan,
                                     & sFlag )
                      != IDE_SUCCESS );

            //---------------------------------
            // ���ڵ尡 �����ϸ� �ݺ�
            //---------------------------------
        }
        while( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST );

        //---------------------------------
        // finiAggregation
        //---------------------------------
        IDE_TEST( finiAggregation( aTemplate,
                                   aAggrNode )
                  != IDE_SUCCESS );

        //----------------------------------------
        // 2. Aggregation ����� Sort Temp�� �ݿ�
        //----------------------------------------
        
        //---------------------------------
        // �ٽ� ù ��° ���ڵ带 ������
        //---------------------------------

        // ���� row ����
        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        
        IDE_TEST( getFirstRecord( aTemplate,
                                  aDataPlan,
                                  & sFlag )
                  != IDE_SUCCESS );
        
        do
        {
            //---------------------------------
            // UPDATE
            //---------------------------------

            // ���� Row�� ����
            for ( sNode = (qmdMtrNode*)aAggrResultNode;
                  sNode != NULL;
                  sNode = sNode->next )
            {
                /* BUG-43087 support ratio_to_report
                 * RATIO_TO_REPORT �Լ��� finalize���� ������ �����ϱ� ������
                 * Aggretation�� Result�� �����ϱ� �� ���� row�� ���� �� finalize
                 * �� �����Ѵ�.
                 */
                if ( sNode->srcNode->node.module == &mtfRatioToReport )
                {
                    IDE_TEST( qtc::finalize( sNode->srcNode, aTemplate )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( sNode->func.setMtr( aTemplate,
                                              sNode,
                                              aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );
            }

            // update()
            IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                      != IDE_SUCCESS );
            

            //---------------------------------
            // ���� ���ڵ带 ������
            //---------------------------------

            // ���� row ����
            aDataPlan->mtrRowIdx = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            
            IDE_TEST( getNextRecord( aTemplate,
                                     aDataPlan,
                                     & sFlag )
                      != IDE_SUCCESS );


            //---------------------------------
            // ���ڵ尡 �����ϸ� �ݺ�
            //---------------------------------
        }
        while( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST );
        
    }
    else
    {
        // ���ڵ尡 �ϳ��� ���� ��� ������ ����
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::partitionAggregation( qcTemplate        * aTemplate,
                                     qmndWNST    * aDataPlan,
                               const qmdMtrNode  * aOverColumnNode,
                               const qmdAggrNode * aAggrNode,
                               const qmdMtrNode  * aAggrResultNode )
{
/***********************************************************************
 *
 * Description :
 *    ��Ƽ�� ���� aggregation�� �����ϰ�, �� ����� Sort Temp�� �ݿ���
 *
 * Implementation :
 *    1. ���� ��Ƽ�ǿ� ���� aggregation ����
 *    2. Aggregation ����� Sort Temp�� �ݿ� (update)
 *
 ***********************************************************************/
    qmcRowFlag        sFlag = QMC_ROW_INITIALIZE;
    qmdMtrNode      * sNode;
    SLong             sExecAggrCnt = 0;  // execAggregation()�� ������ ī��Ʈ
    mtcRankValueType  sRankValue;

    //---------------------------------
    // set update columns
    //---------------------------------
    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    
    //----------------------------------------
    // 1. ���� ��Ƽ�ǿ� ���� aggregation ����
    //----------------------------------------    

    //---------------------------------
    // ù ��° ���ڵ带 ������
    //---------------------------------

    // ���� row ����
    aDataPlan->mtrRowIdx = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate,
                              aDataPlan,
                              & sFlag )
              != IDE_SUCCESS );

    if( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // ���ڵ尡 �����ϸ� �Ʒ��� �ݺ�
        do
        {   
            //---------------------------------
            // store cursor
            //---------------------------------
            // ���� ��ġ�� Ŀ���� ������
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                & aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
        
            //---------------------------------
            // clearDistNode
            //---------------------------------
            IDE_TEST( clearDistNode( aDataPlan->distNode,
                                     aDataPlan->distNodeCnt )
                      != IDE_SUCCESS );
        
            //---------------------------------
            // initAggregation
            //---------------------------------
            IDE_TEST( initAggregation( aTemplate,
                                       aAggrNode )
                      != IDE_SUCCESS );

            sExecAggrCnt = 0;
            sRankValue = MTC_RANK_VALUE_FIRST;
            
            do
            {
                //---------------------------------
                // execAggregation
                //---------------------------------
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           (void*)&sRankValue,
                                           aDataPlan->distNode,
                                           aDataPlan->distNodeCnt )
                          != IDE_SUCCESS );

                sExecAggrCnt++;

                //---------------------------------
                // ���� ���ڵ带 ������
                //---------------------------------

                // ���� row ����
                aDataPlan->mtrRowIdx = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate,
                                         aDataPlan,
                                         & sFlag )
                          != IDE_SUCCESS );

                //---------------------------------
                // ���� ��Ƽ������ �˻�
                //---------------------------------

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           & sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Data�� ������ ����
                    break;
                }

                //---------------------------------
                // ���� ������ �˻�
                //---------------------------------
                
                if ( (sFlag & QMC_ROW_COMPARE_MASK) == QMC_ROW_COMPARE_SAME )
                {
                    sRankValue = MTC_RANK_VALUE_SAME;
                }
                else
                {
                    sRankValue = MTC_RANK_VALUE_DIFF;
                }

                // ���� ��Ƽ���̸� �ݺ�
            }
            while( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME );
            
            
            //---------------------------------
            // finiAggregation
            //---------------------------------
            IDE_TEST( finiAggregation( aTemplate,
                                       aAggrNode )
                      != IDE_SUCCESS );

            
            //----------------------------------------
            // 2. Aggregation ����� Sort Temp�� �ݿ�
            //----------------------------------------

            //---------------------------------
            // restore cursor
            //---------------------------------

            // ���� ��ġ�� Ŀ���� ������ ��ġ�� ������Ŵ
            // ���� row ����
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            
            // �˻��� Row�� �̿��� Tuple Set ����
            // getFirst & NextRecord �Լ��� setTupleSet ����� �����ϰ� �־� �ʿ䰡 ����,
            // restoreCursor�� ȣ���� ��쿡�� setTupleSet�� �Բ� ȣ���ؾ� ��
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            
            do
            {
                //---------------------------------
                // UPDATE
                //---------------------------------
                
                // ���� Row�� ����
                for ( sNode = (qmdMtrNode*)aAggrResultNode;
                      sNode != NULL;
                      sNode = sNode->next )
                {
                    /* BUG-43087 support ratio_to_report
                     * RATIO_TO_REPORT �Լ��� finalize���� ������ �����ϱ� ������
                     * Aggretation�� Result�� �����ϱ� �� ���� row�� ���� �� finalize
                     * �� �����Ѵ�.
                     */
                    if ( sNode->srcNode->node.module == &mtfRatioToReport )
                    {
                        IDE_TEST( qtc::finalize( sNode->srcNode, aTemplate )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    IDE_TEST( sNode->func.setMtr( aTemplate,
                                                  sNode,
                                                  aDataPlan->plan.myTuple->row )
                              != IDE_SUCCESS );
                }

                // update()
                IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                          != IDE_SUCCESS );

                sExecAggrCnt--;

                //---------------------------------
                // ���� ��Ƽ������ �˻�
                //---------------------------------

                IDE_DASSERT( sExecAggrCnt >= 0 );
                
                if( sExecAggrCnt > 0 )
                {
                    // ���� row ����
                    aDataPlan->mtrRowIdx = 1;
                    aDataPlan->plan.myTuple->row =
                        aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    // ���� row ����
                    aDataPlan->mtrRowIdx = 0;
                    aDataPlan->plan.myTuple->row =
                        aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                }

                //---------------------------------
                // ���� ��Ƽ���� ���,
                // ���� ���ڵ带 ������
                //---------------------------------
                IDE_TEST( getNextRecord( aTemplate,
                                         aDataPlan,
                                         & sFlag )
                          != IDE_SUCCESS );

                if( sExecAggrCnt > 0 )
                {
                    sFlag &= ~QMC_ROW_GROUP_MASK;
                    sFlag |= QMC_ROW_GROUP_SAME;
                }
                else
                {
                    sFlag &= ~QMC_ROW_GROUP_MASK;
                    sFlag |= QMC_ROW_GROUP_NULL;
                }
                // ���ڵ尡 �����ϰ�, ���� ��Ƽ���̸� �ݺ�
            }
            while( ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST ) &&
                   ( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME ) );

            
            // ���ڵ尡 �����ϰ�, �ٸ� ��Ƽ���̸� ���ο� aggregation�� ����
        }
        while( ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST ) &&
               ( (sFlag & QMC_ROW_GROUP_MASK) != QMC_ROW_GROUP_SAME ) );
    }
    else
    {
        // ���ڵ尡 �ϳ��� ���� ��� ������ ����
    }

    aDataPlan->mtrRowIdx = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Partition By Order By Aggregation
 *
 *   partition By order by RANGE betwwen UNBOUNDED PRECEDING and CURRENT ROW
 *   �� ���� ������ �̷��� ������ ���������� Ranking���� �Լ��� ���� ����.
 */
IDE_RC qmnWNST::partitionOrderByAggregation( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    mtcRankValueType   sRankValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
        sExecAggrCnt = 0;
        sRankValue   = MTC_RANK_VALUE_FIRST;

        do
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       (void*)&sRankValue,
                                       aDataPlan->distNode,
                                       aDataPlan->distNodeCnt )
                      != IDE_SUCCESS );

            sExecAggrCnt++;
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }

            if ( (sFlag & QMC_ROW_COMPARE_MASK) == QMC_ROW_COMPARE_SAME )
            {
                sRankValue = MTC_RANK_VALUE_SAME;
            }
            else
            {
                sRankValue = MTC_RANK_VALUE_DIFF;
                if ( sExecAggrCnt > 0 )
                {
                    IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                              != IDE_SUCCESS );

                    IDE_TEST( updateAggrRows( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              NULL,
                                              sExecAggrCnt )
                              != IDE_SUCCESS );
                    sExecAggrCnt = 0;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

        if ( sExecAggrCnt > 0 )
        {
             IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                       != IDE_SUCCESS );

             IDE_TEST( updateAggrRows( aTemplate,
                                       aDataPlan,
                                       aAggrResultNode,
                                       &sFlag,
                                       sExecAggrCnt )
                       != IDE_SUCCESS );
             sExecAggrCnt = 0;
         }
         else
         {
             IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                       != IDE_SUCCESS );
         }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * update Aggregate Rows
 *
 *  Sort Temp�� aExecAggrCount ��ŭ aggregate �� ���� update �Ѵ�.
 *
 *  partition by order by �� �ǹ̴� Window�� Range�� ������ ������ �ִ�.
 *
 *  �׷��� Aggrete �� ���� ��� ���� ������ update�ϴµ� Rownumber�� ���� ���� ����
 *  �����ϱ� ������ update �ٷ����� Aggregate�� �����ؼ� update�� �����Ѵ�.
 *
 *  �ٸ� �Լ��� ��� �̹� Aggregate �� ���� aExecAggrCount ��ŭ update �Ѵ�.
 */
IDE_RC qmnWNST::updateAggrRows( qcTemplate * aTemplate,
                                qmndWNST   * aDataPlan,
                                qmdMtrNode * aAggrResultNode,
                                qmcRowFlag * aFlag,
                                SLong        aExecAggrCount )
{
    qmdMtrNode * sNode;
    SLong        sUpdateCount = 0;
    qmcRowFlag   sFlag        = QMC_ROW_INITIALIZE;

    aDataPlan->mtrRowIdx = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                          &aDataPlan->cursorInfo )
              != IDE_SUCCESS );

    aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

    IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    while ( sUpdateCount < aExecAggrCount )
    {
        for ( sNode = aAggrResultNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            /* mtfRowNumber�� Aggregate�� �����Ѵ� */
            if ( ( sNode->srcNode->node.module == &mtfRowNumber ) ||
                 ( sNode->srcNode->node.module == &mtfRowNumberLimit ) )
            {
                IDE_TEST( qtc::aggregate( sNode->srcNode,
                                          aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
            
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        /* SortTemp �� Update�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                  != IDE_SUCCESS );
        sUpdateCount++;

        if ( sUpdateCount < aExecAggrCount )
        {
            aDataPlan->mtrRowIdx = 1;
            aDataPlan->plan.myTuple->row =
                aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }
        else
        {
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row =
                aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }

        IDE_TEST( getNextRecord( aTemplate,
                                 aDataPlan,
                                 &sFlag )
                  != IDE_SUCCESS );
    }

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aFlag != NULL )
    {
        *aFlag = sFlag;
    }
    else
    {
        /* Nothign to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 *  Order By Aggregation
 *
 *   order by RANGE betwwen UNBOUNDED PRECEDING and CURRENT ROW
 *   �� ����. ������ �̷��� ������ ���������� Ranking���� �Լ��� ���� ����.
 */
IDE_RC qmnWNST::orderByAggregation( qcTemplate  * aTemplate,
                                    qmndWNST    * aDataPlan,
                                    qmdMtrNode  * aOverColumnNode,
                                    qmdAggrNode * aAggrNode,
                                    qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    mtcRankValueType   sRankValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        do
        {
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            sExecAggrCnt = 0;
            sRankValue   = MTC_RANK_VALUE_FIRST;

            do
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           (void*)&sRankValue,
                                           aDataPlan->distNode,
                                           aDataPlan->distNodeCnt )
                          != IDE_SUCCESS );

                sExecAggrCnt++;
                aDataPlan->mtrRowIdx = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }

                if ( (sFlag & QMC_ROW_COMPARE_MASK) == QMC_ROW_COMPARE_SAME )
                {
                    sRankValue = MTC_RANK_VALUE_SAME;
                }
                else
                {
                    sRankValue = MTC_RANK_VALUE_DIFF;
                }

            } while ( ( sFlag & QMC_ROW_COMPARE_MASK ) == QMC_ROW_COMPARE_SAME );

            if ( sExecAggrCnt > 0 )
            {
                IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                          != IDE_SUCCESS );

                IDE_TEST( updateAggrRows( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag,
                                          sExecAggrCnt )
                          != IDE_SUCCESS );
                sExecAggrCnt = 0;
            }
            else
            {
                /* Nothing to do */
            }
        } while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
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
qmnWNST::clearDistNode( qmdDistNode * aDistNode,
                        const UInt    aDistNodeCnt )
{
/***********************************************************************
 *
 * Description :
 *    Distinct Column�� ���� Temp Table�� Clear
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt  i;
    qmdDistNode * sDistNode;

    for ( i = 0, sDistNode = aDistNode;
          i < aDistNodeCnt;
          i++, sDistNode++ )
    {
        IDE_TEST( qmcHashTemp::clear( & sDistNode->hashMgr )
                  != IDE_SUCCESS );
        sDistNode->mtrRow = sDistNode->dstTuple->row;
        sDistNode->isDistinct = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::initAggregation( qcTemplate        * aTemplate,
                          const qmdAggrNode * aAggrNode )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmdAggrNode * sNode;
    
    for ( sNode = aAggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( qtc::initialize( sNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::execAggregation( qcTemplate         * aTemplate,
                          const qmdAggrNode  * aAggrNode,
                          void               * aAggrInfo,
                          qmdDistNode        * aDistNode,
                          const UInt           aDistNodeCnt )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmdAggrNode * sAggrNode;

    // BUG-42277
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // set distinct column and insert to hash(DISTINCT)
    IDE_TEST( setDistMtrColumns( aTemplate, aDistNode, aDistNodeCnt )
              != IDE_SUCCESS );

    for ( sAggrNode = aAggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        if ( ( sAggrNode->dstNode->node.module == &mtfRowNumber ) ||
             ( sAggrNode->dstNode->node.module == &mtfRowNumberLimit ) )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }
        
        if ( sAggrNode->myDist == NULL )
        {
            // Non Distinct Aggregation�� ���
            IDE_TEST( qtc::aggregateWithInfo( sAggrNode->dstNode,
                                              aAggrInfo,
                                              aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            // Distinct Aggregation�� ���
            if ( sAggrNode->myDist->isDistinct == ID_TRUE )
            {
                // Distinct Argument�� ���
                IDE_TEST( qtc::aggregateWithInfo( sAggrNode->dstNode,
                                                  aAggrInfo,
                                                  aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // Non-Distinct Argument�� ���
                // Aggregation�� �������� �ʴ´�.
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::setDistMtrColumns( qcTemplate        * aTemplate,
                            qmdDistNode       * aDistNode,
                            const UInt          aDistNodeCnt )
{
/***********************************************************************
 *
 * Description :
 *     Distinct Column�� �����Ѵ�.
 *
 * Implementation :
 *     Memory ������ �Ҵ� �ް�, Distinct Column�� ����
 *     Hash Temp Table�� ������ �õ��Ѵ�.
 *
 ***********************************************************************/
    UInt i;
    qmdDistNode * sDistNode;

    for ( i = 0, sDistNode = aDistNode;
          i < aDistNodeCnt;
          i++, sDistNode++ )
    {
        if ( sDistNode->isDistinct == ID_TRUE )
        {
            // ���ο� �޸� ������ �Ҵ�
            // Memory Temp Table�� ��쿡�� ���ο� ������ �Ҵ�޴´�.
            IDE_TEST( qmcHashTemp::alloc( & sDistNode->hashMgr,
                                          & sDistNode->mtrRow )
                      != IDE_SUCCESS );

            sDistNode->dstTuple->row = sDistNode->mtrRow;
        }
        else
        {
            // To Fix PR-8556
            // ���� �޸𸮸� �״�� ����� �� �ִ� ���
            sDistNode->mtrRow = sDistNode->dstTuple->row;
        }

        // Distinct Column�� ����
        IDE_TEST( sDistNode->func.setMtr( aTemplate,
                                          (qmdMtrNode*) sDistNode,
                                          sDistNode->mtrRow ) != IDE_SUCCESS );

        // Hash Temp Table�� ����
        // Is Distinct�� ����� ���� ���� ���θ� �Ǵ��� �� �ִ�.
        IDE_TEST( qmcHashTemp::addDistRow( & sDistNode->hashMgr,
                                           & sDistNode->mtrRow,
                                           & sDistNode->isDistinct )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC
qmnWNST::finiAggregation( qcTemplate        * aTemplate,
                          const qmdAggrNode * aAggrNode )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation�� ������
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmdAggrNode * sAggrNode;

    for ( sAggrNode = aAggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        /* BUG-43087 support ratio_to_report
         * RATIO_TO_REPORT �Լ��� finalize���� ������ �����ϱ� ������
         * Aggretation�� Result�� �����ϱ� �� ���� row�� ���Ҷ� finalize
         * �� �����Ѵ�.
         */
        if ( sAggrNode->dstNode->node.module != &mtfRatioToReport )
        {
            IDE_TEST( qtc::finalize( sAggrNode->dstNode, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::compareRows( const qmndWNST   * aDataPlan,
                      const qmdMtrNode * aMtrNode,
                      qmcRowFlag       * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Window Sort�� ������ �����
 *
 * Implementation :
 *    1. over column���� partition by column��� ���� ��Ƽ���� ���Ѵ�.
 *    2. over column���� order by column��� ���� ���� ���Ѵ�.
 *
 ***********************************************************************/
    const qmdMtrNode * sNode;
    SInt               sPartCompResult;
    SInt               sOrderCompResult;
    mtdValueInfo       sValueInfo1;
    mtdValueInfo       sValueInfo2;

    //------------------------
    // Partition Compare
    //------------------------

    sPartCompResult  = -1;
    sOrderCompResult = -1;
    
    for( sNode = aMtrNode;
         sNode != NULL;
         sNode = sNode->next )
    {
        sValueInfo1.value  = sNode->func.getRow( (qmdMtrNode*)sNode, aDataPlan->mtrRow[0]);
        sValueInfo1.column = (const mtcColumn *) sNode->func.compareColumn;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.value  = sNode->func.getRow( (qmdMtrNode*)sNode, aDataPlan->mtrRow[1]);
        sValueInfo2.column = (const mtcColumn *) sNode->func.compareColumn;
        sValueInfo2.flag   = MTD_OFFSET_USE;

        if ( (sNode->myNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
             == QMC_MTR_SORT_ORDER_FIXED_FALSE )
        {
            // partition by column
            sPartCompResult = sNode->func.compare( &sValueInfo1,
                                                   &sValueInfo2 );

            if( sPartCompResult != 0 )
            {
                break;
            }
        }
        else
        {
            // order by column
            sOrderCompResult = sNode->func.compare( &sValueInfo1,
                                                    &sValueInfo2 );

            if( sOrderCompResult != 0 )
            {
                break;
            }
        }
    }
    
    if( sPartCompResult == 0 )
    {
        *aFlag &= ~QMC_ROW_GROUP_MASK;
        *aFlag |= QMC_ROW_GROUP_SAME;
    }
    else
    {
        *aFlag &= ~QMC_ROW_GROUP_MASK;
        *aFlag |= QMC_ROW_GROUP_NULL;
    }

    if( sOrderCompResult == 0 )
    {
        *aFlag &= ~QMC_ROW_COMPARE_MASK;
        *aFlag |= QMC_ROW_COMPARE_SAME;
    }
    else
    {
        *aFlag &= ~QMC_ROW_COMPARE_MASK;
        *aFlag |= QMC_ROW_COMPARE_DIFF;
    }
    
    return IDE_SUCCESS;
}

IDE_RC
qmnWNST::allocMtrRow( qcTemplate     * aTemplate,
                      const qmncWNST * aCodePlan,
                      const qmndWNST * aDataPlan,
                      void           * aMtrRow[2] )
{
/***********************************************************************
 *
 * Description :
 *    MTR ROW�� �Ҵ�
 *
 * Implementation :
 *    Sort Temp Table���� �۾��ϹǷ� ������ ���� �Ҵ��Ѵ�.
 *    1. Memory Sort Temp: ������ �Ҵ��� �ʿ䰡 ����
 *    2. Disk Sort Temp: �̹� DataPlan->plan.myTuple->row�� �Ҵ�� ���� �����Ƿ�
 *                       �߰��� �ϳ��� mtrRowSizeũ�⸦ �Ҵ� �޾� ���
 *
 ***********************************************************************/
    iduMemory * sMemory;

    sMemory = aTemplate->stmt->qmxMem;

    //-------------------------------------------
    // �� Row�� �񱳸� ���� ���� �Ҵ�
    //-------------------------------------------
    
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        // nothing to do
    }
    else
    {
        // �̹� �Ҵ�� ������ �����Ƿ� �̸� �̿�
        IDE_DASSERT( aDataPlan->plan.myTuple->row != NULL );
        aMtrRow[0] = aDataPlan->plan.myTuple->row;

        // �񱳸� ���� �߰��� �ʿ��� ������ �Ҵ�
        IDE_TEST( sMemory->alloc( aDataPlan->mtrRowSize,
                                  (void**)&(aMtrRow[1]))
                  != IDE_SUCCESS);
        IDE_TEST_RAISE( aMtrRow[1] == NULL, err_mem_alloc );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::getFirstRecord( qcTemplate  * aTemplate,
                         qmndWNST    * aDataPlan,
                         qmcRowFlag  * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� Temp Table�� ù ��° ���ڵ带 ������
 *
 * Implementation :
 *
 ***********************************************************************/
    void       * sOrgRow;
    void       * sSearchRow;
    
    // ù ��° ���ڵ带 ���� ��
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    
    IDE_TEST( qmcSortTemp::getFirstSequence( aDataPlan->sortMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // �˻��� row�� mtrRow ����
    aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

    // Row ���� ���� ���� �� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        // Data�� ������ ��� Tuple Set ����
        IDE_TEST( setTupleSet( aTemplate,
                               aDataPlan->mtrNode,
                               aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

        // ????
        aDataPlan->plan.myTuple->modify++;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnWNST::getNextRecord( qcTemplate  * aTemplate,
                        qmndWNST    * aDataPlan,
                        qmcRowFlag  * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    getFirstRecord ���� �ݺ������� ȣ��Ǹ� ���ڵ带 ������� �ϳ��� ������
 *
 * Implementation :
 *
 ***********************************************************************/
    void       * sOrgRow;
    void       * sSearchRow;
    
    // ù ��° ���ڵ带 ���� ��
    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    
    IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->sortMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    
    aDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // �˻��� row�� mtrRow ����
    aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

    // Row ���� ���� ���� �� Tuple Set ����
    if ( sSearchRow != NULL )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        // Data�� ������ ��� Tuple Set ����
        IDE_TEST( setTupleSet( aTemplate,
                               aDataPlan->mtrNode,
                               aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

        // ????
        aDataPlan->plan.myTuple->modify++;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }    
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::printAnalyticFunctionInfo( qcTemplate     * aTemplate,
                                    const qmncWNST * aCodePlan,
                                          qmndWNST * aDataPlan,
                                    ULong            aDepth,
                                    iduVarString   * aString,
                                    qmnDisplay       aMode )
{
/***********************************************************************
 *
 * Description :
 *    Window Sort�� ������ �����
 *
 * Implementation :
 *
 ***********************************************************************/
    ULong    i;
    UInt     sSortKeyIdx;

    //-----------------------------
    // ù ��° ����Ű�� ���
    //-----------------------------

    for( sSortKeyIdx = 0;
         sSortKeyIdx < aCodePlan->sortKeyCnt;
         sSortKeyIdx++ )
    {
        //-----------------------------    
        // 1. ����Ű�� ���
        //-----------------------------    
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString, " " );
        }
    
        iduVarStringAppendFormat( aString, "SORT_KEY[%"ID_UINT32_FMT"]: (", sSortKeyIdx );

        if ( aMode == QMN_DISPLAY_ALL )
        {
            // explain plan = on; �� ���
            if ( (*aDataPlan->flag & QMND_WNST_INIT_DONE_MASK)
                 == QMND_WNST_INIT_DONE_TRUE )
            {
                IDE_TEST( printLinkedColumns( aTemplate,
                                              aDataPlan->sortNode[sSortKeyIdx],
                                              aString )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( printLinkedColumns( aTemplate,
                                              aCodePlan->sortNode[sSortKeyIdx],
                                              aString )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // explain plan = only; �� ���
            IDE_TEST( printLinkedColumns( aTemplate,
                                          aCodePlan->sortNode[sSortKeyIdx],
                                          aString )
                      != IDE_SUCCESS );
        }
        
        if ( ( sSortKeyIdx == 0 ) &&
             ( ( ( aCodePlan->flag & QMNC_WNST_STORE_MASK )
                   == QMNC_WNST_STORE_PRESERVED_ORDER ) ||
                 ( ( aCodePlan->flag & QMNC_WNST_STORE_MASK )
                   == QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER ) ) )
        {
            // ù ��° ����Ű�� PRESERVED ORDER�� ���
            // �̸� ����ϰ� �ٹٲ�
            iduVarStringAppend( aString, ") PRESERVED ORDER\n" );
        }
        else
        {
            // �� ���� ��� �׳� �ٹٲ�
            iduVarStringAppend( aString, ")\n" );  
        }

        //-----------------------------    
        // 2. ���õ�  Analytic Function�� ��� ���
        //-----------------------------    
        IDE_TEST( printWindowNode( aTemplate,
                                   aCodePlan->wndNode[sSortKeyIdx],
                                   aDepth+1,
                                   aString )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::printLinkedColumns( qcTemplate       * aTemplate,
                             const qmcMtrNode * aNode,
                             iduVarString     * aString )
{
/***********************************************************************
 *
 * Description :
 *    ����� Į������(qmcMtrNode)�� ��ǥ�� �����Ͽ� �����
 *
 * Implementation :
 *
 ***********************************************************************/
    const qmcMtrNode  * sNode;

   // ����� Į���� ���
    for( sNode = aNode;
         sNode != NULL;
         sNode = sNode->next )
    {
        IDE_TEST( qmoUtil::printExpressionInPlan( aTemplate,
                                                  aString,
                                                  sNode->srcNode,
                                                  QMO_PRINT_UPPER_NODE_NORMAL )
                  != IDE_SUCCESS );

        // BUG-33663
        if ( (sNode->flag & QMC_MTR_SORT_ORDER_MASK)
             == QMC_MTR_SORT_DESCENDING )
        {
            iduVarStringAppendLength( aString,
                                      " DESC",
                                      5 );
        }
        else
        {
            // Nothing to do.
        }
        
        if( sNode->next != NULL )
        {
            // ��ǥ ���
            iduVarStringAppend( aString, "," );                    
        }
        else
        {
            // ������ Į��
            break;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::printLinkedColumns( qcTemplate    * aTemplate,
                             qmdMtrNode    * aNode,
                             iduVarString  * aString )
{
/***********************************************************************
 *
 * Description :
 *    ����� Į������(qmcMtrNode)�� ��ǥ�� �����Ͽ� �����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmdMtrNode  * sNode;

   // ����� Į���� ���
    for( sNode = aNode;
         sNode != NULL;
         sNode = sNode->next )
    {
        IDE_TEST( qmoUtil::printExpressionInPlan( aTemplate,
                                                  aString,
                                                  sNode->srcNode,
                                                  QMO_PRINT_UPPER_NODE_NORMAL )
                  != IDE_SUCCESS );
        
        // BUG-33663
        if ( (sNode->flag & QMC_MTR_SORT_ORDER_MASK)
             == QMC_MTR_SORT_DESCENDING )
        {
            iduVarStringAppendLength( aString,
                                      " DESC",
                                      5 );
        }
        else
        {
            // Nothing to do.
        }
        
        if( sNode->next != NULL )
        {
            // ��ǥ ���
            iduVarStringAppend( aString, "," );                    
        }
        else
        {
            // ������ Į��
            break;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnWNST::printWindowNode( qcTemplate       * aTemplate,
                          const qmcWndNode * aWndNode,
                          ULong              aDepth,
                          iduVarString     * aString )
{
/***********************************************************************
 *
 * Description :
 *    ����� Window Node�� ������ Analytic Function ������
 *    aDepth��ŭ �鿩�Ἥ �����
 *
 * Implementation :
 *
 ***********************************************************************/
    ULong                  i;
    const qmcWndNode     * sWndNode;
    const qmcMtrNode     * sNode;
    
    for( sWndNode = aWndNode;
         sWndNode != NULL;
         sWndNode = sWndNode->next )
    {
        for( sNode = sWndNode->aggrNode;
             sNode != NULL;
             sNode = sNode->next )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString, " " );
            }

            IDE_TEST( qmoUtil::printExpressionInPlan( aTemplate,
                                                      aString,
                                                      sNode->srcNode,
                                                      QMO_PRINT_UPPER_NODE_NORMAL )
                      != IDE_SUCCESS );

            iduVarStringAppend( aString, "\n" );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * windowAggregation
 *
 * �����쿡 ���� �ɼǿ� ���� ������ ������ �ϴ� �Լ��� ȣ���Ѵ�.
 */
IDE_RC qmnWNST::windowAggregation( qcTemplate   * aTemplate,
                                   qmndWNST     * aDataPlan,
                                   qmcWndWindow * aWindow,
                                   qmdMtrNode   * aOverColumnNode,
                                   qmdMtrNode   * aOrderByColumn,
                                   qmdAggrNode  * aAggrNode,
                                   qmdMtrNode   * aAggrResultNode,
                                   idBool         aIsPartition )
{

    if ( aWindow->rowsOrRange == QTC_OVER_WINODW_ROWS )
    {
        switch ( aWindow->startOpt ) /* Start Point */
        {
            case QTC_OVER_WINODW_OPT_UNBOUNDED_PRECEDING:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedUnFollowRows( aTemplate,
                                                                     aDataPlan,
                                                                     aOverColumnNode,
                                                                     aAggrNode,
                                                                     aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedUnFollowRows( aTemplate,
                                                                 aDataPlan,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedCurrentRows( aTemplate,
                                                                    aDataPlan,
                                                                    aOverColumnNode,
                                                                    aAggrNode,
                                                                    aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedCurrentRows( aTemplate,
                                                                aDataPlan,
                                                                aAggrNode,
                                                                aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedPrecedRows( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aWindow->endValue.number,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedPrecedRows( aTemplate,
                                                               aDataPlan,
                                                               aWindow->endValue.number,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedFollowRows( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aWindow->endValue.number,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedFollowRows( aTemplate,
                                                               aDataPlan,
                                                               aWindow->endValue.number,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_CURRENT_ROW:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionCurrentUnFollowRows( aTemplate,
                                                                    aDataPlan,
                                                                    aOverColumnNode,
                                                                    aAggrNode,
                                                                    aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderCurrentUnFollowRows( aTemplate,
                                                                aDataPlan,
                                                                aAggrNode,
                                                                aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        IDE_TEST( currentCurrentRows( aTemplate,
                                                      aDataPlan,
                                                      aAggrNode,
                                                      aAggrResultNode )
                                  != IDE_SUCCESS );
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionCurrentFollowRows( aTemplate,
                                                                  aDataPlan,
                                                                  aOverColumnNode,
                                                                  aWindow->endValue.number,
                                                                  aAggrNode,
                                                                  aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderCurrentFollowRows( aTemplate,
                                                              aDataPlan,
                                                              aWindow->endValue.number,
                                                              aAggrNode,
                                                              aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_N_PRECEDING:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedUnFollowRows( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aWindow->startValue.number,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedUnFollowRows( aTemplate,
                                                               aDataPlan,
                                                               aWindow->startValue.number,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedCurrentRows( aTemplate,
                                                                  aDataPlan,
                                                                  aOverColumnNode,
                                                                  aWindow->startValue.number,
                                                                  aAggrNode,
                                                                  aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedCurrentRows( aTemplate,
                                                              aDataPlan,
                                                              aWindow->startValue.number,
                                                              aAggrNode,
                                                              aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedPrecedRows( aTemplate,
                                                                 aDataPlan,
                                                                 aOverColumnNode,
                                                                 aWindow->startValue.number,
                                                                 aWindow->endValue.number,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedPrecedRows( aTemplate,
                                                             aDataPlan,
                                                             aWindow->startValue.number,
                                                             aWindow->endValue.number,
                                                             aAggrNode,
                                                             aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedFollowRows( aTemplate,
                                                                 aDataPlan,
                                                                 aOverColumnNode,
                                                                 aWindow->startValue.number,
                                                                 aWindow->endValue.number,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedFollowRows( aTemplate,
                                                             aDataPlan,
                                                             aWindow->startValue.number,
                                                             aWindow->endValue.number,
                                                             aAggrNode,
                                                             aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_N_FOLLOWING:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionFollowUnFollowRows( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aWindow->startValue.number,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderFollowUnFollowRows( aTemplate,
                                                               aDataPlan,
                                                               aWindow->startValue.number,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionFollowFollowRows( aTemplate,
                                                                 aDataPlan,
                                                                 aOverColumnNode,
                                                                 aWindow->startValue.number,
                                                                 aWindow->endValue.number,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderFollowFollowRows( aTemplate,
                                                             aDataPlan,
                                                             aWindow->startValue.number,
                                                             aWindow->endValue.number,
                                                             aAggrNode,
                                                             aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            default:
                IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                break;
        }
    }
    else if ( aWindow->rowsOrRange == QTC_OVER_WINODW_RANGE )
    {
        switch ( aWindow->startOpt ) /* Start Point */
        {
            case QTC_OVER_WINODW_OPT_UNBOUNDED_PRECEDING:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedUnFollowRows( aTemplate,
                                                                     aDataPlan,
                                                                     aOverColumnNode,
                                                                     aAggrNode,
                                                                     aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedUnFollowRows( aTemplate,
                                                                 aDataPlan,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionOrderByAggregation( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderByAggregation( aTemplate,
                                                          aDataPlan,
                                                          aOverColumnNode,
                                                          aAggrNode,
                                                          aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        IDE_TEST_RAISE( aOrderByColumn->next != NULL,
                                        ERR_INVALID_WINDOW_SPECIFICATION );
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedPrecedFollowRange( aTemplate,
                                                                          aDataPlan,
                                                                          aOverColumnNode,
                                                                          aOrderByColumn,
                                                                          aWindow->endValue.number,
                                                                          aWindow->endValue.type,
                                                                          aAggrNode,
                                                                          aAggrResultNode,
                                                                          ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedPrecedFollowRange( aTemplate,
                                                                      aDataPlan,
                                                                      aOrderByColumn,
                                                                      aWindow->endValue.number,
                                                                      aWindow->endValue.type,
                                                                      aAggrNode,
                                                                      aAggrResultNode,
                                                                      ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        IDE_TEST_RAISE( aOrderByColumn->next != NULL,
                                        ERR_INVALID_WINDOW_SPECIFICATION );

                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionUnPrecedPrecedFollowRange( aTemplate,
                                                                          aDataPlan,
                                                                          aOverColumnNode,
                                                                          aOrderByColumn,
                                                                          aWindow->endValue.number,
                                                                          aWindow->endValue.type,
                                                                          aAggrNode,
                                                                          aAggrResultNode,
                                                                          ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderUnPrecedPrecedFollowRange( aTemplate,
                                                                      aDataPlan,
                                                                      aOrderByColumn,
                                                                      aWindow->endValue.number,
                                                                      aWindow->endValue.type,
                                                                      aAggrNode,
                                                                      aAggrResultNode,
                                                                      ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_CURRENT_ROW:
            {
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionCurrentUnFollowRange( aTemplate,
                                                                     aDataPlan,
                                                                     aOverColumnNode,
                                                                     aAggrNode,
                                                                     aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderCurrentUnFollowRange( aTemplate,
                                                                 aDataPlan,
                                                                 aOverColumnNode,
                                                                 aAggrNode,
                                                                 aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        IDE_TEST( currentCurrentRange( aTemplate,
                                                       aDataPlan,
                                                       aOverColumnNode,
                                                       aAggrNode,
                                                       aAggrResultNode )
                                  != IDE_SUCCESS );
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        IDE_TEST_RAISE( aOrderByColumn->next != NULL,
                                        ERR_INVALID_WINDOW_SPECIFICATION );

                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionCurrentFollowRange( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aOrderByColumn,
                                                                   aWindow->endValue.number,
                                                                   aWindow->endValue.type,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderCurrentFollowRange( aTemplate,
                                                               aDataPlan,
                                                               aOrderByColumn,
                                                               aWindow->endValue.number,
                                                               aWindow->endValue.type,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_N_PRECEDING:
            {
                IDE_TEST_RAISE( aOrderByColumn->next != NULL,
                                ERR_INVALID_WINDOW_SPECIFICATION );
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedFollowUnFollowRange( aTemplate,
                                                                          aDataPlan,
                                                                          aOverColumnNode,
                                                                          aOrderByColumn,
                                                                          aWindow->startValue.number,
                                                                          aWindow->startValue.type,
                                                                          aAggrNode,
                                                                          aAggrResultNode,
                                                                          ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedFollowUnFollowRange( aTemplate,
                                                                      aDataPlan,
                                                                      aOrderByColumn,
                                                                      aWindow->startValue.number,
                                                                      aWindow->startValue.type,
                                                                      aAggrNode,
                                                                      aAggrResultNode,
                                                                      ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedCurrentRange( aTemplate,
                                                                   aDataPlan,
                                                                   aOverColumnNode,
                                                                   aOrderByColumn,
                                                                   aWindow->startValue.number,
                                                                   aWindow->startValue.type,
                                                                   aAggrNode,
                                                                   aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedCurrentRange( aTemplate,
                                                               aDataPlan,
                                                               aOrderByColumn,
                                                               aWindow->startValue.number,
                                                               aWindow->startValue.type,
                                                               aAggrNode,
                                                               aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedPrecedRange( aTemplate,
                                                                  aDataPlan,
                                                                  aOverColumnNode,
                                                                  aOrderByColumn,
                                                                  aWindow->startValue.number,
                                                                  aWindow->startValue.type,
                                                                  aWindow->endValue.number,
                                                                  aWindow->endValue.type,
                                                                  aAggrNode,
                                                                  aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedPrecedRange( aTemplate,
                                                              aDataPlan,
                                                              aOrderByColumn,
                                                              aWindow->startValue.number,
                                                              aWindow->startValue.type,
                                                              aWindow->endValue.number,
                                                              aWindow->endValue.type,
                                                              aAggrNode,
                                                              aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedFollowRange( aTemplate,
                                                                  aDataPlan,
                                                                  aOverColumnNode,
                                                                  aOrderByColumn,
                                                                  aWindow->startValue.number,
                                                                  aWindow->startValue.type,
                                                                  aWindow->endValue.number,
                                                                  aWindow->endValue.type,
                                                                  aAggrNode,
                                                                  aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedFollowRange( aTemplate,
                                                              aDataPlan,
                                                              aOrderByColumn,
                                                              aWindow->startValue.number,
                                                              aWindow->startValue.type,
                                                              aWindow->endValue.number,
                                                              aWindow->endValue.type,
                                                              aAggrNode,
                                                              aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            case QTC_OVER_WINODW_OPT_N_FOLLOWING:
            {
                IDE_TEST_RAISE( aOrderByColumn->next != NULL,
                                ERR_INVALID_WINDOW_SPECIFICATION );
                switch ( aWindow->endOpt )
                {
                    case QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionPrecedFollowUnFollowRange( aTemplate,
                                                                          aDataPlan,
                                                                          aOverColumnNode,
                                                                          aOrderByColumn,
                                                                          aWindow->startValue.number,
                                                                          aWindow->startValue.type,
                                                                          aAggrNode,
                                                                          aAggrResultNode,
                                                                          ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderPrecedFollowUnFollowRange( aTemplate,
                                                                      aDataPlan,
                                                                      aOrderByColumn,
                                                                      aWindow->startValue.number,
                                                                      aWindow->startValue.type,
                                                                      aAggrNode,
                                                                      aAggrResultNode,
                                                                      ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                        break;
                    case QTC_OVER_WINODW_OPT_CURRENT_ROW:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_PRECEDING:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                    case QTC_OVER_WINODW_OPT_N_FOLLOWING:
                        if ( aIsPartition == ID_TRUE )
                        {
                            IDE_TEST( partitionFollowFollowRange( aTemplate,
                                                                  aDataPlan,
                                                                  aOverColumnNode,
                                                                  aOrderByColumn,
                                                                  aWindow->startValue.number,
                                                                  aWindow->startValue.type,
                                                                  aWindow->endValue.number,
                                                                  aWindow->endValue.type,
                                                                  aAggrNode,
                                                                  aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( orderFollowFollowRange( aTemplate,
                                                              aDataPlan,
                                                              aOverColumnNode,
                                                              aWindow->startValue.number,
                                                              aWindow->startValue.type,
                                                              aWindow->endValue.number,
                                                              aWindow->endValue.type,
                                                              aAggrNode,
                                                              aAggrResultNode )
                                      != IDE_SUCCESS );
                        }
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                        break;
                }
                break;
            }
            default:
                IDE_RAISE( ERR_INVALID_EXEC_METHOD );
                break;
        }
    }
    else
    {
        IDE_RAISE( ERR_INVALID_EXEC_METHOD );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_EXEC_METHOD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::windowAggregation",
                                  "Invalid exec method" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By UNBOUNDED PRECEDING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó������ �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ ������ ������ �Ѵ�.
 *
 *  Temp Table���� ù Row�� �о ��� ���� Row�� �о���̸鼭 �� Row�� ���� ��Ƽ��������
 *  �˻��Ѵ�. ���� ��Ƽ���� ��� Aggregation�� �����ϰ� �ƴѰ�� Aggregation�� �������ϰ�
 *  �ش� Temp Table�� Aggretaion Column�� Update �� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionUnPrecedUnFollowRows( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag        = QMC_ROW_INITIALIZE;
    SLong            sExecAggrCnt = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* �ʱ�ȭ ���� �� Ŀ���� �����Ѵ� */
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
        sExecAggrCnt = 0;

        do
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );
            sExecAggrCnt++;
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ����� Row�� ���� Row�� ���� ��ġ������ ���Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
            /* ���� ��Ƽ���� ��쿡�� Aggregation�� �����Ѵ�. */
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

        /* ���� ��Ƽ���� �ƴѰ�� Aggregatino�� finiAggregation �� �Ѵ�.*/
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        /* Store�� Ŀ���� �̵��ؼ� ���� �׷� Row�� ���� update�Ѵ� */
        IDE_TEST( updateAggrRows( aTemplate,
                                  aDataPlan,
                                  aAggrResultNode,
                                  &sFlag,
                                  sExecAggrCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By UNBOUNDED PRECEDING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ó������ �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ������ ������ ������ �Ѵ�.
 *
 *  TempTalbe�� ó������ ������ �о���̸鼭 Aggregation�� �����ϰ� �� ���� ó�� ���� ������
 *  Update�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderUnPrecedUnFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag        = QMC_ROW_INITIALIZE;
    SLong            sExecAggrCnt = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    /* Row �� �ִٸ� �ʱ�ȭ�� �����Ѵ� */
    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* Row�� �ִٸ� ���������� Aggregation�� �����Ѵ�. */
    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( execAggregation( aTemplate,
                                   aAggrNode,
                                   NULL,
                                   NULL,
                                   0 )
                  != IDE_SUCCESS );
        sExecAggrCnt++;
        aDataPlan->mtrRowIdx    = 1;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );
    }

    if ( sExecAggrCnt > 0 )
    {
        /* finilization */
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* Table�� ó������ �̵��� update�� �����Ѵ�. */
        IDE_TEST( updateAggrRows( aTemplate,
                                  aDataPlan,
                                  aAggrResultNode,
                                  &sFlag,
                                  sExecAggrCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By UNBOUNDED PRECEDING - CURRENT ROW
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó������ �����Ѵ�.
 *  End   Point �� CURRENT ROW ���� Row ������ ������ �Ѵ�.
 *
 *  ��Ƽ���� ó������ ���� Row���� Aggregation�� �����ϹǷ� ó������ Aggregation�� �����ϸ鼭
 *  update�� �����ϰ� ���� �ٸ� �׷��̶�� Aggregation�� �ʱ�ȭ�ϰ� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionUnPrecedCurrentRows( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    qmdMtrNode       * sNode;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        do
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            for ( sNode = ( qmdMtrNode *)aAggrResultNode;
                  sNode != NULL;
                  sNode = sNode->next )
            {
                IDE_TEST( sNode->func.setMtr( aTemplate,
                                              sNode,
                                              aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );
            }

            /* ���ݱ��� ���� Aggregation�� update�Ѵ�. */
            IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                      != IDE_SUCCESS );

            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx = 1;
            }
            else
            {
                aDataPlan->mtrRowIdx = 0;
            }
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate,
                                     aDataPlan,
                                     &sFlag )
                      != IDE_SUCCESS );

            if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                /* ����� Row�� ���� Row�� ���� ��ġ������ ���Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
            /* ���� ��Ƽ���� ��쿡�� Aggregation�� �����Ѵ�. */
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By UNBOUNDED PRECEDING - CURRENT ROW
 *
 *  Start Point �� UNBOUNDED PRECEDING ó������ �����Ѵ�.
 *  End   Point �� CURRENT ROW ���� Row ������ ������ �Ѵ�.
 *
 *  ó������ ���� Row���� Aggregation�� �����ϹǷ� ó������ Aggregation�� �����ϸ鼭
 *  update�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderUnPrecedCurrentRows( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    qmdMtrNode       * sNode;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );
    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( execAggregation( aTemplate,
                                   aAggrNode,
                                   NULL,
                                   NULL,
                                   0 )
                  != IDE_SUCCESS );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        for ( sNode = ( qmdMtrNode *)aAggrResultNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        /* ���ݱ��� ���� Aggregation�� update�Ѵ�. */
        IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        if ( aDataPlan->mtrRowIdx == 0 )
        {
            aDataPlan->mtrRowIdx = 1;
        }
        else
        {
            aDataPlan->mtrRowIdx = 0;
        }
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( getNextRecord( aTemplate,
                                 aDataPlan,
                                 &sFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By UNBOUNDED PRECEDING - N PRECEDING
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó������ �����Ѵ�.
 *  End   Point �� N         PRECEDING ���� Row���� N ���� Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ���� Row�� ���°������
 *  �����ϸ鼭 �̺��� N �� �������� Aggregation�� �����ѵڿ� ���� Row�� Restore�ѵڿ�
 *  Update�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionUnPrecedPrecedRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aEndValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sEndPoint    = aEndValue;
    SLong              sWindowPos;
    SLong              sCount;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sWindowPos = 0;

        /* ��Ƽ���� ó���� cursor�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� cursor�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ Cursor�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            /* ó������ ���� Row�� N�� ������ Record�� �����鼭 Aggregation �� �����Ѵ� */
            for ( sCount = sWindowPos - sEndPoint;
                  sCount >= 0;
                  sCount-- )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) != QMC_ROW_DATA_EXIST )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� Row�� Ŀ���� �����Ѵ� */
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
            /* ���� ��Ƽ���� ��쿡�� Aggregation�� �����Ѵ�. */
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By UNBOUNDED PRECEDING - N PRECEDING
 *
 *  Start Point �� UNBOUNDED PRECEDING ó������ �����Ѵ�.
 *  End   Point �� N         PRECEDING ������ ������ ������ �Ѵ�.
 *
 *  ���� Row�� Cursor�� �����Ѵ�. ���� Row�� ���°������
 *  �����ϸ鼭 �̺��� N �� �������� Aggregation�� �����ѵڿ� ���� Row�� Restore�ѵڿ�
 *  Update�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderUnPrecedPrecedRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aEndValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sWindowPos   = 0;
    SLong              sCount       = 0;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� cursor�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* �׻� ó�� Row�� Record�� �д´�. */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
        IDE_TEST( setTupleSet( aTemplate,
                               aDataPlan->mtrNode,
                               aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

        /* ó������ ���� Row�� N�� ������ Record�� �����鼭 Aggregation �� �����Ѵ� */
        for ( sCount = sWindowPos - sEndPoint;
              sCount >= 0;
              sCount-- )
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) != QMC_ROW_DATA_EXIST )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row�� Ŀ���� �����Ѵ� */
        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* N �� ������ Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By UNBOUNDED PRECEDING - N FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó������ �����Ѵ�.
 *  End   Point �� N         FOLLOWING ���� Row���� N ���� Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ���� Row�� ���°������
 *  �����ϸ鼭 �̺��� N �� �� ������ Aggregation�� �����ѵڿ� ���� Row�� Restore�ѵڿ�
 *  Update�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionUnPrecedFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aEndValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó���� cursor�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;

        do
        {
            /* ���� Row�� cursor�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ Cursor�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            /* ó������ ���� Row�� N�� �ı��� Record�� �����鼭 Aggregation �� �����Ѵ� */
            for ( sCount = sWindowPos + sEndPoint;
                  sCount >= 0;
                  sCount-- )
            {

                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                    if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    break;
                }
            }
            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� Row�� Ŀ���� �����Ѵ� */
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By UNBOUNDED PRECEDING - N FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ó������ �����Ѵ�.
 *  End   Point �� N         FOLLOWING ������ ������ ������ �Ѵ�.
 *
 *  ���� Row�� Cursor�� �����Ѵ�. ���� Row�� ���°������
 *  �����ϸ鼭 �̺��� N �� �� ������ Aggregation�� �����ѵڿ� ���� Row�� Restore�ѵڿ�
 *  Update�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderUnPrecedFollowRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aEndValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sWindowPos   = 0;
    SLong              sCount       = 0;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� cursor�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* �׻� ó�� Row�� Record�� �д´�. */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
        IDE_TEST( setTupleSet( aTemplate,
                               aDataPlan->mtrNode,
                               aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

        /* ó������ ���� Row�� N�� �ı��� Record�� �����鼭 Aggregation �� �����Ѵ� */
        for ( sCount = sWindowPos + sEndPoint;
              sCount >= 0;
              sCount-- )
        {

            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) != QMC_ROW_DATA_EXIST )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row�� Ŀ���� �����Ѵ� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By CURRENT ROW - UNBOUNDED FOLLOWING
 *
 *  Start Point �� CURRENT   ROW ���� Row ����  �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�. ���� Row ���� ��Ƽ���� ������ Aggregation��
 *  ������ �ڿ� ���� Row�� restore�Ŀ� update�� �ڿ� ���� row�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionCurrentUnFollowRows( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag        = QMC_ROW_INITIALIZE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        do
        {
            /* Aggr�ʱ�ȭ �� ���� Row�� cursor�� �����Ѵ�. */
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            /* ���� ��Ƽ���� ��� ��� Aggregation�� �����Ѵ� */
            do
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� Row�� restore�ѵڿ� update �׸��� Row�� �д´�. */
            IDE_TEST( updateAggrRows( aTemplate,
                                      aDataPlan,
                                      aAggrResultNode,
                                      &sFlag,
                                      1 )
                      != IDE_SUCCESS );
        } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By CURRENT ROW - UNBOUNDED FOLLOWING
 *
 *  Start Point �� CURRENT   ROW ���� Row ����  �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�. ���� Row ���� ������ Aggregation��
 *  ������ �ڿ� ���� Row�� restore�Ŀ� update�� �ڿ� ���� row�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderCurrentUnFollowRows( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag        = QMC_ROW_INITIALIZE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        do
        {
            /* Aggr�ʱ�ȭ �� ���� Row�� cursor�� �����Ѵ�. */
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            /* ������ Aggregation�� �����Ѵ� */
            do
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Nothing to do */
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
            /* ���� Row�� restore�ѵڿ� update �׸��� ���� Row�� �д´�. */
            IDE_TEST( updateAggrRows( aTemplate,
                                      aDataPlan,
                                      aAggrResultNode,
                                      &sFlag,
                                      1 )
                      != IDE_SUCCESS );
        } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By CURRENT ROW - CURRENT ROW
 *
 *  Start Point �� CURRENT   ROW ���� Row ����  �����Ѵ�.
 *  End   Point �� CURRENT   ROW ���� Row�����̴�.
 *
 *  ���� ROW�� ����ؼ� UPDATE �Ѵ�.
 */
IDE_RC qmnWNST::currentCurrentRows( qcTemplate  * aTemplate,
                                    qmndWNST    * aDataPlan,
                                    qmdAggrNode * aAggrNode,
                                    qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag   sFlag = QMC_ROW_INITIALIZE;
    qmdMtrNode * sNode;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        IDE_TEST( execAggregation( aTemplate,
                                   aAggrNode,
                                   NULL,
                                   NULL,
                                   0 )
                  != IDE_SUCCESS );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        for ( sNode = ( qmdMtrNode *)aAggrResultNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          aDataPlan->plan.myTuple->row)
                      != IDE_SUCCESS );
        }

        IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        if ( aDataPlan->mtrRowIdx == 0 )
        {
            aDataPlan->mtrRowIdx = 1;
        }
        else
        {
            aDataPlan->mtrRowIdx = 0;
        }
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( getNextRecord( aTemplate,
                                 aDataPlan,
                                 &sFlag )
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By CURRENT ROW - N FOLLOWING
 *
 *  Start Point �� CURRENT   ROW ���� Row ����  �����Ѵ�.
 *  End   Point �� N         FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�. ���� Row ���� N�� �ı��� Aggregation��
 *  ������ �ڿ� ���� Row�� restore�Ŀ� update�� �ڿ� ���� row�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionCurrentFollowRows( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            SLong         aEndValue,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� ���� N�� �ı��� Record�� �����鼭 ����� �����Ѵ� */
        for ( sExecAggrCnt = sEndPoint;
              sExecAggrCnt >= 0;
              sExecAggrCnt-- )
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );
            if ( sExecAggrCnt == 0 )
            {
                break;
            }
            else
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                    if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    break;
                }
            }
        }
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row�� restore�ѵڿ� update �׸��� Row�� �д´�. */
        IDE_TEST( updateAggrRows( aTemplate,
                                  aDataPlan,
                                  aAggrResultNode,
                                  &sFlag,
                                  1 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By CURRENT ROW - N FOLLOWING
 *
 *  Start Point �� CURRENT ROW ���� Row ����  �����Ѵ�.
 *  End   Point �� N       FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�. ���� Row ���� N�� �� ������ Aggregation��
 *  ������ �ڿ� ���� Row�� restore�Ŀ� update�� �ڿ� ���� row�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderCurrentFollowRows( qcTemplate  * aTemplate,
                                        qmndWNST    * aDataPlan,
                                        SLong         aEndValue,
                                        qmdAggrNode * aAggrNode,
                                        qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row�� N�� �ı��� Record�� �����鼭 Aggregation �� �����Ѵ� */
        for ( sExecAggrCnt = sEndPoint;
              sExecAggrCnt >= 0;
              sExecAggrCnt-- )
        {

            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );
            if ( sExecAggrCnt == 0 )
            {
                break;
            }
            else
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Nothing to do */
                }
                else
                {
                    break;
                }
            }
        }
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row�� restore�ѵڿ� update �׸��� Row�� �д´�. */
        IDE_TEST( updateAggrRows( aTemplate,
                                  aDataPlan,
                                  aAggrResultNode,
                                  &sFlag,
                                  1 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N PRECEDING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� N         PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ��Ƽ���� ������ Aggregation ����
 */
IDE_RC qmnWNST::partitionPrecedUnFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aStartValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag sFlag        = QMC_ROW_INITIALIZE;
    SLong      sCount       = 0;
    SLong      sWindowPos   = 0;
    SLong      sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;

        do
        {
            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ ���ư��� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            /* ��Ƽ���� ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
            for ( sCount = sWindowPos - sStartPoint;
                  sCount > 0;
                  sCount-- )
            {
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
            }

            /* N�� �� Row���� ��Ƽ���� ������ Aggregation�� �����Ѵ� */
            do
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By N PRECEDING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� N         PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ������ Aggregation ����
 */
IDE_RC qmnWNST::orderPrecedUnFollowRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aStartValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        /* ��Ƽ���� ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
        for ( sCount = sWindowPos - sStartPoint;
              sCount > 0;
              sCount-- )
        {
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothign to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        /* N�� �� Row���� ������ Aggregation�� �����Ѵ� */
        do
        {
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );

            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothign to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        } while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N PRECEDING - CURRENT ROW
 *
 *  Start Point �� N       PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� CURRENT ROW ���� Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ���� Row���� Aggregation ����
 */
IDE_RC qmnWNST::partitionPrecedCurrentRows( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            SLong         aStartValue,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;
        do
        {
            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ ���ư��� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );


            /* ��Ƽ���� ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
            for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
                  sCount > 0;
                  sCount--, sExecAggrCnt++ )
            {
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
            }

            do
            {
                /* N�� �� Row���� ���� Row���� Aggregation�� �����Ѵ� */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );

                if ( sWindowPos <= sExecAggrCnt )
                {
                    break;
                }
                else
                {
                    ++sExecAggrCnt;
                }
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N PRECEDING - CURRENT ROW
 *
 *  Start Point �� N       PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� CURRENT ROW ���� Row�����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ���� Row���� Aggregation ����
 */
IDE_RC qmnWNST::orderPrecedCurrentRows( qcTemplate  * aTemplate,
                                        qmndWNST    * aDataPlan,
                                        SLong         aStartValue,
                                        qmdAggrNode * aAggrNode,
                                        qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ó������ ���ư��� */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        /* ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
        for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
              sCount > 0;
              sCount--, sExecAggrCnt++ )
        {
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        do
        {
            /* N�� �� Row���� ���� Row���� Aggregation�� �����Ѵ� */
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );

            if ( sWindowPos <= sExecAggrCnt )
            {
                break;
            }
            else
            {
                ++sExecAggrCnt;
            }
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N PRECEDING - N PRECEDING
 *
 *  Start Point �� N PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N PRECEDING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ���� Row���� Aggregation ����
 */
IDE_RC qmnWNST::partitionPrecedPrecedRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           SLong         aStartValue,
                                           SLong         aEndValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;
        do
        {
            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            if ( sStartPoint < sEndPoint )
            {
                /* EndPoint���� Ŭ��� ����� ��ģ�� */
                sWindowPos = sStartPoint;
            }
            else
            {
                /* ��Ƽ���� ó������ Restore�Ѵ� */
                aDataPlan->mtrRowIdx = 0;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                      & aDataPlan->partitionCursorInfo )
                          != IDE_SUCCESS );

                aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
                IDE_TEST( setTupleSet( aTemplate,
                                       aDataPlan->mtrNode,
                                       aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );
            }

            if ( sWindowPos < sEndPoint )
            {
                /* EndPoint���� Ŭ��� ����� ��ģ�� */
                IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                          != IDE_SUCCESS );
            }
            else
            {
                /* ��Ƽ���� ó������ StartPoint�� N�� ������ SKIP�Ѵ� */
                for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
                      sCount > 0;
                      sCount--, sExecAggrCnt++ )
                {
                    if ( aDataPlan->mtrRowIdx == 0 )
                    {
                        aDataPlan->mtrRowIdx    = 1;
                        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                              != IDE_SUCCESS );
                }

                do
                {
                    /* StartPoint�� N�� ������ EndPoint�� N�� ������ Aggr�Ѵ�. */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );

                    if ( sWindowPos <= sExecAggrCnt + sEndPoint )
                    {
                        break;
                    }
                    else
                    {
                        ++sExecAggrCnt;
                    }

                    if ( aDataPlan->mtrRowIdx == 0)
                    {
                        aDataPlan->mtrRowIdx    = 1;
                        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                              != IDE_SUCCESS );
                    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                    {
                        /* ���� ��Ƽ�������� üũ�Ѵ� */
                        IDE_TEST( compareRows( aDataPlan,
                                               aOverColumnNode,
                                               &sFlag )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        break;
                    }
                } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

                IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                          != IDE_SUCCESS );

                aDataPlan->mtrRowIdx = 0;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                /* ���� Row ��ġ�� �ǵ��ƿ´� */
                IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                      &aDataPlan->cursorInfo )
                          != IDE_SUCCESS );

                aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

                IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );

            }

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By N PRECEDING - N PRECEDING
 *
 *  Start Point �� N       PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N       PRECEDING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� Cursor�� �����Ѵ�. ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ���� Row���� Aggregation ����
 */
IDE_RC qmnWNST::orderPrecedPrecedRows( qcTemplate  * aTemplate,
                                       qmndWNST    * aDataPlan,
                                       SLong         aStartValue,
                                       SLong         aEndValue,
                                       qmdAggrNode * aAggrNode,
                                       qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        if ( sStartPoint < sEndPoint )
        {
            /* EndPoint���� Ŭ��� ����� ��ģ�� */
            sWindowPos = sStartPoint;
        }
        else
        {
            /* ó�� Row�� �д´� */
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        if ( sWindowPos < sEndPoint )
        {
            /* EndPoint���� Ŭ��� ����� ��ģ�� */
            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
        }
        else
        {
            /* ó������ StartPoint�� N�� ������ SKIP�Ѵ� */
            for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
                  sCount > 0;
                  sCount--, sExecAggrCnt++ )
            {
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
            }

            do
            {
                /* StartPoint�� N�� ������ EndPoint�� N�� ������ Aggr�Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );

                if ( sWindowPos <= sExecAggrCnt + sEndPoint )
                {
                    break;
                }
                else
                {
                    ++sExecAggrCnt;
                }
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

            } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N PRECEDING - N PRECEDING
 *
 *  Start Point �� N  PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N  FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� Cursor�� �����Ѵ�. ��Ƽ���� ó������
 *  ���ư��� ���� ROW�� N�� �������� SKIP �ϰ� �� �� ���� ���� Row���� Aggregation ����
 */
IDE_RC qmnWNST::partitionPrecedFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           SLong         aStartValue,
                                           SLong         aEndValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;
        do
        {
            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ Restore�Ѵ� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            /* ��Ƽ���� ó������ StartPoint�� N�� ������ SKIP�Ѵ� */
            for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
                  sCount > 0;
                  sCount--, sExecAggrCnt++ )
            {
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
            }

            do
            {
                /* StartPoint�� N�� ������ EndPoint�� N�� �� ���� Aggr�Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );

                if ( sWindowPos + sEndPoint <= sExecAggrCnt )
                {
                    break;
                }
                else
                {
                    ++sExecAggrCnt;
                }

                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By N PRECEDING - N FOLLOWING
 *
 *  Start Point �� N  PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N  FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�. ó������ ���ư��� ���� ROW�� N�� �������� SKIP
 *  �ϰ� �� �� ���� ���� Row���� N�� �ı��� Aggregation ����
 */
IDE_RC qmnWNST::orderPrecedFollowRows( qcTemplate  * aTemplate,
                                       qmndWNST    * aDataPlan,
                                       SLong         aStartValue,
                                       SLong         aEndValue,
                                       qmdAggrNode * aAggrNode,
                                       qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sExecAggrCnt = 0;
    SLong              sCount       = 0;
    SLong              sWindowPos   = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        /* ��Ƽ���� ó������ StartPoint�� N�� ������ SKIP�Ѵ� */
        for ( sCount = sWindowPos - sStartPoint, sExecAggrCnt = 0;
              sCount > 0;
              sCount--, sExecAggrCnt++ )
        {
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        do
        {
            /* StartPoint�� N�� ������ EndPoint�� N�� �� ���� Aggr�Ѵ�. */
            IDE_TEST( execAggregation( aTemplate,
                                       aAggrNode,
                                       NULL,
                                       NULL,
                                       0 )
                      != IDE_SUCCESS );

            if ( sWindowPos + sEndPoint <= sExecAggrCnt )
            {
                break;
            }
            else
            {
                ++sExecAggrCnt;
            }
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        ++sWindowPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N FOLLOWING - UNBOUNDED FOLLWOING
 *
 *  Start Point �� N         FOLLOWING ���� Row�� N �� �ĺ��� �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ���� ��Ƽ���� ������ �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�.
 *  ���� ROW�� N�� �� ������ SKIP �ϰ� �� �� ���� ��Ƽ���� ���������� Aggregation ����
 */
IDE_RC qmnWNST::partitionFollowUnFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aStartValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    qmdMtrNode       * sMtrNode;
    SLong              sCount       = 0;
    SLong              sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        /* ���� ��ġ���� N�� �� ���� SKIP�Ѵ�. �̶� �ٸ� ��Ƽ���� ��츦 �����ؼ� Skip�� ����� */
        for ( sCount = sStartPoint;
              sCount > 0;
              sCount-- )
        {
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );

                if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                break;
            }
        }

        if ( sCount <= 0 )
        {
            /* N �� �ĺ��� ��Ƽ���� ���������� ����� �Ѵ�. */
            while ( 1 )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );

                    if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        for ( sMtrNode = aAggrResultNode;
              sMtrNode != NULL;
              sMtrNode = sMtrNode->next )
        {
            IDE_TEST( sMtrNode->func.setMtr( aTemplate,
                                             sMtrNode,
                                             aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        /* Update�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        if ( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME )
        {
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }
        else
        {
            aDataPlan->mtrRowIdx    = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }

        /* ���� Record�� �����´� */
        IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By N FOLLOWING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� N         FOLLOWING ���� Row�� N �� �ĺ��� �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ���� ��Ƽ���� ������ �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�.
 *  ���� ROW�� N�� �ı����� SKIP �ϰ� �� �� ���� ������ Aggregation ����
 */
IDE_RC qmnWNST::orderFollowUnFollowRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aStartValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sCount       = 0;
    SLong              sStartPoint  = aStartValue;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        /* ���� ��ġ���� N�� �� ���� SKIP�Ѵ�. */
        for ( sCount = sStartPoint;
              sCount > 0;
              sCount-- )
        {
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* Nothing to do */
            }
            else
            {
                break;
            }
        }

        if ( sCount <= 0 )
        {
            /* N �� �ĺ��� ���������� ����� �Ѵ�. */
            while ( 1 )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Nothing to do */
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row�� �ǵ��� �´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Partition By Order By N FOLLOWING - N FOLLWOING
 *
 *  Start Point �� N FOLLOWING ���� Row�� N �� �ĺ��� �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�.
 *  ���� ROW�� Start N�� �������� SKIP �ϰ� �� �� ���� End N �� �ı��� Aggregation ����
 */
IDE_RC qmnWNST::partitionFollowFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           SLong         aStartValue,
                                           SLong         aEndValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    qmdMtrNode       * sMtrNode;
    SLong              sCount       = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;
    SLong              sExecCount   = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        /* ���� ��ġ���� N�� �� ���� SKIP�Ѵ�. �̶� �ٸ� ��Ƽ���� ��츦 �����ؼ� Skip�� ����� */
        for ( sCount = sStartPoint;
              sCount > 0;
              sCount-- )
        {
            if ( sCount > sEndPoint )
            {
                break;
            }
            else
            {
                /* Nothin to do */
            }
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );

                if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                break;
            }
        }

        if ( sCount <= 0 )
        {
            /* N �� �ı��� ����� �Ѵ�. �̶� �ٸ� ��Ƽ�������� �����Ѵ� */
            for ( sExecCount = sEndPoint - sStartPoint;
                  sExecCount >= 0;
                  sExecCount-- )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* ���� ��Ƽ�������� üũ�Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );

                    if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        for ( sMtrNode = aAggrResultNode;
              sMtrNode != NULL;
              sMtrNode = sMtrNode->next )
        {
            IDE_TEST( sMtrNode->func.setMtr( aTemplate,
                                             sMtrNode,
                                             aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        /* Update�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        if ( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME )
        {
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }
        else
        {
            aDataPlan->mtrRowIdx    = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        }

        /* ���� Record�� �����´� */
        IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ROWS Order By N FOLLOWING - N FOLLWOING
 *
 *  Start Point �� N FOLLOWING ���� Row�� N �� �ĺ��� �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� Cursor�� �����Ѵ�.
 *  ���� ROW�� Start N�� �������� SKIP �ϰ� �� �� ���� End N �� �ı��� Aggregation ����
 */
IDE_RC qmnWNST::orderFollowFollowRows( qcTemplate  * aTemplate,
                                       qmndWNST    * aDataPlan,
                                       SLong         aStartValue,
                                       SLong         aEndValue,
                                       qmdAggrNode * aAggrNode,
                                       qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag        = QMC_ROW_INITIALIZE;
    SLong              sCount       = 0;
    SLong              sStartPoint  = aStartValue;
    SLong              sEndPoint    = aEndValue;
    SLong              sExecCount   = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        /* ���� ��ġ���� N�� �� ���� SKIP�Ѵ�. */
        for ( sCount = sStartPoint;
              sCount > 0;
              sCount-- )
        {
            if ( sCount > sEndPoint )
            {
                break;
            }
            else
            {
                /* Nothin to do */
            }
            if ( aDataPlan->mtrRowIdx == 0 )
            {
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* Nothing to do */
            }
            else
            {
                break;
            }
        }

        if ( sCount <= 0 )
        {
            for ( sExecCount = sEndPoint - sStartPoint;
                  sExecCount >= 0;
                  sExecCount-- )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                if ( aDataPlan->mtrRowIdx == 0 )
                {
                    aDataPlan->mtrRowIdx    = 1;
                    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Nothing to do */
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row�� �ǵ��� �´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By CURRENT ROW - UNBOUNDED FOLLOWING
 *
 *  Start Point �� CURRENT   ROW       ���� Row ���� �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� ��Ƽ���� ������ Aggr�� ����ϴµ�
 *  �� �� ���� Row�� Logical�ϰ� ���� �� Order by ������ �÷����� ���� Row�� ���� �׸�ŭ
 *  Update�� �����ϰ� ���� Record�� �д´�.
 */
IDE_RC qmnWNST::partitionCurrentUnFollowRange( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag        = QMC_ROW_INITIALIZE;
    SLong            sExecAggrCnt;
    SLong            sSameAggrCnt;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        while ( 1 )
        {
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
            sExecAggrCnt = 0;
            sSameAggrCnt = 1;

            do
            {
                /* ���� Row ���� ��Ƽ���� ������ Aggr�� �����Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                sExecAggrCnt++;
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }

                /* Partitioy By �÷��� Order By �÷��� ��� ���� ��� �̴� */
                if ( ( sFlag & QMC_ROW_COMPARE_MASK ) == QMC_ROW_COMPARE_SAME )
                {
                    ++sSameAggrCnt;
                }
                else
                {
                    /* Nothin to do */
                }

            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� ���ư��� Order By �÷����� ���� ��� ���� Update�� �����ϰ�
             * ���� Record�� �д´�.
             */
            IDE_TEST( updateAggrRows( aTemplate,
                                      aDataPlan,
                                      aAggrResultNode,
                                      &sFlag,
                                      sSameAggrCnt )
                      != IDE_SUCCESS );

            if ( ( sFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_EXIST )
            {
                if ( sExecAggrCnt <= sSameAggrCnt )
                {
                    break;
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
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By CURRENT ROW- UNBOUNDED FOLLOWING
 *
 *  Start Point �� CURRENT   ROW       ���� Row ���� �����Ѵ�.
 *  End   Point �� UNBOUNDED FOLLOWING ������ �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� ��Ƽ���� ������ Aggr�� ����ϴµ�
 *  �� �� ���� Row�� Logical�ϰ� ���� �� Order by ������ �÷����� ���� Row�� ���� �׸�ŭ
 *  Update�� �����ϰ� ���� Record�� �д´�.
 */
IDE_RC qmnWNST::orderCurrentUnFollowRange( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag       sFlag = QMC_ROW_INITIALIZE;
    SLong            sExecAggrCnt;
    SLong            sSameAggrCnt;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        do
        {
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            sExecAggrCnt = 0;
            sSameAggrCnt = 1;

            do
            {
                /* ���� Row���� ������ Row���� Aggr�� �����Ѵ� */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                sExecAggrCnt++;
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }

                /* Partitioy By �÷��� Order By �÷��� ��� ���� ��� �̴� */
                if ( (sFlag & QMC_ROW_COMPARE_MASK) == QMC_ROW_COMPARE_SAME )
                {
                    ++sSameAggrCnt;
                }
                else
                {
                    /* Nothing to do */
                }
            } while ( ( sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� ���ư��� Order By �÷����� ���� ��� ���� Update�� �����ϰ�
             * ���� Record�� �д´�.
             */
            IDE_TEST( updateAggrRows( aTemplate,
                                      aDataPlan,
                                      aAggrResultNode,
                                      &sFlag,
                                      sSameAggrCnt )
                      != IDE_SUCCESS );
        } while ( sExecAggrCnt > sSameAggrCnt );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE CURRENT ROW - CURRENT ROW
 *
 *  Start Point �� CURRENT ROW ���� Row ���� �����Ѵ�.
 *  End   Point �� CURRENT ROW ���� Row �����̴�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� ���� Row�� Logical�ϰ� ����
 *  �� Order by ������ �÷����� ���� Row�� ���� �׸�ŭ Update�� �����ϰ� ���� Record�� �д´�.
 */
IDE_RC qmnWNST::currentCurrentRange( qcTemplate  * aTemplate,
                                     qmndWNST    * aDataPlan,
                                     qmdMtrNode  * aOverColumnNode,
                                     qmdAggrNode * aAggrNode,
                                     qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag sFlag        = QMC_ROW_INITIALIZE;
    SLong      sExecAggrCnt;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        do
        {
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );
            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
            sExecAggrCnt = 0;

            do
            {
                /* ���� Row���� Order by ������ �÷����� ���� Row�� Aggr �Ѵ� */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                sExecAggrCnt++;
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Logical�ϰ� ���� �� ���Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_COMPARE_MASK ) == QMC_ROW_COMPARE_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� ���ư��� Order By �÷����� ���� ��� ���� Update�� �����ϰ�
             * ���� Record�� �д´�.
             */
            IDE_TEST( updateAggrRows( aTemplate,
                                      aDataPlan,
                                      aAggrResultNode,
                                      &sFlag,
                                      sExecAggrCnt )
                      != IDE_SUCCESS );
        } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Calculate Interval
 *
 *   Range ���������� ���Ǹ� N PRECEDING, N FOLLOWING���� ���Ǵ� N�� ���ؼ�
 *   ���� Row�� ����� �����Ѵ�.
 *
 *   �׻� ORDER BY�� ���Ǵ� �÷��� 1 �� ���߸� �Ѵ�.
 *
 *   aInterval     - N�� �ǹ��Ѵ�. Range������ N�� Logical�� ������ �����Դ�.
 *   aIntervalType - N�� �������� �����ѵ� Order By �÷��� �����̰ų� Date�ΰ�츸
 *                   �����ϴ�.
 *   aValue        - ���� Row���� Interval ����ŭ ���ų� ���� ���� ����� �Ѱ��ش�.
 *   aIsPreceding  - PRECEDING �� ��쿡�� ���� Row���� ���� �����̰�,
 *                   FOLLOWING �� ��쿡�� ���� Row���� ���� ���ϴ°��� �ǹ��Ѵ�.
 *
 *   ���� ROW�� ORDER BY �ķ��� �÷� Type�� value�� ��´�.
 *   ���� NULL �ΰ�쿡�� ������� �ʰ� NULL Type���� ��ø� �Ѵ�.
 */
IDE_RC qmnWNST::calculateInterval( qcTemplate        * aTemplate,
                                   qmcdSortTemp      * aTempTable,
                                   qmdMtrNode        * aNode,
                                   void              * aRow,
                                   SLong               aInterval,
                                   UInt                aIntervalType,
                                   qmcWndWindowValue * aValue,
                                   idBool              aIsPreceding )
{
    SLong            sLong     = 0;
    SDouble          sDouble   = 0;
    mtdDateType      sDateType;
    SLong            sInterval = 0;
    const void     * sRowValue;
    mtdNumericType * sNumeric1 = NULL;
    SChar            sBuffer[MTD_NUMERIC_SIZE_MAXIMUM];
    mtcColumn      * sColumn;

    /* Temp Table �� Value �� ��� */
    if ( ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
           == QMC_MTR_TYPE_COPY_VALUE ) ||
         ( ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
             == QMC_MTR_TYPE_MEMORY_KEY_COLUMN ) &&
           ( ( aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE )
             == QMCD_SORT_TMP_STORAGE_DISK ) ) ||
         ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
           == QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN ) ) /* PROJ-2464 hybrid partitioned table ���� */
    {
        sColumn = aNode->dstColumn;
        
        /* Value�� ��� Dst �÷����� �д´� */
        sRowValue = mtc::value( sColumn, aNode->dstTuple->row, MTD_OFFSET_USE );

        if ( sColumn->module->isNull( sColumn, sRowValue )
             == ID_TRUE )
        {
            /* NULL �ΰ�� ������� �ʴ´�. */
            aValue->type = QMC_WND_WINDOW_VALUE_NULL;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Value�� �ƴ� ��� ���� �Ŀ� srcColumn���� �д´�. */
        IDE_TEST( aNode->func.setTuple( aTemplate, aNode, aRow ) != IDE_SUCCESS );

        sColumn = aNode->srcColumn;
        
        sRowValue = mtc::value( sColumn, aNode->srcTuple->row, MTD_OFFSET_USE );

        if ( sColumn->module->isNull( sColumn, sRowValue )
             == ID_TRUE )
        {
            /* NULL �ΰ�� ������� �ʴ´�. */
            aValue->type = QMC_WND_WINDOW_VALUE_NULL;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sColumn->module->id == MTD_SMALLINT_ID )
    {
        /* SMALLINT �ΰ�� LONG���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_LONG;
        sLong = (SLong)(*(mtdSmallintType*)sRowValue);
        IDE_TEST_RAISE( MTD_SMALLINT_MAXIMUM < aInterval,
                        ERR_INVALID_WINDOW_SPECIFICATION );
    }
    else if ( sColumn->module->id == MTD_INTEGER_ID )
    {
        /* INTEGER �ΰ�� LONG���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_LONG;
        sLong = (SLong)(*(mtdIntegerType*)sRowValue);
        IDE_TEST_RAISE( MTD_INTEGER_MAXIMUM < aInterval,
                        ERR_INVALID_WINDOW_SPECIFICATION );
    }
    else if ( sColumn->module->id == MTD_BIGINT_ID )
    {
        /* BIGINT �ΰ�� LONG���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_LONG;
        sLong = (SLong)(*(mtdBigintType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_DOUBLE_ID )
    {
        /* DOUBLE �ΰ�� DOUBLE���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_DOUBLE;
        sDouble = (SDouble)(*(mtdDoubleType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_REAL_ID )
    {
        /* REAL �ΰ�� DOUBLE���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_DOUBLE;
        sDouble = (SDouble)(*(mtdRealType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_DATE_ID )
    {
        /* DATE �ΰ�� DATE���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_DATE;
        sDateType = (*(mtdDateType*)sRowValue);
    }
    else if ( ( sColumn->module->id == MTD_FLOAT_ID )   ||
              ( sColumn->module->id == MTD_NUMERIC_ID ) ||
              ( sColumn->module->id == MTD_NUMBER_ID ) )
    {
        /* Float, Numeric, Number�� NUMERIC���� ó���Ѵ� */
        aValue->type = QMC_WND_WINDOW_VALUE_NUMERIC;
        sNumeric1 = (mtdNumericType*)sRowValue;
        IDE_TEST( mtv::nativeN2Numeric( aInterval, (mtdNumericType * )sBuffer )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
    }

    /* PRECEDING �� ��� ���� ROW���� ������ �����Ѵ� */
    if ( aIsPreceding == ID_TRUE )
    {
        switch ( aValue->type )
        {
            case QMC_WND_WINDOW_VALUE_LONG:
                aValue->longType = sLong - aInterval;
                break;
            case QMC_WND_WINDOW_VALUE_DOUBLE:
                aValue->doubleType = sDouble - (SDouble)aInterval;
                break;
            case QMC_WND_WINDOW_VALUE_DATE:
                switch ( aIntervalType )
                {
                    case QTC_OVER_WINDOW_VALUE_TYPE_YEAR:
                        if ( aInterval > 0 )
                        {
                            sInterval = 12 * aInterval;
                            IDE_TEST( mtdDateInterface::addMonth( &aValue->dateType,
                                                                  &sDateType,
                                                                  -sInterval )
                                      != IDE_SUCCESS );
                            aValue->dateField = MTD_DATE_DIFF_YEAR;
                        }
                        else
                        {
                            aValue->dateType = sDateType;
                            aValue->dateField = MTD_DATE_DIFF_DAY;
                        }
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_MONTH:
                        if ( aInterval > 0 )
                        {
                            sInterval = aInterval;
                            IDE_TEST( mtdDateInterface::addMonth( &aValue->dateType,
                                                                  &sDateType,
                                                                  -sInterval )
                                      != IDE_SUCCESS );
                            aValue->dateField = MTD_DATE_DIFF_MONTH;
                        }
                        else
                        {
                            aValue->dateType = sDateType;
                            aValue->dateField = MTD_DATE_DIFF_DAY;
                        }
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_NUMBER:
                    case QTC_OVER_WINDOW_VALUE_TYPE_DAY:
                        sInterval = aInterval;
                        IDE_TEST( mtdDateInterface::addDay( &aValue->dateType,
                                                            &sDateType,
                                                            -sInterval )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_DAY;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_HOUR:
                        sInterval = 3600 * aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               -sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_HOUR;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_MINUTE:
                        sInterval = 60 * aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               -sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_MINUTE;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_SECOND:
                        sInterval = aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               -sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_SECOND;
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NUMERIC:
                IDE_TEST( mtc::subtractFloat( (mtdNumericType * )aValue->numericType,
                                              MTD_FLOAT_PRECISION_MAXIMUM,
                                              sNumeric1,
                                              (mtdNumericType * )sBuffer )
                          != IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    else /* FOLLOWING �� ��� ���� ROW���� ������ �����Ѵ� */
    {
        switch ( aValue->type )
        {
            case QMC_WND_WINDOW_VALUE_LONG:
                aValue->longType = sLong + aInterval;
                break;
            case QMC_WND_WINDOW_VALUE_DOUBLE:
                aValue->doubleType = sDouble + (SDouble)aInterval;
                break;
            case QMC_WND_WINDOW_VALUE_DATE:
                switch ( aIntervalType )
                {
                    case QTC_OVER_WINDOW_VALUE_TYPE_YEAR:
                        if ( aInterval > 0 )
                        {
                            sInterval = 12 * aInterval;
                            IDE_TEST( mtdDateInterface::addMonth( &aValue->dateType,
                                                                  &sDateType,
                                                                  sInterval )
                                      != IDE_SUCCESS );
                            aValue->dateField = MTD_DATE_DIFF_YEAR;
                        }
                        else
                        {
                            aValue->dateType  = sDateType;
                            aValue->dateField = MTD_DATE_DIFF_DAY;
                        }
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_MONTH:
                        if ( aInterval > 0 )
                        {
                            sInterval = aInterval;
                            IDE_TEST( mtdDateInterface::addMonth( &aValue->dateType,
                                                                  &sDateType,
                                                                  sInterval )
                                      != IDE_SUCCESS );
                            aValue->dateField = MTD_DATE_DIFF_MONTH;
                        }
                        else
                        {
                            aValue->dateType  = sDateType;
                            aValue->dateField = MTD_DATE_DIFF_DAY;
                        }
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_NUMBER:
                    case QTC_OVER_WINDOW_VALUE_TYPE_DAY:
                        sInterval = aInterval;
                        IDE_TEST( mtdDateInterface::addDay( &aValue->dateType,
                                                            &sDateType,
                                                            sInterval )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_DAY;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_HOUR:
                        sInterval = 3600 * aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_HOUR;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_MINUTE:
                        sInterval = 60 * aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_MINUTE;
                        break;
                    case QTC_OVER_WINDOW_VALUE_TYPE_SECOND:
                        sInterval = aInterval;
                        IDE_TEST( mtdDateInterface::addSecond( &aValue->dateType,
                                                               &sDateType,
                                                               sInterval,
                                                               0 )
                                  != IDE_SUCCESS );
                        aValue->dateField = MTD_DATE_DIFF_SECOND;
                        break;
                    default:
                        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
                        break;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NUMERIC:
                IDE_TEST( mtc::addFloat( (mtdNumericType * )aValue->numericType,
                                         MTD_FLOAT_PRECISION_MAXIMUM,
                                         sNumeric1,
                                         (mtdNumericType * )sBuffer )
                          != IDE_SUCCESS );
                break;
            default:
                break;
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Compare RANGE Value
 *
 *  RANGE���� ���ȴ�. calculate�� value �� ���� ���� Record�� �񱳸� �ؼ� �� Reocrd��
 *  ������ ��� Record������ �Ǵ��Ѵ�.
 *
 *  aRow             - ���� ���� Row�̴�.
 *  aValue           - ���ؾ��� ���̴�.
 *  aIsLessThanEqual - ORDER BY ������ ASC, ���� DESC������ ���� �۰ų� ������
 *                     ������ �ؾ����� ũ�ų� ������ ������ �ؾ������� �޶�����.
 *  aResult          - ������ �������� �ƴ� ���� �Ǵ��Ѵ�.
 *
 */
IDE_RC qmnWNST::compareRangeValue( qcTemplate        * aTemplate,
                                   qmcdSortTemp      * aTempTable,
                                   qmdMtrNode        * aNode,
                                   void              * aRow,
                                   qmcWndWindowValue * aValue,
                                   idBool              aIsLessThanEqual,
                                   idBool            * aResult )
{
    SLong            sLong     = 0;
    SDouble          sDouble   = 0;
    mtdDateType      sDateType;
    const void     * sRowValue;
    mtdNumericType * sNumeric1 = NULL;
    mtdNumericType * sNumeric2 = NULL;
    SChar            sBuffer[MTD_NUMERIC_SIZE_MAXIMUM];
    idBool           sResult = ID_FALSE;
    mtcColumn      * sColumn;

    /* Temp Table �� Value �� ��� */
    if ( ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
           == QMC_MTR_TYPE_COPY_VALUE ) ||
         ( ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
             == QMC_MTR_TYPE_MEMORY_KEY_COLUMN ) &&
           ( ( aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE )
             == QMCD_SORT_TMP_STORAGE_DISK ) ) ||
         ( ( aNode->myNode->flag & QMC_MTR_TYPE_MASK )
           == QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN ) ) /* PROJ-2464 hybrid partitioned table ���� */
    {
        sColumn = aNode->dstColumn;
        
        /* Value�� ��� Dst �÷����� �д´� */
        sRowValue = mtc::value( sColumn, aNode->dstTuple->row, MTD_OFFSET_USE );

        if ( sColumn->module->isNull( sColumn, sRowValue )
             == ID_TRUE )
        {
            /* NULL �� ��� ������ �ʰ� FALSE�̴�.. */
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Value�� �ƴ� ��� ���� �Ŀ� srcColumn���� �д´�. */
        IDE_TEST( aNode->func.setTuple( aTemplate, aNode, aRow ) != IDE_SUCCESS );
        
        sColumn = aNode->srcColumn;
        
        sRowValue = mtc::value( sColumn, aNode->srcTuple->row, MTD_OFFSET_USE );

        if ( sColumn->module->isNull( sColumn, sRowValue )
             == ID_TRUE )
        {
            /* NULL �� ��� ������ �ʰ� FALSE�̴�. */
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sColumn->module->id == MTD_SMALLINT_ID )
    {
        /* SMALLINT �ΰ�� LONG���� ó���Ѵ� */
        sLong = (SLong)(*(mtdSmallintType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_INTEGER_ID )
    {
        /* INTEGER �ΰ�� LONG���� ó���Ѵ� */
        sLong = (SLong)(*(mtdIntegerType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_BIGINT_ID )
    {
        /* BIGINT �ΰ�� LONG���� ó���Ѵ� */
        sLong = (SLong)(*(mtdBigintType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_DOUBLE_ID )
    {
        /* DOUBLE �ΰ�� DOUBLE���� ó���Ѵ� */
        sDouble = (SDouble)(*(mtdDoubleType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_REAL_ID )
    {
        /* REAL �ΰ�� DOUBLE���� ó���Ѵ� */
        sDouble = (SDouble)(*(mtdRealType*)sRowValue);
    }
    else if ( sColumn->module->id == MTD_DATE_ID )
    {
        /* DATE �ΰ�� DATE���� ó���Ѵ� */
        sDateType = (*(mtdDateType*)sRowValue);
    }
    else if ( ( sColumn->module->id == MTD_FLOAT_ID )   ||
              ( sColumn->module->id == MTD_NUMERIC_ID ) ||
              ( sColumn->module->id == MTD_NUMBER_ID ) )
    {
        /* Float, Numeric, Number�� NUMERIC���� ó���Ѵ� */
        sNumeric1 = (mtdNumericType*)sRowValue;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_WINDOW_SPECIFICATION );
    }

    if ( aIsLessThanEqual == ID_FALSE )
    {
        switch ( aValue->type )
        {
            case QMC_WND_WINDOW_VALUE_LONG:
                if ( aValue->longType >= sLong )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_DOUBLE:
                if ( aValue->doubleType >= sDouble )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_DATE:
                IDE_TEST( mtdDateInterface::dateDiff( &sLong,
                                                      &sDateType,
                                                      &aValue->dateType,
                                                      aValue->dateField )
                          != IDE_SUCCESS );
                if ( sLong >= 0 )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NUMERIC:
                sNumeric2 = (mtdNumericType * )sBuffer;
                IDE_TEST( mtc::subtractFloat( sNumeric2,
                                              MTD_FLOAT_PRECISION_MAXIMUM,
                                              (mtdNumericType * )aValue->numericType,
                                              sNumeric1 )
                          != IDE_SUCCESS );
                if ( MTD_NUMERIC_IS_POSITIVE( sNumeric2 ) == ID_TRUE )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NULL:
                sResult = ID_TRUE;
                break;
            default:
                break;
        }
    }
    else
    {
        switch ( aValue->type )
        {
            case QMC_WND_WINDOW_VALUE_LONG:
                if ( aValue->longType <= sLong )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_DOUBLE:
                if ( aValue->doubleType <= sDouble )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_DATE:
                IDE_TEST( mtdDateInterface::dateDiff( &sLong,
                                                      &sDateType,
                                                      &aValue->dateType,
                                                      aValue->dateField )
                          != IDE_SUCCESS );
                if ( sLong <= 0 )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NUMERIC:
                sNumeric2 = (mtdNumericType * )sBuffer;
                IDE_TEST( mtc::subtractFloat( sNumeric2,
                                              MTD_FLOAT_PRECISION_MAXIMUM,
                                              sNumeric1,
                                              (mtdNumericType * )aValue->numericType )
                          != IDE_SUCCESS );
                if ( MTD_NUMERIC_IS_POSITIVE( sNumeric2 ) == ID_TRUE )
                {
                    sResult = ID_TRUE;
                }
                else
                {
                    sResult = ID_FALSE;
                }
                break;
            case QMC_WND_WINDOW_VALUE_NULL:
                sResult = ID_TRUE;
                break;
            default:
                break;
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Update One ROW and NEXT Record
 *
 *   Temp Table���� Update�� �ʿ��� �� Row�� �÷��� UPDATE �ϰ� ���� ���ڵ带 �д´�.
 */
IDE_RC qmnWNST::updateOneRowNextRecord( qcTemplate * aTemplate,
                                        qmndWNST   * aDataPlan,
                                        qmdMtrNode * aAggrResultNode,
                                        qmcRowFlag * aFlag )
{
    qmdMtrNode  * sMtrNode;
    qmcRowFlag    sFlag = QMC_ROW_INITIALIZE;

    for ( sMtrNode = aAggrResultNode;
          sMtrNode != NULL;
          sMtrNode = sMtrNode->next )
    {
        IDE_TEST( sMtrNode->func.setMtr( aTemplate,
                                         sMtrNode,
                                         aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmcSortTemp::updateRow( aDataPlan->sortMgr )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 1;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    *aFlag = sFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By UNBOUNDED PRECEDING - N PRECEDING or N FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó�� ���� �����Ѵ�.
 *  End   Point �� N PRECEDING ���� Row�� N �� �� �����̴�.
 *                 N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ��ġ���� ó���� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ��Ƽ���� ó������ N PRECEDING�̶�� N �� ������ ����ϰ�
 *  N FOLLOWING �̶�� N �� ������ ����Ѵ�.
 */
IDE_RC qmnWNST::partitionUnPrecedPrecedFollowRange( qcTemplate  * aTemplate,
                                                    qmndWNST    * aDataPlan,
                                                    qmdMtrNode  * aOverColumnNode,
                                                    qmdMtrNode  * aOrderByColumn,
                                                    SLong         aEndValue,
                                                    SInt          aEndType,
                                                    qmdAggrNode * aAggrNode,
                                                    qmdMtrNode  * aAggrResultNode,
                                                    idBool        aIsPreceding )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    idBool             sResult;
    idBool             sIsLess;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess = ID_FALSE;
        sIsPreceding = aIsPreceding;
    }
    else
    {
        sIsLess = ID_TRUE;
        sIsPreceding = ( aIsPreceding == ID_TRUE ? ID_FALSE : ID_TRUE );
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� End Point�� N ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aEndValue,
                                         aEndType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            do
            {
                /* ���� ���� EndPoint�� ���� ���Ѵ�. */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                           != IDE_SUCCESS );
                if ( sResult == ID_TRUE )
                {
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                /* ���� Row�� ��´� */
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Row�� ���Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By UNBOUNDED PRECEDING - N PRECEDING or N FOLLOWING
 *
 *  Start Point �� UNBOUNDED PRECEDING ��Ƽ���� ó�� ���� �����Ѵ�.
 *  End   Point �� N PRECEDING ���� Row�� N �� �� �����̴�.
 *                 N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ó������ ���� Row�� �������� N PRECEDING�̶��
 *  N �� ������ ����ϰ� N FOLLOWING �̶�� N �� ������ ����Ѵ�.
 */
IDE_RC qmnWNST::orderUnPrecedPrecedFollowRange( qcTemplate  * aTemplate,
                                                qmndWNST    * aDataPlan,
                                                qmdMtrNode  * aOrderByColumn,
                                                SLong         aEndValue,
                                                SInt          aEndType,
                                                qmdAggrNode * aAggrNode,
                                                qmdMtrNode  * aAggrResultNode,
                                                idBool        aIsPreceding )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    idBool             sResult;
    idBool             sIsLess;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_FALSE;
        sIsPreceding = aIsPreceding;
    }
    else
    {
        sIsLess      = ID_TRUE;
        sIsPreceding = ( aIsPreceding == ID_TRUE ? ID_FALSE : ID_TRUE );
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� End Point�� N�� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ó������ �ǵ��� �´� */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
        {
            /* ���� ���� EndPoint�� ���� ���Ѵ�. */
            IDE_TEST( compareRangeValue( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         &sValue1,
                                         sIsLess,
                                         &sResult )
                      != IDE_SUCCESS );
            if ( sResult == ID_TRUE )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row�� ��´� */
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By CURRENT ROW - N FOLLOWING
 *
 *  Start Point �� CURRENT ROW       ���� Row���� �����Ѵ�.
 *  End   Point �� N       FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 */
IDE_RC qmnWNST::partitionCurrentFollowRange( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             qmdMtrNode  * aOrderByColumn,
                                             SLong         aEndValue,
                                             SInt          aEndType,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag          sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue   sValue1;
    idBool              sResult;
    idBool              sIsLess;
    idBool              sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_FALSE;
        sIsPreceding = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_TRUE;
        sIsPreceding = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row�� ���� End Point�� N ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue1,
                                     sIsPreceding )
                   != IDE_SUCCESS );

        IDE_TEST( execAggregation( aTemplate,
                                   aAggrNode,
                                   NULL,
                                   NULL,
                                   0 )
                  != IDE_SUCCESS );

        while ( 1 )
        {
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Record�� �д´� */
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���ڵ带 ���صȴ�. */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );

                /* ���� ��Ƽ�ǿ� ������ üũ�غ��� */
                if ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME )
                {
                    /* EndPoint�� �� ���� ����� ���غ���. */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_TRUE )
                    {
                        IDE_TEST( execAggregation( aTemplate,
                                                   aAggrNode,
                                                   NULL,
                                                   NULL,
                                                   0 )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( compareRows( aDataPlan,
                                   aOverColumnNode,
                                   &sFlag )
                      != IDE_SUCCESS );
            while ( ( sFlag & QMC_ROW_COMPARE_MASK ) == QMC_ROW_COMPARE_SAME )
            {
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                    &aDataPlan->partitionCursorInfo )
                          != IDE_SUCCESS );

                IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                  aDataPlan,
                                                  aAggrResultNode,
                                                  &sFlag )
                           != IDE_SUCCESS );
                if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            }
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By CURRENT ROW - N FOLLOWING
 *
 *  Start Point �� CURRENT ROW       ���� Row���� �����Ѵ�.
 *  End   Point �� N       FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 */
IDE_RC qmnWNST::orderCurrentFollowRange( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         qmdMtrNode  * aOrderByColumn,
                                         SLong         aEndValue,
                                         SInt          aEndType,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag          sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue   sValue1;
    idBool              sResult;
    idBool              sIsLess;
    idBool              sIsPreceding;

    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_FALSE;
        sIsPreceding = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_TRUE;
        sIsPreceding = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue1,
                                     sIsPreceding )
                   != IDE_SUCCESS );

        IDE_TEST( execAggregation( aTemplate,
                                   aAggrNode,
                                   NULL,
                                   NULL,
                                   0 )
                  != IDE_SUCCESS );

        while ( 1 )
        {
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                         != IDE_SUCCESS );
                if ( sResult == ID_TRUE )
                {
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( compareRows( aDataPlan,
                                   aOrderByColumn,
                                   &sFlag )
                      != IDE_SUCCESS );
            while ( ( sFlag & QMC_ROW_COMPARE_MASK ) == QMC_ROW_COMPARE_SAME )
            {
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                    &aDataPlan->partitionCursorInfo )
                          != IDE_SUCCESS );

                IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                  aDataPlan,
                                                  aAggrResultNode,
                                                  &sFlag )
                           != IDE_SUCCESS );
                if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOrderByColumn,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            }
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By N PRECEDING or N FOLLOWING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� N         PRECEDING ���� Row ���� N ���� �� �� ���� �����ϰų� Ȥ��
 *                 N         FOLLOWINg ���� Row ���� N ���� ���� �� ���� �����ؼ�
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *  PRECEDING�� ���Ǿ����� FOLLOWING�� ���Ǿ������� ���� ����� �޶�����.
 *
 *  ó�� ���� �ϸ� ��Ƽ���� ó���� ���� Row�� ��ġ�� �����Ѵ�.
 *  ���� Row���� START Point�� value�� ��´�. �� �� ��Ƽ���� ó������ ���ư� �� value��
 *  ���ؼ� Skip�ѵڿ� ��Ƽ���� ������ Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionPrecedFollowUnFollowRange( qcTemplate  * aTemplate,
                                                    qmndWNST    * aDataPlan,
                                                    qmdMtrNode  * aOverColumnNode,
                                                    qmdMtrNode  * aOrderByColumn,
                                                    SLong         aStartValue,
                                                    SInt          aStartType,
                                                    qmdAggrNode * aAggrNode,
                                                    qmdMtrNode  * aAggrResultNode,
                                                    idBool        aIsPreceding )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsPreceding = aIsPreceding;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsPreceding = ( aIsPreceding == ID_TRUE ? ID_FALSE : ID_TRUE );
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� Start Point�� N ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aStartValue,
                                         aStartType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó������ �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            sSkipEnd = ID_FALSE;
            do
            {
                if ( sSkipEnd == ID_FALSE )
                {
                    /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult )
                              != IDE_SUCCESS );
                }
                else
                {
                    sResult = ID_TRUE;
                }
                if ( sResult == ID_TRUE )
                {
                    /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                    sSkipEnd = ID_TRUE;
                }
                else
                {
                    /* Start �� ���̹Ƿ� Skip�Ѵ� */
                    sSkipEnd = ID_FALSE;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                /* ���� Row�� �д´� */
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Row�� ���Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By N PRECEDING or N FOLLOWING - UNBOUNDED FOLLOWING
 *
 *  Start Point �� N         PRECEDING ���� Row ���� N ���� �� �� ���� �����ϰų� Ȥ��
 *                 N         FOLLOWINg ���� Row ���� N ���� ���� �� ���� �����ؼ�
 *  End   Point �� UNBOUNDED FOLLOWING ��Ƽ���� ������ �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *  PRECEDING�� ���Ǿ����� FOLLOWING�� ���Ǿ������� ���� ����� �޶�����.
 *
 *  ó�� ���� Row�� ��ġ�� �����Ѵ�.
 *  ���� Row���� START Point�� value�� ��´�. �� �� ó������ ���ư� �� value��
 *  ���ؼ� Skip�ѵڿ� ������ Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderPrecedFollowUnFollowRange( qcTemplate  * aTemplate,
                                                qmndWNST    * aDataPlan,
                                                qmdMtrNode  * aOrderByColumn,
                                                SLong         aStartValue,
                                                SInt          aStartType,
                                                qmdAggrNode * aAggrNode,
                                                qmdMtrNode  * aAggrResultNode,
                                                idBool        aIsPreceding )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsPreceding = aIsPreceding;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsPreceding = ( aIsPreceding == ID_TRUE ? ID_FALSE : ID_TRUE );
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aStartValue,
                                     aStartType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� Row�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        sSkipEnd                = ID_FALSE;
        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            if ( sSkipEnd == ID_FALSE )
            {
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                sResult = ID_TRUE;
            }
            if ( sResult == ID_TRUE )
            {
                /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                sSkipEnd = ID_TRUE;
            }
            else
            {
                /* Start �� ���� �̹Ƿ� Skip�Ѵ�. */
                sSkipEnd = ID_FALSE;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� ��ġ�� �ǵ��� �´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By N PRECEDING - CURRENT ROW
 *
 *  Start Point �� N       PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� CURRENT ROW       ���� Row �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint
 *  ���� Value�� ����� �̸� ���ؼ� ��Ƽ���� ó������ N�� �� Value���� Skip�ϰ� ���� Row��
 *  �� ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionPrecedCurrentRange( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             qmdMtrNode  * aOrderByColumn,
                                             SLong         aStartValue,
                                             SInt          aStartType,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aStartValue,
                                         aStartType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� Row�� ���� CURRENT ROW ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         0,
                                         aStartType,
                                         &sValue2,
                                         ID_TRUE )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó����ġ�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            sSkipEnd = ID_FALSE;
            do
            {
                if ( sSkipEnd == ID_FALSE )
                {
                    /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult )
                               != IDE_SUCCESS );
                }
                else
                {
                    sResult = ID_TRUE;
                }
                if ( sResult == ID_TRUE )
                {
                    /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                    sSkipEnd = ID_TRUE;
                }
                else
                {
                    /* Start �� ���� �̹Ƿ� Skip�Ѵ�. */
                    sSkipEnd = ID_FALSE;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }

                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� ���� Row�� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By N PRECEDING - CURRENT ROW
 *
 *  Start Point �� N       PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� CURRENT ROW       ���� Row �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint
 *  ���� Value�� ����� �̸� ���ؼ� ó������ N�� �� Value���� Skip�ϰ� ���� Row��
 *  �� ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderPrecedCurrentRange( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         qmdMtrNode  * aOrderByColumn,
                                         SLong         aStartValue,
                                         SInt          aStartType,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aStartValue,
                                     aStartType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� Row�� ���� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     0,
                                     aStartType,
                                     &sValue2,
                                     ID_TRUE )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        sSkipEnd                = ID_FALSE;
        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ó�� Record�� �����´� */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            if ( sSkipEnd == ID_FALSE )
            {
                /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                sResult = ID_TRUE;
            }
            if ( sResult == ID_TRUE )
            {
                /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                sSkipEnd = ID_TRUE;
            }
            else
            {
                /* Start �� ���� �̹Ƿ� Skip�Ѵ�. */
                sSkipEnd = ID_FALSE;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� ���� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By N PRECEDING - N PRECEDING
 *
 *  Start Point �� N PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N PRECEDING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ��Ƽ���� ó������ N�� �� Value���� Skip�ϰ� N�� �� Value
 *  ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionPrecedPrecedRange( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            qmdMtrNode  * aOrderByColumn,
                                            SLong         aStartValue,
                                            SInt          aStartType,
                                            SLong         aEndValue,
                                            SInt          aEndType,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
           QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aStartValue,
                                         aStartType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aEndValue,
                                         aEndType,
                                         &sValue2,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó����ġ�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            sSkipEnd = ID_FALSE;

            do
            {
                /* ���� ������ �������� ũ�ٸ� ����� */
                if ( aStartValue < aEndValue )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
                if ( sSkipEnd == ID_FALSE )
                {
                    /* ���� ���� EndPoint�� ���� ���ؼ� ���� Row�� End���� ������ �ʴ´ٸ� ������ */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult )
                              != IDE_SUCCESS );
                }
                else
                {
                    sResult = ID_TRUE;
                }
                if ( sResult == ID_TRUE )
                {
                    /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                    sSkipEnd = ID_TRUE;
                }
                else
                {
                    /* Start �� ���� �̹Ƿ� Skip�Ѵ�. */
                    sSkipEnd = ID_FALSE;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                /* ���� Row�� �д´� */
                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    /* Row�� ���Ѵ� */
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� EndPoint�� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                               != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By N PRECEDING - N PRECEDING
 *
 *  Start Point �� N PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N PRECEDING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ��Ƽ���� ó������ N�� �� Value���� Skip�ϰ� N�� �� Value
 *  ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderPrecedPrecedRange( qcTemplate  * aTemplate,
                                        qmndWNST    * aDataPlan,
                                        qmdMtrNode  * aOrderByColumn,
                                        SLong         aStartValue,
                                        SInt          aStartType,
                                        SLong         aEndValue,
                                        SInt          aEndType,
                                        qmdAggrNode * aAggrNode,
                                        qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aStartValue,
                                     aStartType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue2,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        sSkipEnd                = ID_FALSE;
        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            /* ���� ������ �������� ũ�ٸ� ����� */
            if ( aStartValue < aEndValue )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }

            if ( sSkipEnd == ID_FALSE )
            {
                /* ���� ���� EndPoint�� ���� ���ؼ� ���� Row�� End���� ������ �ʴ´ٸ� ������ */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue2,
                                             sIsMore,
                                             &sResult )
                          != IDE_SUCCESS );
                if ( sResult == ID_FALSE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }

                /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                sResult = ID_TRUE;
            }
            if ( sResult == ID_TRUE )
            {
                /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                sSkipEnd = ID_TRUE;
            }
            else
            {
                /* Start �� ���� �̹Ƿ� SkIP�Ѵ� */
                sSkipEnd = ID_FALSE;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row�� �д´� */
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� EndPoint�� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By N PRECEDING - N FOLLOWING
 *
 *  Start Point �� N PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ��Ƽ���� ó������ N�� �� Value���� Skip�ϰ� N�� �� Value
 *  ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionPrecedFollowRange( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            qmdMtrNode  * aOrderByColumn,
                                            SLong         aStartValue,
                                            SInt          aStartType,
                                            SLong         aEndValue,
                                            SInt          aEndType,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;
    idBool             sIsFollowing;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
        sIsFollowing = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
        sIsFollowing = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aStartValue,
                                         aStartType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aEndValue,
                                         aEndType,
                                         &sValue2,
                                         sIsFollowing )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó����ġ�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            sSkipEnd = ID_FALSE;
            do
            {
                if ( sSkipEnd == ID_FALSE )
                {
                    /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult )
                              != IDE_SUCCESS );
                }
                else
                {
                    sResult = ID_TRUE;
                }
                if ( sResult == ID_TRUE )
                {
                    /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                    sSkipEnd = ID_TRUE;
                }
                else
                {
                    /* Start �� ���� �̹Ƿ� Skip�Ѵ�. */
                    sSkipEnd = ID_FALSE;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� EndPoint�� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By N PRECEDING - N FOLLOWING
 *
 *  Start Point �� N PRECEDING ���� Row�� N �� ������ �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ó������ N�� �� Value���� Skip�ϰ� N�� �� Value
 *  ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderPrecedFollowRange( qcTemplate  * aTemplate,
                                        qmndWNST    * aDataPlan,
                                        qmdMtrNode  * aOrderByColumn,
                                        SLong         aStartValue,
                                        SInt          aStartType,
                                        SLong         aEndValue,
                                        SInt          aEndType,
                                        qmdAggrNode * aAggrNode,
                                        qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;
    idBool             sIsFollowing;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_TRUE;
        sIsFollowing = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_FALSE;
        sIsFollowing = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aStartValue,
                                     aStartType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue2,
                                     sIsFollowing )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        sSkipEnd = ID_FALSE;

        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            if ( sSkipEnd == ID_FALSE )
            {
                /* ���� ���� StartPoint�� ���� ���Ѵ�. */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                sResult = ID_TRUE;
            }
            if ( sResult == ID_TRUE )
            {
                /* Start �� �����̹Ƿ� ����� �����Ѵ�. */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                sSkipEnd = ID_TRUE;
            }
            else
            {
                /* Start �� ���� �̹Ƿ� SKIP�Ѵ� */
                sSkipEnd = ID_FALSE;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                if ( sSkipEnd == ID_TRUE )
                {
                    /* ���� Row�� EndPoint�� Value�� ������ ���غ��� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue2,
                                                 sIsMore,
                                                 &sResult )
                              != IDE_SUCCESS );
                    if ( sResult == ID_FALSE )
                    {
                        break;
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
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Partition By Order By N FOLLOWING - N FOLLOWING
 *
 *  Start Point �� N FOLLOWING ���� Row�� N �� �� ���� �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ��Ƽ���� ó���� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ������� Start Value��  N�� �� ������ End Value�� N�� ��
 *  �� ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::partitionFollowFollowRange( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            qmdMtrNode  * aOrderByColumn,
                                            SLong         aStartValue,
                                            SInt          aStartType,
                                            SLong         aEndValue,
                                            SInt          aEndType,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult1;
    idBool             sResult2;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
           QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        do
        {
            /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aStartValue,
                                         aStartType,
                                         &sValue1,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
            IDE_TEST( calculateInterval( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         aEndValue,
                                         aEndType,
                                         &sValue2,
                                         sIsPreceding )
                      != IDE_SUCCESS );

            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ��Ƽ���� ó����ġ�� �̵��Ѵ�. */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );
            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            sSkipEnd = ID_FALSE;
            do
            {
                /* ���� ���� �������� ���� ũ�ٸ� ����� */
                if ( aStartValue > aEndValue )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
                if ( sSkipEnd == ID_FALSE )
                {
                    /* ���� Row�� StartPoint ���� ���Ѵ� */
                    IDE_TEST( compareRangeValue( aTemplate,
                                                 aDataPlan->sortMgr,
                                                 aOrderByColumn,
                                                 aDataPlan->plan.myTuple->row,
                                                 &sValue1,
                                                 sIsLess,
                                                 &sResult1 )
                              != IDE_SUCCESS );
                }
                else
                {
                    sResult1 = ID_TRUE;
                }

                /* ���� Row�� EndPoint���� ���Ѵ� */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue2,
                                             sIsMore,
                                             &sResult2 )
                          != IDE_SUCCESS );
                if ( sResult2 == ID_FALSE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }

                if ( sResult1 == ID_TRUE )
                {
                    /* Start�� �����̹Ƿ� Aggregation�Ѵ� */
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
                    sSkipEnd = ID_TRUE;
                }
                else
                {
                    /* Start�� ���� �̹Ƿ� SKIP�Ѵ� */
                    sSkipEnd = ID_FALSE;
                }
                aDataPlan->mtrRowIdx    = 1;
                aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           &sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row�� �ǵ��� �´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * RANGE Order By N FOLLOWING - N FOLLOWING
 *
 *  Start Point �� N FOLLOWING ���� Row�� N �� �� ���� �����Ѵ�.
 *  End   Point �� N FOLLOWING ���� Row�� N �� �� �����̴�.
 *
 *  ORDER BY ������ ���� �÷��� ASC���� DESC������ �����ؼ� ���ؾ��� �ɼ��� �����Ѵ�.
 *
 *  ó�� �����ϸ� ���� Row�� ��ġ�� �����Ѵ�. ���� Row���� StartPoint,EndPoint
 *  ���� Value�� ����� �̸� ���ؼ� ������� Start Value��  N�� �� ������ End Value�� N�� ��
 *  �� ���� Aggregation�� �����Ѵ�.
 */
IDE_RC qmnWNST::orderFollowFollowRange( qcTemplate  * aTemplate,
                                        qmndWNST    * aDataPlan,
                                        qmdMtrNode  * aOrderByColumn,
                                        SLong         aStartValue,
                                        SInt          aStartType,
                                        SLong         aEndValue,
                                        SInt          aEndType,
                                        qmdAggrNode * aAggrNode,
                                        qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag         sFlag   = QMC_ROW_INITIALIZE;
    qmcWndWindowValue  sValue1;
    qmcWndWindowValue  sValue2;
    idBool             sResult1;
    idBool             sResult2;
    idBool             sSkipEnd;
    idBool             sIsLess;
    idBool             sIsMore;
    idBool             sIsPreceding;

    /* ORDER BY ������ ASC���� DESC������ ���� ��� �� �޶����� */
    if ( ( aOrderByColumn->myNode->flag & QMC_MTR_SORT_ORDER_MASK ) ==
            QMC_MTR_SORT_ASCENDING )
    {
        sIsLess      = ID_TRUE;
        sIsMore      = ID_FALSE;
        sIsPreceding = ID_FALSE;
    }
    else
    {
        sIsLess      = ID_FALSE;
        sIsMore      = ID_TRUE;
        sIsPreceding = ID_TRUE;
    }

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );

    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� Row�� ���� Start Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aStartValue,
                                     aStartType,
                                     &sValue1,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� Row�� ���� End Point�� N�� �� ���� ����Ѵ�. */
        IDE_TEST( calculateInterval( aTemplate,
                                     aDataPlan->sortMgr,
                                     aOrderByColumn,
                                     aDataPlan->plan.myTuple->row,
                                     aEndValue,
                                     aEndType,
                                     &sValue2,
                                     sIsPreceding )
                  != IDE_SUCCESS );

        /* ���� ��ġ�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        sSkipEnd                = ID_FALSE;
        aDataPlan->mtrRowIdx    = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            /* ���� ���� �������� ���� ũ�ٸ� ����� */
            if ( aStartValue > aEndValue )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }

            if ( sSkipEnd == ID_FALSE )
            {
                /* ���� Row�� StartPoint ���� ���Ѵ� */
                IDE_TEST( compareRangeValue( aTemplate,
                                             aDataPlan->sortMgr,
                                             aOrderByColumn,
                                             aDataPlan->plan.myTuple->row,
                                             &sValue1,
                                             sIsLess,
                                             &sResult1 )
                          != IDE_SUCCESS );
            }
            else
            {
                sResult1 = ID_TRUE;
            }

            /* ���� Row�� EndPoint���� ���Ѵ� */
            IDE_TEST( compareRangeValue( aTemplate,
                                         aDataPlan->sortMgr,
                                         aOrderByColumn,
                                         aDataPlan->plan.myTuple->row,
                                         &sValue2,
                                         sIsMore,
                                         &sResult2 )
                      != IDE_SUCCESS );
            if ( sResult2 == ID_FALSE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }

            if ( sResult1 == ID_TRUE )
            {
                /* Start�� �����̹Ƿ� Aggregation�Ѵ� */
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
                sSkipEnd = ID_TRUE;
            }
            else
            {
                /* Start�� ���� �̹Ƿ� SKIP�Ѵ� */
                sSkipEnd = ID_FALSE;
            }
            aDataPlan->mtrRowIdx    = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx = 0;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        /* ���� Row�� �ǵ��� �´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnWNST::getPositionValue( qcTemplate  * aTemplate,
                                  qmdAggrNode * aAggrNode,
                                  SLong       * aNumber )
{
    mtcStack  * sStack;
    mtcNode   * sArg1;
    mtcNode   * sArg2;
    SLong       sNumberValue = 0;

    /* lag, lead �Լ��� next�� ����. */
    IDE_DASSERT( aAggrNode->next == NULL );
    
    sArg1 = aAggrNode->dstNode->node.arguments;
    sArg2 = sArg1->next;

    if ( sArg2 != NULL )
    {
        IDE_TEST( qtc::calculate( (qtcNode*)sArg2, aTemplate )
                  != IDE_SUCCESS );

        sStack = aTemplate->tmplate.stack;
        
        /* bigint�� �޾��� */
        IDE_TEST_RAISE( sStack->column->module->id != MTD_BIGINT_ID,
                        ERR_INVALID_WINDOW_SPECIFICATION );
        
        sNumberValue = (SLong)(*(mtdBigintType*)sStack->value);
        
        IDE_TEST_RAISE( sNumberValue < 0, ERR_INVALID_WINDOW_SPECIFICATION );

        *aNumber = sNumberValue;
    }
    else
    {
        *aNumber = 1;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnWNST::checkNullAggregation( qcTemplate  * aTemplate,
                                      qmdAggrNode * aAggrNode,
                                      idBool      * aIsNull )
{
    mtcStack  * sStack;
    mtcNode   * sArg1;

    /* lag, lead �Լ��� next�� ����. */
    IDE_DASSERT( aAggrNode->next == NULL );
    
    sArg1 = aAggrNode->dstNode->node.arguments;

    IDE_DASSERT( sArg1 != NULL );
    
    IDE_TEST( qtc::calculate( (qtcNode*)sArg1, aTemplate )
              != IDE_SUCCESS );

    sStack = aTemplate->tmplate.stack;
    
    if ( sStack->column->module->isNull( sStack->column,
                                         sStack->value ) == ID_TRUE )
    {
        *aIsNull = ID_TRUE;
    }
    else
    {
        *aIsNull = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 *  PROJ-1804 Ranking Function
 */
IDE_RC qmnWNST::partitionOrderByLagAggr( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         qmdMtrNode  * aOverColumnNode,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode )
{
    SLong       sLagPoint    = 0;
    qmcRowFlag  sFlag        = QMC_ROW_INITIALIZE;
    SLong       sCount       = 0;
    SLong       sWindowPos   = 0;
    idBool      sIsNull      = ID_FALSE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó�� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        sWindowPos = 0;

        do
        {
            /* ���� ��ġ�� �����Ѵ�. */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
            
            IDE_TEST( getPositionValue( aTemplate, aAggrNode, &sLagPoint )
                      != IDE_SUCCESS );
        
            IDE_TEST( initAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            /* ��Ƽ���� ó������ ���ư��� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  & aDataPlan->partitionCursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;
            IDE_TEST( setTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );

            /* ��Ƽ���� ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
            for ( sCount = sWindowPos - sLagPoint;
                  sCount > 0;
                  sCount-- )
            {
                /* BUG-40279 lead, lag with ignore nulls */
                /* ignore nulls�� ����Ͽ� ���������� null�� �ƴ� ������ ���Ѵ�. */
                if ( aAggrNode->dstNode->node.module == &mtfLagIgnoreNulls )
                {
                    if ( sWindowPos >= sLagPoint )
                    {
                        IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                                  != IDE_SUCCESS );

                        if ( sIsNull == ID_FALSE )
                        {
                            IDE_TEST( execAggregation( aTemplate,
                                                       aAggrNode,
                                                       NULL,
                                                       NULL,
                                                       0 )
                                      != IDE_SUCCESS );
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
                    /* Nothing to do */
                }

                IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );
            }

            if ( sWindowPos >= sLagPoint )
            {
                /* BUG-40279 lead, lag with ignore nulls */
                if ( aAggrNode->dstNode->node.module == &mtfLagIgnoreNulls )
                {
                    IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                              != IDE_SUCCESS );
                }
                else
                {
                    sIsNull = ID_FALSE;
                }

                if ( sIsNull == ID_FALSE )
                {
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );
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

            IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                      != IDE_SUCCESS );

            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
            IDE_TEST( updateOneRowNextRecord( aTemplate,
                                              aDataPlan,
                                              aAggrResultNode,
                                              &sFlag )
                       != IDE_SUCCESS );

            ++sWindowPos;
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        } while ( ( sFlag & QMC_ROW_GROUP_MASK ) == QMC_ROW_GROUP_SAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1804 Ranking Function */
IDE_RC qmnWNST::orderByLagAggr( qcTemplate  * aTemplate,
                                qmndWNST    * aDataPlan,
                                qmdAggrNode * aAggrNode,
                                qmdMtrNode  * aAggrResultNode )
{
    SLong       sLagPoint    = 0;
    qmcRowFlag  sFlag        = QMC_ROW_INITIALIZE;
    SLong       sCount       = 0;
    SLong       sWindowPos   = 0;
    idBool      sIsNull      = ID_FALSE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    sWindowPos = 0;
    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( getPositionValue( aTemplate, aAggrNode, &sLagPoint )
                  != IDE_SUCCESS );
        
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ó������ ���ư��� */
        IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                  != IDE_SUCCESS );

        /* ��Ƽ���� ó������ ���� Row���� N�� ������ SKIP �Ѵ� */
        for ( sCount = sWindowPos - sLagPoint;
              sCount > 0;
              sCount-- )
        {
            /* BUG-40279 lead, lag with ignore nulls */
            /* ignore nulls�� ����Ͽ� ���������� null�� �ƴ� ������ ���Ѵ�. */
            if ( aAggrNode->dstNode->node.module == &mtfLagIgnoreNulls )
            {
                if ( sWindowPos >= sLagPoint )
                {
                    IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                              != IDE_SUCCESS );

                    if ( sIsNull == ID_FALSE )
                    {
                        IDE_TEST( execAggregation( aTemplate,
                                                   aAggrNode,
                                                   NULL,
                                                   NULL,
                                                   0 )
                                  != IDE_SUCCESS );
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
                /* Nothing to do */
            }

            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
        }

        if ( sWindowPos >= sLagPoint )
        {
            /* BUG-40279 lead, lag with ignore nulls */
            if ( aAggrNode->dstNode->node.module == &mtfLagIgnoreNulls )
            {
                IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsNull = ID_FALSE;
            }

            if ( sIsNull == ID_FALSE )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
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

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );

        ++sWindowPos;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1804 Ranking Function */
IDE_RC qmnWNST::partitionOrderByLeadAggr( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdMtrNode  * aOverColumnNode,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode )
{
    SLong       sLeadPoint = 0;
    qmcRowFlag  sFlag      = QMC_ROW_INITIALIZE;
    SLong       sCount     = 0;
    idBool      sIsNull    = ID_FALSE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( getPositionValue( aTemplate, aAggrNode, &sLeadPoint )
                  != IDE_SUCCESS );
        
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        aDataPlan->mtrRowIdx    = 1;
        aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

        for ( sCount = sLeadPoint; sCount > 0; sCount-- )
        {
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
            if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                /* ���� ��Ƽ�������� üũ�Ѵ� */
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       &sFlag )
                          != IDE_SUCCESS );
                if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                break;
            }
        }

        if ( sCount == 0 )
        {
            /* BUG-40279 lead, lag with ignore nulls */
            /* ignore nulls�� ����Ͽ� ������ null�� �ƴ� ������ ã�´�. */
            if ( aAggrNode->dstNode->node.module == &mtfLeadIgnoreNulls )
            {
                while ( 1 )
                {
                    IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                              != IDE_SUCCESS );
                    if ( sIsNull == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                
                    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                              != IDE_SUCCESS );
                    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                    {
                        /* ���� ��Ƽ�������� üũ�Ѵ� */
                        IDE_TEST( compareRows( aDataPlan,
                                               aOverColumnNode,
                                               &sFlag )
                                  != IDE_SUCCESS );
                        if ( ( sFlag & QMC_ROW_GROUP_MASK ) != QMC_ROW_GROUP_SAME )
                        {
                            break;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                sIsNull = ID_FALSE;
            }

            if ( sIsNull == ID_FALSE )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
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

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
        IDE_TEST( updateAggrRows( aTemplate,
                                  aDataPlan,
                                  aAggrResultNode,
                                  &sFlag,
                                  1 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1804 Ranking Function */
IDE_RC qmnWNST::orderByLeadAggr( qcTemplate  * aTemplate,
                                 qmndWNST    * aDataPlan,
                                 qmdAggrNode * aAggrNode,
                                 qmdMtrNode  * aAggrResultNode )
{
    SLong       sLeadPoint = 0;
    qmcRowFlag  sFlag      = QMC_ROW_INITIALIZE;
    SLong       sCount     = 0;
    idBool      sIsNull    = ID_FALSE;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ���� ��ġ�� �����Ѵ�. */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        IDE_TEST( getPositionValue( aTemplate, aAggrNode, &sLeadPoint )
                  != IDE_SUCCESS );
        
        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        for ( sCount = sLeadPoint; sCount > 0; sCount-- )
        {
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
            if ( ( sFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_EXIST )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sCount == 0 )
        {
            /* BUG-40279 lead, lag with ignore nulls */
            /* ignore nulls�� ����Ͽ� ������ null�� �ƴ� ������ ã�´�. */
            if ( aAggrNode->dstNode->node.module == &mtfLeadIgnoreNulls )
            {
                while ( 1 )
                {
                    IDE_TEST( checkNullAggregation( aTemplate, aAggrNode, &sIsNull )
                              != IDE_SUCCESS );
                    if ( sIsNull == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                
                    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                              != IDE_SUCCESS );
                    if ( ( sFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_EXIST )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            else
            {
                sIsNull = ID_FALSE;
            }

            if ( sIsNull == ID_FALSE )
            {
                IDE_TEST( execAggregation( aTemplate,
                                           aAggrNode,
                                           NULL,
                                           NULL,
                                           0 )
                          != IDE_SUCCESS );
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

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ���� Row ��ġ�� �ǵ��ƿ´� */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              &aDataPlan->cursorInfo )
                  != IDE_SUCCESS );

        aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

        IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
        IDE_TEST( updateOneRowNextRecord( aTemplate,
                                          aDataPlan,
                                          aAggrResultNode,
                                          &sFlag )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnWNST::getMinLimitValue( qcTemplate * aTemplate,
                                  qmdWndNode * aWndNode,
                                  SLong      * aNumber )
{
    qmdWndNode   * sWndNode;
    qmdAggrNode  * sAggrNode;
    mtcStack     * sStack;
    mtcNode      * sArg1;
    SLong          sNumberValue;
    SLong          sMaxNumberValue = ID_SLONG_MAX;

    for( sWndNode = aWndNode;
         sWndNode != NULL;
         sWndNode = sWndNode->next )
    {
        for ( sAggrNode = sWndNode->aggrNode;
              sAggrNode != NULL;
              sAggrNode = sAggrNode->next )
        {
            /* row_number_limit */
            IDE_TEST_RAISE( sAggrNode->dstNode->node.module != &mtfRowNumberLimit,
                            ERR_INVALID_FUNCTION );
            
            sArg1 = sAggrNode->dstNode->node.arguments;

            if ( sArg1 != NULL )
            {
                IDE_TEST( qtc::calculate( (qtcNode*)sArg1, aTemplate )
                          != IDE_SUCCESS );

                sStack = aTemplate->tmplate.stack;

                /* bigint�� �޾��� */
                IDE_TEST_RAISE( sStack->column->module->id != MTD_BIGINT_ID,
                                ERR_INVALID_WINDOW_SPECIFICATION );

                sNumberValue = (SLong)(*(mtdBigintType*)sStack->value);

                if ( ( sNumberValue > 0 ) && ( sNumberValue < sMaxNumberValue ) )
                {
                    sMaxNumberValue = sNumberValue;
                }
                else
                {
                    /* Nothing to do */
                    // sMaxNumberValue = ID_SLONG_MAX;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    *aNumber = sMaxNumberValue;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnWNST::getMinLimitValue",
                                  "Invalid analytic function" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-43086 support Ntile analytic function */
IDE_RC qmnWNST::getNtileValue( qcTemplate  * aTemplate,
                               qmdAggrNode * aAggrNode,
                               SLong       * aNumber )
{
    mtcStack  * sStack;
    mtcNode   * sArg1;
    SLong       sNumberValue = 0;

    sArg1 = aAggrNode->dstNode->node.arguments;

    IDE_TEST( qtc::calculate( (qtcNode*)sArg1, aTemplate )
            != IDE_SUCCESS );

    sStack = aTemplate->tmplate.stack;

    /* bigint�� �޾��� */
    IDE_TEST_RAISE( sStack->column->module->id != MTD_BIGINT_ID,
            ERR_INVALID_WINDOW_SPECIFICATION );

    sNumberValue = (SLong)(*(mtdBigintType*)sStack->value);

    IDE_TEST_RAISE( ( sNumberValue <= 0 ) && ( sNumberValue != MTD_BIGINT_NULL ),
                    ERR_INVALID_WINDOW_SPECIFICATION );

    *aNumber = sNumberValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_WINDOW_SPECIFICATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_WINDOW_INVALID_AGGREGATION ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-43086 support Ntile analytic function */
IDE_RC qmnWNST::partitionOrderByNtileAggr( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag  sFlag       = QMC_ROW_INITIALIZE;
    SLong       sSkipCount  = 0;
    SLong       sNtileValue = 0;
    SLong       sRowCount   = 0;
    SLong       sQuotient   = 0;
    SLong       sRemainder  = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );
    
    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        /* ��Ƽ���� ó���� cursor�� �����Ѵ� */
        IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                            &aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );
        
        IDE_TEST( getNtileValue( aTemplate, aAggrNode, &sNtileValue )
                  != IDE_SUCCESS );

        sRowCount   = 0;
        do
        {
            sRowCount++;
            aDataPlan->mtrRowIdx = 1;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];
            IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                      != IDE_SUCCESS );
            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( compareRows( aDataPlan,
                                       aOverColumnNode,
                                       & sFlag )
                          != IDE_SUCCESS );
            }
            else
            {
                // Data�� ������ ����
                break;
            }
        } while( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME );

        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            /* ���� ��ġ�� �����Ѵ� */
            IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMgr,
                                                &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( initAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        /* ��Ƽ���� ó������ Cursor�� �̵��Ѵ�. */
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                              & aDataPlan->partitionCursorInfo )
                  != IDE_SUCCESS );

        sFlag = 0;
        if ( sNtileValue == MTD_BIGINT_NULL )
        {
            while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                  aDataPlan,
                                                  aAggrResultNode,
                                                  &sFlag )
                           != IDE_SUCCESS );
                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           & sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Data�� ������ ����
                    break;
                }
            }
        }
        else
        {
            sQuotient  = sRowCount / sNtileValue;
            sRemainder = sRowCount % sNtileValue;

            do
            {
                if ( sSkipCount < 1 )
                {
                    IDE_TEST( execAggregation( aTemplate,
                                               aAggrNode,
                                               NULL,
                                               NULL,
                                               0 )
                              != IDE_SUCCESS );

                    if ( ( sRemainder > 0 ) && ( sQuotient > 0 ) )
                    {
                        sRemainder--;
                        sSkipCount = sQuotient;
                    }
                    else
                    {
                        sSkipCount = sQuotient - 1;
                    }
                }
                else
                {
                    sSkipCount--;
                }

                IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                          != IDE_SUCCESS );
                aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

                /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
                IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                  aDataPlan,
                                                  aAggrResultNode,
                                                  &sFlag )
                           != IDE_SUCCESS );

                if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                {
                    IDE_TEST( compareRows( aDataPlan,
                                           aOverColumnNode,
                                           & sFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Data�� ������ ����
                    break;
                }
            } while ( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME );
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );

        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            aDataPlan->mtrRowIdx = 0;
            aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

            /* ���� Row ��ġ�� �ǵ��ƿ´� */
            IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMgr,
                                                  &aDataPlan->cursorInfo )
                      != IDE_SUCCESS );

            aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

            IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-43086 support Ntile analytic function */
IDE_RC qmnWNST::orderByNtileAggr( qcTemplate  * aTemplate,
                                  qmndWNST    * aDataPlan,
                                  qmdAggrNode * aAggrNode,
                                  qmdMtrNode  * aAggrResultNode )
{
    qmcRowFlag  sFlag       = QMC_ROW_INITIALIZE;
    SLong       sSkipCount  = 0;
    SLong       sNtileValue = 0;
    SLong       sRowCount   = 0;
    SLong       sQuotient   = 0;
    SLong       sRemainder  = 0;

    IDE_TEST( qmcSortTemp::setUpdateColumnList( aDataPlan->sortMgr,
                                                aAggrResultNode )
              != IDE_SUCCESS );
    aDataPlan->mtrRowIdx    = 0;
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx];

    IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
              != IDE_SUCCESS );

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( getNtileValue( aTemplate, aAggrNode, &sNtileValue )
                  != IDE_SUCCESS );
            
        IDE_TEST( initAggregation( aTemplate,
                                   aAggrNode )
                  != IDE_SUCCESS );

        if ( sNtileValue == MTD_BIGINT_NULL )
        {
            while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                  aDataPlan,
                                                  aAggrResultNode,
                                                  &sFlag )
                           != IDE_SUCCESS );
            }
        }
        else
        {
            while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    sRowCount++;
                    IDE_TEST( getNextRecord( aTemplate, aDataPlan, &sFlag )
                              != IDE_SUCCESS );
                }

                sQuotient  = sRowCount / sNtileValue;
                sRemainder = sRowCount % sNtileValue;

                /* ó�� Row ��ġ�� �ǵ��ƿ´� */
                IDE_TEST( getFirstRecord( aTemplate, aDataPlan, &sFlag )
                          != IDE_SUCCESS );

                while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
                {
                    if ( sSkipCount < 1 )
                    {
                        IDE_TEST( execAggregation( aTemplate,
                                                   aAggrNode,
                                                   NULL,
                                                   NULL,
                                                   0 )
                                  != IDE_SUCCESS );

                        if ( ( sRemainder > 0 ) && ( sQuotient > 0 ) )
                        {
                            sRemainder--;
                            sSkipCount = sQuotient;
                        }
                        else
                        {
                            sSkipCount = sQuotient - 1;
                        }
                    }
                    else
                    {
                        sSkipCount--;
                    }

                    IDE_TEST( setTupleSet( aTemplate, aDataPlan->mtrNode, aDataPlan->plan.myTuple->row )
                              != IDE_SUCCESS );
                    aDataPlan->mtrRow[aDataPlan->mtrRowIdx] = aDataPlan->plan.myTuple->row;

                    /* Aggregation�� ���� update�ϰ� ���� Row�� �д´�. */
                    IDE_TEST( updateOneRowNextRecord( aTemplate,
                                                      aDataPlan,
                                                      aAggrResultNode,
                                                      &sFlag )
                               != IDE_SUCCESS );
                }
            }
        }

        IDE_TEST( finiAggregation( aTemplate, aAggrNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

