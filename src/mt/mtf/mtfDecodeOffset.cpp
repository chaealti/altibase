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
 * $Id: mtfDecodeOffset.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtdModule mtdVarchar;
extern mtdModule mtdNchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdFloat;
extern mtdModule mtdInteger;
extern mtdModule mtdDate;
extern mtdModule mtdInterval;

static mtcName mtfDecodeOffsetFunctionName[1] = {
    { NULL, 13, (void*)"DECODE_OFFSET" }
};

static IDE_RC mtfDecodeOffsetEstimate( mtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       mtcCallBack* aCallBack );

mtfModule mtfDecodeOffset= {
    1 | MTC_NODE_OPERATOR_FUNCTION | MTC_NODE_EAT_NULL_TRUE,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfDecodeOffsetFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfDecodeOffsetEstimate
};

IDE_RC mtfDecodeOffsetCalculate( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfDecodeOffsetCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfDecodeOffsetEstimate( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt      /* aRemain */,
                                mtcCallBack* aCallBack )
{
    UInt             sCount;
    UInt             sFence;
    UInt             sGroups[MTD_GROUP_MAXIMUM];
    const mtdModule* sModules[MTC_NODE_ARGUMENT_COUNT_MAXIMUM];
    idBool           sNvarchar = ID_FALSE;

    // Argument�� �ּ� �ΰ� �־�� �Ѵ�. EX : DECODE_OFFSET( 1, A )
    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_DISTINCT_MASK ) == MTC_NODE_DISTINCT_TRUE,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // DECODE_OFFSET�� ù �� ° ���ڴ� offset�� ��Ÿ���� �������� �´�.
    sModules[0] = &mtdInteger;

    // Initialization for type group
    for ( sCount = 0; sCount < MTD_GROUP_MAXIMUM; sCount++ )
    {
        sGroups[sCount] = 0;
    }

    // �� �� ° ���ں���, � �׷쿡 ���ϴ� type���� ���Ѵ�.
    for ( sCount = 2; sCount <= sFence; sCount++ )
    {
        sGroups[aStack[sCount].column->module->flag & MTD_GROUP_MASK] = 1;
    }

    if ( ( sGroups[MTD_GROUP_NUMBER] != 0 ) && ( sGroups[MTD_GROUP_TEXT] == 0  ) &&
         ( sGroups[MTD_GROUP_DATE]   == 0 ) && ( sGroups[MTD_GROUP_MISC] == 0 ) &&
         ( sGroups[MTD_GROUP_INTERVAL] == 0 )  )
    {
        // ���� �� �� ���� ��� ���� Ÿ���� float�� �ȴ�.
        sModules[1] = &mtdFloat;
    }
    else if ( ( sGroups[MTD_GROUP_NUMBER] == 0 ) && ( sGroups[MTD_GROUP_TEXT] == 0 ) &&
              ( sGroups[MTD_GROUP_DATE] != 0 ) && ( sGroups[MTD_GROUP_MISC] == 0 ) &&
              ( sGroups[MTD_GROUP_INTERVAL] == 0 ) )
    {
        // Date �� �� ���� ��� ���� Ÿ���� date�� �ȴ�.
        sModules[1] = &mtdDate;
    }
    else if ( ( sGroups[MTD_GROUP_NUMBER] == 0 ) && ( sGroups[MTD_GROUP_TEXT] == 0 ) &&
              ( sGroups[MTD_GROUP_DATE] == 0 ) && ( sGroups[MTD_GROUP_MISC] == 0 ) &&
              ( sGroups[MTD_GROUP_INTERVAL] != 0 ) )
    {
        // Interval �� �� ���� ��� ���� Ÿ���� interval�� �ȴ�.
        sModules[1] = &mtdInterval;
    }
    else
    {
        // NULL Ÿ���� ������ �� ���� Ÿ�Ե��� ���� �� ���,
        // ���� Ÿ���� varchar�� �ȴ�.
        for ( sCount = 2; sCount <= sFence; sCount++ )
        {
            // Ư����, Nvarchar �Ǵ� Nchar�� ���� �� ��� ���� Ÿ���� Nvarchar�� �����ȴ�.
            if ( ( aStack[sCount].column->module->id == mtdNvarchar.id ) ||
                 ( aStack[sCount].column->module->id == mtdNchar.id    ) )
            {
                sNvarchar = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do
            }
        }

        if ( sNvarchar == ID_TRUE )
        {
            sModules[1] = &mtdNvarchar;
        }
        else
        {
            sModules[1] = &mtdVarchar;
        }
    }

    // ������ ������ ��ǥŸ��( ����Ÿ�� )���� ������ ��带 �����Ѵ�. ( ù �� ° ���ڴ� integer )
    for ( sCount = 2; sCount < sFence; sCount++ )
    {
        sModules[sCount] = sModules[1];
    }

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    // Byte�� length �ʵ尡 ����.
    if ( ( aStack[1].column->module->flag & MTD_NON_LENGTH_MASK )
         == MTD_NON_LENGTH_TYPE )
    {
        // Byte�� ��� Precision, Scale�� ������ ��쿡�� �����ϵ��� �Ѵ�.
        for ( sCount = 2; sCount <= sFence; sCount++ )
        {
            IDE_TEST_RAISE( !mtc::isSameType( aStack[1].column,
                                              aStack[sCount].column ),
                            ERR_INVALID_PRECISION );
        }
    }
    else
    {
        // Nothing to do.
    }

    // ���� Ÿ���� �ʱ�ȭ
    mtc::initializeColumn( aStack[0].column, aStack[2].column );

    for ( sCount = 3; sCount <= sFence; sCount++ )
    {
        // Size�� ���� ū column���� �ʱ�ȭ ȯ��.
        if ( aStack[0].column->column.size < aStack[sCount].column->column.size )
        {
            mtc::initializeColumn( aStack[0].column, aStack[sCount].column );
        }
        else
        {
            // Nothing to do.
        }
    }

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_INVALID_PRECISION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_PRECISION));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfDecodeOffsetCalculate( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*,
                                 mtcTemplate* aTemplate )
{
    mtcNode        * sNode;
    SInt             sValue;
    SInt             sCount;    
    UInt             sFence;
    mtcExecute     * sExecute;    

    IDE_TEST_RAISE( aRemain < 3, ERR_STACK_OVERFLOW );

    // Set return stack
    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack[0].value  = (void*)( (UChar*)aTemplate->rows[aNode->table].row
                                + aStack->column->column.offset );


    // Calculate the first argument( offset )
    sNode    = aNode->arguments;
    sExecute = MTC_TUPLE_EXECUTE( &aTemplate->rows[sNode->table], sNode );

    IDE_TEST( sExecute->calculate( sNode,
                                   aStack + 1,
                                   aRemain - 1,
                                   sExecute->calculateInfo,
                                   aTemplate )
              != IDE_SUCCESS );

    if ( sNode->conversion != NULL )
    {
        IDE_TEST( mtf::convertCalculate( sNode,
                                         aStack + 1,
                                         aRemain - 1,
                                         NULL,
                                         aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    sValue = *(mtdIntegerType*)aStack[1].value;

    if ( sValue == MTD_INTEGER_NULL )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

        if ( sValue == 0 )
        {
            sValue++;
        }
        else
        {
            // Nothing to do.
        }

        // DECODE_OFFSET( offset_value, 0 or 1 or -5, 2 or -4, 3 or -3, 4 or -2, 5 or -1 )
        if ( sValue < 0 )
        {
            sValue += sFence;
        }
        else
        {
            // Nothing to do.
        }

        // ������ ������ ����
        IDE_TEST_RAISE( ( sValue <= 0 ) || ( sValue > ( sFence - 1 ) ),
                         ERR_ARGUMENT_NOT_APPLICABLE );

        // Calculate the Nth( offset_value ) node
        for ( sNode = sNode->next, sCount = 0;
              sCount < sValue - 1;
              sNode = sNode->next, sCount++ )
        {
            // Nothing to do.
        }

        sExecute = MTC_TUPLE_EXECUTE( &aTemplate->rows[sNode->table], sNode );

        IDE_TEST( sExecute->calculate( sNode,
                                       aStack + 2,
                                       aRemain - 2,
                                       sExecute->calculateInfo,
                                       aTemplate )
                  != IDE_SUCCESS );

        if ( sNode->conversion != NULL )
        {
            IDE_TEST( mtf::convertCalculate( sNode, 
                                             aStack + 2,
                                             aRemain - 2,
                                             NULL,
                                             aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nohting to do.
        }

        if ( aStack[2].column->module->isNull( aStack[2].column,
                                               aStack[2].value )
             == ID_TRUE )
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
        }
        else
        {
            idlOS::memcpy( aStack[0].value,
                           aStack[2].value,
                           aStack[2].column->module->actualSize( aStack[2].column,
                                                                 aStack[2].value ) );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
