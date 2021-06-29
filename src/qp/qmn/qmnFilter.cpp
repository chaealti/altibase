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
 * $Id: qmnFilter.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     FILT(FILTer) Node
 *
 *     ������ �𵨿��� selection�� �����ϴ� Plan Node �̴�.
 *     SCAN ���� �޸� Storage Manager�� ���� �������� �ʰ�,
 *     �̹� selection�� record�� ���� selection�� �����Ѵ�.
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
#include <qmnFilter.h>
#include <qcg.h>

IDE_RC
qmnFILT::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    FILT ����� �ʱ�ȭ
 *
 * Implementation :
 *    - ���� �ʱ�ȭ�� ���� ���� ��� ���� �ʱ�ȭ ����
 *    - Child Plan�� ���� �ʱ�ȭ
 *    - Constant Filter�� ���� ��� �˻�
 *    - Constant Filter�� ����� ���� ���� �Լ� ����
 *
 ***********************************************************************/

    qmncFILT * sCodePlan = (qmncFILT*) aPlan;
    qmndFILT * sDataPlan =
        (qmndFILT*) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnFILT::doItDefault;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( (*sDataPlan->flag & QMND_FILT_INIT_DONE_MASK)
         == QMND_FILT_INIT_DONE_FALSE )
    {
        // ���� �ʱ�ȭ ����
        IDE_TEST( firstInit(sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Constant Filter�� ����
    //------------------------------------------------

    if ( sCodePlan->constantFilter != NULL )
    {
        IDE_TEST( qtc::judge( & sJudge,
                              sCodePlan->constantFilter,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    //------------------------------------------------
    // Constant Filter�� ���� ���� �Լ� ����
    //------------------------------------------------

    /*
     * BUG-29551
     * constant filter �� false �̸� ���� plan �� ���� ���Ұ��̹Ƿ�
     * init ���� ���� �ʴ´�
     */
    if (sJudge == ID_TRUE)
    {
        /* Child Plan�� �ʱ�ȭ */
        IDE_TEST(aPlan->left->init(aTemplate, aPlan->left) != IDE_SUCCESS);

        /* Constant Filter�� �����ϴ� ��� �Ϲ� ���� �Լ��� ���� */
        sDataPlan->doIt = qmnFILT::doItFirst;
    }
    else
    {
        //-------------------------------------------
        // Constant Filter�� �������� �ʴ� ���
        // - �׻� ������ �������� �����Ƿ�
        //   ��� ����� �������� �ʴ� �Լ��� �����Ѵ�.
        //-------------------------------------------
        sDataPlan->doIt = qmnFILT::doItAllFalse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnFILT::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    FILT�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::doIt"));

    qmndFILT * sDataPlan =
        (qmndFILT*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnFILT::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    FILT ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::padNull"));

    qmncFILT * sCodePlan = (qmncFILT*) aPlan;
    // qmndFILT * sDataPlan =
    //     (qmndFILT*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_FILT_INIT_DONE_MASK)
         == QMND_FILT_INIT_DONE_FALSE )
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnFILT::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    FILT ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::printPlan"));

    qmncFILT * sCodePlan = (qmncFILT*) aPlan;
    qmndFILT * sDataPlan =
        (qmndFILT*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    UInt  i;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // FILT ��� ǥ��
    //----------------------------

    iduVarStringAppend( aString,
                        "FILTER\n" );

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        IDE_TEST( printPredicateInfo( aTemplate,
                                      sCodePlan,
                                      aDepth,
                                      aString ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------
    // Subquery ������ ���
    // Subquery�� ������ ���� predicate���� ������ �� �ִ�.
    //     1. Constant Filter
    //     2. Normal Filter
    //----------------------------

    // To Fix PR-8030
    // Constant Filter�� Subquery ���� ���
    if ( sCodePlan->constantFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->constantFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Normal Filter�� Subquery ���� ���
    if ( sCodePlan->filter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->filter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnFILT::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* Flag */ )
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::doItDefault"));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnFILT::doItAllFalse( qcTemplate * /* aTemplate */,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Filter ������ �����ϴ� Record�� �ϳ��� ���� ��� ���
 *
 *    Constant Filter �˻��Ŀ� �����Ǵ� �Լ��� ���� �����ϴ�
 *    Record�� �������� �ʴ´�.
 *
 * Implementation :
 *    �׻� record ������ �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::doItAllFalse"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::doItAllFalse"));

    qmncFILT * sCodePlan = (qmncFILT*) aPlan;

    // ���ռ� �˻�
    IDE_DASSERT( sCodePlan->constantFilter != NULL );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnFILT::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    FILT�� ���� �Լ�
 *    Child�� �����ϰ� Record�� ���� ��� ������ �˻��Ѵ�.
 *    ������ ������ ������ �̸� �ݺ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncFILT * sCodePlan = (qmncFILT*) aPlan;
    // qmndFILT * sDataPlan =
    //     (qmndFILT*) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge = ID_FALSE;

    while ( sJudge == ID_FALSE )
    {
        // Child�� ����
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );

        if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
        {
            // Record�� ���� ���
            break;
        }
        else
        {
            // Record�� ���� ��� ���� �˻�.

            // fix BUG-10417
            // sCodePlan->filter�� NULL�� ����� ����� �ʿ��ϸ�,
            // sCodePlan->filter == NULL �̸�, sJudge = ID_TRUE�� ����.
            if( sCodePlan->filter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge, sCodePlan->filter, aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                sJudge = ID_TRUE;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnFILT::firstInit( qmncFILT   * aCodePlan,
                    qmndFILT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    FILT node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnFILT::firstInit"));

    // ���ռ� �˻�
    IDE_DASSERT( aCodePlan->constantFilter != NULL ||
                 aCodePlan->filter != NULL );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_FILT_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_FILT_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnFILT::printPredicateInfo( qcTemplate   * aTemplate,
                             qmncFILT     * aCodePlan,
                             ULong          aDepth,
                             iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *    Predicate�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnFILT::printPredicateInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;

    // Constant Filter ���
    if (aCodePlan->constantFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ CONSTANT FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->constantFilter)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }


    // Normal Filter ���
    if (aCodePlan->filter != NULL)
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
                                          aCodePlan->filter)
                 != IDE_SUCCESS);
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
