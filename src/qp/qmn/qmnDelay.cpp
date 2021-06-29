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
 * $Id$
 *
 * Description :
 *     DLAY(DeLAY) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmnDelay.h>
#include <qcg.h>

IDE_RC
qmnDLAY::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DLAY ����� �ʱ�ȭ
 *
 * Implementation :
 *    - ���� �ʱ�ȭ�� ���� ���� ��� ���� �ʱ�ȭ ����
 *    - Child Plan�� ���� �ʱ�ȭ
 *    - Constant Delay�� ���� ��� �˻�
 *    - Constant Delay�� ����� ���� ���� �Լ� ����
 *
 ***********************************************************************/

    qmncDLAY * sCodePlan = (qmncDLAY*) aPlan;
    qmndDLAY * sDataPlan =
        (qmndDLAY*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnDLAY::doIt;

    return IDE_SUCCESS;
}

IDE_RC
qmnDLAY::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    DLAY�� ���� �Լ�
 *    Child�� �����ϰ� Record�� ���� ��� ������ �˻��Ѵ�.
 *    ������ ������ ������ �̸� �ݺ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDLAY::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //qmncDLAY * sCodePlan = (qmncDLAY*) aPlan;
    qmndDLAY * sDataPlan =
        (qmndDLAY*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (*sDataPlan->flag & QMND_DLAY_INIT_DONE_MASK)
         == QMND_DLAY_INIT_DONE_FALSE )
    {
        /* Child Plan�� �ʱ�ȭ */
        IDE_TEST( aPlan->left->init( aTemplate, aPlan->left ) != IDE_SUCCESS );
        
        *sDataPlan->flag &= ~QMND_DLAY_INIT_DONE_MASK;
        *sDataPlan->flag |= QMND_DLAY_INIT_DONE_TRUE;
    }
    else
    {
        // Nothing To Do
    }
    
    // Child�� ����
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDLAY::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DLAY ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDLAY::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDLAY::padNull"));

    qmncDLAY * sCodePlan = (qmncDLAY*) aPlan;

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_DLAY_INIT_DONE_MASK)
         == QMND_DLAY_INIT_DONE_FALSE )
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
qmnDLAY::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    DLAY ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDLAY::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDLAY::printPlan"));

    qmncDLAY * sCodePlan = (qmncDLAY*) aPlan;
    qmndDLAY * sDataPlan =
        (qmndDLAY*) (aTemplate->tmplate.data + aPlan->offset);
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
    // DLAY ��� ǥ��
    //----------------------------

    iduVarStringAppend( aString,
                        "DELAY\n" );

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
