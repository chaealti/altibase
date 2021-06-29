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
 * $Id: mtvChar2Varchar.cpp 29282 2008-11-13 08:03:38Z mhjeong $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtl.h>

#include <mtdTypes.h>

extern mtvModule mtvChar2Echar;

extern mtdModule mtdChar;
extern mtdModule mtdEchar;

static IDE_RC mtvEstimate( mtcNode     * aNode,
                           mtcTemplate * aTemplate,
                           mtcStack    * aStack,
                           SInt          aRemain,
                           mtcCallBack * aCallBack );

IDE_RC mtvCalculate_Char2Echar( mtcNode     * aNode,
                                mtcStack    * aStack,
                                SInt          aRemain,
                                void        * aInfo,
                                mtcTemplate * aTemplate );

mtvModule mtvChar2Echar = {
    &mtdEchar,
    &mtdChar,
    MTV_COST_DEFAULT,
    mtvEstimate
};

static const mtcExecute mtvExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtvCalculate_Char2Echar,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

static IDE_RC mtvEstimate( mtcNode     * aNode,
                           mtcTemplate * aTemplate,
                           mtcStack    * aStack,
                           SInt          /* aRemain */,
                           mtcCallBack * /* aCallBack */ )
{
    UInt  sECCSize;
    
    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    aTemplate->rows[aNode->table].execute[aNode->column] = mtvExecute;

    IDE_TEST( aTemplate->getECCSize( aStack[1].column->precision,
                                     & sECCSize )                                     
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdEchar,
                                     1,
                                     aStack[1].column->precision,
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeEncryptColumn(
                  aStack[0].column,
                  (SChar*) "",                 // default policy
                  aStack[1].column->precision, // encrypted size
                  sECCSize )                   // ECC size
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtvCalculate_Char2Echar( mtcNode     * /* aNode */,
                                mtcStack    * aStack,
                                SInt          /* aRemain */,
                                void        * /* aInfo */,
                                mtcTemplate * aTemplate )
{
    mtcECCInfo     sInfo;
    mtdEcharType * sEcharValue;    
    mtdCharType  * sCharValue;
    UShort         sLength;

    sEcharValue  = (mtdEcharType*)aStack[0].value;
    sCharValue   = (mtdCharType*)aStack[1].value;
    
    sEcharValue->mCipherLength = sCharValue->length;

    if( sEcharValue->mCipherLength > 0 )
    {
        idlOS::memcpy( sEcharValue->mValue,
                       sCharValue->value,
                       sEcharValue->mCipherLength );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------------------
    // PROJ-2002 Column Security
    //
    // [padding �����ϴ� ����]
    // char type�� compare�� padding�� �����ϰ� ���Ѵ�.
    // ���� echar type�� padding�� �����Ͽ� ecc�� �����ϸ� 
    // ecc�� memcmp������ echar type�� �񱳰� �����ϴ�.
    // 
    // ��, NULL�� ' ', '  '�� �񱳸� ���Ͽ� 
    // NULL�� ���ؼ��� ecc�� �������� ������, ' ', '  '��
    // space padding �ϳ�(' ')�� ecc�� �����Ѵ�.
    // 
    // ����) char'NULL' => echar( encrypt(''),   ecc('')  )
    //       char' '    => echar( encrypt(' '),  ecc(' ') )
    //       char'  '   => echar( encrypt('  '), ecc(' ') )
    //       char'a'    => echar( encrypt('a'),  ecc('a') )
    //       char'a '   => echar( encrypt('a '), ecc('a') )
    //-----------------------------------------------------
    
    // sEcharValue���� space pading�� ������ ���̸� ã�´�.
    for( sLength = sEcharValue->mCipherLength; sLength > 1; sLength-- )
    {
        if( sEcharValue->mValue[sLength - 1] != ' ' )
        {
            break;
        }
    }

    // sEcharValue���� space padding�� ������ value�� �̿��� ECC�� ����Ѵ�.
    if( sLength > 0 )
    {
        IDE_TEST( aTemplate->getECCInfo( aTemplate,
                                         & sInfo )
                  != IDE_SUCCESS );
        
        IDE_TEST( aTemplate->encodeECC(
                      & sInfo,
                      sEcharValue->mValue,
                      sLength,
                      sEcharValue->mValue + sEcharValue->mCipherLength,
                      & sEcharValue->mEccLength )
                  != IDE_SUCCESS );
    }
    else
    {
        sEcharValue->mEccLength = 0;
    }
        
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
