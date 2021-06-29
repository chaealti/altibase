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
 * $Id: qmnConcatenate.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     BUNI(Bag Union) Node
 *
 *     ������ �𵨿��� Bag Union�� �����ϴ� Plan Node �̴�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Bag Union
 *         - Set Union
 *
 *     Left Child�� ���� Data�� Right Child�� ���� Data��
 *     ��� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnConcatenate.h>


IDE_RC 
qmnCONC::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    CONC ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncCONC * sCodePlan = (qmncCONC *) aPlan;
    qmndCONC * sDataPlan = 
        (qmndCONC *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    
    sDataPlan->doIt = qmnCONC::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_CONC_INIT_DONE_MASK)
         == QMND_CONC_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    IDE_TEST( aPlan->left->init( aTemplate, 
                                 aPlan->left ) != IDE_SUCCESS);

    // Right Child�� ���� �ʱ�ȭ�� �ʿ� ������ �����Ѵ�.
    
    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnCONC::doItLeft;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnCONC::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    CONC�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncCONC * sCodePlan = (qmncCONC *) aPlan;
    qmndCONC * sDataPlan = 
        (qmndCONC*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnCONC::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Null Padding�� �����Ѵ�.
 *
 * Implementation :
 *    ������ Null Row�� ������ ������,
 *    Child�� ���� Null Padding�� �����Ѵ�.
 *    �Ϲ� Two-Child Plan�� �޸� Left�� Right�� ������ ������
 *    �����Ƿ� Left�� ���� Null Padding���� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncCONC * sCodePlan = (qmncCONC *) aPlan;
    // qmndCONC * sDataPlan = 
    //     (qmndCONC *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_CONC_INIT_DONE_MASK)
         == QMND_CONC_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Child �� ���� Null Padding
    //------------------------------------------------
    
    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnCONC::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     ���� ������ ����Ѵ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncCONC * sCodePlan = (qmncCONC*) aPlan;
    qmndCONC * sDataPlan = 
        (qmndCONC*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

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
    // CONC ��� ǥ��
    //----------------------------

    iduVarStringAppend( aString,
                        "CONCATENATION \n" );

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
    // Child Plan�� ���� ���
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
qmnCONC::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnCONC::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnCONC::doItLeft( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left Child�κ��� Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::doItLeft"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncCONC * sCodePlan = (qmncCONC*) aPlan;
    qmndCONC * sDataPlan = 
        (qmndCONC*) (aTemplate->tmplate.data + aPlan->offset);

    //----------------------------
    // Read Left Row
    //----------------------------
    
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag ) 
              != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        // Row�� �������� ���� ��� Right Child�� ���� ó���� ����
        
        sDataPlan->doIt = qmnCONC::doItRight;
        IDE_TEST( aPlan->right->init( aTemplate, aPlan->right ) 
                  != IDE_SUCCESS );
        
        IDE_TEST( qmnCONC::doItRight( aTemplate, aPlan, aFlag ) 
                  != IDE_SUCCESS );
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
qmnCONC::doItRight( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Right Child�κ��� Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::doItRight"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncCONC * sCodePlan = (qmncCONC*) aPlan;
    qmndCONC * sDataPlan = 
        (qmndCONC*) (aTemplate->tmplate.data + aPlan->offset);

    //----------------------------
    // Read Right Row
    //----------------------------

    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag ) 
              != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        sDataPlan->doIt = qmnCONC::doItLeft;

        *aFlag = QMC_ROW_DATA_NONE;
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
qmnCONC::firstInit( qmndCONC   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCONC::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_CONC_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_CONC_INIT_DONE_TRUE;
    
    return IDE_SUCCESS;
    
#undef IDE_FN
}

