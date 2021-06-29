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
 * $Id: mtfInstrb.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <idErrorCode.h>
#include <mtdTypes.h>

#define MTF_ABS(x) ( ( (x) < 0 ) ? -(x) : (x) )

extern mtfModule mtfInstrb;

extern mtdModule mtdInteger;

static mtcName mtfInstrbFunctionName[1] = {
    { NULL, 6, (void*)"INSTRB"    }
};

static IDE_RC mtfInstrbEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 mtcCallBack* aCallBack );

IDE_RC mtfFindPosition( UShort*          aPosition,
                        const UChar*     aSource,
                        UShort           aSourceLen,
                        const UChar*     aTarget,
                        UShort           aTargetLen,
                        SShort           aStart,
                        UShort           aOccurrence );

IDE_RC mtfReverseFindPosition( UShort*          aPosition,
                               const UChar*     aSource,
                               UShort           aSourceLen,
                               const UChar*     aTarget,
                               UShort           aTargetLen,
                               SShort           aStart,
                               UShort           aOccurrence );

mtfModule mtfInstrb = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfInstrbFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfInstrbEstimate
};

static IDE_RC mtfInstrbCalculateFor2Args( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

static IDE_RC mtfInstrbCalculateFor3Args( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

static IDE_RC mtfInstrbCalculateFor4Args( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfInstrbCalculateFor2Args,
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
    mtfInstrbCalculateFor3Args,
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
    mtfInstrbCalculateFor4Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfInstrbEstimate( mtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt      /* aRemain */,
                          mtcCallBack* aCallBack )
{
    const mtdModule* sModules[4];

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 2 ||
                    ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 4,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                            aStack[1].column->module )
              != IDE_SUCCESS );

    sModules[1] = sModules[0];
    sModules[2] = &mtdInteger;
    sModules[3] = &mtdInteger;

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 )
    {
        aTemplate->rows[aNode->table].execute[aNode->column] =
                                                    mtfExecuteFor2Args;
    }
    else if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
    {
        aTemplate->rows[aNode->table].execute[aNode->column] =
                                                    mtfExecuteFor3Args;
    }
    else if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 4 )
    {
        aTemplate->rows[aNode->table].execute[aNode->column] =
                                                    mtfExecuteFor4Args;
    }
    else
    {
        // nothing to do
    }

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdInteger,
                                     0,
                                     0,
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

