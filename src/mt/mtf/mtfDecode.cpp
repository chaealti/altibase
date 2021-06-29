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
 * $Id: mtfDecode.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfDecode;

extern mtdModule mtdList;
extern mtdModule mtdDate;
extern mtdModule mtdFloat;
extern mtdModule mtdInterval;
extern mtdModule mtdVarchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdNchar;
extern mtdModule mtdNull;

static mtcName mtfDecodeFunctionName[1] = {
    { NULL, 6, (void*)"DECODE" }
};

static IDE_RC mtfDecodeEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 mtcCallBack* aCallBack );

mtfModule mtfDecode = {
    1|MTC_NODE_OPERATOR_FUNCTION|MTC_NODE_EAT_NULL_TRUE,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfDecodeFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfDecodeEstimate
};

IDE_RC mtfDecodeCalculate( mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*        aInfo,
                           mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfDecodeCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfDecodeEstimate( mtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt      /* aRemain */,
                          mtcCallBack* aCallBack )
{
    UInt             sCount;
    UInt             sFence;
    UInt             sGroups[MTD_GROUP_MAXIMUM];
    const mtdModule* sModules[MTC_NODE_ARGUMENT_COUNT_MAXIMUM];
    const mtdModule* sTarget;
    idBool           sNcharExists = ID_FALSE;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 3,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    sFence      = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // BUG-21357
    // ����Ŭ�� �޸� ��Ƽ���̽����� �پ��� number type�� �ִ�.
    // expr�� �׻� ù��° search parameter�� Ÿ������ ��ȯ�� ���
    // ��쿡 ���� value overflow�� �߻��� �� �����Ƿ�
    // ��Ƽ���̽��� �پ��� number type�� ����Ͽ� expr�� ù��°
    // search parameter�� number type�� ��쿡 �̵��� comparison
    // module�� ���Ѵ�.
    if ( ( (aStack[1].column->module->flag & MTD_GROUP_MASK) == MTD_GROUP_NUMBER ) &&
         ( (aStack[2].column->module->flag & MTD_GROUP_MASK) == MTD_GROUP_NUMBER ) )
    {
        IDE_TEST( mtf::getComparisonModule( &sTarget,
                                            aStack[1].column->module->no,
                                            aStack[2].column->module->no )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sTarget == NULL,
                        ERR_CONVERSION_NOT_APPLICABLE );

        sModules[0] = sTarget;
    }
    else
    {
        // BUG-21357
        // �������� search parameter�� expr�� Ÿ������ ��ȯ�� �Ǿ�����,
        // �̴� ����Ŭ ����� �޶� ����ڰ� �ǵ����� ���� ����� �ʷ��� �� �ִ�.
        // ���� ����Ŭ ������ ���� expr�� ù��° search parameter�� Ÿ������
        // ��ȯ�ǵ��� �Ѵ�.
        sModules[0] = aStack[2].column->module;
        IDE_TEST_RAISE( sModules[0] == &mtdList, ERR_CONVERSION_NOT_APPLICABLE );

        // ù��° search parameter�� null�̸�
        // expr�� �ٸ� search parameter���� ù��° search parameter Ÿ������
        // ��ȯ�ϴ°� �Ұ����ϴ�.
        // �̷� ��� ù��° search parameter�� VARCHARŸ������ �����ϰ� ��ȯ�Ѵ�.
        if ( sModules[0] == &mtdNull )
        {
            sModules[0] = &mtdVarchar;
        }
        else
        {
            // Nothing to do.
        }
    }

    // To Fix PR-15766
    IDE_TEST_RAISE( mtf::isEquiValidType( sModules[0] )
                    != ID_TRUE, ERR_CONVERSION_NOT_APPLICABLE );

    for( sCount = 1; sCount < sFence; sCount += 2 )
    {
        sModules[sCount] = sModules[0];
    }
    for( sCount = 0; sCount < MTD_GROUP_MAXIMUM; sCount++ )
    {
        sGroups[sCount] = 0;
    }
    for( sCount = 3; sCount <= sFence; sCount += 2 )
    {
        // BUG-24024
        if( aStack[sCount].column->module->id != MTD_NULL_ID )
        {
            sGroups[aStack[sCount].column->module->flag&MTD_GROUP_MASK] = 1;
        }
    }
    if( ( sFence & 1 ) == 0 )
    {
        // BUG-24024
        if( aStack[sFence].column->module->id != MTD_NULL_ID )
        {
            sGroups[aStack[sFence].column->module->flag&MTD_GROUP_MASK] = 1;
        }
    }
    if( sGroups[MTD_GROUP_NUMBER] != 0 && sGroups[MTD_GROUP_TEXT] == 0 &&
        sGroups[MTD_GROUP_DATE]   == 0 && sGroups[MTD_GROUP_MISC] == 0 &&
        sGroups[MTD_GROUP_INTERVAL] == 0  )
    {
        sModules[2] = &mtdFloat;
    }
    else if( sGroups[MTD_GROUP_NUMBER] == 0 && sGroups[MTD_GROUP_TEXT] == 0 &&
             sGroups[MTD_GROUP_DATE]   != 0 && sGroups[MTD_GROUP_MISC] == 0 &&
             sGroups[MTD_GROUP_INTERVAL] == 0  )
    {
        sModules[2] = &mtdDate;
    }
    else if( sGroups[MTD_GROUP_NUMBER] == 0 && sGroups[MTD_GROUP_TEXT] == 0 &&
             sGroups[MTD_GROUP_DATE]   == 0 && sGroups[MTD_GROUP_MISC] == 0 &&
             sGroups[MTD_GROUP_INTERVAL] != 0  )
    {
        sModules[2] = &mtdInterval;
    }
    else
    {
        // PROJ-2523
        // Result �� �ϳ��� nvarchar, nchar Ÿ���� ��� ��� Ÿ���� nvarchar�� �Ѵ�.
        for ( sCount = 3; sCount <= sFence; sCount += 2 )
        {
            if ( ( aStack[ sCount ].column->module->id == mtdNvarchar.id ) ||
                 ( aStack[ sCount ].column->module->id == mtdNchar.id    ) )
            {
                sNcharExists = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( ( sNcharExists == ID_FALSE ) &&
             ( ( sFence & 1 ) == 0 ) &&
             ( ( aStack[ sFence ].column->module->id == mtdNvarchar.id ) ||
               ( aStack[ sFence ].column->module->id == mtdNchar.id ) ) )
        {
            sNcharExists = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        if ( sNcharExists == ID_TRUE )
        {
            sModules[2] = &mtdNvarchar;
        }
        else
        {
            sModules[2] = &mtdVarchar;
        }
    }
    for( sCount = 2; sCount < sFence; sCount += 2 )
    {
        sModules[sCount] = sModules[2];
    }
    if( ( sFence & 1 ) == 0 )
    {
        sModules[sFence-1] = sModules[2];
    }

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    if( ( aStack[3].column->module->flag & MTD_NON_LENGTH_MASK ) == MTD_NON_LENGTH_TYPE )
    {
        for( sCount = 5; sCount <= sFence; sCount += 2 )
        {
            IDE_TEST_RAISE( !mtc::isSameType( aStack[3].column,
                                              aStack[sCount].column ),
                            ERR_INVALID_PRECISION );
        }
        if( ( sFence & 1 ) == 0 )
        {
            IDE_TEST_RAISE( !mtc::isSameType( aStack[3].column,
                                              aStack[sFence].column ),
                            ERR_INVALID_PRECISION );
        }
    }

    // BUG-23102
    // mtcColumn���� �ʱ�ȭ�Ѵ�.
    mtc::initializeColumn( aStack[0].column, aStack[3].column );
    for( sCount = 5; sCount <= sFence; sCount += 2 )
    {
        if( aStack[0].column->column.size <
            aStack[sCount].column->column.size )
        {
            mtc::initializeColumn( aStack[0].column, aStack[sCount].column );
        }
    }
    if( ( sFence & 1 ) == 0 )
    {
        if(aStack[0].column->column.size<aStack[sFence].column->column.size)
        {
            mtc::initializeColumn( aStack[0].column, aStack[sFence].column );
        }
    }

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION( ERR_INVALID_PRECISION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_PRECISION));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfDecodeCalculate( mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*,
                           mtcTemplate* aTemplate )
{
    const mtdModule* sModule;
    mtcNode*         sNode;
    mtdValueInfo     sValueInfo1;
    mtdValueInfo     sValueInfo2;
    mtcExecute     * sExecute;
    
    IDE_TEST_RAISE( aRemain < 3, ERR_STACK_OVERFLOW );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack[0].value  = (void*)( (UChar*) aTemplate->rows[aNode->table].row
                                + aStack->column->column.offset );

    sNode    = aNode->arguments;
    sExecute = MTC_TUPLE_EXECUTE(&aTemplate->rows[sNode->table], sNode);
    
    IDE_TEST( sExecute->calculate( sNode,
                                   aStack + 1,
                                   aRemain - 1,
                                   sExecute->calculateInfo,
                                   aTemplate )
              != IDE_SUCCESS );

    if( sNode->conversion != NULL )
    {
        IDE_TEST( mtf::convertCalculate( sNode,
                                         aStack + 1,
                                         aRemain - 1,
                                         NULL,
                                         aTemplate )
                  != IDE_SUCCESS );
    }
    sModule = aStack[1].column->module;
    for( sNode = sNode->next; sNode != NULL; sNode = sNode->next->next )
    {
        sExecute = MTC_TUPLE_EXECUTE(&aTemplate->rows[sNode->table], sNode);

        IDE_TEST( sExecute->calculate( sNode,
                                       aStack + 2,
                                       aRemain - 2,
                                       sExecute->calculateInfo,
                                       aTemplate )
                  != IDE_SUCCESS );

        if( sNode->conversion != NULL )
        {
            IDE_TEST( mtf::convertCalculate( sNode, 
                                             aStack + 2,
                                             aRemain - 2,
                                             NULL,
                                             aTemplate )
                      != IDE_SUCCESS );
        }
        if( sNode->next != NULL )
        {
            sValueInfo1.column = aStack[1].column;
            sValueInfo1.value  = aStack[1].value;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aStack[2].column;
            sValueInfo2.value  = aStack[2].value;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
            
            if ( sModule->logicalCompare[MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                 &sValueInfo2 ) == 0 )
            {
                sNode    = sNode->next;
                sExecute = MTC_TUPLE_EXECUTE(&aTemplate->rows[sNode->table], sNode);

                IDE_TEST( sExecute->calculate( sNode,
                                               aStack + 2,
                                               aRemain - 2,
                                               sExecute->calculateInfo,
                                               aTemplate )
                          != IDE_SUCCESS );

                if( sNode->conversion != NULL )
                {
                    IDE_TEST( mtf::convertCalculate( sNode, 
                                                     aStack + 2,
                                                     aRemain - 2,
                                                     NULL,
                                                     aTemplate )
                              != IDE_SUCCESS );
                }
                idlOS::memcpy(                               aStack[0].value,
                                                             aStack[2].value,
                               aStack[2].column->module->actualSize(
                                                          aStack[2].column,
                                                           aStack[2].value ) );
                break;
            }
        }
        else
        {
            idlOS::memcpy(                                   aStack[0].value,
                                                             aStack[2].value,
                           aStack[2].column->module->actualSize(
                                                          aStack[2].column,
                                                           aStack[2].value ) );
            break;
        }
    }
    if( sNode == NULL )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
