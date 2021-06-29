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
 * $Id: mtfTrim.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>

extern mtfModule mtfTrim;

extern mtdModule mtdVarchar;
extern mtdModule mtdNvarchar;

static mtcName mtfTrimFunctionName[1] = {
    { NULL, 4, (void*)"TRIM" }
};

static IDE_RC mtfTrimEstimate( mtcNode*     aNode,
                               mtcTemplate* aTemplate,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               mtcCallBack* aCallBack );

mtfModule mtfTrim = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfTrimFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfTrimEstimate
};

static IDE_RC mtfTrimCalculateFor1Arg( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate );

static IDE_RC mtfTrimCalculateFor2Args( mtcNode*     aNode,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        void*        aInfo,
                                        mtcTemplate* aTemplate );

static IDE_RC mtfTrimCalculateNcharFor1Arg( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate );

static IDE_RC mtfTrimCalculateNcharFor2Args( mtcNode*     aNode,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        void*        aInfo,
                                        mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTrimCalculateFor1Arg,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTrimCalculateFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTrimCalculateNcharFor1Arg,
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
    mtfTrimCalculateNcharFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfTrimEstimate( mtcNode*     aNode,
                        mtcTemplate* aTemplate,
                        mtcStack*    aStack,
                        SInt      /* aRemain */,
                        mtcCallBack* aCallBack )
{
    const mtdModule* sModules[2];

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 1 ||
                    ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );

    sModules[1] = sModules[0];

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );


    // PROJ-1579 NCHAR
    if( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
        (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
    {
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteNcharFor1Arg;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteNcharFor2Args;
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdNvarchar,
                                         1,
                                         aStack[1].column->precision,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteFor1Arg;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteFor2Args;
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdVarchar,  // BUG-16501
                                         1,
                                         aStack[1].column->precision,
                                         0 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLeftRightTrim( const mtlModule* aLanguage,
                         UChar*           aResult,
                         UShort           aResultMax,
                         UShort*          aResultLen,
                         const UChar*     aSource,
                         UShort           aSourceLen,
                         const UChar*     aTrim,
                         UShort           aTrimLen )
{
/***********************************************************************
 *
 * Description : Trim
 *
 * Implementation :
 *
 ***********************************************************************/
    
    UChar  * sStart;
    UChar  * sLast;
    UChar  * sSourceIndex;
    UChar  * sSourceFence;
    UChar  * sCurSourceIndex;
    UChar  * sTrimIndex;
    UChar  * sTrimFence;
    idBool   sIsSame = ID_FALSE;
    UChar    sSourceSize;
    UChar    sTrimSize;

    //----------------------------------------
    // left trim ���� ����� �ƴ� ù ���ڸ� ã��
    //----------------------------------------
    
    sSourceIndex = (UChar*) aSource;
    sSourceFence = (UChar*) aSource + aSourceLen;

    *aResultLen = 0;

    sStart = sSourceIndex;
    sLast = sSourceFence;
    
    while ( sSourceIndex < sSourceFence )
    {
        sCurSourceIndex = sSourceIndex;
        
        sTrimIndex = (UChar*) aTrim;
        sTrimFence = (UChar*) aTrim + aTrimLen;

        sSourceSize =  mtl::getOneCharSize( sCurSourceIndex,
                                            sSourceFence,
                                            aLanguage ); 

        while ( sTrimIndex < sTrimFence )
        {
            sTrimSize =  mtl::getOneCharSize( sTrimIndex,
                                              sTrimFence,
                                              aLanguage );

            sIsSame = mtc::compareOneChar( sCurSourceIndex,
                                           sSourceSize,
                                           sTrimIndex,
                                           sTrimSize );
            
            if ( sIsSame == ID_TRUE )
            {
                // ������ ���ڰ� ����
                break;
            }
            else
            {
                // ���� trim ���ڷ� ����
                // TASK-3420 ���ڿ� ó�� ��å ����
                (void)aLanguage->nextCharPtr( & sTrimIndex, sTrimFence );
            }
        }

        if ( sIsSame == ID_FALSE )
        {
            sStart = sSourceIndex;
            break;
        }
        else
        {
            // ���� Source ���ڷ� ����
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );
        }
    }

    if ( ( sIsSame == ID_TRUE ) && ( sSourceIndex == sSourceFence ) )
    {
        // Source�� ��� ���ڰ� trim ���ڿ� ������ ���
    }
    else
    {
        //----------------------------------------
        // right trim ���� ����� �ƴ� ù ���ڸ� ã��
        //----------------------------------------

        while ( sSourceIndex < sSourceFence )
        {
            sTrimIndex = (UChar*) aTrim;
            sTrimFence = (UChar*) aTrim + aTrimLen;

            sSourceSize =  mtl::getOneCharSize( sSourceIndex,
                                                sSourceFence,
                                                aLanguage );
            
            while ( sTrimIndex < sTrimFence )
            {
                sTrimSize =  mtl::getOneCharSize( sTrimIndex,
                                                  sTrimFence,
                                                  aLanguage );

                sIsSame = mtc::compareOneChar( sSourceIndex,
                                               sSourceSize,
                                               sTrimIndex,
                                               sTrimSize );
            
                if ( sIsSame == ID_TRUE )
                {
                    // nothing to do
                    break;
                }
                else
                {
                    // ���� trim ���ڷ� ����
                    // TASK-3420 ���ڿ� ó�� ��å ����
                    (void)aLanguage->nextCharPtr( & sTrimIndex, sTrimFence );
                }
            }
            
            if ( sIsSame == ID_FALSE )
            {
                sLast = sSourceIndex;
            }
            else
            {
                // nothing to do
            }
        
            // ���� Source ���ڷ� ����
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );
        }
    }

    //----------------------------------------
    // left trim ����� �ƴ� ���ں���
    // right trim ����� �ƴ� ���ڱ��� ����� copy 
    //----------------------------------------

    if ( ( sIsSame == ID_TRUE ) && ( sLast == sSourceFence ) )
    {
        // Source�� ��� ���ڰ� trim ���ڿ� ���� ���
        *aResultLen = 0;
    }
    else
    {
        if ( sLast == sSourceFence )
        {
             sLast++;
        }
        else
        {
            // ���� Source ���ڷ� ����
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sLast, sSourceFence );
        }

        *aResultLen = sLast - sStart;
        
        IDE_TEST_RAISE( *aResultLen > aResultMax, ERR_EXCEED_MAX );
        
        idlOS::memcpy( aResult, sStart, *aResultLen );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }    

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLeftRightTrimFor1ByteSpace( const mtlModule* aLanguage,
                                      UChar*           aResult,
                                      UShort           aResultMax,
                                      UShort*          aResultLen,
                                      UChar*           aSource,
                                      UShort           aSourceLen )
{
/***********************************************************************
 *
 * Description : Trim
 *     BUG-10370
 *     1byte space 0x20�� ����ϴ� charset�� ���� trim�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    UChar  * sSourceIndex;
    UChar  * sSourceFence;
    UChar  * sSourceStart;
    UChar  * sSourceEnd;

    IDE_ASSERT_MSG( aLanguage->id != MTL_UTF16_ID,
                    "aLanguage->id : %"ID_UINT32_FMT"\n",
                    aLanguage->id );

    if ( aSourceLen > 0 )
    {
        //----------------------------------------
        // left trim ���� ����� �ƴ� ù ���ڸ� ã��
        //----------------------------------------
        
        sSourceIndex = aSource;
        sSourceFence = aSource + aSourceLen;

        for ( ; sSourceIndex < sSourceFence; sSourceIndex++ )
        {
            if ( *sSourceIndex != *aLanguage->specialCharSet[MTL_SP_IDX] )
            {
                // space�� �ƴ� ���
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        
        if ( sSourceIndex == sSourceFence )
        {
            // Source�� ��� ���ڰ� Trim ������ ���
            *aResultLen = 0;
        }
        else
        {
            //----------------------------------------
            // right trim ���� ����� �ƴ� ù ���ڸ� ã��
            //----------------------------------------
            
            sSourceStart = sSourceIndex;
            sSourceEnd = sSourceFence - 1;

            for ( ; sSourceStart <= sSourceEnd; sSourceEnd-- )
            {
                if ( *sSourceEnd != *aLanguage->specialCharSet[MTL_SP_IDX] )
                {
                    // space�� �ƴ� ���
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            if ( sSourceStart > sSourceEnd )
            {
                // Source�� ��� ���ڰ� Trim ������ ���
                *aResultLen = 0;
            }
            else
            {
                // Source�� ó�� ���ں��� SourceEnd���� ����
                *aResultLen = ( sSourceEnd - sSourceStart ) + 1;
                
                IDE_TEST_RAISE( *aResultLen > aResultMax, ERR_EXCEED_MAX );
                
                idlOS::memcpy( aResult, sSourceStart, *aResultLen );
            }
        }
    }
    else
    {
        *aResultLen = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }    

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLeftRightTrimFor2ByteSpace( const mtlModule* aLanguage,
                                      UChar*           aResult,
                                      UShort           aResultMax,
                                      UShort*          aResultLen,
                                      UChar*           aSource,
                                      UShort           aSourceLen )
{
/***********************************************************************
 *
 * Description : Trim
 *     BUG-10370
 *     byte space 0x00 0x20�� ����ϴ� charset�� ���� trim�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    UChar  * sSourceIndex;
    UChar  * sSourceFence;
    UChar  * sSourceStart;
    UChar  * sSourceEnd;

    IDE_ASSERT_MSG( aLanguage->id == MTL_UTF16_ID,
                    "aLanguage->id : %"ID_UINT32_FMT"\n",
                    aLanguage->id );

    if ( aSourceLen > 0 )
    {
        //----------------------------------------
        // left trim ���� ����� �ƴ� ù ���ڸ� ã��
        //----------------------------------------
        
        sSourceIndex = aSource;
        sSourceFence = aSource + aSourceLen;

        for ( ; sSourceIndex + 1 < sSourceFence; sSourceIndex += 2 )
        {
            if ( *sSourceIndex !=
                 ((mtlU16Char*)aLanguage->specialCharSet[MTL_SP_IDX])->value1 )
            {
                // space�� �ƴ� ���
                break;
            }
            else
            {
                // Nothing to do.
            }
            
            if ( *(sSourceIndex + 1) !=
                 ((mtlU16Char*)aLanguage->specialCharSet[MTL_SP_IDX])->value2 )
            {
                // space�� �ƴ� ���
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        
        if ( sSourceIndex == sSourceFence )
        {
            // Source�� ��� ���ڰ� Trim ������ ���
            *aResultLen = 0;
        }
        else
        {
            //----------------------------------------
            // right trim ���� ����� �ƴ� ù ���ڸ� ã��
            //----------------------------------------
            
            sSourceStart = sSourceIndex;
            sSourceEnd = sSourceFence - 1;

            for ( ; sSourceStart + 1 <= sSourceEnd; sSourceEnd -= 2 )
            {
                if ( *sSourceEnd !=
                     ((mtlU16Char*)aLanguage->specialCharSet[MTL_SP_IDX])->value2 )
                {
                    // space�� �ƴ� ���
                    break;
                }
                else
                {
                    // Nothing to do.
                }

                if ( *(sSourceEnd - 1) !=
                     ((mtlU16Char*)aLanguage->specialCharSet[MTL_SP_IDX])->value1 )
                {
                    // space�� �ƴ� ���
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            if ( sSourceStart > sSourceEnd )
            {
                // Source�� ��� ���ڰ� Trim ������ ���
                *aResultLen = 0;
            }
            else
            {
                // Source�� ó�� ���ں��� SourceEnd���� ����
                *aResultLen = ( sSourceEnd - sSourceStart ) + 1;
                
                IDE_TEST_RAISE( *aResultLen > aResultMax, ERR_EXCEED_MAX );
                
                idlOS::memcpy( aResult, sSourceStart, *aResultLen );
            }
        }
    }
    else
    {
        *aResultLen = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }    

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfTrimCalculateFor1Arg( mtcNode*     aNode,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                void*        aInfo,
                                mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Trim Calculate with 1 argument
 *
 * Implementation :
 *    TRIM( char1 )
 *
 *    aStack[0] : �Է��� ���ڿ��� ��������� ' '�� ��� ������ ��
 *    aStack[1] : char1 ( �Է� ���ڿ� )
 *
 *    ex) TRIM(' ab ' ) ==> 'ab'
 *
 ***********************************************************************/
    
    mtdCharType*   sResult;
    mtdCharType*   sVarchar;
    
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
        sResult   = (mtdCharType*)aStack[0].value;
        sVarchar = (mtdCharType*)aStack[1].value;
        
        IDE_TEST( mtfLeftRightTrimFor1ByteSpace( aStack[1].column->language,
                                                 sResult->value,
                                                 aStack[0].column->precision,
                                                 &sResult->length,
                                                 sVarchar->value,
                                                 sVarchar->length )
                  != IDE_SUCCESS );

        if ( sResult->length == 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // nothing to do
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfTrimCalculateFor2Args( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Trim Calculate with 2 arguments
 *
 * Implementation :
 *    TRIM( char1, char2 )
 *
 *    aStack[0] : �Է��� ���ڿ��� ��������� char2�� ���Ͽ� ������
 *                �� ������ ���ڸ� ������ ��
 *    aStack[1] : char1 ( �Է� ���ڿ� )
 *    aStack[2] : char1 ( trim ��� ���� )
 *
 *    ex) TRIM( 'abAabBba', 'ab' ) ==> 'AabB'
 *
 ***********************************************************************/
    
    mtdCharType* sResult;
    mtdCharType* sVarchar1;
    mtdCharType* sVarchar2;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult    = (mtdCharType*)aStack[0].value;
        sVarchar1 = (mtdCharType*)aStack[1].value;
        sVarchar2 = (mtdCharType*)aStack[2].value;
        
        IDE_TEST( mtfLeftRightTrim( aStack[1].column->language,
                                    sResult->value,
                                    aStack[0].column->precision,
                                    &sResult->length,
                                    (const UChar *)sVarchar1->value,
                                    sVarchar1->length,
                                    (const UChar *)sVarchar2->value,
                                    sVarchar2->length )
                  != IDE_SUCCESS );

        if ( sResult->length == 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // nothing to do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfTrimCalculateNcharFor1Arg( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Trim Calculate with 1 argument for NCHAR
 *
 * Implementation :
 *    TRIM( char1 )
 *
 *    aStack[0] : �Է��� ���ڿ��� ��������� ' '�� ��� ������ ��
 *    aStack[1] : char1 ( �Է� ���ڿ� )
 *
 *    ex) TRIM(' ab ' ) ==> 'ab'
 *
 ***********************************************************************/
    
    mtdNcharType    * sResult;
    mtdNcharType    * sSource;
    const mtlModule * sSrcCharSet;
    UShort            sResultMaxLen;
    
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
        sResult  = (mtdNcharType*)aStack[0].value;
        sSource  = (mtdNcharType*)aStack[1].value;

        sSrcCharSet = aStack[1].column->language;

        sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

        // ------------------------------
        // Trim ���� �Լ�
        // ------------------------------
        
        if( sSrcCharSet->id == MTL_UTF16_ID )
        {
            IDE_TEST( mtfLeftRightTrimFor2ByteSpace( aStack[1].column->language,
                                                     sResult->value,
                                                     sResultMaxLen,
                                                     &sResult->length,
                                                     sSource->value,
                                                     sSource->length )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( mtfLeftRightTrimFor1ByteSpace( aStack[1].column->language,
                                                     sResult->value,
                                                     sResultMaxLen,
                                                     &sResult->length,
                                                     sSource->value,
                                                     sSource->length )
                      != IDE_SUCCESS );
        }

        if ( sResult->length == 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // nothing to do
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfTrimCalculateNcharFor2Args( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Trim Calculate with 2 arguments for NCHAR
 *
 * Implementation :
 *    TRIM( char1, char2 )
 *
 *    aStack[0] : �Է��� ���ڿ��� ��������� char2�� ���Ͽ� ������
 *                �� ������ ���ڸ� ������ ��
 *    aStack[1] : char1 ( �Է� ���ڿ� )
 *    aStack[2] : char1 ( trim ��� ���� )
 *
 *    ex) TRIM( 'abAabBba', 'ab' ) ==> 'AabB'
 *
 ***********************************************************************/
    
    mtdNcharType    * sResult;
    mtdNcharType    * sSource;
    mtdNcharType    * sTrim;
    const mtlModule * sSrcCharSet;
    UShort            sResultMaxLen;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult = (mtdNcharType*)aStack[0].value;
        sSource = (mtdNcharType*)aStack[1].value;
        sTrim   = (mtdNcharType*)aStack[2].value;

        sSrcCharSet = aStack[1].column->language;

        sResultMaxLen = sSrcCharSet->maxPrecision(aStack[0].column->precision);

        // ------------------------------
        // LeftTrim ���� �Լ�
        // ------------------------------

        IDE_TEST( mtfLeftRightTrim( aStack[1].column->language,
                                    sResult->value,
                                    sResultMaxLen,
                                    &sResult->length,
                                    (const UChar *)sSource->value,
                                    sSource->length,
                                    (const UChar *)sTrim->value,
                                    sTrim->length )
                  != IDE_SUCCESS );

        if ( sResult->length == 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // nothing to do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
