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
 * $Id: mtfRegExpReplace.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <idErrorCode.h>
#include <mtdTypes.h>

#include <mtfRegExp.h>
#include <mtl.h>

extern mtdModule mtdInteger;
extern mtdModule mtdBinary;
extern mtdModule mtdVarchar;
extern mtdModule mtdNvarchar;

static mtcName mtfRegExpReplaceFunctionName[1] = {
    { NULL, 14, (void*)"REGEXP_REPLACE" }
};

extern IDE_RC mtfCompileExpression( mtcNode            * aPatternNode,
                                    mtcTemplate        * aTemplate,
                                    mtfRegExpression  ** aCompiledExpression,
                                    mtcCallBack        * aCallBack );

static IDE_RC mtfRegExpReplaceEstimate( mtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       mtcCallBack* aCallBack );

IDE_RC mtfRegExpReplaceString( const mtlModule  * aLanguage,
                               UChar            * aResult,
                               UShort             aResultMaxLen,
                               UShort           * aResultLen,
                               mtfRegExpression * aExp,
                               const UChar      * aSource,
                               UShort             aSourceLen,
                               UChar            * aReplace,
                               UShort             aReplaceLen,
                               SInt               aStart,
                               SInt               aOccurrence );

mtfModule mtfRegExpReplace = {
    2|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfRegExpReplaceFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfRegExpReplaceEstimate
};

