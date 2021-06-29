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
 * $Id: qtcValue.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Value�� �ǹ��ϴ� Node
 *     Ex) 'ABC'
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <mte.h>

//-----------------------------------------
// Value �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 5, (void*)"VALUE" }
};

//-----------------------------------------
// Value �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcValueEstimate( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                mtcCallBack* aCallBack );

mtfModule qtc::valueModule = {
    1|                      // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC, // ��Ÿ ������
    ~0,                     // Indexable Mask : �ǹ� ����
    1.0,                    // default selectivity (�� ������ �ƴ�)
    qtcNames,               // �̸� ����
    NULL,                   // Counter ������ ����
    mtf::initializeDefault, // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,   // ���� ����� ���� �Լ�, ����
    qtcValueEstimate        // Estimate �� �Լ�
};

//-----------------------------------------
// Value �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_Value(  mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_Value,   // VALUE ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateEmpty,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC qtcValueEstimate( mtcNode*     aNode,
                         mtcTemplate* aTemplate,
                         mtcStack*    aStack,
                         SInt         /* aRemain */,
                         mtcCallBack* /* aCallback */ )
{
/***********************************************************************
 *
 * Description :
 *    Value �����ڿ� ���Ͽ� Estimate �� ������.
 *    Value Node�� ���� Execute ������ ������
 *
 * Implementation :
 *
 *    Stack�� Value Node�� ���� Column ������ �����ϰ�
 *    Value Node�� ���� Execute ������ Setting
 ***********************************************************************/

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;
    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    return IDE_SUCCESS;
}

IDE_RC qtcCalculate_Value( mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*,
                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Value�� ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_Value"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack->value  = (void*)( (UChar*) aTemplate->rows[aNode->table].row
                              + aStack->column->column.offset );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
