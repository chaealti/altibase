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
 * $Id: qmnCounter.cpp 20233 2007-08-06 01:58:21Z sungminee $
 *
 * Description :
 *     CNTR(CouNTeR) Node
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
#include <qmnCounter.h>
#include <qcg.h>

IDE_RC 
qmnCNTR::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    CNTR ����� �ʱ�ȭ
 *
 * Implementation :
 *    - ���� �ʱ�ȭ�� ���� ���� ��� ���� �ʱ�ȭ ����
 *    - Child Plan�� ���� �ʱ�ȭ
 *    - Stop Filter�� ���� ��� �˻�
 *    - Stop Filter�� ����� ���� ���� �Լ� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::init"));

    qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;
    qmndCNTR * sDataPlan = 
        (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;
    
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnCNTR::doItDefault;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( (*sDataPlan->flag & QMND_CNTR_INIT_DONE_MASK)
         == QMND_CNTR_INIT_DONE_FALSE )
    {
        // ���� �ʱ�ȭ ����
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //--------------------------------
    // Rownum �ʱ�ȭ
    //--------------------------------

    sDataPlan->rownumValue = 1;
    *(sDataPlan->rownumPtr) = sDataPlan->rownumValue;

    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    IDE_TEST( aPlan->left->init( aTemplate, 
                                 aPlan->left ) 
              != IDE_SUCCESS);

    //------------------------------------------------
    // Stop Filter�� ����
    //------------------------------------------------

    if ( sCodePlan->stopFilter != NULL )
    {
        IDE_TEST( qtc::judge( & sJudge,
                              sCodePlan->stopFilter,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    //------------------------------------------------
    // Stop Filter�� ���� ���� �Լ� ����
    //------------------------------------------------
    
    if ( sJudge == ID_TRUE )
    {
        //---------------------------------
        // Stop Filter�� �����ϴ� ���
        // �Ϲ� ���� �Լ��� ����
        //---------------------------------

        sDataPlan->doIt = qmnCNTR::doItFirst;
    }
    else
    {
        //-------------------------------------------
        // Stop Filter�� �������� �ʴ� ���
        // - �׻� ������ �������� �����Ƿ�
        //   ��� ����� �������� �ʴ� �Լ��� �����Ѵ�.
        //-------------------------------------------

        sDataPlan->doIt = qmnCNTR::doItAllFalse;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnCNTR::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    CNTR�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::doIt"));

    qmndCNTR * sDataPlan = 
        (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnCNTR::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    CNTR ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::padNull"));

    qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;
    // qmndCNTR * sDataPlan = 
    //     (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_CNTR_INIT_DONE_MASK)
         == QMND_CNTR_INIT_DONE_FALSE )
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
qmnCNTR::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    CNTR ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::printPlan"));

    qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;
    qmndCNTR * sDataPlan = 
        (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);
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
    // CNTR ��� ǥ��
    //----------------------------

    if (sCodePlan->stopFilter != NULL)
    {
        iduVarStringAppend( aString,
                            "COUNTER STOPKEY\n" );
    }
    else
    {
        iduVarStringAppend( aString,
                            "COUNTER\n" );
    }

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        IDE_TEST( printPredicateInfo( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
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
    //     1. Stop Filter
    //----------------------------

    // Normal Filter�� Subquery ���� ���
    if ( sCodePlan->stopFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->stopFilter,
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
qmnCNTR::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnCNTR::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::doItDefault"));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnCNTR::doItAllFalse( qcTemplate * /* aTemplate */,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Filter ������ �����ϴ� Record�� �ϳ��� ���� ��� ���
 *
 *    Stop Filter �˻��Ŀ� �����Ǵ� �Լ��� ���� �����ϴ�
 *    Record�� �������� �ʴ´�.
 *
 * Implementation :
 *    �׻� record ������ �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::doItAllFalse"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::doItAllFalse"));

    qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;

    // ���ռ� �˻�
    IDE_DASSERT( sCodePlan->stopFilter != NULL );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;
    
    return IDE_SUCCESS;
    
#undef IDE_FN
}

IDE_RC 
qmnCNTR::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    CNTR�� ���� �Լ�
 *    Child�� �����ϰ� Record�� ���� ������ �ݺ��Ѵ�.
 * 
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;
    qmndCNTR * sDataPlan = 
        (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);

    // Child�� ����
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag ) 
              != IDE_SUCCESS );
        
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnCNTR::doItNext;
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
qmnCNTR::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    CNTR�� ���� �Լ�
 *    Child�� �����ϰ� Record�� ���� ������ �ݺ��Ѵ�.
 * 
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncCNTR * sCodePlan = (qmncCNTR*) aPlan;
    qmndCNTR * sDataPlan = 
        (qmndCNTR*) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge = ID_FALSE;

    //--------------------------------
    // Rownum ����
    //--------------------------------
    
    sDataPlan->rownumValue ++;
    *(sDataPlan->rownumPtr) = sDataPlan->rownumValue;
    
    //------------------------------------------------
    // Stop Filter�� ����
    //------------------------------------------------

    if ( sCodePlan->stopFilter != NULL )
    {
        IDE_TEST( qtc::judge( & sJudge,
                              sCodePlan->stopFilter,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    //------------------------------------------------
    // Stop Filter�� ���� ���� �Լ� ����
    //------------------------------------------------
    
    if ( sJudge == ID_TRUE )
    {
        // Child�� ����
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag ) 
                  != IDE_SUCCESS );
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnCNTR::firstInit( qcTemplate * aTemplate,
                    qmncCNTR   * aCodePlan,
                    qmndCNTR   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    CNTR node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnCNTR::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnCNTR::firstInit"));

    //---------------------------------
    // CNTR ���� ������ �ʱ�ȭ
    //---------------------------------

    aDataPlan->rownumTuple =
        & aTemplate->tmplate.rows[aCodePlan->rownumRowID];

    // Rownum ��ġ ����
    aDataPlan->rownumPtr = (SLong*) aDataPlan->rownumTuple->row;
    aDataPlan->rownumValue = *(aDataPlan->rownumPtr);

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_CNTR_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_CNTR_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnCNTR::printPredicateInfo( qcTemplate   * aTemplate,
                             qmncCNTR     * aCodePlan,
                             qmndCNTR     * /* aDataPlan */,
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

#define IDE_FN "qmnCNTR::printPredicateInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    
    // Stop Filter ���
    if (aCodePlan->stopFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ STOPKEY ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->stopFilter)
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

