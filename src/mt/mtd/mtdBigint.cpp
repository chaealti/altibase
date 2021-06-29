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
 * $Id: mtdBigint.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>

#define MTD_BIGINT_ALIGN    (ID_SIZEOF(ULong))
#define MTD_BIGINT_SIZE     (ID_SIZEOF(mtdBigintType))
#define MTD_BIGINT_MAXIMUM1 ((mtdBigintType)ID_LONG(922337203685477580))
#define MTD_BIGINT_MAXIMUM2 ((mtdBigintType)ID_LONG(99999999999999999))
#define MTD_BIGINT_MINIMUM1 ((mtdBigintType)ID_LONG(-922337203685477580))
#define MTD_BIGINT_MINIMUM2 ((mtdBigintType)ID_LONG(-99999999999999999))

extern mtdModule mtdBigint;
extern mtdModule mtdDouble;

static mtdBigintType mtdBigintNull = MTD_BIGINT_NULL;

static IDE_RC mtdInitialize( UInt aNo );

static IDE_RC mtdEstimate( UInt * aColumnSize,
                           UInt * aArguments,
                           SInt * aPrecision,
                           SInt * aScale );

static IDE_RC mtdValue( mtcTemplate* aTemplate,
                        mtcColumn*   aColumn,
                        void*        aValue,
                        UInt*        aValueOffset,
                        UInt         aValueSize,
                        const void*  aToken,
                        UInt         aTokenLength,
                        IDE_RC*      aResult );

static UInt mtdActualSize( const mtcColumn* aColumn,
                           const void*      aRow );

static void mtdSetNull( const mtcColumn* aColumn,
                        void*            aRow );

static UInt mtdHash( UInt             aHash,
                     const mtcColumn* aColumn,
                     const void*      aRow );

static idBool mtdIsNull( const mtcColumn* aColumn,
                         const void*      aRow );

static SInt mtdBigintLogicalAscComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2  );

static SInt mtdBigintLogicalDescComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2  );

static SInt mtdBigintFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                                 mtdValueInfo * aValueInfo2 );

static SInt mtdBigintFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                  mtdValueInfo * aValueInfo2 );

static SInt mtdBigintMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 );

static SInt mtdBigintMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 );

static SInt mtdBigintStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                          mtdValueInfo * aValueInfo2 );

static SInt mtdBigintStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 );

static SInt mtdBigintStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 );

static SInt mtdBigintStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 );

static void mtdEndian( void* aValue );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static IDE_RC mtdEncode( mtcColumn   * aColumn,
                         void        * aValue,
                         UInt          aValueSize,
                         UChar       * aCompileFmt,
                         UInt          aCompileFmtLen,
                         UChar       * aText,
                         UInt        * aTextLen,
                         IDE_RC      * aRet );

static IDE_RC mtdStoredValue2MtdValue( UInt              aColumnSize,
                                       void            * aDestValue,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue );

static UInt mtdNullValueSize();

static mtcName mtdTypeName[1] = {
    { NULL, 6, (void*)"BIGINT" },
};

static mtcColumn mtdColumn;

