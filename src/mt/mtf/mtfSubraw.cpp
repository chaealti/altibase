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
 * $Id: mtfSubraw.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

extern mtfModule mtfSubraw;

extern mtdModule mtdInteger;
extern mtdModule mtdVarbyte;

static mtcName mtfSubrawFunctionName[1] = {
    { NULL, 6, (void*)"SUBRAW" }
};

static IDE_RC mtfSubrawEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 mtcCallBack* aCallBack );

mtfModule mtfSubraw = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfSubrawFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfSubrawEstimate
};

static IDE_RC mtfSubrawCalculateFor2Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

static IDE_RC mtfSubrawCalculateFor3Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfSubrawCalculateFor2Args,
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
    mtfSubrawCalculateFor3Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfSubrawEstimate( mtcNode*     aNode,
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

    // PROJ-1362
    sModules[0] = &mtdVarbyte;
    sModules[1] = &mtdInteger;
    sModules[2] = &mtdInteger;

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
    {
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteFor2Args;
    }
    else
    {
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecuteFor3Args;
    }

    sPrecision = aStack[1].column->precision;

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     &mtdVarbyte,
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

IDE_RC mtfSubrawCalculateFor2Args( mtcNode*     aNode,
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
 *    SUBRAW( byte, m )
 *
 *    aStack[0] : �Էµ� byte�� m��° byte���� ������ ��� byte ��ȯ
 *    aStack[1] : byte ( �Էµ� byte )
 *    aStack[2] : n ( ������ )
 *
 *    ex ) SUBRAW( 'AABBCC', 2 ) ==> 'BBCC'
 *
 ***********************************************************************/
    
    mtdByteType     * sResult;
    mtdByteType     * sSource;
    mtdIntegerType    sStart;
    UChar           * sStartIndex;
    UShort            sSourceLength = 0;

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
        sResult   = (mtdByteType*)aStack[0].value;
        sSource   = (mtdByteType*)aStack[1].value;
        sStart    = *(mtdIntegerType*)aStack[2].value;

        //-----------------------------------------
        // ��ü byte ���� ����
        //-----------------------------------------        
        sSourceLength = sSource->length;
        

        //-----------------------------------------
        // byte������ ���� byte�� ���° byte���� �˾Ƴ�
        //-----------------------------------------
        
        if ( sStart > 0 )
        {
            // subraw('AABBCC', 0)�̳� subraw('AABBCC', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else if ( sStart < 0 )
        {
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
        
        if ( ( sSourceLength <= sStart ) || ( sStart < 0 ) )
        {
            // ���� byte ��ġ�� byte���� ������ �Ѿ ���,
            // - ù��° byte���ٵ� ���� byte ��ġ�� ���� ���
            // - ������ byte���� ���� byte ��ġ�� ū ���
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            // ���� byte ��ġ�� byte ��ü �������� �۰ų� ����  ���
            
            // ���� index�� ã��
            sStartIndex = sSource->value + sStart;

            //-----------------------------------------
            // ���� index���� �������� byte�� copy�Ͽ� ����� ����
            //-----------------------------------------
            
            sResult->length = sSource->length - ( sStartIndex - sSource->value );
            idlOS::memcpy( sResult->value,
                           sStartIndex,
                           sResult->length );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfSubrawCalculateFor3Args( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : SUBRAW Calculate
 *
 * Implementation :
 *    SUBRAW( raw, m, n )
 *
 *    aStack[0] : �Էµ� byte���� m��° byte���� n���� byte ��ȯ
 *    aStack[1] : �Էµ� byte��
 *    aStack[2] : ���� ( eg. m )
 *    aStack[3] : ��ȯ byte ����   ( eg. n )
 *
 *    ex) SUBRAW('AABBCC', 1, 2 ) ==> 'AABB'
 *
 ***********************************************************************/
    
    mtdByteType     * sResult;
    mtdByteType     * sSource;
    mtdIntegerType    sStart;
    mtdIntegerType    sLength;
    UChar           * sStartIndex;
    UChar           * sSourceFence;
    UChar           * sEndIndex;
    UShort            sSourceLength = 0;

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
        sResult   = (mtdByteType*)aStack[0].value;
        sSource   = (mtdByteType*)aStack[1].value;
        sStart    = *(mtdIntegerType*)aStack[2].value;
        sLength   = *(mtdIntegerType*)aStack[3].value;


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
        // ��ü byte ���� ����
        //-----------------------------------------
        sSourceLength = sSource->length;
        
        //-----------------------------------------
        // byte������ ���� ���ڰ� ���° byte���� �˾Ƴ�
        //-----------------------------------------
        
        if ( sStart > 0 )
        {
            // subraw('AABBCC', 0)�̳� substring('AABBCC', 1)�̳� ����.
            // ���� ���� ����Ŭ ������ �׷��ؼ� �����Ѵ�.
            // ���� ������ 0�����̴�.
            sStart--;
        }
        else if ( sStart < 0 )
        {
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
        
        if ( ( sSourceLength <= sStart ) || ( sStart < 0 ) )
        {
            // ���� byte ��ġ�� byte���� ������ �Ѿ ���,
            // - ù��° byte���ٵ� ���� byte ��ġ�� ���� ���
            // - ������ byte���� ���� byte ��ġ�� ū ���
            aStack[0].column->module->null( aStack[0].column,
                                           aStack[0].value );
        }
        else
        {
            // ���� index�� ã��
            sStartIndex = sSource->value + sStart;
            
            // �� index�� ã��
            sSourceFence = sSource->value + sSource->length;
            sEndIndex = sStartIndex + sLength;
            
            if ( sEndIndex >= sSourceFence )
            {
                sEndIndex = sSourceFence;
            }
            else
            {
                /* Nothing to do */
            }
            
            //-----------------------------------------
            // start position���� n���� ���� byte�� copy�Ͽ� ����� ����
            //-----------------------------------------
            
            sResult->length = sEndIndex - sStartIndex;
            
            IDE_TEST_RAISE( sResult->length > aStack[0].column->precision,
                            ERR_EXCEED_MAX );
            
            idlOS::memcpy( sResult->value,
                           sStartIndex,
                           sResult->length );
        }

    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_EXCEED_MAX );
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_REACH_END ));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

