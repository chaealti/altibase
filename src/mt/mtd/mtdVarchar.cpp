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
 * $Id: mtdVarchar.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>
#include <mtuProperty.h>
#include <mtlCollate.h>

extern mtdModule mtdVarchar;
extern mtdModule mtdChar;

#define MTD_VARCHAR_ALIGN             (ID_SIZEOF(UShort))
#define MTD_VARCHAR_PRECISION_DEFAULT (1) // To Fix BUG-12597

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

static IDE_RC mtdGetPrecision( const mtcColumn * aColumn,
                               const void      * aRow,
                               SInt            * aPrecision,
                               SInt            * aScale );

static void mtdSetNull( const mtcColumn* aColumn,
                        void*            aRow );

static UInt mtdHash( UInt             aHash,
                     const mtcColumn* aColumn,
                     const void*      aRow );

static idBool mtdIsNull( const mtcColumn* aColumn,
                         const void*      aRow );

static SInt mtdVarcharLogicalAscComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharLogicalDescComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                                  mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                               mtdValueInfo * aValueInfo2 );

/* PROJ-2433 */
static SInt mtdVarcharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                                  mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 );

static SInt mtdVarcharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 );
/* PROJ-2433 end */

static IDE_RC mtdCanonize( const mtcColumn * aCanon,
                           void**            aCanonizedValue,
                           mtcEncryptInfo  * aCanonInfo,
                           const mtcColumn * aColumn,
                           void*             aValue,
                           mtcEncryptInfo  * aColumnInfo,
                           mtcTemplate     * aTemplate );

static void mtdEndian( void* aValue );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static IDE_RC mtdValueFromOracle( mtcColumn*  aColumn,
                                  void*       aValue,
                                  UInt*       aValueOffset,
                                  UInt        aValueSize,
                                  const void* aOracleValue,
                                  SInt        aOracleLength,
                                  IDE_RC*     aResult );

static IDE_RC mtdStoredValue2MtdValue( UInt              aColumnSize,
                                       void            * aDestValue,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue );

static UInt mtdNullValueSize();

static UInt mtdHeaderSize();

static UInt mtdStoreSize( const smiColumn * aColumn );

static mtcName mtdTypeName[2] = {
    { mtdTypeName+1, 7, (void*)"VARCHAR"  },
    { NULL,          8, (void*)"VARCHAR2" }
};

static mtcColumn mtdColumn;

mtdModule mtdVarchar = {
    mtdTypeName,
    &mtdColumn,
    MTD_VARCHAR_ID,
    0,
    { SMI_BUILTIN_B_TREE_INDEXTYPE_ID,
      SMI_BUILTIN_B_TREE2_INDEXTYPE_ID,
      0, 0, 0, 0, 0 },
    MTD_VARCHAR_ALIGN,
    MTD_GROUP_TEXT|
      MTD_CANON_NEED|
      MTD_CREATE_ENABLE|
      MTD_COLUMN_TYPE_VARIABLE|
      MTD_SELECTIVITY_ENABLE|
      MTD_CREATE_PARAM_PRECISION|
      MTD_CASE_SENS_TRUE|
      MTD_SEARCHABLE_SEARCHABLE|
      MTD_LITERAL_TRUE|
      MTD_VARIABLE_LENGTH_TYPE_TRUE|  // PROJ-1705
      MTD_DATA_STORE_DIVISIBLE_TRUE|  // PROJ-1705
      MTD_DATA_STORE_MTDVALUE_FALSE|  // PROJ-1705
      MTD_PSM_TYPE_ENABLE, // PROJ-1904
    MTD_CHAR_STORE_PRECISION_MAXIMUM,
    0,
    0,
    (void*)&mtdCharNull,
    mtdInitialize,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecision,
    mtdSetNull,
    mtdHash,
    mtdIsNull,
    mtd::isTrueNA,
    {
        mtdVarcharLogicalAscComp,    // Logical Comparison
        mtdVarcharLogicalDescComp
    },
    {
        // Key Comparison
        {
            // mt value�� ���� compare
            mtdVarcharFixedMtdFixedMtdKeyAscComp, // Ascending Key Comparison
            mtdVarcharFixedMtdFixedMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� ���� compare
            mtdVarcharMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdVarcharMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare
            mtdVarcharStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdVarcharStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare
            mtdVarcharStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdVarcharStoredStoredKeyDescComp // Descending Key Comparison
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� fixed mt value�� ���� compare */
            mtdVarcharIndexKeyFixedMtdKeyAscComp,
            mtdVarcharIndexKeyFixedMtdKeyDescComp
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� mt value�� ���� compare */
            mtdVarcharIndexKeyMtdKeyAscComp,
            mtdVarcharIndexKeyMtdKeyDescComp
        }
    },
    mtdCanonize,
    mtdEndian,
    mtdValidate,
    mtdChar.selectivity,
    mtd::encodeCharDefault,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtdValueFromOracle,
    mtd::makeColumnInfoDefault,

    // BUG-28934
    mtk::mergeAndRangeDefault,
    mtk::mergeOrRangeListDefault,

    {
        // PROJ-1705
        mtdStoredValue2MtdValue,
        // PROJ-2429
        mtd::mtdStoredValue2MtdValue4CompressColumn
    },
    mtdNullValueSize,
    mtdHeaderSize,

    // PROJ-2399
    mtdStoreSize
};

