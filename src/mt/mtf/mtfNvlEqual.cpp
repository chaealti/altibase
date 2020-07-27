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
 * $Id: mtfNvlEqual.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfNvlNotEqual;

extern mtfModule mtfNvlEqual;

extern mtdModule mtdList;

static mtcName mtfNvlEqualFunctionName[1] = {
    { NULL, 9, (void*)"NVL_EQUAL" }
};

static IDE_RC mtfNvlEqualEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

mtfModule mtfNvlEqual = {
    1|MTC_NODE_OPERATOR_EQUAL|MTC_NODE_COMPARISON_TRUE|
        MTC_NODE_FILTER_NEED|
        MTC_NODE_PRINT_FMT_PREFIX_PA,
    ~0,
    1.0/10.0,  // TODO : default selectivity
    mtfNvlEqualFunctionName,
    &mtfNvlNotEqual,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfNvlEqualEstimate
};

IDE_RC mtfNvlEqualEstimateRange( mtcNode*,
                                 mtcTemplate*,
                                 UInt,
                                 UInt*    aSize );

IDE_RC mtfNvlEqualExtractRange( mtcNode*       aNode,
                                mtcTemplate*   aTemplate,
                                mtkRangeInfo * aInfo,
                                smiRange*      aRange );

IDE_RC mtfNvlEqualCalculate( mtcNode*     aNode,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             void*        aInfo,
                             mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfNvlEqualCalculate,
    NULL,
    mtx::calculateNA,
    mtfNvlEqualEstimateRange,
    mtfNvlEqualExtractRange
};

IDE_RC mtfNvlEqualEstimate( mtcNode*     aNode,
                            mtcTemplate* aTemplate,
                            mtcStack*    aStack,
                            SInt      /* aRemain */,
                            mtcCallBack* aCallBack )
{
    extern mtdModule mtdBoolean;

    mtcNode* sNode;

    const mtdModule* sModules[3];

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 3,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sNode = aNode->arguments;
    if ( ( sNode->lflag & MTC_NODE_COMPARISON_MASK ) ==
         MTC_NODE_COMPARISON_TRUE )
    {
        sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    }
    else
    {
        /* Nothing to do */
    }
    
    aNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    aNode->lflag |= sNode->lflag & MTC_NODE_INDEX_MASK;
    for( sNode  = sNode->next;
         sNode != NULL;
         sNode  = sNode->next )
    {
        sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    }

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdBoolean,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtf::getComparisonModule( &sModules[0],
                                        aStack[1].column->module->no,
                                        aStack[2].column->module->no )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sModules[0] == NULL, ERR_CONVERSION_NOT_APPLICABLE );

    IDE_TEST_RAISE( mtf::isEquiValidType( sModules[0] ) != ID_TRUE,
                    ERR_CONVERSION_NOT_APPLICABLE );

    IDE_TEST( mtf::getComparisonModule( &sModules[0],
                                        sModules[0]->no,
                                        aStack[3].column->module->no )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sModules[0] == NULL, ERR_CONVERSION_NOT_APPLICABLE );

    // To Fix BUG-15205
    IDE_TEST_RAISE( mtf::isEquiValidType( sModules[0] ) != ID_TRUE,
                    ERR_CONVERSION_NOT_APPLICABLE );

    sModules[1] = sModules[0];
    sModules[2] = sModules[0];

    IDE_TEST_RAISE( sModules[0] == &mtdList, ERR_CONVERSION_NOT_APPLICABLE );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfNvlEqualEstimateRange( mtcNode*,
                                 mtcTemplate*,
                                 UInt,
                                 UInt*    aSize )
{
    *aSize = ( ID_SIZEOF(smiRange) + ( ID_SIZEOF(mtkRangeCallBack) << 1 ) ) << 1;
    
    return IDE_SUCCESS;
}

