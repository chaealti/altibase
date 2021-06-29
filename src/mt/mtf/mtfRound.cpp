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
 * $Id: mtfRound.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfRound;

extern mtdModule mtdFloat;
extern mtdModule mtdDate;
extern mtdModule mtdChar;

static mtcName mtfRoundFunctionName[1] = {
    { NULL, 5, (void*)"ROUND" }
};

static IDE_RC mtfRoundEstimate( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                mtcCallBack* aCallBack );

mtfModule mtfRound = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfRoundFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfRoundEstimate
};

IDE_RC mtfRoundCalculateFloatFor1Arg( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate );

IDE_RC mtfRoundCalculateFloatFor2Args( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate );

IDE_RC mtfRoundCalculateFor1Arg( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

IDE_RC mtfRoundCalculateFor2Args( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

static const mtcExecute mtfExecuteFloatFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRoundCalculateFloatFor1Arg,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

static const mtcExecute mtfExecuteFloatFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRoundCalculateFloatFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

static const mtcExecute mtfExecuteFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRoundCalculateFor1Arg,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

static const mtcExecute mtfExecuteFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfRoundCalculateFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfRoundEstimate( mtcNode*     aNode,
                         mtcTemplate* aTemplate,
                         mtcStack*    aStack,
                         SInt      /* aRemain */,
                         mtcCallBack* aCallBack )
{
    const mtdModule* sFloatModules[2];
    const mtdModule* sArgModules[2] = { &mtdDate, &mtdChar };

    /* BUG-44091 where ���� round(), trunc() ���� ��� ������ �����մϴ�.  */
    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 1 ||
                    ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    if ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
    {
        mtc::makeFloatConversionModule( aStack + 1, &sFloatModules[0] );
    }
    else
    {
        mtc::makeFloatConversionModule( aStack + 1, &sFloatModules[0] );
        mtc::makeFloatConversionModule( aStack + 2, &sFloatModules[1] );
    }

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    if( ( aStack[1].column->module->flag & MTD_GROUP_MASK ) == MTD_GROUP_DATE )
    {
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                            mtfExecuteFor1Arg;
        }
        else
        {
            IDE_TEST( mtf::makeConversionNodes( aNode,
                                                aNode->arguments,
                                                aTemplate,
                                                aStack + 1,
                                                aCallBack,
                                                sArgModules )
                      != IDE_SUCCESS );

            aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                            mtfExecuteFor2Args;
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         & mtdDate,
                                         0,
                                         0,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            sFloatModules )
                  != IDE_SUCCESS );

        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                mtfExecuteFloatFor1Arg;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                mtfExecuteFloatFor2Args;
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         & mtdFloat,
                                         0,
                                         0,
                                         0 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRoundCalculateFloatFor1Arg( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Round Calculate
 *
 * Implementation :
 *    aStack[0] : ������ �ݿø��� ����
 *    aStack[1] : �Է� ����
 *
 *    ex) ROUND( 15.79 ) ==> 16
 *
 ***********************************************************************/

    mtdNumericType * sMtdZeroValue;
    UChar            sMtdZeroBuff[20] = { 1, 128, 0, };
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( mtdFloat.isNull( aStack[1].column,
                         aStack[1].value ) == ID_TRUE )
    {
        mtdFloat.null( aStack[0].column,
                       aStack[0].value );
    }
    else
    {
        sMtdZeroValue = (mtdNumericType*)sMtdZeroBuff;
        IDE_TEST( mtc::roundFloat( (mtdNumericType*)aStack[0].value,
                                   (mtdNumericType*)aStack[1].value,
                                   sMtdZeroValue )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRoundCalculateFloatFor2Args( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Round Calculate
 *
 * Implementation :
 *    aStack[0] : ������ �ݿø��� ����
 *    aStack[1] : �Է� ����
 *    aStack[2] : �ݿø��Ͽ� ��Ÿ�� �Ҽ��� �Ʒ� ������ ����
 *
 *    ex) ROUND( 15.796, 2 ) ==> 15.8
 *
 ***********************************************************************/

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( (mtdFloat.isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (mtdFloat.isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        mtdFloat.null( aStack[0].column,
                       aStack[0].value );
    }
    else
    {
        IDE_TEST( mtc::roundFloat( (mtdNumericType*)aStack[0].value,
                                   (mtdNumericType*)aStack[1].value,
                                   (mtdNumericType*)aStack[2].value )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfRoundCalculateFor1Arg(  mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Round Calculate
 *
 * Implementation :
 *    aStack[0] : fmt�� '��'�� �����ϰ� �ݿø��Ѵ�.
 *    aStack[1] : ��¥ ��
 *
 ***********************************************************************/

    mtdDateType*     sValue;
    mtdDateType*     sDate;

    SShort           sYear = 0;
    UShort           sMonth = 0;
    UShort           sDay = 0;
    UShort           sHour = 0;

    UChar sLastDays[13] = { 0, 31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31 };

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
        sValue   = (mtdDateType*)aStack[0].value;
        sDate    = (mtdDateType*)aStack[1].value;

        sYear = mtdDateInterface::year(sDate);
        sMonth = mtdDateInterface::month(sDate);
        sDay = mtdDateInterface::day(sDate);
        sHour = mtdDateInterface::hour(sDate);

        // date�� ������ ���
        if ( mtdDateInterface::isLeapYear( sYear ) == ID_TRUE )
        {
             sLastDays[2] = 29;
        }

        if ( sHour >= 12 )
        {
            // ������ ������ üũ
            if ( sDay == sLastDays[sMonth] ) 
            {
                if ( sMonth == 12 )
                {
                    sYear++;
                    sMonth = 1;
                    sDay = 1;
                }
                else
                {
                    sMonth++;
                    sDay = 1;
                }
            }
            else
            {
                /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                if ( ( sYear == 1582 ) &&
                     ( sMonth == 10 ) &&
                     ( sDay == 4 ) )
                {
                    sDay += 10;
                }
                else
                {
                    /* Nothing to do */
                }

                sDay++;
            }
        }
        // 12�ø� ���� ������ �� ���ϴ� ������.
        else
        {
            // nothing to do
        }

        IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                        ERR_INVALID_YEAR );

        IDE_TEST( mtdDateInterface::makeDate(sValue,
                                             sYear,
                                             sMonth,
                                             sDay, 
                                             0, 0, 0, 0)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_YEAR );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_YEAR ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfRoundCalculateFor2Args(  mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Round Calculate
 *
 * Implementation :
 *    aStack[0] :
 *    aStack[1] : �о�� ��Ʈ��
 *
 ***********************************************************************/

    mtdDateType*     sValue;
    mtdDateType*     sDate;
    mtdCharType*     sVarchar;
    const mtlModule* sLanguage;
    SInt             sYear = 0;
    SInt             sQuarter = 0;
    SInt             sMonth = 0;
    SInt             sDayOfWeek = 0;
    SInt             sDay = 0;
    SInt             sHour = 0;
    SInt             sMinute = 0;
    SInt             sSecond = 0;

    UChar sLastDays[13] = { 0, 31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31 };

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
        sValue   = (mtdDateType*)aStack[0].value;
        sDate    = (mtdDateType*)aStack[1].value;
        sVarchar = (mtdCharType*)aStack[2].value;
        sLanguage = aStack[2].column->language;

        sYear = mtdDateInterface::year(sDate);
        sMonth = mtdDateInterface::month(sDate);
        sDay = mtdDateInterface::day(sDate);
        sHour = mtdDateInterface::hour(sDate);
        sMinute = mtdDateInterface::minute(sDate);
        sSecond = mtdDateInterface::second(sDate);

        // date�� ������ ���
        if ( mtdDateInterface::isLeapYear( sYear ) == ID_TRUE )
        {
             sLastDays[2] = 29;
        }
        else
        {
            /* Nothing to do */
        }

        if( sLanguage->extractSet->matchCentury( sVarchar->value,
                                                 sVarchar->length ) == 0 )
        {
            if ( sYear <= 0 )
            {
                // -50����� ���� ����� �ø�
                sYear = ( ( sYear - 50 ) / 100 ) * 100 + 1;
            }
            else
            {
                // 51����� ���� ����� �ø�
                sYear = ( ( sYear + 49 ) / 100 ) * 100 + 1;
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            sMonth = 1;
            sDay = 1;

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchYear( sVarchar->value,
                                                   sVarchar->length ) == 0 )
        {
            // 7�� ���� ���� �ط� �ø�
            if ( sMonth >= 7 )
            {
                sYear++;
            }
            //7�� �����̸� 1���� ������.
            else
            {
                // nothing to do
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            sMonth = 1;
            sDay = 1;

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchQuarter( sVarchar->value,
                                                      sVarchar->length ) == 0 )
        {
            sQuarter = (SInt) idlOS::ceil( ( (SDouble) sMonth / 3 ) );
            // �ش� �б��� �ι�° ���� 16�Ϻ��� ���� �б�� �ø�
            if ( ( sMonth >= ( sQuarter * 3 - 1 ) && sDay >= 16 ) ||
                   sMonth == ( sQuarter * 3 ) )
            {
                sMonth = ( sQuarter + 1 ) * 3 - 2;
                if ( sMonth > 12 )
                {
                    sYear++;
                    sMonth = 1;
                }
            }
            else
            {
                sMonth = sQuarter * 3 - 2;
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            sDay = 1;

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchMonth( sVarchar->value,
                                                    sVarchar->length ) == 0 )
        {
            // 16�� ���� ���� �޷� �ø�
            if ( sDay >= 16)
            {
                if ( sMonth == 12 )
                {
                    sYear++;
                    sMonth = 1;
                }
                else
                {
                    sMonth++;
                }
            }
            // 16�� �����̸� 1�Ϸ� ������.
            else
            {
                // nothing to do
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            sDay = 1;

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchWeek( sVarchar->value,
                                                   sVarchar->length ) == 0 )
        {
            sDayOfWeek = mtc::dayOfWeek ( sYear, sMonth, sDay );
            // ����Ϻ��� ���� �Ͽ��Ϸ� �ø�.
            if ( sDayOfWeek >= 4)
            {
                /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                if ( ( sYear == 1582 ) &&
                     ( sMonth == 10 ) &&
                     ( sDay == 4 ) )
                {
                    sDay += 10;
                }
                else
                {
                    /* Nothing to do */
                }

                sDay += ( 7 - sDayOfWeek );

                /* BUG-45730 ���� ���� ���� [ = ���� sDay�� �ش� �޿� ������ ���̵� �������. ] */
                if ( sDay > sLastDays[sMonth] )
                {
                    sDay -= sLastDays[sMonth]; 
                    sMonth++;
                    if ( sMonth > 12 )
                    {
                        sYear++;
                        sMonth = 1;
                    }
                }
            }
            else
            {
                sDay -= sDayOfWeek;
                if ( sDay < 1 )
                {
                    sMonth--;
                    if ( sMonth < 1 )
                    {
                        sYear--;
                        sMonth = 12;
                    }
                    sDay = sLastDays[sMonth] + sDay;
                }
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchDay( sVarchar->value,
                                                  sVarchar->length ) == 0 )
        {
            if ( sHour >= 12 )
            {
                // ������ ������ üũ
                if ( sDay == sLastDays[sMonth] )
                {
                    if ( sMonth == 12 )
                    {
                        sYear++;
                        sMonth = 1;
                        sDay = 1;
                    }
                    else
                    {
                        sMonth++;
                        sDay = 1;
                    }
                }
                else
                {
                    /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                    if ( ( sYear == 1582 ) &&
                         ( sMonth == 10 ) &&
                         ( sDay == 4 ) )
                    {
                        sDay += 10;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    sDay++;
                }
            }
            // 12�ø� ���� ������ �� ���ϴ� ������.
            else
            {
                // nothing to do
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 0, 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchHour( sVarchar->value,
                                                   sVarchar->length ) == 0 )
        {
            if ( sMinute >= 30 )
            {
                if ( sHour == 23 )
                {
                    // ������ ������ üũ
                    if ( sDay == sLastDays[sMonth] )
                    {
                        if ( sMonth == 12 )
                        {
                            sYear++;
                            sMonth = 1;
                            sDay = 1;
                            sHour = 0;
                        }
                        else
                        {
                            sMonth++;
                            sDay = 1;
                            sHour = 0;
                        }
                     }
                     else
                     {
                         /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                         if ( ( sYear == 1582 ) &&
                              ( sMonth == 10 ) &&
                              ( sDay == 4 ) )
                         {
                             sDay += 10;
                         }
                         else
                         {
                             /* Nothing to do */
                         }

                         sDay++;
                         sHour = 0;
                     }
                }
                else
                {
                    sHour++;
                }
            }
            // 30���� ���� ������ �� ���ϴ� ������.
            else
            {
                // nothing to do
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 sHour,
                                                 0, 0, 0)
                      != IDE_SUCCESS );
        }
        else if( sLanguage->extractSet->matchMinute( sVarchar->value,
                                                     sVarchar->length ) == 0 )
        {
            if ( sSecond >= 30 )
            {
                if ( sMinute == 59 )
                {
                    if ( sHour == 23 )
                    {
                        // ������ ������ üũ
                        if ( sDay == sLastDays[sMonth] )
                        {
                            if ( sMonth == 12 )
                            {
                                sYear++;
                                sMonth = 1;
                                sDay = 1;
                                sHour = 0; 
                                sMinute = 0;
                            }
                            else
                            {
                                sMonth++;
                                sDay = 1;
                                sHour = 0; 
                                sMinute = 0;
                            }
                        }
                        else
                        {
                            /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                            if ( ( sYear == 1582 ) &&
                                 ( sMonth == 10 ) &&
                                 ( sDay == 4 ) )
                            {
                                sDay += 10;
                            }
                            else
                            {
                                /* Nothing to do */
                            }

                            sDay++;
                            sHour = 0;
                            sMinute = 0;
                        }
                    }
                    else
                    {
                        sHour++;
                        sMinute = 0;
                    }
                }
                else
                {
                    sMinute++;
                }
            }
            // 30�ʸ� ���� ������ �� ���ϴ� ������.
            else
            {
                // nothing to do
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            IDE_TEST( mtdDateInterface::makeDate(sValue,
                                                 sYear,
                                                 sMonth,
                                                 sDay,
                                                 sHour,
                                                 sMinute,
                                                 0, 0)
                      != IDE_SUCCESS );
        }
        /* BUG-45730 ROUND, TRUNC �Լ����� DATE ���� IW �߰� ���� */
        else if( sLanguage->extractSet->matchISOWeek( sVarchar->value,
                                                      sVarchar->length ) == 0 )
        {
            /* �Ͽ��Ϻ��� 0, 1, 2, 3, 4, 5, 6 ������ 6, 0, 1, 2, 3, 4, 5 ���� ���� */
            sDayOfWeek = ( ( mtc::dayOfWeek( sYear, sMonth, sDay ) + 6 ) % 7 );

            /* �ݿ��Ϻ��� ���� �����Ϸ� �ø�. */
            if ( sDayOfWeek >= 4 )
            {
                /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
                if ( ( sYear == 1582 ) &&
                     ( sMonth == 10 ) &&
                     ( sDay == 4 ) )
                {
                    sDay += 10;
                }
                else
                {
                    /* Nothing to do */
                }

                sDay += ( 7 - sDayOfWeek );

                /* = ���� sDay�� �ش� �޿� ������ ���̵� �������. */
                if ( sDay > sLastDays[sMonth] )
                {
                    sDay -= sLastDays[sMonth];

                    sMonth++;

                    if ( sMonth > 12 )
                    {
                        sYear++;
                        sMonth = 1;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                sDay -= sDayOfWeek;

                if ( sDay < 1 )
                {
                    sMonth--;

                    if ( sMonth < 1 )
                    {
                        sYear--;

                        sMonth = 12;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    sDay = sLastDays[sMonth] + sDay;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            IDE_TEST_RAISE( ( sYear < -9999 ) || ( sYear > 9999 ),
                            ERR_INVALID_YEAR );

            IDE_TEST( mtdDateInterface::makeDate( sValue,
                                                  sYear,
                                                  sMonth,
                                                  sDay,
                                                  0,
                                                  0,
                                                  0,
                                                  0 )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_RAISE(ERR_INVALID_LITERAL);
        }
    } 
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION( ERR_INVALID_YEAR );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_YEAR));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

} 