IDE_RC mtfReverseFindPosition( UShort*          aPosition,
                               const UChar*     aSource,
                               UShort           aSourceLen,
                               const UChar*     aTarget,
                               UShort           aTargetLen,
                               SShort           aStart,
                               UShort           aOccurrence )
{
/***********************************************************************
 *
 * Description : source�� �����ϴ� target ��ġ �Ųٷ� ã��
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort sSourceIndex = 0;
    UShort sTargetIndex = 0;
    UShort sCurSourceIndex;
    idBool sIsSame = ID_FALSE;
    UShort sCnt = 0;
    UShort sFindCnt = 0;

    // target ��ġ �ʱ�ȭ
    *aPosition = 0;

    if ( aTargetLen == 0 )
    {
        // target ���ڿ��� 0 �� ���
        return IDE_SUCCESS;
    }
    else
    {
        // nothing to do
    }

    // Ž���Ǵ� substring�� ����� ã��.
    while ( sSourceIndex < aSourceLen )
    {
        //----------------------------------------
        // Source�� target ���ڿ��� �����ϴ��� �˻�
        //----------------------------------------

        sCurSourceIndex = sSourceIndex;
        sTargetIndex = 0;
        while ( sTargetIndex < aTargetLen )
        {
            // Source ���ڿ� Target ���ڰ� �������� ��
            // ���� �ѹ��ڸ�ŭ �ε��� ����
            if ( aSource[sCurSourceIndex] == aTarget[sTargetIndex] )
            {
                // ������ ���, ���� target���� ����
                sIsSame = ID_TRUE;
                sTargetIndex++;
                sCurSourceIndex++;

                if ( sCurSourceIndex == aSourceLen )
                {
                    // Source�� ������ �����ε�,
                    // Target�� ������ ���ڰ� �ƴ� ���
                    if ( sTargetIndex < aTargetLen )
                    {
                        sIsSame = ID_FALSE;
                        break;
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
                sIsSame = ID_FALSE;
                break;
            }
        }

        if ( sIsSame == ID_TRUE )
        {
            if ( ( aSourceLen - MTF_ABS(aStart) ) >= sSourceIndex )
            {
                *aPosition = sSourceIndex + 1;
                sCnt++;
            }
            // �������� ����
            sSourceIndex++;
        }
        else
        {
            // �������� ����
            sSourceIndex++;
        }
    }

    /* ã�� Count�� ���ٸ� ����� 0�� */
    if ( sCnt != 0 )
    {
        // ã�� substring�� �տ��� �� ��°���� ����.
        if ( ( aSourceLen - MTF_ABS(aStart) + 1 ) >= *aPosition )
        {
            sFindCnt = sCnt - aOccurrence + 1;
        }
        else
        {
            sFindCnt = sCnt - aOccurrence;
        }

        IDE_TEST( mtfFindPosition( aPosition,
                                   aSource,
                                   aSourceLen,
                                   aTarget,
                                   aTargetLen,
                                   1,
                                   sFindCnt )
                  != IDE_SUCCESS )
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfFindPosition( UShort*          aPosition,
                        const UChar*     aSource,
                        UShort           aSourceLen,
                        const UChar*     aTarget,
                        UShort           aTargetLen,
                        SShort           aStart,
                        UShort           aOccurrence )
{
/***********************************************************************
 *
 * Description : source�� �����ϴ� target ��ġ ã��
 *
 * Implementation :
 *
 ***********************************************************************/
    UShort sSourceIndex = 0;
    UShort sTargetIndex = 0;
    UShort sCurSourceIndex;
    idBool sIsSame = ID_FALSE;
    UShort sCnt = 1;

    // target ��ġ �ʱ�ȭ
    *aPosition = 0;
    
    if ( aTargetLen == 0 )
    {
        // target ���ڿ��� 0 �� ���
        return IDE_SUCCESS;
    }
    else
    {
        // nothing to do
    }

    sSourceIndex = aStart - 1;
    
    while ( sSourceIndex < aSourceLen )
    {
        //----------------------------------------
        // Source�� target ���ڿ��� �����ϴ��� �˻�
        //----------------------------------------
        
        sCurSourceIndex = sSourceIndex;
        sTargetIndex = 0;
        while ( sTargetIndex < aTargetLen )
        {
            // Source ���ڿ� Target ���ڰ� �������� �� 
            // ���� �ѹ��ڸ�ŭ �ε��� ����
            if ( aSource[sCurSourceIndex] == aTarget[sTargetIndex] )
            {
                // ������ ���, ���� target���� ����
                sIsSame = ID_TRUE;
                sTargetIndex++;
                sCurSourceIndex++;
                
                if ( sCurSourceIndex == aSourceLen )
                {
                    // Source�� ������ �����ε�,
                    // Target�� ������ ���ڰ� �ƴ� ���
                    if ( sTargetIndex < aTargetLen )
                    {
                        sIsSame = ID_FALSE;
                        break;
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
                sIsSame = ID_FALSE;
                break;
            }
        }

        if ( sIsSame == ID_TRUE )
        {
            // aOccurence�� sCnt�� ���ƾ� Ž�� ����.
            if ( aOccurrence == sCnt )
            {
                //----------------------------------------
                // Source�� target ���ڿ��� �����ϴ� ���, Position ����
                //----------------------------------------

                *aPosition = sSourceIndex + 1;
                break;
            }
            else
            {
                // Ž���� substring ���� ����
                sCnt++;

                // �������� ����
                sSourceIndex++;
            }
        }
        else
        {
            // �������� ����
            sSourceIndex++;
        }
    }

    return IDE_SUCCESS;
    
//    IDE_EXCEPTION_END;
    
//    return IDE_FAILURE;
}

IDE_RC mtfInstrbCalculateFor2Args( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
    mtdCharType* sVarchar1;
    mtdCharType* sVarchar2;
    UShort       sPosition;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value  ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sVarchar1 = (mtdCharType*)aStack[1].value;
        sVarchar2 = (mtdCharType*)aStack[2].value;
        
        IDE_TEST( mtfFindPosition( & sPosition,
                                   sVarchar1->value,
                                   sVarchar1->length,
                                   sVarchar2->value,
                                   sVarchar2->length,
                                   1,
                                   1 )
                  != IDE_SUCCESS )
        
        *(mtdIntegerType*)aStack[0].value = sPosition;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC mtfInstrbCalculateFor3Args( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
    mtdCharType* sVarchar1;
    mtdCharType* sVarchar2;
    UShort       sPosition;
    SInt         sStart = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE ) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sVarchar1 = (mtdCharType*)aStack[1].value;
        sVarchar2 = (mtdCharType*)aStack[2].value;
        sStart = *(mtdIntegerType*)aStack[3].value;

        /* BUG-34232 
         * sStart( Ž���� ������ ��ġ )�� �Է¹��� ���ڿ� ���̺��� ū ���, 
         * ������ �߻���Ű�� ��� 0�� �����ϵ��� ���� */
        if ( MTF_ABS(sStart) > sVarchar1->length )
        {
            sPosition = 0;
        }
        else
        {
            if ( sStart > 0 )
            {
               IDE_TEST( mtfFindPosition(  &sPosition,
                                           sVarchar1->value,
                                           sVarchar1->length,
                                           sVarchar2->value,
                                           sVarchar2->length,
                                           sStart,
                                           1 )
                          != IDE_SUCCESS );
            }
            else if ( sStart < 0 )
            {
                IDE_TEST( mtfReverseFindPosition( &sPosition,
                                                  sVarchar1->value,
                                                  sVarchar1->length,
                                                  sVarchar2->value,
                                                  sVarchar2->length,
                                                  sStart,
                                                  1 )
                          != IDE_SUCCESS );
            }
            else
            {
                sPosition = 0;
            }
        }

        *(mtdIntegerType*)aStack[0].value = sPosition;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mtfInstrbCalculateFor4Args( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
    mtdCharType* sVarchar1;
    mtdCharType* sVarchar2;
    UShort       sPosition;
    SInt         sStart = 0;
    SInt         sOccurrence = 0;

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
        sVarchar1 = (mtdCharType*)aStack[1].value;
        sVarchar2 = (mtdCharType*)aStack[2].value;
        sStart = *(mtdIntegerType*)aStack[3].value;
        sOccurrence = *(mtdIntegerType*)aStack[4].value;

        IDE_TEST_RAISE ( ( sOccurrence <= 0 ),
                         ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );

        /* BUG-34232 
         * sStart( Ž���� ������ ��ġ )�� �Է¹��� ���ڿ� ���̺��� ū ���, 
         * ������ �߻���Ű�� ��� 0�� �����ϵ��� ���� */
        if ( MTF_ABS(sStart) > sVarchar1->length )
        {
            sPosition = 0;
        }
        else
        {
            if ( sStart > 0 )
            {
               IDE_TEST( mtfFindPosition(  &sPosition,
                                           sVarchar1->value,
                                           sVarchar1->length,
                                           sVarchar2->value,
                                           sVarchar2->length,
                                           sStart,
                                           sOccurrence )
                          != IDE_SUCCESS );
            }
            else if ( sStart < 0 )
            {
                IDE_TEST( mtfReverseFindPosition( &sPosition,
                                                  sVarchar1->value,
                                                  sVarchar1->length,
                                                  sVarchar2->value,
                                                  sVarchar2->length,
                                                  sStart,
                                                  sOccurrence )
                          != IDE_SUCCESS );
            }
            else
            {
                sPosition = 0;
            }
        }

        *(mtdIntegerType*)aStack[0].value = sPosition;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
    {
        IDE_SET( ideSetErrorCode(mtERR_ABORT_ARGUMENT_VALUE_OUT_OF_RANGE,
                                 sOccurrence ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
 
