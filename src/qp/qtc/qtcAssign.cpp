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
 * $Id: qtcAssign.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     ASSIGN ������ �����ϴ� Node
 *
 *     To fix BUG-20272
 *     subquery�� target�� conversion node�� �����Ǿ�� �ȵǴ� ���
 *     conversion�� ���� assign node�� �����Ѵ�.
 *
 *     ex) decode(:v1, 1, (select max(i1) from t1))
 *                                ^^^^^^
 *     max(i1) ���� aggr������ mtrNode�� �����ϹǷ� indirect conversion��
 *     �����Ǿ�� �ȵȴ�. �̷� ��� max(i1)�� ���ڷ� ���� assign node��
 *     �����Ͽ� assign node�� indirect conversion node�� �޾ƾ� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <ide.h>
#include <mte.h>

extern mtdModule mtdList;

//-----------------------------------------
// Assign �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 6, (void*)"ASSIGN" }
};

//-----------------------------------------
// Assign �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qtc::assignModule = {
    1 |                     // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC, // ��Ÿ ������
    ~(MTC_NODE_INDEX_MASK), // Indexable Mask
    1.0,                    // default selectivity (�� ������ �ƴ�)
    qtcNames,               // �̸� ����
    NULL,                   // Counter ������ ����
    mtf::initializeDefault, // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,   // ���� ����� ���� �Լ�, ����
    qtcEstimate             // Estimate �� �Լ�
};

//-----------------------------------------
// Assign �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_Assign( mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_Assign,  // ASSIGN ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC qtcEstimate( mtcNode*     aNode,
                    mtcTemplate* aTemplate,
                    mtcStack*    aStack,
                    SInt      /* aRemain */,
                    mtcCallBack* aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Assign �����ڿ� ���Ͽ� Estimate �� ������.
 *    Assign Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *    TODO - Host ������ ���翡 ���� ����� �Ǿ� ���� ����
 *
 *    Argument�� Column������ �̿��Ͽ� Assign Node�� Column ������ �����ϰ�,
 *    Assign Node�� Execute ������ Setting��
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    qtcCallBackInfo * sInfo;
    mtcNode         * sNode;
    mtcColumn       * sColumn;

    sInfo = (qtcCallBackInfo*)aCallBack->info;

    // Argument �� ��´�.
    sNode = mtf::convertedNode( aNode->arguments,
                                & sInfo->tmplate->tmplate );

    // Argument�� Column ������ ���� �� Execute ������ setting�Ѵ�.
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;

    // BUG-23102
    // mtcColumn���� �ʱ�ȭ�Ѵ�.
    mtc::initializeColumn( sColumn,
                           aTemplate->rows[sNode->table].columns + sNode->column );

    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    // ���������� Estimation�� ���� Stack�� Column ���� Setting
    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_Assign( mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*,
                            mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Assign ���� ( a := b ) ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Argument�� �����ϰ� �� ����� Assign����� ��ġ�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_Assign"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcNode*  sNode;

    // BUG-33674
    IDE_TEST_RAISE( aRemain < 2, ERR_STACK_OVERFLOW );
    
    // Assign Node�� Column���� �� Value ������ ��ġ�� ������.
    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack->value  = (void*)( (UChar*) aTemplate->rows[aNode->table].row
                              + aStack->column->column.offset );

    // Argument�� ������ ȹ����.
    sNode  = aNode->arguments;

    // Argument�� ������ ������.
    IDE_TEST( aTemplate->rows[sNode->table].
              execute[sNode->column].calculate(                         sNode,
                                                                        aStack + 1,
                                                                        aRemain - 1,
                                                                        aTemplate->rows[sNode->table].execute[sNode->column].calculateInfo,
                                                                        aTemplate )
              != IDE_SUCCESS );

    // Argument�� Conversion�� ���� ��� Conversion ������ ������.
    if( sNode->conversion != NULL )
    {
        IDE_TEST( mtf::convertCalculate( sNode,
                                         aStack + 1,
                                         aRemain - 1,
                                         NULL,
                                         aTemplate )
                  != IDE_SUCCESS );
    }

    // Argument�� ���� ����� ����� Stack�����κ���
    // Assign Node�� Value ��ġ�� ������.
    idlOS::memcpy( aStack[0].value, aStack[1].value,
                   aStack[1].column->module->actualSize(
                       aStack[1].column,
                       aStack[1].value ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
