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
 * $Id$
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtfModule mtfDecodeCountList;

extern mtdModule mtdBigint;
extern mtdModule mtdList;
extern mtdModule mtdBinary;

static mtcName mtfDecodeCountListFunctionName[1] = {
    { NULL, 17, (void*)"DECODE_COUNT_LIST" }
};

static IDE_RC mtfDecodeCountListEstimate( mtcNode*     aNode,
                                          mtcTemplate* aTemplate,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          mtcCallBack* aCallBack );

mtfModule mtfDecodeCountList = {
    2|MTC_NODE_OPERATOR_AGGREGATION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfDecodeCountListFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfDecodeCountListEstimate
};

static IDE_RC mtfDecodeCountListEstimate( mtcNode*     aNode,
                                          mtcTemplate* aTemplate,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          mtcCallBack* aCallBack );

IDE_RC mtfDecodeCountListInitialize( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

IDE_RC mtfDecodeCountListAggregate( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

IDE_RC mtfDecodeCountListFinalize( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

IDE_RC mtfDecodeCountListCalculate( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

static const mtcExecute mtfDecodeCountListExecute = {
    mtfDecodeCountListInitialize,
    mtfDecodeCountListAggregate,
    mtf::calculateNA,
    mtfDecodeCountListFinalize,
    mtfDecodeCountListCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

typedef struct mtfDecodeCountSortedValue
{
    mtcColumn   * column;
    void        * value;
    SInt          idx;
} mtfDecodeCountSortedValue;

typedef struct mtfDecodeCountListCalculateInfo
{
    SInt                         sSearchCount;
    mtfDecodeCountSortedValue  * sSearchValue;  // sorted value

} mtfDecodeCountListCalculateInfo;

typedef struct mtfDecodeCountListInfo
{
    // ù��° ����
    mtcExecute     * sCountColumnExecute;
    mtcNode        * sCountColumnNode;

    // �ι�° ����
    mtcExecute     * sExprExecute;
    mtcNode        * sExprNode;

    // return ����
    mtcColumn      * sReturnColumn;
    void           * sReturnValue;
    mtcStack       * sReturnStack;
    SInt             sReturnCount;

} mtfDecodeCountListInfo;

extern "C" SInt
compareDecodeCountSortedValue( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    mtfDecodeCountSortedValue  * sElem1 = (mtfDecodeCountSortedValue*)aElem1;
    mtfDecodeCountSortedValue  * sElem2 = (mtfDecodeCountSortedValue*)aElem2;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;

    IDE_ASSERT( sElem1 != NULL );
    IDE_ASSERT( sElem2 != NULL );

    sValueInfo1.column = (const mtcColumn *)sElem1->column;
    sValueInfo1.value  = sElem1->value;
    sValueInfo1.flag   = MTD_OFFSET_USELESS;
                
    sValueInfo2.column = (const mtcColumn *)sElem2->column;
    sValueInfo2.value  = sElem2->value;
    sValueInfo2.flag   = MTD_OFFSET_USELESS;

    IDE_DASSERT( sElem1->column->module->no == sElem2->column->module->no );

    return sElem1->column->module->logicalCompare[MTD_COMPARE_ASCENDING]
        ( &sValueInfo1, &sValueInfo2 );
}

IDE_RC mtfDecodeCountListEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt,
                                   mtcCallBack* aCallBack )
{
#define IDE_FN "IDE_RC mtfDecodeCountListEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtdModule * sTarget;
    const mtdModule * sModules[MTC_NODE_ARGUMENT_COUNT_MAXIMUM];
    mtcColumn       * sMtcColumn;
    mtcStack        * sListStack;
    mtcNode         * sNode;
    idBool            sIsConstValue;
    UInt              sBinaryPrecision;
    SInt              sListCount = 1;
    SInt              sCount;
    UInt              sFence;

    mtfDecodeCountListCalculateInfo * sCalculateInfo;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;
    
    sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    /* BUG-44109 pivot ������ transform �Լ��� list �� decode �Լ�����
       �߸��� ���� ������ ����� ��� �����������մϴ�.  */
    IDE_TEST_RAISE( ( sFence != 1 ) && ( sFence != 3 ),
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST_RAISE( aStack[1].column->module == &mtdList,
                    ERR_CONVERSION_NOT_APPLICABLE );
    
    if ( sFence == 3 )
    {
        IDE_TEST_RAISE( aStack[2].column->module == &mtdList,
                        ERR_CONVERSION_NOT_APPLICABLE );
        
        if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK )
             == MTC_NODE_REESTIMATE_FALSE )
        {
            if ( aStack[3].column->module != &mtdList )
            {
                // decode_Count_list( i1, i2, 1 )
        
                IDE_TEST( mtf::getComparisonModule(
                              &sTarget,
                              aStack[2].column->module->no,
                              aStack[3].column->module->no )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE( sTarget == NULL,
                                ERR_CONVERSION_NOT_APPLICABLE );

                // To Fix PR-15208
                IDE_TEST_RAISE( mtf::isEquiValidType( sTarget ) != ID_TRUE,
                                ERR_CONVERSION_NOT_APPLICABLE );
        
                sModules[0] = aStack[1].column->module;
                sModules[1] = sTarget;
                sModules[2] = sTarget;
            
                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments,
                                                    aTemplate,
                                                    aStack + 1,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );
            }
            else
            {
                // decode_Count_list( i1, i2, (1,2,3,...) )
                //
                // aStack[2] = i2
                // aStack[3] = (1,2,3,...)
                // sListStack = (mtcStack*)aStack[3].value
                // sListStack[0] = 1
                // sListStack[1] = 2
                
                sListStack = (mtcStack*)aStack[3].value;
                sListCount = aStack[3].column->precision;

                /* BUG-40349 sListCount�� 2�̻��̾�� �Ѵ�. */
                IDE_TEST_RAISE( sListCount < 2, ERR_LIST_COUNT );

                // list�� ��� element�� ������ type���� convert�Ǿ�� �ϹǷ�
                // list�� ù��° element�� �����.
                IDE_TEST_RAISE( sListStack[0].column->module == &mtdList,
                                ERR_CONVERSION_NOT_APPLICABLE );
        
                IDE_TEST( mtf::getComparisonModule(
                              &sTarget,
                              aStack[2].column->module->no,
                              sListStack[0].column->module->no )
                          != IDE_SUCCESS );
            
                IDE_TEST_RAISE( sTarget == NULL,
                                ERR_CONVERSION_NOT_APPLICABLE );
                
                // To Fix PR-15208
                IDE_TEST_RAISE( mtf::isEquiValidType( sTarget ) != ID_TRUE,
                                ERR_CONVERSION_NOT_APPLICABLE );
        
                for ( sCount = 0; sCount < sListCount; sCount++ )
                {
                    IDE_TEST_RAISE( sListStack[sCount].column->module == &mtdList,
                                    ERR_CONVERSION_NOT_APPLICABLE );
                
                    sModules[sCount] = sTarget;
                }
        
                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments->next->next->arguments,
                                                    aTemplate,
                                                    sListStack,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );
            
                sModules[0] = aStack[1].column->module;
                sModules[1] = sTarget;
                sModules[2] = & mtdList;
                
                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments,
                                                    aTemplate,
                                                    aStack + 1,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );
            }
            
            if ( sListCount == 1 )
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfDecodeCountListExecute;

                // Count ����� ������
                IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                                 & mtdBigint,
                                                 0,
                                                 0,
                                                 0 )
                          != IDE_SUCCESS );
        
                // Count info ������ mtdBinary�� ����
                sBinaryPrecision = ID_SIZEOF(mtfDecodeCountListInfo);
                IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                                 & mtdBinary,
                                                 1,
                                                 sBinaryPrecision,
                                                 0 )
                          != IDE_SUCCESS );
            }
            else
            {
                aTemplate->rows[aNode->table].execute[aNode->column] = mtfDecodeCountListExecute;

                // Count ����� ������ �÷�����
                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            ID_SIZEOF(mtcColumn),
                                            (void**)&sMtcColumn )
                          != IDE_SUCCESS );
        
                // Count ����� ������
                IDE_TEST( mtc::initializeColumn( sMtcColumn,
                                                 & mtdBigint,
                                                 0,
                                                 0,
                                                 0 )
                          != IDE_SUCCESS );
        
                // execution�� sListCount���� stack�� bigint value�� ������ ������ �����Ѵ�.
                IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                                 & mtdList,
                                                 2,
                                                 sListCount,
                                                 sMtcColumn->column.size * sListCount )
                          != IDE_SUCCESS );

                // estimate�� sListCount���� stack�� �����Ѵ�.
                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            ID_SIZEOF(mtcStack) * sListCount,
                                            (void**)&(aStack[0].value) )
                          != IDE_SUCCESS);

                // list stack�� smiColumn.value�� ����صд�.
                aStack[0].column->column.value = aStack[0].value;

                sListStack = (mtcStack*)aStack[0].value;
                for ( sCount = 0; sCount < sListCount; sCount++ )
                {
                    sListStack[sCount].column = sMtcColumn;
                    sListStack[sCount].value  = sMtcColumn->module->staticNull;
                }
        
                // Count info ������ mtdBinary�� ����
                sBinaryPrecision = ID_SIZEOF(mtfDecodeCountListInfo);
                IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                                 & mtdBinary,
                                                 1,
                                                 sBinaryPrecision,
                                                 0 )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            if ( aStack[3].column->module != &mtdList )
            {
                sNode = mtf::convertedNode( aNode->arguments->next->next,
                                            aTemplate );
                
                if ( ( sNode == aNode->arguments->next->next )
                     &&
                     ( ( aTemplate->rows[sNode->table].lflag
                         & MTC_TUPLE_TYPE_MASK )
                       == MTC_TUPLE_TYPE_CONSTANT ) )
                {
                    sIsConstValue = ID_TRUE;
                }
                else
                {
                    sIsConstValue = ID_FALSE;
                }
            }
            else
            {
                sIsConstValue = ID_TRUE;
                    
                for ( sNode = aNode->arguments->next->next->arguments;
                      sNode != NULL;
                      sNode = sNode->next )
                {
                    if ( ( mtf::convertedNode( sNode, aTemplate ) == sNode )
                         &&
                         ( ( aTemplate->rows[sNode->table].lflag
                             & MTC_TUPLE_TYPE_MASK )
                           == MTC_TUPLE_TYPE_CONSTANT ) )
                    {
                        // Nothing to do.
                    }
                    else
                    {
                        sIsConstValue = ID_FALSE;
                        break;
                    }
                }
            }

            if ( sIsConstValue == ID_TRUE )
            {
                // mtfDecodeCountListCalculateInfo ������ ������ �Ҵ�
                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            ID_SIZEOF(mtfDecodeCountListCalculateInfo),
                                            (void**) & sCalculateInfo )
                          != IDE_SUCCESS );
                
                aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo = sCalculateInfo;
            
                if ( aStack[3].column->module != &mtdList )
                {
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF(mtfDecodeCountSortedValue),
                                                (void**) & sCalculateInfo->sSearchValue )
                              != IDE_SUCCESS );
                    
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF(mtcColumn),
                                                (void**) & sMtcColumn )
                              != IDE_SUCCESS );
                    
                    // decode_Count_list( i1, i2, 1 )
                    sNode = aNode->arguments->next->next;

                    sCalculateInfo->sSearchCount = 1;
                    
                    // ��� tuple�� ���Ҵ�ǹǷ� �����ؼ� �����Ѵ�.
                    mtc::copyColumn( sMtcColumn,
                                     &(aTemplate->rows[sNode->table].columns[sNode->column]) );
                    
                    sCalculateInfo->sSearchValue[0].column = sMtcColumn;
                    sCalculateInfo->sSearchValue[0].value = (void*)
                        mtc::value( sCalculateInfo->sSearchValue[0].column,
                                    aTemplate->rows[sNode->table].row,
                                    MTD_OFFSET_USE );
                    sCalculateInfo->sSearchValue[0].idx = 0;
                }
                else
                {
                    // decode_Count_list( i1, i2, (1,2,3,...) )
                    sCalculateInfo->sSearchCount = aStack[3].column->precision;
                
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF(mtfDecodeCountSortedValue) * sCalculateInfo->sSearchCount,
                                                (void**) & sCalculateInfo->sSearchValue )
                              != IDE_SUCCESS );
                    
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF(mtcColumn) * sCalculateInfo->sSearchCount,
                                                (void**) & sMtcColumn )
                              != IDE_SUCCESS );
                    
                    sListStack = (mtcStack*)aStack[3].value;
                    sListCount = aStack[3].column->precision;
                    
                    for ( sCount = 0, sNode = aNode->arguments->next->next->arguments;
                          ( sCount < sCalculateInfo->sSearchCount ) && ( sNode != NULL );
                          sCount++, sNode = sNode->next, sMtcColumn++ )
                    {
                        // ��� ���� type�̾�� �Ѵ�.
                        IDE_DASSERT( sListStack[0].column->module->no ==
                                     sListStack[sCount].column->module->no );
                        
                        // ��� tuple�� ���Ҵ�ǹǷ� �����ؼ� �����Ѵ�.
                        mtc::copyColumn( sMtcColumn,
                                         &(aTemplate->rows[sNode->table].columns[sNode->column]) );
                        
                        sCalculateInfo->sSearchValue[sCount].column = sMtcColumn;
                        sCalculateInfo->sSearchValue[sCount].value = (void*)
                            mtc::value( sCalculateInfo->sSearchValue[sCount].column,
                                        aTemplate->rows[sNode->table].row,
                                        MTD_OFFSET_USE );
                        sCalculateInfo->sSearchValue[sCount].idx = sCount;
                    }
                    
                    // sort
                    IDE_DASSERT( sCalculateInfo->sSearchCount > 1 );
        
                    idlOS::qsort( sCalculateInfo->sSearchValue,
                                  sCalculateInfo->sSearchCount,
                                  ID_SIZEOF(mtfDecodeCountSortedValue),
                                  compareDecodeCountSortedValue );
                
                    // �ߺ��� �־�� �ȵȴ�. (bsearch�� �Ѱ��� ã���ش�.)
                    for ( sCount = 1; sCount < sCalculateInfo->sSearchCount; sCount++ )
                    {
                        sValueInfo1.column = (const mtcColumn *)
                            sCalculateInfo->sSearchValue[sCount - 1].column;
                        sValueInfo1.value  =
                            sCalculateInfo->sSearchValue[sCount - 1].value;
                        sValueInfo1.flag   = MTD_OFFSET_USELESS;
                    
                        sValueInfo2.column = (const mtcColumn *)
                            sCalculateInfo->sSearchValue[sCount].column;
                        sValueInfo2.value  =
                            sCalculateInfo->sSearchValue[sCount].value;
                        sValueInfo2.flag   = MTD_OFFSET_USELESS;
                    
                        IDE_TEST_RAISE( sCalculateInfo->sSearchValue[sCount].column->module->
                                        logicalCompare[MTD_COMPARE_ASCENDING]
                                        ( &sValueInfo1,
                                          &sValueInfo2 )
                                        == 0,
                                        ERR_INVALID_FUNCTION_ARGUMENT );
                    }
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        sModules[0] = aStack[1].column->module;
        
        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            sModules )
                  != IDE_SUCCESS );
        
        aTemplate->rows[aNode->table].execute[aNode->column] = mtfDecodeCountListExecute;

        // Count ����� ������
        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         & mtdBigint,
                                         0,
                                         0,
                                         0 )
                  != IDE_SUCCESS );
        
        // Count info�� �ʿ����.
        IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                         & mtdBinary,
                                         1,
                                         0,
                                         0 )
                  != IDE_SUCCESS );
    }

    if ( sFence == 3 )
    {
        // decode_Count_list(i1, i2, (1,2,3))�� ���� ����° ���ڰ� ����� ���
        if ( sListCount > 1 )
        {
            sIsConstValue = ID_TRUE;
            
            for ( sCount = 0, sNode = aNode->arguments->next->next->arguments;
                  ( sCount < sListCount ) && ( sNode != NULL );
                  sCount++, sNode = sNode->next )
            {
                if ( ( MTC_NODE_IS_DEFINED_VALUE( sNode ) == ID_TRUE )
                     &&
                     ( ( ( aTemplate->rows[sNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                         == MTC_TUPLE_TYPE_CONSTANT ) ||
                       ( ( aTemplate->rows[sNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                         == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
                {
                    // Nothing to do.
                }
                else
                {
                    sIsConstValue = ID_FALSE;
                    break;
                }
            }

            if ( sIsConstValue == ID_TRUE )
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
            }
            else
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
            }
        }
        else
        {
            if ( ( MTC_NODE_IS_DEFINED_VALUE( aNode->arguments->next->next ) == ID_TRUE )
                 &&
                 ( ( ( aTemplate->rows[aNode->arguments->next->next->table].lflag
                       & MTC_TUPLE_TYPE_MASK )
                     == MTC_TUPLE_TYPE_CONSTANT ) ||
                   ( ( aTemplate->rows[aNode->arguments->next->next->table].lflag
                       & MTC_TUPLE_TYPE_MASK )
                     == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
            }
            else
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
            }
        }
            
        // BUG-38070 undef type���� re-estimate���� �ʴ´�.
        if ( ( aTemplate->variableRow != ID_USHORT_MAX ) &&
             ( ( aNode->lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) )
        {
            if ( aTemplate->rows[aTemplate->variableRow].
                 columns->module->id == MTD_UNDEF_ID )
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
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
        aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
        aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION( ERR_LIST_COUNT );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                              "mtfDecodeCountListEstimate",
                              "invalid list count" ) );
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC mtfDecodeCountListInitialize( mtcNode*     aNode,
                                     mtcStack*,
                                     SInt,
                                     void*,
                                     mtcTemplate* aTemplate )
{
#define IDE_FN "IDE_RC mtfDecodeCountListInitialize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtcColumn         * sColumn;
    mtcNode                 * sArgNode[2];
    mtdBinaryType           * sValue;
    mtfDecodeCountListInfo  * sInfo;
    mtcStack                * sTempStack;
    mtdBigintType           * sTempValue;
    SInt                      sCount;
    UInt                      sFence;

    sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;
    
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sValue = (mtdBinaryType*)
        ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

    // BUG-43709
    sValue->mLength = sColumn[1].precision;
    sInfo = (mtfDecodeCountListInfo*)(sValue->mValue);

    /* BUG-44469 [qx] codesonar warning in QX, MT, ST */
    IDE_TEST_RAISE( sInfo == NULL, ERR_LIST_INFO );

    //-----------------------------
    // Count info �ʱ�ȭ
    //-----------------------------

    sArgNode[0] = aNode->arguments;

    // Count column ����
    sInfo->sCountColumnExecute = aTemplate->rows[sArgNode[0]->table].execute + sArgNode[0]->column;
    sInfo->sCountColumnNode    = sArgNode[0];

    if ( sFence == 3 )
    {
        sArgNode[1] = sArgNode[0]->next;

        // expression column ����
        sInfo->sExprExecute = aTemplate->rows[sArgNode[1]->table].execute + sArgNode[1]->column;
        sInfo->sExprNode    = sArgNode[1];
    }
    else
    {
        // Nothing to do.
    }
    
    // return column ����
    sInfo->sReturnColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sInfo->sReturnValue  = (void *)
        ((UChar*) aTemplate->rows[aNode->table].row + sInfo->sReturnColumn->column.offset);
    
    if ( sInfo->sReturnColumn->module != &mtdList )
    {
        sInfo->sReturnStack = NULL;
        sInfo->sReturnCount = 1;
    }
    else
    {
        sInfo->sReturnStack = (mtcStack*)sInfo->sReturnValue;
        sInfo->sReturnCount = sInfo->sReturnColumn->precision;
        
        // stack �ʱ�ȭ
        // (1) estimate�� ������ column ������ �ʱ�ȭ
        // (2) value�� ���� ������ ����
        sTempStack = (mtcStack*) sInfo->sReturnColumn->column.value;
        sTempValue = (mtdBigintType*)
            ( (UChar*)sInfo->sReturnStack + ID_SIZEOF(mtcStack) * sInfo->sReturnCount );
        
        for ( sCount = 0;
              sCount < sInfo->sReturnCount;
              sCount++, sTempStack++, sTempValue++ )
        {
            sInfo->sReturnStack[sCount].column = sTempStack->column;
            sInfo->sReturnStack[sCount].value  = sTempValue;
        }
        
        IDE_DASSERT( (UChar*) sTempValue ==
                     (UChar*) sInfo->sReturnValue + sInfo->sReturnColumn->column.size );
    }
    
    //-----------------------------
    // Count ����� �ʱ�ȭ
    //-----------------------------
    
    if ( sInfo->sReturnStack == NULL )
    {
        IDE_DASSERT( sInfo->sReturnColumn->module == &mtdBigint );
        
        *(mtdBigintType*)(sInfo->sReturnValue) = 0;
    }
    else
    {
        for ( sCount = 0; sCount < sInfo->sReturnCount; sCount++ )
        {
            IDE_DASSERT( sInfo->sReturnStack[sCount].column->module == &mtdBigint );
            
            *(mtdBigintType*)(sInfo->sReturnStack[sCount].value) = 0;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LIST_INFO );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                              "mtfDecodeCountListInitialize",
                              "invalid list info" ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE; 

#undef IDE_FN
}

IDE_RC mtfDecodeCountListAggregate( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate )
{
#define IDE_FN "IDE_RC mtfDecodeCountListAggregate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtdModule            * sModule;
    const mtcColumn            * sColumn;
    mtdBinaryType              * sValue;
    mtfDecodeCountListInfo     * sInfo;
    mtfDecodeCountSortedValue    sExprValue;
    mtfDecodeCountSortedValue  * sFound;
    UInt                         sFence;

    mtfDecodeCountListCalculateInfo * sCalculateInfo = (mtfDecodeCountListCalculateInfo*) aInfo;

    sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;
    
    sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sValue = (mtdBinaryType*)((UChar*)aTemplate->rows[aNode->table].row
                              + sColumn[1].column.offset);
    sInfo = (mtfDecodeCountListInfo*)(sValue->mValue);

    /* BUG-39635 Codesonar warning */
    sExprValue.column     = NULL;
    sExprValue.value      = NULL;
    sExprValue.idx        = 0;

    if ( sFence == 3 )
    {
        IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

        // ����° ���ڴ� �ݵ�� ������� �Ѵ�.
        IDE_TEST_RAISE( sCalculateInfo == NULL, ERR_INVALID_FUNCTION_ARGUMENT );
        
        // �ι�° ����
        IDE_TEST( sInfo->sExprExecute->calculate( sInfo->sExprNode,
                                                  aStack,
                                                  aRemain,
                                                  sInfo->sExprExecute->calculateInfo,
                                                  aTemplate )
                  != IDE_SUCCESS );

        if( sInfo->sExprNode->conversion != NULL )
        {
            IDE_TEST( mtf::convertCalculate( sInfo->sExprNode,
                                             aStack,
                                             aRemain,
                                             NULL,
                                             aTemplate )
                      != IDE_SUCCESS );
        }

        // decode �������
        sExprValue.column = aStack[0].column;
        sExprValue.value  = aStack[0].value;
        sExprValue.idx    = 0;
        
        sFound = (mtfDecodeCountSortedValue*)
            idlOS::bsearch( & sExprValue,
                            sCalculateInfo->sSearchValue,
                            sCalculateInfo->sSearchCount,
                            ID_SIZEOF(mtfDecodeCountSortedValue),
                            compareDecodeCountSortedValue );
    }
    else
    {
        // null�� �ƴϸ� ��
        sFound = & sExprValue;
    }

    if ( sFound != NULL )
    {
        // ù��° ����
        IDE_TEST( sInfo->sCountColumnExecute->calculate( sInfo->sCountColumnNode,
                                                         aStack,
                                                         aRemain,
                                                         sInfo->sCountColumnExecute->calculateInfo,
                                                         aTemplate )
                  != IDE_SUCCESS );

        if( sInfo->sCountColumnNode->conversion != NULL )
        {
            IDE_TEST( mtf::convertCalculate( sInfo->sCountColumnNode,
                                             aStack,
                                             aRemain,
                                             NULL,
                                             aTemplate )
                      != IDE_SUCCESS );
        }

        sModule = aStack[0].column->module;

        if ( sModule->isNull( aStack[0].column,
                              aStack[0].value ) != ID_TRUE )
        {
            if ( sInfo->sReturnStack == NULL )
            {
                *(mtdBigintType*)(sInfo->sReturnValue) += 1;
            }
            else
            {
                IDE_DASSERT( sFound->idx < sInfo->sReturnCount );
                
                *(mtdBigintType*)(sInfo->sReturnStack[sFound->idx].value) += 1;
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC mtfDecodeCountListFinalize( mtcNode*,
                                   mtcStack*,
                                   SInt,
                                   void*,
                                   mtcTemplate* )
{
#define IDE_FN "IDE_RC mtfDecodeCountListFinalize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC mtfDecodeCountListCalculate( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt,
                                    void*,
                                    mtcTemplate* aTemplate )
{
#define IDE_FN "IDE_RC mtfDecodeCountListCalculate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    
    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack->value  = (void*)( (UChar*) aTemplate->rows[aNode->table].row
                              + aStack->column->column.offset );
    
    return IDE_SUCCESS;
    
#undef IDE_FN
}
