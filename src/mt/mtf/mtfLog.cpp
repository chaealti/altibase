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
 

/***********************************************************************
 * $Id: mtfLog.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfLog;

extern mtdModule mtdDouble;

static mtcName mtfLogFunctionName[1] = {
    { NULL, 3, (void*)"LOG" }
};

static IDE_RC mtfLogEstimate( mtcNode*     aNode,
                              mtcTemplate* aTemplate,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              mtcCallBack* aCallBack );

mtfModule mtfLog = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (비교 연산자가 아님)
    mtfLogFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfLogEstimate
};

IDE_RC mtfLogCalculate(    mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*        aInfo,
                           mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLogCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfLogEstimate( mtcNode*     aNode,
                       mtcTemplate* aTemplate,
                       mtcStack*    aStack,
                       SInt      /* aRemain */,
                       mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdDouble,
        &mtdDouble
    };

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    //IDE_TEST( mtdDouble.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdDouble,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLogCalculate( mtcNode*     aNode,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        void*        aInfo,
                        mtcTemplate* aTemplate )
{
    mtdDoubleType sValue1;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( ( mtdDouble.isNull( aStack[1].column,
                            aStack[1].value ) == ID_TRUE ) ||
        ( mtdDouble.isNull( aStack[2].column,
                            aStack[2].value ) == ID_TRUE ) )
    {
        mtdDouble.null( aStack[0].column,
                        aStack[0].value );
    }
    else
    {
        IDE_TEST_RAISE( *(mtdDoubleType*)aStack[1].value <= 0 ||
                        *(mtdDoubleType*)aStack[1].value == 1,
                        ERR_ARGUMENT_NOT_APPLICABLE );
        IDE_TEST_RAISE( *(mtdDoubleType*)aStack[2].value <= 0,
                        ERR_ARGUMENT_NOT_APPLICABLE );

        sValue1 = idlOS::log( *(mtdDoubleType*)aStack[1].value );
        
        IDE_TEST_RAISE( sValue1 == 0, ERR_DIVIDE_BY_ZERO );
        
        *(mtdDoubleType*)aStack[0].value =
            idlOS::log( *(mtdDoubleType*)aStack[2].value ) / sValue1;
        
        IDE_TEST_RAISE( mtdDouble.isNull( aStack[0].column,
                                          aStack[0].value )
                        == ID_TRUE, ERR_ARGUMENT_NOT_APPLICABLE );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_DIVIDE_BY_ZERO );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_DIVIDE_BY_ZERO));
    
    IDE_EXCEPTION(ERR_ARGUMENT_NOT_APPLICABLE  );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
