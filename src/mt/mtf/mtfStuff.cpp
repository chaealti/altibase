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
 * $Id: mtfStuff.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

extern mtfModule mtfStuff;

extern mtdModule mtdInteger;

static mtcName mtfStuffFunctionName[1] = {
    { NULL, 5, (void*)"STUFF" }
};

static IDE_RC mtfStuffEstimate( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                mtcCallBack* aCallBack );

mtfModule mtfStuff = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfStuffFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfStuffEstimate
};

static IDE_RC mtfStuffCalculate( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate );

const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfStuffCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfStuffEstimate( mtcNode*     aNode,
                         mtcTemplate* aTemplate,
                         mtcStack*    aStack,
                         SInt      /* aRemain */,
                         mtcCallBack* aCallBack )
{
    const mtdModule* sModules[4];
    SInt             sPrecision;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 4,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );

    sModules[1] = &mtdInteger;
    sModules[2] = &mtdInteger;
    sModules[3] = sModules[0];

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
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

        if( aStack[1].column->language->id == MTL_UTF16_ID )
        {
            sPrecision = (UShort)idlOS::floor(4000 / MTL_UTF16_PRECISION);
        }
        else
        {
            sPrecision = (UShort)idlOS::floor(4000 / MTL_UTF8_PRECISION);
        }
    }
    else
    {
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

        sPrecision = 4000;
    }

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sModules[0],
                                     1,
                                     sPrecision,
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