IDE_RC mtfNvlEqualExtractRange( mtcNode*       aNode,
                                mtcTemplate*   aTemplate,
                                mtkRangeInfo * aInfo,
                                smiRange*      aRange )
{
    mtcNode*          sIndexNode;
    mtcNode*          sValueNode1;
    mtcNode*          sValueNode2 = NULL;
    mtkRangeCallBack* sMinimumCallBack;
    mtkRangeCallBack* sMaximumCallBack;
    mtcColumn*        sValueColumn;
    void*             sValue;
    void*             sValueTemp;
    
    // NVL_EQUAL( idxCol, value1, value2 )
    // value2로 keyrange 구성
    sIndexNode  = aNode->arguments;
    sValueNode1 = sIndexNode->next;
    sValueNode2 = sValueNode1->next;

    sValueNode1 = mtf::convertedNode( sValueNode1, aTemplate );
    sValueNode2 = mtf::convertedNode( sValueNode2, aTemplate );

    sMinimumCallBack = (mtkRangeCallBack*)( aRange + 1 );
    sMaximumCallBack = sMinimumCallBack + 1;
    
    aRange->prev           = NULL;
    aRange->next           = (smiRange*)( sMaximumCallBack + 1 );

    aRange->minimum.data   = sMinimumCallBack;
    aRange->maximum.data   = sMaximumCallBack;
    sMinimumCallBack->next = NULL;
    sMaximumCallBack->next = NULL;
    sMinimumCallBack->flag = 0;
    sMaximumCallBack->flag = 0;
    
    sValueColumn = aTemplate->rows[sValueNode2->table].columns
        + sValueNode2->column;
    sValue       = aTemplate->rows[sValueNode2->table].row;

    sValueTemp = (void*)mtd::valueForModule(
        (smiColumn*)sValueColumn,
        sValue,
        MTD_OFFSET_USE,
        sValueColumn->module->staticNull );

    if ( sValueColumn->module->isNull( sValueColumn,
                                       sValueTemp ) == ID_TRUE )
    {
        aRange->next                 = NULL;
        
        //---------------------------
        // RangeCallBack 설정
        //---------------------------
        
        if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
             aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
        {
            // mtd type의 column value에 대한 range callback
            aRange->minimum.callback     = mtk::rangeCallBackGT4Mtd;
            aRange->maximum.callback     = mtk::rangeCallBackLT4Mtd;
        }
        else
        {
            if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                 ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
            {
                /* MTD_COMPARE_STOREDVAL_MTDVAL
                   stored type의 column value에 대한 range callback */
                aRange->minimum.callback     = mtk::rangeCallBackGT4Stored;
                aRange->maximum.callback     = mtk::rangeCallBackLT4Stored;
            }
            else
            {
                /* PROJ-2433 */
                aRange->minimum.callback     = mtk::rangeCallBackGT4IndexKey;
                aRange->maximum.callback     = mtk::rangeCallBackLT4IndexKey;
            }
        }
        
        sMinimumCallBack->compare    = mtk::compareMinimumLimit;
        sMinimumCallBack->columnIdx  = aInfo->columnIdx;
        sMinimumCallBack->value      = NULL;

        sMaximumCallBack->compare    = mtk::compareMinimumLimit;
        sMaximumCallBack->columnIdx  = aInfo->columnIdx;
        sMaximumCallBack->value      = NULL;
    }
    else
    {
        //---------------------------
        // RangeCallBack 설정 
        //---------------------------
        
        if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
             aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
        {
            // mtd type의 column value에 대한 range callback 
            aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
            aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
        }
        else
        {
            if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                 ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
            {
                /* MTD_COMPARE_STOREDVAL_MTDVAL
                   stored type의 column value에 대한 range callback */
                aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
            }
            else
            {
                /* PROJ-2433 */
                aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
            }
        }

        //----------------------------------------------
        // MinimumCallBack & MaximumCallBack 정보 설정
        //----------------------------------------------
        
        sMinimumCallBack->columnIdx  = aInfo->columnIdx;
        sMinimumCallBack->columnDesc = *aInfo->column;
        sMinimumCallBack->valueDesc  = *sValueColumn;

        sMinimumCallBack->value      = sValue;

        sMaximumCallBack->columnIdx  = aInfo->columnIdx;
        sMaximumCallBack->columnDesc = *aInfo->column;
        sMaximumCallBack->valueDesc  = *sValueColumn;

        sMaximumCallBack->value      = sValue;

        // PROJ-1364
        if ( aInfo->isSameGroupType == ID_FALSE )
        {
            sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
            sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

            sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
            sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

            if ( aInfo->direction == MTD_COMPARE_ASCENDING )
            {
                sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;

                sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
            }
            else
            {
                sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;

                sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
            }
            
            sMinimumCallBack->compare    =
                aInfo->column->module->keyCompare[aInfo->compValueType]
                                                 [aInfo->direction];
            
            sMaximumCallBack->compare    =
                aInfo->column->module->keyCompare[aInfo->compValueType]
                                                 [aInfo->direction];
        }
        else
        {
            sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
            sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

            sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
            sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

            if ( aInfo->direction == MTD_COMPARE_ASCENDING )
            {
                sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;

                sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
            }
            else
            {
                sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                
                sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
            }
            
            sMinimumCallBack->compare = sMaximumCallBack->compare =
                mtd::findCompareFunc( aInfo->column,
                                      sValueColumn,
                                      aInfo->compValueType,
                                      aInfo->direction );
        }
        
        /* or NULL range */
        aRange->next->prev = aRange;
        aRange             = aRange->next;
        
        sMinimumCallBack = (mtkRangeCallBack*)( aRange + 1 );
        sMaximumCallBack = sMinimumCallBack + 1;
        
        aRange->next                 = NULL;

        //---------------------------
        // RangeCallBack 설정 
        //---------------------------

        if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
             aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
        {
            aRange->minimum.callback = mtk::rangeCallBackGE4Mtd;
            aRange->maximum.callback = mtk::rangeCallBackLE4Mtd;
        }
        else
        {
            if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                 ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
            {
                aRange->minimum.callback = mtk::rangeCallBackGE4Stored;
                aRange->maximum.callback = mtk::rangeCallBackLE4Stored;
            }
            else
            {
                /* PROJ-2433 */
                aRange->minimum.callback = mtk::rangeCallBackGE4IndexKey;
                aRange->maximum.callback = mtk::rangeCallBackLE4IndexKey;
            }
        }
        
        aRange->minimum.data         = sMinimumCallBack;
        aRange->maximum.data         = sMaximumCallBack;

        //---------------------------
        // MinimumCallBack 정보 설정
        //---------------------------
                
        sMinimumCallBack->next       = NULL;
        sMinimumCallBack->columnDesc = *aInfo->column;
        sMinimumCallBack->columnIdx  = aInfo->columnIdx;
        //sMinimumCallBack->valueDesc  = NULL;
        sMinimumCallBack->value      = NULL;
        
        sMaximumCallBack->next       = NULL;
        sMaximumCallBack->columnDesc = *aInfo->column;
        sMaximumCallBack->columnIdx  = aInfo->columnIdx;
        //sMaximumCallBack->valueDesc  = NULL;
        sMaximumCallBack->value      = NULL;

        //---------------------------
        // MaximumCallBack 정보 설정
        //---------------------------

        if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
             ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
             ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
             ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
        {
            sMinimumCallBack->compare = mtk::compareMaximumLimit4Mtd;
            sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
        }
        else
        {
            sMinimumCallBack->compare = mtk::compareMaximumLimit4Stored;
            sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
        }
    } /* sValueColumn->module->isNull() */
    return IDE_SUCCESS;
}

