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
 * $Id: qmnFullOuter.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     FOJN(Full Outer JoiN) Node
 *
 *     ������ �𵨿��� Full Outer Join�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *
 *     Full Outer Join�� ũ�� ������ ���� �� �ܰ�� ����ȴ�.
 *         - Left Outer Join Phase
 *             : Left Outer Join�� �����ϳ�, �����ϴ� Right Row�� ���Ͽ�
 *               Hit Flag�� Setting�Ͽ� ���� Phase�� ���� ó���� �غ��Ѵ�.
 *         - Right Outer Join Phase
 *             : Right Row�� Hit ���� ���� Row�� ��� Left�� ����
 *               Null Padding�� �����Ѵ�.
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
#include <qmnSort.h>
#include <qmnHash.h>
#include <qmnFullOuter.h>
#include <qcg.h>


IDE_RC 
qmnFOJN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    FOJN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    sDataPlan->doIt = qmnFOJN::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_FOJN_INIT_DONE_MASK)
         == QMND_FOJN_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(sCodePlan, sDataPlan) != IDE_SUCCESS );
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

    /*
     * PROJ-2402 Parallel Table Scan
     * parallel scan �� ���
     * �ʿ����ʿ� ���� thread �� �й��ϱ� ����
     * �����ʿ� PRLQ, HASH(�Ǵ� SORT) �� ������
     * �������� ���� init �Ѵ�.
     */
    if (((aPlan->right->flag & QMN_PLAN_PRLQ_EXIST_MASK) ==
         QMN_PLAN_PRLQ_EXIST_TRUE) &&
        ((aPlan->right->flag & QMN_PLAN_MTR_EXIST_MASK) ==
         QMN_PLAN_MTR_EXIST_TRUE))
    {
        IDE_TEST(aPlan->right->init(aTemplate,
                                    aPlan->right)
                 != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnFOJN::doItLeftHitPhase;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnFOJN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    FOJN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //  qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::padNull( qcTemplate * aTemplate,
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

#define IDE_FN "qmnFOJN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    // qmndFOJN * sDataPlan = 
    //    (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_FOJN_INIT_DONE_MASK)
         == QMND_FOJN_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
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
qmnFOJN::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN*) aPlan;
    qmndFOJN * sDataPlan = 
       (qmndFOJN*) (aTemplate->tmplate.data + aPlan->offset);
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
    // FOJN ��� ǥ��
    //----------------------------
    
    iduVarStringAppend( aString,
                        "FULL-OUTER-JOIN ( " );

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
qmnFOJN::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnFOJN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT(0);

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::doItLeftHitPhase( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left Outer Join ó�� �������� ���ο� Left Row�� ���� ó��
 *
 * Implementation :
 *    LOJN�� �޸� �����ϴ� Right Row ���� �� Hit Flag�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::doItLeftHitPhase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;
    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    //------------------------------------
    // Read Left Row
    //------------------------------------
    
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //------------------------------------
        // Read Right Row
        //------------------------------------

        // To Fix PR-9822
        // Right�� Left�� ���� ������ �ʱ�ȭ�� �־�� �Ѵ�.
        IDE_TEST( aPlan->right->init( aTemplate, 
                                      aPlan->right ) != IDE_SUCCESS);
        
        IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
                  != IDE_SUCCESS );

        //------------------------------------
        // Filter ������ ������ ������ �ݺ�
        //------------------------------------
        
        while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            if ( sCodePlan->filter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge, sCodePlan->filter, aTemplate ) 
                          != IDE_SUCCESS );
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

        //------------------------------------
        // Right Row�� ���� ��� Hit Flag ����
        // Right Row�� ���� ��� Null Padding
        //------------------------------------
        
        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( sDataPlan->setHitFlag( aTemplate, aPlan->right )
                      != IDE_SUCCESS );
            
            sDataPlan->doIt = qmnFOJN::doItRightHitPhase;
        }
        else
        {
            IDE_TEST( aPlan->right->padNull( aTemplate, aPlan->right )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnFOJN::doItLeftHitPhase;
        }
    }
    else
    {
        // �� �̻� ����� ���� ��� Right Outer Join���� ����
        IDE_TEST( doItFirstNonHitPhase( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::doItRightHitPhase( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left Outer Join ó�� �������� ���ο� Right Row�� ���� ó��
 *
 * Implementation :
 *    LOJN�� �޸� �����ϴ� Right Row ���� �� Hit Flag�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::doItRightHitPhase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;
    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    //------------------------------------
    // Read Right Row
    //------------------------------------
    
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
              != IDE_SUCCESS );

    //------------------------------------
    // Filter ������ ������ ������ �ݺ�
    //------------------------------------
    
    while ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        if ( sCodePlan->filter != NULL )
        {
            IDE_TEST( qtc::judge( & sJudge, sCodePlan->filter, aTemplate ) 
                      != IDE_SUCCESS );
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
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
                      != IDE_SUCCESS );
        }
    }

    //------------------------------------
    // Right Row�� ���� ��� Hit Flag ����
    //------------------------------------
    
    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( sDataPlan->setHitFlag( aTemplate, aPlan->right )
                  != IDE_SUCCESS );
        
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        // ���� ����� ���� ��� ���ο� Left Row�� �̿��� ó��
        IDE_TEST( qmnFOJN::doItLeftHitPhase( aTemplate, aPlan, aFlag ) 
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::doItFirstNonHitPhase( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Right Outer Join ó�� �������� ���� ���� �Լ�
 *
 * Implementation :
 *    Child Plan�� �˻� ��带 �����Ѵ�.
 *    Hit Flag�� ���� Child Row�� ȹ���ϰ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::doItFirstNonHitPhase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    // To Fix PR-9822
    // Right�� Left�� ���� ��쵵 �ʱ�ȭ�� �־�� �Ѵ�.
    IDE_TEST( aPlan->right->init( aTemplate, 
                                  aPlan->right ) != IDE_SUCCESS);
    
    //------------------------------------
    // Right�� ���Ͽ� Non-Hit �˻� ���� ����
    //------------------------------------
    
    if ( ( sCodePlan->flag & QMNC_FOJN_RIGHT_CHILD_MASK )
         == QMNC_FOJN_RIGHT_CHILD_HASH )
    {
        qmnHASH::setNonHitSearch( aTemplate, aPlan->right );
    }
    else
    {
        qmnSORT::setNonHitSearch( aTemplate, aPlan->right );
    }

    //------------------------------------
    // Read Right Row
    //------------------------------------
    
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        // Left Row�� ���� Null Padding
        IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
                  != IDE_SUCCESS );
        sDataPlan->doIt = qmnFOJN::doItNextNonHitPhase;
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnFOJN::doItLeftHitPhase;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::doItNextNonHitPhase( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Right Outer Join ó�� �������� ���� ���� �Լ�
 *
 * Implementation :
 *    Hit Flag�� ���� Child Row�� ȹ���ϰ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::doItNextNonHitPhase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncFOJN * sCodePlan = (qmncFOJN *) aPlan;
    qmndFOJN * sDataPlan = 
        (qmndFOJN *) (aTemplate->tmplate.data + aPlan->offset);

    //------------------------------------
    // Read Right Row
    //------------------------------------
    
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
                  != IDE_SUCCESS );
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnFOJN::doItLeftHitPhase;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnFOJN::firstInit( qmncFOJN   * aCodePlan,
                    qmndFOJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Data������ ���� �ʱ�ȭ�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFOJN::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // Hit Flag �Լ� ������ ����
    //---------------------------------

    if( (aCodePlan->flag & QMNC_FOJN_RIGHT_CHILD_MASK )
        == QMNC_FOJN_RIGHT_CHILD_HASH )
    {
        IDE_DASSERT( aCodePlan->plan.right->type == QMN_HASH );
        aDataPlan->setHitFlag = qmnHASH::setHitFlag;
    }
    else
    {
        IDE_DASSERT( aCodePlan->plan.right->type == QMN_SORT );
        aDataPlan->setHitFlag = qmnSORT::setHitFlag;
    }

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_FOJN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_FOJN_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}