mtdModule mtdBigint = {
    mtdTypeName,
    &mtdColumn,
    MTD_BIGINT_ID,
    0,
    { SMI_BUILTIN_B_TREE_INDEXTYPE_ID,
      SMI_BUILTIN_B_TREE2_INDEXTYPE_ID,
      0, 0, 0, 0, 0 },
    MTD_BIGINT_ALIGN,
    MTD_GROUP_NUMBER|
      MTD_CANON_NEEDLESS|
      MTD_CREATE_ENABLE|
      MTD_COLUMN_TYPE_FIXED|
      MTD_SELECTIVITY_ENABLE|
      MTD_NUMBER_GROUP_TYPE_BIGINT|
      MTD_SEARCHABLE_PRED_BASIC|
      MTD_UNSIGNED_ATTR_TRUE|
      MTD_NUM_PREC_RADIX_TRUE|
      MTD_VARIABLE_LENGTH_TYPE_FALSE| // PROJ-1705
      MTD_DATA_STORE_DIVISIBLE_FALSE| // PROJ-1705
      MTD_DATA_STORE_MTDVALUE_FALSE|  // PROJ-1705
      MTD_PSM_TYPE_ENABLE, // PROJ-1904
    19,
    0,
    0,
    &mtdBigintNull,
    mtdInitialize,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtd::getPrecisionNA,
    mtdSetNull,
    mtdHash,
    mtdIsNull,
    mtd::isTrueNA,
    {
        mtdBigintLogicalAscComp,  // Logical Comparison
        mtdBigintLogicalDescComp,  // Logical Comparison
    },
    {
        // Key Comparison
        {
            // mt value�� ���� compare
            mtdBigintFixedMtdFixedMtdKeyAscComp, // Ascending Key Comparison
            mtdBigintFixedMtdFixedMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� ���� compare
            mtdBigintMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdBigintMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare
            mtdBigintStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdBigintStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare
            mtdBigintStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdBigintStoredStoredKeyDescComp // Descending Key Comparison
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� fixed mt value�� ���� compare */
            mtdBigintFixedMtdFixedMtdKeyAscComp,
            mtdBigintFixedMtdFixedMtdKeyDescComp
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� mt value�� ���� compare */
            mtdBigintMtdMtdKeyAscComp,
            mtdBigintMtdMtdKeyDescComp
        }
    },
    mtd::canonizeDefault,
    mtdEndian,
    mtdValidate,
    mtdDouble.selectivity,
    mtdEncode,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtd::valueFromOracleDefault,
    mtd::makeColumnInfoDefault,

    // BUG-28934
    mtk::mergeAndRangeDefault,
    mtk::mergeOrRangeListDefault,

    {    
        // PROJ-1705
        mtdStoredValue2MtdValue,
        // PROJ-2429
        NULL
    }, 
    mtdNullValueSize,
    mtd::mtdHeaderSizeDefault,

    // PROJ-2399
    mtd::mtdStoreSizeDefault
};

