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
 * $Id: mtfStddevKeep.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfStddevKeep;

extern mtdModule mtdDouble;
extern mtdModule mtdBigint;
extern mtdModule mtdList;
extern mtdModule mtdBoolean;
extern mtdModule mtdBinary;

static mtcName mtfStddevKeepFunctionName[1] = {
    { NULL, 11, (void*)"STDDEV_KEEP" }
};

static IDE_RC mtfStddevKeepEstimate( mtcNode     * aNode,
                                     mtcTemplate * aTemplate,
                                     mtcStack    * aStack,
                                     SInt          aRemain,
                                     mtcCallBack * aCallBack );

mtfModule mtfStddevKeep = {
    5 | MTC_NODE_OPERATOR_AGGREGATION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (비교 연산자가 아님)
    mtfStddevKeepFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfStddevKeepEstimate
};

IDE_RC mtfStddevKeepInitialize( mtcNode     * aNode,
                                mtcStack    * aStack,
                                SInt          aRemain,
                                void        * aInfo,
                                mtcTemplate * aTemplate );

IDE_RC mtfStddevKeepAggregate( mtcNode     * aNode,
                               mtcStack    * aStack,
                               SInt          aRemain,
                               void        * aInfo,
                               mtcTemplate * aTemplate );

IDE_RC mtfStddevKeepFinalize( mtcNode     * aNode,
                              mtcStack    * aStack,
                              SInt          aRemain,
                              void        * aInfo,
                              mtcTemplate * aTemplate );

IDE_RC mtfStddevKeepCalculate( mtcNode     * aNode,
                               mtcStack    * aStack,
                               SInt          aRemain,
                               void        * aInfo,
                               mtcTemplate * aTemplate );

