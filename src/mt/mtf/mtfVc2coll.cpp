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
 * $Id: mtfVc2coll.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>

#include <mtdTypes.h>

extern mtdModule mtdList;
extern mtdModule mtdChar;
extern mtdModule mtdVarchar;
extern mtdModule mtdNchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdNull;
extern mtdModule mtdNumeric;
extern mtdModule mtdFloat;
extern mtdModule mtdBit;
extern mtdModule mtdVarbit;
extern mtdModule mtdByte;
extern mtdModule mtdVarbyte;
extern mtdModule mtdEchar;
extern mtdModule mtdEvarchar;

static mtcName mtfVc2collFunctionName[1] = {
    { NULL, 7, (void*)"VC2COLL" }
};

static IDE_RC mtfVc2collEstimate( mtcNode*     aNode,
                                  mtcTemplate* aTemplate,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  mtcCallBack* aCallBack );

mtfModule mtfVc2coll = {
     1|MTC_NODE_OPERATOR_FUNCTION|MTC_NODE_VARIABLE_TRUE,
    ~0,
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfVc2collFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfVc2collEstimate
};

IDE_RC mtfVc2collCalculate( mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate );

static const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfVc2collCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfVc2collEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt      /* aRemain */,
                           mtcCallBack* aCallBack )
{
    UInt             sCount;
    UInt             sFence;
    const mtdModule* sModules[MTC_NODE_ARGUMENT_COUNT_MAXIMUM];
    const mtdModule* sRepresentModule;
    mtcColumn      * sBiggestColumn;

    // Argument�� ������ �ȵȴ�.
    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_DISTINCT_MASK ) == MTC_NODE_DISTINCT_TRUE,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    sFence = aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // Argument�� 2�� �̻� �� ��
    if ( sFence > 1 )
    {
        /* Arugment���� ��ǥ Ÿ�� ����� ���Ѵ�.
         *
         * VC2COLL( 'A', 0.33, 'ABC', 5, 'K   '... )
         *           |     |     |
         *            -----      |
         *              |        |
         *          ��ǥŸ��    |
         *              |        |
         *               --------
         *                   |
         *               ��ǥŸ��
         *                     ...
         */
        for ( sCount = 1, sRepresentModule = aStack[1].column->module;
              sCount <= sFence;
              sCount++ )
        {
            // Argument�� LIST / ROWTYPE / RECORDTYPE / ARRAY / LOB / UNDEF / GEOMETRY �� �� �� ����.
            IDE_TEST_RAISE( ( aStack[sCount].column->module->id == MTD_LIST_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_ROWTYPE_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_RECORDTYPE_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_ASSOCIATIVE_ARRAY_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_BLOB_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_CLOB_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_BLOB_LOCATOR_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_CLOB_LOCATOR_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_UNDEF_ID ) ||
                            ( aStack[sCount].column->module->id == MTD_GEOMETRY_ID ),
                            ERR_CONVERSION_NOT_APPLICABLE );

            if ( sRepresentModule->id == aStack[sCount].column->module->id )
            {
                // ���� Module�� ���
                // Nothing to do.
            }
            else
            {
                // �ٸ� ����̸� ��ǥ Ÿ�� ����� ���Ѵ�.
                IDE_TEST( mtf::getComparisonModule( &sRepresentModule,
                                                    sRepresentModule->no,
                                                    aStack[sCount].column->module->no )
                          != IDE_SUCCESS );

                // ��ǥ Ÿ�� ����� ����.
                IDE_TEST_RAISE( sRepresentModule == NULL, ERR_CONVERSION_NOT_APPLICABLE );
            }
        }

        // ��ǥŸ�� ����� LIST / ROWTYPE / RECORDTYPE / ARRAY / LOB / UNDEF / GEOMETRY �̸� ����
        IDE_TEST_RAISE( ( sRepresentModule->id == MTD_LIST_ID ) ||
                        ( sRepresentModule->id == MTD_ROWTYPE_ID ) ||
                        ( sRepresentModule->id == MTD_RECORDTYPE_ID ) ||
                        ( sRepresentModule->id == MTD_ASSOCIATIVE_ARRAY_ID ) ||
                        ( sRepresentModule->id == MTD_BLOB_ID ) ||
                        ( sRepresentModule->id == MTD_CLOB_ID ) ||
                        ( sRepresentModule->id == MTD_BLOB_LOCATOR_ID ) ||
                        ( sRepresentModule->id == MTD_CLOB_LOCATOR_ID ) ||
                        ( sRepresentModule->id == MTD_UNDEF_ID ) ||
                        ( sRepresentModule->id == MTD_GEOMETRY_ID ),
                        ERR_CONVERSION_NOT_APPLICABLE );

        // Canonize�� ���ϱ� ���ؼ�
        // ������ ��ǥ Ÿ�Կ� �����Ǵ� Variable type�� �ִٸ� �ش� Type���� ��ü�Ѵ�.
        switch( sRepresentModule->id )
        {
            /*
             *  [ BEFORE ]       =>    [ AFTER ]
             *
             *    BIT            =>     VARBIT
             *    BYTE           =>     VARBYTE
             *    CHAR           =>     VARCHAR
             *    NCHAR          =>     VARCHAR
             *    ECHAR          =>     VARCHAR
             *    EVARCHAR       =>     VARCHAR
             *
             *    NVARCHAR       =>     VARCHAR
             *
             *    NUMERIC        =>     FLOAT
             *    NUMBER         =>     FLOAT
             *
             */
            case MTD_CHAR_ID :
            case MTD_NCHAR_ID :
            case MTD_ECHAR_ID :
            case MTD_NVARCHAR_ID :
            case MTD_EVARCHAR_ID :
                sRepresentModule = &mtdVarchar;
                break;
            case MTD_BIT_ID :
                sRepresentModule = &mtdVarbit;
                break;
            case MTD_BYTE_ID :
                sRepresentModule = &mtdVarbyte;
                break;
            case MTD_NUMBER_ID :
            case MTD_NUMERIC_ID :
                // To eleminate scale
                sRepresentModule = &mtdFloat;
                break;
            default :
                break;
        }

        // ������ ��ǥ Ÿ�� ���� Conversion Node�� �����ϱ� ���� module array�� ���� �Ѵ�.
        for ( sCount = 0;
              sCount < sFence;
              sCount++ )
        {
            sModules[sCount] = sRepresentModule;
        }

        // ������ ��带 �����Ѵ�
        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            sModules )
                  != IDE_SUCCESS );

        // Column size�� ���� ū Column�� ���Ѵ�.
        for ( sCount = 2, sBiggestColumn = aStack[1].column;
              sCount <= sFence;
              sCount++ )
        {
            if ( sBiggestColumn->column.size > aStack[sCount].column->column.size )
            {
                /* Nothing to do. */
            }
            else if ( sBiggestColumn->column.size < aStack[sCount].column->column.size )
            {
                sBiggestColumn = aStack[sCount].column;
            }
            else
            {
                // Column size �� ���Ƶ� Precision�� �ٸ� �� �ִ�.
                sBiggestColumn = ( sBiggestColumn->precision >= aStack[sCount].column->precision )?
                    sBiggestColumn : aStack[sCount].column;
            }
        }

        for ( sCount = 1;
              sCount <= sFence;
              sCount++ )
        {
            if ( aStack[sCount].column != sBiggestColumn )
            {
                // VC2COLL()�� Argument���� stack��
                // ������ ���� Size�� ���� ū Column���� �ʱ�ȭ �Ѵ�.
                mtc::initializeColumn( aStack[sCount].column,
                                       sBiggestColumn );
            }
            else
            {
                // Nothing to do.
            }
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         & mtdList,
                                         1,
                                         sFence,
                                         0 )
                  != IDE_SUCCESS );

        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    aStack[0].column->column.size,
                                    (void**)&(aStack[0].value ) )
                  != IDE_SUCCESS );

        // List stack�� smiColumn.value�� ����صд�.
        aStack[0].column->column.value = aStack[0].value;

        idlOS::memcpy( aStack[0].value,
                       aStack + 1,
                       aStack[0].column->column.size );

        /* ���������� ������ ���� ���°� �ȴ�.
         *
         * Function :  VC2COLL( A,  B,  C )
         *                 |    |   |   |
         * Stack    :     [0]  [1]  [2] [3]
         *                 |    |   |   |
         *                 |  copy copy copy
         *                 |    |   |   |
         *                 |    v   v   v
         * Return   :    LIST ( A,  B,  C )
         *                 |    |   |   |
         *                 |    |   |   |
         *                 |    |   |   |
         * Stack    :    value-[0]  [1] [2]
         */
    }
    else
    {
        // Argument�� LIST / ROWTYPE / RECORDTYPE / ARRAY / LOB / UNDEF / GEOMETRY �� �� �� ����.
        IDE_TEST_RAISE( ( aStack[1].column->module->id == MTD_LIST_ID ) ||
                        ( aStack[1].column->module->id == MTD_ROWTYPE_ID ) ||
                        ( aStack[1].column->module->id == MTD_RECORDTYPE_ID ) ||
                        ( aStack[1].column->module->id == MTD_ASSOCIATIVE_ARRAY_ID ) ||
                        ( aStack[1].column->module->id == MTD_BLOB_ID ) ||
                        ( aStack[1].column->module->id == MTD_CLOB_ID ) ||
                        ( aStack[1].column->module->id == MTD_BLOB_LOCATOR_ID ) ||
                        ( aStack[1].column->module->id == MTD_CLOB_LOCATOR_ID ) ||
                        ( aStack[1].column->module->id == MTD_UNDEF_ID ) ||
                        ( aStack[1].column->module->id == MTD_GEOMETRY_ID ),
                        ERR_CONVERSION_NOT_APPLICABLE );

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         & mtdList,
                                         1,
                                         1,
                                         0 )
                  != IDE_SUCCESS );

        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    aStack[0].column->column.size,
                                    (void**)&(aStack[0].value ) )
                  != IDE_SUCCESS );

        aStack[0].column->column.value = aStack[0].value;
        
        idlOS::memcpy( aStack[0].value,
                       aStack + 1,
                       aStack[0].column->column.size );
    }

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

IDE_RC mtfVc2collCalculate( mtcNode*     aNode,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            void*        aInfo,
                            mtcTemplate* aTemplate )
{
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    IDE_DASSERT( aStack[0].column->module == &mtdList );
    
    idlOS::memcpy( aStack[0].value,
                   aStack + 1,
                   aStack[0].column->column.size );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
