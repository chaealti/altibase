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
 * $Id: qmnAggregation.cpp 85261 2019-04-17 01:26:11Z andrew.shin $
 *
 * Description :
 *     AGGR(AGGRegation) Node
 *
 *     ������ �𵨿��� ������ ���� ����� �����ϴ� Plan Node �̴�.
 *
 *         - Sort-based Grouping
 *         - Distinct Aggregation
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnAggregation.h>

IDE_RC
qmnAGGR::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    AGGR ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAGGR * sCodePlan = (qmncAGGR *) aPlan;
    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnAGGR::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_AGGR_INIT_DONE_MASK)
         == QMND_AGGR_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sDataPlan->mtrRowIdx = 0;

    // init left child
    IDE_TEST( aPlan->left->init( aTemplate,
                                 aPlan->left ) != IDE_SUCCESS);

    if ( (sCodePlan->flag & QMNC_AGGR_GROUPED_MASK )
         == QMNC_AGGR_GROUPED_TRUE )
    {
        sDataPlan->doIt = qmnAGGR::doItGroupAggregation;
    }
    else
    {
        sDataPlan->doIt = qmnAGGR::doItAggregation;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnAGGR::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    AGGR�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    // ���� ��ġ�� �����Ѵ�.
    sDataPlan->mtrRowIdx++;
    sDataPlan->plan.myTuple->row = sDataPlan->mtrRow[sDataPlan->mtrRowIdx % 2];

    // �ش� �Լ��� �����Ѵ�.
    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child�� ���Ͽ� padNull()�� ȣ���ϰ�
 *    AGGR ����� null row�� setting�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAGGR * sCodePlan = (qmncAGGR *) aPlan;
    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_AGGR_INIT_DONE_MASK)
         == QMND_AGGR_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }

    // Child Plan�� ���Ͽ� Null Padding����
    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    // AGGR ����� Null Row����
    sDataPlan->plan.myTuple->row = sDataPlan->nullRow;

    // Null Padding�� record�� ���� ����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    AGGR ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAGGR * sCodePlan = (qmncAGGR*) aPlan;
    qmndAGGR * sDataPlan =
        (qmndAGGR*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    UInt  sMtrRowIdx = 0;
    ULong i;

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( (*sDataPlan->flag & QMND_AGGR_INIT_DONE_MASK)
             == QMND_AGGR_INIT_DONE_TRUE )
        {
            /* PROJ-2448 Subquery caching */
            sMtrRowIdx = (sDataPlan->mtrRowIdx > 0)? sDataPlan->mtrRowIdx - 1: sDataPlan->mtrRowIdx;

            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                iduVarStringAppendFormat( aString,
                                          "AGGREGATION ( "
                                          "ITEM_SIZE: %"ID_UINT32_FMT", "
                                          "GROUP_COUNT: %"ID_UINT32_FMT,
                                          sDataPlan->mtrRowSize,
                                          sMtrRowIdx );
            }
            else
            {
                // BUG-29209
                // ITEM_SIZE ���� �������� ����
                iduVarStringAppendFormat( aString,
                                          "AGGREGATION ( "
                                          "ITEM_SIZE: BLOCKED, "
                                          "GROUP_COUNT: %"ID_UINT32_FMT,
                                          sMtrRowIdx );
            }
        }
        else
        {
            iduVarStringAppend( aString,
                                "AGGREGATION "
                                "( ITEM_SIZE: 0, GROUP_COUNT: 0" );
        }
    }
    else
    {
        iduVarStringAppend( aString,
                            "AGGREGATION "
                            "( ITEM_SIZE: ??, GROUP_COUNT: ??" );
    }

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    //----------------------------
    // PROJ-1473 mtrNode info 
    //----------------------------

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->myNode,
                           "myNode",
                           sCodePlan->myNode->dstNode->node.table,
                           ID_USHORT_MAX,
                           ID_USHORT_MAX);

        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->distNode,
                           "distNode",
                           sCodePlan->myNode->dstNode->node.table,
                           ID_USHORT_MAX,
                           ID_USHORT_MAX);
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
qmnAGGR::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnAGGR::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::doItAggregation( qcTemplate * aTemplate,
                          qmnPlan    * aPlan,
                          qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     One-Group Aggregation�� ����
 *
 * Implementation :
 *     Child�� ���� ������ �ݺ� �����Ͽ� �� ����� Return�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::doItAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate,
                                 aPlan->left,
                                 & sFlag ) != IDE_SUCCESS);

    // �ʱ�ȭ ����
    IDE_TEST( clearDistNode( sDataPlan )
              != IDE_SUCCESS);

    IDE_TEST( initAggregation(aTemplate, sDataPlan)
              != IDE_SUCCESS);

    // Record�� ���� ������ Aggregation ����
    while( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( execAggregation( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        // doIt left child
        IDE_TEST( aPlan->left->doIt( aTemplate,
                                     aPlan->left,
                                     & sFlag ) != IDE_SUCCESS);
    }

    // Aggregation�� ������
    IDE_TEST( finiAggregation( aTemplate, sDataPlan )
              != IDE_SUCCESS );

    /* BUG-46906 */
    IDE_TEST( setTupleSet( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_EXIST;

    // ���������� Data ������ �����ϱ� ����
    sDataPlan->doIt = qmnAGGR::doItLast;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::doItGroupAggregation( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Multi-Group Aggregatino�� ���� ����
 *
 * Implementation :
 *     ���� Group�� ���� Child�� �ݺ� �����Ͽ� Aggregation�� �Ѵ�.
 *     �ٸ� Group�� �ö�� ���
 *        ���ο� Group�� ���� �ʱ�ȭ�� ����
 *        ���� Group�� ���� �������� �����ϰ� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::doItGroupAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndAGGR        * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);
    qmcRowFlag        sFlag     = QMC_ROW_INITIALIZE;

    //------------------------------------
    // ���� Group�� ���� Aggregation ����
    //------------------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate,
                                 aPlan->left,
                                 & sFlag ) != IDE_SUCCESS);

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //--------------------------------
        // ���� Group�� ���� �ʱ�ȭ
        //--------------------------------

        IDE_TEST( clearDistNode( sDataPlan )
                  != IDE_SUCCESS);

        IDE_TEST( initAggregation(aTemplate, sDataPlan)
                  != IDE_SUCCESS);

        IDE_TEST( execAggregation( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        // ���� Group�� ���� �ݺ� ����
        IDE_TEST( aPlan->left->doIt( aTemplate,
                                     aPlan->left,
                                     & sFlag ) != IDE_SUCCESS);

        while( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME )
        {
            IDE_TEST( execAggregation( aTemplate, sDataPlan )
                      != IDE_SUCCESS );

            IDE_TEST( aPlan->left->doIt( aTemplate,
                                         aPlan->left,
                                         & sFlag ) != IDE_SUCCESS);
        }

        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
        {
            //--------------------------------
            // Record �� ���̻� ���� ���
            //--------------------------------

            sDataPlan->doIt = qmnAGGR::doItLast;

            /* BUG-46906 */
            // ���� Group�� ���� ������ ����
            IDE_TEST( finiAggregation( aTemplate, sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* BUG-46906 */
            // ���� Group�� ���� ������ ����
            IDE_TEST( finiAggregation( aTemplate, sDataPlan )
                      != IDE_SUCCESS );

            //--------------------------------
            // �ٸ� Group�� �ö�� ���
            //--------------------------------

            // ���ο� Group�� ���� ó��
            IDE_TEST( setNewGroup( aTemplate, sDataPlan )
                      != IDE_SUCCESS );

            // ���� Group�� ��ġ�� ������ ���� ���� �Լ��� ����
            sDataPlan->plan.myTuple->row =
                sDataPlan->mtrRow[sDataPlan->mtrRowIdx % 2];

            sDataPlan->doIt = qmnAGGR::doItNext;
        }

        /* BUG-46906 */
        IDE_TEST( setTupleSet( aTemplate, sDataPlan )
                   != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_EXIST;
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
qmnAGGR::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Multi-Group Aggregation�� ���� ����
 *
 * Implementation :
 *    ���� Group ������ Tuple Set�� ����
 *    ���� Group�ϵ��� �ݺ��Ͽ� Aggregation�� ����
 *    �ٸ� Group�� �ö�� ���
 *        ���ο� Group�� ���� �ʱ�ȭ�� ����
 *        ���� Group�� ���� �������� �����ϰ� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    // To fix PR-4355
    // ȣ�� �������� ���� Group�� ���� Tuple Set ������ �����Ǿ� �ִ�.
    // ����, ���� Group�� ���� Tuple Set������ �����Ѵ�.
    IDE_TEST( setTupleSet( aTemplate, sDataPlan )
              != IDE_SUCCESS );

    //-----------------------------
    // ���� Group�� ���� �ݺ� ����
    //-----------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate,
                                 aPlan->left,
                                 & sFlag ) != IDE_SUCCESS);

    while( (sFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_SAME )
    {
        IDE_TEST( execAggregation( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        IDE_TEST( aPlan->left->doIt( aTemplate,
                                     aPlan->left,
                                     & sFlag ) != IDE_SUCCESS);
    }

    //-----------------------------
    // ���� Group�� ���� ������
    //-----------------------------

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        // �� �̻� Record�� ���� ���
        sDataPlan->doIt = qmnAGGR::doItLast;

        /* BUG-46906 */
        // ���� Group�� ���� ������ ����
        IDE_TEST( finiAggregation( aTemplate, sDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        /* BUG-46906 */
        // ���� Group�� ���� ������ ����
        IDE_TEST( finiAggregation( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        // �ٸ� Group�� �����ϴ� ���
        // �ٸ� Group�� ���� �ʱ�ȭ�� �����Ѵ�.

        // ���ο� Group�� ���� ó��
        IDE_TEST( setNewGroup( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        // ���� Group�� ��ġ ����
        sDataPlan->plan.myTuple->row = sDataPlan->mtrRow[sDataPlan->mtrRowIdx % 2];
    }

    /* BUG-46906 */
    IDE_TEST( setTupleSet( aTemplate, sDataPlan )
              != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_EXIST;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::doItLast( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Record ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::doItLast"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAGGR * sCodePlan = (qmncAGGR *) aPlan;
    qmndAGGR * sDataPlan =
        (qmndAGGR *) (aTemplate->tmplate.data + aPlan->offset);

    // set that no data found
    *aFlag = QMC_ROW_DATA_NONE;

    // BUG-44041 ������ġ�� �����մϴ�.
    sDataPlan->plan.myTuple->row = sDataPlan->mtrRow[(sDataPlan->mtrRowIdx-1) % 2];

    if ( (sCodePlan->flag & QMNC_AGGR_GROUPED_MASK )
         == QMNC_AGGR_GROUPED_TRUE )
    {
        sDataPlan->doIt = qmnAGGR::doItGroupAggregation;
    }
    else
    {
        sDataPlan->doIt = qmnAGGR::doItAggregation;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}


IDE_RC
qmnAGGR::firstInit( qcTemplate * aTemplate,
                    qmncAGGR   * aCodePlan,
                    qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    AGGR node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    //---------------------------------
    // AGGR ���� ������ �ʱ�ȭ
    //---------------------------------

    // 1. ���� Column�� �ʱ�ȭ
    // 2. Distinct Column������ �ʱ�ȭ
    // 3. Aggregation Column ������ �ʱ�ȭ
    // 4. Grouping Column ������ ��ġ ����
    // 5. Tuple ��ġ ����

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    if ( aCodePlan->distNode != NULL )
    {
        IDE_TEST( initDistNode( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->distNodeCnt = 0;
        aDataPlan->distNode = NULL;
    }

    if ( aDataPlan->aggrNodeCnt > 0 )
    {
        IDE_TEST( initAggrNode( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->aggrNode = NULL;
    }

    if ( (aCodePlan->flag & QMNC_AGGR_GROUPED_MASK)
         == QMNC_AGGR_GROUPED_TRUE )
    {
        IDE_TEST( initGroupNode( aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->groupNode = NULL;
    }

    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    //---------------------------------
    // ���� �ٸ� Group�� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------

    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    IDE_TEST( allocMtrRow( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    IDE_TEST( makeNullRow( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_AGGR_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_AGGR_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::initMtrNode( qcTemplate * aTemplate,
                      qmncAGGR   * aCodePlan,
                      qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::initMtrNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcMtrNode * sNode;
    UShort       i;    

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );

    // Distinct Column�� Disk Temp Table�� ����ϴ� Memory Temp Table��
    // ����ϴ�, Row ������ ���� Tuple Set�� ������ Memory Storage���� �Ѵ�.
    // Distinct Column��  Disk�� ������ ���, Plan�� flag������ DISK
    // �� Distinct Column�� ���� Tuple Set�� Storage ���� Disk Type�� �ȴ�.
    IDE_DASSERT(
        ( aTemplate->tmplate.rows[aCodePlan->myNode->dstNode->node.table].lflag
          & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_MEMORY );

    //----------------------------------
    // Aggregation ������ ����
    //----------------------------------

    for( i = 0, sNode = aCodePlan->myNode;
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

    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);

    // ���� Column�� ���� ���� ����
    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    // ���� Column�� �ʱ�ȭ
    // Aggregation�� ���� �� Conversion���� �����ؼ��� �ȵ�
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                (UShort)aCodePlan->baseTableCount + aDataPlan->aggrNodeCnt )
              != IDE_SUCCESS );

    // ���� Column�� offset�� ������.
    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  0 ) // ������ header�� �ʿ� ����
              != IDE_SUCCESS );

    // Row Size�� ���
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::initDistNode( qcTemplate * aTemplate,
                       qmncAGGR   * aCodePlan,
                       qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Distinct Argument�� �����ϱ� ���� Distinct ������ �����Ѵ�.
 *
 * Implementation :
 *    �ٸ� ���� Column�� �޸� Distinct Argument Column������
 *    ���� ���� ���� ������ �������� �ʴ´�.
 *    �̴� �� Column������ ������ Tuple�� ����ϸ�, ���� ���� ����
 *    ���踦 ���� ���� �Ӵ��� Hash Temp Table�� ���� ���� ����
 *    ����ϱ� ���ؼ��̴�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::initDistNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmcMtrNode  * sCNode;
    qmdDistNode * sDistNode;
    UInt          sFlag;
    UInt          sHeaderSize;
    UInt          i;

    // ���ռ� �˻�.
    IDE_DASSERT( aCodePlan->distNodeOffset > 0 );

    //------------------------------------------------------
    // Distinct ���� Column�� �⺻ ���� ����
    // Distinct Node�� ���������� ���� ������ ���� ó���Ǹ�,
    // ���� Distinct Node���� ���� ������ �������� �ʴ´�.
    //------------------------------------------------------

    aDataPlan->distNode =
        (qmdDistNode *) (aTemplate->tmplate.data + aCodePlan->distNodeOffset);

    for( sCNode = aCodePlan->distNode, sDistNode = aDataPlan->distNode,
             aDataPlan->distNodeCnt = 0;
         sCNode != NULL;
         sCNode = sCNode->next, sDistNode++,
             aDataPlan->distNodeCnt++ )
    {
        sDistNode->myNode = sCNode;
        sDistNode->srcNode = NULL;
        sDistNode->next = NULL;
    }

    //------------------------------------------------------------
    // [Hash Temp Table�� ���� ���� ����]
    // AGGR ����� Row�� �޸� Distinct Column�� ���� ��ü��
    // Memory �Ǵ� Disk�� �� �ִ�.  �� ������ plan.flag�� �̿��Ͽ�
    // �Ǻ��ϸ�, �ش� distinct column�� �����ϱ� ���� Tuple Set����
    // ������ ���� ��ü�� ����ϰ� �־�� �Ѵ�.
    // �̿� ���� ���ռ� �˻�� Hash Temp Table���� �˻��ϰ� �ȴ�.
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

    for ( i = 0, sDistNode = aDataPlan->distNode;
          i < aDataPlan->distNodeCnt;
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

#undef IDE_FN
}

IDE_RC
qmnAGGR::initAggrNode( qcTemplate * aTemplate,
                       qmncAGGR   * aCodePlan,
                       qmndAGGR   * aDataPlan )
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

    UInt i;

    qmdAggrNode * sAggrNode;
    qmdDistNode * sDistNode;

    //-----------------------------------------------
    // ���ռ� �˻�
    //-----------------------------------------------

    IDE_DASSERT( aCodePlan->aggrNodeOffset > 0 );

    aDataPlan->aggrNode =
        (qmdAggrNode*) (aTemplate->tmplate.data + aCodePlan->aggrNodeOffset);

    //-----------------------------------------------
    // Aggregation Node�� ���� ������ �����ϰ� �ʱ�ȭ
    //-----------------------------------------------

    IDE_TEST( linkAggrNode( aCodePlan,
                            aDataPlan ) != IDE_SUCCESS );

    // aggregation node should not use converted source node
    //   e.g) SUM( MIN(I1) )
    //        MIN(I1)'s converted node is not aggregation node
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                (qmdMtrNode*) aDataPlan->aggrNode,
                                (UShort)aDataPlan->aggrNodeCnt )
              != IDE_SUCCESS );

    // Aggregation Column �� offset�� ������
    IDE_TEST( qmc::refineOffsets( (qmdMtrNode*) aDataPlan->aggrNode,
                                  0 ) // ������ header�� �ʿ� ����
              != IDE_SUCCESS );

    //-----------------------------------------------
    // Distinct Aggregation�� ��� �ش� Distinct Node��
    // ã�� �����Ѵ�.
    //-----------------------------------------------

    for ( sAggrNode = aDataPlan->aggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        if ( sAggrNode->myNode->myDist != NULL )
        {
            // Distinct Aggregation�� ���
            for ( i = 0, sDistNode = aDataPlan->distNode;
                  i < aDataPlan->distNodeCnt;
                  i++, sDistNode++ )
            {
                if ( sDistNode->myNode == sAggrNode->myNode->myDist )
                {
                    sAggrNode->myDist = sDistNode;
                    break;
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

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::linkAggrNode( qmncAGGR   * aCodePlan,
                       qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Code Materialized Node �������κ��� Aggregation Node��������
 *    �����Ͽ� �����Ѵ�.
 *
 * Implementation :
 *    Code ������ Materialized Node�� ������ ������ ����.
 *
 *    <----- Aggregation ���� ---->|<----- Grouping ���� ------>
 *    [SUM]----->[AVG]----->[MAX]----->[i1]----->[i2]
 *
 *    Aggregation������ ���� GROUPING ������ ���� �������� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::linkAggrNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    qmcMtrNode  * sNode;
    qmdAggrNode * sAggrNode;
    qmdAggrNode * sPrevNode;

    for( i = 0, sNode = aCodePlan->myNode;
         i < aCodePlan->baseTableCount;
         sNode = sNode->next, i++ )
    {
        // Nothing To Do 
    }

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

    return IDE_SUCCESS;

    // IDE_EXCEPTION_END;

    // return IDE_FAIULRE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::initGroupNode( qmncAGGR   * aCodePlan,
                        qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::initGroupNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    qmdMtrNode * sMtrNode;

    for ( i = 0, sMtrNode = aDataPlan->mtrNode;
          i < aCodePlan->baseTableCount + aDataPlan->aggrNodeCnt;
          i++, sMtrNode = sMtrNode->next ) ;

    aDataPlan->groupNode = sMtrNode;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnAGGR::allocMtrRow( qcTemplate * aTemplate,
                      qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::allocMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemory * sMemory;

    sMemory = aTemplate->stmt->qmxMem;

    //-------------------------------------------
    // �� Row�� ���带 ���� ���� �Ҵ�
    //-------------------------------------------

    IDU_FIT_POINT_RAISE( "qmnAGGR::allocMtrRow::cralloc::DataPlan_mtrRow0",
                          err_mem_alloc );
    IDE_TEST( sMemory->cralloc( aDataPlan->mtrRowSize,
                                (void**)&(aDataPlan->mtrRow[0]))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->mtrRow[0] == NULL, err_mem_alloc );

    IDU_FIT_POINT_RAISE( "qmnAGGR::allocMtrRow::cralloc::DataPlan_mtrRow1", 
                          err_mem_alloc );
    IDE_TEST( sMemory->cralloc( aDataPlan->mtrRowSize,
                                (void**)&(aDataPlan->mtrRow[1]))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->mtrRow[1] == NULL, err_mem_alloc );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::makeNullRow(qcTemplate * aTemplate,
                     qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::makeNullRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;
    iduMemory  * sMemory;

    sMemory = aTemplate->stmt->qmxMem;

    //-------------------------------------------
    // Null Row�� ���� ���� �Ҵ�
    //-------------------------------------------

    IDU_FIT_POINT_RAISE( "qmnAGGR::makeNullRow::cralloc::DataPlan_nullRow", 
                          err_mem_alloc );
    IDE_TEST( sMemory->cralloc( aDataPlan->mtrRowSize,
                                (void**)&(aDataPlan->nullRow))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->nullRow == NULL, err_mem_alloc );

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        //-----------------------------------------------
        // ���� ���� �����ϴ� Column�� ���ؼ���
        // NULL Value�� �����Ѵ�.
        //-----------------------------------------------

        sNode->func.makeNull( sNode,
                              aDataPlan->nullRow );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    IDE_EXCEPTION_END;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnAGGR::clearDistNode( qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Distinct Column�� ������ ���� ������
 *    Temp Table�� Clear�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::clearDistNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    qmdDistNode * sDistNode;

    for ( i = 0, sDistNode = aDataPlan->distNode;
          i < aDataPlan->distNodeCnt;
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

#undef IDE_FN
}



IDE_RC
qmnAGGR::initAggregation( qcTemplate * aTemplate,
                          qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� �����Ѵ�.
 *    Aggregation Column�� �ʱ�ȭ�ϰ�, Group Column�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::initAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdAggrNode * sNode;

    for ( sNode = aDataPlan->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( qtc::initialize( sNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    IDE_TEST( setGroupColumns( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::setGroupColumns( qcTemplate * aTemplate,
                          qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Grouping Column�� ���� ������ ����
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::setGroupColumns"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    // idBool sExist;
    // sExist = ID_TRUE;

    for ( sNode = aDataPlan->groupNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST(
            sNode->func.setMtr( aTemplate,
                                sNode,
                                aDataPlan->mtrRow[aDataPlan->mtrRowIdx % 2] )
            != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::execAggregation( qcTemplate * aTemplate,
                          qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation�� �����Ѵ�.
 *
 * Implementation :
 *    Distinct Column�� ���Ͽ� �����ϰ�,
 *    Distinct Aggregation�� ��� Distinction ���ο� ����,
 *    Aggregation ���� ���θ� �Ǵ��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::execAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdAggrNode * sAggrNode;

    // BUG-42277
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // set distinct column and insert to hash(DISTINCT)
    IDE_TEST( setDistMtrColumns( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    for ( sAggrNode = aDataPlan->aggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        if ( sAggrNode->myDist == NULL )
        {
            // Non Distinct Aggregation�� ���
            IDE_TEST( qtc::aggregate( sAggrNode->dstNode, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            // Distinct Aggregation�� ���
            if ( sAggrNode->myDist->isDistinct == ID_TRUE )
            {
                // Distinct Argument�� ���
                IDE_TEST( qtc::aggregate( sAggrNode->dstNode, aTemplate )
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

#undef IDE_FN
}

IDE_RC
qmnAGGR::setDistMtrColumns( qcTemplate * aTemplate,
                            qmndAGGR * aDataPlan )
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

#define IDE_FN "qmnAGGR::setDistMtrColumns"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    qmdDistNode * sDistNode;

    for ( i = 0, sDistNode = aDataPlan->distNode;
          i < aDataPlan->distNodeCnt;
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

#undef IDE_FN
}

IDE_RC
qmnAGGR::finiAggregation( qcTemplate * aTemplate,
                          qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation�� �������ϰ� ����� Tuple Set�� Setting�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::finiAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdAggrNode * sAggrNode;

    for ( sAggrNode = aDataPlan->aggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        IDE_TEST( qtc::finalize( sAggrNode->dstNode, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAGGR::setTupleSet( qcTemplate * aTemplate,
                      qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� ����� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST(
            sNode->func.setTuple( aTemplate,
                                  sNode,
                                  aDataPlan->mtrRow[aDataPlan->mtrRowIdx % 2] )
            != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnAGGR::setNewGroup( qcTemplate * aTemplate,
                      qmndAGGR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Aggregation ���� �� ���ο� �׷쿡 ���� �ʱ�ȭ
 *
 * Implementation :
 *     ���ο� Group�� ���� ������ �����ϰ� �ʱ�ȭ ����
 *     ����  Group�� ��ġ�� �̵�
 *
 ***********************************************************************/

#define IDE_FN "qmnAGGR::setNewGroup"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // ���ο� Group�� ��ġ�� �̵�
    aDataPlan->mtrRowIdx++;
    aDataPlan->plan.myTuple->row =
        aDataPlan->mtrRow[aDataPlan->mtrRowIdx % 2];

    // ���ο� Group�� ���� �ʱ�ȭ �� Aggregation ����
    IDE_TEST( clearDistNode( aDataPlan )
              != IDE_SUCCESS);

    IDE_TEST( initAggregation(aTemplate, aDataPlan)
              != IDE_SUCCESS);

    IDE_TEST( execAggregation( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    // ���� Group���� ����
    aDataPlan->mtrRowIdx--;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
