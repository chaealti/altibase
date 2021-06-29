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
 * $Id: mtfUpper.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>

extern mtfModule mtfUpper;
extern mtlModule mtlUTF16;

static mtcName mtfUpperFunctionName[1] = {
    { NULL, 5, (void*)"UPPER" }
};

static IDE_RC mtfUpperEstimate( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                mtcCallBack* aCallBack );

mtfModule mtfUpper = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfUpperFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfUpperEstimate
};

static IDE_RC mtfUpperCalculate( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

// PROJ-1579 NCHAR
static IDE_RC mtfUpperCalculateChar4MB( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate );

// PROJ-1579 NCHAR
static IDE_RC mtfUpperCalculateNchar4MB( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate );

const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfUpperCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

// PROJ-1579 NCHAR
const mtcExecute mtfExecuteChar4MB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfUpperCalculateChar4MB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

// PROJ-1579 NCHAR
const mtcExecute mtfExecuteNchar4MB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfUpperCalculateNchar4MB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfUpperEstimate( mtcNode*     aNode,
                         mtcTemplate* aTemplate,
                         mtcStack*    aStack,
                         SInt      /* aRemain */,
                         mtcCallBack* aCallBack )
{
    const mtdModule* sModules[1];

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    // PROJ-1579 NCHAR
    if( aStack[1].column->language->id == MTL_ASCII_ID )
    {
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;
    }
    else
    {
        if( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
            (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteNchar4MB;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteChar4MB;
        }
    }

    /*
    IDE_TEST( sModules[0]->estimate( aStack[0].column,
                                     1,
                                     aStack[1].column->precision,
                                     0 )
              != IDE_SUCCESS );
    */

    // PROJ-1579 NCHAR
    // ASCII �̿��� ���ڿ� ���� ��ҹ��� ��ȯ������
    // precision�� �������� �ʴ´�.
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sModules[0],
                                     1,
                                     aStack[1].column->precision,
                                     0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfUpperCalculate( mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *    UPPER( char )
 *
 *    aStack[0] : �Է� ���ڿ��� �빮�ڷ� ��ȯ�� ��
 *    aStack[1] : char ( �Է� ���ڿ� )
 *
 *    ex) UPPER( 'Capital' ) ==> 'CAPITAL'
 *
 ***********************************************************************/
    
    mtdCharType*   sResult;
    mtdCharType*   sInput;
    UChar*         sTarget;
    UChar*         sIterator;
    UChar*         sFence;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult  = (mtdCharType*)aStack[0].value;
        sInput = (mtdCharType*)aStack[1].value;
        
        sResult->length = sInput->length;
        
        for( sTarget   = sResult->value,
                 sIterator = sInput->value,
                 sFence    = sIterator + sInput->length;
             sIterator < sFence;
             sIterator++, sTarget++ )
        {
            if( *sIterator >= 'a' && *sIterator <= 'z' )
            {
                *sTarget = *sIterator - 0x20;
            }
            else
            {
                *sTarget = *sIterator;
            }
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfUpperCalculateChar4MB( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : 
 *      PROJ-1579 NCHAR
 *      CHAR Ÿ�Կ� ���� �빮�� ��ȯ
 *
 * Implementation :
 *    UPPER( char )
 *
 *    aStack[0] : �Է� ���ڿ��� �빮�ڷ� ��ȯ�� ��
 *    aStack[1] : char ( �Է� ���ڿ� )
 *
 *    ex) UPPER( 'Capital' ) ==> 'CAPITAL'
 *
 ***********************************************************************/
    
    mtdCharType        * sResult;
    mtdCharType        * sSource;
    UChar              * sSourceIndex;
    UChar              * sTempIndex;
    UChar              * sSourceFence;
    UChar              * sResultValue;
    UChar              * sResultFence;

    const mtlModule    * sSrcCharSet;
    const mtlModule    * sDestCharSet;
    idnCharSetList       sIdnSrcCharSet;
    idnCharSetList       sIdnDestCharSet;
    SInt                 sSrcRemain = 0;
    SInt                 sDestRemain = 0;
    SInt                 sTempRemain = 0;
    mtlU16Char           sU16Result;
    SInt                 sU16ResultLen;
    mtlU16Char           sUpperResult;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult = (mtdCharType*)aStack[0].value;
        sSource = (mtdCharType*)aStack[1].value;

        sSourceIndex = sSource->value;
        sSrcRemain   = sSource->length;
        sSourceFence = sSourceIndex + sSrcRemain;

        // ��ȯ ����� ũ�⸦ üũ�ϱ� ����
        sDestRemain = aStack[0].column->precision;

        sResultValue = sResult->value;
        sResultFence = sResultValue + sDestRemain;

        // ����� ���̿� �Է��� ���̴� ����.
        sResult->length = sSource->length;

        sSrcCharSet = aStack[1].column->language;
        sDestCharSet = & mtlUTF16;

        // --------------------------------------------------------
        // �Ʒ��� ���� �۾��� �ʿ��ϴ�.
        // 1. SrcCharSet => UTF16���� ��ȯ
        // 2. ��ҹ��� ��ȯǥ ����
        // 3. UTF16 => SrcCharSet���� ��ȯ
        // --------------------------------------------------------

        sIdnSrcCharSet = mtl::getIdnCharSet( sSrcCharSet );
        sIdnDestCharSet = mtl::getIdnCharSet( sDestCharSet );

        while( sSourceIndex < sSourceFence )
        {
            IDE_TEST_RAISE( sResultValue >= sResultFence,
                            ERR_INVALID_DATA_LENGTH );

            sTempRemain = sDestRemain;
            
            // 7bit ASCII�� ��쿡�� ��ȯǥ ������ �ʿ����.
            if( IDN_IS_ASCII( *sSourceIndex ) == ID_TRUE )
            {
                if( *sSourceIndex >= 'a' && *sSourceIndex <= 'z' )
                {
                    *sResultValue = *sSourceIndex - 0x20;
                }
                else
                {
                    *sResultValue = *sSourceIndex;
                }
                sDestRemain -= 1;
            }
            else
            {
                sU16ResultLen = MTL_UTF16_PRECISION;

                // 1. SrcCharSet => DestCharSet(UTF16)���� ��ȯ
                IDE_TEST( convertCharSet( sIdnSrcCharSet,
                                          sIdnDestCharSet,
                                          sSourceIndex,
                                          sSrcRemain,
                                          & sU16Result,
                                          & sU16ResultLen,
                                          -1 /* mNlsNcharConvExcp */ )
                          != IDE_SUCCESS );

                /* BUG-42671 ���� ���ڰ� ���� ���
                 * UTF16�� Replaceement character�� 2Byte�� �ݸ�
                 * utf8�� Replacement character�� 3byte�� ����
                 * DestRemain�� 3byte�� �ٷ� Src�� 1byte�� �����Ǿ�
                 * �ᱹ ����Ǵٺ��� ���۰� ���ڸ��� �ǹǷ�
                 * utf18�� ? �� utf8 �� ? �� ��ȯ���� �ʰ� ���� �״��
                 * �� copy �Ѵ�
                 */
                if ( ( sU16Result.value1 == 0xff ) &&
                     ( sU16Result.value2 == 0xfd ) )
                {
                    *sResultValue = *sSourceIndex;
                    sDestRemain--;
                }
                else
                {
                    // 2. ��ҹ��� ��ȯǥ ����
                    // IDN_NLS_CASE_UNICODE_MAX���� ���� ��쿡�� ��ȯ�Ѵ�.
                    // IDN_NLS_CASE_UNICODE_MAX���� ũ�� ��ҹ��� ��ȯ�� 
                    // �ǹ̰� �����Ƿ� �״�� copy�Ѵ�.

                    mtl::getUTF16UpperStr( &sUpperResult, &sU16Result );

                    // 3. DestCharSet(UTF16) => SrcCharSet���� ��ȯ
                    IDE_TEST( convertCharSet( sIdnDestCharSet,
                                              sIdnSrcCharSet,
                                              & sUpperResult,
                                              MTL_UTF16_PRECISION,
                                              sResultValue,
                                              & sDestRemain,
                                              -1 /* mNlsNcharConvExcp */ )
                              != IDE_SUCCESS );
                }
            }

            sTempIndex = sSourceIndex;

            (void)sSrcCharSet->nextCharPtr( & sSourceIndex, sSourceFence );
            
            sResultValue += (sTempRemain - sDestRemain);
            
            sSrcRemain -= ( sSourceIndex - sTempIndex );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DATA_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfUpperCalculateNchar4MB( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : 
 *      PROJ-1579 NCHAR
 *      NCHAR Ÿ�Կ� ���� �빮�� ��ȯ
 *
 * Implementation :
 *    UPPER( nchar )
 *
 *    aStack[0] : �Է� ���ڿ��� �빮�ڷ� ��ȯ�� ��
 *    aStack[1] : nchar ( �Է� ���ڿ� )
 *
 *    ex) UPPER( 'Capital' ) ==> 'CAPITAL'
 *
 ***********************************************************************/
    
    mtdNcharType       * sResult;
    mtdNcharType       * sSource;
    UChar              * sSourceIndex;
    UShort               sSourceLen;
    UChar              * sTempIndex;
    UChar              * sSourceFence;
    UChar              * sResultValue;
    UChar              * sResultFence;

    const mtlModule    * sSrcCharSet;
    const mtlModule    * sDestCharSet;
    idnCharSetList       sIdnSrcCharSet;
    idnCharSetList       sIdnDestCharSet;
    SInt                 sSrcRemain = 0;
    SInt                 sDestRemain = 0;
    SInt                 sTempRemain = 0;
    mtlU16Char           sU16Result;
    SInt                 sU16ResultLen;
    mtlU16Char           sUpperResult;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult = (mtdNcharType*)aStack[0].value;
        sSource = (mtdNcharType*)aStack[1].value;

        // UTF8 or UTF16
        sSrcCharSet = aStack[1].column->language;
        sDestCharSet = & mtlUTF16;

        sSourceIndex = sSource->value;
        sSourceLen   = sSource->length;
        sSrcRemain   = sSourceLen;
        sSourceFence = sSourceIndex + sSrcRemain;

        sResultValue = sResult->value;
        sDestRemain  = sSrcCharSet->maxPrecision(aStack[0].column->precision);
        sResultFence = sResultValue + sDestRemain;

        // ����� ���̿� �Է��� ���̴� ����.
        sResult->length = sSource->length;

        // ------------------------------------
        // ��ҹ��� ��ȯǥ�� �����ϱ� ���� 
        // UTF16 ĳ���� ������ ��ȯ�Ѵ�.
        // ------------------------------------
        if( sSrcCharSet->id == MTL_UTF16_ID )
        {
            while( sSourceIndex < sSourceFence )
            {
                IDE_TEST_RAISE( sResultValue >= sResultFence,
                                ERR_INVALID_DATA_LENGTH );

                mtl::getUTF16UpperStr( (mtlU16Char*)sResultValue,
                                       (mtlU16Char*)sSourceIndex );

                (void)sSrcCharSet->nextCharPtr( & sSourceIndex, sSourceFence );
                
                sResultValue += MTL_UTF16_PRECISION;
            }
        }
        else
        {
            // --------------------------------------------------------
            // SrcCharSet�� UTF16�� �ƴϸ� �Ʒ��� ���� �۾��� �ʿ��ϴ�.
            // 1. SrcCharSet(UTF8) => UTF16���� ��ȯ
            // 2. ��ҹ��� ��ȯǥ ����
            // 3. UTF16 => SrcCharSet(UTF8)���� ��ȯ
            // --------------------------------------------------------

            sIdnSrcCharSet = mtl::getIdnCharSet( sSrcCharSet );
            sIdnDestCharSet = mtl::getIdnCharSet( sDestCharSet );

            while( sSourceIndex < sSourceFence )
            {
                IDE_TEST_RAISE( sResultValue >= sResultFence,
                                ERR_INVALID_DATA_LENGTH );

                sTempRemain = sDestRemain;
                
                // 7bit ASCII�� ��쿡�� ��ȯǥ ������ �ʿ����.
                if( IDN_IS_ASCII( *sSourceIndex ) == ID_TRUE )
                {
                    if( *sSourceIndex >= 'a' && *sSourceIndex <= 'z' )
                    {
                        *sResultValue = *sSourceIndex - 0x20;
                    }
                    else
                    {
                        *sResultValue = *sSourceIndex;
                    }
                    sDestRemain -= 1;
                }
                else
                {
                    sU16ResultLen = MTL_UTF16_PRECISION;

                    // 1. SrcCharSet(UTF8) => DestCharSet(UTF16)���� ��ȯ
                    IDE_TEST( convertCharSet( sIdnSrcCharSet,
                                              sIdnDestCharSet,
                                              sSourceIndex,
                                              sSrcRemain,
                                              & sU16Result,
                                              & sU16ResultLen,
                                              -1 /* mNlsNcharConvExcp */ )
                              != IDE_SUCCESS );

                    /* BUG-42671 ���� ���ڰ� ���� ���
                     * UTF16�� Replaceement character�� 2Byte�� �ݸ�
                     * utf8�� Replacement character�� 3byte�� ����
                     * DestRemain�� 3byte�� �ٷ� Src�� 1byte�� �����Ǿ�
                     * �ᱹ ����Ǵٺ��� ���۰� ���ڸ��� �ǹǷ�
                     * utf18�� ? �� utf8 �� ? �� ��ȯ���� �ʰ� ���� �״��
                     * �� copy �Ѵ�
                     */
                    if ( ( sU16Result.value1 == 0xff ) &&
                         ( sU16Result.value2 == 0xfd ) )
                    {
                        *sResultValue = *sSourceIndex;
                        sDestRemain--;
                    }
                    else
                    {
                        // 2. ��ҹ��� ��ȯǥ ����
                        // IDN_NLS_CASE_UNICODE_MAX���� ���� ��쿡�� ��ȯ�Ѵ�.
                        // IDN_NLS_CASE_UNICODE_MAX���� ũ�� ��ҹ��� ��ȯ�� 
                        // �ǹ̰� �����Ƿ� �״�� copy�Ѵ�.
                        mtl::getUTF16UpperStr( &sUpperResult, &sU16Result );

                        // 3. DestCharSet(UTF16) => SrcCharSet(UTF8)���� ��ȯ
                        IDE_TEST( convertCharSet( sIdnDestCharSet,
                                                  sIdnSrcCharSet,
                                                  & sUpperResult,
                                                  MTL_UTF16_PRECISION,
                                                  sResultValue,
                                                  & sDestRemain,
                                                  -1 /* mNlsNcharConvExcp */ )
                                  != IDE_SUCCESS );
                    }
                }

                sTempIndex = sSourceIndex;

                (void)sSrcCharSet->nextCharPtr( & sSourceIndex, sSourceFence );
                
                sResultValue += (sTempRemain - sDestRemain);
                
                sSrcRemain -= ( sSourceIndex - sTempIndex );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DATA_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