IDE_RC mtfStuffCalculate( mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Stuff Calculate
 *
 * Implementation : 
 *    STUFF ( char1, start, length, char2 )
 *
 *    aStack[0] : char1�� start���� length��ŭ ����� �� ���̿� char2�� ������ ���ڿ�
 *    aStack[1] : char1 ( ���ڰ� )
 *    aStack[2] : start ( ���ڰ� )
 *    aStack[3] : length ( ���ڰ� )
 *    aStack[4] : char2 ( ���ڰ� )
 * 
 *    Ex) STUFF ('ABCDE', 3, 2, 'FGHK') ==> ABFGHKE
 *
 ***********************************************************************/
    
    const mtlModule   * sLanguage;
    mtdCharType       * sResult;
    mtdCharType       * sString1;
    mtdCharType       * sString2;
    SInt                sLength = 0;
    SInt                sStart = 0;
    UChar             * sIndex;
    UChar             * sFence;
    UChar             * sStartIndex;
    UChar             * sEndIndex;
    UShort              sResultIndex = 0;
    UShort              sString1CharCount = 0;
    UShort              sString2CharCount = 0;
    UShort              i = 0;
    idBool              sIsAllRemove = ID_FALSE;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column, 
                                           aStack[2].value ) == ID_TRUE) ||
        (aStack[3].column->module->isNull( aStack[3].column,
                                           aStack[3].value ) == ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sLanguage = aStack[1].column->language;
        sResult = (mtdCharType*)aStack[0].value;
        sString1 = (mtdCharType*)aStack[1].value;
        sStart = *(mtdIntegerType*)aStack[2].value;
        sLength = *(mtdIntegerType*)aStack[3].value;
        sString2 = (mtdCharType*)aStack[4].value;
        sResultIndex = 0;

        IDE_TEST_RAISE( sLength < 0,
                        ERR_ARGUMENT3_VALUE_OUT_OF_RANGE ); 

        sString1CharCount = 0;
        sIndex            = sString1->value;
        sFence            = sIndex + sString1->length;

        // string1�� ���ڿ��� ����( = ���ڰ��� )�� ����
        while ( sIndex < sFence )
        {
            (void)sLanguage->nextCharPtr( & sIndex, sFence );
            
            sString1CharCount++;
        }

        //fix BUG-18161
        IDE_TEST_RAISE( ( sStart > sString1CharCount + 1 ) ||
                        ( sStart < 1 ),
                        ERR_ARGUMENT2_VALUE_OUT_OF_RANGE );

        // BUG-25914
        // stuff ���� ����� �������� precision�� ���� �� ����.
        if( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
            (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
        {
            //---------------------------------------------------------------------
            // case 1. nchar / nvarchar
            // 
            // start/length�� nchar/nvarchar�� precision�� �ǹ̰� ���� ������
            // stuff����� ���ڼ��� �ִ� precision�� ���Ѵ�.
            //                                                    
            // ����)                                  ���     / precision
            //       stuff('abcde', 1, 3, 'xx')     => xxde     |     4      
            //       stuff('abcde', 1, 5, 'xx')     => xx       |     2   
            //       stuff('abcde', 3, 5, 'xx')     => abcxx    |     5   
            //       stuff('abcde', 6, 3, 'xx')     => abcdexx  |     7  
            //       stuff('abcde', 1, 5, '')       => abcdexx  |     7     
            //       stuff('��ab��', 1, 2, '�ٶ�') => b�ٶ� |     4
            //       stuff('��ab��', 2, 3, '�ٶ�') => ���ٶ�  |     3
            //       stuff('��ab��', 2, 8, '�ٶ�') => ���ٶ�  |     3
            //---------------------------------------------------------------------
            
            sString2CharCount = 0;
            sIndex            = sString2->value;
            sFence            = sIndex + sString2->length;

            // string2�� ���ڿ��� ����( = ���ڰ��� )�� ����.
            while ( sIndex < sFence )
            {
                (void)sLanguage->nextCharPtr( & sIndex, sFence );
            
                sString2CharCount++;
            }

            IDE_TEST_RAISE( ( sString1CharCount -
                              IDL_MIN( sString1CharCount - sStart + 1, sLength)
                              + sString2CharCount ) >
                            aStack[0].column->precision,
                            ERR_INVALID_LENGTH );
        }
        else
        {
            //---------------------------------------------------------------------
            // case 2. char / varchar
            // 
            // char/varchar�� precision�� byte�� �ǹ��ϱ� ������
            // stuff����� byte���� �ִ� precision�� ���Ѵ�.
            //                                                    
            // ����)                                  ���     / precision
            //       stuff('abcde', 1, 3, 'xx')     => xxde     |     4      
            //       stuff('abcde', 1, 5, 'xx')     => xx       |     2   
            //       stuff('abcde', 3, 5, 'xx')     => abcxx    |     5   
            //       stuff('abcde', 6, 3, 'xx')     => abcdexx  |     7  
            //       stuff('abcde', 1, 5, '')       => abcdexx  |     7     
            //       stuff('��ab��', 1, 2, '�ٶ�') => b�ٶ� |     7 
            //       stuff('��ab��', 2, 3, '�ٶ�') => ���ٶ�  |     6
            //       stuff('��ab��', 2, 8, '�ٶ�') => ���ٶ�  |     6 
            //---------------------------------------------------------------------

            sIndex        = sString1->value;
            sFence        = sIndex + sString1->length;
            sResultIndex  = 0;
            sStartIndex   = sString1->value;
            sEndIndex     = sString1->value;
            
            // char1���� ���� ������ byte���̸� ����.
            for( i = 1; sIndex < sFence ; i++ )
            {
                (void)sLanguage->nextCharPtr( & sIndex, sFence );
                
                if( i == sStart )
                {
                    sStartIndex = sIndex;
                }
                else
                {
                    // Nothing to do.
                }

                if( (i == sStart + sLength) || (sIndex == sFence) )
                {
                    sEndIndex = sIndex;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            IDE_TEST_RAISE( ( sString1->length - 
                              (sEndIndex - sStartIndex)
                              + sString2->length ) >
                            aStack[0].column->precision,
                            ERR_INVALID_LENGTH );
        }
         
        // 1. char1�� �ڿ� char2�� ������ ��� 
        // (sStart == sString1->length + 1, sLength�� �������)
        if ( sStart == sString1CharCount + 1 )
        {
            sIndex = 0;
            sResultIndex = 0;

            idlOS::memcpy ( sResult->value,
                            sString1->value,
                            sString1->length );

            sResultIndex = sResultIndex + sString1->length;

            idlOS::memcpy ( sResult->value + sResultIndex,
                            sString2->value,
                            sString2->length );

            sResult->length = sString1->length + sString2->length; 
        }
        // 2. char1���� start ���ʿ� char2�� ������ ��� (sStart > 0)
        else if ( sStart > 0 )
        {
            sIndex = sString1->value;
            sFence = sIndex + sString1->length;
            sResultIndex = 0;

            // char1���� start-1��ŭ ������ ���� ��, start-1������ ���ڸ� memcpy�Ѵ�.
            for ( i = 0; i < sStart - 1; i++ )
            {
                (void)sLanguage->nextCharPtr( & sIndex, sFence );
            }

            // char1���� 1 ~ start-1���� ����
            idlOS::memcpy ( sResult->value + sResultIndex,
                            sString1->value,
                            sIndex - sString1->value );
            
            sResultIndex += sIndex - sString1->value;
            
            // ������ length�� char1�� �ʰ��ϸ�
            // start���� char1�� ������ ���ڱ����� ���� ������ �缳��
            if ( sLength > sString1CharCount - sStart + 1)
            {
                sLength = sString1CharCount - sStart + 1;
            }
            
            // length ��ŭ �ǳʶڴ�.
            for ( i = 0; i < sLength; i++ )
            {
                (void)sLanguage->nextCharPtr( & sIndex, sFence );
                
                // char1�� ������ �����ϸ� sIsAllRemove flag ����
                if ( sIndex >= sFence )
                {
                    sIsAllRemove = ID_TRUE;
                }
            }
            
            // char2 ���� ����
            idlOS::memcpy (sResult->value + sResultIndex,
                           sString2->value,
                           sString2->length);
            
            sResultIndex = sResultIndex + sString2->length;
            
            // char1 �����ִ� ���ڰ� �ִ� ���
            if ( sIsAllRemove == ID_FALSE )
            {
                idlOS::memcpy (sResult->value + sResultIndex,
                               sIndex,
                               sString1->length - (sIndex - sString1->value) );
            }
            
            sResult->length = sResultIndex +
                sString1->length - (sIndex - sString1->value);
        }
        else
        {
            //nothing to do
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    }
    
    IDE_EXCEPTION( ERR_ARGUMENT2_VALUE_OUT_OF_RANGE );
    {        
        IDE_SET( ideSetErrorCode(mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                                 sStart ) );
    }    

    IDE_EXCEPTION( ERR_ARGUMENT3_VALUE_OUT_OF_RANGE );
    {        
        IDE_SET( ideSetErrorCode(mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                                 sLength ) );
    }    

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