IDE_RC mtdInitialize( UInt aNo )
{ 
    IDE_TEST( mtd::initializeModule( &mtdBigint, aNo ) != IDE_SUCCESS );
    
    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdBigint,
                                     0,   // arguments
                                     0,   // precision
                                     0 )  // scale
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtdEstimate( UInt * aColumnSize,
                    UInt * aArguments,
                    SInt * /*aPrecision*/,
                    SInt * /*aScale*/ )
{
/***********************************************************************
 *
 * Description : data type module�� semantic �˻� �� column size ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    IDE_TEST_RAISE( *aArguments != 0, ERR_INVALID_PRECISION );
 
    *aColumnSize = MTD_BIGINT_SIZE;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_PRECISION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_PRECISION));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtdValue( mtcTemplate* /* aTemplate */,
                 mtcColumn*   aColumn,
                 void*        aValue,
                 UInt*        aValueOffset,
                 UInt         aValueSize,
                 const void*  aToken,
                 UInt         aTokenLength,
                 IDE_RC*      aResult )
{
    UInt           sValueOffset;
    mtdBigintType* sValue;
    const UChar*   sToken;
    const UChar*   sFence;
    SLong          sIntermediate;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_BIGINT_ALIGN );
    
    sToken = (const UChar*)aToken;
    
    if( sValueOffset + MTD_BIGINT_SIZE <= aValueSize )
    {
        sValue = (mtdBigintType*)( (UChar*)aValue + sValueOffset );
        if( aTokenLength == 0 )
        {
            *sValue = MTD_BIGINT_NULL;
        }
        else
        {
            sFence = sToken + aTokenLength;
            if( *sToken == '-' )
            {
                sToken++;
                IDE_TEST_RAISE( sToken >= sFence, ERR_INVALID_LITERAL );
                for( sIntermediate = 0; sToken < sFence; sToken++ )
                {
                    IDE_TEST_RAISE( *sToken < '0' || *sToken > '9',
                                    ERR_INVALID_LITERAL );
                    if( *sToken <= '7' )
                    {
                        IDE_TEST_RAISE( sIntermediate < MTD_BIGINT_MINIMUM1,
                                        ERR_VALUE_OVERFLOW );
                    }
                    else
                    {
                        IDE_TEST_RAISE( sIntermediate <=  MTD_BIGINT_MINIMUM1,
                                        ERR_VALUE_OVERFLOW );
                    }
                    sIntermediate = sIntermediate * 10 - ( *sToken - '0' );
                }
                *sValue = sIntermediate;
            }
            else
            {
                if( *sToken == '+' )
                {
                    sToken++;
                }
                IDE_TEST_RAISE( sToken >= sFence, ERR_INVALID_LITERAL );
                for( sIntermediate = 0; sToken < sFence; sToken++ )
                {
                    IDE_TEST_RAISE( *sToken < '0' || *sToken > '9',
                                    ERR_INVALID_LITERAL );
                    if( *sToken <= '7' )
                    {
                        IDE_TEST_RAISE( sIntermediate > MTD_BIGINT_MAXIMUM1,
                                        ERR_VALUE_OVERFLOW );
                    }
                    else
                    {
                        IDE_TEST_RAISE( sIntermediate >= MTD_BIGINT_MAXIMUM1,
                                        ERR_VALUE_OVERFLOW );
                    }
                    sIntermediate = sIntermediate * 10 + ( *sToken - '0' );
                }
                *sValue = sIntermediate;
            }
        }
       
        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset + MTD_BIGINT_SIZE;
        
        *aResult = IDE_SUCCESS;
    }
    else
    {
        *aResult = IDE_FAILURE;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL )
      IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));
    
    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

UInt mtdActualSize( const mtcColumn*,
                    const void*)
{
    return (UInt)MTD_BIGINT_SIZE;
}

void mtdSetNull( const mtcColumn* /* aColumn */,
                 void*            aRow )
{
    if( aRow != NULL )
    {
        *(mtdBigintType*)aRow = MTD_BIGINT_NULL;
    }
}

UInt mtdHash( UInt             aHash,
              const mtcColumn* ,
              const void*      aRow )
{
    return mtc::hash( aHash, (const UChar*)aRow, MTD_BIGINT_SIZE );
}

idBool mtdIsNull( const mtcColumn* ,
                  const void*      aRow )
{
    return *(mtdBigintType*)aRow == MTD_BIGINT_NULL ? ID_TRUE : ID_FALSE ;
}

