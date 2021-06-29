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
 * $Id: mtfSubstring.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

extern mtfModule mtfSubstring;

extern mtdModule mtdInteger;
extern mtdModule mtdBigint;
extern mtdModule mtdVarchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdClob;

static mtcName mtfSubstringFunctionName[2] = {
    { mtfSubstringFunctionName+1, 9, (void*)"SUBSTRING" },
    { NULL,              6, (void*)"SUBSTR"    }
};

static IDE_RC mtfSubstringEstimate( mtcNode*     aNode,
                                    mtcTemplate* aTemplate,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    mtcCallBack* aCallBack );

mtfModule mtfSubstring = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfSubstringFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfSubstringEstimate
};

static IDE_RC mtfSubstringCalculateFor2Args( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate );

static IDE_RC mtfSubstringCalculateFor3Args( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate );

static IDE_RC mtfSubstringCalculateXlobLocatorFor2Args( mtcNode*     aNode,
                                                        mtcStack*    aStack,
                                                        SInt         aRemain,
                                                        void*        aInfo,
                                                        mtcTemplate* aTemplate );

static IDE_RC mtfSubstringCalculateXlobLocatorFor3Args( mtcNode*     aNode,
                                                        mtcStack*    aStack,
                                                        SInt         aRemain,
                                                        void*        aInfo,
                                                        mtcTemplate* aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
static IDE_RC mtfSubstringCalculateClobValueFor2Args( mtcNode     * aNode,
                                                      mtcStack    * aStack,
                                                      SInt          aRemain,
                                                      void        * aInfo,
                                                      mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
static IDE_RC mtfSubstringCalculateClobValueFor3Args( mtcNode     * aNode,
                                                      mtcStack    * aStack,
                                                      SInt          aRemain,
                                                      void        * aInfo,
                                                      mtcTemplate * aTemplate );

static IDE_RC mtfSubstringCalculateNcharFor3Args( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubstringCalculateFor2Args,
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
    mtfSubstringCalculateFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteXlobLocatorFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubstringCalculateXlobLocatorFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteXlobLocatorFor3Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubstringCalculateXlobLocatorFor3Args,
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
    mtfSubstringCalculateNcharFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecuteClobValueFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubstringCalculateClobValueFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecuteClobValueFor3Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubstringCalculateClobValueFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfSubstringEstimate( mtcNode*     aNode,
                             mtcTemplate* aTemplate,
                             mtcStack*    aStack,
                             SInt      /* aRemain */,
                             mtcCallBack* aCallBack )
{
    const mtdModule* sModules[3];
    SInt             sPrecision;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 2 ||
                    ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 3,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    if ( aStack[1].column->module->id == MTD_CLOB_LOCATOR_ID )
    {
        // PROJ-1362
        sModules[0] = &mtdVarchar;
        sModules[1] = &mtdBigint;
        sModules[2] = &mtdBigint;

        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments->next,
                                            aTemplate,
                                            aStack + 2,
                                            aCallBack,
                                            sModules + 1 )
                  != IDE_SUCCESS );

        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteXlobLocatorFor2Args;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteXlobLocatorFor3Args;
        }

        // fix BUG-25560
        // precison�� clob�� �ִ� ũ��� ����
        /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
         * Lob Locator�� VARCHAR�� ������ �ִ� ũ��� ����
         */
        sPrecision = MTD_VARCHAR_PRECISION_MAXIMUM;
    }
    else if ( aStack[1].column->module->id == MTD_CLOB_ID )
    {
        if ( aTemplate->isBaseTable( aTemplate, aNode->arguments->table ) == ID_TRUE )
        {
            // PROJ-1362
            IDE_TEST( mtf::getLobFuncResultModule( &sModules[0],
                                                   aStack[1].column->module )
                      != IDE_SUCCESS );
            sModules[1] = &mtdBigint;
            sModules[2] = &mtdBigint;

            IDE_TEST( mtf::makeConversionNodes( aNode,
                                                aNode->arguments,
                                                aTemplate,
                                                aStack + 1,
                                                aCallBack,
                                                sModules )
                      != IDE_SUCCESS );

            if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteXlobLocatorFor2Args;
            }
            else
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteXlobLocatorFor3Args;
            }

            // fix BUG-25560
            // precison�� clob�� �ִ� ũ��� ����
            /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
             * Lob Column�̸� VARCHAR�� ������ �ִ� ũ��� �����Ѵ�.
             */
            sPrecision = MTD_VARCHAR_PRECISION_MAXIMUM;
        }
        else
        {
            sModules[0] = &mtdClob;
            sModules[1] = &mtdBigint;
            sModules[2] = &mtdBigint;

            IDE_TEST( mtf::makeConversionNodes( aNode,
                                                aNode->arguments,
                                                aTemplate,
                                                aStack + 1,
                                                aCallBack,
                                                sModules )
                      != IDE_SUCCESS );

            if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteClobValueFor2Args;
            }
            else
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteClobValueFor3Args;
            }

            // fix BUG-25560
            // precison�� clob�� �ִ� ũ��� ����
            /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
             * Lob Value�̸� VARCHAR�� �ִ� ũ��� Lob Value�� ũ�� �� ���� ������ �����Ѵ�.
             */
            sPrecision = IDL_MIN( MTD_VARCHAR_PRECISION_MAXIMUM,
                                  aStack[1].column->precision );
        }
    }
    else
    {
        IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                                aStack[1].column->module )
                  != IDE_SUCCESS );
        sModules[1] = &mtdInteger;
        sModules[2] = &mtdInteger;

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
            if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                        mtfExecuteFor2Args;
            }
            else
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                        mtfExecuteNcharFor3Args;
            }
        }
        else
        {
            if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                        mtfExecuteFor2Args;
            }
            else
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                        mtfExecuteFor3Args;
            }
        }

        sPrecision = aStack[1].column->precision;
    }

    // PROJ-1579 NCHAR
    if( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
        (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
    {
        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdNvarchar,
                                         1,
                                         sPrecision,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdVarchar,  // BUG-16501
                                         1,
                                         sPrecision,
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

IDE_RC mtfSubstringCommon( const mtlModule * aLanguage,
                           mtcStack        * aStack,
                           UChar           * aResult,
                           UShort            aResultMaxLen,
                           UShort          * aResultLen,
                           UChar           * aSource,
                           UShort            aSourceLen,
                           SInt              aStart,
                           SInt              aLength )
{
/***********************************************************************
 *
 * Description : Substring
 *
 * Implementation :
 *
 ***********************************************************************/

    UChar           * sSourceIndex;
    UChar           * sSourceFence;
    UChar           * sStartIndex;
    UChar           * sEndIndex;
    UShort            sSourceLength;
    UShort            i;

    //-----------------------------------------
    // ��ü ���� ���� ����
    //-----------------------------------------        

    sSourceIndex = aSource;
    sSourceFence = sSourceIndex + aSourceLen;
    sSourceLength = 0;

    //-----------------------------------------
    // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
    //-----------------------------------------

    if( aStart > 0 )
    {
        // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
        // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
        // ���� ������ 0�����̴�.
        aStart--;
    }
    else if( aStart < 0 )
    {
        while ( sSourceIndex < sSourceFence )
        {
            (void)aLanguage->nextCharPtr( &sSourceIndex, sSourceFence );
            sSourceLength++;
        }
        aStart = aStart + sSourceLength;
    }
    else
    {
        // nothing to do
    }

    //-----------------------------------------
    // ��� ����
    //-----------------------------------------

    if ( aStart < 0 )
    {
        // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
        // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
        // - ������ ���ں��� ���� ���� ��ġ�� ū ���
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // ���� index�� ã��
        sStartIndex = aSource;

        for ( i = 0; i < aStart; i++ )
        {
            if ( sStartIndex < sSourceFence )
            {
                (void)aLanguage->nextCharPtr( &sStartIndex, sSourceFence );
            }
            else
            {
                /* Nothing to do */
            }

            /* BUG-42544 enhancement substring function */
            if ( sStartIndex >= sSourceFence )
            {
                aStack[0].column->module->null( aStack[0].column, aStack[0].value );
                IDE_CONT( normal_exit );
            }
            else
            {
                /* Nothing to do */
            }
        }

        // �� index�� ã��
        sEndIndex = sStartIndex;

        for ( i = 0 ; i < aLength ; i++ )
        {
            (void)aLanguage->nextCharPtr( &sEndIndex, sSourceFence );

            if ( sEndIndex == sSourceFence )
            {
                break;
            }
            else
            {
                // nothing to do
            }
        }

        //-----------------------------------------
        // start position���� n���� ���� string�� copy�Ͽ� ����� ����
        //-----------------------------------------
        
        *aResultLen = sEndIndex - sStartIndex;
        
        IDE_TEST_RAISE( *aResultLen > aResultMaxLen,
                        ERR_EXCEED_MAX );
            
        idlOS::memcpy( aResult,
                       sStartIndex,
                       *aResultLen );
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }    
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}



IDE_RC mtfSubstringCalculateFor2Args( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Substring Calculate
 *
 * Implementation :
 *    SUBSTR( string, m )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� ������ ��� ���� ��ȯ
 *    aStack[1] : string ( �Էµ� ���ڿ� )
 *    aStack[2] : n ( ������ )
 *
 *    ex ) SUBSTR( 'SALESMAN', 6 ) ==> 'MAN'
 *
 ***********************************************************************/
    
    mtdCharType     * sResult;
    mtdCharType     * sSource;
    mtdIntegerType    sStart;
    UChar           * sStartIndex;
    const mtlModule * sLanguage;
    UChar           * sSourceIndex;
    UChar           * sSourceFence;
    UShort            sSourceLength = 0;
    UShort            i;

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
        sResult   = (mtdCharType*)aStack[0].value;
        sSource   = (mtdCharType*)aStack[1].value;
        sStart    = *(mtdIntegerType*)aStack[2].value;
        sLanguage = aStack[1].column->language;

        //-----------------------------------------
        // ��ü ���� ���� ����
        //-----------------------------------------        

        sSourceIndex = sSource->value;
        sSourceFence = sSourceIndex + sSource->length;

        sSourceLength = 0;

        //-----------------------------------------
        // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
        //-----------------------------------------

        if( sStart > 0 )
        {
            // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else if( sStart < 0 )
        {
            while ( sSourceIndex < sSourceFence )
            {
                (void)sLanguage->nextCharPtr( &sSourceIndex, sSourceFence );
                sSourceLength++;
            }
            // ������ ���
            sStart = sStart + sSourceLength;
        }
        else
        {
            // nothing to do
        }

        //-----------------------------------------
        // ��� ����
        //-----------------------------------------
        
        if ( sStart < 0 )
        {
            // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
            // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
            // - ������ ���ں��� ���� ���� ��ġ�� ū ���
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // ���� ���� ��ġ�� ���� ��ü �������� �۰ų� ����  ���
            
            // ���� index�� ã��
            sStartIndex = sSource->value;
       
            for ( i = 0; i < sStart; i++ )
            {
                if ( sStartIndex < sSourceFence )
                {
                    (void)sLanguage->nextCharPtr( &sStartIndex, sSourceFence );
                }
                else
                {
                    /* Nothing to do */
                }

                /* BUG-42544 enhancement substring function */
                if ( sStartIndex >= sSourceFence )
                {
                    aStack[0].column->module->null( aStack[0].column, aStack[0].value );
                    IDE_CONT( normal_exit );
                }
                else
                {
                    /* Nothing to do */
                }
            }

            //-----------------------------------------
            // ���� index���� �������� string�� copy�Ͽ� ����� ����
            //-----------------------------------------
            
            sResult->length = sSource->length - ( sStartIndex - sSource->value );
            idlOS::memcpy( sResult->value,
                           sStartIndex,
                           sResult->length );
        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateFor3Args( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Substring Calculate
 *
 * Implementation :
 *    SUBSTR( string, m, n )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� n���� ���� ��ȯ
 *    aStack[1] : �Էµ� ���ڿ�
 *    aStack[2] : ���� ( eg. m )
 *    aStack[3] : ��ȯ ���� ����   ( eg. n )
 *
 *    ex) SUBSTR('SALESMAN', 1, 5 ) ==> 'SALES'
 *
 ***********************************************************************/
    
    mtdCharType     * sResult;
    mtdCharType     * sSource;
    mtdIntegerType    sStart;
    mtdIntegerType    sLength;
    //const mtlModule * sLanguage;

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
        sResult   = (mtdCharType*)aStack[0].value;
        sSource   = (mtdCharType*)aStack[1].value;
        sStart    = *(mtdIntegerType*)aStack[2].value;
        sLength   = *(mtdIntegerType*)aStack[3].value;
        //sLanguage = aStack[1].column->language;

        if( sLength <= 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
            return IDE_SUCCESS;
        }
        else
        {
            // nothing to do
        }

        IDE_TEST( mtfSubstringCommon( aStack[1].column->language,
                                      aStack,
                                      sResult->value,
                                      aStack[0].column->precision,
                                      &sResult->length,
                                      sSource->value,
                                      sSource->length,
                                      sStart,
                                      sLength )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateXlobLocatorFor2Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1362
 *               Clob Substring Calculate
 *
 * Implementation :
 *    SUBSTR( string, m )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� ������ ��� ���� ��ȯ
 *    aStack[1] : string ( �Էµ� ���ڿ� )
 *    aStack[2] : n ( ������ )
 *
 *    ex ) SUBSTR( 'SALESMAN', 6 ) ==> 'MAN'
 *
 ***********************************************************************/
    
    const mtlModule  * sLanguage;
    mtdCharType      * sResult;
    mtdBigintType      sStart;
    UInt               sStartIndex;
    UShort             sSize;
    UChar            * sIndex;
    UChar            * sIndexPrev;
    UChar            * sFence;    
    UChar            * sBufferFence;    
    UChar              sBuffer[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION];
    UInt               sBufferOffset;
    UInt               sBufferMount;
    UInt               sBufferSize;
    mtdClobLocatorType sLocator = MTD_LOCATOR_NULL;
    UInt               sLobLength;
    idBool             sIsNull;
    UInt               sReadLength;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;
    
    IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                        & sIsNull,
                                        & sLobLength,
                                        mtc::getStatistics(aTemplate) )
              != IDE_SUCCESS );
    
    if ( (sIsNull == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE ))
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sResult   = (mtdCharType*)aStack[0].value;
        sStart    = *(mtdBigintType*)aStack[2].value;
        sLanguage = aStack[1].column->language;
        
        sResult->length = 0;
        
        //-----------------------------------------
        // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
        //-----------------------------------------
        
        if( sStart > 0 )
        {
            // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else
        {
            // Nothing to do.
        }
        
        //-----------------------------------------
        // ��� ����
        //-----------------------------------------
        
        if ( ( sLobLength <= sStart ) || ( sStart < 0 ) )
        {
            // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
            // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // ���� index�� ã��
            sStartIndex = 0;
            sBufferOffset = 0;

            while ( sBufferOffset < sLobLength )
            {
                // ���۸� �д´�.
                if ( sBufferOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > sLobLength )
                {
                    sBufferMount = sLobLength - sBufferOffset;
                    sBufferSize = sBufferMount;
                }
                else
                {
                    sBufferMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
                    sBufferSize = MTC_LOB_BUFFER_SIZE;
                }
                
                //ideLog::log( IDE_QP_0, "[substring] offset=%d", sBufferOffset );
                
                IDE_TEST( mtc::readLob( mtc::getStatistics( aTemplate ), /* idvSQL* */
                                        sLocator,
                                        sBufferOffset,
                                        sBufferMount,
                                        sBuffer,
                                        &sReadLength )
                          != IDE_SUCCESS );
                
                // ���ۿ��� ���ڿ� ���̸� ���Ѵ�.
                sIndex = sBuffer;
                sFence = sIndex + sBufferSize;
                sBufferFence = sIndex + sBufferMount;
                
                while ( sIndex < sFence ) 
                {
                    sIndexPrev = sIndex;
                    
                    (void)sLanguage->nextCharPtr( & sIndex, sBufferFence );
                    
                    if ( sStartIndex >= sStart )
                    {
                        sSize = sIndex - sIndexPrev;
                        
                        IDE_TEST_RAISE( sResult->length + sSize >
                                        aStack[0].column->precision,
                                        ERR_EXCEED_MAX );
                        
                        // �����Ѵ�.
                        idlOS::memcpy( sResult->value + sResult->length,
                                       sIndexPrev,
                                       sSize );
                        sResult->length += sSize;
                    }
                    else
                    {
                        sStartIndex++;
                    }
                }
                
                sBufferOffset += ( sIndex - sBuffer );
            }

            if ( sStartIndex < sStart )
            {
                // ���� index�� ã�� ���ߴ�.
                aStack[0].column->module->null( aStack[0].column,
                                                aStack[0].value );
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    IDE_TEST( aTemplate->closeLobLocator( sLocator )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }

    IDE_EXCEPTION_END;

    (void) aTemplate->closeLobLocator( sLocator );
    
    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateXlobLocatorFor3Args( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1362
 *               Clob Substring Calculate
 *
 * Implementation :
 *    SUBSTR( string, m, n )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� n���� ���� ��ȯ
 *    aStack[1] : �Էµ� ���ڿ�
 *    aStack[2] : ���� ( eg. m )
 *    aStack[3] : ��ȯ ���� ����   ( eg. n )
 *
 *    ex) SUBSTR('SALESMAN', 1, 5 ) ==> 'SALES'
 *
 ***********************************************************************/
    
    const mtlModule  * sLanguage;
    mtdCharType      * sResult;
    mtdBigintType      sStart;
    mtdBigintType      sLength;
    UInt               sStartIndex;
    UInt               sEndIndex;
    UShort             sSize;
    UChar            * sIndex;
    UChar            * sIndexPrev;
    UChar            * sFence;    
    UChar            * sBufferFence;    
    UChar              sBuffer[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION];
    UInt               sBufferOffset;
    UInt               sBufferMount;
    UInt               sBufferSize;
    mtdClobLocatorType sLocator = MTD_LOCATOR_NULL;
    UInt               sLobLength;
    idBool             sIsNull;
    UInt               sReadLength;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;
    
    IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                        & sIsNull,
                                        & sLobLength,
                                        mtc::getStatistics(aTemplate) )
              != IDE_SUCCESS );
    
    if ( (sIsNull == ID_TRUE) ||
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
        sResult   = (mtdCharType*)aStack[0].value;
        sStart    = *(mtdBigintType*)aStack[2].value;
        sLength   = *(mtdBigintType*)aStack[3].value;
        sLanguage = aStack[1].column->language;
        
        sResult->length = 0;
        
        if( sLength <= 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            //-----------------------------------------
            // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
            //-----------------------------------------
        
            if( sStart > 0 )
            {
                // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
                // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
                // ���� ������ 0�����̴�.
                sStart--;
            }
            else
            {
                // Nothing to do.
            }
        
            //-----------------------------------------
            // ��� ����
            //-----------------------------------------
        
            if ( ( sLobLength <= sStart ) || ( sStart < 0 ) )
            {
                // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
                // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
                aStack[0].column->module->null( aStack[0].column,
                                                aStack[0].value );
            }
            else
            {
                // ���� index�� ã��
                sStartIndex = 0;
                sEndIndex = 0;
                sBufferOffset = 0;
                
                while ( sBufferOffset < sLobLength )
                {
                    // ���۸� �д´�.
                    if ( sBufferOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > sLobLength )
                    {
                        sBufferMount = sLobLength - sBufferOffset;
                        sBufferSize = sBufferMount;
                    }
                    else
                    {
                        sBufferMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
                        sBufferSize = MTC_LOB_BUFFER_SIZE;
                    }
                
                    //ideLog::log( IDE_QP_0, "[substring] offset=%d", sBufferOffset );
                    
                    IDE_TEST( mtc::readLob( mtc::getStatistics( aTemplate ), /* idvSQL* */
                                            sLocator,
                                            sBufferOffset,
                                            sBufferMount,
                                            sBuffer,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    // ���ۿ��� ���ڿ� ���̸� ���Ѵ�.
                    sIndex = sBuffer;
                    sFence = sIndex + sBufferSize;
                    sBufferFence = sIndex + sBufferMount;
                    
                    while ( sIndex < sFence ) 
                    {
                        sIndexPrev = sIndex;
                        
                        (void)sLanguage->nextCharPtr( & sIndex, sBufferFence );
                        
                        if ( sStartIndex >= sStart )
                        {
                            sSize = sIndex - sIndexPrev;
                            
                            IDE_TEST_RAISE( sResult->length + sSize >
                                            aStack[0].column->precision,
                                            ERR_EXCEED_MAX );
                            
                            // �����Ѵ�.
                            idlOS::memcpy( sResult->value + sResult->length,
                                           sIndexPrev,
                                           sSize );
                            sResult->length += sSize;

                            sEndIndex++;
                            
                            if ( sEndIndex >= sLength )
                            {
                                break;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        }
                        else
                        {
                            sStartIndex++;
                        }
                    }
                    
                    if ( sEndIndex >= sLength )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                            
                    sBufferOffset += ( sIndex - sBuffer );
                }
                
                if ( sStartIndex < sStart )
                {
                    // ���� index�� ã�� ���ߴ�.
                    aStack[0].column->module->null( aStack[0].column,
                                                    aStack[0].value );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }
    
    IDE_TEST( aTemplate->closeLobLocator( sLocator )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {        
        IDE_SET(ideSetErrorCode( idERR_ABORT_idnReachEnd ));
    }    
    
    IDE_EXCEPTION_END;

    (void) aTemplate->closeLobLocator( sLocator );
    
    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateNcharFor3Args( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Substring Calculate for NCHAR
 *
 * Implementation :
 *    SUBSTR( string, m, n )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� n���� ���� ��ȯ
 *    aStack[1] : �Էµ� ���ڿ�
 *    aStack[2] : ���� ( eg. m )
 *    aStack[3] : ��ȯ ���� ����   ( eg. n )
 *
 *    ex) SUBSTR('SALESMAN', 1, 5 ) ==> 'SALES'
 *
 ***********************************************************************/
    
    const mtlModule  * sLanguage;
    mtdNcharType     * sResult;
    mtdNcharType     * sSource;
    mtdIntegerType     sStart;
    mtdIntegerType     sLength;
    UShort             sResultMaxLen;

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
        sResult   = (mtdNcharType*)aStack[0].value;
        sSource   = (mtdNcharType*)aStack[1].value;
        sStart    = *(mtdIntegerType*)aStack[2].value;
        sLength   = *(mtdIntegerType*)aStack[3].value;
        sLanguage = aStack[1].column->language;

        if( sLength <= 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
            return IDE_SUCCESS;
        }
        else
        {
            // nothing to do
        }

        sResultMaxLen = sLanguage->maxPrecision(aStack[0].column->precision);

        IDE_TEST( mtfSubstringCommon( aStack[1].column->language,
                                      aStack,
                                      sResult->value,
                                      sResultMaxLen,
                                      &sResult->length,
                                      sSource->value,
                                      sSource->length,
                                      sStart,
                                      sLength )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateClobValueFor2Args( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Substring Calculate For CLOB Value
 *
 * Implementation :
 *    SUBSTR( clob_value, m )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� ������ ��� ���� ��ȯ
 *    aStack[1] : clob_value ( �Էµ� ���ڿ� )
 *    aStack[2] : m ( ������ )
 *
 *    ex ) SUBSTR( CLOB'SALESMAN', 6 ) ==> 'MAN'
 *
 ***********************************************************************/

    mtdCharType     * sResult;
    mtdClobType     * sSource;
    mtdBigintType     sStart;
    const mtlModule * sLanguage;
    UChar           * sStartIndex;
    UChar           * sSourceIndex;
    UChar           * sSourceFence;
    SLong             sSourceLength = 0;
    SLong             sCopySize;
    SLong             i;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
         aStack[0].column->module->null( aStack[0].column,
                                         aStack[0].value );
    }
    else
    {
        sResult   = (mtdCharType *)aStack[0].value;
        sSource   = (mtdClobType *)aStack[1].value;
        sStart    = *(mtdBigintType *)aStack[2].value;
        sLanguage = aStack[1].column->language;

        //-----------------------------------------
        // ��ü ���� ���� ����
        //-----------------------------------------

        sSourceIndex = sSource->value;
        sSourceFence = sSourceIndex + sSource->length;

        sSourceLength = 0;

        //-----------------------------------------
        // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
        //-----------------------------------------

        if ( sStart > 0 )
        {
            // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else if ( sStart < 0 )
        {
            while ( sSourceIndex < sSourceFence )
            {
                (void)sLanguage->nextCharPtr( &sSourceIndex, sSourceFence );
                sSourceLength++;
            }
            // ������ ���
            sStart = sStart + sSourceLength;
        }
        else
        {
            // nothing to do
        }

        //-----------------------------------------
        // ��� ����
        //-----------------------------------------

        if ( sStart < 0 )
        {
            // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
            // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
            // - ������ ���ں��� ���� ���� ��ġ�� ū ���
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // ���� ���� ��ġ�� ���� ��ü �������� �۰ų� ����  ���

            // ���� index�� ã��
            sStartIndex = sSource->value;

            for ( i = 0; i < sStart; i++ )
            {
                if ( sStartIndex < sSourceFence )
                {
                    (void)sLanguage->nextCharPtr( &sStartIndex, sSourceFence );
                }
                else
                {
                    /* Nothing to do */
                }

                /* BUG-42544 enhancement substring function */
                if ( sStartIndex >= sSourceFence )
                {
                    aStack[0].column->module->null( aStack[0].column, aStack[0].value );
                    IDE_CONT( normal_exit );
                }
                else
                {
                    /* Nothing to do */
                }
            }

            //-----------------------------------------
            // ���� index���� �������� string�� copy�Ͽ� ����� ����
            //-----------------------------------------

            sCopySize = sSource->length - (sStartIndex - sSource->value);
            IDE_TEST_RAISE( sCopySize > (SLong) aStack[0].column->precision,
                            ERR_EXCEED_MAX );

            sResult->length = (UShort)sCopySize;
            idlOS::memcpy( sResult->value,
                           sStartIndex,
                           sResult->length );
        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_idnReachEnd ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfSubstringCalculateClobValueFor3Args( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Substring Calculate For CLOB Value
 *
 * Implementation :
 *    SUBSTR( clob_value, m, n )
 *
 *    aStack[0] : �Էµ� ���ڿ��� m��° ���ں��� n���� ���� ��ȯ
 *    aStack[1] : clob_value ( �Էµ� ���ڿ� )
 *    aStack[2] : m ( ������ )
 *    aStack[3] : n ( ��ȯ ���� �� )
 *
 *    ex ) SUBSTR( CLOB'SALESMAN', 1, 5 ) ==> 'SALES'
 *
 ***********************************************************************/

    mtdCharType     * sResult;
    mtdClobType     * sSource;
    mtdBigintType     sStart;
    mtdBigintType     sLength;
    const mtlModule * sLanguage;
    UChar           * sStartIndex;
    UChar           * sEndIndex;
    UChar           * sSourceIndex;
    UChar           * sSourceFence;
    SLong             sSourceLength = 0;
    SLong             sCopySize;
    SLong             i;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
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
        sResult   = (mtdCharType *)aStack[0].value;
        sSource   = (mtdClobType *)aStack[1].value;
        sStart    = *(mtdBigintType *)aStack[2].value;
        sLength   = *(mtdBigintType *)aStack[3].value;
        sLanguage = aStack[1].column->language;

        if ( sLength <= 0 )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
            return IDE_SUCCESS;
        }
        else
        {
            // nothing to do
        }

        //-----------------------------------------
        // ��ü ���� ���� ����
        //-----------------------------------------

        sSourceIndex = sSource->value;
        sSourceFence = sSourceIndex + sSource->length;

        sSourceLength = 0;

        //-----------------------------------------
        // ���ڿ����� ���� ���ڰ� ���° �������� �˾Ƴ�
        //-----------------------------------------

        if ( sStart > 0 )
        {
            // substring('abcde', 0)�̳� substring('abcde', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else if ( sStart < 0 )
        {
            while ( sSourceIndex < sSourceFence )
            {
                (void)sLanguage->nextCharPtr( &sSourceIndex, sSourceFence );
                sSourceLength++;
            }
            // ������ ���
            sStart = sStart + sSourceLength;
        }
        else
        {
            // nothing to do
        }

        //-----------------------------------------
        // ��� ����
        //-----------------------------------------

        if ( sStart < 0 )
        {
            // ���� ���� ��ġ�� ���ڿ��� ������ �Ѿ ���,
            // - ù��° ���ں��ٵ� ���� ���� ��ġ�� ���� ���
            // - ������ ���ں��� ���� ���� ��ġ�� ū ���
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // ���� ���� ��ġ�� ���� ��ü �������� �۰ų� ����  ���

            // ���� index�� ã��
            sStartIndex = sSource->value;

            for ( i = 0; i < sStart; i++ )
            {
                if ( sStartIndex < sSourceFence )
                {
                    (void)sLanguage->nextCharPtr( &sStartIndex, sSourceFence );
                }
                else
                {
                    /* Nothing to do */
                }

                /* BUG-42544 enhancement substring function */
                if ( sStartIndex >= sSourceFence )
                {
                    aStack[0].column->module->null( aStack[0].column, aStack[0].value );
                    IDE_CONT( normal_exit );
                }
                else
                {
                    /* Nothing to do */
                }
            }

            // �� index�� ã��
            sEndIndex = sStartIndex;

            for ( i = 0 ; i < sLength ; i++ )
            {
                (void)sLanguage->nextCharPtr( &sEndIndex, sSourceFence );

                if ( sEndIndex == sSourceFence )
                {
                    break;
                }
                else
                {
                    // nothing to do
                }
            }

            //-----------------------------------------
            // ���� index���� n���� ���� string�� copy�Ͽ� ����� ����
            //-----------------------------------------

            sCopySize = sEndIndex - sStartIndex;
            IDE_TEST_RAISE( sCopySize > (SLong) aStack[0].column->precision,
                            ERR_EXCEED_MAX );

            sResult->length = (UShort)sCopySize;
            idlOS::memcpy( sResult->value,
                           sStartIndex,
                           sResult->length );
        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_idnReachEnd ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