IDE_RC mtfNvlEqualCalculate( mtcNode*     aNode,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             void*        aInfo,
                             mtcTemplate* aTemplate )
{
    const mtdModule * sModule;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( aStack[1].column->module->isNull ( aStack[1].column,
                                            aStack[1].value ) != ID_TRUE )
    {
        sModule = aStack[1].column->module;

        if ( sModule->isNull( aStack[3].column,
                              aStack[3].value ) == ID_TRUE )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
        }
        else
        {
            sValueInfo1.column = aStack[1].column;
            sValueInfo1.value  = aStack[1].value;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aStack[3].column;
            sValueInfo2.value  = aStack[3].value;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
            
            if ( sModule->logicalCompare[MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                 &sValueInfo2 ) == 0 )
            {
                *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
            }
            else
            {
                *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
            }
        }
        
    }
    else
    {
        sModule = aStack[2].column->module;

        if ( ( sModule->isNull( aStack[2].column,
                                aStack[2].value ) == ID_TRUE ) ||
             ( sModule->isNull( aStack[3].column,
                                aStack[3].value ) == ID_TRUE ) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
        }
        else
        {
            sValueInfo1.column = aStack[2].column;
            sValueInfo1.value  = aStack[2].value;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aStack[3].column;
            sValueInfo2.value  = aStack[3].value;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
            
            if ( sModule->logicalCompare[MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                 &sValueInfo2 ) == 0 )
            {
                *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
            }
            else
            {
                *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
            }
        }
        
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