static IDE_RC mtfRegExpReplaceCalculateFor2Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor3Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor4Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor5Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor2Args( mtcNode*     aNode,
                                                      mtcStack*    aStack,
                                                      SInt         aRemain,
                                                      void*        aInfo,
                                                      mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor3Args( mtcNode*     aNode,
                                                      mtcStack*    aStack,
                                                      SInt         aRemain,
                                                      void*        aInfo,
                                                      mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor4Args( mtcNode*     aNode,
                                                      mtcStack*    aStack,
                                                      SInt         aRemain,
                                                      void*        aInfo,
                                                      mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor5Args( mtcNode*     aNode,
                                                      mtcStack*    aStack,
                                                      SInt         aRemain,
                                                      void*        aInfo,
                                                      mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor2ArgsFast( mtcNode*     aNode,
                                                     mtcStack*    aStack,
                                                     SInt         aRemain,
                                                     void*        aInfo,
                                                     mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor3ArgsFast( mtcNode*     aNode,
                                                     mtcStack*    aStack,
                                                     SInt         aRemain,
                                                     void*        aInfo,
                                                     mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor4ArgsFast( mtcNode*     aNode,
                                                     mtcStack*    aStack,
                                                     SInt         aRemain,
                                                     void*        aInfo,
                                                     mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateFor5ArgsFast( mtcNode*     aNode,
                                                     mtcStack*    aStack,
                                                     SInt         aRemain,
                                                     void*        aInfo,
                                                     mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor2ArgsFast( mtcNode*     aNode,
                                                          mtcStack*    aStack,
                                                          SInt         aRemain,
                                                          void*        aInfo,
                                                          mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor3ArgsFast( mtcNode*     aNode,
                                                          mtcStack*    aStack,
                                                          SInt         aRemain,
                                                          void*        aInfo,
                                                          mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor4ArgsFast( mtcNode*     aNode,
                                                          mtcStack*    aStack,
                                                          SInt         aRemain,
                                                          void*        aInfo,
                                                          mtcTemplate* aTemplate );

static IDE_RC mtfRegExpReplaceCalculateNcharFor5ArgsFast( mtcNode*     aNode,
                                                          mtcStack*    aStack,
                                                          SInt         aRemain,
                                                          void*        aInfo,
                                                          mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteFor3Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteFor4Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateFor4Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteFor5Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateFor5Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateNcharFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor3Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateNcharFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor4Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateNcharFor4Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor5Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRegExpReplaceCalculateNcharFor5Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfRegExpReplaceEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt      /* aRemain */,
                                 mtcCallBack* aCallBack )
{
    const mtdModule   * sModules[5];
    
    UInt                sPrecision;
    mtcNode           * sPatternNode;
    mtfRegExpression  * sCompiledExpression;
    SInt                sReturnLength;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 2 ) ||
                    ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 5 ),
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;
    
    IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );

    sModules[0] = &mtdVarchar;
    sModules[1] = sModules[0];
    sModules[2] = sModules[0];
    sModules[3] = &mtdInteger;
    sModules[4] = &mtdInteger;

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    /* regexp�� compiled pattern�� ������ */
    sPrecision = MTF_REG_EXPRESSION_SIZE( aStack[2].column->precision );
    
    IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                     & mtdBinary,
                                     1,
                                     sPrecision,
                                     0 )
              != IDE_SUCCESS );

    if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK )
         == MTC_NODE_REESTIMATE_FALSE )
    {
        // no re-estimate
        if ( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
             (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
        {
            if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] =
                    mtfExecuteNcharFor2Args;

                sReturnLength = aStack[1].column->precision;
            }
            else
            {
                if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteNcharFor3Args;
                }
                else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 4 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteNcharFor4Args;
                }
                else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 5 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteNcharFor5Args;
                }
                else
                {
                    // Nothing to do
                }

                if ( aStack[2].column->precision > 0 )
                {
                    if ( aStack[1].column->language->id == MTL_UTF16_ID )
                    {
                        sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                                (SInt)MTD_UTF16_NCHAR_PRECISION_MAXIMUM );
                    }
                    else
                    {
                        sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                                (SInt)MTD_UTF8_NCHAR_PRECISION_MAXIMUM );
                    }
                }
                else
                {
                    sReturnLength = aStack[1].column->precision;
                }
            }
        }
        else
        {
            if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] =
                    mtfExecuteFor2Args;

                sReturnLength = aStack[1].column->precision;
            }
            else
            {
                if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteFor3Args;
                }
                else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 4 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteFor4Args;
                }
                else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 5 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecuteFor5Args;
                }
                else
                {
                    // Nothing to do
                }

                if ( aStack[2].column->precision > 0 )
                {
                    sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                            MTD_CHAR_PRECISION_MAXIMUM );
                }
                else
                {
                    sReturnLength = aStack[1].column->precision;
                }

                if ( (aStack[3].column->module->id == MTD_NCHAR_ID) ||
                        (aStack[3].column->module->id == MTD_NVARCHAR_ID) )
                {
                    if ( aStack[3].column->language->id == MTL_UTF16_ID )
                    {
                        sReturnLength *= MTL_UTF16_PRECISION;
                    }
                    else
                    {
                        sReturnLength *= MTL_UTF8_PRECISION;
                    }
                }
                else
                {
                    // Nothing to do
                }
            }
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdVarchar,
                                         1,
                                         sReturnLength,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // re-estimate
        sPatternNode = mtf::convertedNode( aNode->arguments->next,
                                           aTemplate );

        if ( ( sPatternNode == aNode->arguments->next )
             &&
             ( ( aTemplate->rows[sPatternNode->table].lflag & MTC_TUPLE_TYPE_MASK )
               == MTC_TUPLE_TYPE_CONSTANT ) )
        {
            IDE_TEST ( mtfCompileExpression( sPatternNode,
                                             aTemplate,
                                             &sCompiledExpression,
                                             aCallBack )
                       != IDE_SUCCESS );
            
            if ( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
                 (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
            {
                if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfRegExpReplaceCalculateNcharFor2ArgsFast;

                    sReturnLength = aStack[1].column->precision;
                }
                else
                {
                    if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateNcharFor3ArgsFast;
                    }
                    else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 4 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateNcharFor4ArgsFast;
                    }
                    else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 5 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateNcharFor5ArgsFast;
                    }
                    else
                    {
                        // Nothing to do
                    }

                    if ( aStack[2].column->precision > 0 )
                    {
                        if ( aStack[1].column->language->id == MTL_UTF16_ID )
                        {
                            sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                                    (SInt)MTD_UTF16_NCHAR_PRECISION_MAXIMUM );
                        }
                        else
                        {
                            sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                                    (SInt)MTD_UTF8_NCHAR_PRECISION_MAXIMUM );
                        }
                    }
                    else
                    {
                        sReturnLength = aStack[1].column->precision;
                    }
                }
            }
            else
            {
                if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfRegExpReplaceCalculateFor2ArgsFast;

                    sReturnLength = aStack[1].column->precision;
                }
                else
                {
                    if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateFor3ArgsFast;
                    }
                    else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 4 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateFor4ArgsFast;
                    }
                    else if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 5 )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfRegExpReplaceCalculateFor5ArgsFast;
                    }
                    else
                    {
                        // Nothing to do
                    }
                    if ( aStack[2].column->precision > 0 )
                    {
                        sReturnLength = IDL_MIN( aStack[1].column->precision * ( aStack[3].column->precision + 1 ) + aStack[3].column->precision,
                                MTD_CHAR_PRECISION_MAXIMUM );
                    }
                    else
                    {
                        sReturnLength = aStack[1].column->precision;
                    }

                    if ( (aStack[3].column->module->id == MTD_NCHAR_ID) ||
                         (aStack[3].column->module->id == MTD_NVARCHAR_ID) )
                    {
                        if ( aStack[3].column->language->id == MTL_UTF16_ID )
                        {
                            sReturnLength *= MTL_UTF16_PRECISION;
                        }
                        else
                        {
                            sReturnLength *= MTL_UTF8_PRECISION;
                        }
                    }
                    else
                    {
                        // Nothing to do
                    }
                }
            }

            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                sCompiledExpression;

            // ���̻� ������� ����
            IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                             & mtdBinary,
                                             1,
                                             0,
                                             0 )
                      != IDE_SUCCESS );

            IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                             &mtdVarchar,
                                             1,
                                             sReturnLength,
                                             0 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( ( MTC_NODE_IS_DEFINED_VALUE( aNode->arguments->next ) == ID_TRUE )
         &&
         ( ( ( aTemplate->rows[aNode->arguments->next->table].lflag
               & MTC_TUPLE_TYPE_MASK )
             == MTC_TUPLE_TYPE_CONSTANT ) ||
           ( ( aTemplate->rows[aNode->arguments->next->table].lflag
               & MTC_TUPLE_TYPE_MASK )
             == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
    {
        aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
        aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
            
        // BUG-38070 undef type���� re-estimate���� �ʴ´�.
        if ( ( aTemplate->variableRow != ID_USHORT_MAX ) &&
             ( ( aNode->lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) )
        {
            if ( aTemplate->rows[aTemplate->variableRow].
                 columns->module->id == MTD_UNDEF_ID )
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
        aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
    }

    /* BUG-44740 mtfRegExpression ������ ���� Tuple Row�� �ʱ�ȭ�Ѵ�. */
    aTemplate->rows[aNode->table].lflag &= ~MTC_TUPLE_ROW_MEMSET_MASK;
    aTemplate->rows[aNode->table].lflag |= MTC_TUPLE_ROW_MEMSET_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceString( const mtlModule  * aLanguage,
                               UChar            * aResult,
                               UShort             aResultMaxLen,
                               UShort           * aResultLen,
                               mtfRegExpression * aExp,
                               const UChar      * aSource,
                               UShort             aSourceLen,
                               UChar            * aReplace,
                               UShort             aReplaceLen,
                               SInt               aStart,
                               SInt               aOccurrence )
{
    const SChar *sBeginStr;
    const SChar *sEndStr;
    
    UChar       *sSourceIndex;
    UChar       *sSourceFence;
    UChar       *sNextIndex;
    UChar       *sResultIndex;
    UShort       sResultFence = aResultMaxLen;
    UShort       sResultLen = 0;
    SInt         sCnt = 1;
    UShort       sLen = 0;
    SInt         sStart = 0;
    UShort       sOneCharSize = 0;
    idBool       sFound = ID_FALSE;
    idBool       sBlankMatched = ID_FALSE;
    
    // Ž���� ������ ��ġ ����
    sSourceIndex = (UChar*) aSource;
    sNextIndex = sSourceIndex;
    sResultIndex = aResult;
    sSourceFence = sSourceIndex + aSourceLen;
    
    *aResultLen = 0;
    
    sStart = aStart;

    while ( sStart > 1 )
    {
        if ( sNextIndex < sSourceFence )
        {
            (void)aLanguage->nextCharPtr( & sNextIndex, sSourceFence );
        }
        else
        {
            break;
        }

        --sStart;

        IDE_TEST_RAISE( sResultLen + sNextIndex - sSourceIndex > sResultFence,
                        ERR_INVALID_LENGTH );

        idlOS::memcpy( sResultIndex,
                       sSourceIndex,
                       sNextIndex - sSourceIndex );

        sResultLen += sNextIndex - sSourceIndex;

        sSourceIndex = sNextIndex;

        sResultIndex = aResult + sResultLen;
    }
    sBeginStr = (SChar*)sSourceIndex;
    
    // BUG-45386
    // 'ABC', 'B*' �̷� ��� endOfLine������ ��ġ�Ǿ�� �Ѵ�.
    // ���� sSourceIndex�� sSourceFence�� ��쿡�� search�� �Ѵ�.
    while ( sSourceIndex <= sSourceFence )
    {
        if ( mtfRegExp::search( aExp,
                                (const SChar *)sSourceIndex,
                                aSourceLen - (UShort)( (vULong)sSourceIndex - (vULong)aSource ),
                                &sBeginStr,
                                &sEndStr ) == ID_TRUE )
        {
            sFound = ID_TRUE;           // ������ �ѹ��̶� ��ġ-> TRUE
            sBlankMatched = ID_FALSE;   // ������ empty string('')�� ��ġ
            
            // source ���ڿ����� sSourceIndex ��ġ���� ��ġ�� ���� �� ��ġ������ Result�� ����
            // ex: regexp_replace( 'ABC', 'B' ) -> 'A'���� Result�� ����
            if ( sSourceIndex != (UChar*)sBeginStr )
            {
                sLen = (UShort)( (vULong)sBeginStr - (vULong)sSourceIndex );

                IDE_TEST_RAISE( sResultLen + sLen > sResultFence,
                                ERR_INVALID_LENGTH );

                idlOS::memcpy( sResultIndex,
                               sSourceIndex,
                               sLen );

                sResultLen += sLen;
                sResultIndex += sLen;
            }
            else
            {
                // ������ ���� ��ġ(sSourceIndex)���� �ٷ� ���� ���
                // ex: regexp_replace( 'ABC', 'A' )
                // Nothing To Do
            }

            // 'ABC','B*' �̷��� ��� sEndStr�� �־��� sSourceIndex�� ������ �ִ�.
            // sSourceIndex = sBeginStr = sEndStr
            if ( sSourceIndex != (UChar*)sEndStr )
            {
                sSourceIndex = (UChar*)sEndStr;
            }
            else
            {
                if ( sSourceIndex < sSourceFence )
                {
                    (void)aLanguage->nextCharPtr( &sSourceIndex, sSourceFence );
                }
                else
                {
                    // Nothing To Do
                }
                sBlankMatched = ID_TRUE;
            }
            
            if ( aOccurrence > 0 )
            {
                if ( aOccurrence == sCnt )
                {
                    // ���� ������ aReplace�� �ٲ㼭 copy
                    if ( aReplace == NULL )
                    {
                        // NULL�� ��� Result�� �������� �ʴ´�(���� ����)
                        // Nothing To Do
                    }
                    else
                    {
                        // sResult�� aReplace�� �����Ѵ�
                        if ( aReplaceLen > 0 )
                        {
                            IDE_TEST_RAISE( sResultLen + aReplaceLen > sResultFence,
                                            ERR_INVALID_LENGTH );

                            idlOS::memcpy( sResultIndex,
                                           aReplace,
                                           aReplaceLen );

                            sResultLen += aReplaceLen;
                            sResultIndex += aReplaceLen;
                        }
                        else
                        {
                            // ġȯ ���ڿ�(aReplace)�� ���̰� 0�� ��� Result�� �������� �ʴ´�(���� ����)
                            // Nothing To Do
                        }
                    }

                    // 'ABC','B*' �̷��� ���
                    // �ƹ��͵� ���µ� ���� ���̻��̿��� �� ���ڿ�('')�� ��ġ�� �� �ִ�.
                    // �� ���� replace string�� �տ� ���̰� source�� �������ش�.
                    // ex) SELECT REGEXP_REPLACE('ABC', 'B*', '123') FROM DUAL;
                    // ���: '123A123123C'
                    // (B�� 123���� ��ü, ������ A�� C�� ��� �տ� 123�� ���� ����)

                    // �� line������ aResult�� replace string�� �ٿ����� ����.
                    // source string �� ���ڸ� �����ؼ� �ٿ������� �ȴ�.
                    if ( sBlankMatched == ID_TRUE )
                    {
                        sOneCharSize = (UShort) mtl::getOneCharSize( (UChar*)sEndStr,
                                                                     sSourceFence,
                                                                     aLanguage );

                        IDE_TEST_RAISE( sResultLen + sOneCharSize > sResultFence,
                                        ERR_INVALID_LENGTH );

                        idlOS::memcpy( sResultIndex,
                                       sBeginStr,
                                       sOneCharSize );

                        sResultLen += sOneCharSize;
                        sResultIndex += sOneCharSize;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                    break;
                }
                else
                {
                    // ���� ������ ġȯ�����ʰ� �״�� ����
                    sCnt++;

                    // 'ABC','B*' �̷��� ���
                    // �ƹ��͵� ���µ� ���� ���̻��̿��� �� ���ڿ�('')�� ��ġ�� �� �ִ�.
                    // �� ���� replace string�� �տ� ���̰� source�� �������ش�.
                    // ex) SELECT REGEXP_REPLACE('ABC', 'B*', '123') FROM DUAL;
                    // ���: '123A123123C'
                    // (B�� 123���� ��ü, ������ A�� C�� ��� �տ� 123�� ���� ����)
                   
                    // aOccurrence�� ��ġ���� �����Ƿ� ġȯ ���ڿ�(aReplace)�� ���� �ʰ�
                    // source string �� ���ڸ� �����ؼ� �ٿ������� �ȴ�.
                    if ( sBeginStr == sEndStr )
                    {
                        sLen = (UShort) mtl::getOneCharSize( (UChar*)sEndStr,
                                                             sSourceFence,
                                                             aLanguage );
                    }
                    else
                    {
                        sLen = (UShort)( (vULong)sEndStr - (vULong)sBeginStr );
                    }

                    IDE_TEST_RAISE( sResultLen + sLen > sResultFence,
                                    ERR_INVALID_LENGTH );

                    idlOS::memcpy( sResultIndex,
                                   sBeginStr,
                                   sLen );

                    sResultLen += sLen;
                    sResultIndex += sLen;
                }
            }
            else
            {
                // �Ź� ��ġ�� ������ ġȯ�ؼ� ����
                if ( aReplace == NULL )
                {
                    // NULL�� ��� Result�� �������� �ʴ´�(���� ����)
                    // Nothing To Do
                }
                else
                {
                    if ( aReplaceLen > 0 )
                    {
                        IDE_TEST_RAISE( sResultLen + aReplaceLen > sResultFence,
                                        ERR_INVALID_LENGTH );

                        idlOS::memcpy( sResultIndex,
                                       aReplace,
                                       aReplaceLen );

                        sResultLen += aReplaceLen;
                        sResultIndex += aReplaceLen;
                    }
                    else
                    {
                        // ġȯ ���ڿ�(aReplace)�� ���̰� 0�� ��� Result�� �������� �ʴ´�(���� ����)
                        // Nothing To Do
                    }
                }

                // 'ABC','B*' �̷��� ���
                // �ƹ��͵� ���µ� ���� ���̻��̿��� �� ���ڿ�('')�� ��ġ�� �� �ִ�.
                // �� ���� replace string�� �տ� ���̰� source�� �������ش�.
                // ex) SELECT REGEXP_REPLACE('ABC', 'B*', '123') FROM DUAL;
                // ���: '123A123123C'
                // (B�� 123���� ��ü, ������ A�� C�� ��� �տ� 123�� ���� ����)

                // �� line������ aResult�� replace string�� �ٿ����� ����.
                // source string �� ���ڸ� �����ؼ� �ٿ������� �ȴ�.
                if ( sBlankMatched == ID_TRUE )
                {
                    sOneCharSize = (UShort) mtl::getOneCharSize( (UChar*)sEndStr,
                                                                 sSourceFence,
                                                                 aLanguage );

                    IDE_TEST_RAISE( sResultLen + sOneCharSize > sResultFence,
                                    ERR_INVALID_LENGTH );

                    idlOS::memcpy( sResultIndex,
                                   sBeginStr,
                                   sOneCharSize );
                    
                    sResultLen += sOneCharSize;
                    sResultIndex += sOneCharSize;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }
        else
        {
            break;
        }

        // BUG-45386
        // 'ABC', 'B*' �̷� ��� endOfLine������ ��ġ�Ǿ�� �Ѵ�.
        // ���� sSourceIndex�� sSourceFence�� ��쿡�� search�� �Ѵ�.
        // �� Fence�� �������� �� 1���� search�ϸ� �Ǳ� ������ ���⼭ break�Ѵ�.
        if ( ( sSourceIndex == (UChar*)sBeginStr ) && ( sSourceIndex == sSourceFence ) )
        {
            break;
        }
        else
        {
            // Nothing To Do
        }
    }

    if ( sFound != ID_TRUE )
    {
        // �ƹ��͵� ġȯ���� ����; search ������ ���� ���⼭ source ��ü�� Result�� ����
        if ( sSourceIndex < sSourceFence )
        {
            IDE_TEST_RAISE( aSourceLen > sResultFence,
                            ERR_INVALID_LENGTH );

            idlOS::memcpy( aResult,
                           aSource,
                           aSourceLen );
        }
        else
        {
            // Nothing To Do
        }
        *aResultLen = aSourceLen;
    }
    else
    {
        // ġȯ ����ǰ� ������ ���ϱ��� �˻� ���� ���
        // source�� ���� string�� Result�� ����
        if ( sSourceIndex < sSourceFence )
        {
            sLen = (UShort)( (vULong)sSourceFence - (vULong)sSourceIndex );
            
            IDE_TEST_RAISE( sResultLen + sLen > sResultFence,
                            ERR_INVALID_LENGTH );
            
            idlOS::memcpy( sResultIndex,
                           sSourceIndex,
                           sLen );

            sResultLen += sLen;
        }
        else
        {
            // Nothing To Do
        }
        
        *aResultLen = sResultLen;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor2Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 2 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1 ( string2, int1, int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ������
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            
            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              NULL,
                                              0,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor3Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 3 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2 ( int1, int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor4Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 4 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2, int1 ( int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *    aStack[4] : int1 ( �˻� ���� ��ġ )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
        
            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);
            
            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
    
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              0 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor5Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 5 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2, int1, int2 )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *    aStack[4] : int1 ( �˻� ���� ��ġ )
 *    aStack[5] : int2 ( ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    SInt               sOccurrence = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
            sOccurrence = *(mtdIntegerType*)aStack[5].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );

            IDE_TEST_RAISE( ( sOccurrence < 0 ),
                            ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              sOccurrence )
                    != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION( ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sOccurrence ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor2Args( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 2 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1 ( string2, int1, int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ������
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sSrcCharSet = aStack[1].column->language;

            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              NULL,
                                              0,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor3Args( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 3 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2 ( int1, int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sSrcCharSet = aStack[1].column->language;

            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor4Args( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 4 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2, int1 ( int2 ) )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *    aStack[4] : int1 ( �˻� ���� ��ġ )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sSrcCharSet = aStack[1].column->language;

            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              0 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor5Args( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Regexp_Replace Calculate with 5 arguments
 *
 * Implementation :
 *    REGEXP_REPLACE( char, string1, string2, int1, int2 )
 *
 *    aStack[0] : char �� string1�� �ش��ϴ� �κ��� ġȯ��
 *    aStack[1] : char 
 *    aStack[2] : string1 ( ġȯ ��� ���� )
 *    aStack[3] : string2 ( ġȯ ���� )
 *    aStack[4] : int1 ( �˻� ���� ��ġ )
 *    aStack[5] : int2 ( ���� )
 *
 ***********************************************************************/
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sFrom;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    SInt               sOccurrence = 0;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    const mtcColumn  * sColumn;
    mtdBinaryType    * sTempValue;
    mtfRegExpression * sCompiledExpression;
    UChar            * sPattern;
    UShort             sPatternLength = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sFrom       = (mtdCharType*)aStack[2].value;
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
            sOccurrence = *(mtdIntegerType*)aStack[5].value;

            sPattern = sFrom->value;
            sPatternLength = sFrom->length;

            sSrcCharSet = aStack[1].column->language;

            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );

            IDE_TEST_RAISE( ( sOccurrence < 0 ),
                            ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );

            sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
            sTempValue      = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);
            sCompiledExpression = (mtfRegExpression*)(sTempValue->mValue);

            /* BUG-45213 valgrin warning
             * SortTemp�� ���� mtrNode�� ���� ��� �׻� ���ο� Row��
             * �Ҵ�ǹǷ� ���� sCompiledExpression->patternLen ���غ���
             * �ǹ̰� ���� ���� �̷���� �׳� Compile�Ѵ�.
             */
            if ( ( aTemplate->rows[aNode->table].lflag & MTC_TUPLE_PLAN_MTR_MASK )
                 == MTC_TUPLE_PLAN_MTR_TRUE )
            {
                IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                 (const SChar *)sPattern,
                                                 sPatternLength )
                          != IDE_SUCCESS );
            }
            else
            {
                // pattern compile
                if ( sPatternLength != sCompiledExpression->patternLen )
                {
                    IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                     (const SChar *)sPattern,
                                                     sPatternLength )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( idlOS::memcmp( sPattern,
                                        sCompiledExpression->patternBuf,
                                        sPatternLength ) != 0 )
                    {
                        IDE_TEST( mtfRegExp::expCompile( sCompiledExpression,
                                                         (const SChar *)sPattern,
                                                         sPatternLength )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              sOccurrence )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION( ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sOccurrence ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor2ArgsFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;

    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult = (mtdCharType*)aStack[0].value;
        sSource = (mtdCharType*)aStack[1].value;
            
        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              NULL,
                                              0,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor3ArgsFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor4ArgsFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
            
            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateFor5ArgsFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    SInt               sOccurrence = 0;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
            sOccurrence = *(mtdIntegerType*)aStack[5].value;
    
            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    
            IDE_TEST_RAISE( ( sOccurrence < 0 ),
                            ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              aStack[0].column->precision,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              sOccurrence )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );
    
    IDE_EXCEPTION( ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sOccurrence ) );

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor2ArgsFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;

    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;
    
        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sSrcCharSet = aStack[1].column->language;
    
            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              NULL,
                                              0,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor3ArgsFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
    
            sSrcCharSet = aStack[1].column->language;
    
            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              1,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor4ArgsFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
    
            sSrcCharSet = aStack[1].column->language;
    
            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);
            
            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              0 )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRegExpReplaceCalculateNcharFor5ArgsFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate )
{
    mtdCharType      * sResult;
    mtdCharType      * sSource;
    mtdCharType      * sReplace;
    SInt               sStart = 0;
    SInt               sOccurrence = 0;
    const mtlModule  * sSrcCharSet;
    UShort             sResultMaxLen = 0;
    
    mtfRegExpression * sCompiledExpression;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult     = (mtdCharType*)aStack[0].value;
        sSource     = (mtdCharType*)aStack[1].value;

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value ) == ID_TRUE )
        {
            // NULL pattern
            idlOS::memcpy( sResult->value,
                           sSource->value,
                           sSource->length );
            sResult->length = sSource->length;
        }
        else
        {
            sReplace    = (mtdCharType*)aStack[3].value;
            sStart      = *(mtdIntegerType*)aStack[4].value;
            sOccurrence = *(mtdIntegerType*)aStack[5].value;
    
            sSrcCharSet = aStack[1].column->language;
    
            sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);
    
            IDE_TEST_RAISE( ( sStart <= 0 ),
                            ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    
            IDE_TEST_RAISE( ( sOccurrence < 0 ),
                            ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
            
            sCompiledExpression = (mtfRegExpression*)aInfo;
            
            IDE_TEST( mtfRegExpReplaceString( aStack[1].column->language,
                                              sResult->value,
                                              sResultMaxLen,
                                              & sResult->length,
                                              sCompiledExpression,
                                              sSource->value,
                                              sSource->length,
                                              sReplace->value,
                                              sReplace->length,
                                              sStart,
                                              sOccurrence )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sStart ) );
    
    IDE_EXCEPTION( ERR_ARGUMENT5_VALUE_OUT_OF_RANGE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                              sOccurrence ) );

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