IDE_RC mtdInitialize( UInt aNo )
{
    IDE_TEST( mtd::initializeModule( &mtdVarchar, aNo )
              != IDE_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdVarchar,
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
                    SInt * aPrecision,
                    SInt * /*aScale*/ )
{
    if( *aArguments == 0 )
    {
        *aArguments = 1;
        *aPrecision = MTD_VARCHAR_PRECISION_DEFAULT;
    }
    
    IDE_TEST_RAISE( *aArguments != 1, ERR_INVALID_SCALE );

    IDE_TEST_RAISE( *aPrecision < MTD_VARCHAR_PRECISION_MINIMUM ||
                    *aPrecision > MTD_VARCHAR_PRECISION_MAXIMUM,
                    ERR_INVALID_LENGTH );

    *aColumnSize = ID_SIZEOF(UShort) + *aPrecision;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    
    IDE_EXCEPTION( ERR_INVALID_SCALE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_SCALE));
    
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
    UInt         sValueOffset;
    mtdCharType* sValue;
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    UChar*       sFence;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_VARCHAR_ALIGN );

    sValue = (mtdCharType*)( (UChar*)aValue + sValueOffset );

    *aResult = IDE_SUCCESS;

    // To fix BUG-13444
    // tokenFence�� RowFence�� ������ �˻�����̹Ƿ�,
    // ���� RowFence�˻� �� TokenFence�˻縦 �ؾ� �Ѵ�.
    sIterator = sValue->value;
    sFence    = (UChar*)aValue + aValueSize;
    if( sIterator >= sFence )
    {
        *aResult = IDE_FAILURE;
    }
    else
    {
        for( sToken      = (const UChar*)aToken,
             sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
             sIterator++, sToken++ )
        {
            if( sIterator >= sFence )
            {
                *aResult = IDE_FAILURE;
                break;
            }
            if( *sToken == '\'' )
            {
                sToken++;
                IDE_TEST_RAISE( sToken >= sTokenFence || *sToken != '\'',
                                ERR_INVALID_LITERAL );
            }
            *sIterator = *sToken;
        }

        // BUG-40290
        // ���۰� ���߶� �߷��� length �� ������ָ� ����.
        sValue->length = sIterator - sValue->value;
    }

    if( *aResult == IDE_SUCCESS )
    {
        // precision, scale �� ���� ��, estimate�� semantic �˻�
        aColumn->flag            = 1;
        aColumn->precision       = sValue->length != 0 ? sValue->length : 1;
        aColumn->scale           = 0;

        IDE_TEST( mtdEstimate( & aColumn->column.size,
                               & aColumn->flag,
                               & aColumn->precision,
                               & aColumn->scale )
                  != IDE_SUCCESS );
        
        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset
                                   + ID_SIZEOF(UShort) + sValue->length;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

UInt mtdActualSize( const mtcColumn* ,
                    const void*      aRow )
{
    return ID_SIZEOF(UShort) + ((mtdCharType*)aRow)->length;
}

static IDE_RC mtdGetPrecision( const mtcColumn * ,
                               const void      * aRow,
                               SInt            * aPrecision,
                               SInt            * aScale )
{
    *aPrecision = ((mtdCharType*)aRow)->length;
    *aScale = 0;

    return IDE_SUCCESS;
}

void mtdSetNull( const mtcColumn* /*aColumn*/,
                 void*            aRow )
{
    if( aRow != NULL )
    {
        ((mtdCharType*)aRow)->length = 0;
    }
}

UInt mtdHash( UInt             aHash,
              const mtcColumn* ,
              const void*      aRow )
{
    return mtc::hashWithoutSpace( aHash, ((mtdCharType*)aRow)->value, ((mtdCharType*)aRow)->length );
}

idBool mtdIsNull( const mtcColumn* ,
                  const void*      aRow )
{
    return (((mtdCharType*)aRow)->length == 0) ? ID_TRUE : ID_FALSE;
}

SInt mtdVarcharLogicalAscComp( mtdValueInfo * aValueInfo1,
                               mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    
    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (UChar*)sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength1 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue1, sValue2, sLength1 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharLogicalDescComp( mtdValueInfo * aValueInfo1,
                                mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    //---------
    // compare
    //---------

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength2 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue2, sValue1, sLength2 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    
    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    //---------
    // compare
    //---------

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (UChar*)sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength1 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue1, sValue2, sLength1 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    //---------
    // compare
    //---------

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength2 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue2, sValue1, sLength2 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                 mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                           aValueInfo1->value,
                                           aValueInfo1->flag,
                                           mtdVarchar.staticNull );
    sLength1 = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                           aValueInfo2->value,
                                           aValueInfo2->flag,
                                           mtdVarchar.staticNull );

    sLength2 = sVarcharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (UChar*)sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength1 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue1, sValue2, sLength1 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                  mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------        
    sVarcharValue1 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                           aValueInfo1->value,
                                           aValueInfo1->flag,
                                           mtdVarchar.staticNull );
    sLength1 = sVarcharValue1->length;

    //---------
    // value2
    //---------        
    sVarcharValue2 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                           aValueInfo2->value,
                                           aValueInfo2->flag,
                                           mtdVarchar.staticNull );

    sLength2 = sVarcharValue2->length;

    //---------
    // compare
    //---------

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;
        
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength2 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue2, sValue1, sLength2 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                    mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDummy;
    
    //---------
    // value1
    //---------    
    // PROJ-2429 Dictionary based data compress for on-disk DB
    // Dictionary compression column�� ��� store type��mt type����
    // ��ȯ�ؼ� ���� �����͸� �����´�.
    if ( (((smiColumn*)aValueInfo1->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength1 = (UShort)aValueInfo1->length;
        sValue1  = (UChar*)aValueInfo1->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo1->length == ID_SIZEOF(smOID) );

        sVarcharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sVarcharValue1->length;
        sValue1  = sVarcharValue1->value;
    }
    
    //---------
    // value2
    //---------        
    sVarcharValue2 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                           aValueInfo2->value,
                                           aValueInfo2->flag,
                                           mtdVarchar.staticNull );
    sLength2 = sVarcharValue2->length;
    sValue2  = sVarcharValue2->value;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength1 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue1, sValue2, sLength1 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDummy;
    
    //---------
    // value1
    //---------    
    // PROJ-2429 Dictionary based data compress for on-disk DB
    // Dictionary compression column�� ��� store type��mt type����
    // ��ȯ�ؼ� ���� �����͸� �����´�.
    if ( (((smiColumn*)aValueInfo1->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength1 = (UShort)aValueInfo1->length;
        sValue1  = (UChar*)aValueInfo1->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo1->length == ID_SIZEOF(smOID) );

        sVarcharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sVarcharValue1->length;
        sValue1  = sVarcharValue1->value;
    }

    //---------
    // value2
    //---------        
    sVarcharValue2 = (const mtdCharType*)
                      mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                           aValueInfo2->value,
                                           aValueInfo2->flag,
                                           mtdVarchar.staticNull );
    sLength2 = sVarcharValue2->length;
    sValue2  = sVarcharValue2->value;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength2 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue2, sValue1, sLength2 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}
