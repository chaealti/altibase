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
 * $Id: mtfPosition.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>

#define MTF_ABS(x) ( ( (x) < 0 ) ? -(x) : (x) )

extern mtfModule mtfPosition;

extern mtdModule mtdInteger;

static mtcName mtfPositionFunctionName[2] = {
    { mtfPositionFunctionName+1, 8, (void*)"POSITION" },
    { NULL,                      5, (void*)"INSTR"    }
};

static IDE_RC mtfPositionEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

IDE_RC mtfFindPosition( const mtlModule* aLanguage,
                        UShort*          aPosition,
                        const UChar*     aSource,
                        UShort           aSourceLen,
                        const UChar*     aTarget,
                        UShort           aTargetLen,
                        SShort           aStart,
                        UShort           aOccurrence );

IDE_RC mtfReverseFindPosition( const mtlModule* aLanguage,
                               UShort*          aPosition,
                               const UChar*     aSource,
                               UShort           aSourceLen,
                               const UChar*     aTarget,
                               UShort           aTargetLen,
                               SShort           aStart,
                               UShort           aOccurrence );

mtfModule mtfPosition = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfPositionFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfPositionEstimate
};

static IDE_RC mtfPositionCalculateFor2Args( mtcNode*     aNode, 
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

static IDE_RC mtfPositionCalculateFor3Args( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

static IDE_RC mtfPositionCalculateFor4Args( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfPositionCalculateFor2Args,
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
    mtfPositionCalculateFor3Args,
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
    mtfPositionCalculateFor4Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfPositionEstimate( mtcNode*     aNode,
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

IDE_RC mtfReverseFindPosition( const mtlModule* aLanguage,
                               UShort*          aPosition,
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

    UChar  * sSourceIndex;
    UChar  * sSourceFence;
    UChar  * sTargetIndex;
    UChar  * sTargetFence;
    UChar  * sCurSourceIndex;
    UShort   sSourceCharCount = 0;
    UShort   sSourceTotalCharCount = 0;
    idBool   sIsSame = ID_FALSE;
    UShort   sCnt = 0;
    UShort   sFindCnt = 0;
    UChar    sSourceSize;
    UChar    sTargetSize;
    
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

    // Source�� ���� ������ ����
    sSourceIndex = (UChar*) aSource;
    sSourceFence = sSourceIndex + aSourceLen;

    while ( sSourceIndex < sSourceFence )
    {
        // TASK-3420 ���ڿ� ó�� ��å ����
        (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

        ++sSourceTotalCharCount;
    }

    // Target�� ���� ������ ����
    sTargetIndex = (UChar*) aTarget;
    sTargetFence = sTargetIndex + aTargetLen;

    while ( sTargetIndex < sTargetFence )
    {
        // TASK-3420 ���ڿ� ó�� ��å ����
        (void)aLanguage->nextCharPtr( & sTargetIndex, sTargetFence );
    }

    // Ž���Ǵ� substring�� ����� ã��. 
    sSourceIndex = (UChar*) aSource;
    sSourceFence = sSourceIndex + aSourceLen;

    while ( sSourceIndex < sSourceFence )
    {
        //----------------------------------------
        // Source�� target ���ڿ��� �����ϴ��� �˻�
        //----------------------------------------

        sCurSourceIndex = sSourceIndex;
        
        sTargetIndex = (UChar*) aTarget;
        sTargetFence = sTargetIndex + aTargetLen;
        
        while ( sTargetIndex < sTargetFence )
        {
            sSourceSize =  mtl::getOneCharSize( sCurSourceIndex,
                                                sSourceFence,
                                                aLanguage ); 
        
            sTargetSize =  mtl::getOneCharSize( sTargetIndex,
                                                sTargetFence,
                                                aLanguage );
            
            // Source ���ڿ� Target ���ڰ� �������� ��
            // ���� �ѹ��ڸ�ŭ �ε��� ����

            sIsSame = mtc::compareOneChar( sCurSourceIndex,
                                           sSourceSize,
                                           sTargetIndex,
                                           sTargetSize );
            
            if ( sIsSame == ID_TRUE )
            {
                // ������ ���, ���� target���� ����
                // TASK-3420 ���ڿ� ó�� ��å ����
                (void)aLanguage->nextCharPtr( & sTargetIndex, sTargetFence );

                (void)aLanguage->nextCharPtr( & sCurSourceIndex, sSourceFence );

                if ( sCurSourceIndex == sSourceFence )
                {
                    // Source�� ������ �����ε�,
                    // Target�� ������ ���ڰ� �ƴ� ���
                    if ( sTargetIndex < sTargetFence )
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
                break;
            }
        }

        if ( sIsSame == ID_TRUE )
        {
            //----------------------------------------
            // start�� �����̹Ƿ� sSourceTotalCharCount  - MTF_ABS(aStart)
            // �ٷ� ������ �ִ� position���� ���ϰ� , sCnt ����
            // sSourceCharCount�� ���� ������ ����, Temp�� ��ü ������ ����
            //----------------------------------------

            if ( ( sSourceTotalCharCount - MTF_ABS(aStart) ) >= sSourceCharCount )
            {
                *aPosition = sSourceCharCount + 1;
                sCnt++;
            }

            // �������� ����
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

            ++sSourceCharCount;
        }
        else
        {
            // �������� ����
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

            ++sSourceCharCount;
        }
    }
    
    /* ã�� Count�� ���ٸ� ����� 0�� */
    if ( sCnt != 0 )
    {
        // ã�� substring�� �տ��� �� ��°���� ����.
        if ( ( sSourceTotalCharCount - MTF_ABS(aStart)  + 1 ) >= *aPosition )
        {
            sFindCnt = sCnt - aOccurrence + 1;
        }
        else
        {
            sFindCnt = sCnt - aOccurrence;
        }

        IDE_TEST( mtfFindPosition( aLanguage,
                                   aPosition,
                                   aSource,
                                   aSourceLen,
                                   aTarget,
                                   aTargetLen,
                                   1,
                                   sFindCnt )
                  != IDE_SUCCESS ); 
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfFindPosition( const mtlModule* aLanguage,
                        UShort*          aPosition,
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
    
    UChar  * sSourceIndex;
    UChar  * sSourceFence;
    UChar  * sTargetIndex;
    UChar  * sTargetFence;
    UChar  * sCurSourceIndex;
    UShort   sSourceCharCount = 0;
    idBool   sIsSame = ID_FALSE;
    UShort   sCnt = 1;
    UChar    sSourceSize;
    UChar    sTargetSize;
    SShort   sStart;

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

    // Ž���� ������ ��ġ ����
    sSourceIndex = (UChar*) aSource;
    sSourceFence = sSourceIndex + aSourceLen;
    
    sStart = aStart;
    while ( sStart - 1 > 0 )
    {
        // TASK-3420 ���ڿ� ó�� ��å ����
        (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

        ++sSourceCharCount;
        --sStart;
    }

    while ( sSourceIndex < sSourceFence )
    {
        //----------------------------------------
        // Source�� target ���ڿ��� �����ϴ��� �˻�
        //----------------------------------------
        
        sCurSourceIndex = sSourceIndex;
        
        sTargetIndex = (UChar*) aTarget;
        sTargetFence = sTargetIndex + aTargetLen;
        
        while ( sTargetIndex < sTargetFence )
        {
            sSourceSize =  mtl::getOneCharSize( sCurSourceIndex,
                                            sSourceFence,
                                            aLanguage );
            
            sTargetSize =  mtl::getOneCharSize( sTargetIndex,
                                                sTargetFence,
                                                aLanguage );
            
            // Source ���ڿ� Target ���ڰ� �������� �� 
            // ���� �ѹ��ڸ�ŭ �ε��� ����
            sIsSame = mtc::compareOneChar( sCurSourceIndex,
                                           sSourceSize,
                                           sTargetIndex,
                                           sTargetSize );

            if ( sIsSame == ID_TRUE )
            {
                // ������ ���, ���� target���� ����
                // TASK-3420 ���ڿ� ó�� ��å ����
                (void)aLanguage->nextCharPtr( & sTargetIndex, sTargetFence );

                (void)aLanguage->nextCharPtr( & sCurSourceIndex, sSourceFence );

                if ( sCurSourceIndex == sSourceFence )
                {
                    // Source�� ������ �����ε�,
                    // Target�� ������ ���ڰ� �ƴ� ���
                    if ( sTargetIndex < sTargetFence )
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

                // To Fix BUG-12741
                *aPosition = sSourceCharCount + 1;
                break;
            }
            else
            {
                // Ž���� substring ���� ����
                sCnt++;

                // TASK-3420 ���ڿ� ó�� ��å ����
                (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

                ++sSourceCharCount;
            }
        }
        else
        {
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aLanguage->nextCharPtr( & sSourceIndex, sSourceFence );

            ++sSourceCharCount;
        }
    }

    return IDE_SUCCESS;
}


IDE_RC mtfPositionCalculateFor2Args( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Position Calculate
 *
 * Implementation :
 *    INSTR( char1, char2 )
 *
 *    aStack[0] : char1�� �Էµ� ���ڿ����� char2�� �ִ� ��ġ��
 *    aStack[1] : char1 ( ���ڿ� )
 *    aStack[2] : char2 ( target ���ڿ� )
 *
 *    ex) INSTR( 'JAKIM', 'KIM' ) ==> 3
 *
 ***********************************************************************/
    
    mtdCharType* sSource;
    mtdCharType* sTarget;
    UShort       sPosition;
    
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
        sSource = (mtdCharType*)aStack[1].value;
        sTarget = (mtdCharType*)aStack[2].value;
        
        IDE_TEST( mtfFindPosition( aStack[1].column->language,
                                   &sPosition,
                                   sSource->value,
                                   sSource->length,
                                   sTarget->value, 
                                   sTarget->length, 
                                   1,
                                   1 )
                  != IDE_SUCCESS )
            
        *(mtdIntegerType*)aStack[0].value = sPosition;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC mtfPositionCalculateFor3Args( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Position Calculate
 *
 * Implementation :
 *    INSTR( char1, char2, start )
 *
 *    aStack[0] : char1�� �Էµ� ���ڿ����� char2�� �ִ� ��ġ��
 *    aStack[1] : char1 ( ���ڿ� )
 *    aStack[2] : char2 ( target ���ڿ� )
 *    aStack[3] : start (Ž���� ������ ������ ��ġ)
 *
 *    ex) INSTR( 'JAKIM JAKIM', 'KIM', 4 ) ==> 9
 *
 ***********************************************************************/

    mtdCharType * sSource;
    mtdCharType * sTarget;
    UShort        sPosition;
    SInt          sStart = 0;
    SInt          sSourceCharCount = 0;
    UChar       * sIndex;
    UChar       * sFence;

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
        sSource = (mtdCharType*)aStack[1].value;
        sTarget = (mtdCharType*)aStack[2].value;
        sStart = *(mtdIntegerType*)aStack[3].value;

        // Source�� ���� ������ ����
        sSourceCharCount = 0;

        sIndex = sSource->value;
        sFence = sIndex + sSource->length;
        
        while ( sIndex < sFence )
        {
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aStack[1].column->language->nextCharPtr( & sIndex, sFence );

            ++sSourceCharCount;
        }

        /* BUG-34232 
         * sStart( Ž���� ������ ��ġ )�� �Է¹��� ���ڿ� ���̺��� ū ���, 
         * ������ �߻���Ű�� ��� 0�� �����ϵ��� ���� */
        if ( MTF_ABS(sStart) > sSourceCharCount )
        {
            sPosition = 0;
        }
        else
        {
            if ( sStart > 0 )
            {
                IDE_TEST( mtfFindPosition( aStack[1].column->language,
                                           &sPosition,
                                           sSource->value,
                                           sSource->length,
                                           sTarget->value,
                                           sTarget->length,
                                           sStart,
                                           1 )
                          != IDE_SUCCESS );
            }
            else if ( sStart < 0 )
            {
                IDE_TEST( mtfReverseFindPosition( aStack[1].column->language,
                                                  &sPosition,
                                                  sSource->value,
                                                  sSource->length,
                                                  sTarget->value,
                                                  sTarget->length,
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



IDE_RC mtfPositionCalculateFor4Args( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Position Calculate
 *
 * Implementation :
 *    INSTR( char1, char2, start, occurence )
 *
 *    aStack[0] : char1�� �Էµ� ���ڿ����� char2�� �ִ� ��ġ��
 *    aStack[1] : char1 ( ���ڿ� )
 *    aStack[2] : char2 ( target ���ڿ� )
 *    aStack[3] : start (Ž���� ������ ������ ��ġ)
 *    aStack[4] : Ž���� substring�� ����
 *
 *    ex) INSTR( 'JAKIM JAKIM', 'KIM', 2, 2 ) ==> 9
 *
 ***********************************************************************/

    mtdCharType * sSource;
    mtdCharType * sTarget;
    UShort        sPosition;
    SInt          sStart = 0;
    SInt          sOccurrence = 0;
    SInt          sSourceCharCount = 0;
    UChar       * sIndex;
    UChar       * sFence;

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
        sSource = (mtdCharType*)aStack[1].value;
        sTarget = (mtdCharType*)aStack[2].value;
        sStart = *(mtdIntegerType*)aStack[3].value;
        sOccurrence = *(mtdIntegerType*)aStack[4].value;

        // Source�� ���� ������ ����
        sSourceCharCount = 0;

        sIndex = sSource->value;
        sFence = sIndex + sSource->length;
        
        while ( sIndex < sFence )
        {
            // TASK-3420 ���ڿ� ó�� ��å ����
            (void)aStack[1].column->language->nextCharPtr( & sIndex, sFence );

            ++sSourceCharCount;
        }

        IDE_TEST_RAISE ( ( sOccurrence <= 0 ),
                         ERR_ARGUMENT4_VALUE_OUT_OF_RANGE );
        
        /* BUG-34232 
         * sStart( Ž���� ������ ��ġ )�� �Է¹��� ���ڿ� ���̺��� ū ���, 
         * ������ �߻���Ű�� ��� 0�� �����ϵ��� ���� */       
        if ( MTF_ABS(sStart) > sSourceCharCount )
        {
            sPosition = 0;
        }
        else
        {
            if ( sStart > 0 )
            {
                IDE_TEST( mtfFindPosition( aStack[1].column->language,
                                           &sPosition,
                                           sSource->value,
                                           sSource->length,
                                           sTarget->value,
                                           sTarget->length,
                                           sStart,
                                           sOccurrence )
                          != IDE_SUCCESS );
            }
            else if ( sStart < 0 )
            {
                IDE_TEST( mtfReverseFindPosition( aStack[1].column->language,
                                                  &sPosition,
                                                  sSource->value,
                                                  sSource->length,
                                                  sTarget->value,
                                                  sTarget->length,
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
