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
 * $Id: qtcIndirect.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Indirection ������ �����ϴ� Node
 *
 *     ������ ���� ������ ��ġ�� �ʰ�, ���ο� ���� ���踦 ����� ����
 *     ����Ѵ�.
 *
 *     ���� ��� ������ ���� ���ǰ� �ִٰ� ����.
 *     WHERE (I1, I2) IN ( SELECT A1, A2 FROM ... );
 *
 *     ���⿡ (I1 = A1) AND (I2 = A2)�� ���� ������ ������ ��,
 *     ���� �׸��� ���� ���� ���踦 �����ϸ鼭 �� ������ ������ �� �ִ�.
 *
 *                    [IN]
 *                     |
 *                     V
 *                    [LIST]  -------------> [LIST]
 *                     |                      |
 *                     V                      V
 *                     I1 --> I2             A1 --> A2
 *                     ^                      ^
 *                     |                      |
 *                    [Indirect] ---------> [Indirect]
 *                     ^                      
 *                     |                      
 *                    [ = ]   : (I1 = A1)�� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <ide.h>

//-----------------------------------------
// INDIRECT �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 8, (void*)"INDIRECT" }
};

//-----------------------------------------
// INDIRECT �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qtc::indirectModule = {
    1 |                      // �ϳ��� Column ����
    MTC_NODE_OPERATOR_MISC | // ��Ÿ ������
    MTC_NODE_INDIRECT_TRUE,  // Indirection�� ������
    ~0,                      // Indexable Mask : �ǹ� ����
    1.0,                     // default selectivity (�� ������ �ƴ�)
    qtcNames,                // �̸� ����
    NULL,                    // Counter ������ ����
    mtf::initializeDefault,  // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,    // ���� ����� ���� �Լ�, ����
    qtcEstimate              // Estimate �� �Լ�
};

//-----------------------------------------
// INDIRECT �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_Indirect( mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*        aInfo,
                              mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,      // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,      // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,      // Aggregation ���� �Լ�, ����
    qtcCalculate_Indirect, // INDIRECT�� ���� ���� �Լ�
    NULL,                  // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA,  // Key Range ũ�� ���� �Լ�, ���� 
    mtk::extractRangeNA    // Key Range ���� �Լ�, ����
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
 *    INDIRECT �����ڿ� ���Ͽ� Estimate �� ������.
 *    Indirect Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    PROJ-1492�� CAST�����ڰ� �߰��Ǿ� ȣ��Ʈ ������ ����ϴ���
 *    �� Ÿ���� ���ǵǾ� Validation�� Estimate �� ȣ���� �� �ִ�.
 *
 *    Indirect ���� ������ Column ������ �ʿ�����Ƿ�,
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

    sCallBackInfo = (qtcCallBackInfo *) aCallBack->info;
    sTemplate = & sCallBackInfo->tmplate->tmplate;

    // Column ������ skipModule�� �����ϰ�, Execute �Լ��� �����Ѵ�.
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    /*
    IDE_TEST( qtc::skipModule.estimate( sColumn, 0, 0, 0 )
              != IDE_SUCCESS );
    */

    // Argument�� ��� �� ������ ���� Node���� ����� �� �ֵ���
    // Stack�� �����Ѵ�.
    sNode = aNode->arguments;

    IDE_TEST( mtc::initializeColumn( sColumn,
                                     & qtc::skipModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    sNode = mtf::convertedNode( sNode, sTemplate );

    aStack[0].column = aTemplate->rows[sNode->table].columns + sNode->column;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_Indirect( mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*,
                              mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Indirection�Ͽ� argument�� �����ϰ�, �̿� ���� Conversion�� �����Ѵ�.
 *
 * Implementation :
 *
 *    Argument�� �����ϰ� Argument�� Conversion�� �����Ѵ�.
 *    ��, �ڽŸ��� ������ �۾��� �������� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_Indirect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    
    mtcNode*  sNode;
    
    // BUG-33674
    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    sNode  = aNode->arguments;
    IDE_TEST( aTemplate->rows[sNode->table].
              execute[sNode->column].calculate(                         sNode,
                                                                       aStack,
                                                                      aRemain,
           aTemplate->rows[sNode->table].execute[sNode->column].calculateInfo,
                                                                    aTemplate )
              != IDE_SUCCESS );
    
    if( sNode->conversion != NULL )
    {
        IDE_TEST( mtf::convertCalculate( sNode,
                                         aStack,
                                         aRemain,
                                         NULL,
                                         aTemplate )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
#undef IDE_FN
}
 
