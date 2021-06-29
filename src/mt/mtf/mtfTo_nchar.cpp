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
 * $Id: mtfTo_nchar.cpp 26496 2008-06-11 11:02:30Z copyrei $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>
#include <mtdTypes.h>

extern mtfModule mtfTo_nchar;
extern mtfModule mtfTo_char;

extern mtdModule mtdDate;
extern mtdModule mtdNumeric;
extern mtdModule mtdFloat;
extern mtdModule mtdChar;
extern mtdModule mtdNchar;
extern mtdModule mtdVarchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdByte;
extern mtdModule mtdVarbyte;
extern mtdModule mtdNibble;
extern mtdModule mtdBit;
extern mtdModule mtdVarbit;
extern mtdModule mtdClob;
extern mtdModule mtdClobLocator;

extern mtlModule mtlUTF16;

static mtcName mtfTo_ncharFunctionName[1] = {
    { NULL, 8, (void*)"TO_NCHAR" }
};

static IDE_RC mtfTo_ncharEstimate( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

mtfModule mtfTo_nchar = {
    1|MTC_NODE_OPERATOR_FUNCTION,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    mtfTo_ncharFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfTo_ncharEstimate
};

static IDE_RC mtfTo_ncharCalculateFor1Arg( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

static IDE_RC mtfTo_ncharCalculateNcharFor1Arg( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

static IDE_RC mtfTo_ncharCalculateNumberFor2Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

static IDE_RC mtfTo_ncharCalculateDateFor2Args( mtcNode*     aNode,
                                          mtcStack*    aStack,
                                          SInt         aRemain,
                                          void*        aInfo,
                                          mtcTemplate* aTemplate );

const mtcExecute mtfExecuteFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTo_ncharCalculateFor1Arg,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNcharFor1Arg = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTo_ncharCalculateNcharFor1Arg,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteNumberFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTo_ncharCalculateNumberFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

const mtcExecute mtfExecuteDateFor2Args = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfTo_ncharCalculateDateFor2Args,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC mtfTo_ncharEstimate( mtcNode*     aNode,
                            mtcTemplate* aTemplate,
                            mtcStack*    aStack,
                            SInt      /* aRemain */,
                            mtcCallBack* aCallBack )
{
    const mtdModule* sModules[2];
    SInt             sPrecision;
    mtcNode        * sCharNode;
    mtcColumn      * sCharColumn;
    mtdCharType    * sFormat;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 1 ||
                    ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 )
    {
        IDE_TEST( mtf::getCharFuncResultModule( &sModules[0],
                                                aStack[1].column->module )
                  != IDE_SUCCESS );

        switch( aStack[1].column->module->flag & MTD_GROUP_MASK )
        {
            case MTD_GROUP_TEXT:
                sPrecision = aStack[1].column->precision;
                break;
            case MTD_GROUP_DATE:
            case MTD_GROUP_NUMBER:
            case MTD_GROUP_INTERVAL:
                sPrecision = MTC_TO_CHAR_MAX_PRECISION;
                break;
            case MTD_GROUP_MISC:
                // BUG-16741
                if ( ( aStack[1].column->module == &mtdByte ) ||
                     ( aStack[1].column->module == &mtdVarbyte ) )
                {
                    sPrecision = aStack[1].column->precision * 2;
                }
                else if ( (aStack[1].column->module == &mtdNibble) ||
                          (aStack[1].column->module == &mtdBit)    ||
                          (aStack[1].column->module == &mtdVarbit) )
                {
                    sPrecision = aStack[1].column->precision;
                }
                /* BUG-42666 To_char function is not considered cloblocator */
                else if ( ( aStack[1].column->module == &mtdClob ) ||
                          ( aStack[1].column->module == &mtdClobLocator ) )
                {
                    /* BUG-36219 TO_CHAR, TO_NCHAR���� LOB ���� */
                    if ( mtl::mNationalCharSet->id == MTL_UTF8_ID )
                    {
                        if ( aStack[1].column->precision != 0 )
                        {
                            sPrecision = IDL_MIN( aStack[1].column->precision,
                                                  (SInt)MTD_UTF8_NCHAR_PRECISION_MAXIMUM );
                        }
                        else
                        {
                            sPrecision = (SInt)MTD_UTF8_NCHAR_PRECISION_MAXIMUM;
                        }
                    }
                    else if( mtl::mNationalCharSet->id == MTL_UTF16_ID )
                    {
                        if ( aStack[1].column->precision != 0 )
                        {
                            sPrecision = IDL_MIN( aStack[1].column->precision,
                                                  (SInt)MTD_UTF16_NCHAR_PRECISION_MAXIMUM );
                        }
                        else
                        {
                            sPrecision = (SInt)MTD_UTF16_NCHAR_PRECISION_MAXIMUM;
                        }
                    }
                    else
                    {
                        ideLog::log( IDE_ERR_0,
                                     "mtl::mNationalCharSet->id : %u\n",
                                     mtl::mNationalCharSet->id );

                        IDE_ASSERT( 0 );
                    }
                }
                else
                {
                    // mtdNull�� ��쵵 ����
                    sPrecision = 1;
                }
                break;
            default:
                ideLog::log( IDE_ERR_0, 
                             "( aStack[1].column->module->flag & MTD_GROUP_MASK ) : %x\n",
                             aStack[1].column->module->flag & MTD_GROUP_MASK );

                IDE_ASSERT( 0 );
        }

        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            sModules )
                  != IDE_SUCCESS );

        if( (aStack[1].column->module->id == MTD_NCHAR_ID) ||
            (aStack[1].column->module->id == MTD_NVARCHAR_ID) )
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteNcharFor1Arg;
        }
        else
        {
            aTemplate->rows[aNode->table].execute[aNode->column] =
                                                        mtfExecuteFor1Arg;
        }

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdNvarchar,
                                         1,
                                         sPrecision,
                                         0 )
              != IDE_SUCCESS );
    }
    else if ( ( aStack[1].column->module->flag & MTD_GROUP_MASK ) ==
                                                            MTD_GROUP_NUMBER )
    {
        sModules[0] = &mtdNumeric;
        sModules[1] = &mtdChar;

        // number format�� �ִ� ���̸� 64�� �����Ѵ�.
        IDE_TEST_RAISE( aStack[2].column->precision >
                        MTC_TO_CHAR_MAX_PRECISION,
                        ERR_TO_CHAR_MAX_PRECISION );

        // 'fmt'�� 'rn'�� ��� 15���� �����ؾ� ��.
        // 'fmt'�� 'xxxx'�� ��� �ִ� 8��.
        sPrecision = IDL_MAX( 15, aStack[2].column->precision + 3 );

        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            sModules )
                  != IDE_SUCCESS );

        aTemplate->rows[aNode->table].execute[aNode->column] =
                                                    mtfExecuteNumberFor2Args;

        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         &mtdNvarchar,  // BUG-16501
                                         1,
                                         sPrecision,
                                         0 )
              != IDE_SUCCESS );
    }
    else
    {
        sModules[0] = &mtdDate;

        if( aStack[2].column->language != NULL )
        {
            // PROJ-1579 NCHAR
            IDE_TEST( mtf::getCharFuncCharResultModule( &sModules[1],
                                                    aStack[2].column->module )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-1579 NCHAR
            IDE_TEST( mtf::getCharFuncCharResultModule( &sModules[1],
                                                    aStack[1].column->module )
                      != IDE_SUCCESS );
        }

        IDE_TEST_RAISE( aStack[2].column->precision >
                        MTC_TO_CHAR_MAX_PRECISION,
                        ERR_TO_CHAR_MAX_PRECISION );

        if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == 
             MTC_NODE_REESTIMATE_FALSE )
        {
            IDE_TEST( mtf::makeConversionNodes( aNode,
                                                aNode->arguments,
                                                aTemplate,
                                                aStack + 1,
                                                aCallBack,
                                                sModules )
                      != IDE_SUCCESS );

            aTemplate->rows[aNode->table].execute[aNode->column] = 
                                                    mtfExecuteDateFor2Args;

            // date fmt�� 'day'�� ��� �ִ� 9�ڸ�(wednesday)���� �� �� �ִ�.
            // ���� precision�� (aStack[2].column->precision) * 3�� �����Ѵ�.
            // toChar �Լ����� sBuffer�� ���� �ʰ� �ٷ� aStack[0]�� ���� ���ؼ�
            // precision�� �ִ밪 + 1�� ��´�.  �������� NULL�� �� �� 
            // �ֱ� �����̴�.

            IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                 &mtdNvarchar,  // BUG-16501
                                 1,
                                 ((aStack[2].column->precision) * 3 + 1),
                                 0 )
                      != IDE_SUCCESS );
        }
        // REESTIMATE�� TRUE�� ������ format ������ �����Ѵ�.
        else
        {
            sCharNode = mtf::convertedNode( aNode->arguments->next,
                                            aTemplate );

            sCharColumn = &(aTemplate->rows[sCharNode->table].
                              columns[sCharNode->column]);

            if( ( sCharNode == aNode->arguments->next ) &&
                ( ( aTemplate->rows[sCharNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                  == MTC_TUPLE_TYPE_CONSTANT ) )
            {
                sFormat = (mtdCharType *)mtd::valueForModule(
                    (smiColumn*)&(sCharColumn->column),
                    aTemplate->rows[sCharNode->table].row,
                    MTD_OFFSET_USE,
                    mtdChar.staticNull );

                // format�� ���� ��� makeFormatInfo�� ȣ������ �ʴ´�.
                if ( sFormat->length != 0 )
                {
                    IDE_TEST( mtfToCharInterface::makeFormatInfo(
                                  aNode,
                                  aTemplate,
                                  sFormat->value,
                                  sFormat->length,
                                  aCallBack )
                             != IDE_SUCCESS );
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
    }

    if ( ( ( aStack[1].column->module->flag & MTD_GROUP_MASK ) == MTD_GROUP_DATE ) &&
         ( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 ) )
    {
        if( ( MTC_NODE_IS_DEFINED_VALUE( aNode->arguments->next ) == ID_TRUE )
            &&
            ( ( ( aTemplate->rows[aNode->arguments->next->table].lflag
                  & MTC_TUPLE_TYPE_MASK )
                == MTC_TUPLE_TYPE_CONSTANT ) ||
              ( ( aTemplate->rows[aNode->arguments->next->table].lflag
                  & MTC_TUPLE_TYPE_MASK )
                == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
        {
            aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
            aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
            
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
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION( ERR_TO_CHAR_MAX_PRECISION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_TO_CHAR_MAX_PRECISION,
                            MTC_TO_CHAR_MAX_PRECISION));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfTo_ncharCalculateFor1Arg( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : To_Nchar Calculate for CHAR
        NCHAR �̿��� Ÿ���� NCHAR Ÿ������ ��ȯ�ϴ� �Լ�
 *
 * Implementation :
 *    TO_NCHAR( date )
 *
 *    aStack[0] : �Էµ� ��¥ ������ ���������� ��ȯ�Ͽ� ���
 *    aStack[1] : date
 *
 *    ex) TO_NCHAR( join_date ) ==> '09-JUN-2005'
 *
 ***********************************************************************/

    mtdCharType     * sSource;
    mtdNcharType    * sResult;

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
        // CHAR => NCHAR�� ���
        sSource = (mtdCharType*)aStack[1].value;
        sResult = (mtdNcharType*)aStack[0].value;

        IDE_TEST( mtdNcharInterface::toNchar( aStack,
                                (const mtlModule *) mtl::mDBCharSet,
                                (const mtlModule *) mtl::mNationalCharSet,
                                sSource,
                                sResult )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfTo_ncharCalculateNcharFor1Arg( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : To_Nchar Calculate for NCHAR
 *
 * Implementation :
 *    TO_NCHAR( date )
 *
 *    aStack[0] : �Էµ� ��¥ ������ ���������� ��ȯ�Ͽ� ���
 *    aStack[1] : date
 *
 *    ex) TO_NCHAR( join_date ) ==> '09-JUN-2005'
 *
 ***********************************************************************/
    UInt    sStackSize;

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
        sStackSize = aStack[1].column->module->actualSize( aStack[1].column,
                                                           aStack[1].value );
        IDE_TEST_RAISE( aStack[0].column->column.size < sStackSize,
                        ERR_BUFFER_OVERFLOW );

        // NCHAR => NCHAR�̹Ƿ� �׳� �����Ѵ�.
        idlOS::memcpy( aStack[0].value, aStack[1].value, sStackSize );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_BUFFER_OVERFLOW )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                 "mtfTo_ncharCalculateFor1Arg",
                                 "buffer overflow" ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfTo_ncharCalculateDateFor2Args( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : To_Nchar Calculate for CHAR
        NCHAR �̿��� Ÿ���� NCHAR Ÿ������ ��ȯ�ϴ� �Լ�
 *
 * Implementation :
 *    TO_NCHAR( date, fmt )
 *
 *    aStack[0] : �Էµ� ��¥ ������ ���������� ��ȯ�Ͽ� ���
 *    aStack[1] : date
 *    aStack[2] : date_fmt
 *
 *    ex) TO_NCHAR( join_date, 'YYYY-MM-DD HH:MI::SS' )
 *       ==> '2005-JUN-09'
 *
 ***********************************************************************/

    mtdNcharType*   sResult;

    // fmt�� �ִ� ���̴� MTC_TO_CHAR_MAX_PRECISION����,
    // ����� ���̴� �װ��� * 3 + 1�� �� �� �ִ�.
    // ��� Ÿ���� UTF8 �Ǵ� UTF16�̹Ƿ� * 2�� �Ѵ�.
    // UTF8:  �ѱ�(2 byte => 3byte)
    // UTF16: ASCII(1 byte => 2byte)
    UChar   sToCharResult[( (MTC_TO_CHAR_MAX_PRECISION * 3) + 1 ) * 2] = {0,};

    IDE_TEST( mtfToCharInterface::mtfTo_charCalculateDateFor2Args(
                    aNode,
                    aStack,
                    aRemain,
                    aInfo,
                    aTemplate )
              != IDE_SUCCESS );

    if( mtl::mDBCharSet->id != mtl::mNationalCharSet->id )
    {
        // mtdNcharType�� mtdCharType�� ���̰� �����Ƿ� mtdNcharType���� 
        // ĳ�����ص� �ȴ�.
        sResult = (mtdNcharType*)aStack[0].value;

        idlOS::memcpy( sToCharResult,
                       sResult->value,
                       sResult->length );

        // CHAR => NCHAR
        IDE_TEST( mtdNcharInterface::toNchar( aStack,
                                (const mtlModule *) mtl::mDBCharSet,
                                (const mtlModule *) mtl::mNationalCharSet,
                                sToCharResult,
                                idlOS::strlen((SChar *)sToCharResult),
                                sResult )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do

        // CHAR�� ĳ���� �°� NCHAR�� ĳ���� ���� ������
        // ��ȯ�� �ʿ䰡 ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfTo_ncharCalculateNumberFor2Args( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : To_Nchar Calculate for CHAR
        NCHAR �̿��� Ÿ���� NCHAR Ÿ������ ��ȯ�ϴ� �Լ�
 *
 * Implementation :
 *    TO_NCHAR( number, fmt )
 *
 *    aStack[0] : �Էµ� ��¥ ������ ���������� ��ȯ�Ͽ� ���
 *    aStack[1] : number
 *    aStack[2] : number_fmt
 *
 *    ex) TO_NCHAR( 123.456, '999.9999' )
 *       ==> '123.4560'
 *
 ***********************************************************************/

    mtdNcharType*   sResult;
    UChar           sToCharResult[MTC_TO_CHAR_MAX_PRECISION + 3 + 1] = {0,};

    IDE_TEST( mtfToCharInterface::mtfTo_charCalculateNumberFor2Args(
                    aNode,
                    aStack,
                    aRemain,
                    aInfo,
                    aTemplate )
              != IDE_SUCCESS );

    // TO_NCHAR( number, fmt )���� ASCII �̿��� ���� �������� �����Ƿ�
    // NCHAR ĳ���� ���� UTF16�� ���� ��ȯ�Ѵ�.
    if( mtl::mNationalCharSet->id == MTL_UTF16_ID )
    {
        // mtdNcharType�� mtdCharType�� ���̰� �����Ƿ� mtdNcharType���� 
        // ĳ�����ص� �ȴ�.
        sResult = (mtdNcharType*)aStack[0].value;

        idlOS::memcpy( sToCharResult,
                       sResult->value,
                       sResult->length );

        // CHAR => NCHAR
        IDE_TEST( mtdNcharInterface::toNchar( aStack,
                                (const mtlModule *) mtl::mDBCharSet,
                                (const mtlModule *) mtl::mNationalCharSet,
                                sToCharResult,
                                idlOS::strlen((SChar *)sToCharResult),
                                sResult )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
 
