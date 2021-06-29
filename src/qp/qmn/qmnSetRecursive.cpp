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
 * $Id: qmnSetRecursive.cpp 88511 2020-09-08 07:02:11Z donovan.seo $
 *
 * Description :
 *     SREC Node
 * 
 *     Left Child�� ���� Data�� Right Child�� ���� Data��
 *     ��� �����Ѵ�.
 *     Left Child�� Right Child�� VMTR�� ���� SWAP�ϸ鼭 �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qcuProperty.h>
#include <qmnSetRecursive.h>
#include <qmnViewMaterialize.h>
#include <qmnViewScan.h>

IDE_RC qmnSREC::init( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SREC ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSREC * sCodePlan = (qmncSREC *) aPlan;
    qmndSREC * sDataPlan =
        (qmndSREC *) (aTemplate->tmplate.data + aPlan->offset);
    
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    
    //------------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------------
    
    // first initialization
    if ( ( *sDataPlan->flag & QMND_SREC_INIT_DONE_MASK )
         == QMND_SREC_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(sCodePlan, sDataPlan)
                  != IDE_SUCCESS );
    }
    else
    {
        /* BUG-48116 recurisive with fatal */
        if ( ( *sDataPlan->flag & QMND_SREC_RIGHT_INIT_DONE_MASK )
             == QMND_SREC_RIGHT_INIT_DONE_TRUE )
        {
            // BUG-44070 �ݺ� ���������� USE_TOW_PASS_HASH ��
            // top ������ USE_FULL_NL �� �����ʿ� recursive view�� ���� ���
            // hash temp table�� �籸�� �ϵ��� touch �Ѵ�.
            IDE_TEST( qmnVSCN::touchDependency( aTemplate,
                                                sCodePlan->recursiveChild )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }                

    // init level
    sDataPlan->recursionLevel = 0;

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnSREC::doItLeftFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::doIt( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SREC�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    qmndSREC * sDataPlan =
        (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::padNull( qcTemplate * /* aTemplate */,
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

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

IDE_RC qmnSREC::printPlan( qcTemplate   * aTemplate,
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

    qmncSREC * sCodePlan = (qmncSREC*) aPlan;
    //qmndSREC * sDataPlan =
    //    (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    ULong      i;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }    

    iduVarStringAppendFormat( aString,
                              "RECURSIVE UNION ALL\n" );
        
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
}

IDE_RC qmnSREC::doItDefault( qcTemplate * /* aTemplate */,
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

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

IDE_RC qmnSREC::firstInit( qmncSREC   * aCodePlan,
                           qmndSREC   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    left Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->plan.left->type == QMN_VMTR );
    IDE_DASSERT( aCodePlan->recursiveChild->type == QMN_VSCN );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_SREC_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SREC_INIT_DONE_TRUE;

    *aDataPlan->flag &= ~QMND_SREC_RIGHT_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SREC_RIGHT_INIT_DONE_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qmnSREC::doItLeftFirst( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    left subquery ����� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSREC   * sCodePlan = (qmncSREC*) aPlan;
    qmndSREC   * sDataPlan =
        (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    void       * sIndexHandle;
    void       * sTableHandle;
    qmdMtrNode * sSortRecord;

    //---------------------------------
    // BUG-43917
    // recursive view�� join �����ʿ� ���� ���
    // left, right VMTR�� ������� ���� �ʱ�ȭ �ؾ��Ѵ�.
    //---------------------------------

    IDE_TEST( qmnVMTR::resetDependency( aTemplate,
                                        sCodePlan->plan.left )
              != IDE_SUCCESS );

    IDE_TEST( qmnVMTR::resetDependency( aTemplate,
                                        sCodePlan->plan.right )
              != IDE_SUCCESS );
    
    //---------------------------------
    // left�� Child ����
    //---------------------------------

    IDE_TEST( sCodePlan->plan.left->init( aTemplate,
                                          sCodePlan->plan.left )
              != IDE_SUCCESS );
    
    //---------------------------------
    // Temp Table ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( qmnVMTR::getCursorInfo( aTemplate,
                                      sCodePlan->plan.left,
                                      & sTableHandle,
                                      & sIndexHandle,
                                      & (sDataPlan->vmtrLeft.memSortMgr),
                                      & sSortRecord )
              != IDE_SUCCESS );
    
    // ���ռ� �˻�
    IDE_DASSERT( sDataPlan->vmtrLeft.memSortMgr != NULL );

    IDE_TEST( qmnVMTR::getMtrNode( aTemplate,
                                   sCodePlan->plan.left,
                                   &(sDataPlan->vmtrLeft.mtrNode) )
              != IDE_SUCCESS );
    
    IDE_TEST( qmcMemSort::getNumOfElements( sDataPlan->vmtrLeft.memSortMgr,
                                            &(sDataPlan->vmtrLeft.recordCnt) )
              != IDE_SUCCESS );

    sDataPlan->vmtrLeft.recordPos = 0;
    
    if ( sDataPlan->vmtrLeft.recordCnt == 0 )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        sDataPlan->recursionLevel++;
        IDE_TEST_RAISE( sDataPlan->recursionLevel > QCU_RECURSION_LEVEL_MAXIMUM,
                        ERR_RECURSION_LEVEL_MAXIMUM );
        
        sDataPlan->doIt = qmnSREC::doItLeftNext;

        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_RECURSION_LEVEL_MAXIMUM )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_RECURSION_LEVEL_MAXIMUM_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::doItLeftNext( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    left subquery ����� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    //qmncSREC * sCodePlan = (qmncSREC*) aPlan;
    qmndSREC * sDataPlan =
        (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    void     * sSearchRow;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    if ( sDataPlan->vmtrLeft.recordPos < sDataPlan->vmtrLeft.recordCnt )
    {
        IDE_TEST( qmcMemSort::getElement( sDataPlan->vmtrLeft.memSortMgr,
                                          sDataPlan->vmtrLeft.recordPos,
                                          & sSearchRow )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sSearchRow == NULL, ERR_NULL_ROW );

        IDE_TEST( setStackValue( aTemplate,
                                 sDataPlan->vmtrLeft.mtrNode,
                                 sSearchRow )
                  != IDE_SUCCESS );
        
        sDataPlan->vmtrLeft.recordPos++;
        
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        sDataPlan->doIt = qmnSREC::doItRightFirst;
        
        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_NULL_ROW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSREC::doItLeftNext",
                                  "row is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::doItRightFirst( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    right subquery ����� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSREC   * sCodePlan = (qmncSREC*) aPlan;
    qmndSREC   * sDataPlan =
        (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    void       * sIndexHandle;
    void       * sTableHandle;
    qmdMtrNode * sSortRecord;
    
    //---------------------------------
    // right�� Child ����
    //---------------------------------
        
    IDE_TEST( sCodePlan->plan.right->init( aTemplate,
                                           sCodePlan->plan.right )
              != IDE_SUCCESS );
    
    /* BUG-48116 recurisive with fatal */
    *sDataPlan->flag &= ~QMND_SREC_RIGHT_INIT_DONE_MASK;
    *sDataPlan->flag |= QMND_SREC_RIGHT_INIT_DONE_TRUE;

    //---------------------------------
    // Temp Table ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( qmnVMTR::getCursorInfo( aTemplate,
                                      sCodePlan->plan.right,
                                      & sTableHandle,
                                      & sIndexHandle,
                                      & (sDataPlan->vmtrRight.memSortMgr),
                                      & sSortRecord )
              != IDE_SUCCESS );
    
    // ���ռ� �˻�
    IDE_DASSERT( sDataPlan->vmtrRight.memSortMgr != NULL );

    IDE_TEST( qmnVMTR::getMtrNode( aTemplate,
                                   sCodePlan->plan.right,
                                   &(sDataPlan->vmtrRight.mtrNode) )
              != IDE_SUCCESS );
    
    IDE_TEST( qmcMemSort::getNumOfElements( sDataPlan->vmtrRight.memSortMgr,
                                            &(sDataPlan->vmtrRight.recordCnt) )
              != IDE_SUCCESS );

    sDataPlan->vmtrRight.recordPos = 0;
    
    if ( sDataPlan->vmtrRight.recordCnt == 0 )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        sDataPlan->recursionLevel++;
        IDE_TEST_RAISE( sDataPlan->recursionLevel > QCU_RECURSION_LEVEL_MAXIMUM,
                        ERR_RECURSION_LEVEL_MAXIMUM );

        sDataPlan->doIt = qmnSREC::doItRightNext;

        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_RECURSION_LEVEL_MAXIMUM )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_RECURSION_LEVEL_MAXIMUM_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::doItRightNext( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    right subquery ����� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSREC       * sCodePlan = (qmncSREC*) aPlan;
    qmndSREC       * sDataPlan =
        (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    void           * sSearchRow;
    qmcdMemSortTemp  sSortTemp;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    if ( sDataPlan->vmtrRight.recordPos < sDataPlan->vmtrRight.recordCnt )
    {
        IDE_TEST( qmcMemSort::getElement( sDataPlan->vmtrRight.memSortMgr,
                                          sDataPlan->vmtrRight.recordPos,
                                          & sSearchRow )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sSearchRow == NULL, ERR_NULL_ROW );

        IDE_TEST( setStackValue( aTemplate,
                                 sDataPlan->vmtrRight.mtrNode,
                                 sSearchRow )
                  != IDE_SUCCESS );
        
        sDataPlan->vmtrRight.recordPos++;
        
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        // left, right Temp Table swap
        QMN_SWAP_SORT_TEMP( sDataPlan->vmtrLeft.memSortMgr,
                            sDataPlan->vmtrRight.memSortMgr );
        
        // ������� ���� �ʱ�ȭ
        IDE_TEST( qmnVMTR::resetDependency( aTemplate,
                                            sCodePlan->plan.right )
                  != IDE_SUCCESS );
        IDE_TEST( qmnVSCN::touchDependency( aTemplate,
                                            sCodePlan->recursiveChild )
                  != IDE_SUCCESS );
        
        sDataPlan->doIt = qmnSREC::doItRightFirst;
        
        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_NULL_ROW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSREC::doItRightNext",
                                  "row is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSREC::setStackValue( qcTemplate * aTemplate,
                               qmdMtrNode * aNode,
                               void       * aSearchRow )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *    ���� VIEW�� ���� stack�� �׾� �ø���.
 *    ���� ����ȭ�� ���� VMTR�� mtrNode�� tuple�� �������� �ʰ�
 *    value pointer�� ��� stack�� �״´�.
 *
 ***********************************************************************/

    //qmncSREC    * sCodePlan = (qmncSREC*) aPlan;
    //qmndSREC    * sDataPlan =
    //    (qmndSREC*) (aTemplate->tmplate.data + aPlan->offset);
    qmdMtrNode  * sNode;
    UInt          sTempTypeOffset;
    mtcStack    * sStack;
    SInt          sRemain;
    void        * sValue;

    sStack  = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;
    
    for ( sNode = aNode; sNode != NULL; sNode = sNode->next, sStack++, sRemain-- )
    {
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );

        if ( ( sNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sTempTypeOffset = *(UChar*)((UChar*)aSearchRow + sNode->dstColumn->column.offset);
        }
        else if ( ( sNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sTempTypeOffset = *(UShort*)((UChar*)aSearchRow + sNode->dstColumn->column.offset);
        }
        else if ( ( sNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sTempTypeOffset = *(UInt*)((UChar*)aSearchRow + sNode->dstColumn->column.offset);
        }
        else
        {
            sTempTypeOffset = 0;
        }
        
        if ( sTempTypeOffset > 0 )
        {
            sValue = (void*)((SChar*)aSearchRow + sTempTypeOffset);
        }
        else
        {
            sValue = (void*) mtc::value( sNode->dstColumn,
                                         aSearchRow,
                                         MTD_OFFSET_USE );
        }
        
        sStack->column = sNode->dstColumn;
        sStack->value  = sValue;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_STACK_OVERFLOW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
