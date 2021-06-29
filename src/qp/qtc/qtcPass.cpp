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
 * $Id: qtcPass.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Pass Node
 *        - ���� ����� ����� �״�� �����ϴ� Node
 *        - ������ �ߺ� ������ �����ϱ� ���� ���ȴ�.
 *     Ex) SELECT i1 + 1 FROM T1 ORDER BY 1;
 *
 *     [PROJ]--------->[Assign]
 *        |              |
 *        |              |
 *        |              |
 *        |            [Pass Node]
 *        |              |
 *        |              V
 *        |        dst [I1 + 1]
 *        |              |
 *     [SORT]-------------
 *                       |
 *                 src [I1 + 1]
 *
 *     �������� ���� [Pass Node]�� �߰��� ���������μ� [i1 + 1]��
 *     �ݺ� ������ ������ �� �ִ�.
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
// Pass �������� �̸��� ���� ����
//-----------------------------------------

static mtcName mtfFunctionName[1] = {
    { NULL, 4, (void*)"PASS" }
};

//-----------------------------------------
// Pass �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qtc::passModule = {
    1|                          // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC,     // ��Ÿ ������
    ~0,                         // Indexable Mask : �ǹ� ����
    1.0,                        // default selectivity (�� ������ �ƴ�)
    mtfFunctionName,            // �̸� ����
    NULL,                       // Counter ������ ����
    mtf::initializeDefault,     // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,       // ���� ����� ���� �Լ�, ����
    qtcEstimate                 // Estimate �� �Լ�
};


//-----------------------------------------
// Pass �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_Pass( mtcNode*  aNode,
                          mtcStack* aStack,
                          SInt      aRemain,
                          void*     aInfo,
                          mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_Pass,    // PASS ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC qtcEstimate( mtcNode*     aNode,
                    mtcTemplate* aTemplate,
                    mtcStack*    /* aStack */,
                    SInt         /* aRemain */,
                    mtcCallBack* /* aCallBack */)
{
/***********************************************************************
 *
 * Description :
 *    Pass �����ڿ� ���Ͽ� Estimate �� ������.
 *    Pass Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    ȣ��Ʈ ������ ������ ���, ���������� ó���� ����
 *    Argument��ü�� ������ Setting�Ѵ�.
 *    �� �� ������ ������ Estimation�� ���� ���� ��
 *    Execution�� �ƹ� ������ ��ġ�� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcNode*         sNode;
    mtcColumn *      sColumn;

    if ( ( aNode->lflag & MTC_NODE_INDIRECT_MASK )
         == MTC_NODE_INDIRECT_TRUE )
    {
        // To Fix PR-8192
        // �Ϲ� Expression���κ��� ������ Pass Node��.

        // Column ������ skipModule�� �����.
        /*
        IDE_TEST( qtc::skipModule.estimate( aTuple[aNode->table].columns +
                                            aNode->column,
                                            0, 0, 0 )
                  != IDE_SUCCESS );
        */
        IDE_TEST(
            mtc::initializeColumn(
                aTemplate->rows[aNode->table].columns + aNode->column,
                & qtc::skipModule,
                0,
                0,
                0 )
            != IDE_SUCCESS );

        //sNode = aNode->arguments;

        aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;
    }
    else
    {
        // Group By Expression���κ��� ������ Pass Node��.
        // �� �ڽ��� Column ������ ������ �����Ѵ�.

        // Converted Node�� ȹ���ؼ��� �ȵȴ�.
        sNode = aNode->arguments;

        // Argument�� Column ������ ���� �� Execute ������ setting�Ѵ�.
        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;

        // BUG-23102
        // mtcColumn���� �ʱ�ȭ�Ѵ�.
        mtc::initializeColumn( sColumn,
                               aTemplate->rows[sNode->table].columns
                               + sNode->column );

        aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_Pass( mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Argument�� �̿��Ͽ� ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

    mtcNode   * sNode;
    mtcColumn * sPassNodeColumn;

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    // Argument ȹ��.
    sNode = mtf::convertedNode( aNode->arguments,
                                aTemplate );

    IDE_DASSERT(sNode->column != MTC_RID_COLUMN_ID);

    // Argument�� �̿��� Column �� Value ���� Setting
    
    aStack->column = aTemplate->rows[sNode->table].columns + sNode->column;

    /* BUG-44762 case when subquery
     *
     * Case When���� Subquery�� ���� ���� Pass Node�� ��쿡�� Node�� ����
     * Value�� ������� �ʴ´�. SubQuery�� ����� �̹� Case when����
     * Stack�� �̸� �����س��Ҵ�
     */
    if ( ( ( ((qtcNode*)(aNode->arguments))->lflag & QTC_NODE_SUBQUERY_MASK )
           == QTC_NODE_SUBQUERY_EXIST ) &&
        ( ( aNode->lflag & MTC_NODE_CASE_EXPRESSION_PASSNODE_MASK )
            == MTC_NODE_CASE_EXPRESSION_PASSNODE_TRUE ) )
    {
        /* Nothing to do */
    }
    else
    {
        aStack->value  = (void*)mtc::value( aStack->column,
                                            aTemplate->rows[sNode->table].row,
                                            MTD_OFFSET_USE );
    }

    if ( ( aNode->lflag & MTC_NODE_INDIRECT_MASK )
         == MTC_NODE_INDIRECT_FALSE )
    {
        // BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻�
        // BUG-28446 [valgrind], BUG-38133
        // qtcCalculate_Pass(mtcNode*, mtcStack*, int, void*, mtcTemplate*) (qtcPass.cpp:333)
        if( ( aNode->lflag & MTC_NODE_CASE_EXPRESSION_PASSNODE_MASK )
            == MTC_NODE_CASE_EXPRESSION_PASSNODE_FALSE )
        {            
            // To Fix PR-10033
            // GROUP BY �÷��� �����ϴ� Pass Node�� Direct Node�̴�.
            // Pass Node�� Indirect Node�� �ƴ� ���
            // �ش� ������ ���� �����ϰ� �־�� �Ѵ�.
            // �̴� Key Range���� ������ ��, indirect node�� ���
            // �ش� Node�� ã�� �� ������ �ִ� Data�� �̿��Ͽ�
            // Key Range�� �����ϴ� �ݸ�, direct node�� ��쿡��
            // Pass Node�κ��� ���� ��� Key Range�� �����ϱ� �����̴�.

            sPassNodeColumn = aTemplate->rows[aNode->table].columns
                + aNode->column;
        
            idlOS::memcpy(
                (SChar*) aTemplate->rows[aNode->table].row
                + sPassNodeColumn->column.offset,
                aStack->value,
                aStack->column->module->actualSize( aStack->column,
                                                    aStack->value ) );
        }
        else
        {
            // BUG-28446 [valgrind]
            // qtcCalculate_Pass(mtcNode*, mtcStack*, int, void*, mtcTemplate*) (qtcPass.cpp:333)
            // SIMPLE CASEó���� ���� �ʿ信 ���� ������ PASS NODE���� ǥ��
            // skipModule�̸� size�� 0�̴�.
            // ����, ����� �� ����.
        }
    }
    else
    {
        // Indirect Pass Node�� ��� Argument���� ����ϱ� ������
        // ���� ���� ������ �ʿ䰡 ����.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