SInt mtdVarcharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDummy;

    //---------
    // value1
    //---------    
    if ( (((smiColumn*)aValueInfo1->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength1 = (UShort)aValueInfo1->length;
        sValue1  = (UChar*)aValueInfo1->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo1->length == ID_SIZEOF(smOID) );

        sVarcharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sVarcharValue1->length;
        sValue1  = sVarcharValue1->value;
    }

    //---------
    // value2
    //---------    
    if ( (((smiColumn*)aValueInfo2->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength2 = (UShort)aValueInfo2->length;
        sValue2  = (UChar*)aValueInfo2->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo1->length == ID_SIZEOF(smOID) );

        sVarcharValue2 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength2 = sVarcharValue2->length;
        sValue2  = sVarcharValue2->value;
    }

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2,
                                  sLength1 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue1, sValue2, sLength1 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

SInt mtdVarcharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDummy;

    //---------
    // value1
    //---------    
    if ( (((smiColumn*)aValueInfo1->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength1 = (UShort)aValueInfo1->length;
        sValue1  = (UChar*)aValueInfo1->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo1->length == ID_SIZEOF(smOID) );

        sVarcharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sVarcharValue1->length;
        sValue1  = sVarcharValue1->value;
    }

    //---------
    // value2
    //---------    
    if ( (((smiColumn*)aValueInfo2->column)->flag & SMI_COLUMN_COMPRESSION_MASK) !=
         SMI_COLUMN_COMPRESSION_TRUE )
    {
        sLength2 = (UShort)aValueInfo2->length;
        sValue2  = (UChar*)aValueInfo2->value;
        
    }
    else
    {
        IDE_DASSERT( aValueInfo2->length == ID_SIZEOF(smOID) );

        sVarcharValue2 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength2 = sVarcharValue2->length;
        sValue2  = sVarcharValue2->value;
    }

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1,
                                  sLength2 ) > 0 ? 1 : -1 ;
        }
        return idlOS::memcmp( sValue2, sValue1, sLength2 );
    }
    
    if( sLength1 < sLength2 )
    {
        return 1;
    }
    if( sLength1 > sLength2 )
    {
        return -1;
    }
    return 0;
}

