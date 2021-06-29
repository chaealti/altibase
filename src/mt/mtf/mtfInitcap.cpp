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
 * $Id: mtfInitcap.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>

extern mtfModule mtfInitcap;

extern mtlModule mtlUTF16;

static mtcName mtfInitcapFunctionName[1] = {
    { NULL, 7, (void*)"INITCAP" }
};

static IDE_RC mtfInitcapEstimate( mtcNode*     aNode,
                                  mtcTemplate* aTemplate,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  mtcCallBack* aCallBack );

mtfModule mtfInitcap = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfInitcapFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfInitcapEstimate
};

static IDE_RC mtfInitcapCalculate( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

// PROJ-1579 NCHAR
static IDE_RC mtfInitcapCalculateChar4MB( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

// PROJ-1579 NCHAR
static IDE_RC mtfInitcapCalculateNchar4MB( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfInitcapCalculate,
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
    mtfInitcapCalculateChar4MB,
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
    mtfInitcapCalculateNchar4MB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfInitcapEstimate( mtcNode*     aNode,
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

IDE_RC mtfInitcapCalculate( mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Initcap Calculate
 *
 * Implementation :
 *    INITCAP( char )
 *
 *    aStack[0] : �־��� ���ڿ��� ù���ڸ� �빮�ڷ� ��ȯ
 *    aStack[1] : char ( �־��� ���ڿ� )
 *
 *    ex) initcap( 'the soap' )
 *        --> result : The soap
 *
 ***********************************************************************/
    
    mtdCharType*   sResult;
    mtdCharType*   sSource;
    UChar*         sCurResult;
    UChar*         sCurInput;
    UChar*         sFence; 
    UInt           sCharStatus = 0;
    // sCharStatus
    /*
      0 : start state
      1 : initCapitaled state
     */
    
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
        sSource  = (mtdCharType*)aStack[1].value;

        // sResult->length�� �־��� ���ڿ��� ���� ����
        sResult->length = sSource->length;

        // sResult->value�� �־��� ���ڿ��� ù���ڸ� �빮�ڷ� ��ȯ�Ͽ� ����
        for( sCurResult = sResult->value,
             sCurInput  = sSource->value,
             sFence     = sCurInput + sSource->length;
             sCurInput < sFence;
             sCurInput++, sCurResult++ )
        {

            // PR-13399�� ���� ���� ������ oracle�� �����ϰ� ��ü.
            
            if( *sCurInput == ' ' ||
                *sCurInput == '\t' ||
                *sCurInput == '\n' ||
                isalnum( *sCurInput ) == 0 )
            {
                // ����, ��, ���� ������ ���, �״�� ����
                *sCurResult = *sCurInput;
                sCharStatus = 0;
            }
            else if( isalnum( *sCurInput ) != 0 )
            {
                if( sCharStatus == 0 )
                {
                    if( *sCurInput >= 'a' && *sCurInput <= 'z' )
                    {
                        *sCurResult = *sCurInput - 0x20;
                    }
                    else
                    {
                        // �״�� ����
                        *sCurResult = *sCurInput;
                    }
                    sCharStatus = 1;
                }
                else if( sCharStatus ==  1 )
                {
                    if( *sCurInput >= 'A' && *sCurInput <= 'Z' )
                    {
                        *sCurResult = *sCurInput + 0x20;
                    }
                    else
                    {
                        // �״�� ����
                        *sCurResult = *sCurInput;
                    }
                }
            }
            else
            {
                // nothing to do
            }
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfInitcapCalculateChar4MB( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : 
 *      PROJ-1579 NCHAR
 *      Initcap Calculate
 *
 * Implementation :
 *    INITCAP( char )
 *
 *    aStack[0] : �־��� ���ڿ��� ù���ڸ� �빮�ڷ� ��ȯ
 *    aStack[1] : char ( �־��� ���ڿ� )
 *
 *    ex) initcap( 'the soap' )
 *        --> result : The soap
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
    mtlU16Char           sCaseResult;
    UInt                 sCharStatus = 0;
    
    // sCharStatus
    /*
      0 : start state
      1 : initCapitaled state
     */
    
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
        sSource  = (mtdCharType*)aStack[1].value;

        sSourceIndex = sSource->value;
        sSrcRemain   = sSource->length;
        sSourceFence = sSourceIndex + sSrcRemain;

        // ��ȯ ����� ũ�⸦ üũ�ϱ� ����
        sDestRemain = aStack[0].column->precision;

        sResultValue = sResult->value;
        sResultFence = sResultValue + sDestRemain;

        // sResult->length�� �־��� ���ڿ��� ���� ����
        sResult->length = sSource->length;

        sSrcCharSet = aStack[1].column->language;
        sDestCharSet = & mtlUTF16;

        sIdnSrcCharSet = mtl::getIdnCharSet( sSrcCharSet );
        sIdnDestCharSet = mtl::getIdnCharSet( sDestCharSet );

        // sResult->value�� �־��� ���ڿ��� ù���ڸ� �빮�ڷ� ��ȯ�Ͽ� ����
        while( sSourceIndex < sSourceFence )
        {
            IDE_TEST_RAISE( sResultValue >= sResultFence,
                            ERR_INVALID_DATA_LENGTH );

            sTempRemain = sDestRemain;
                        
            // PR-13399�� ���� ���� ������ oracle�� �����ϰ� ��ü.
            if( *sSourceIndex == ' ' ||
                *sSourceIndex == '\t' ||
                *sSourceIndex == '\n' )
            {
                // ����, ��, ���� ������ ���, �״�� ����
                *sResultValue = *sSourceIndex;
                sCharStatus = 0;
                sDestRemain -= 1;
            }
            else if( isalnum( *sSourceIndex ) == 0 )
            {
                if( IDN_IS_ASCII( *sSourceIndex ) == ID_TRUE )
                {
                    *sResultValue = *sSourceIndex;
                    sCharStatus = 0;
                    sDestRemain -= 1;
                }
                else
                {
                    // 7bit ASCII�� �ƴ� ����
                    // ��ҹ��� ��ȯǥ�� ���� ��ȯ�� �ʿ��ϴ�.

                    // --------------------------------------------------------
                    // �Ʒ��� ���� �۾��� �ʿ��ϴ�.
                    // 1. SrcCharSet => UTF16���� ��ȯ
                    // 2. ��ҹ��� ��ȯǥ ����
                    // 3. UTF16 => SrcCharSet���� ��ȯ
                    // --------------------------------------------------------

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

                        if ( sCharStatus == 0 )
                        {
                            mtl::getUTF16UpperStr( &sCaseResult, &sU16Result );
                            sCharStatus = 1;
                        }
                        else
                        {
                            mtl::getUTF16LowerStr( &sCaseResult, &sU16Result );
                        }
                        
                        // 3. DestCharSet(UTF16) => SrcCharSet(UTF8)���� ��ȯ
                        IDE_TEST( convertCharSet( sIdnDestCharSet,
                                                  sIdnSrcCharSet,
                                                  & sCaseResult,
                                                  MTL_UTF16_PRECISION,
                                                  sResultValue,
                                                  & sDestRemain,
                                                  -1 /* mNlsNcharConvExcp */ )
                                  != IDE_SUCCESS );
                    }
                }
            }
            else if( isalnum( *sSourceIndex ) != 0 )
            {
                if( sCharStatus == 0 )
                {
                    if( *sSourceIndex >= 'a' && *sSourceIndex <= 'z' )
                    {
                        *sResultValue = *sSourceIndex - 0x20;
                    }
                    else
                    {
                        // �״�� ����
                        *sResultValue = *sSourceIndex;
                    }
                    sCharStatus = 1;
                }
                else
                {
                    if( *sSourceIndex >= 'A' && *sSourceIndex <= 'Z' )
                    {
                        *sResultValue = *sSourceIndex + 0x20;
                    }
                    else
                    {
                        // �״�� ����
                        *sResultValue = *sSourceIndex;
                    }
                }

                sDestRemain -= 1;
            }
            else
            {
                // nothing to do
            }

            sTempIndex = sSourceIndex;

            (void)sSrcCharSet->nextCharPtr( & sSourceIndex, sSourceFence );
            
            sResultValue += (sTempRemain - sDestRemain );
            
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

IDE_RC mtfInitcapCalculateNchar4MB( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : 
 *      PROJ-1579 NCHAR
 *      Initcap Calculate
 *
 * Implementation :
 *    INITCAP( nchar )
 *
 *    aStack[0] : �־��� ���ڿ��� ù���ڸ� �빮�ڷ� ��ȯ
 *    aStack[1] : nchar ( �־��� ���ڿ� )
 *
 *    ex) initcap( 'the soap' )
 *        --> result : The soap
 *
 ***********************************************************************/
    
    mtdNcharType       * sResult;
    mtdNcharType       * sSource;
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
    mtlU16Char           sCaseResult;
    UInt                 sCharStatus = 0;

    idBool               sIsSpaceSame = ID_FALSE;
    idBool               sIsTabSame = ID_FALSE;
    idBool               sIsNewLineSame = ID_FALSE;
    UChar                sSize;
    
    // sCharStatus
    /*
      0 : start state
      1 : initCapitaled state
     */
    
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

        sSrcCharSet = aStack[1].column->language;
        sDestCharSet = & mtlUTF16;

        sSourceIndex = sSource->value;
        sSrcRemain   = sSource->length;
        sSourceFence = sSourceIndex + sSrcRemain;

        // ��ȯ ����� ũ�⸦ üũ�ϱ� ����
        sDestRemain = sSrcCharSet->maxPrecision(aStack[0].column->precision);

        sResultValue = sResult->value;
        sResultFence = sResultValue + sDestRemain;

        // sResult->length�� �־��� ���ڿ��� ���� ����
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


                sTempRemain = sDestRemain;
                
                sSize = mtl::getOneCharSize( sSourceIndex,
                                             sSourceFence,
                                             sSrcCharSet );

                sIsSpaceSame = mtc::compareOneChar( sSourceIndex,
                                                    sSize,
                                                    sSrcCharSet->specialCharSet[MTL_SP_IDX],
                                                    sSrcCharSet->specialCharSize );
                
                sIsTabSame = mtc::compareOneChar( sSourceIndex,
                                                  sSize,
                                                  sSrcCharSet->specialCharSet[MTL_TB_IDX],
                                                  sSrcCharSet->specialCharSize );
                
                sIsNewLineSame = mtc::compareOneChar( sSourceIndex,
                                                      sSize,
                                                      sSrcCharSet->specialCharSet[MTL_NL_IDX],
                                                      sSrcCharSet->specialCharSize );
                
                if( (sIsSpaceSame == ID_TRUE) ||
                    (sIsTabSame == ID_TRUE) ||
                    (sIsNewLineSame == ID_TRUE) )
                {
                    // ����, ��, ���� ������ ���, �״�� ����
                    sResultValue[0] = sSourceIndex[0];
                    sResultValue[1] = sSourceIndex[1];
                    sCharStatus = 0;
                }
                else
                {
                    if( sCharStatus == 0 )
                    {
                        mtl::getUTF16UpperStr( (mtlU16Char*)sResultValue,
                                               (mtlU16Char*)sSourceIndex );
                        sCharStatus = 1;
                    }
                    else
                    {
                        mtl::getUTF16LowerStr( (mtlU16Char*)sResultValue,
                                               (mtlU16Char*)sSourceIndex );
                    }
                }

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

                // PR-13399�� ���� ���� ������ oracle�� �����ϰ� ��ü.
                if( *sSourceIndex == ' ' ||
                    *sSourceIndex == '\t' ||
                    *sSourceIndex == '\n' )
                {
                    // ����, ��, ���� ������ ���, �״�� ����
                    *sResultValue = *sSourceIndex;
                    sCharStatus = 0;

                    sDestRemain -= 1;
                }
                else if( isalnum( *sSourceIndex ) == 0 )
                {
                    if( IDN_IS_ASCII( *sSourceIndex ) == ID_TRUE )
                    {
                        *sResultValue = *sSourceIndex;
                        sCharStatus = 0;
                        sDestRemain -= 1;
                    }
                    else
                    {
                        // 7bit ASCII�� �ƴ� ����
                        // ��ҹ��� ��ȯǥ�� ���� ��ȯ�� �ʿ��ϴ�.

                        // --------------------------------------
                        // �Ʒ��� ���� �۾��� �ʿ��ϴ�.
                        // 1. SrcCharSet => UTF16���� ��ȯ
                        // 2. ��ҹ��� ��ȯǥ ����
                        // 3. UTF16 => SrcCharSet���� ��ȯ
                        // --------------------------------------

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

                            if ( sCharStatus == 0 )
                            {
                                mtl::getUTF16UpperStr( &sCaseResult, &sU16Result );
                                sCharStatus = 1;
                            }
                            else
                            {
                                mtl::getUTF16LowerStr( &sCaseResult, &sU16Result );
                            }

                            // 3. DestCharSet(UTF16) => SrcCharSet(UTF8)���� ��ȯ
                            IDE_TEST( convertCharSet( sIdnDestCharSet,
                                                      sIdnSrcCharSet,
                                                      & sCaseResult,
                                                      MTL_UTF16_PRECISION,
                                                      sResultValue,
                                                      & sDestRemain,
                                                      -1 /* mNlsNcharConvExcp */ )
                                      != IDE_SUCCESS );
                        }
                    }
                }
                else if( isalnum( *sSourceIndex ) != 0 )
                {
                    if( sCharStatus == 0 )
                    {
                        if( *sSourceIndex >= 'a' && *sSourceIndex <= 'z' )
                        {
                            *sResultValue = *sSourceIndex - 0x20;
                        }
                        else
                        {
                            // �״�� ����
                            *sResultValue = *sSourceIndex;
                        }
                        sCharStatus = 1;
                    }
                    else
                    {
                        if( *sSourceIndex >= 'A' && *sSourceIndex <= 'Z' )
                        {
                            *sResultValue = *sSourceIndex + 0x20;
                        }
                        else
                        {
                            // �״�� ����
                            *sResultValue = *sSourceIndex;
                        }
                    }
                    sDestRemain -= 1;
                }
                else
                {
                    // nothing to do
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
