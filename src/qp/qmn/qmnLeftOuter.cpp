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
 * $Id: qmnLeftOuter.cpp 90785 2021-05-06 07:26:22Z hykim $
 *
 * Description :
 *     LOJN(Left Outer JoiN) Node
 *
 *     ������ �𵨿��� Left Outer Join�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *         - Full Outer Join�� Anti-Outer ����ȭ �����
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
#include <qmnHash.h>
#include <qmnLeftOuter.h>
#include <qcg.h>


IDE_RC 
qmnLOJN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    LOJN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnLOJN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnLOJN::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_LOJN_INIT_DONE_MASK)
         == QMND_LOJN_INIT_DONE_FALSE )
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
    if ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
         == QMN_PLAN_JOIN_METHOD_INVERSE_HASH )
    {
        sDataPlan->doIt = qmnLOJN::doItInverseLeft;
    }
    else
    {
        sDataPlan->doIt = qmnLOJN::doItLeft;
    }

    // PROJ-2750
    sDataPlan->mSkipRightCnt = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnLOJN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    LOJN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLOJN::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnLOJN::padNull( qcTemplate * aTemplate,
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

#define IDE_FN "qmnLOJN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    // qmndLOJN * sDataPlan = 
    //     (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_LOJN_INIT_DONE_MASK)
         == QMND_LOJN_INIT_DONE_FALSE )
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
qmnLOJN::printPlan( qcTemplate   * aTemplate,
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

#define IDE_FN "qmnLOJN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLOJN * sCodePlan = (qmncLOJN*) aPlan;
    qmndLOJN * sDataPlan = 
       (qmndLOJN*) (aTemplate->tmplate.data + aPlan->offset);
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
    // LOJN ��� ǥ��
    //----------------------------
    iduVarStringAppend( aString,
                        "LEFT-OUTER-JOIN" );

    /* PROJ-2339 */
    if ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
        == QMN_PLAN_JOIN_METHOD_INVERSE_HASH )
    {
        iduVarStringAppend( aString, " INVERSE ( " ); // add 'INVERSE' keyword
    }
    else
    {
        iduVarStringAppend( aString, " ( " );
    }

    //----------------------------
    // Join Method ���
    //----------------------------
    qmn::printJoinMethod( aString, sCodePlan->plan.flag );

    //----------------------------
    // PROJ-2750 Skip Right Count ���
    //----------------------------
    printSkipRightCnt( aTemplate,
                       aString,
                       sCodePlan->flag,
                       sDataPlan->mSkipRightCnt,
                       aMode );

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
qmnLOJN::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnLOJN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT(0);
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnLOJN::doItLeft( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���ο� Left Row�� ���� ó��
 *
 * Implementation :
 *     JOIN�� �޸� Left�� �����ϴ� Right Row�� �������� ���� ���
 *     Null Padding�Ͽ� ����� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnLOJN::doItLeft"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan =
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag = QMC_ROW_DATA_NONE;
    idBool sJudge;

    //------------------------------------
    // Read Left Row
    //------------------------------------

    // PROJ-2750
    *aFlag &= ~QMC_ROW_NULL_PADDING_MASK;
    *aFlag |=  QMC_ROW_NULL_PADDING_FALSE;

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // PROJ-2750
        if ( ( ( sCodePlan->flag & QMNC_LOJN_SKIP_RIGHT_COND_MASK ) == QMNC_LOJN_SKIP_RIGHT_COND_TRUE ) &&
             ( ( *aFlag & QMC_ROW_NULL_PADDING_MASK ) == QMC_ROW_NULL_PADDING_TRUE ) )
        {
            IDE_TEST( aPlan->right->init( aTemplate, aPlan->right ) != IDE_SUCCESS );
            IDE_TEST( aPlan->right->padNull( aTemplate, aPlan->right ) != IDE_SUCCESS );
            sDataPlan->mSkipRightCnt++;
            sDataPlan->doIt = qmnLOJN::doItLeft;
        }
        else
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
            // Right Row�� ���� ��� Null Padding
            //------------------------------------

            if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                // PROJ-2750
                *aFlag |= QMC_ROW_NULL_PADDING_FALSE;
                sDataPlan->doIt = qmnLOJN::doItRight;
            }
            else
            {
                IDE_TEST( aPlan->right->padNull( aTemplate, aPlan->right )
                          != IDE_SUCCESS );

                // PROJ-2750
                *aFlag |= QMC_ROW_NULL_PADDING_TRUE;
                sDataPlan->doIt = qmnLOJN::doItLeft;
            }
        }
    }
    else
    {
        // �� �̻� ����� ����
        // PROJ-2750
        *aFlag |= QMC_ROW_NULL_PADDING_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnLOJN::doItRight( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���ο� Right Row�� ���� ó��
 *
 * Implementation :
 *     Filter ������ ������ ������ �ݺ� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnLOJN::doItRight"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;
    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

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

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        // ���� ����� ���� ��� ���ο� Left Row�� �̿��� ó��
        IDE_TEST( qmnLOJN::doItLeft( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
        if ( (*aFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_EXIST )
        {
            sDataPlan->doIt = qmnLOJN::doItLeft;
        }
        else
        {
            // nothing to do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnLOJN::doItInverseLeft( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ���ο� Left Row�� ���� ó��
 *
 * Implementation :
 *     JOIN�� �޸� Left�� �����ϴ� Right Row�� �������� ���� ���
 *     Null Padding�Ͽ� ����� �����Ѵ�.
 *
 ***********************************************************************/

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sLeftFlag = QMC_ROW_DATA_NONE;
    qmcRowFlag sRightFlag = QMC_ROW_DATA_NONE;
    idBool sJudge;

    //------------------------------------
    // Left Row�� ��� Ž��
    //------------------------------------
    
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
              != IDE_SUCCESS );

    while ( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        //------------------------------------
        // Read Right Row
        //------------------------------------

        // To Fix PR-9822
        // Right�� Left�� ���� ������ �ʱ�ȭ�� �־�� �Ѵ�.
        IDE_TEST( aPlan->right->init( aTemplate, 
                                      aPlan->right ) != IDE_SUCCESS);
        
        IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sRightFlag )
                  != IDE_SUCCESS );

        //------------------------------------
        // Filter ������ ������ ������ �ݺ�
        //------------------------------------
        
        while ( (sRightFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
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
                                              & sRightFlag ) != IDE_SUCCESS );
            }
        }

        //------------------------------------
        // Return this Row / Search Next Left Row
        //------------------------------------
        
        if ( (sRightFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( sDataPlan->setHitFlag( aTemplate, aPlan->right )
                      != IDE_SUCCESS );

            sDataPlan->doIt = qmnLOJN::doItInverseRight;

            *aFlag &= ~QMC_ROW_DATA_MASK;
            *aFlag |= QMC_ROW_DATA_EXIST;

            break;
        }
        else
        {
            // Matched right record is not found.
            // Search the hashtable with next left one.
            IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                      != IDE_SUCCESS );

        }
    }

    // Non-Hit Phase
    if ( (sLeftFlag & QMC_ROW_DATA_MASK) != QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( qmnLOJN::doItInverseNonHitFirst( aTemplate, aPlan, aFlag ) 
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do : return this record
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnLOJN::doItInverseRight( qcTemplate * aTemplate,
                                  qmnPlan    * aPlan,
                                  qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Right Row�� ��� ó��
 *
 * Implementation :
 *     Right Row�� �� �����ϸ� Join ����� �����Ѵ�.
 *
 ***********************************************************************/

    qmncLOJN * sCodePlan = (qmncLOJN *) aPlan;
    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

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
        IDE_TEST( qmnLOJN::doItInverseLeft( aTemplate, aPlan, aFlag ) 
                  != IDE_SUCCESS );

        if ( (*aFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_EXIST )
        {
            // It goes to endpoint.
            sDataPlan->doIt = qmnLOJN::doItInverseLeft;
        }
        else
        {
            // nothing to do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnLOJN::doItInverseNonHitFirst( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left Outer Inverse Join ��ó�� �������� ���� ���� �Լ�
 *
 * Implementation :
 *    Child Plan�� �˻� ��带 �����Ѵ�.
 *    Hit Flag�� ���� Child Row�� ȹ���ϰ� �ȴ�.
 *
 ***********************************************************************/

    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

    // To Fix PR-9822
    // Right�� Left�� ���� ��쵵 �ʱ�ȭ�� �־�� �Ѵ�.
    IDE_TEST( aPlan->right->init( aTemplate, 
                                  aPlan->right ) != IDE_SUCCESS);
    
    //------------------------------------
    // Right�� ���Ͽ� Non-Hit �˻� ���� ����
    //------------------------------------
    qmnHASH::setNonHitSearch( aTemplate, aPlan->right );

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
        sDataPlan->doIt = qmnLOJN::doItInverseNonHitNext;

        // PROJ-2750
        *aFlag |= QMC_ROW_NULL_PADDING_TRUE;
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnLOJN::doItInverseLeft;

        // PROJ-2750
        *aFlag |= QMC_ROW_NULL_PADDING_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmnLOJN::doItInverseNonHitNext( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left Outer Inverse Join ��ó�� �������� ���� ���� �Լ�
 *
 * Implementation :
 *    Child Plan�� �˻� ��带 �����Ѵ�.
 *    Hit Flag�� ���� Child Row�� ȹ���ϰ� �ȴ�.
 *
 ***********************************************************************/

    qmndLOJN * sDataPlan = 
        (qmndLOJN *) (aTemplate->tmplate.data + aPlan->offset);

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

        // PROJ-2750
        *aFlag |= QMC_ROW_NULL_PADDING_TRUE;
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnLOJN::doItInverseLeft;

        // PROJ-2750
        *aFlag |= QMC_ROW_NULL_PADDING_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC 
qmnLOJN::firstInit( qmncLOJN   * aCodePlan,
                    qmndLOJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 * Implementation :
 *
 ***********************************************************************/

    if ( ( aCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
        == QMN_PLAN_JOIN_METHOD_INVERSE_HASH )
    {   
        IDE_DASSERT( aCodePlan->plan.right->type == QMN_HASH );
        aDataPlan->setHitFlag = qmnHASH::setHitFlag;
    }   
    else
    {
        // Nothing to do.   
    } 

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_LOJN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_LOJN_INIT_DONE_TRUE;

    return IDE_SUCCESS;
}

void qmnLOJN::printSkipRightCnt( qcTemplate   * aTemplate,
                                 iduVarString * aString,
                                 UInt           aFlag,
                                 UInt           aSkipRightCnt,
                                 qmnDisplay     aMode )
{
/***********************************************************************
 * Description : PROJ-2750 Skip Right Count �� ����Ѵ�.
 ***********************************************************************/

    if ( ( aMode == QMN_DISPLAY_ALL ) &&
         ( QCG_GET_SESSION_TRCLOG_DETAIL_INFORMATION( aTemplate->stmt ) == 1 ) &&
         ( ( aFlag & QMNC_LOJN_SKIP_RIGHT_COND_MASK ) == QMNC_LOJN_SKIP_RIGHT_COND_TRUE ) &&
         ( aSkipRightCnt > 0 ) )
    {
        iduVarStringAppendFormat( aString, ", SKIP RIGHT COUNT: %"ID_INT32_FMT"", aSkipRightCnt );
    }
}
