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
 *     Multiple INST(Multi INSerT) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmnMultiInsert.h>
#include <qmnInsert.h>

IDE_RC 
qmnMTIT::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    MTIT ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMTIT::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMTIT * sCodePlan = (qmncMTIT *) aPlan;
    qmndMTIT * sDataPlan = 
        (qmndMTIT *) (aTemplate->tmplate.data + aPlan->offset);

    qmnChildren * sChildren;
    
    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aPlan->left     == NULL );
    IDE_DASSERT( aPlan->right    == NULL );
    IDE_DASSERT( aPlan->children != NULL );

    //---------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------
    
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    
    sDataPlan->doIt = qmnMTIT::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_MTIT_INIT_DONE_MASK)
         == QMND_MTIT_INIT_DONE_FALSE )
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

    for ( sChildren = sCodePlan->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        // BUG-45288
        ((qmndINST*) (aTemplate->tmplate.data +
                      sChildren->childPlan->offset))->isAppend =
            ((qmncINST*)sChildren->childPlan)->isAppend;    

        IDE_TEST( sChildren->childPlan->init( aTemplate,
                                              sChildren->childPlan )
                  != IDE_SUCCESS );
    }

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnMTIT::doIt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnMTIT::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    MTIT�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMTIT::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMTIT    * sCodePlan = (qmncMTIT *) aPlan;
    qmnChildren * sChildren;
    qmcRowFlag    sFlag = QMC_ROW_INITIALIZE;
    
    //--------------------------------------------------
    // ��� Child�� �����Ѵ�.
    //--------------------------------------------------

    sChildren = sCodePlan->plan.children;

    // ù��° ����� �ѱ��.
    IDE_TEST( sChildren->childPlan->doIt( aTemplate,
                                          sChildren->childPlan,
                                          aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        for ( sChildren = sChildren->next;
              sChildren != NULL;
              sChildren = sChildren->next )
        {
            IDE_TEST( sChildren->childPlan->doIt( aTemplate,
                                                  sChildren->childPlan,
                                                  &sFlag )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnMTIT::padNull( qcTemplate * /* aTemplate */,
                  qmnPlan    * /* aPlan */)
{
/***********************************************************************
 *
 * Description :
 *    ȣ��Ǿ�� �ȵ�.
 *    ���� Node�� �ݵ�� VIEW����̸�,
 *    View�� �ڽ��� Null Row���� �����ϱ� �����̴�.
 *    
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMTIT::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMTIT::printPlan( qcTemplate   * aTemplate,
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

#define IDE_FN "qmnMTIT::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMTIT * sCodePlan = (qmncMTIT*) aPlan;
    qmndMTIT * sDataPlan = 
        (qmndMTIT*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;

    qmnChildren * sChildren;

    //----------------------------
    // Display ��ġ ����
    //----------------------------
    
    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // MTIT ��� ǥ��
    //----------------------------
    
    iduVarStringAppend( aString,
                        "MULTIPLE-INSERT\n" );

    //----------------------------
    // Child Plan�� ���� ���
    //----------------------------

    for ( sChildren = sCodePlan->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        IDE_TEST( sChildren->childPlan->printPlan( aTemplate,
                                                   sChildren->childPlan,
                                                   aDepth + 1,
                                                   aString,
                                                   aMode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC 
qmnMTIT::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    ȣ��Ǿ�� �ȵ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMTIT::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnMTIT::firstInit( qmndMTIT   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMTIT::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_MTIT_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_MTIT_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}
