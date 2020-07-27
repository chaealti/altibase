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
 * $Id: qtcIndirect.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 *
 * Description :
 *
 *     Indirection 연산을 수행하는 Node
 *
 *     기존의 연결 정보를 해치지 않고, 새로운 연결 관계를 만들기 위해
 *     사용한다.
 *
 *     예를 들어 다음과 같은 질의가 있다고 하자.
 *     WHERE (I1, I2) IN ( SELECT A1, A2 FROM ... );
 *
 *     여기에 (I1 = A1) AND (I2 = A2)와 같은 정보를 구성할 때,
 *     다음 그림과 같이 연결 관계를 유지하면서 그 정보를 구성할 수 있다.
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
 *                    [ = ]   : (I1 = A1)의 구성
 *
 * 용어 설명 :
 *
 * 약어 :
 *
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <ide.h>

//-----------------------------------------
// INDIRECT 연산자의 이름에 대한 정보
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 8, (void*)"INDIRECT" }
};

//-----------------------------------------
// INDIRECT 연산자의 Module 에 대한 정보
//-----------------------------------------

static IDE_RC qtcEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qtc::indirectModule = {
    1 |                      // 하나의 Column 공간
    MTC_NODE_OPERATOR_MISC | // 기타 연산자
    MTC_NODE_INDIRECT_TRUE,  // Indirection을 수행함
    ~0,                      // Indexable Mask : 의미 없음
    1.0,                     // default selectivity (비교 연산자 아님)
    qtcNames,                // 이름 정보
    NULL,                    // Counter 연산자 없음
    mtf::initializeDefault,  // 서버 구동시 초기화 함수, 없음
    mtf::finalizeDefault,    // 서버 종료시 종료 함수, 없음
    qtcEstimate              // Estimate 할 함수
};

//-----------------------------------------
// INDIRECT 연산자의 수행 함수의 정의
//-----------------------------------------

IDE_RC qtcCalculate_Indirect( mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*        aInfo,
                              mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,      // Aggregation 초기화 함수, 없음
    mtf::calculateNA,      // Aggregation 수행 함수, 없음
    mtf::calculateNA,
    mtf::calculateNA,      // Aggregation 종료 함수, 없음
    qtcCalculate_Indirect, // INDIRECT를 위한 연산 함수
    NULL,                  // 연산을 위한 부가 정보, 없음
    mtx::calculateNA,
    mtk::estimateRangeNA,  // Key Range 크기 추출 함수, 없음 
    mtk::extractRangeNA    // Key Range 생성 함수, 없음
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
 *    INDIRECT 연산자에 대하여 Estimate 를 수행함.
 *    Indirect Node에 대한 Column 정보 및 Execute 정보를 Setting한다.
 *
 * Implementation :
 *
 *    PROJ-1492로 CAST연산자가 추가되어 호스트 변수를 사용하더라도
 *    그 타입이 정의되어 Validation시 Estimate 를 호출할 수 있다.
 *
 *    Indirect 노드는 별도의 Column 정보가 필요없으므로,
 *    Skip Module로 estimation을 하며, 상위 Node에서의 estimate 를
 *    위하여 하위 Node의 정보를 Stack에 설정하여 준다.
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

    // Column 정보를 skipModule로 설정하고, Execute 함수를 지정한다.
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;

    /*
    IDE_TEST( qtc::skipModule.estimate( sColumn, 0, 0, 0 )
              != IDE_SUCCESS );
    */

    // Argument를 얻어 이 정보를 상위 Node에서 사용할 수 있도록
    // Stack에 설정한다.
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
 *    Indirection하여 argument를 수행하고, 이에 대한 Conversion을 수행한다.
 *
 * Implementation :
 *
 *    Argument를 수행하고 Argument의 Conversion을 수행한다.
 *    즉, 자신만의 별도의 작업을 수행하지 않는다.
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
 
