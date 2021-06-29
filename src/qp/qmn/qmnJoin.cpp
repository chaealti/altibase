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
 * $Id: qmnJoin.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     JOIN(JOIN) Node
 *
 *     ������ �𵨿��� cartesian product�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Cartesian Product
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnHash.h>
#include <qmnSort.h>
#include <qmnJoin.h>
#include <qcg.h>
#include <qmoUtil.h>

IDE_RC
qmnJOIN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    JOIN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnJOIN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan =
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    sDataPlan->doIt = qmnJOIN::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_JOIN_INIT_DONE_MASK)
         == QMND_JOIN_INIT_DONE_FALSE )
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

    switch( sCodePlan->flag & QMNC_JOIN_TYPE_MASK )
    {
        case QMNC_JOIN_TYPE_INNER:
            sDataPlan->doIt = qmnJOIN::doItLeft;
            break;
        case QMNC_JOIN_TYPE_SEMI:
            {
                switch ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
                {
                    // BUG-43950 INVERSE_HASH �� doItInverse �� ����Ѵ�.
                    case QMN_PLAN_JOIN_METHOD_INVERSE_HASH :
                    case QMN_PLAN_JOIN_METHOD_INVERSE_SORT :
                        sDataPlan->doIt = qmnJOIN::doItInverse;
                        break;
                    case QMN_PLAN_JOIN_METHOD_INVERSE_INDEX :
                        sDataPlan->doIt = qmnJOIN::doItLeft;
                        break;
                    default:
                        sDataPlan->doIt = qmnJOIN::doItSemi;
                }
                break;
            }
        case QMNC_JOIN_TYPE_ANTI:
            {
                switch ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
                {
                    case QMN_PLAN_JOIN_METHOD_INVERSE_HASH :
                    case QMN_PLAN_JOIN_METHOD_INVERSE_SORT :
                        sDataPlan->doIt = qmnJOIN::doItInverse;
                        break;
                    default:
                        sDataPlan->doIt = qmnJOIN::doItAnti;
                }
                break;
            }
        default:
            IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnJOIN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    JOIN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnJOIN::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan =
        (qmndJOIN*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnJOIN::padNull( qcTemplate * aTemplate,
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

#define IDE_FN "qmnJOIN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    // qmndJOIN * sDataPlan =
    //   (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_JOIN_INIT_DONE_MASK)
         == QMND_JOIN_INIT_DONE_FALSE )
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
qmnJOIN::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     JOIN�� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnJOIN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncJOIN * sCodePlan = (qmncJOIN*) aPlan;
    qmndJOIN * sDataPlan =
        (qmndJOIN*) (aTemplate->tmplate.data + aPlan->offset);
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
    // JOIN ��� ǥ��
    //----------------------------

    switch( sCodePlan->flag & QMNC_JOIN_TYPE_MASK )
    {
        case QMNC_JOIN_TYPE_INNER:
            iduVarStringAppend( aString, "JOIN" );
            break;
        case QMNC_JOIN_TYPE_SEMI:
            iduVarStringAppend( aString, "SEMI-JOIN" );
            break;
        case QMNC_JOIN_TYPE_ANTI:
            iduVarStringAppend( aString, "ANTI-JOIN" );
            break;
        default:
            IDE_DASSERT( 0 );
    }

    /* PROJ-2339, 2385 */
    if ( ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
          == QMN_PLAN_JOIN_METHOD_INVERSE_INDEX ) ||
         ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
          == QMN_PLAN_JOIN_METHOD_INVERSE_HASH ) ||
         ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
          == QMN_PLAN_JOIN_METHOD_INVERSE_SORT ) )
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
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    // Filter ���� ���
    if ( sCodePlan->filter != NULL)
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
        else
        {
            // Nothing To Do
        }

        // Subquery ���� ���
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->filter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
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
qmnJOIN::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnJOIN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT(0);

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnJOIN::doItLeft( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Left���� �Ѱ�, Right���� �Ѱ� ����
 *
 * Implementation :
 *    - Left���� �Ѱ� Fetch�� �� Right ����
 *    - ���ǿ� �´� Right ���� ��� �ٽ� Left ����
 *    - Left�� ���� ������ �ݺ�
 *
 ***********************************************************************/

#define IDE_FN "qmnJOIN::doItLeft"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan =
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag    = QMC_ROW_DATA_NONE;
    idBool     sRetry;

    //-------------------------------------
    // Left�� Right�� ��� �ְų�,
    // Left�� ���� ������ �ݺ� ����
    //-------------------------------------

    while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );

        if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            // To Fix PR-9822
            // Right�� Left�� ���� ������ �ʱ�ȭ�� �־�� �Ѵ�.
            IDE_TEST( aPlan->right->init( aTemplate,
                                          aPlan->right ) != IDE_SUCCESS);

            do
            {
                IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, &sFlag )
                          != IDE_SUCCESS );

                IDE_TEST( checkFilter( aTemplate,
                                       sCodePlan,
                                       sFlag,
                                       &sRetry )
                          != IDE_SUCCESS );
            } while( sRetry == ID_TRUE );

        }
        else
        {
            // end of left child
            break;
        }
    }

    //-------------------------------------
    // ���� ���� �Լ� ����
    //-------------------------------------

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // ���� ���� �� Right���� �켱 �˻�
        sDataPlan->doIt = qmnJOIN::doItRight;
    }
    else
    {
        // ���� ���� �� Left�� �켱 �˻�
        sDataPlan->doIt = qmnJOIN::doItLeft;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnJOIN::doItRight( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ������ Left�� �ΰ� Right���� �Ѱ� ����
 *
 * Implementation :
 *    - Right�� ����
 *    - �������� �ʴ´ٸ� Left���� �����ϵ��� ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qmnJOIN::doItRight"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    // qmndJOIN * sDataPlan =
    //     (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);
    idBool sRetry;

    do
    {
        IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
                  != IDE_SUCCESS );

        IDE_TEST( checkFilter( aTemplate,
                               sCodePlan,
                               *aFlag,
                               &sRetry )
                  != IDE_SUCCESS );
    } while( sRetry == ID_TRUE );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        // nothing to do
    }
    else
    {
        IDE_TEST( qmnJOIN::doItLeft( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnJOIN::doItSemi( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
    qmncJOIN   * sCodePlan = (qmncJOIN *) aPlan;
    qmcRowFlag   sLeftFlag  = QMC_ROW_DATA_NONE;
    qmcRowFlag   sRightFlag = QMC_ROW_DATA_NONE;
    idBool       sRetry;

    //-------------------------------------
    // Left�� right�� ��� �ְų�,
    // �Ǵ� left�� ���� ������ �ݺ� ����
    //-------------------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, &sLeftFlag )
              != IDE_SUCCESS );

    while( ( sLeftFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( aPlan->right->init( aTemplate,
                                      aPlan->right ) != IDE_SUCCESS);

        do
        {
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, &sRightFlag )
                      != IDE_SUCCESS );

            IDE_TEST( checkFilter( aTemplate,
                                   sCodePlan,
                                   sRightFlag,
                                   &sRetry )
                      != IDE_SUCCESS );
        } while( sRetry == ID_TRUE );

        if( ( sRightFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
        {
            break;
        }
        else
        {
            IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, &sLeftFlag )
                      != IDE_SUCCESS );
        }
    }

    // ���� ����� left ����� ���� ����
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= ( sLeftFlag & QMC_ROW_DATA_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnJOIN::doItAnti( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
    qmncJOIN   * sCodePlan = (qmncJOIN *) aPlan;
    qmcRowFlag   sLeftFlag  = QMC_ROW_DATA_NONE;
    qmcRowFlag   sRightFlag = QMC_ROW_DATA_NONE;
    idBool       sRetry;

    //-------------------------------------
    // Left�� �ְ� right�� ���� ������,
    // �Ǵ� left�� ���� ������ �ݺ� ����
    //-------------------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, &sLeftFlag )
              != IDE_SUCCESS );

    while( ( sLeftFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        IDE_TEST( aPlan->right->init( aTemplate,
                                      aPlan->right ) != IDE_SUCCESS);

        do
        {
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, &sRightFlag )
                      != IDE_SUCCESS );

            IDE_TEST( checkFilter( aTemplate,
                                   sCodePlan,
                                   sRightFlag,
                                   &sRetry )
                      != IDE_SUCCESS );
        } while( sRetry == ID_TRUE );

        if( ( sRightFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, &sLeftFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            break;
        }
    }

    // ���� ����� left ����� ���� ����
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= ( sLeftFlag & QMC_ROW_DATA_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnJOIN::doItInverse( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Semi Join Inverse Sort + Anti Join Inverse (Hash + Sort) �� ���� ó��
 *
 * Implementation :
 *     ��ġ�ϴ� RIGHT Record�� Hit Flag�� �ۼ��ϰ�,
 *     Hit/Non-Hit �� RIGHT Record�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan = 
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

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
        while ( ID_TRUE )
        {
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
            // Search Next Right Row / Return this Row
            //------------------------------------

            if ( (sRightFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                IDE_TEST( sDataPlan->setHitFlag( aTemplate, aPlan->right )
                          != IDE_SUCCESS );
            }
            else
            {
                break;
            }
        }

        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                  != IDE_SUCCESS );
    }

    IDE_DASSERT( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE );

    switch ( sCodePlan->flag & QMNC_JOIN_TYPE_MASK )
    {
        case QMNC_JOIN_TYPE_SEMI:
            IDE_TEST( qmnJOIN::doItInverseHitFirst( aTemplate, aPlan, aFlag ) 
                      != IDE_SUCCESS );
            break;
        case QMNC_JOIN_TYPE_ANTI:
            IDE_TEST( qmnJOIN::doItInverseNonHitFirst( aTemplate, aPlan, aFlag ) 
                      != IDE_SUCCESS );
            break;
        default:
            IDE_DASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnJOIN::doItInverseHitFirst( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Semi Join Inverse Sort �� ���� ��ӵ� ó��
 *
 * Implementation :
 *     Hit�� Record�� ó������ �˻��Ѵ�
 *
 ***********************************************************************/
    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan = 
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( aPlan->right->init( aTemplate, 
                                  aPlan->right ) != IDE_SUCCESS);
    
    //------------------------------------
    // Right�� ���Ͽ� Hit �˻� ���� ����
    //------------------------------------

    if ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
        == QMN_PLAN_JOIN_METHOD_INVERSE_HASH )
    {   
        IDE_DASSERT( sCodePlan->plan.right->type == QMN_HASH );
        qmnHASH::setHitSearch( aTemplate, aPlan->right );
    }   
    else
    {
        IDE_DASSERT( sCodePlan->plan.right->type == QMN_SORT );
        qmnSORT::setHitSearch( aTemplate, aPlan->right );
    }

    //------------------------------------
    // Read Right Row
    //------------------------------------
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnJOIN::doItInverseHitNext;
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnJOIN::doItInverse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmnJOIN::doItInverseHitNext( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Semi Join Inverse Sort �� ���� ��ӵ� ó��
 *
 * Implementation :
 *     Hit�� Record�� ����ؼ� �˻��Ѵ�
 *
 ***********************************************************************/
    qmndJOIN* sDataPlan = 
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    //------------------------------------
    // Read Right Row (Continuously)
    //------------------------------------
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // ��� ����
        sDataPlan->doIt = qmnJOIN::doItInverse;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmnJOIN::doItInverseNonHitFirst( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Anti Join Inverse �� ���� ��ӵ� ó��
 *
 * Implementation :
 *     Hit���� ���� Record�� ó������ �˻��Ѵ�
 *
 ***********************************************************************/
    qmncJOIN * sCodePlan = (qmncJOIN *) aPlan;
    qmndJOIN * sDataPlan = 
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( aPlan->right->init( aTemplate, 
                                  aPlan->right ) != IDE_SUCCESS);
    
    //------------------------------------
    // Right�� ���Ͽ� Non-Hit �˻� ���� ����
    //------------------------------------

    if ( ( sCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
        == QMN_PLAN_JOIN_METHOD_INVERSE_HASH )
    {   
        IDE_DASSERT( sCodePlan->plan.right->type == QMN_HASH );
        qmnHASH::setNonHitSearch( aTemplate, aPlan->right );
    }   
    else
    {
        IDE_DASSERT( sCodePlan->plan.right->type == QMN_SORT );
        qmnSORT::setNonHitSearch( aTemplate, aPlan->right );
    }

    //------------------------------------
    // Read Right Row
    //------------------------------------
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnJOIN::doItInverseNonHitNext;
    }
    else
    {
        // ��� ����
        sDataPlan->doIt = qmnJOIN::doItInverse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmnJOIN::doItInverseNonHitNext( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Anti Join Inverse �� ���� ��ӵ� ó��
 *
 * Implementation :
 *     Hit���� ���� Record�� ����ؼ� �˻��Ѵ�
 *
 ***********************************************************************/
    qmndJOIN* sDataPlan = 
        (qmndJOIN *) (aTemplate->tmplate.data + aPlan->offset);

    //------------------------------------
    // Read Right Row (Continuously)
    //------------------------------------
    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, aFlag )
              != IDE_SUCCESS );
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // ��� ����
        sDataPlan->doIt = qmnJOIN::doItInverse;
   }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmnJOIN::checkFilter( qcTemplate * aTemplate,
                      qmncJOIN   * aCodePlan,
                      UInt         aRightFlag,
                      idBool     * aRetry )
{
    idBool sJudge;

    if( ( ( aRightFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST ) &&
        ( aCodePlan->filter != NULL ) )
    {
        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->filter,
                              aTemplate )
                  != IDE_SUCCESS );

        if( sJudge == ID_FALSE )
        {
            *aRetry = ID_TRUE;
        }
        else
        {
            *aRetry = ID_FALSE;
        }
    }
    else
    {
        *aRetry = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnJOIN::firstInit( qmncJOIN   * aCodePlan,
                    qmndJOIN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    JOIN node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    //---------------------------------
    // Hit Flag �Լ� ������ ����
    //---------------------------------

    switch ( aCodePlan->plan.flag & QMN_PLAN_JOIN_METHOD_TYPE_MASK )
    {
        case QMN_PLAN_JOIN_METHOD_INVERSE_HASH :
            IDE_DASSERT( aCodePlan->plan.right->type == QMN_HASH );
            aDataPlan->setHitFlag   = qmnHASH::setHitFlag;
            aDataPlan->isHitFlagged = qmnHASH::isHitFlagged;
            break;
        case QMN_PLAN_JOIN_METHOD_INVERSE_SORT :
            IDE_DASSERT( aCodePlan->plan.right->type == QMN_SORT );
            aDataPlan->setHitFlag   = qmnSORT::setHitFlag;
            aDataPlan->isHitFlagged = qmnSORT::isHitFlagged;
            break;
        default :
            // Nothing to do.
            break;
    }
     
    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_JOIN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_JOIN_INIT_DONE_TRUE;

    return IDE_SUCCESS;
}
