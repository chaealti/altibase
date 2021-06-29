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
 * $Id: qtcSubqueryWrapper.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Subquery Wrapper Node
 *        IN Subquery�� Key Range ������ ���� Node�� Node Transform��
 *        ���� Key Range �����ÿ��� ���Ǹ�, Subquery�� ��� ����ϸ�
 *        Subquery Target�� ù��° Column�� ���ؼ��� �����ȴ�..
 * 
 *        - Ex) i1 IN ( SELECT a1 FROM T1 );
 *
 *          [=]
 *           |
 *           V
 *          [Indirect]-------->[Subquery Wrapper]
 *           |                  |
 *           V                  V
 *          [i1]               [Subquery]
 *                              |
 *                              V
 *                             [a1]
 *
 *     ���� �׸������� ���� IN�� Key Range�� ���� [=] �����ڷ� ��ü�Ǹ�,
 *     [Subquery Wrapper] �� [Subquery]�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qtc.h>
#include <qmn.h>

extern mtdModule mtdList;

//-----------------------------------------
// Subquery Wrapper �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 17, (void*)"SUBQUERY_WRAPPER" }
};

//-----------------------------------------
// Subquery Wrapper �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qtc::subqueryWrapperModule = {
    1|                      // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC| // ��Ÿ ������
    MTC_NODE_INDIRECT_TRUE, // Indirection��
    ~0,                     // Indexable Mask : �ǹ� ����
    1.0,                    // default selectivity (�� ������ �ƴ�)
    qtcNames,               // �̸� ����
    NULL,                   // Counter ������ ���� 
    mtf::initializeDefault, // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,   // ���� ����� ���� �Լ�, ���� 
    qtcEstimate             // Estimate �� �Լ�
};

//-----------------------------------------
// Subquery Wrapper �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_SubqueryWrapper( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,             // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,             // Aggregation ���� �Լ�, ����
    mtf::calculateNA, 
    mtf::calculateNA,             // Aggregation ���� �Լ�, ����
    qtcCalculate_SubqueryWrapper, // SUBQUERY WRAPPER ���� �Լ�
    NULL,                         // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA,         // Key Range ũ�� ���� �Լ�, ���� 
    mtk::extractRangeNA           // Key Range ���� �Լ�, ����
};


