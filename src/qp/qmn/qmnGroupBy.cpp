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
 * $Id: qmnGroupBy.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     GRBY(GRoup BY) Node
 *
 *     ������ �𵨿��� ������ ���� ����� �����ϴ� Plan Node �̴�.
 *
 *         - Sort-based Distinction
 *         - Sort-based Grouping
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnGroupBy.h>

IDE_RC
qmnGRBY::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    GRBY ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRBY * sCodePlan = (qmncGRBY *) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnGRBY::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_GRBY_INIT_DONE_MASK)
         == QMND_GRBY_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }

    // init left child
    IDE_TEST( aPlan->left->init( aTemplate,
                                 aPlan->left ) != IDE_SUCCESS);

    sDataPlan->doIt = qmnGRBY::doItFirst;
    sDataPlan->mtrRowIdx = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnGRBY::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    GRBY�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnGRBY::doIt"));

    qmndGRBY * sDataPlan =
        (qmndGRBY*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Child�� ���Ͽ� padNull()�� ȣ���ϰ�
 *    GRBY ����� null row�� setting�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRBY * sCodePlan = (qmncGRBY *) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY *) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_GRBY_INIT_DONE_MASK)
         == QMND_GRBY_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Child Plan�� ���Ͽ� Null Padding����
    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    // GRBY ����� Null Row����
    sDataPlan->plan.myTuple->row = sDataPlan->nullRow;

    // Null Padding�� record�� ���� ����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    GRBY ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRBY * sCodePlan = (qmncGRBY*) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    iduVarStringAppend( aString,
                        "GROUPING\n" );

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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnGRBY::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    GRBY �� ���� ���� �Լ�
 *    Child�� �����ϰ� �뵵�� �´� ���� �Լ��� �����Ѵ�.
 *    ���� ���õ� Row�� �ݵ�� ���� ���� ���޵Ǳ� ������
 *    �뵵�� �´� ó���� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRBY * sCodePlan = (qmncGRBY *) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY *) (aTemplate->tmplate.data + aPlan->offset);
    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    // Child�� ����
    IDE_TEST( sCodePlan->plan.left->doIt( aTemplate,
                                          sCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // ���� Row�� ����.
        IDE_TEST( setMtrRow( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        *aFlag = sFlag;

        // ���� Row�� �ƴ��� ǥ��
        *aFlag &= ~QMC_ROW_GROUP_MASK;
        *aFlag |= QMC_ROW_GROUP_NULL;

        // �������� ���� Row�� ����� �� �ֵ��� Tuple Set�� ����
        IDE_TEST( setTupleSet( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        // �뵵�� �´� ���� �Լ��� �����Ѵ�.
        switch (sCodePlan->flag & QMNC_GRBY_METHOD_MASK)
        {
            case QMNC_GRBY_METHOD_DISTINCTION :
                {
                    sDataPlan->doIt = qmnGRBY::doItDistinct;
                    break;
                }
            case QMNC_GRBY_METHOD_GROUPING :
                {
                    sDataPlan->doIt = qmnGRBY::doItGroup;
                    break;
                }
            default :
                {
                    IDE_ASSERT( 0 );
                    break;
                }
        }
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }

    // ������ ������ ��ġ ����
    sDataPlan->mtrRowIdx++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN

}

IDE_RC
qmnGRBY::doItGroup( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Sort-Based Grouping�� ó���� ��, ����Ǵ� �Լ��̴�.
 *    ���� Group������ ���� �Ǵ��� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::doItGroup"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncGRBY * sCodePlan = (qmncGRBY *) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    // Child ����
    IDE_TEST( sCodePlan->plan.left->doIt( aTemplate,
                                          sCodePlan->plan.left,
                                          & sFlag ) != IDE_SUCCESS );

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // ���� Row�� ����
        IDE_TEST( setMtrRow( aTemplate, sDataPlan )
                  != IDE_SUCCESS );

        *aFlag = sFlag;

        // ����� �� Row�� ���Ͽ� ���� Group������ �Ǵ�.
        if ( compareRows( sDataPlan ) == 0 )
        {
            *aFlag &= ~QMC_ROW_GROUP_MASK;
            *aFlag |= QMC_ROW_GROUP_SAME;
        }
        else
        {
            *aFlag &= ~QMC_ROW_GROUP_MASK;
            *aFlag |= QMC_ROW_GROUP_NULL;

            // ���� Group�� �ƴ� ���
            // �������� ���� Row�� ����� �� �ֵ��� Tuple Set�� ����
            IDE_TEST( setTupleSet( aTemplate, sDataPlan )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->doIt = qmnGRBY::doItFirst;
    }

    sDataPlan->mtrRowIdx++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::doItDistinct( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Sort-based Distinction�� ���� ���� �Լ�
 *
 * Implementation :
 *    ���� Record������ �Ǵ��Ͽ� �ٸ� Record�� ��쿡��,
 *    ���� Plan�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::doItDistinct"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( doItGroup( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    while ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        if ( (*aFlag & QMC_ROW_GROUP_MASK) == QMC_ROW_GROUP_NULL )
        {
            break;
        }
        IDE_TEST( doItGroup( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::doItLast( qcTemplate * aTemplate,
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

#define IDE_FN "qmnGRBY::doItLast"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncGRBY * sCodePlan = (qmncGRBY *) aPlan;
    qmndGRBY * sDataPlan =
        (qmndGRBY *) (aTemplate->tmplate.data + aPlan->offset);

    // set that no data found
    *aFlag = QMC_ROW_DATA_NONE;

    sDataPlan->doIt = qmnGRBY::doItFirst;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnGRBY::firstInit( qcTemplate * aTemplate,
                    qmncGRBY   * aCodePlan,
                    qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    GRBY node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // ���ռ� �˻�
    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aCodePlan != NULL );
    IDE_DASSERT( aDataPlan != NULL );

    //---------------------------------
    // GRBY ���� ������ �ʱ�ȭ
    //---------------------------------

    // 1. ���� Column�� ���� �ʱ�ȭ
    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );

    // 2. Group Column ��ġ ����
    IDE_TEST( initGroupNode( aDataPlan )
              != IDE_SUCCESS );

    IDE_ASSERT( aDataPlan->mtrNode != NULL );
    
    // 3. Tuple������ ����
    aDataPlan->plan.myTuple = aDataPlan->mtrNode->dstTuple;

    //---------------------------------
    // ���� Group �Ǵ��� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------

    // 1. ���� Column�� Row Size ���
    // ���ռ� �˻�
    IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                 == MTC_TUPLE_STORAGE_MEMORY );

    // Tuple�� Row Size ����
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    // Row Size ȹ��
    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    // 2. �񱳸� ���� ���� �Ҵ� �� Null Row ����
    IDE_TEST( allocMtrRow( aTemplate, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_GRBY_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_GRBY_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::initMtrNode( qcTemplate * aTemplate,
                      qmncGRBY   * aCodePlan,
                      qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� ������ �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::initMtrNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // ���ռ� �˻�
    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );

    // ���� Column�� ���� ȹ��
    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);

    // ���� Column�� ���� ���� ����
    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    // ���� Column�� �ʱ�ȭ
    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                aCodePlan->baseTableCount ) //basetable��������������
              != IDE_SUCCESS );

    // ���� Column�� offset�� ������.
    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  0) // ������ Header�� �������� ����
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::initGroupNode( qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�߿��� Group Node�� ���� ��ġ�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::initGroupNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        if ( (sNode->myNode->flag & QMC_MTR_GROUPING_MASK)
             == QMC_MTR_GROUPING_TRUE )
        {
            break;
        }
    }

    aDataPlan->groupNode = sNode;

    // ���ռ� �˻�.
    IDE_DASSERT( aDataPlan->groupNode != NULL );

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnGRBY::allocMtrRow( qcTemplate * aTemplate,
                      qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::allocMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;
    iduMemory  * sMemory;

    sMemory = aTemplate->stmt->qmxMem;

    //-------------------------------------------
    // �� Row�� �񱳸� ���� ���� �Ҵ�
    //-------------------------------------------

    IDE_TEST( sMemory->alloc( aDataPlan->mtrRowSize,
                              (void**)&(aDataPlan->mtrRow[0]))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->mtrRow[0] == NULL, err_mem_alloc );

    IDE_TEST( sMemory->alloc( aDataPlan->mtrRowSize,
                              (void**)&(aDataPlan->mtrRow[1]))
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->mtrRow[1] == NULL, err_mem_alloc );

    //-------------------------------------------
    // Null Row�� ���� ���� �Ҵ�
    //-------------------------------------------

    IDE_TEST( sMemory->alloc(aDataPlan->mtrRowSize,
                             (void**) & aDataPlan->nullRow )
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->nullRow == NULL, err_mem_alloc );

    *(qmcRowFlag*)aDataPlan->nullRow = QMC_ROW_DATA_EXIST | QMC_ROW_VALUE_NULL;

    //-------------------------------------------
    // Null Row�� ����
    //-------------------------------------------

    for ( sNode = aDataPlan->mtrNode; sNode != NULL; sNode = sNode->next )
    {
        //-----------------------------------------------
        // ���� ���� �����ϴ� Column�� ���ؼ���
        // NULL Value�� �����Ѵ�.
        //-----------------------------------------------

        sNode->func.makeNull( sNode, aDataPlan->nullRow );
    }

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
qmnGRBY::setMtrRow( qcTemplate * aTemplate,
                    qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Row�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::setMtrRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;
    // idBool       sExist;

    // ������ ��ġ�� ����
    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx % 2];

    // ���� Row�� ����
    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST(
            sNode->func.setMtr( aTemplate,
                                sNode,
                                aDataPlan->plan.myTuple->row ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnGRBY::setTupleSet( qcTemplate * aTemplate,
                      qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    �������� ���� Row�� ����� �� �ֵ��� Tuple Set�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnGRBY::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;
    // idBool       sExist;

    aDataPlan->plan.myTuple->row = aDataPlan->mtrRow[aDataPlan->mtrRowIdx % 2];

    // Base Table�� �������� ������, Memory Column�� �������� �����
    // ó���� ���� Tuple Set�� ������Ų��.
    for ( sNode = aDataPlan->mtrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate, sNode, aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

SInt
qmnGRBY::compareRows( qmndGRBY   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ������ �� Row���� ���� ���θ� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmdMtrNode  * sNode;

    SInt          sResult = -1;
    mtdValueInfo  sValueInfo1;
    mtdValueInfo  sValueInfo2;

    for( sNode = aDataPlan->groupNode;
         sNode != NULL;
         sNode = sNode->next )
    {
        sValueInfo1.value  = sNode->func.getRow( sNode, aDataPlan->mtrRow[0]);
        sValueInfo1.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo1.flag   = MTD_OFFSET_USE;
        
        sValueInfo2.value  = sNode->func.getRow( sNode, aDataPlan->mtrRow[1]);
        sValueInfo2.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo2.flag   = MTD_OFFSET_USE;
        
        sResult = sNode->func.compare( &sValueInfo1, &sValueInfo2 );
        
        if( sResult != 0 )
        {
            break;
        }
    }

    return sResult;
}