SInt mtdBigintLogicalAscComp( mtdValueInfo * aValueInfo1,
                              mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 > sValue2 )
        {
            return 1;
        }
        if( sValue1 < sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintLogicalDescComp( mtdValueInfo * aValueInfo1,
                               mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 < sValue2 )
        {
            return 1;
        }
        if( sValue1 > sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                          mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)MTD_VALUE_FIXED( aValueInfo1 );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)MTD_VALUE_FIXED( aValueInfo2 );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 > sValue2 )
        {
            return 1;
        }
        if( sValue1 < sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)MTD_VALUE_FIXED( aValueInfo1 );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)MTD_VALUE_FIXED( aValueInfo2 );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 < sValue2 )
        {
            return 1;
        }
        if( sValue1 > sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                             aValueInfo1->value,
                             aValueInfo1->flag,
                             mtdBigint.staticNull );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                             aValueInfo2->value,
                             aValueInfo2->flag,
                             mtdBigint.staticNull );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 > sValue2 )
        {
            return 1;
        }
        if( sValue1 < sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                 mtdValueInfo * aValueInfo2 )
{
    mtdBigintType   sValue1;
    mtdBigintType   sValue2;

    //---------
    // value1
    //---------
    sValue1 = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                             aValueInfo1->value,
                             aValueInfo1->flag,
                             mtdBigint.staticNull );

    //---------
    // value2
    //---------
    sValue2 = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                             aValueInfo2->value,
                             aValueInfo2->flag,
                             mtdBigint.staticNull );

    //---------
    // compare 
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 < sValue2 )
        {
            return 1;
        }
        if( sValue1 > sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                   mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdBigintType sValue1;
    mtdBigintType sValue2;

    //---------
    // value1
    //---------        
    ID_LONG_BYTE_ASSIGN( &sValue1, aValueInfo1->value );
    

    //---------
    // value2
    //---------        
    sValue2 = *(mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column, 
                             aValueInfo2->value, 
                             aValueInfo2->flag,
                             mtdBigint.staticNull );
    //---------
    // compare
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 > sValue2 )
        {
            return 1;
        }
        if( sValue1 < sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                    mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdBigintType sValue1;
    mtdBigintType sValue2;
    
    //---------
    // value1
    //---------        
    ID_LONG_BYTE_ASSIGN( &sValue1, aValueInfo1->value );

    //---------
    // value2
    //---------        
    sValue2 = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column, 
                             aValueInfo2->value, 
                             aValueInfo2->flag,
                             mtdBigint.staticNull );
    
    //---------
    // compare
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 < sValue2 )
        {
            return 1;
        }
        if( sValue1 > sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdBigintType    sValue1;
    mtdBigintType    sValue2;

    //---------
    // value1
    //---------
    ID_LONG_BYTE_ASSIGN( &sValue1, aValueInfo1->value );

    //---------
    // value2
    //---------
    ID_LONG_BYTE_ASSIGN( &sValue2, aValueInfo2->value );

    //---------
    // compare
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 > sValue2 )
        {
            return 1;
        }
        if( sValue1 < sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdBigintStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdBigintType    sValue1;
    mtdBigintType    sValue2;

    //---------
    // value1
    //---------
    ID_LONG_BYTE_ASSIGN( &sValue1, aValueInfo1->value );
    
    //---------
    // value2
    //---------        
    ID_LONG_BYTE_ASSIGN( &sValue2, aValueInfo2->value );
    
    //---------
    // compare
    //---------
    if( sValue1 != MTD_BIGINT_NULL && sValue2 != MTD_BIGINT_NULL )
    {
        if( sValue1 < sValue2 )
        {
            return 1;
        }
        if( sValue1 > sValue2 )
        {
            return -1;
        }
        return 0;
    }

    if( sValue1 < sValue2 )
    {
        return 1;
    }
    if( sValue1 > sValue2 )
    {
        return -1;
    }
    return 0;
}

/* PROJ-2446 ONE SOURCE ȣ�� �Ǵ� ���� ���� Ȯ�� �ʿ� */
void mtdPartialKeyAscending( ULong*           aPartialKey,
                             const mtcColumn* aColumn,
                             const void*      aRow,
                             UInt             aFlag )
{
    mtdBigintType sValue;
    
    sValue = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aColumn, 
                             aRow, 
                             aFlag,
                             mtdBigint.staticNull );
    
    if( sValue > 0 )
    {
        *aPartialKey = (ULong)sValue + ID_ULONG(9223372036854775807);
    }
    else if( sValue != MTD_BIGINT_NULL )
    {
        *aPartialKey = sValue + ID_LONG(9223372036854775807);
    }
    else
    {
        *aPartialKey = MTD_PARTIAL_KEY_MAXIMUM;
    }
}