IDE_RC qtcEstimate( mtcNode * aNode,
                    mtcTemplate* aTemplate,
                    mtcStack * aStack,
                    SInt       /* aRemain */,
                    mtcCallBack * aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Subquery Wrapper �����ڿ� ���Ͽ� Estimate �� ������.
 *    Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    Subquery Wrapper ���� ������ Column ������ �ʿ�����Ƿ�,
 *    Skip Module�� estimation�� �ϸ�, ���� Node������ estimate ��
 *    ���Ͽ� ���� Node�� ������ Stack�� �����Ͽ� �ش�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcNode   * sNode;
    mtcColumn * sColumn;
    mtcTemplate * sTemplate;
    qtcCallBackInfo * sCallBackInfo;

    //------------------------------------
    // ���ռ� �˻�
    //------------------------------------

    IDE_DASSERT( aNode->arguments->module == & qtc::subqueryModule );

    //------------------------------------
    // ����� Estimate
    //------------------------------------

    sCallBackInfo = (qtcCallBackInfo *) aCallBack->info;
    sTemplate = & sCallBackInfo->tmplate->tmplate;

    // Column ������ skipModule�� �����ϰ�, Execute �Լ��� �����Ѵ�.
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    /*
    IDE_TEST( qtc::skipModule.estimate( sColumn, 0, 0, 0 )
              != IDE_SUCCESS );
    */
    IDE_TEST( mtc::initializeColumn( sColumn,
                                     & qtc::skipModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    // To Fix PR-8259
    // Subquery Node ������ Argument�� ���
    // �� ������ ���� Node���� ����� �� �ֵ���
    // Stack�� �����Ѵ�.
    sNode = aNode->arguments->arguments;
    sNode = mtf::convertedNode( sNode, sTemplate );
    aStack[0].column = aTemplate->rows[sNode->table].columns + sNode->column;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_SubqueryWrapper( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *    Subquery Wrapper�� ������ ������.
 *    ���� Subquery Node�� �� �Ǿ� �����Ѵ�.
 *
 * Implementation :
 *
 *    Subquery�� Plan Tree�� ȹ���� ��,
 *    Plan Tree�� �ʱ�ȭ ���ο� ����, plan tree�� ���� ������ �ϸ�,
 *    ����� ���� ��쿡�� NULL�� Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qtcCalculate_SubqueryWrapper"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    
    mtcNode*     sNode;
    mtcStack*    sStack;
    SInt         sRemain;
    qmnPlan*     sPlan;
    qmcRowFlag   sFlag = QMC_ROW_INITIALIZE;

    mtcColumn *  sColumn;
    void      *  sValue;
    
    sStack     = aTemplate->stack;
    sRemain    = aTemplate->stackRemain;

    // Node�� Stack�� Subquery�� Stack�� �ʿ���.
    // List�� Subquery�� ��� Stack �˻縦 Plan Tree���� ������.
    IDE_TEST_RAISE( aRemain < 2, ERR_STACK_OVERFLOW );
    
    // Subquery�� Plan Tree ȹ��.
    sNode = aNode->arguments;
    sPlan = ((qtcNode*)sNode)->subquery->myPlan->plan;

    aTemplate->stack       = aStack + 1;
    aTemplate->stackRemain = aRemain - 1;
    
    if ( aTemplate->execInfo[aNode->info] == QTC_WRAPPER_NODE_EXECUTE_FALSE )
    {
        //---------------------------------
        // �ʱ�ȭ�� ���� ���� ���
        //---------------------------------

        // Plan�� �ʱ�ȭ�Ѵ�.
        IDE_TEST( sPlan->init( (qcTemplate*)aTemplate, sPlan )
                  != IDE_SUCCESS );

        // ����Ǿ����� ǥ����.
        aTemplate->execInfo[aNode->info] = QTC_WRAPPER_NODE_EXECUTE_TRUE;
    }
    else
    {
        //---------------------------------
        // �ʱ�ȭ�� �� ���
        //---------------------------------

        // Nothing To Do
    }

    // Plan �� ����
    IDE_TEST( sPlan->doIt( (qcTemplate*)aTemplate, sPlan, &sFlag )
              != IDE_SUCCESS );

    if ( (sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        //--------------------------------
        // ���� ����� ���� ���
        //--------------------------------

        aStack[0] = aStack[1];
        aStack[0].value = NULL;  // ����� ������ ǥ��

        // Plan Tree ����
        aTemplate->execInfo[aNode->info] = QTC_WRAPPER_NODE_EXECUTE_FALSE;
    }
    else
    {
        //--------------------------------
        // ���� ����� �ִ� ���
        //--------------------------------

        // To Fix PR-8259
        // ���� Subquery�� List������ One Column������ �Ǵ��Ͽ� ó���ؾ� ��.
        // List ��      : ( SELECT i1, i2 FROM .. )
        // One Column�� : ( SELECT i1 FROM .. )

        // Subquery�� Columnȹ��
        sColumn = aTemplate->rows[sNode->table].columns + sNode->column;
        sValue = (void*)
            ( (SChar*) aTemplate->rows[sNode->table].row
              + sColumn->column.offset );
        
        if ( sColumn->module == & mtdList )
        {
            // List�� Subquery�� ���
            // Subquery Node�� Value���� ����
            //
            // Subquery�� Target ������ ������ Stack��
            // ��°�� ������ ������ �����Ѵ�.
            idlOS::memcpy( sValue,
                           & aStack[1],
                           sColumn->column.size );

            // List Subquery Node�� Column���� �� Value������
            // Wrapper Node�� Stack ������ ����
            aStack[0].column = sColumn;
            aStack[0].value = sValue;
        }
        else
        {
            // One Column�� Subquery�� ���
            // Subquery�� ����� ����.
            aStack[0] = aStack[1];
        }
    }

    aTemplate->stack       = sStack;
    aTemplate->stackRemain = sRemain;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;

    aTemplate->stack       = sStack;
    aTemplate->stackRemain = sRemain;
    
    return IDE_FAILURE;
    
#undef IDE_FN
}