/* PROJ-2433
 * Direct key Index�� direct key�� mtd�� compare �Լ�
 * - partial direct key�� ó���ϴºκ� �߰� */
SInt mtdVarcharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;
    
    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    /*
     * PROJ-2433 Direct Key Index
     * Partial Direct Key ó��
     * 
     * - Direct Key�� partial direct key�� ���
     *   partial�� ���̸�ŭ�� ���ϵ��� length�� �����Ѵ�
     */ 
    if ( ( aValueInfo1->flag & MTD_PARTIAL_KEY_MASK ) == MTD_PARTIAL_KEY_ON )
    {
        sDirectKeyPartialSize = aValueInfo1->length;

        /* partail key �̸� */
        if ( sDirectKeyPartialSize != 0 )
        {
            /* direct key ���̺���*/
            if ( ( sLength1 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength1 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }

            /* search key �� partial ���̸�ŭ ����*/
            if ( ( sLength2 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength2 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }
        }
        else
        {
            /* nothing todo */
        }
    }
    else
    {
        /* nothing todo */
    }

    //---------
    // compare
    //---------

    if ( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        sValue1  = (UChar*)sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;

        if ( sLength1 > sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength2 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        if ( sLength1 < sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength1 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        return ( idlOS::memcmp( sValue1,
                                sValue2,
                                sLength1 ) );
    }
    else
    {
        /* nothing todo */
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing todo */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing todo */
    }
    return 0;
}

SInt mtdVarcharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1       = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2       = sVarcharValue2->length;

    /*
     * PROJ-2433 Direct Key Index
     * Partial Direct Key ó��
     * 
     * - Direct Key�� partial direct key�� ���
     *   partial�� ���̸�ŭ�� ���ϵ��� length�� �����Ѵ�
     */ 
    if ( ( aValueInfo1->flag & MTD_PARTIAL_KEY_MASK ) == MTD_PARTIAL_KEY_ON )
    {
        sDirectKeyPartialSize = aValueInfo1->length;

        /* partail key �̸� */
        if ( sDirectKeyPartialSize != 0 )
        {
            /* direct key ���̺���*/
            if ( ( sLength1 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength1 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }

            /* search key �� partial ���̸�ŭ ����*/
            if ( ( sLength2 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength2 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }
        }
        else
        {
            /* nothing todo */
        }
    }
    else
    {
        /* nothing todo */
    }

    //---------
    // compare
    //---------

    if ( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        sValue1  = sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;

        if ( sLength2 > sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength1 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        if ( sLength2 < sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength2 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        return ( idlOS::memcmp( sValue2,
                                sValue1,
                                sLength2 ) ) ;
    }
    else
    {
        /* nothing todo */
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing todo */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing todo */
    }
    return 0;
}

SInt mtdVarcharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------
    sVarcharValue1 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                               aValueInfo1->value,
                                                               aValueInfo1->flag,
                                                               mtdVarchar.staticNull );
    sLength1 = sVarcharValue1->length;

    //---------
    // value2
    //---------
    sVarcharValue2 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                               aValueInfo2->value,
                                                               aValueInfo2->flag,
                                                               mtdVarchar.staticNull );

    sLength2 = sVarcharValue2->length;

    /*
     * PROJ-2433 Direct Key Index
     * Partial Direct Key ó��
     * 
     * - Direct Key�� partial direct key�� ���
     *   partial�� ���̸�ŭ�� ���ϵ��� length�� �����Ѵ�
     */ 
    if ( ( aValueInfo1->flag & MTD_PARTIAL_KEY_MASK ) == MTD_PARTIAL_KEY_ON )
    {
        sDirectKeyPartialSize = aValueInfo1->length;

        /* partail key �̸� */
        if ( sDirectKeyPartialSize != 0 )
        {
            /* direct key ���̺���*/
            if ( ( sLength1 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength1 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }

            /* search key �� partial ���̸�ŭ ����*/
            if ( ( sLength2 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength2 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }
        }
        else
        {
            /* nothing todo */
        }
    }
    else
    {
        /* nothing todo */
    }

    //---------
    // compare
    //---------
    
    if ( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        sValue1  = (UChar*)sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;

        if ( sLength1 > sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength2 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        if ( sLength1 < sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength1 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        return ( idlOS::memcmp( sValue1,
                                sValue2,
                                sLength1 ) ) ;
    }
    else
    {
        /* nothing todo */
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing todo */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing todo */
    }
    return 0;
}

SInt mtdVarcharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sVarcharValue1;
    const mtdCharType  * sVarcharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------        
    sVarcharValue1 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                               aValueInfo1->value,
                                                               aValueInfo1->flag,
                                                               mtdVarchar.staticNull );
    sLength1 = sVarcharValue1->length;

    //---------
    // value2
    //---------        
    sVarcharValue2 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                               aValueInfo2->value,
                                                               aValueInfo2->flag,
                                                               mtdVarchar.staticNull );

    sLength2 = sVarcharValue2->length;

    /*
     * PROJ-2433 Direct Key Index
     * Partial Direct Key ó��
     * 
     * - Direct Key�� partial direct key�� ���
     *   partial�� ���̸�ŭ�� ���ϵ��� length�� �����Ѵ�
     */ 
    if ( ( aValueInfo1->flag & MTD_PARTIAL_KEY_MASK ) == MTD_PARTIAL_KEY_ON )
    {
        sDirectKeyPartialSize = aValueInfo1->length;

        /* partail key �̸� */
        if ( sDirectKeyPartialSize != 0 )
        {
            /* direct key ���̺���*/
            if ( ( sLength1 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength1 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }

            /* search key �� partial ���̸�ŭ ����*/
            if ( ( sLength2 + mtdHeaderSize() ) > sDirectKeyPartialSize )
            {
                sLength2 = (UShort)( sDirectKeyPartialSize - mtdHeaderSize() );
            }
            else
            {
                /* nothing todo */
            }
        }
        else
        {
            /* nothing todo */
        }
    }
    else
    {
        /* nothing todo */
    }

    //---------
    // compare
    //---------

    if ( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        sValue1  = sVarcharValue1->value;
        sValue2  = sVarcharValue2->value;

        if ( sLength2 > sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength1 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        if ( sLength2 < sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength2 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing todo */
        }
        return ( idlOS::memcmp( sValue2,
                                sValue1,
                                sLength2 ) );
    }
    else
    {
        /* nothing todo */
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing todo */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing todo */
    }
    return 0;
}

static IDE_RC mtdCanonize( const mtcColumn * aCanon,
                           void**            aCanonizedValue,
                           mtcEncryptInfo  * /* aCanonInfo */,
                           const mtcColumn * /* aColumn */,
                           void*             aValue,
                           mtcEncryptInfo  * /* aColumnInfo */,
                           mtcTemplate     * /* aTemplate */ )
{
    IDE_TEST_RAISE( ((mtdCharType*)aValue)->length > (UInt)aCanon->precision,
                    ERR_INVALID_LENGTH );
    
    *aCanonizedValue = aValue;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

void mtdEndian( void* aValue )
{
    UChar* sLength;
    UChar  sIntermediate;
    
    sLength = (UChar*)&(((mtdCharType*)aValue)->length);
    
    sIntermediate = sLength[0];
    sLength[0]    = sLength[1];
    sLength[1]    = sIntermediate;
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
        
    mtdCharType * sCharVal = (mtdCharType*)aValue;
    IDE_TEST_RAISE( sCharVal == NULL, ERR_INVALID_NULL );
    
    IDE_TEST_RAISE( aValueSize < ID_SIZEOF(UShort), ERR_INVALID_LENGTH);
    IDE_TEST_RAISE( sCharVal->length + ID_SIZEOF(UShort) != aValueSize,
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdVarchar,
                                     1,                // arguments
                                     sCharVal->length, // precision
                                     0 )               // scale
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

IDE_RC mtdValueFromOracle( mtcColumn*  aColumn,
                           void*       aValue,
                           UInt*       aValueOffset,
                           UInt        aValueSize,
                           const void* aOracleValue,
                           SInt        aOracleLength,
                           IDE_RC*     aResult )
{
    UInt         sValueOffset;
    mtdCharType* sValue;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_VARCHAR_ALIGN );
    
    if( aOracleLength < 0 )
    {
        aOracleLength = 0;
    }

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdVarchar,
                                     1,
                                     ( aOracleLength > 1 ) ? aOracleLength : 1,
                                     0 )
        != IDE_SUCCESS );
    
    if( sValueOffset + aColumn->column.size <= aValueSize )
    {
        sValue = (mtdCharType*)((UChar*)aValue+sValueOffset);
        
        sValue->length = aOracleLength;
        
        idlOS::memcpy( sValue->value, aOracleValue, aOracleLength );
        
        aColumn->column.offset = sValueOffset;
        
        *aValueOffset = sValueOffset + aColumn->column.size;
        
        *aResult = IDE_SUCCESS;
    }
    else
    {
        *aResult = IDE_FAILURE;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

static IDE_RC mtdStoredValue2MtdValue( UInt              aColumnSize,
                                       void            * aDestValue,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue )
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdCharType* sVarcharValue;

    sVarcharValue = (mtdCharType*)aDestValue;
    
    if( ( aDestValueOffset == 0 ) && ( aLength == 0 ) )
    {
        // NULL ����Ÿ
        sVarcharValue->length = 0;
    }
    else
    {
        IDE_TEST_RAISE( (aDestValueOffset + aLength + mtdHeaderSize()) > aColumnSize, ERR_INVALID_STORED_VALUE );

        sVarcharValue->length = (UShort)(aDestValueOffset + aLength);
        idlOS::memcpy( (UChar*)sVarcharValue + mtdHeaderSize() + aDestValueOffset, aValue, aLength );
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
 * �� ) mtdCharType( UShort length; UChar value[1] ) ����
 *      length Ÿ���� UShort�� ũ�⸦ ��ȯ
 *******************************************************************/
    return mtdActualSize( NULL, &mtdCharNull );
}

static UInt mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdCharType( UShort length; UChar value[1] ) ����
 *      length Ÿ���� UShort�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/

    return ID_SIZEOF(UShort);    
}

static UInt mtdStoreSize( const smiColumn * /*aColumn*/ ) 
{
/***********************************************************************
 * PROJ-2399 row tmaplate 
 * sm�� ����Ǵ� �������� ũ�⸦ ��ȯ�Ѵ�.
 * variable Ÿ���� ������ Ÿ���� ID_UINT_MAX�� ��ȯ
 * mtheader�� sm�� ����Ȱ�찡 �ƴϸ� mtheaderũ�⸦ ���� ��ȯ
 **********************************************************************/

    return ID_UINT_MAX;
}
