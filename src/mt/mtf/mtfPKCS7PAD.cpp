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
 * $Id: mtfPKCS7PAD.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

extern mtdModule mtdVarchar;

static mtcName mtfPKCS7PADFunctionName[1] = {
    { NULL, 10, (void*)"PKCS7PAD16" }
};

static IDE_RC mtfPKCS7PADEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

mtfModule mtfPKCS7PAD = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (비교 연산자가 아님)
    mtfPKCS7PADFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfPKCS7PADEstimate
};

static IDE_RC mtfPKCS7PADCalculate( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfPKCS7PADCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfPKCS7PADEstimate( mtcNode*     aNode,
                            mtcTemplate* aTemplate,
                            mtcStack*    aStack,
                            SInt      /* aRemain */,
                            mtcCallBack* aCallBack )
{
    const mtdModule* sModules[1];
    UInt  sPrecision;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sModules[0] = &mtdVarchar;

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    /* 결과를 저장함 */
    sPrecision = ( ( aStack[1].column->precision / 16 ) + 1 ) * 16;
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     &mtdVarchar,
                                     1,
                                     sPrecision,
                                     0 )
              != IDE_SUCCESS );
    
    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));
    
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfPKCS7PADCalculate( mtcNode*     aNode,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             void*        aInfo,
                             mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PKCS7_PAD Calculate
 *
 * Implementation :
 * 
 * ... DD [ DD DD DD DD DD DD DD DD DD DD DD DD 04 04 04 04 ]
 ***********************************************************************/
    
    mtdCharType*     sPaddedValue;
    mtdCharType*     sData;
    
    UInt             sBlockCount;
    UInt             sLastBlockDataSize;
    UInt             sPadSize;
    UInt             i;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( ( aStack[1].column->module->isNull( aStack[1].column,
                                             aStack[1].value ) == ID_TRUE ) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sData = (mtdCharType*)aStack[1].value;
        sPaddedValue = (mtdCharType*)aStack[0].value;
        
        sBlockCount = ( sData->length / 16 ) + 1;
        sLastBlockDataSize = sData->length % 16;
        sPadSize = 16 - sLastBlockDataSize;
        
        idlOS::memcpy( sPaddedValue->value, sData->value, sData->length );
        for ( i = 0; i < sPadSize; i++ )
        {
            sPaddedValue->value[sData->length + i] = sPadSize;
        }
        sPaddedValue->length = sBlockCount * 16;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
