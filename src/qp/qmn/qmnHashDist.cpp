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
 * $Id: qmnHashDist.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     HSDS(HaSh DiStinct) Node
 *
 *     ������ �𵨿��� hash-based distinction ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Hash-based Distinction
 *         - Set Union
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnHashDist.h>
#include <qmxResultCache.h>

IDE_RC
qmnHSDS::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    HSDS ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHSDS * sCodePlan = (qmncHSDS *) aPlan;
    qmndHSDS * sDataPlan =
        (qmndHSDS *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;
    idBool     sDependency;

    sDataPlan->doIt = qmnHSDS::doItDefault;

    //----------------------------------------
    // ���� �ʱ�ȭ ����
    //----------------------------------------

    if ( (*sDataPlan->flag & QMND_HSDS_INIT_DONE_MASK)
         == QMND_HSDS_INIT_DONE_FALSE )
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

        IDE_TEST( qmcHashTemp::clear( sDataPlan->hashMgr )
                  != IDE_SUCCESS );

        //----------------------------------------
        // Child�� �ݺ� �����Ͽ� Temp Table�� ����
        //----------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        if ( ( sCodePlan->flag & QMNC_HSDS_IN_TOP_MASK )
             == QMNC_HSDS_IN_TOP_FALSE )
        {
            IDE_TEST( qmnHSDS::doItDependent( aTemplate, aPlan, & sFlag )
                      != IDE_SUCCESS );

            while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( qmnHSDS::doItDependent( aTemplate, aPlan, & sFlag )
                          != IDE_SUCCESS );
            }

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
            // Top Query���� ���Ǵ� ��� �ʱ�ȭ �������� Data�� ��������
            // �ʰ� ���� �������� Data ���� �� ��� ������ �����Ѵ�.
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

    if ( ( sCodePlan->flag & QMNC_HSDS_IN_TOP_MASK )
         == QMNC_HSDS_IN_TOP_FALSE )
    {
        // �̹� ������ ����� �����Ѵ�.
        sDataPlan->doIt = qmnHSDS::doItFirstIndependent;
    }
    else
    {
        if ( ( *sDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
             == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
        {
            if ( ( *sDataPlan->resultData.flag & QMX_RESULT_CACHE_STORED_MASK )
                 == QMX_RESULT_CACHE_STORED_TRUE )
            {
                // �̹� ������ ����� �����Ѵ�.
                sDataPlan->doIt = qmnHSDS::doItFirstIndependent;
            }
            else
            {
                // ���� ���� �߿� Temp Table�� �����ϰ�,
                // ��� �����ø��� �����Ѵ�.
                sDataPlan->doIt = qmnHSDS::doItDependent;
            }
        }
        else
        {
            // ���� ���� �߿� Temp Table�� �����ϰ�,
            // ��� �����ø��� �����Ѵ�.
            sDataPlan->doIt = qmnHSDS::doItDependent;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnHSDS::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    HSDS �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncHSDS * sCodePlan = (qmncHSDS *) aPlan;
    qmndHSDS * sDataPlan =
        (qmndHSDS*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHSDS::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    HSDS ����� Tuple�� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    Child Plan�� Null Padding�� �����ϰ�,
 *    �ڽ��� Null Row�� Temp Table�κ��� ȹ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHSDS * sCodePlan = (qmncHSDS *) aPlan;
    qmndHSDS * sDataPlan =
        (qmndHSDS *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_HSDS_INIT_DONE_MASK)
         == QMND_HSDS_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }


    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    IDE_TEST( qmcHashTemp::getNullRow( sDataPlan->hashMgr,
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
qmnHSDS::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    HSDS ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHSDS * sCodePlan = (qmncHSDS*) aPlan;
    qmndHSDS * sDataPlan =
        (qmndHSDS*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndHSDS * sCacheDataPlan = NULL;
    idBool     sIsInit       = ID_FALSE;
    SLong sRecordCnt;
    ULong sPageCnt;
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

        if ( (*sDataPlan->flag & QMND_HSDS_INIT_DONE_MASK)
             == QMND_HSDS_INIT_DONE_TRUE )
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
                        "DISTINCT ( ITEM_SIZE: %"ID_UINT32_FMT", "
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
                        "DISTINCT ( ITEM_SIZE: BLOCKED, "
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
                        "DISTINCT ( ITEM_SIZE: %"ID_UINT32_FMT", "
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
                        "DISTINCT ( ITEM_SIZE: BLOCKED, "
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
                                      "DISTINCT ( ITEM_SIZE: 0, ITEM_COUNT: 0, "
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
                                  "DISTINCT ( ITEM_SIZE: ??, ITEM_COUNT: ??, "
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
            sCacheDataPlan = (qmndHSDS *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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

#undef IDE_FN
}

IDE_RC
qmnHSDS::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnHSDS::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHSDS::doItDependent( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���ο� Row ���� �� �ٷ� ����� ����
 *    Top Query���� ����� ��� ȣ��ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::doItDependent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncHSDS * sCodePlan = (qmncHSDS *) aPlan;
    qmndHSDS * sDataPlan =
        (qmndHSDS *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;
    idBool     sInserted;

    sInserted = ID_TRUE;

    //------------------------------
    // Child Record�� ����
    //------------------------------

    IDE_TEST( sCodePlan->plan.left->doIt( aTemplate,
                                          sCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //---------------------------------------
        // 1.  ���� Row�� ���� ���� �Ҵ�
        // 2.  ���� Row�� ����
        // 3.  ���� Row�� ����
        // 4.  ���� ���� ���ο� ���� ��� ����
        //---------------------------------------

        // 1.  ���� Row�� ���� ���� �Ҵ�
        if ( sInserted == ID_TRUE )
        {
            IDE_TEST( qmcHashTemp::alloc( sDataPlan->hashMgr,
                                          & sDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }
        else
        {
            // ������ ������ ���� �̹� �Ҵ���� ������ ����Ѵ�.
            // ����, ������ ������ �Ҵ� ���� �ʿ䰡 ����.
        }

        // 2.  ���� Row�� ����
        IDE_TEST( setMtrRow( aTemplate,
                             sDataPlan ) != IDE_SUCCESS );

        // 3.  ���� Row�� ����
        IDE_TEST( qmcHashTemp::addDistRow( sDataPlan->hashMgr,
                                           & sDataPlan->plan.myTuple->row,
                                           & sInserted )
                  != IDE_SUCCESS );

        // 4.  ���� ���� ���ο� ���� ��� ����
        if ( sInserted == ID_TRUE )
        {
            // ������ ������ ��� Distinct Row�̴�.
            // ����, �ش� ����� �����Ѵ�.
            break;
        }
        else
        {
            // ������ ������ ��� ������ Row�� �����ϰ� �ִ�.
            // ����, Child�� �ٽ� �����Ѵ�.
            IDE_TEST( sCodePlan->plan.left->doIt( aTemplate,
                                                  sCodePlan->plan.left,
                                                  & sFlag ) != IDE_SUCCESS );
        }
    }

    //------------------------------
    // ��� ����
    //------------------------------

    if ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS);
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }

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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHSDS::doItFirstIndependent( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Subquery ������ ���� ����� ���� ���� �Լ�
 *
 * Implementation :
 *    �̹� ������ Temp Table�κ��� �� ���� ���´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::doItFirstIndependent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndHSDS * sDataPlan =
        (qmndHSDS *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getFirstSequence( sDataPlan->hashMgr,
                                             & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );

        sDataPlan->doIt = qmnHSDS::doItNextIndependent;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHSDS::doItNextIndependent( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Subquery ������ ���� ����� ���� ���� �Լ�
 *
 * Implementation :
 *    �̹� ������ Temp Table�κ��� �� ���� ���´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::doItNextIndependent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndHSDS * sDataPlan =
        (qmndHSDS *) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;
    IDE_TEST( qmcHashTemp::getNextSequence( sDataPlan->hashMgr,
                                            & sSearchRow )
              != IDE_SUCCESS );
    sDataPlan->plan.myTuple->row = (sSearchRow == NULL ) ? sOrgRow : sSearchRow;

    if ( sSearchRow == NULL )
    {
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->doIt = qmnHSDS::doItFirstIndependent;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_EXIST;

        IDE_TEST( setTupleSet( aTemplate,
                               sDataPlan ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnHSDS::firstInit( qcTemplate * aTemplate,
                    qmncHSDS   * aCodePlan,
                    qmndHSDS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    HSDS node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndHSDS * sCacheDataPlan = NULL;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    //---------------------------------
    // HSDS ���� ������ �ʱ�ȭ
    //---------------------------------
    //
    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndHSDS *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
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

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );

    IDE_TEST( initHashNode( aDataPlan ) != IDE_SUCCESS );

    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    aDataPlan->depTuple =
        & aTemplate->tmplate.rows[aCodePlan->depTupleRowID];
    aDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_HSDS_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_HSDS_INIT_DONE_TRUE;

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
qmnHSDS::initMtrNode( qcTemplate * aTemplate,
                      qmncHSDS   * aCodePlan,
                      qmndHSDS   * aDataPlan )
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
qmnHSDS::initHashNode( qmndHSDS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Hashing Column�� ���� ��ġ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::initHashNode"
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

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnHSDS::initTempTable( qcTemplate * aTemplate,
                        qmncHSDS   * aCodePlan,
                        qmndHSDS   * aDataPlan )
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
    qmndHSDS * sCacheDataPlan = NULL;

    //-----------------------------
    // ���ռ� �˻�
    //-----------------------------

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
    /* QMNC_HSDS_IN_TOP_TRUE �϶��� temp table �� ������ �������� �ʰ�
     * 1 row �� insert �ϰ� ���� �÷����� insert �� rid �� �״�� �����ϰ� �ȴ�.
     * ������ hash temp table �� index�� ��������� rid �� ����ɼ� �ִ�.
     * ���� SM ���� rid �� �������� �ʵ��� ��û�� �ؾ� �Ѵ�. */
    if ( (aCodePlan->flag & QMNC_HSDS_IN_TOP_MASK) == QMNC_HSDS_IN_TOP_TRUE )
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
        IDU_FIT_POINT( "qmnHSDS::initTempTable::qmxAlloc:hashMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                  (void **)&aDataPlan->hashMgr )
                  != IDE_SUCCESS );
        IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     aDataPlan->hashNode,
                                     NULL,  // Aggregation Column����
                                     aCodePlan->bucketCnt,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndHSDS *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnHSDS::initTempTable::qrcAlloc:hashMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdHashTemp ),
                                                               (void **)&aDataPlan->hashMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcHashTemp::init( aDataPlan->hashMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         aDataPlan->hashNode,
                                         NULL,  // Aggregation Column����
                                         aCodePlan->bucketCnt,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->hashMgr = aDataPlan->hashMgr;
        }
        else
        {
            aDataPlan->hashMgr = sCacheDataPlan->hashMgr;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnHSDS::checkDependency( qcTemplate * aTemplate,
                                 qmncHSDS   * aCodePlan,
                                 qmndHSDS   * aDataPlan,
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
qmnHSDS::setMtrRow( qcTemplate * aTemplate,
                    qmndHSDS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::setMtrRow"
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
qmnHSDS::setTupleSet( qcTemplate * aTemplate,
                      qmndHSDS   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     �˻��� ���� Row�� �̿��Ͽ� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnHSDS::setTupleSet"
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