/* PROJ-2446 ONE SOURCE ȣ�� �Ǵ� ���� ���� Ȯ�� �ʿ� */
void mtdPartialKeyDescending( ULong*           aPartialKey,
                              const mtcColumn* aColumn,
                              const void*      aRow,
                              UInt             aFlag )
{
    mtdBigintType sValue;
    
    sValue = *(const mtdBigintType*)
        mtd::valueForModule( (smiColumn*)aColumn, 
                             aRow, 
                             aFlag,
                             mtdBigint.staticNull );
    
    if( sValue > 0 )
    {
        *aPartialKey = ID_ULONG(9223372036854775807) - (ULong)sValue;
    }
    else if( sValue != MTD_BIGINT_NULL )
    {
        *aPartialKey = ID_LONG(9223372036854775807) + (ULong)(-sValue);
    }
    else
    {
        *aPartialKey = MTD_PARTIAL_KEY_MAXIMUM;
    }
}

void mtdEndian( void* aValue )
{
    UChar* sValue;
    UChar  sIntermediate;
    
    sValue        = (UChar*)aValue;
    sIntermediate = sValue[0];
    sValue[0]     = sValue[7];
    sValue[7]     = sIntermediate;
    sIntermediate = sValue[1];
    sValue[1]     = sValue[6];
    sValue[6]     = sIntermediate;
    sIntermediate = sValue[2];
    sValue[2]     = sValue[5];
    sValue[5]     = sIntermediate;
    sIntermediate = sValue[3];
    sValue[3]     = sValue[4];
    sValue[4]     = sIntermediate;
}


IDE_RC mtdValidate( mtcColumn * aColumn,
                    void      * aValue,
                    UInt        aValueSize)
{
/***********************************************************************
 *
 * Description : value�� semantic �˻� �� mtcColum �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    
    IDE_TEST_RAISE( aValue == NULL, ERR_INVALID_NULL );
    
    IDE_TEST_RAISE( aValueSize != ID_SIZEOF(mtdBigintType),
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdBigint,
                                     0,   // arguments
                                     0,   // precision
                                     0 )  // scale
              != IDE_SUCCESS );

    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_NULL);
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE));
    }
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mtdEncode( mtcColumn  * /* aColumn */,
                  void       * aValue,
                  UInt         /* aValueSize */,
                  UChar      * /* aCompileFmt */,
                  UInt         /* aCompileFmtLen */,
                  UChar      * aText,
                  UInt       * aTextLen,
                  IDE_RC     * aRet )
{
    UInt sStringLen;

    //----------------------------------
    // Parameter Validation
    //----------------------------------

    IDE_ASSERT( aValue != NULL );
    IDE_ASSERT( aText != NULL );
    IDE_ASSERT( *aTextLen > 0 );
    IDE_ASSERT( aRet != NULL );
    
    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;

    //----------------------------------
    // Set String
    //----------------------------------
    
    // To Fix BUG-16801
    if ( mtdIsNull( NULL, aValue ) == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sStringLen = idlVA::appendFormat( (SChar*) aText, 
                                          *aTextLen,
                                          "%"ID_INT64_FMT,
                                          *(mtdBigintType*)aValue );
    }

    //----------------------------------
    // Finalization
    //----------------------------------
    
    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;
    
    *aRet = IDE_SUCCESS;
    
    return IDE_SUCCESS;
}

IDE_RC mtdStoredValue2MtdValue( UInt              aColumnSize,
                                void            * aDestValue,
                                UInt           /* aDestValueOffset */,
                                UInt              aLength,
                                const void      * aValue )
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdBigintType* sBigintValue;

    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����.

    sBigintValue = (mtdBigintType*)aDestValue;
    
    if( aLength == 0 )
    {
        // NULL ����Ÿ
        *sBigintValue = mtdBigintNull;        
    }
    else
    {
        IDE_TEST_RAISE( aLength != aColumnSize , ERR_INVALID_STORED_VALUE );
        IDE_TEST_RAISE( aLength != ID_SIZEOF( mtdBigintType ), ERR_INVALID_STORED_VALUE );

        ID_LONG_BYTE_ASSIGN( sBigintValue, aValue );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt mtdNullValueSize()
{
/*******************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ
 *******************************************************************/

    return mtdActualSize( NULL,
                          & mtdBigintNull );
}

