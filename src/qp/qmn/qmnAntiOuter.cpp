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
 * $Id: qmnAntiOuter.cpp 82075 2018-01-17 06:39:52Z jina.kim $ 
 *
 * Description :
 *     AOJN(Anti Outer JoiN) Node
 *
 *     ������ �𵨿��� Full Outer Join�� ó���� ����
 *     Ư���� ������ �����ϴ� Plan Node �̴�.
 *
 *     Left Outer Join�� �帧�� �����ϳ� ������ ���� ū ���̰� �ִ�.
 *        - Left Row�� �����ϴ� Right Row�� ���� ��츦 �˻��Ѵ�.
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
#include <qmnAntiOuter.h>
#include <qcg.h>


IDE_RC 
qmnAOJN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    AOJN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAOJN * sCodePlan = (qmncAOJN *) aPlan;
    qmndAOJN * sDataPlan = 
        (qmndAOJN *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnAOJN::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_AOJN_INIT_DONE_MASK)
         == QMND_AOJN_INIT_DONE_FALSE )
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

    // To Fix PR-9822
    // doIt() �������� �ʱ�ȭ�ȴ�.
    // IDE_TEST( aPlan->right->init( aTemplate, 
    //                               aPlan->right ) != IDE_SUCCESS);

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnAOJN::doItLeft;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnAOJN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    AOJN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndAOJN * sDataPlan = 
        (qmndAOJN *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnAOJN::padNull( qcTemplate * aTemplate,
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
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAOJN * sCodePlan = (qmncAOJN *) aPlan;
    // qmndAOJN * sDataPlan = 
    //   (qmndAOJN *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_AOJN_INIT_DONE_MASK)
         == QMND_AOJN_INIT_DONE_FALSE )
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

    IDE_TEST( aPlan->right->padNull( aTemplate, aPlan->right )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnAOJN::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAOJN * sCodePlan = (qmncAOJN*) aPlan;
    qmndAOJN * sDataPlan = 
       (qmndAOJN*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // AOJN ��� ǥ��
    //----------------------------

    iduVarStringAppend( aString,
                        "ANTI-OUTER-JOIN( " );

    //----------------------------
    // Join Method ���
    //----------------------------
    qmn::printJoinMethod( aString, sCodePlan->plan.flag );

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    //----------------------------
    // Predicate ���� ǥ��
    //----------------------------
    
    if ( sCodePlan->filter != NULL )
    {
        if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
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
                                              sCodePlan->filter)
                != IDE_SUCCESS);
        }

        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->filter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
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
qmnAOJN::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnAOJN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnAOJN::doItLeft( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���ο� Left Row�� ���� ó��
 *
 * Implementation :
 *    �����ϴ� Right Row�� ���� Left Row�� ã��
 *    Right Row�� Null Padding �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::doItLeft"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncAOJN * sCodePlan = (qmncAOJN *) aPlan;
    // qmndAOJN * sDataPlan = 
    //     (qmndAOJN *) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;
    qmcRowFlag sFlag = QMC_ROW_DATA_EXIST;

    //----------------------------------
    // �����ϴ� Right Row�� ���� Left Row��
    // ã�� ������ �ݺ� ����
    //----------------------------------
    
    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );

        if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( aPlan->right->init( aTemplate, aPlan->right )
                      != IDE_SUCCESS );
            
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
                      != IDE_SUCCESS );
            while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
            {
                if ( sCodePlan->filter != NULL )
                {
                    IDE_TEST( qtc::judge( & sJudge,
                                          sCodePlan->filter,
                                          aTemplate ) != IDE_SUCCESS );
                }
                else
                {
                    sJudge = ID_TRUE;
                }
                
                if ( sJudge == ID_TRUE )
                {
                    break;
                }
                else
                {
                    IDE_TEST( aPlan->right->doIt( aTemplate, 
                                                  aPlan->right, 
                                                  & sFlag ) != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // end of left child
            break;
        }
    }

    //----------------------------------
    // Left Row ã�� ��� Right�� ���� Null Padding
    //----------------------------------
    
    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( aPlan->right->padNull(aTemplate, aPlan->right )
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
qmnAOJN::firstInit( qmndAOJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnAOJN::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_AOJN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_AOJN_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}

