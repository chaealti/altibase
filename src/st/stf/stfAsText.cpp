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
 * $Id$
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtdTypes.h>
#include <qtc.h>
#include <ste.h>
#include <stfBasic.h>

#define STF_ASTEXT_MINIMUM_PRECISION   (32)
#define STF_ASTEXT_MAXIMUM_PRECISION   (MTD_VARCHAR_PRECISION_MAXIMUM)
#define STF_ASTEXT_DEFAULT_PRECISION   (256)

extern mtfModule stfAsText;

static mtcName stfAsTextFunctionName[2] = {
    { stfAsTextFunctionName+1, 9, (void*)"ST_ASTEXT" }, // Fix BUG-15519
    { NULL, 6, (void*)"ASTEXT" }
};

static IDE_RC stfAsTextEstimate(
                        mtcNode*     aNode,
                        mtcTemplate* aTemplate,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        mtcCallBack* aCallBack );

mtfModule stfAsText = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (비교 연산자가 아님)
    stfAsTextFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    stfAsTextEstimate
};

IDE_RC stfAsTextCalculate(
                        mtcNode*     aNode,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        void*        aInfo,
                        mtcTemplate* aTemplate );

static const mtcExecute stfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    stfAsTextCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC stfAsTextEstimate( mtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt      /* aRemain */,
                          mtcCallBack* aCallBack )
{
    mtcNode        * sNode;
    ULong            sLflag;
    SInt             sPrecision;
    mtcNode        * sPrecNode;
    SLong            sValue;

    extern mtdModule stdGeometry;

    const mtdModule* sModules[2];
    const mtdModule* sVarCharModules[1];

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    // To Fix BUG-15940
    // ASTEXT( OBJ ) or ASTEXT( OBJ, precision )
    IDE_TEST_RAISE( (( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 1) ||
                    (( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 2),
                    ERR_INVALID_FUNCTION_ARGUMENT );

    for( sNode  = aNode->arguments, sLflag = MTC_NODE_INDEX_UNUSABLE;
         sNode != NULL;
         sNode  = sNode->next )
    {
        if( ( sNode->lflag & MTC_NODE_COMPARISON_MASK ) ==
            MTC_NODE_COMPARISON_TRUE )
        {
            sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
        }
        sLflag |= sNode->lflag & MTC_NODE_INDEX_MASK;
    }

    aNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    aNode->lflag |= sLflag;

    if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
    {
        sModules[0] = &stdGeometry;
    }
    else
    {
        sModules[0] = &stdGeometry;
        sModules[1] = aStack[2].column->module;
    }

    IDE_TEST( mtf::getCharFuncResultModule( &sVarCharModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = stfExecute;

    //--------------------------
    // To Fix BUG-15940
    // ASTEXT( obj ) or ASTEXT( obj, precision )
    //--------------------------

    if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
    {
        //--------------------------
        // ASTEXT(OBJ)
        //--------------------------

        sPrecision = STF_ASTEXT_DEFAULT_PRECISION;
    }
    else
    {
        //--------------------------
        // ASTEXT(OBJ, precision)
        //--------------------------

        IDE_DASSERT( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 );
        sPrecNode = aNode->arguments->next;

        //--------------------------
        // constant value validate & get precision
        //--------------------------

        IDE_TEST_RAISE( qtc::getConstPrimitiveNumberValue( (qcTemplate*) aTemplate,
                                                           (qtcNode*) sPrecNode,
                                                           & sValue )
                        == ID_FALSE, ERR_INVALID_PRECISION );
        
        //--------------------------
        // Check Precision
        //--------------------------

        IDE_TEST_RAISE( ( (sValue < STF_ASTEXT_MINIMUM_PRECISION) ||
                          (sValue > STF_ASTEXT_MAXIMUM_PRECISION) ),
                        ERR_INVALID_PRECISION );

        sPrecision = (SInt) sValue;
    }

    //--------------------------
    // Estimation
    //--------------------------

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sVarCharModules[0],
                                     1,
                                     sPrecision,
                                     0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_INVALID_PRECISION );
    IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_FUNCTION_PRECISION));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC stfAsTextCalculate(
                        mtcNode*     aNode,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        void*        aInfo,
                        mtcTemplate* aTemplate )
{
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    IDE_TEST( stfBasic::asText( aNode,
                                aStack,
                                aRemain,
                                aInfo,
                                aTemplate ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
