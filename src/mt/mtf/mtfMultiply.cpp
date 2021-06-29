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
 * $Id: mtfMultiply.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtuProperty.h>
#include <mtdTypes.h>

extern mtfModule mtfMultiply;

extern mtxModule mtxMultiply; /* PROJ-2632 */

static mtcName mtfMultiplyFunctionName[1] = {
    { NULL, 1, (void*)"*" }
};

static IDE_RC mtfMultiplyInitialize( void );

static IDE_RC mtfMultiplyFinalize( void );

static IDE_RC mtfMultiplyEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

mtfModule mtfMultiply = {
    1|MTC_NODE_OPERATOR_FUNCTION|
        MTC_NODE_PRINT_FMT_MISC|
        MTC_NODE_COMMUTATIVE_TRUE,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfMultiplyFunctionName,
    NULL,
    mtfMultiplyInitialize,
    mtfMultiplyFinalize,
    mtfMultiplyEstimate
};

// fix BUG-13757
/*
static IDE_RC mtfMultiplyEstimateInteger( mtcNode*     aNode,
                                          mtcTemplate* aTemplate,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          mtcCallBack* aCallBack );
*/

// To Remove Warning
/*
static IDE_RC mtfMultiplyEstimateSmallint( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );
*/

// fix BUG-13757
/*
static IDE_RC mtfMultiplyEstimateBigint( mtcNode*     aNode,
                                         mtcTemplate* aTemplate,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         mtcCallBack* aCallBack );
*/

static IDE_RC mtfMultiplyEstimateFloat( mtcNode*     aNode,
                                        mtcTemplate* aTemplate,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        mtcCallBack* aCallBack );

static IDE_RC mtfMultiplyEstimateReal( mtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       mtcCallBack* aCallBack );

static IDE_RC mtfMultiplyEstimateDouble( mtcNode*     aNode,
                                         mtcTemplate* aTemplate,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         mtcCallBack* aCallBack );

static mtfSubModule mtfXX[1] = {
    { NULL, mtf::estimateNA }
};

// To Fix PR-8722
//static mtfSubModule mtfNN[6] = {
//    { mtfNN+1, mtfMultiplyEstimateInteger  },
//    { mtfNN+2, mtfMultiplyEstimateSmallint },
//    { mtfNN+3, mtfMultiplyEstimateBigint   },
//    { mtfNN+4, mtfMultiplyEstimateFloat    },
//    { mtfNN+5, mtfMultiplyEstimateReal     },
//    { NULL,    mtfMultiplyEstimateDouble   }
//};

// To Fix PR-8722
// [Overflow(Underflow)-PRONE ������ ����]
// A4 ������ Parsing �ܰ迡�� �������� ���� Type ������
// �ش� ���ڰ� ���Ե� �� �ִ� �ּ� ���� ���� Data Type����
// �����ȴ�.  ( ex) 1 ==> SMALLINT, 100000 ==> INTEGER )
// �� ��, SMALLINT * SMALLINT �� ���� SMALLINT�� ó���� ���
// (24*60*60)�� ���� ������ SMALLINT ���� ������ ���
// �߸��� ������� ���� ��찡 ����.
// �̷��� ������ DATA Type�� ���� Second������ ������Ű�� ���� ���Ǵ� ��,
// A3������ Parsing �ܰ迡�� ��� �������� INTEGER�� �����ϱ� ������
// ������ �߻����� �ʾҴ�.
// �̷��� ������ �ذ��ϱ� ���Ͽ� SMALLINT���� �̿��� ���� ����� �����Ѵ�.
// ����, �̷��� �ذ��Ѵ� �ϴ��� A3�� ����������
// INTEGER * INTEGER ���� INTEGER�� ������ ��� �� �ִ�.

