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
 * $Id: mtdNumber.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>

extern mtdModule mtdFloat;
extern mtdModule mtdNumeric;

static IDE_RC mtdInitialize( UInt aNo );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static UInt mtdStoreSize( const smiColumn * aColumn );

static mtcName mtdTypeName[1] = {
    { NULL, 6, (void*)"NUMBER" }
};

mtdModule mtdNumber = {
    mtdTypeName,
    NULL,
    MTD_NUMBER_ID,
    0,
    { SMI_BUILTIN_B_TREE_INDEXTYPE_ID,
      SMI_BUILTIN_B_TREE2_INDEXTYPE_ID,
      0, 0, 0, 0, 0 },
    0,
    0|  // MTD_SELECTIVITY_DISABLE(Float �Ǵ� Numeric���� ó��)
    MTD_CREATE_ENABLE|
    MTD_CREATE_PARAM_PRECISIONSCALE|
    MTD_SEARCHABLE_PRED_BASIC|
    MTD_UNSIGNED_ATTR_TRUE|
    MTD_NUM_PREC_RADIX_TRUE|
    MTD_VARIABLE_LENGTH_TYPE_TRUE|   // PROJ-1705    
    MTD_DATA_STORE_DIVISIBLE_FALSE|  // PROJ-1705
    MTD_DATA_STORE_MTDVALUE_FALSE|   // PROJ-1705
    MTD_PSM_TYPE_ENABLE, // PROJ-1904
    MTD_NUMERIC_PRECISION_MAXIMUM,
    MTD_NUMERIC_SCALE_MINIMUM,
    MTD_NUMERIC_SCALE_MAXIMUM,
    NULL, // staticNull
    mtdInitialize,
    NULL, // mtdEstimate : Į�� ���� ��, Float/Numeric���� �ʱ�ȭ�ǹǷ� ��� X
    mtdFloat.value,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    {
        mtd::compareNA,           // Logical Comparison
        mtd::compareNA
    },
    {
        // Key Comparison
        {
            mtd::compareNA,         // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            mtd::compareNA,         // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            mtd::compareNA,         // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            mtd::compareNA,         // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            /* PROJ-2433 */
            mtd::compareNA,
            mtd::compareNA
        }
        ,
        {
            /* PROJ-2433 */
            mtd::compareNA,
            mtd::compareNA
        }
    },
    NULL,
    NULL,
    mtdValidate,
    mtd::selectivityNA, // NUMBER Module�� Float/Numeric Module�� ��ȯ��.
    mtd::encodeNumericDefault,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtdFloat.valueFromOracle,
    mtd::makeColumnInfoDefault,

    // BUG-28934
    mtk::mergeAndRangeDefault,
    mtk::mergeOrRangeListDefault,

    {
        // PROJ-1705
        mtd::mtdStoredValue2MtdValueNA, 
        // PROJ-2429
        mtd::mtdStoredValue2MtdValue4CompressColumn
    },
    mtd::mtdNullValueSizeNA,
    mtd::mtdHeaderSizeNA,

    // PROJ-2399
    mtdStoreSize
};

IDE_RC mtdInitialize( UInt aNo )
{
    IDE_TEST( mtd::initializeModule( &mtdNumber, aNo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtdValidate( mtcColumn * aColumn,
                    void      * aValue,
                    UInt        aValueSize)
{
    IDE_TEST( mtdNumeric.validate( aColumn, aValue, aValueSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static UInt mtdStoreSize( const smiColumn * /*aColumn*/ )
{
/***********************************************************************
 * PROJ-2399 row tmaplate 
 * sm�� ����Ǵ� �������� ũ�⸦ ��ȯ�Ѵ�. 
 * varchar�� ���� variable Ÿ���� ������ Ÿ���� ID_UINT_MAX�� ��ȯ
 * mtheader�� sm�� ����Ȱ�찡 �ƴϸ� mtheaderũ�⸦ ���� ��ȯ 
 **********************************************************************/

    return ID_UINT_MAX;
}
