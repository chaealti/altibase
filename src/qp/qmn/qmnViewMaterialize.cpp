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
 * $Id: qmnViewMaterialize.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     VMTR(View MaTeRialization) Node
 *
 *     ������ �𵨿��� View�� ���� Materialization�� �����ϴ� Node�̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnViewMaterialize.h>
#include <qmxResultCache.h>

IDE_RC qmnVMTR::checkDependency( qcTemplate * aTemplate,
                                 qmncVMTR   * aCodePlan,
                                 qmndVMTR   * aDataPlan,
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
qmnVMTR::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    VMTR ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncVMTR * sCodePlan = (qmncVMTR *) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR *) (aTemplate->tmplate.data + aPlan->offset);

    idBool     sDependency;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    // first initialization
    if ( (*sDataPlan->flag & QMND_VMTR_INIT_DONE_MASK)
         == QMND_VMTR_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }

    IDE_TEST( checkDependency( aTemplate,
                               sCodePlan,
                               sDataPlan,
                               &sDependency ) != IDE_SUCCESS );
    
    if ( sDependency == ID_TRUE )
    {
        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------
        
        IDE_TEST( qmcSortTemp::clear( sDataPlan->sortMgr ) != IDE_SUCCESS );
   
        IDE_TEST( storeChild( aTemplate, sCodePlan, sDataPlan ) != IDE_SUCCESS );

        //----------------------------------------
        // Temp Table ���� �� �ʱ�ȭ
        //----------------------------------------

        sDataPlan->depValue = sDataPlan->depTuple->modify;
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
qmnVMTR::doIt( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnVMTR::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVMTR::padNull( qcTemplate * /* aTemplate */,
                  qmnPlan    * /* aPlan */)
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVMTR::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *   VMTR ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    qmndVMTR * sCacheDataPlan = NULL;
    idBool     sIsInit       = ID_FALSE;

    ULong sPageCount;
    SLong sRecordCount;

    ULong i;

    if ( ( *sDataPlan->flag & QMND_VMTR_PRINTED_MASK )
         == QMND_VMTR_PRINTED_FALSE )
    {
        // VMTR ���� ���� ���� ���� Plan Node�� ������.
        // ����, �� ���� ��µǵ��� �Ѵ�.

        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }

        if ( aMode == QMN_DISPLAY_ALL )
        {
            if ( (*sDataPlan->flag & QMND_VMTR_INIT_DONE_MASK)
                 == QMND_VMTR_INIT_DONE_TRUE )
            {
                sIsInit = ID_TRUE;
                IDE_TEST( qmcSortTemp::getDisplayInfo( sDataPlan->sortMgr,
                                                       & sPageCount,
                                                       & sRecordCount )
                          != IDE_SUCCESS );

                if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                     == QMN_PLAN_STORAGE_MEMORY )
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "MATERIALIZATION ( "
                            "ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT,
                            sDataPlan->viewRowSize,
                            sRecordCount );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "MATERIALIZATION ( "
                            "ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT,
                            sRecordCount );
                    }
                }
                else
                {
                    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
                    {
                        iduVarStringAppendFormat(
                            aString,
                            "MATERIALIZATION ( "
                            "ITEM_SIZE: %"ID_UINT32_FMT", "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: %"ID_UINT64_FMT,
                            sDataPlan->viewRowSize,
                            sRecordCount,
                            sPageCount );
                    }
                    else
                    {
                        // BUG-29209
                        // ITEM_SIZE, DISK_PAGE_COUNT ���� �������� ����
                        iduVarStringAppendFormat(
                            aString,
                            "MATERIALIZATION ( "
                            "ITEM_SIZE: BLOCKED, "
                            "ITEM_COUNT: %"ID_INT64_FMT", "
                            "DISK_PAGE_COUNT: BLOCKED",
                            sRecordCount );
                    }
                }
            }
            else
            {
                iduVarStringAppend( aString,
                                    "MATERIALIZATION ( "
                                    "ITEM_SIZE: 0, ITEM_COUNT: 0" );

            }
        }
        else
        {
            iduVarStringAppend( aString,
                                "MATERIALIZATION ( "
                                "ITEM_SIZE: ??, ITEM_COUNT: ??" );

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
                sCacheDataPlan = (qmndVMTR *) (aTemplate->resultCache.data + sCodePlan->plan.offset);
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
                               sCodePlan->depTupleID,
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

        IDE_TEST( aPlan->left->printPlan( aTemplate,
                                          aPlan->left,
                                          aDepth + 1,
                                          aString,
                                          aMode ) != IDE_SUCCESS );

        *sDataPlan->flag &= ~QMND_VMTR_PRINTED_MASK;
        *sDataPlan->flag |= QMND_VMTR_PRINTED_TRUE;
    }
    else
    {
        // �̹� Plan������ ��µ� ������
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVMTR::getCursorInfo( qcTemplate       * aTemplate,
                        qmnPlan          * aPlan,
                        void            ** aTableHandle,
                        void            ** aIndexHandle,
                        qmcdMemSortTemp ** aMemSortTemp,
                        qmdMtrNode      ** aMemSortRecord )
{
/***********************************************************************
 *
 * Description :
 *    VSCN������ ȣ���� �� ������, Temp Table�� ���� ������
 *    ���� ������ ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::getCursorInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( qmcSortTemp::getCursorInfo( sDataPlan->sortMgr,
                                          aTableHandle,
                                          aIndexHandle,
                                          aMemSortTemp,
                                          aMemSortRecord )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnVMTR::getNullRowDisk( qcTemplate       * aTemplate,
                                qmnPlan          * aPlan,
                                void             * aRow,
                                scGRID           * aRowRid )
{
/***********************************************************************
 *
 * Description :
 *    VSCN������ ȣ���� �� ������, Temp Table�κ��� Null Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->mtrRow = sDataPlan->mtrNode->dstTuple->row;

    IDE_TEST( qmcSortTemp::getNullRow( sDataPlan->sortMgr,
                                       & sDataPlan->mtrRow )
              != IDE_SUCCESS );

    IDE_DASSERT( sDataPlan->mtrRow != NULL );

    idlOS::memcpy( aRow,
                   sDataPlan->mtrRow,
                   sDataPlan->sortMgr->nullRowSize );
    idlOS::memcpy( aRowRid,
                   & sDataPlan->mtrNode->dstTuple->rid,
                   ID_SIZEOF(scGRID) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/**
 * PROJ-2462 ResultCache
 *
 * Memory Temp�� NullRow���� ��� �״�� �����ϸ�ȴ�. 
 */
IDE_RC qmnVMTR::getNullRowMemory( qcTemplate *  aTemplate,
                                  qmnPlan    *  aPlan,
                                  void       ** aRow )
{
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->mtrRow = sDataPlan->mtrNode->dstTuple->row;

    IDE_TEST( qmcSortTemp::getNullRow( sDataPlan->sortMgr,
                                       & sDataPlan->mtrRow )
              != IDE_SUCCESS );

    IDE_DASSERT( sDataPlan->mtrRow != NULL );

    *aRow = sDataPlan->mtrRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnVMTR::getNullRowSize( qcTemplate       * aTemplate,
                         qmnPlan          * aPlan,
                         UInt             * aRowSize )
{
/***********************************************************************
 *
 * Description :
 *    VSCN������ ȣ���� �� ������, Row Size�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::getNullRowSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    *aRowSize = sDataPlan->sortMgr->nullRowSize;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnVMTR::getTuple( qcTemplate       * aTemplate,
                   qmnPlan          * aPlan,
                   mtcTuple        ** aTuple )
{
/***********************************************************************
 *
 * Description :
 *    VSCN������ ȣ���� �� ������, Tuple�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::getRowSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    *aTuple = sDataPlan->mtrNode->dstTuple;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qmnVMTR::getMtrNode( qcTemplate       * aTemplate,
                            qmnPlan          * aPlan,
                            qmdMtrNode      ** aNode )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    SREC������ ȣ���� �� ������, mtrNode�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    *aNode = sDataPlan->mtrNode;

    return IDE_SUCCESS;
    
}

IDE_RC
qmnVMTR::firstInit( qcTemplate * aTemplate,
                    qmncVMTR   * aCodePlan,
                    qmndVMTR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    VMTR node�� Data ������ ����� ���� �ʱ�ȭ�� �����ϰ�,
 *    Child�� ���� ����� ������.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmndVMTR * sCacheDataPlan = NULL;

    //---------------------------------
    // VMTR ���� ������ �ʱ�ȭ
    //---------------------------------

    /* PROJ-2462 Result Cache */
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_TRUE )
    {
        sCacheDataPlan = (qmndVMTR *) (aTemplate->resultCache.data + aCodePlan->plan.offset);
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

    // ���� Column�� �ʱ�ȭ
    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );
    
    // Temp Table�� �ʱ�ȭ
    IDE_TEST( initTempTable( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // View Row�� ũ�� �ʱ�ȭ
    aDataPlan->viewRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    // View Row�� ���� ���� �ʱ�ȭ
    //    - Memory Temp Table�� ����� ��� �ǹ� ����
    aDataPlan->mtrRow = aDataPlan->mtrNode->dstTuple->row;

    aDataPlan->myTuple  = aDataPlan->mtrNode->dstTuple;
    aDataPlan->depTuple = &aTemplate->tmplate.rows[ aCodePlan->depTupleID ];
    aDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;
    
    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_VMTR_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_VMTR_INIT_DONE_TRUE;

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
qmnVMTR::initMtrNode( qcTemplate * aTemplate,
                      qmncVMTR   * aCodePlan,
                      qmndVMTR   * aDataPlan )
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
    
    // Store And Search�� ������ ���� ����� ���Ѵ�.
    // PROJ-2469 Optimize View Materialization
    // ���� Plan���� ������� �ʴ� MtrNode Type - QMC_MTR_TYPE_USELESS_COLUMN �� �� �ִ�.
    IDE_DASSERT( ( ( aCodePlan->myNode->flag & QMC_MTR_TYPE_MASK )
                   == QMC_MTR_TYPE_COPY_VALUE ) ||
                 ( ( aCodePlan->myNode->flag & QMC_MTR_TYPE_MASK )
                   == QMC_MTR_TYPE_USELESS_COLUMN )
                 );    

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
                                0 ) // Base Table �������� ����
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
qmnVMTR::initTempTable( qcTemplate * aTemplate,
                        qmncVMTR   * aCodePlan,
                        qmndVMTR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt       sFlag          = 0;
    qmndVMTR * sCacheDataPlan = NULL;

    //---------------------------------
    // ���� ������ ���� ������ �ʱ�ȭ
    //---------------------------------

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sFlag = QMCD_SORT_TMP_STORAGE_MEMORY;
    }
    else
    {
        sFlag = QMCD_SORT_TMP_STORAGE_DISK;
    }

    //---------------------------------
    // Temp Table�� �ʱ�ȭ
    //---------------------------------
    if ( ( *aDataPlan->flag & QMN_PLAN_RESULT_CACHE_EXIST_MASK )
         == QMN_PLAN_RESULT_CACHE_EXIST_FALSE )
    {
        IDU_FIT_POINT( "qmnVMTR::initTempTable::qmxAlloc:sortMgr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                  (void **)&aDataPlan->sortMgr )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                     aTemplate,
                                     ID_UINT_MAX,
                                     aDataPlan->mtrNode,
                                     NULL,
                                     0,
                                     sFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2462 Result Cache */
        sCacheDataPlan = (qmndVMTR *) (aTemplate->resultCache.data +
                                      aCodePlan->plan.offset);

        if ( ( *aDataPlan->resultData.flag & QMX_RESULT_CACHE_INIT_DONE_MASK )
             == QMX_RESULT_CACHE_INIT_DONE_FALSE )
        {
            IDU_FIT_POINT( "qmnVMTR::initTempTable::qrcAlloc:sortMgr",
                           idERR_ABORT_InsufficientMemory );
            IDE_TEST( sCacheDataPlan->resultData.memory->alloc( ID_SIZEOF( qmcdSortTemp ),
                                                               (void **)&aDataPlan->sortMgr )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::init( aDataPlan->sortMgr,
                                         aTemplate,
                                         sCacheDataPlan->resultData.memoryIdx,
                                         aDataPlan->mtrNode,
                                         NULL,
                                         0,
                                         sFlag )
                      != IDE_SUCCESS );
            sCacheDataPlan->sortMgr = aDataPlan->sortMgr;
        }
        else
        {
            aDataPlan->sortMgr = sCacheDataPlan->sortMgr;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnVMTR::storeChild( qcTemplate * aTemplate,
                     qmncVMTR   * aCodePlan,
                     qmndVMTR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child Plan�� ����� ������.
 *
 * Implementation :
 *    Child�� �ݺ������� �����Ͽ� �� ����� ������.
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::storeChild"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    //---------------------------------
    // Child Plan�� �ʱ�ȭ
    //---------------------------------

    IDE_TEST( aCodePlan->plan.left->init( aTemplate,
                                          aCodePlan->plan.left )
              != IDE_SUCCESS);

    //---------------------------------
    // Child Plan�� ����� ����
    //---------------------------------

    // doIt left child
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        // ���� ������ �Ҵ�
        aDataPlan->mtrRow = aDataPlan->mtrNode->dstTuple->row;
        IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMgr,
                                      & aDataPlan->mtrRow )
                  != IDE_SUCCESS );

        // Record ����
        IDE_TEST( setMtrRow( aTemplate, aDataPlan )
                  != IDE_SUCCESS );

        // Temp Table�� ����
        IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMgr,
                                       aDataPlan->mtrRow )
                  != IDE_SUCCESS );

        // Left Child�� ����
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              & sFlag ) != IDE_SUCCESS );

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

#undef IDE_FN
}


IDE_RC
qmnVMTR::setMtrRow( qcTemplate * aTemplate,
                    qmndVMTR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVMTR::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setMtr( aTemplate,
                                      sNode,
                                      aDataPlan->mtrRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnVMTR::resetDependency( qcTemplate  * aTemplate,
                                 qmnPlan     * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    VSCN������ ȣ���� �� ������, dependency�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // qmncVMTR * sCodePlan = (qmncVMTR*) aPlan;
    qmndVMTR * sDataPlan =
        (qmndVMTR*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->depValue = QMN_PLAN_DEFAULT_DEPENDENCY_VALUE;

    return IDE_SUCCESS;
}

