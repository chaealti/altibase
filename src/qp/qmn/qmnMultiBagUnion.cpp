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
 * $Id: qmnMultiBagUnion.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Multiple BUNI(Multiple Bag Union) Node
 *
 *     ������ �𵨿��� Bag Union�� �����ϴ� Plan Node �̴�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Multiple Bag Union
 *
 *     Multi Children �� ���� Data�� ��� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmnMultiBagUnion.h>

IDE_RC 
qmnMultiBUNI::init( qcTemplate * aTemplate,
                    qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    MultiBUNI ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMultiBUNI::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMultiBUNI * sCodePlan = (qmncMultiBUNI *) aPlan;
    qmndMultiBUNI * sDataPlan = 
        (qmndMultiBUNI *) (aTemplate->tmplate.data + aPlan->offset);

    qmnPlan * sChildPlan;
    
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
    
    sDataPlan->doIt = qmnMultiBUNI::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_MULTI_BUNI_INIT_DONE_MASK)
         == QMND_MULTI_BUNI_INIT_DONE_FALSE )
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

    sDataPlan->curChild = sCodePlan->plan.children;
    sChildPlan = sDataPlan->curChild->childPlan;
    
    IDE_TEST( sChildPlan->init( aTemplate, sChildPlan )
              != IDE_SUCCESS);

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnMultiBUNI::doIt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC 
qmnMultiBUNI::doIt( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    MultiBUNI�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ���� Child Plan �� �����ϰ�, ���� ��� ���� child plan�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnMultiBUNI::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndMultiBUNI * sDataPlan = 
        (qmndMultiBUNI*) (aTemplate->tmplate.data + aPlan->offset);

    qmnPlan       * sChildPlan;
    
    //--------------------------------------------------
    // Data�� ������ ������ ��� Child�� �����Ѵ�.
    //--------------------------------------------------

    while ( 1 )
    {
        //----------------------------
        // ���� Child�� ����
        //----------------------------
        
        sChildPlan = sDataPlan->curChild->childPlan;

        IDE_TEST( sChildPlan->doIt( aTemplate, sChildPlan, aFlag )
                  != IDE_SUCCESS );

        //----------------------------
        // Data ���� ���ο� ���� ó��
        //----------------------------
        
        if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            //----------------------------
            // ���� Child�� Data�� �����ϴ� ���
            //----------------------------
            break;
        }
        else
        {
            //----------------------------
            // ���� Child�� Data�� ���� ���
            //----------------------------
            
            sDataPlan->curChild = sDataPlan->curChild->next;

            if ( sDataPlan->curChild == NULL )
            {
                // ��� Child�� ������ ���
                break;
            }
            else
            {
                // ���� Child Plan �� ���� �ʱ�ȭ�� �����Ѵ�.
                sChildPlan = sDataPlan->curChild->childPlan;
                IDE_TEST( sChildPlan->init( aTemplate, sChildPlan )
                          != IDE_SUCCESS);
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnMultiBUNI::padNull( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnMultiBUNI::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMultiBUNI::printPlan( qcTemplate   * aTemplate,
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

#define IDE_FN "qmnMultiBUNI::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMultiBUNI * sCodePlan = (qmncMultiBUNI*) aPlan;
    qmndMultiBUNI * sDataPlan = 
       (qmndMultiBUNI*) (aTemplate->tmplate.data + aPlan->offset);
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
    // MultiBUNI ��� ǥ��
    //----------------------------
    
    iduVarStringAppend( aString,
                        "BAG-UNION\n" );

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
qmnMultiBUNI::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnMultiBUNI::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC 
qmnMultiBUNI::firstInit( qmndMultiBUNI   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMultiBUNI::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_MULTI_BUNI_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_MULTI_BUNI_INIT_DONE_TRUE;

    return IDE_SUCCESS;

#undef IDE_FN
}