static const mtcExecute mtfExecute = {
    mtfStddevKeepInitialize,
    mtfStddevKeepAggregate,
    mtf::calculateNA,
    mtfStddevKeepFinalize,
    mtfStddevKeepCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfStddevKeepEstimate( mtcNode     * aNode,
                              mtcTemplate * aTemplate,
                              mtcStack    * aStack,
                              SInt          /*aRemain*/,
                              mtcCallBack * aCallBack )
{
    mtcNode                * sNode;
    static const mtdModule * sModules[1] = {
        &mtdDouble
    };

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) <= 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sNode = aNode->arguments->next;
    aNode->arguments->next = NULL;
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );
    aNode->arguments->next = sNode;

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfExecute;

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 0,
                                     & mtdDouble,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                     & mtdDouble,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 2,
                                     & mtdDouble,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 3,
                                     & mtdBigint,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 4,
                                     &mtdBinary,
                                     1,
                                     ID_SIZEOF( mtfKeepOrderData * ),
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( ( aStack[1].column->module == &mtdList ) ||
                    ( aStack[1].column->module == &mtdBoolean ),
                    ERR_CONVERSION_NOT_APPLICABLE );

    // funcData 사용
    aNode->info = aTemplate->funcDataCnt;
    aTemplate->funcDataCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    }

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfStddevKeepInitialize( mtcNode     * aNode,
                                mtcStack    *,
                                SInt,
                                void        *,
                                mtcTemplate * aTemplate )
{
    const mtcColumn       * sColumn;
    iduMemory             * sMemoryMgr = NULL;
    mtfFuncDataBasicInfo  * sFuncData;
    mtfKeepOrderData      * sKeepOrderData;
    mtdBinaryType         * sBinary;

    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;

    *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[0].column.offset) = 0;
    *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[1].column.offset) = 0;
    *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[2].column.offset) = 0;
    *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[3].column.offset) = 0;

    sBinary = (mtdBinaryType*)((UChar*)aTemplate->rows[aNode->table].row +
                                        sColumn[4].column.offset);

    if ( aTemplate->funcData[aNode->info] == NULL )
    {
        IDE_TEST( mtf::allocFuncDataMemory( &sMemoryMgr )
                  != IDE_SUCCESS );
        // function data alloc
        IDU_FIT_POINT_RAISE( "mtfStddevKeepInitialize::alloc::sFuncData",
                             ERR_MEMORY_ALLOCATION );
        IDE_TEST_RAISE( sMemoryMgr->alloc( ID_SIZEOF(mtfFuncDataBasicInfo),
                                           (void**)&sFuncData )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOCATION );
        // function data init
        IDE_TEST( mtf::initializeFuncDataBasicInfo( sFuncData,
                                                    sMemoryMgr )
                  != IDE_SUCCESS );
        // 등록
        aTemplate->funcData[aNode->info] = sFuncData;
    }
    else
    {
        sFuncData = aTemplate->funcData[aNode->info];
    }

    IDU_FIT_POINT_RAISE( "mtfStddevKeepInitialize::alloc::sKeepOrderData",
                         ERR_MEMORY_ALLOCATION );
    IDE_TEST_RAISE( sFuncData->memoryMgr->alloc( ID_SIZEOF( mtfKeepOrderData ),
                                                 (void**)&sKeepOrderData )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOCATION );

    sKeepOrderData->mIsFirst               = ID_TRUE;
    sBinary->mLength                       = ID_SIZEOF( mtfKeepOrderData * );
    *((mtfKeepOrderData**)sBinary->mValue) = sKeepOrderData;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOCATION )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sMemoryMgr != NULL )
    {
        mtf::freeFuncDataMemory( sMemoryMgr );
        aTemplate->funcData[aNode->info] = NULL;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

IDE_RC mtfStddevKeepAggregate( mtcNode     * aNode,
                               mtcStack    * aStack,
                               SInt          aRemain,
                               void        *,
                               mtcTemplate * aTemplate )
{
    const mtcColumn      * sColumn;
    mtfKeepOrderData     * sKeepOrderData;
    mtfFuncDataBasicInfo * sFuncData;
    mtdBinaryType        * sBinary;
    mtdCharType          * sOption;
    UInt                   sAction;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     NULL,
                                     aTemplate )
              != IDE_SUCCESS );

    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sBinary = (mtdBinaryType*)((UChar*)aTemplate->rows[aNode->table].row +
                               sColumn[4].column.offset);
    sKeepOrderData = *((mtfKeepOrderData**)sBinary->mValue);

    if ( sKeepOrderData->mIsFirst == ID_TRUE )
    {
        sKeepOrderData->mIsFirst = ID_FALSE;
        sOption                  = (mtdCharType *)aStack[2].value;
        sFuncData                = aTemplate->funcData[aNode->info];

        IDE_TEST( mtf::setKeepOrderData( aNode->funcArguments->next,
                                         aStack,
                                         sFuncData->memoryMgr,
                                         (UChar *)sOption->value,
                                         sKeepOrderData,
                                         MTF_KEEP_ORDERBY_POS )
                  != IDE_SUCCESS );

        if ( aStack[1].column->module->isNull( aStack[1].column,
                                               aStack[1].value )
             != ID_TRUE )
        {
            *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                    sColumn[1].column.offset) =
                        *(mtdDoubleType*) aStack[1].value * *(mtdDoubleType*)aStack[1].value;
            *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                    sColumn[2].column.offset) = *(mtdDoubleType*)aStack[1].value;
            *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                    sColumn[3].column.offset) = 1;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        mtf::getKeepAction( aStack + MTF_KEEP_ORDERBY_POS,
                            sKeepOrderData,
                            &sAction );

        if ( sAction == MTF_KEEP_ACTION_INIT )
        {
            if ( aStack[1].column->module->isNull( aStack[1].column,
                                                   aStack[1].value )
                 != ID_TRUE )
            {
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[1].column.offset) =
                            *(mtdDoubleType*) aStack[1].value * *(mtdDoubleType*)aStack[1].value;
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[2].column.offset) = *(mtdDoubleType*)aStack[1].value;
                *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                        sColumn[3].column.offset) = 1;
            }
            else
            {
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[1].column.offset) = 0;
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[2].column.offset) = 0;
                *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[3].column.offset) = 0;
            }
        }
        else if ( sAction == MTF_KEEP_ACTION_AGGR )
        {
            if ( aStack[1].column->module->isNull( aStack[1].column,
                                                   aStack[1].value )
                 != ID_TRUE )
            {
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                      sColumn[1].column.offset) +=
                        *(mtdDoubleType*) aStack[1].value * *(mtdDoubleType*)aStack[1].value;
                *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                      sColumn[2].column.offset) += *(mtdDoubleType*)aStack[1].value;
                *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                      sColumn[3].column.offset) += 1;
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfStddevKeepFinalize( mtcNode     * aNode,
                              mtcStack    *,
                              SInt,
                              void        *,
                              mtcTemplate * aTemplate )
{
    const mtcColumn * sColumn;
    mtdBigintType     sCount;
    mtdDoubleType     sPow;
    mtdDoubleType     sSum;
    void            * sValueTemp;

    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;

    sCount = *(mtdBigintType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[3].column.offset);
    if ( sCount == 0 )
    {
        sValueTemp = (void*)mtd::valueForModule(
                                 (smiColumn*)sColumn + 0,
                                 aTemplate->rows[aNode->table].row,
                                 MTD_OFFSET_USE,
                                 sColumn->module->staticNull );

        sColumn[0].module->null( sColumn + 0,
                                 sValueTemp );
    }
    else if ( sCount == 1 )
    {
        *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                            sColumn[0].column.offset) = 0;
    }
    else
    {
        sPow = *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[1].column.offset);
        sSum = *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
                                    sColumn[2].column.offset);
        *(mtdDoubleType*) ( (UChar*) aTemplate->rows[aNode->table].row +
            sColumn[0].column.offset) =
                idlOS::sqrt( idlOS::fabs( ( sPow - sSum * sSum / sCount ) / ( sCount - 1 ) ) );
    }

    return IDE_SUCCESS;
}

IDE_RC mtfStddevKeepCalculate( mtcNode     * aNode,
                               mtcStack    * aStack,
                               SInt,
                               void        *,
                               mtcTemplate * aTemplate )
{
    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack->value  = (void*)( (UChar*) aTemplate->rows[aNode->table].row +
                              aStack->column->column.offset );

    return IDE_SUCCESS;
}

