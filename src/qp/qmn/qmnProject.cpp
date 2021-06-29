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
 * $Id: qmnProject.cpp 90609 2021-04-15 06:36:26Z ahra.cho $
 *
 * Description :
 *     PROJ(PROJection) Node
 *
 *     ������ �𵨿��� projection�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmx.h>
#include <qmnProject.h>
#include <qcg.h>
#include <qmcThr.h>
#include <mtd.h>
#include <qmnViewMaterialize.h>
#include <qsxEnv.h>

IDE_RC
qmnPROJ::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    PROJ ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::init"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnPROJ::doItDefault;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( ( *sDataPlan->flag & QMND_PROJ_INIT_DONE_MASK )
         == QMND_PROJ_INIT_DONE_FALSE )
    {
        // ���� �ʱ�ȭ ����
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // To Fix PR-8836
    // ���� ��忡�� LEVEL Column�� ������ �� �ֱ� ������
    // Child Plan�� �ʱ�ȭ�ϱ� ���� LEVEL Pseudo Column�� �ʱ�ȭ�Ͽ��� ��.
    // LEVEL Pseudo Column�� �ʱ�ȭ
    IDE_TEST( initLevel( aTemplate, sCodePlan )
              != IDE_SUCCESS );

    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    IDE_TEST( aPlan->left->init( aTemplate,
                                 aPlan->left ) != IDE_SUCCESS);

    // PROJ-2462 Result Cache
    if ( ( sCodePlan->flag & QMNC_PROJ_TOP_RESULT_CACHE_MASK )
         == QMNC_PROJ_TOP_RESULT_CACHE_TRUE )
    {
        IDE_DASSERT( sCodePlan->plan.left->type == QMN_VMTR );

        IDE_TEST( qmnPROJ::getVMTRInfo( aTemplate,
                                        sCodePlan,
                                        sDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------------
    // ���� Data �� �ʱ�ȭ
    //------------------------------------------------

    // Limit ���� ������ �ʱ�ȭ
    sDataPlan->limitCurrent = 1;

    // ���� doIt()�� ������� �ʾ����� ǥ��
    *sDataPlan->flag &= ~QMND_PROJ_FIRST_DONE_MASK;
    *sDataPlan->flag |= QMND_PROJ_FIRST_DONE_FALSE;

    //------------------------------------------------
    // ���� �Լ��� ����
    //------------------------------------------------

    IDE_TEST( setDoItFunction( sCodePlan, sDataPlan )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    PROJ �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - Child Plan�� ����
 *    - Record�� �����ϴ� ���
 *        - Sequence �� ����
 *        - ������ ���� �Լ��� ����
 *    - Record�� �������� �ʴ� ���
 *        - Indexable MIN-MAX�� ���� ó��
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    // To fix PR-3921
    if ( sDataPlan->limitCurrent == sDataPlan->limitEnd )
    {
        // �־��� Limit ���ǿ� �ٴٸ� ���
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->limitCurrent = 1;
    }
    else
    {
        // To Fix PR-6907
        // ���� ���߿� Limit�� ���� ����� ��
        // �ٽ� ����Ǵ� ����� �ʱ�ȭ�� �ٽ� �����Ͽ��� �Ѵ�.
        if( sDataPlan->limitCurrent == 1 &&
            sDataPlan->limitEnd != 0 &&
            ( *sDataPlan->flag & QMND_PROJ_FIRST_DONE_MASK )
            == QMND_PROJ_FIRST_DONE_TRUE )
        {
            IDE_TEST( qmnPROJ::init( aTemplate, aPlan ) != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        /*
         * for loop clause
         * �� ù record�� doIt �ÿ��� child�� doIt�Ѵ�.
         * ��������� loop clause �� ���� �����Ǵ� record��
         * �ռ� ����� ������ child�� �� doIt�� ���� ����� ����
         * projection�� ������ �����ϴ� ���°� �ȴ�.
         */
        if ( sDataPlan->loopCount > 0 )
        {
            if ( sDataPlan->loopCurrent == 0 )
            {
                // PROJ-2462 Result Cache
                if ( ( sCodePlan->flag & QMNC_PROJ_TOP_RESULT_CACHE_MASK )
                     == QMNC_PROJ_TOP_RESULT_CACHE_FALSE )
                {
                    // doIt left child
                    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                              != IDE_SUCCESS );
                }
                else
                {
                    // PROJ-2462 ResultCache
                    // Top Result Cache�� ���Ȱ�� VMTR�� ����
                    // record �� �ϳ� �����´�.
                    IDE_TEST( doItVMTR( aTemplate, aPlan, aFlag )
                                        != IDE_SUCCESS );
                }
            }
            else
            {
                *aFlag = QMC_ROW_DATA_EXIST;
            }
        }
        else
        {
            *aFlag = QMC_ROW_DATA_NONE;
        }
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        //-----------------------------------
        // Child�� ����� �����ϴ� ���
        //-----------------------------------

        IDE_TEST( readSequence( aTemplate,
                                sCodePlan,
                                sDataPlan ) != IDE_SUCCESS );

        // loop_level ����
        setLoopCurrent( aTemplate, aPlan );
        
        // �ѹ��� ����Ǿ����� ǥ��
        *sDataPlan->flag &= ~QMND_PROJ_FIRST_DONE_MASK;
        *sDataPlan->flag |= QMND_PROJ_FIRST_DONE_TRUE;

        // PROJ�� ������ �Լ��� ������.
        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------
        // Child�� ����� �������� �ʴ� ���
        //-----------------------------------

        if ( ( ( sCodePlan->flag & QMNC_PROJ_MINMAX_MASK )
               == QMNC_PROJ_MINMAX_TRUE ) &&
             ( (*sDataPlan->flag & QMND_PROJ_FIRST_DONE_MASK )
               == QMND_PROJ_FIRST_DONE_FALSE ) )
        {
            // Indexable MIN-MAX ����ȭ�� ����ǰ�,
            // Child�κ��� ��� ����� ���� ���ߴٸ�,
            // NULL���� �־�� �Ѵ�.

            // �ѹ��� ����Ǿ����� ǥ��
            *sDataPlan->flag &= ~QMND_PROJ_FIRST_DONE_MASK;
            *sDataPlan->flag |= QMND_PROJ_FIRST_DONE_TRUE;

            // Child�� ��� Null Padding��Ŵ
            IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
                      != IDE_SUCCESS );

            // PROJ�� ������ �Լ��� ������.
            IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
                      != IDE_SUCCESS );

            // Data�� �������� ǥ��
            *aFlag &= ~QMC_ROW_DATA_MASK;
            *aFlag |= QMC_ROW_DATA_EXIST;
        }
        else
        {
            // nothing to do
        }

        // fix PR-2367
        // sDataPlan->doIt = qmnPROJ::doItDefault;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    PROJ �� �ش��ϴ� Null Row�� ȹ���Ѵ�.
 *    �Ϲ����� ���, Child Plan�� ���� Null Padding���� ���������,
 *    Outer Column Reference�� �����ϴ� ��쿡�� �̿� ���� Null Padding��
 *    ������ ������ �־�� �Ѵ�.
 *
 * Implementation :
 *    Child Plan�� ���� Null Padding ���� �̹� ������ Null Row��
 *    Stack�� �����Ͽ� �ش�.
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::padNull"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    UInt       i;
    mtcStack * sStack;
    SInt       sRemain;

    // ���ռ� �˻�
    IDE_ASSERT( (sCodePlan->flag & QMNC_PROJ_TOP_MASK)
                == QMNC_PROJ_TOP_FALSE );

    // �ʱ�ȭ�� ������� ���� ��� �ʱ�ȭ�� ����
    if ( ( aTemplate->planFlag[sCodePlan->planID] & QMND_PROJ_INIT_DONE_MASK )
         == QMND_PROJ_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //fix BUG-17872
    if( sDataPlan->nullRow == NULL )
    {
        // Row�� �ִ� Size ���
        IDE_TEST( getMaxRowSize( aTemplate,
                                 sCodePlan,
                                 &sDataPlan->rowSize ) != IDE_SUCCESS );

        // Null Padding�� ���� Null Row ����
        IDE_TEST( makeNullRow( aTemplate, sCodePlan, sDataPlan )
                  != IDE_SUCCESS );
    }

    // fix BUG-9283
    // ���� ���鿡 ���� null padding ����
    // keyRange�� ������ ���, stack�� ���� ���� �ƴ�
    // value node ������ �����ϰ� �ǹǷ�.
    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    // �̹� ������ Null Row�� Stack�� �����Ѵ�.
    for ( sStack  = aTemplate->tmplate.stack,
              sRemain = aTemplate->tmplate.stackRemain,
              i = 0;
          i < sCodePlan->targetCount;
          sStack++, sRemain--, i++ )
    {
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );

        sStack->column = & sDataPlan->nullColumn[i];
        sStack->value = (void *)mtc::value( & sDataPlan->nullColumn[i],
                                            sDataPlan->nullRow,
                                            MTD_OFFSET_USE );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    PROJ ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::printPlan"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;
    UInt  j;

    qmcAttrDesc * sItrAttr;

    //------------------------------------------------------
    // ���� ������ ���
    //------------------------------------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //------------------------------------------------------
    // PROJ Target ������ ���
    //------------------------------------------------------

    IDE_TEST( printTargetInfo( aTemplate,
                               sCodePlan,
                               aString ) != IDE_SUCCESS );

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    //----------------------------
    // PROJ-1473 target info 
    //----------------------------

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }

        iduVarStringAppend( aString,
                            "[ TARGET INFO ]\n" );

        for( sItrAttr = sCodePlan->plan.resultDesc, j = 0;
             sItrAttr != NULL;
             sItrAttr = sItrAttr->next, j++ )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }

            if( sItrAttr->expr->node.arguments == NULL )
            {
                iduVarStringAppendFormat(
                    aString,
                    "sTargetColumn[%"ID_UINT32_FMT"] : [%"ID_INT32_FMT", %"ID_INT32_FMT"],"
                    "sTargetColumn->arg[X, X]\n",
                    j, 
                    (SInt)sItrAttr->expr->node.table,
                    (SInt)sItrAttr->expr->node.column
                    );
            }
            else
            {
                iduVarStringAppendFormat(
                    aString,
                    "sTargetColumn[%"ID_UINT32_FMT"] : [%"ID_INT32_FMT", %"ID_INT32_FMT"],"
                    "sTargetColumn->arg[%"ID_UINT32_FMT"] : [%"ID_INT32_FMT", %"ID_INT32_FMT"]\n",
                    j, 
                    (SInt)sItrAttr->expr->node.table,
                    (SInt)sItrAttr->expr->node.column,
                    j,
                    (SInt)sItrAttr->expr->node.arguments->table,
                    (SInt)sItrAttr->expr->node.arguments->column
                    );
            }
        }
    }    
    
    //------------------------------------------------------
    // Target ������ Subquery ���� ���
    //------------------------------------------------------

    for (sItrAttr = sCodePlan->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next)
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sItrAttr->expr,
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

    //------------------------------------------------------
    // Child Plan ������ ���
    //------------------------------------------------------

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
qmnPROJ::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnPROJ::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::doItDefault"));

    IDE_DASSERT(0);

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::doItProject( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    PROJ �� non-top projection�� �� ���� �Լ��� ����ȴ�.
 *
 * Implementation :
 *    Target�� ��� �����ϸ鼭 Stack�� �� ������ �����ǵ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::doItProject"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::doItProject"));

    qmncPROJ    * sCodePlan = (qmncPROJ*) aPlan;
    mtcStack    * sStack;
    SInt          sRemain;
    qmcAttrDesc * sItrAttr;
    qtcNode     * sNode;

    // Stack ���� ����
    sStack  = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    for ( sItrAttr = sCodePlan->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next,
              aTemplate->tmplate.stack++,
              aTemplate->tmplate.stackRemain-- )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) != QMC_ATTR_USELESS_RESULT_TRUE )
        {
            // Column�̰� Expression�̰� ���� �Լ� ȣ���� ����
            // Stack�� Column������ Value������ �����ϰ� �ȴ�.
            IDE_TEST( qtc::calculate( sItrAttr->expr, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

            /* PROJ-2469 Optimize View Materialization
             * ������� �ʴ� Column�� ���ؼ��� calculate ���� �ʴ´�.
             * Subquery�� ���� Indirect Node�� ���, Argument�� ��ȸ�Ͽ�
             * ���� Column�� �ִ� Node�� ã�´�.
             */
            for ( sNode = sItrAttr->expr;
                  ( sNode->node.lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
                  sNode = ( qtcNode* )sNode->node.arguments )
            {
                // Nothing to do.
            }

            // stack�� column�� �ش� column���� ���� �� �ش�.
            aTemplate->tmplate.stack->column = QTC_TMPL_COLUMN( aTemplate, sNode );

            // stack�� value�� �ش� column�� staticNull�� ���� ���ش�.
            if ( aTemplate->tmplate.stack->column->module->staticNull == NULL )
            {
                // list type��� ���� staticNull�� ���ǵ��� ���� type�� ��� Calculate �Ѵ�. */
                IDE_TEST( qtc::calculate( sItrAttr->expr, aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                aTemplate->tmplate.stack->value =
                    aTemplate->tmplate.stack->column->module->staticNull;
            }
        }
    }
    
    // Stack���� ����
    aTemplate->tmplate.stack       = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    aTemplate->tmplate.stack       = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::doItTopProject( qcTemplate * aTemplate,
                         qmnPlan    * aPlan,
                         qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    PROJ �� top projection�� �� ���� �Լ��� ����ȴ�.
 *
 * Implementation :
 *    Target�� ��� �����ϸ鼭 �� ���� ũ�⸦ ��� ��� ���۸�
 *    �ִ��� �����ϵ��� �Ѵ�.  ���� ���, Variable Column��
 *    ���� ����Ÿ�� ���� ũ�⸸ŭ�� �̿����� ����Ѵ�.
 *    Top Projection�� �ֻ��� qtcNode�� ��� Assign����̸�,
 *    �ش� Node ����� �ڿ������� ��� ���ۿ� ��ϵȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::doItTopProject"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::doItTopProject"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    mtcColumn   * sColumn;
    SChar       * sValue;
    mtcStack    * sStack;
    SInt          sRemain;
    qmcAttrDesc * sItrAttr;

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    SLong       sLobSize = 0;
    UInt        sLobCacheThreshold = 0;
    idBool      sIsNullLob;

    sLobCacheThreshold = QCG_GET_LOB_CACHE_THRESHOLD( aTemplate->stmt );

    sDataPlan->tupleOffset = 0;

    // Stack ���� ����
    sStack  = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    // �� Target�� ���� Projection ����
    for ( sItrAttr = sCodePlan->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next,
          aTemplate->tmplate.stack++,
          aTemplate->tmplate.stackRemain-- )
    {
        //fix BUG-17713
        //sNode�� ���̻� Assign Node�� �ƴϴ�.
        //Assign Node���� ������ �ڵ�� ��� �����Ѵ�.

        // Node�� �����Ѵ�.
        IDE_TEST( qtc::calculate( sItrAttr->expr, aTemplate )
                  != IDE_SUCCESS );

        //fix BUG-17713
        // sNode�� PASS ��� �̸� sNode->dstColumn->module->actualSize�� NULL
        // �̴�. ���� ���� stack�� ����� column, value�� ���� ����Ѵ�.
        sColumn = aTemplate->tmplate.stack->column;
        sValue = (SChar *)aTemplate->tmplate.stack->value;

        // ���� �������� ���̸�ŭ�� offset�� ����Ѵ�.
        sDataPlan->tupleOffset +=
            sColumn->module->actualSize( sColumn,
                                         sValue );

        /* PROJ-2160
           tupleOffset�� row�� ����� �ǹ��Ѵ�.
           LOB_LOCATOR_ID �� ��� size(4) + locator(8) �� ���۵ǹǷ�
           4��ŭ �����־�� �Ѵ�. */
        if ( (sColumn->module->id == MTD_BLOB_LOCATOR_ID) ||
             (sColumn->module->id == MTD_CLOB_LOCATOR_ID) )
        {
            /* 
             * PROJ-2047 Strengthening LOB - LOBCACHE
             * 
             * LOBSize(8)�� HasData(1)�� �����־�� �Ѵ�.
             */
            sDataPlan->tupleOffset += ID_SIZEOF(ULong);
            sDataPlan->tupleOffset += ID_SIZEOF(UChar);

            if (sLobCacheThreshold > 0)
            {
                sLobSize = 0;
                if ( *(smLobLocator*)sValue != MTD_LOCATOR_NULL )
                {
                    (void)smiLob::getLength( QC_STATISTICS(aTemplate->stmt),
                                             *(smLobLocator*)sValue,
                                             &sLobSize,
                                             &sIsNullLob );

                    /* �Ӱ�ġ���� �ش��ϸ� LOBData ����� ���� �ش� */
                    if (sLobSize <= sLobCacheThreshold)
                    {
                        sDataPlan->tupleOffset += sLobSize;
                    }
                    else
                    {
                        /* Nothing */
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* Nothing */
            }
        }
        else
        {
            // nothing todo
        }
    }

    // Stack���� ����
    aTemplate->tmplate.stack       = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // fix BUG-33660
    aTemplate->tmplate.stack       = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::doItWithLimit( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag)
{
/***********************************************************************
 *
 * Description :
 *    Limit�� �Բ� ���� �� ȣ��Ǵ� �Լ��̴�.
 *    doItProject(), doItTopProject()�� �Բ� ���Ǹ�,
 *    Limitation�� ���� ó���� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::doItWithLimit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::doItWithLimit"));

    // qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    for ( ;
          sDataPlan->limitCurrent < sDataPlan->limitStart;
          sDataPlan->limitCurrent++ )
    {
        // Limitation ������ ���� �ʴ´�.
        // ���� Projection���� Child�� �����ϱ⸸ �Ѵ�.
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );

        if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
        {
            break;
        }
    }

    if ( sDataPlan->limitCurrent >= sDataPlan->limitStart &&
         sDataPlan->limitCurrent < sDataPlan->limitEnd )
    {
        // Limitation ���� �ȿ� �ִ� ���
        // Projection�� �����Ѵ�.
        // doItProject() �Ǵ� doItTopProject() �Լ��� ����Ǿ� �ִ�.
        IDE_TEST( sDataPlan->limitExec( aTemplate,
                                        aPlan,
                                        aFlag ) != IDE_SUCCESS );

        // Limit�� ����
        sDataPlan->limitCurrent++;
    }
    else
    {
        // Limitation ������ ��� ���
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::getRowSize( qcTemplate * aTemplate,
                     qmnPlan    * aPlan,
                     UInt       * aSize )
{
/***********************************************************************
 *
 * Description :
 *    Target�� �����ϴ� Column���� �ִ� Size�� ȹ���Ѵ�.
 *    Communication Buffer�� ��������� Ȯ���ϱ� ����
 *    ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::getRowSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::getRowSize"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    // qmndPROJ * sDataPlan =
    //     (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    // To Fix PR-7988
    // Prepare Protocol�� ��� ::init()���� �ش� �Լ���
    // ȣ���ϰ� �ȴ�.  ����, �׻� ����� �־�� �Ѵ�.
    IDE_TEST( getMaxRowSize ( aTemplate, sCodePlan, aSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnPROJ::getCodeTargetPtr( qmnPlan    * aPlan,
                           qmsTarget ** aTarget)
{
/***********************************************************************
 *
 * Description :
 *    Top Projection�� ��쿡�� ���Ǹ�,
 *    MM �ܿ���
 *    Client�� Target Column������ �����ϱ� ���Ͽ� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::getCodeTargetPtr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::getCodeTargetPtr"));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    // qmndPROJ * sDataPlan =
    //     (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    *aTarget = sCodePlan->myTarget;

    return IDE_SUCCESS;

#undef IDE_FN
}

ULong
qmnPROJ::getActualSize( qcTemplate * aTemplate,
                        qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    �ϳ��� Target Row�Ϸ� �� ���� ó���� ũ�⸦ ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::getActualSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::getActualSize"));

    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->tupleOffset;

#undef IDE_FN
}

IDE_RC
qmnPROJ::firstInit( qcTemplate * aTemplate,
                    qmncPROJ   * aCodePlan,
                    qmndPROJ   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    PROJ node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *    - Data ������ �ֿ� ����� ���� �ʱ�ȭ�� ����
 *
 ***********************************************************************/

    ULong sCount;
    UInt  sReserveCnt = 0;

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    IDE_ASSERT( aCodePlan->myTarget != NULL );
    IDE_ASSERT( aCodePlan->myTargetOffset > 0 );

    //--------------------------------
    // PROJ ���� ������ �ʱ�ȭ
    //--------------------------------

    // Tuple Set������ �ʱ�ȭ
    // Top Projection�� ��쿡�� ��ȿ�ϸ�,
    // plan.myTuple�� ��� ���۳��� �޸� ������ ����ϰ� �ȴ�.
    // ===>
    // PROJ-1461
    // cm������ ��� ���۳��� �޸� ������ ���� ����� �� �����Ƿ�,
    // plan.myTuple�� fetch�� ����Ÿ�� ������ �޸� ������ �Ҵ� �޴´�.
    // mm���� qci::fetchColumn�� �ϳ��� ���ڵ� ��ü�� ���ؼ�
    // ����Ÿ�� fetch �ϴ� ���� �ƴ϶�,
    // �ϳ��� �÷��� ���ؼ� fetch�ϱ⶧����,
    // PROJ ��忡�� ���ڵ� ������ ����Ÿ�� �����ϰ� �־�� �Ѵ�.
    aDataPlan->plan.myTuple = & aTemplate->tmplate.
        rows[aCodePlan->myTarget->targetColumn->node.table];

    //--------------------------------
    // Limitation ���� ������ �ʱ�ȭ
    //--------------------------------
    if( aCodePlan->limit != NULL )
    {
        IDE_TEST( qmsLimitI::getStartValue(
                      aTemplate,
                      aCodePlan->limit,
                      &aDataPlan->limitStart )
                  != IDE_SUCCESS );

        IDE_TEST( qmsLimitI::getCountValue(
                      aTemplate,
                      aCodePlan->limit,
                      &sCount )
                  != IDE_SUCCESS );

        if ( aDataPlan->limitStart > 0 )
        {
            aDataPlan->limitEnd = aDataPlan->limitStart + sCount;
        }
        else
        {
            aDataPlan->limitEnd = 0;
        }
    }
    else
    {
        aDataPlan->limitStart = 1;
        aDataPlan->limitEnd = 0;
    }

    // ���ռ� �˻�
    if ( aDataPlan->limitEnd > 0 )
    {
        IDE_ASSERT( (aCodePlan->flag & QMNC_PROJ_LIMIT_MASK)
                    == QMNC_PROJ_LIMIT_TRUE );
    }

    //--------------------------------
    // Loop ���� ������ �ʱ�ȭ
    //--------------------------------
    if ( aCodePlan->loopNode != NULL )
    {
        IDE_TEST( qtc::getLoopCount( aTemplate,
                                     aCodePlan->loopNode,
                                     & aDataPlan->loopCount )
                  != IDE_SUCCESS );
        aDataPlan->loopCurrent = 0;

        if ( aCodePlan->loopLevel != NULL )
        {
            aDataPlan->loopCurrentPtr = (SLong*)
                aTemplate->tmplate.rows[aCodePlan->loopLevel->node.table].row;
        }
        else
        {
            aDataPlan->loopCurrentPtr = NULL;
        }
    }
    else
    {
        aDataPlan->loopCount   = 1;
        aDataPlan->loopCurrent = 0;
        aDataPlan->loopCurrentPtr = NULL;
    }
        
    //---------------------------------
    // Null Row ���� ������ �ʱ�ȭ
    //---------------------------------

    aDataPlan->nullRow = NULL;
    aDataPlan->nullColumn = NULL;

    /* BUG-48776
     * One row return unkown �迭�� scalar subquery result backup ������ �ʱ�ȭ
     */
    aDataPlan->mKeepValue = NULL;
    aDataPlan->mKeepColumn = NULL;

    /*
     * PROJ-1071 Parallel Query
     * thread reserve
     */
    if ( ((aCodePlan->plan.flag & QMN_PLAN_PRLQ_EXIST_MASK) ==
         QMN_PLAN_PRLQ_EXIST_TRUE) &&
         ((aCodePlan->flag & QMNC_PROJ_QUERYSET_TOP_MASK) ==
          QMNC_PROJ_QUERYSET_TOP_TRUE) &&
         // BUG-41279
         // Prevent parallel execution while executing 'select for update' clause.
         ((aTemplate->stmt->spxEnv->mFlag & QSX_ENV_DURING_SELECT_FOR_UPDATE) !=
          QSX_ENV_DURING_SELECT_FOR_UPDATE) )
    {
        IDE_TEST(qcg::reservePrlThr(aCodePlan->plan.mParallelDegree,
                                    &sReserveCnt)
                 != IDE_SUCCESS);

        if (sReserveCnt >= 1)
        {
            IDE_TEST(qmcThrObjCreate(aTemplate->stmt->mThrMgr,
                                     sReserveCnt)
                     != IDE_SUCCESS);
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_PROJ_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_PROJ_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sReserveCnt > 0)
    {
        (void)qcg::releasePrlThr(sReserveCnt);
    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qmnPROJ::getMaxRowSize( qcTemplate * aTemplate,
                        qmncPROJ   * aCodePlan,
                        UInt       * aSize )
{
/***********************************************************************
 *
 * Description :
 *    Target Row�� �ִ� Size�� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt          sSize;
    qtcNode     * sNode;
    mtcColumn   * sMtcColumn;
    qmcAttrDesc * sItrAttr;

    sSize = 0;

    for ( sItrAttr = aCodePlan->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        // ���� Target Column�� ȹ��
        sNode = (qtcNode*)mtf::convertedNode(&sItrAttr->expr->node,
                                             &aTemplate->tmplate );
        sMtcColumn = QTC_TMPL_COLUMN(aTemplate, sNode);

        sSize = idlOS::align(sSize, sMtcColumn->module->align);
        sSize += sMtcColumn->column.size;
    }

    // ���ռ� �˻�
    IDE_ASSERT(sSize > 0);

    *aSize = sSize;

    return IDE_SUCCESS;
}

IDE_RC
qmnPROJ::makeNullRow( qcTemplate * aTemplate,
                      qmncPROJ   * aCodePlan,
                      qmndPROJ   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    SELECT ���� ������ ����� ���� ���,
 *    Null Padding�� ȣ���� �� �ִ�.
 *    �� ��, SELECT target�� Outer Column Reference�� �����Ѵٸ�,
 *    �̿� ���� Null Value�� PROJ ��忡�� �����Ͽ��� �Ѵ�.
 *    Ex) UPDATE T1 SET i1 = ( SELECT T1.i2 FROM T2 LIMIT 1 );
 *                                    ^^^^^
 *
 * Implementation :
 *    ���� �� ���� �����Ѵ�.
 *    - Null Row�� ���� ���� Ȯ��
 *    - Null Column�� ���� ���� Ȯ��
 *    - �� Null Column�� ������ �����ϰ� Null Value����
 ***********************************************************************/

#define IDE_FN "qmnPROJ::makeNullRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnPROJ::makeNullRow"));

    UInt        i;
    ULong       sTupleOffset = 0;

    qmsTarget * sTarget;
    qtcNode   * sNode;
    mtcColumn * sColumn;

    // Null Row�� ���� ���� Ȯ��
    IDU_FIT_POINT( "qmnPROJ::makeNullRow::cralloc::nullRow",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTemplate->stmt->qmxMem->cralloc( aDataPlan->rowSize,
                                                (void**) & aDataPlan->nullRow )
              != IDE_SUCCESS);

    // Null Column�� ���� ���� Ȯ��
    IDU_FIT_POINT( "qmnPROJ::makeNullRow::alloc::nullColumn",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTemplate->stmt->qmxMem->alloc( aCodePlan->targetCount * ID_SIZEOF(mtcColumn),
                                              (void**) & aDataPlan->nullColumn )
              != IDE_SUCCESS);

    for ( sTarget = aCodePlan->myTarget, i = 0;
          sTarget != NULL;
          sTarget = sTarget->next, i++ )
    {
        //-------------------------------------------
        // �� Null Column�� ���Ͽ� Null Value�� ����
        //-------------------------------------------

        // ���� Target Column������ ȹ��
        sNode = (qtcNode*)mtf::convertedNode( & sTarget->targetColumn->node,
                                              & aTemplate->tmplate );
        sColumn = & aTemplate->tmplate.rows[sNode->node.table].
            columns[sNode->node.column];

        // Column ���� ����
        // Variable Column�� Null Value ȹ���� ���Ͽ�
        // ���� ������ NULL Column�� ��� Fixed�� �����Ѵ�.
        mtc::copyColumn( & aDataPlan->nullColumn[i],
                         sColumn );
        
        // BUG-38494
        // Compressed Column ���� �� ��ü�� ����ǹǷ�
        // Compressed �Ӽ��� �����Ѵ�
        aDataPlan->nullColumn[i].column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
        aDataPlan->nullColumn[i].column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

        // Null Column�� offset �缳��
        sTupleOffset =
            idlOS::align( sTupleOffset, sColumn->module->align );

        aDataPlan->nullColumn[i].column.offset = sTupleOffset;

        // To Fix PR-8005
        // Null Value ����
        aDataPlan->nullColumn[i].module->null(
            & aDataPlan->nullColumn[i],
            (void*) ( (SChar*) aDataPlan->nullRow + sTupleOffset) );

        // Offset����
        // fix BUG-31822
        sTupleOffset += sColumn->column.size;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnPROJ::initLevel( qcTemplate * aTemplate,
                    qmncPROJ   * aCodePlan )
{
/***********************************************************************
 *
 * Description :
 *    LEVEL pseudo column�� �� �ʱ�ȭ
 *
 * Implementation :
 *    LEVEL pseudo column�� ������ ��� �� ���� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    qtcNode  * sLevel   = aCodePlan->level;
    qtcNode  * sIsLeaf  = aCodePlan->isLeaf;

    if (sLevel != NULL)
    {
        *(mtdBigintType *)(aTemplate->tmplate.rows[sLevel->node.table].row) = 0;
    }
    else
    {
        /* Nothing to do */
    }

    if (sIsLeaf != NULL)
    {
        *(mtdBigintType *)(aTemplate->tmplate.rows[sIsLeaf->node.table].row) = 0;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

}

IDE_RC
qmnPROJ::setDoItFunction( qmncPROJ   * aCodePlan,
                          qmndPROJ   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Top Projection�� ���ο� Limit�� ���� ���ο� ����
 *    ���� �Լ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( ( aCodePlan->flag & QMNC_PROJ_TOP_RESULT_CACHE_MASK )
         == QMNC_PROJ_TOP_RESULT_CACHE_FALSE )
    {
        switch ( ( aCodePlan->flag &
                 ( QMNC_PROJ_TOP_MASK| QMNC_PROJ_LIMIT_MASK ) ) )
        {
            case (QMNC_PROJ_TOP_TRUE | QMNC_PROJ_LIMIT_TRUE):
                {
                    aDataPlan->doIt = qmnPROJ::doItWithLimit;
                    aDataPlan->limitExec = qmnPROJ::doItTopProject;
                    break;
                }
            case (QMNC_PROJ_TOP_TRUE | QMNC_PROJ_LIMIT_FALSE):
                {
                    aDataPlan->doIt = qmnPROJ::doItTopProject;
                    break;
                }
            case (QMNC_PROJ_TOP_FALSE| QMNC_PROJ_LIMIT_TRUE):
                {
                    aDataPlan->doIt = qmnPROJ::doItWithLimit;
                    aDataPlan->limitExec = qmnPROJ::doItProject;
                    break;
                }
            case (QMNC_PROJ_TOP_FALSE | QMNC_PROJ_LIMIT_FALSE):
                {
                    aDataPlan->doIt = qmnPROJ::doItProject;
                    break;
                }
            default:
                {
                    IDE_DASSERT( 0 );
                    break;
                }
        }
    }
    else
    {
        aDataPlan->doIt = qmnPROJ::doItTopProject;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmnPROJ::readSequence( qcTemplate * aTemplate,
                       qmncPROJ   * aCodePlan,
                       qmndPROJ   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    �ʿ��� ���, Sequence�� next value���� ��´�.
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qmnPROJ::readSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // SEQUENCE : Not to split doItFunctions any more
    // This is Better...
    if (aCodePlan->nextValSeqs != NULL)
    {
        if ( ( *aDataPlan->flag & QMND_PROJ_FIRST_DONE_MASK )
             == QMND_PROJ_FIRST_DONE_FALSE )
        {
            IDE_TEST(qmx::addSessionSeqCaches(aTemplate->stmt,
                                              aTemplate->stmt->myPlan->parseTree)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing To Do
        }

        IDE_TEST(qmx::readSequenceNextVals(
                     aTemplate->stmt, aCodePlan->nextValSeqs)
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

void qmnPROJ::setLoopCurrent( qcTemplate * aTemplate,
                              qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *     loop current ���� ������Ű�� loop level pseudo column ���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ * sDataPlan =
        (qmndPROJ*) (aTemplate->tmplate.data + aPlan->offset);

    if ( sCodePlan->loopNode != NULL )
    {
        sDataPlan->loopCurrent++;

        if ( sDataPlan->loopCurrentPtr != NULL )
        {
            *sDataPlan->loopCurrentPtr = sDataPlan->loopCurrent;
        }
        else
        {
            // Nothing to do.
        }
        
        if ( sDataPlan->loopCurrent >= sDataPlan->loopCount )
        {
            sDataPlan->loopCurrent = 0;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
}

IDE_RC
qmnPROJ::printTargetInfo( qcTemplate   * aTemplate,
                          qmncPROJ     * aCodePlan,
                          iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     Target ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnPROJ::printTargetInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt        sCount = 0;
    UInt        sRowSize;
    qmsTarget * sNode;

    // To Fix PR-8271
    // explain plan = only; �� ���
    // Data������ ������ ����.  ������ �ٽ� �����
    for ( sNode = aCodePlan->myTarget; sNode != NULL;
          sNode = sNode->next )
    {
        sCount++;
    }

    // Data������ ������ ���� �� ����. ������ �ٽ� �����
    IDE_TEST( getMaxRowSize( aTemplate, aCodePlan, & sRowSize )
              != IDE_SUCCESS );

    // PROJ ������ ���
    iduVarStringAppendFormat( aString,
                              "PROJECT ( COLUMN_COUNT: %"ID_UINT32_FMT", "
                              "TUPLE_SIZE: %"ID_UINT32_FMT,
                              sCount,
                              sRowSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

UInt qmnPROJ::getTargetCount( qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1075 project node�� target count�� ��´�.
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qmnPROJ::getTargetCount"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qmncPROJ * sCodePlan = (qmncPROJ*) aPlan;

    return sCodePlan->targetCount;

#undef IDE_FN
}

IDE_RC qmnPROJ::getVMTRInfo( qcTemplate * aTemplate,
                             qmncPROJ   * aCodePlan,
                             qmndPROJ   * aDataPlan )
{
    void * sTableHandle = NULL;
    void * sIndexHandle = NULL;

    IDE_TEST( qmnVMTR::getNullRowSize( aTemplate,
                                       aCodePlan->plan.left,
                                       &aDataPlan->nullRowSize )
              != IDE_SUCCESS );

    IDE_DASSERT( aDataPlan->nullRowSize > 0 );

    IDE_TEST( qmnVMTR::getNullRowMemory( aTemplate,
                                         aCodePlan->plan.left,
                                         &aDataPlan->nullRow )
              != IDE_SUCCESS );

    IDE_TEST( qmnVMTR::getCursorInfo( aTemplate,
                                      aCodePlan->plan.left,
                                      &sTableHandle,
                                      &sIndexHandle,
                                      &aDataPlan->memSortMgr,
                                      &aDataPlan->memSortRecord )
              != IDE_SUCCESS );
    IDE_TEST( qmcMemSort::getNumOfElements( aDataPlan->memSortMgr,
                                            &aDataPlan->recordCnt )
              != IDE_SUCCESS );

    aDataPlan->recordPos = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPROJ::setTupleSet( qcTemplate * aTemplate,
                             qmncPROJ   * aCodePlan,
                             qmndPROJ   * aDataPlan )
{
    qmdMtrNode * sNode       = NULL;
    mtcTuple   * sVMTRTuple  = NULL;
    mtcColumn  * sMyColumn   = NULL;
    mtcColumn  * sVMTRColumn = NULL;
    UInt         i           = 0;
    
    // PROJ-2362 memory temp ���� ȿ���� ����
    // VSCN tuple ����
    if ( aDataPlan->memSortRecord != NULL )
    {
        for ( sNode = aDataPlan->memSortRecord;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setTuple( aTemplate,
                                            sNode,
                                            aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        IDE_TEST( qmnVMTR::getTuple( aTemplate,
                                     aCodePlan->plan.left,
                                     & sVMTRTuple )
                  != IDE_SUCCESS );
        
        sMyColumn = aDataPlan->plan.myTuple->columns;
        sVMTRColumn = sVMTRTuple->columns;
        for ( i = 0; i < sVMTRTuple->columnCount; i++, sMyColumn++, sVMTRColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sMyColumn->column.flag ) == ID_TRUE )
            {
                IDE_DASSERT( sVMTRColumn->module->actualSize(
                                 sVMTRColumn,
                                 sVMTRColumn->column.value )
                             <= sVMTRColumn->column.size );
                
                idlOS::memcpy( (SChar*)sMyColumn->column.value,
                               (SChar*)sVMTRColumn->column.value,
                               sVMTRColumn->module->actualSize(
                                   sVMTRColumn,
                                   sVMTRColumn->column.value ) );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPROJ::doItVMTR( qcTemplate * aTemplate,
                          qmnPlan    * aPlan,
                          qmcRowFlag * aFlag )
{
    qmncPROJ    * sCodePlan = (qmncPROJ*) aPlan;
    qmndPROJ    * sDataPlan = ( qmndPROJ * )( aTemplate->tmplate.data + aPlan->offset );

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    if ( sDataPlan->recordPos < sDataPlan->recordCnt )
    {
        IDE_TEST( qmcMemSort::getElement( sDataPlan->memSortMgr,
                                          sDataPlan->recordPos,
                                          &sDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
        IDE_DASSERT( sDataPlan->plan.myTuple->row != NULL );

        IDE_TEST( setTupleSet( aTemplate, sCodePlan, sDataPlan )
                  != IDE_SUCCESS );

        sDataPlan->recordPos++;
        sDataPlan->plan.myTuple->modify++;

        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

