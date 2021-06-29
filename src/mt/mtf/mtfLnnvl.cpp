/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtxModule mtxLnnvl; /* PROJ-2632 */

//-----------------------------------------
// LNNVL �������� �̸��� ���� ����
//-----------------------------------------

static mtcName mtfFunctionName[1] = {
    { NULL, 5, (void*)"LNNVL" }
};

//-----------------------------------------
// LNNVL �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC mtfEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule mtfLnnvl = {
    1|                                // �ϳ��� Column ����
    MTC_NODE_OPERATOR_FUNCTION|MTC_NODE_COMPARISON_TRUE|MTC_NODE_EAT_NULL_TRUE|
    MTC_NODE_PRINT_FMT_PREFIX_PA,     // ��� format
    ~(MTC_NODE_INDEX_MASK),           // Indexable Mask : Index ��� �Ұ�
    1.0 / 2.0,                        // default selectivity
    mtfFunctionName,                  // �̸� ����
    NULL,                             // Counter ������ ����
    mtf::initializeDefault,           // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,             // ���� ����� ���� �Լ�, ����
    mtfEstimate                       // Estimate �� �Լ�
};

//-----------------------------------------
// Assign �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC mtfLnnvlCalculate( mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtfLnnvlCalculate,    // LNNVL ������ ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtxLnnvl.mCommon,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ���� 
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC mtfEstimate( mtcNode*     aNode,
                    mtcTemplate* aTemplate,
                    mtcStack*    aStack,
                    SInt         /* aRemain */,
                    mtcCallBack* /* aCallback */)
{
/***********************************************************************
 *
 * Description :
 *    LNNVL �����ڿ� ���Ͽ� Estimate �� ������.
 *    LNNVL Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    LNNVL Node�� Column������ Boolean Type���� �����ϰ�,
 *    LNNVL Node�� Execute ������ Setting��
 ***********************************************************************/

    extern mtdModule mtdBoolean;

    mtcNode* sNode;

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    // Argument�� �ϳ������� �˻�
    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    // Argument�� (��, ��) ���������� �˻�
    sNode  = aNode->arguments;

    // LNNVL�� argument�� LNNVL �Ǵ� �񱳿����ڸ� ����(AND, OR �� �Ұ�)
    IDE_TEST_RAISE( ( sNode->lflag & MTC_NODE_COMPARISON_MASK ) != MTC_NODE_COMPARISON_TRUE,
                    ERR_CONVERSION_NOT_APPLICABLE );

    sNode->lflag &= ~(MTC_NODE_INDEX_MASK);

    // Node�� Execute�� ����
    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    // Node�� Column ������ Boolean���� ����
    /*
    IDE_TEST( mtdBoolean.estimate( aStack[0].column, 0, 0, 0 )
              != IDE_SUCCESS );
    */
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdBoolean,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLnnvlCalculate( mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    LNNVL ������ ������.
 *
 *    LNNVL�� �Ϲ� �� �������� NOT���� �̹��� ���̰� �ִ�.
 *
 *    NOT�� Ư¡�� ������ ���� �� Matrix�� ���� �ݸ�, LNNVL�� Unknown
 *    �� ���� ó���� NOT�� �ٸ���.
 *
 *    ----------------------------------------------------------
 *          A         |    NOT ( A )       |   LNNVL ( A )
 *    ----------------------------------------------------------
 *         TRUE       |    FALSE           |   FALSE
 *    ----------------------------------------------------------
 *         FALSE      |    TRUE            |   TRUE
 *    ----------------------------------------------------------
 *         UNKNOWN    |    UNKNOWN         |   TRUE
 *    ----------------------------------------------------------
 *
 *    ���� Matrix���� ������, LNNVL�� UNKNOWN�� ���� ó���� �ٸ���.
 *    �̴� LNNVL�� ������ NOT�� �޸� �ߺ� Data�� �����ϱ� �����̱�
 *    �����̴�.
 *    ���� ���, ������ ���� ������ �ִٰ� ����.
 *        Ex)    A OR B
 *        ==>    [A] concatenation [B AND LNNVL(A)]
 *    [A OR B]�� DNF�� ó�� �� LNNVL�� �̿��� ��ȯ�� �̷������.
 *    ����, [A] �κп����� [A]�� ����� TRUE�� ����� �����ǰ�,
 *    FALSE�� UNKNOWN�� ����� �������� �ʴ´�.
 *    [B] �κп����� [B]�� TRUE�� ��� �߿��� [A]�� ���Ե� �����
 *    �����ؾ� �Ѵ�.  ��, [LNNVL(A)]������ [A]���� ���õ� �������
 *    �����Ϸ���, (A)�� ����� TRUE�� �͸��� �����Ͽ��� �Ѵ�.
 *    ��, (A)�� ����� FALSE�̰ų� UNKNOWN�� ����� �����Ͽ��� �Ѵ�.
 *
 * Implementation :
 *
 *    Argument�� �����ϰ�, Argument�� ������ ����
 *    Matrix�� �ǰ��� ���� �����Ѵ�.
 *
 ***********************************************************************/

    // LNNVL ���� Matrix
    static const mtdBooleanType sMatrix[3] = {
        MTD_BOOLEAN_FALSE,   // TRUE�� ���, FALSE�� ����
        MTD_BOOLEAN_TRUE,    // FALSE�� ���, TRUE�� ����
        MTD_BOOLEAN_TRUE     // UNKNOWN�� ���, TRUE�� ����
    };

    // Argument�� ����
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    // Argument�� ���� Matrix�� �̿��Ͽ� LNNVL�� ���� ����
    *(mtdBooleanType*)aStack[0].value =
                                    sMatrix[*(mtdBooleanType*)aStack[1].value];
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
