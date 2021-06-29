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
 * $Id: qtcConstantWrapper.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Host Constant Wrapper Node
 *        - �׻� ������ ����� �����ϴ� ������ �� ���� �����ϰ�
 *        - �� ����� �ݺ������� ����� �� ����Ѵ�.
 *        - Host ������ �ִ� �κп� ���ؼ��� ó���ϰ�,
 *        - Host ������ ���� ������ Validation ���� �߿�
 *          Pre-Processing Constant Expression�� ���Ͽ� ó���ȴ�.
 * 
 *        - Ex) 4 + ?
 *
 *          [Host Constant Wrapper Node]
 *                     |
 *                     V
 *                    [+]
 *                     |
 *                     V
 *                    [4]------>[?]
 *
 *     ���� �׸������� ���� [4+?]�� �� ���� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qtc.h>
#include <qci.h>

//-----------------------------------------
// Host Constant Wrapper �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 22, (void*)"HOST_CONSTANT_WRAPPER" }
};

//-----------------------------------------
// Constant Wrapper �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcEstimate_HostConstantWrapper( mtcNode*     aNode,
                                               mtcTemplate* aTemplate,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               mtcCallBack* aCallBack );

mtfModule qtc::hostConstantWrapperModule = {
    1|                      // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC| // ��Ÿ ������
    MTC_NODE_INDIRECT_TRUE, // Indirection��
    ~0,                     // Indexable Mask : �ǹ� ����
    0.0,                    // BUG-39036 ( 1 -> 0 ���� )
    qtcNames,               // �̸� ����
    NULL,                   // Counter ������ ����
    mtf::initializeDefault, // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,   // ���� ����� ���� �Լ�, ����
    qtcEstimate_HostConstantWrapper     // Estimate �� �Լ�
};

//-----------------------------------------
// Constant Wrapper �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_HostConstantWrapper( 
                            mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,             // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,             // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,             // Aggregation ���� �Լ�, ����
    qtcCalculate_HostConstantWrapper, // CONSTANT WRAPPER ���� �Լ�
    NULL,                         // ������ ���� �ΰ� ����, ����
    mtx::calculateEmpty,
    mtk::estimateRangeNA,         // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA           // Key Range ���� �Լ�, ����
};

IDE_RC qtcEstimate_HostConstantWrapper( mtcNode*     aNode,
                                        mtcTemplate* aTemplate,
                                        mtcStack*    aStack,
                                        SInt      /* aRemain */,
                                        mtcCallBack* /*aCallBack*/ )
{
/***********************************************************************
 *
 * Description :
 *    Constant Wrapper �����ڿ� ���Ͽ� Estimate �� ������.
 *    Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    Constant Wrapper Node��
 *    Plan Node ���� �� ���������� ó���Ǵ� ����̴�.
 *    ����, ���� Node���� Estimate �� ���� ȣ���ϴ� ��찡 ����.
 *
 *    Constant Wrapper ���� ������ Column ������ �ʿ�����Ƿ�,
 *    Skip Module�� estimation�� �ϸ�, ���� Node������ estimate ��
 *    ���Ͽ� ���� Node�� ������ Stack�� �����Ͽ� �ش�.
 *
 *    PROJ-1492�� CAST�����ڰ� �߰��Ǿ� ȣ��Ʈ ������ ����ϴ���
 *    �� Ÿ���� ���ǵǾ� Validation�� Estimate �� ȣ���� �� �ִ�.
 *
 ***********************************************************************/

#define IDE_FN "qtcEstimate_HostConstantWrapper"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcNode   * sNode;
    mtcColumn * sColumn;

    // Column ������ skipModule�� �����ϰ�, Execute �Լ��� �����Ѵ�.
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    IDE_TEST( mtc::initializeColumn( sColumn,
                                     & qtc::skipModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    // Argument�� ��� �� ������ ���� Node���� ����� �� �ֵ���
    // Stack�� �����Ѵ�.
    sNode = aNode->arguments;

    aStack[0].column = aTemplate->rows[sNode->table].columns + sNode->column;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_HostConstantWrapper( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*,
                                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *    Constant Wrapper�� ������ ������.
 *    ���� �� ���� Argument�� �����ϰ�, �� �Ĵ� Argument�� ��������
 *    �ʴ´�.
 *
 * Implementation :
 *
 *    Template�� Execution ������ �̿��Ͽ�, �̹� ����Ǿ�������
 *    ���θ� �Ǵ��Ѵ�.
 *
 *    ������� ���� ���,
 *        - Argument�� �����ϰ�, �� ��� Stack�� �����Ѵ�.
 *    ����� ���,
 *        - Argument ������ �̿��Ͽ� Stack�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qtcCalculate_HostConstantWrapper"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    
    mtcNode*  sNode;
    mtcStack * sStack;

    IDE_TEST_RAISE( aRemain < 2, ERR_STACK_OVERFLOW );

    if ( aTemplate->execInfo[aNode->info] == QTC_WRAPPER_NODE_EXECUTE_FALSE )
    {
        //---------------------------------
        // �� ���� ������� ���� ���
        //---------------------------------

        // Arguemnt�� ȹ���Ѵ�.
        sNode  = aNode->arguments;
        sStack = aStack;
        sStack++;
        aRemain--;

        // Argument�� �����Ѵ�.
        IDE_TEST( aTemplate->rows[sNode->table].
                  execute[sNode->column].calculate(                     sNode,
                                                                       sStack,
                                                                      aRemain,
           aTemplate->rows[sNode->table].execute[sNode->column].calculateInfo,
                                                                    aTemplate )
              != IDE_SUCCESS );
    
        if( sNode->conversion != NULL )
        {
            IDE_TEST( mtf::convertCalculate( sNode,
                                             sStack,
                                             aRemain,
                                             NULL,
                                             aTemplate )
                      != IDE_SUCCESS );
        }

        // Argument�� ���� ����� Stack�� ����.
        aStack[0] = aStack[1];
        aTemplate->execInfo[aNode->info] = QTC_WRAPPER_NODE_EXECUTE_TRUE;
    }
    else
    {
        //---------------------------------
        // �̹� ����� ���
        //---------------------------------

        // ������� �ִ� Argument�� ȹ����.
        sNode = aNode->arguments;
        sNode = mtf::convertedNode( sNode, aTemplate );

        // Column ������ Stack�� ������.
        aStack[0].column = aTemplate->rows[sNode->table].columns
            + sNode->column;

        // Value ������ ȹ���Ͽ� Stack�� ������.
        aStack[0].value = (void *) mtc::value( aStack[0].column, 
                                               aTemplate->rows[sNode->table].row, 
                                               MTD_OFFSET_USE );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
#undef IDE_FN
}
 