//static mtfSubModule mtfNN[5] = {
//    { mtfNN+1, mtfMultiplyEstimateInteger  },
//    { mtfNN+2, mtfMultiplyEstimateBigint   },
//    { mtfNN+3, mtfMultiplyEstimateFloat    },
//    { mtfNN+4, mtfMultiplyEstimateReal     },
//    { NULL,    mtfMultiplyEstimateDouble   }
//};

// fix BUG-13757
// integer�迭�� ���ϱ� �������� doubleŸ������ ó��
// 
// (1) ������å
//     smallint * smallint = integer
//     integer  * integer  = integer
//     bigint   * bigint   = bigint
//
//     a3, a4���� �̷��� ��å�� �����ϰ� �� ������,
//     �ε��� Ȱ�뵵�� ���̱� ������
//     ��) i1�� integer column�� ���,
//         A. A2������
//            i1 = 3 * 3�� integer = double(?)�̹Ƿ� �ε����� Ż �� ����
//         B. A3, A4������
//            i1 = 3 * 3�� integer = integer�̹Ƿ� �ε����� Ż �� ����.
//
// (2) �������å
//     smallint * smallint = double
//     integer  * integer  = double
//     bigint   * bigint   = double
//
// ��å�������� ���� �������� Ÿ�缺
// (1) ������
//     �������� double type���� conversion�ϴ� ����� �߻���.
// (2) Ÿ�缺
//     A. �ε��� Ȱ���� ������ ������
//        ���ϰ迭 ����ŸŸ���� �ε���Ȱ�뵵����(PROJ-1364)���� ����
//        integer = double�� ��쿡�� �ε����� Ż �� ����.
//     B. isql���� ǥ�� ����
//        ���ϱ⿬���� ����� XXXX.0 ���� ������ ���,
//        .0�� �����ϰ�  XXXX�� ��µ�.
//        ��) select 3*3 from t1�� ���, 9.0�� �ƴ� 9�� ��µ�.
//     C. client program���� ���ϱ� �������� integer type������ �����,
//        .���ϱ� �������� integer�������� �� ���, ������
//        .���ϱ� �������� integer������ ���� ���,client program���� ������
//         ��) (Error : [-1] The data value could not be converted to 
//                      the data type specified by TargetType in SQLBindCol)

static mtfSubModule mtfNN[3] = {
    { mtfNN+1, mtfMultiplyEstimateFloat    },
    { mtfNN+2, mtfMultiplyEstimateReal     },
    { NULL,    mtfMultiplyEstimateDouble   }
};

static mtfSubModule* mtfGroupTable[MTD_GROUP_MAXIMUM][MTD_GROUP_MAXIMUM] = {
/*               MISC   TEXT   NUMBE  DATE   INTER */
/* MISC     */ { mtfNN, mtfNN, mtfNN, mtfNN, mtfNN },
/* TEXT     */ { mtfNN, mtfNN, mtfNN, mtfNN, mtfNN },
/* NUMBER   */ { mtfNN, mtfNN, mtfNN, mtfNN, mtfNN },
/* DATE     */ { mtfNN, mtfNN, mtfNN, mtfNN, mtfNN },
/* INTERVAL */ { mtfNN, mtfNN, mtfNN, mtfNN, mtfNN }
};

// BUG-41994
// high precision�� group table
static mtfSubModule mtfNP[1] = {
    { NULL, mtfMultiplyEstimateFloat }
};

static mtfSubModule* mtfGroupTableHighPrecision[MTD_GROUP_MAXIMUM][MTD_GROUP_MAXIMUM] = {
/*               MISC   TEXT   NUMBE  DATE   INTER */
/* MISC     */ { mtfNP, mtfNP, mtfNP, mtfNP, mtfNP },
/* TEXT     */ { mtfNP, mtfNP, mtfNP, mtfNP, mtfNP },
/* NUMBER   */ { mtfNP, mtfNP, mtfNP, mtfNP, mtfNP },
/* DATE     */ { mtfNP, mtfNP, mtfNP, mtfNP, mtfNP },
/* INTERVAL */ { mtfNP, mtfNP, mtfNP, mtfNP, mtfNP }
};

/* BUG-46195 */
static mtfSubModule mtfNF[1] = {
    { NULL, mtfMultiplyEstimateFloat }
};

static mtfSubModule * mtfGroupTableMaxPrecision[MTD_GROUP_MAXIMUM][MTD_GROUP_MAXIMUM] = {
/*               MISC   TEXT   NUMBE  DATE   INTER */
/* MISC     */ { mtfNF, mtfNF, mtfNF, mtfNF, mtfNF },
/* TEXT     */ { mtfNF, mtfNF, mtfNF, mtfNF, mtfNF },
/* NUMBER   */ { mtfNF, mtfNF, mtfNF, mtfNF, mtfNF },
/* DATE     */ { mtfNF, mtfNF, mtfNF, mtfNF, mtfNF },
/* INTERVAL */ { mtfNF, mtfNF, mtfNF, mtfNF, mtfNF }
};

static mtfSubModule*** mtfTable = NULL;
static mtfSubModule*** mtfTableHighPrecision = NULL;

/* BUG-46195 */
static mtfSubModule*** mtfTableMaxPrecision = NULL;

IDE_RC mtfMultiplyInitialize( void )
{
    IDE_TEST( mtf::initializeComparisonTemplate( &mtfTable,
                                                 mtfGroupTable,
                                                 mtfXX )
              != IDE_SUCCESS );

    IDE_TEST( mtf::initializeComparisonTemplate( &mtfTableHighPrecision,
                                                 mtfGroupTableHighPrecision,
                                                 mtfXX )
              != IDE_SUCCESS );

    /* BUG-46195 */
    IDE_TEST( mtf::initializeComparisonTemplate( &mtfTableMaxPrecision,
                                                 mtfGroupTableMaxPrecision,
                                                 mtfXX )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfMultiplyFinalize( void )
{
    IDE_TEST( mtf::finalizeComparisonTemplate( &mtfTable )
              != IDE_SUCCESS );

    IDE_TEST( mtf::finalizeComparisonTemplate( &mtfTableHighPrecision )
              != IDE_SUCCESS );

    /* BUG-46195 */
    IDE_TEST( mtf::finalizeComparisonTemplate( &mtfTableMaxPrecision )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfMultiplyEstimate( mtcNode*     aNode,
                            mtcTemplate* aTemplate,
                            mtcStack*    aStack,
                            SInt         aRemain,
                            mtcCallBack* aCallBack )
{
    const mtfSubModule   * sSubModule;
    mtfSubModule       *** sTable;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    // BUG-41994
    aTemplate->arithmeticOpModeRef = ID_TRUE;
    if ( aTemplate->arithmeticOpMode == MTC_ARITHMETIC_OPERATION_PRECISION )
    {
        sTable = mtfTableHighPrecision;
    }
    /* BUG-46195 */
    else if ( aTemplate->arithmeticOpMode == MTC_ARITHMETIC_OPERATION_MAX_PRECISION )
    {
        sTable = mtfTableMaxPrecision;
    }
    else
    {
        sTable = mtfTable;
    }

    IDE_TEST( mtf::getSubModule2Args( &sSubModule,
                                      sTable,
                                      aStack[1].column->module->no,
                                      aStack[2].column->module->no )
              != IDE_SUCCESS );

    IDE_TEST( sSubModule->estimate( aNode,
                                    aTemplate,
                                    aStack,
                                    aRemain,
                                    aCallBack )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ZONE: INTEGER */

extern mtdModule mtdInteger;

IDE_RC mtfMultiplyCalculateInteger( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteInteger = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateInteger,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateInteger( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt,
                                   mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdInteger,
        &mtdInteger
    };
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );
    
    aTemplate->rows[aNode->table].execute[aNode->column] = mtfMultiplyExecuteInteger;

    /* PROJ-2632 */
    aTemplate->rows[aNode->table].execute[aNode->column].mSerialExecute
        = mtxMultiply.mGetExecute( sModules[0]->id, sModules[1]->id );

    //IDE_TEST( mtdInteger.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdInteger,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfMultiplyCalculateInteger( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate )
{
    SLong sValue1;
    SLong sValue2;
    SLong sValue3;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( ( *(mtdIntegerType*)aStack[1].value == MTD_INTEGER_NULL ) ||
        ( *(mtdIntegerType*)aStack[2].value == MTD_INTEGER_NULL ) )
    {
        *(mtdIntegerType*)aStack[0].value = MTD_INTEGER_NULL;
    }
    else
    {
        sValue1 = *(mtdIntegerType*)aStack[1].value;
        sValue2 = *(mtdIntegerType*)aStack[2].value;
        sValue3 = sValue1 * sValue2;
        IDE_TEST_RAISE( sValue3 > MTD_INTEGER_MAXIMUM , ERR_OVERFLOW );
        IDE_TEST_RAISE( sValue3 < MTD_INTEGER_MINIMUM , ERR_OVERFLOW );

        *(mtdIntegerType*)aStack[0].value = sValue3;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ZONE: SMALLINT */

// To Remove Warning
/*
extern mtdModule mtdSmallint;

IDE_RC mtfMultiplyCalculateSmallint( 
                                    mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteSmallint = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateSmallint,
    NULL,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateSmallint( mtcNode*     aNode,
                                    mtcTemplate* aTemplate,
                                    mtcStack*    aStack,
                                    SInt,
                                    mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdSmallint,
        &mtdSmallint
    };
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTuple,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTuple[aNode->table].execute[aNode->column] = mtfMultiplyExecuteSmallint;
    
    IDE_TEST( mtdSmallint.estimate( aStack[0].column, 0, 0, 0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfMultiplyCalculateSmallint( 
                             mtcNode*     aNode,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             void*        aInfo,
                             mtcTemplate* aTemplate )
{
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aTuple,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( mtdSmallint.isNull( aStack[1].column,
                            aStack[1].value ) ||
        mtdSmallint.isNull( aStack[2].column,
                            aStack[2].value )  )
    {
        mtdSmallint.null( aStack[0].column,
                          aStack[0].value );
    }
    else
    {
        *(mtdSmallintType*)aStack[0].value =
       *(mtdSmallintType*)aStack[1].value * *(mtdSmallintType*)aStack[2].value;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
*/

/* ZONE: BIGINT */

extern mtdModule mtdBigint;

IDE_RC mtfMultiplyCalculateBigint( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteBigint = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateBigint,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateBigint( mtcNode*     aNode,
                                  mtcTemplate* aTemplate,
                                  mtcStack*    aStack,
                                  SInt,
                                  mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdBigint,
        &mtdBigint
    };
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfMultiplyExecuteBigint;

    /* PROJ-2632 */
    aTemplate->rows[aNode->table].execute[aNode->column].mSerialExecute
        = mtxMultiply.mGetExecute( sModules[0]->id, sModules[1]->id );

    //IDE_TEST( mtdBigint.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdBigint,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfMultiplyCalculateBigint( mtcNode*     aNode,
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

    if( ( *(mtdBigintType*)aStack[1].value == MTD_BIGINT_NULL ) ||
        ( *(mtdBigintType*)aStack[2].value == MTD_BIGINT_NULL ) )
    {
        *(mtdBigintType*)aStack[0].value = MTD_BIGINT_NULL;
    }
    else
    {
        *(mtdBigintType*)aStack[0].value =
           *(mtdBigintType*)aStack[1].value * *(mtdBigintType*)aStack[2].value;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ZONE: FLOAT */

extern mtdModule mtdFloat;
    
IDE_RC mtfMultiplyCalculateFloat( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteFloat = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateFloat,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateFloat( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt,
                                 mtcCallBack* aCallBack )
{
    const mtdModule* sModules[2];

    mtc::makeFloatConversionModule( aStack + 1, &sModules[0] );
    mtc::makeFloatConversionModule( aStack + 2, &sModules[1] );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfMultiplyExecuteFloat;

    /* PROJ-2632 */
    aTemplate->rows[aNode->table].execute[aNode->column].mSerialExecute
        = mtxMultiply.mGetExecute( sModules[0]->id, sModules[1]->id );

    //IDE_TEST( mtdFloat.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdFloat,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    IDE_RC mtfMultiplyCalculateFloat()
 *
 * Argument :
 *    aStack - point of stack
 *
 * Description :
 *    Multiply Float : aStack[0] = aStack[1] * aStack[2] 
 *    �������� ����� idaMultiply �̷������.
 * ---------------------------------------------------------------------------*/
 
IDE_RC mtfMultiplyCalculateFloat( mtcNode*     aNode,
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
        IDE_TEST( mtc::multiplyFloat( (mtdNumericType*)aStack[0].value,
                                      MTD_FLOAT_PRECISION_MAXIMUM,
                                      (mtdNumericType*)aStack[1].value,
                                      (mtdNumericType*)aStack[2].value )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ZONE: REAL */

extern mtdModule mtdReal;
    
IDE_RC mtfMultiplyCalculateReal( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteReal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateReal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateReal( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt,
                                mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdReal,
        &mtdReal
    };
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfMultiplyExecuteReal;

    /* PROJ-2632 */
    aTemplate->rows[aNode->table].execute[aNode->column].mSerialExecute
        = mtxMultiply.mGetExecute( sModules[0]->id, sModules[1]->id );

    //IDE_TEST( mtdReal.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdReal,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfMultiplyCalculateReal(  mtcNode*     aNode,
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
    
    if( (mtdReal.isNull( aStack[1].column,
                         aStack[1].value ) == ID_TRUE) ||
        (mtdReal.isNull( aStack[2].column,
                         aStack[2].value ) == ID_TRUE) )
    {
        mtdReal.null( aStack[0].column,
                      aStack[0].value );
    }
    else
    {
        *(mtdRealType*)aStack[0].value =
               *(mtdRealType*)aStack[1].value * *(mtdRealType*)aStack[2].value;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ZONE: DOUBLE */

extern mtdModule mtdDouble;
    
IDE_RC mtfMultiplyCalculateDouble( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

static const mtcExecute mtfMultiplyExecuteDouble = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfMultiplyCalculateDouble,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfMultiplyEstimateDouble( mtcNode*     aNode,
                                  mtcTemplate* aTemplate,
                                  mtcStack*    aStack,
                                  SInt,
                                  mtcCallBack* aCallBack )
{
    static const mtdModule* sModules[2] = {
        &mtdDouble,
        &mtdDouble
    };
    
    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = mtfMultiplyExecuteDouble;

    /* PROJ-2632 */
    aTemplate->rows[aNode->table].execute[aNode->column].mSerialExecute
        = mtxMultiply.mGetExecute( sModules[0]->id, sModules[1]->id );

    //IDE_TEST( mtdDouble.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdDouble,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfMultiplyCalculateDouble( mtcNode*     aNode,
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
    
    if( (mtdDouble.isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE) ||
        (mtdDouble.isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE) )
    {
        mtdDouble.null( aStack[0].column,
                        aStack[0].value );
    }
    else
    {
        *(mtdDoubleType*)aStack[0].value =
           *(mtdDoubleType*)aStack[1].value * *(mtdDoubleType*)aStack[2].value;

        // fix BUG-13757
        // double'0' * double'-1' = double'-0' �� �����Ƿ�
        // -0 ���� 0 ������ ó���ϱ� ���� �ڵ���.
        /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
        if( *(mtdDoubleType*)aStack[0].value == 0 )
        {
            *(mtdDoubleType*)aStack[0].value = 0;
        }
        else
        {
            // Nothing To Do
        }
        /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
