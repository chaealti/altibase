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
 * $Id: mtdChar.cpp 86985 2020-03-23 02:02:28Z donovan.seo $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>
#include <mtuProperty.h>
#include <mtlCollate.h>

extern mtdModule mtdChar;
extern mtdModule mtdDouble;


//------------------------------------------------------
// mtdSelectivityChar()�� ���� ��ũ�� ����
//------------------------------------------------------

// numeric type of string
# define MTD_DECIMAL      0x00
# define MTD_HEXA_UPPER   0x01
# define MTD_HEXA_LOWER   0x02
# define MTD_ORDINARY     0x04

// MIN
# define MTD_MIN(a,b) ((a<b)?a:b)


// To Remove Warning
const mtdCharType mtdCharNull = { 0, {'\0',} };

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

static SInt mtdCharLogicalAscComp( mtdValueInfo * aValueInfo1,
                                   mtdValueInfo * aValueInfo2 );

static SInt mtdCharLogicalDescComp( mtdValueInfo * aValueInfo1,
                                    mtdValueInfo * aValueInfo2 );

static SInt mtdCharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                               mtdValueInfo * aValueInfo2 );

static SInt mtdCharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                mtdValueInfo * aValueInfo2 );

static SInt mtdCharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2 );

static SInt mtdCharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2 );

static SInt mtdCharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 );

static SInt mtdCharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 );

static SInt mtdCharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 );

static SInt mtdCharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 );

/* PROJ-2433 */
static SInt mtdCharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                               mtdValueInfo * aValueInfo2 );

static SInt mtdCharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                mtdValueInfo * aValueInfo2 );

static SInt mtdCharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                          mtdValueInfo * aValueInfo2 );

static SInt mtdCharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                           mtdValueInfo * aValueInfo2 );
/* PROJ-2433 end */

static IDE_RC mtdCanonize( const mtcColumn * aCanon,
                           void**            aCanonized,
                           mtcEncryptInfo  * aCanonInfo,
                           const mtcColumn * aColumn,
                           void*             aValue,
                           mtcEncryptInfo  * aColumnInfo,
                           mtcTemplate     * aTemplate );

static void mtdEndian( void* aValue );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static SDouble mtdSelectivityChar( void     * aColumnMax,
                                   void     * aColumnMin,
                                   void     * aValueMax,
                                   void     * aValueMin,
                                   SDouble    aBoundFactor,
                                   SDouble    aTotalRecordCnt );

static vSLong mtdStringType( const mtdCharType * aValue );

static SDouble mtdDigitsToDouble( const mtdCharType * aValue, UInt aBase );

static SDouble mtdConvertToDouble( const mtdCharType * aValue );

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

static mtcName mtdTypeName[1] = {
    { NULL, 4, (void*)"CHAR" }
};

static mtcColumn mtdColumn;

mtdModule mtdChar = {
    mtdTypeName,
    &mtdColumn,
    MTD_CHAR_ID,
    0,
    { SMI_BUILTIN_B_TREE_INDEXTYPE_ID,
      SMI_BUILTIN_B_TREE2_INDEXTYPE_ID,
      0, 0, 0, 0, 0 },
    MTD_CHAR_ALIGN,
    MTD_GROUP_TEXT|
    MTD_CANON_NEED_WITH_ALLOCATION|
    MTD_CREATE_ENABLE|
    MTD_COLUMN_TYPE_FIXED|
    MTD_SELECTIVITY_ENABLE|
    MTD_CREATE_PARAM_PRECISION|
    MTD_CASE_SENS_TRUE|
    MTD_SEARCHABLE_SEARCHABLE|  // BUG-17020
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
        mtdCharLogicalAscComp,    // Logical Comparison
        mtdCharLogicalDescComp
    },
    {
        // Key Comparison
        {
            // mt value�� ���� compare
            mtdCharFixedMtdFixedMtdKeyAscComp, // Ascending Key Comparison
            mtdCharFixedMtdFixedMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� ���� compare
            mtdCharMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdCharMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare
            mtdCharStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdCharStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare
            mtdCharStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdCharStoredStoredKeyDescComp // Descending Key Comparison
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� fixed mt value�� ���� compare */
            mtdCharIndexKeyFixedMtdKeyAscComp,
            mtdCharIndexKeyFixedMtdKeyDescComp
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� mt value�� ���� compare */
            mtdCharIndexKeyMtdKeyAscComp,
            mtdCharIndexKeyMtdKeyDescComp
        }
    },
    mtdCanonize,
    mtdEndian,
    mtdValidate,
    mtdSelectivityChar,
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
    IDE_TEST( mtd::initializeModule( &mtdChar, aNo ) != IDE_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdChar,
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
        *aPrecision = MTD_CHAR_PRECISION_DEFAULT;
    }

    IDE_TEST_RAISE( *aArguments != 1, ERR_INVALID_SCALE );

    IDE_TEST_RAISE( *aPrecision < MTD_CHAR_PRECISION_MINIMUM ||
                    *aPrecision > MTD_CHAR_PRECISION_MAXIMUM,
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

    sValueOffset = idlOS::align( *aValueOffset, MTD_CHAR_ALIGN );

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

SInt mtdCharLogicalAscComp( mtdValueInfo * aValueInfo1,
                            mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharLogicalDescComp( mtdValueInfo * aValueInfo1,
                             mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                              mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)
        mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                             aValueInfo1->value,
                             aValueInfo1->flag,
                             mtdChar.staticNull );

    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                             aValueInfo2->value,
                             aValueInfo2->flag,
                             mtdChar.staticNull );

    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                               mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)
        mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                             aValueInfo1->value,
                             aValueInfo1->flag,
                             mtdChar.staticNull );

    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                             aValueInfo2->value,
                             aValueInfo2->flag,
                             mtdChar.staticNull );

    sLength2    = sCharValue2->length;

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                 mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;
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

        sCharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength1 = sCharValue1->length;
        sValue1  = sCharValue1->value;
    }

    //---------
    // value2
    //---------    
    sCharValue2 = (const mtdCharType*)
                   mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                        aValueInfo2->value,
                                        aValueInfo2->flag,
                                        mtdChar.staticNull );
    sLength2 = sCharValue2->length;
    sValue2  = sCharValue2->value;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                  mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;
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

        sCharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength1 = sCharValue1->length;
        sValue1  = sCharValue1->value;
    }

    //---------
    // value2
    //---------    
    sCharValue2 = (const mtdCharType*)
                   mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                        aValueInfo2->value,
                                        aValueInfo2->flag,
                                        mtdChar.staticNull );
    sLength2 = sCharValue2->length;
    sValue2  = sCharValue2->value;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                    mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;
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

        sCharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength1 = sCharValue1->length;
        sValue1  = sCharValue1->value;
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

        sCharValue2 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength2 = sCharValue2->length;
        sValue2  = sCharValue2->value;
    }

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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

SInt mtdCharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;
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

        sCharValue1 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength1 = sCharValue1->length;
        sValue1  = sCharValue1->value;
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

        sCharValue2 = (const mtdCharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );
        sLength2 = sCharValue2->length;
        sValue2  = sCharValue2->value;
    }

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            sExist = ID_FALSE;
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
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
SInt mtdCharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    UInt                 sDirectKeyPartialSize;
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2    = sCharValue2->length;

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
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if ( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1,
                                       sValue2,
                                       sLength2 );
            if ( sCompared != 0 )
            {
                return sCompared;
            }
            else
            {
                /* nothing to do */
            }

            sExist = ID_FALSE;
            for ( sIterator = ( sValue1 + sLength2 ), sFence = ( sValue1 + sLength1 ) ;
                  ( sIterator < sFence ) ;
                  sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1,
                                   sValue2,
                                   sLength1 );
        if ( sCompared != 0 )
        {
            return sCompared;
        }
        else
        {
            /* nothing to do */
        }
        sExist = ID_FALSE;
        for ( sIterator = ( sValue2 + sLength1 ), sFence = ( sValue2 + sLength2 ) ;
              ( sIterator < sFence ) ;
              sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing to do */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing to do */
    }


    return 0;
}

SInt mtdCharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    UInt                 sDirectKeyPartialSize;
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2    = sCharValue2->length;

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
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if ( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2,
                                       sValue1,
                                       sLength1 );
            if ( sCompared != 0 )
            {
                return sCompared;
            }
            else
            {
                /* nothing to do */
            }
            sExist = ID_FALSE;
            for ( sIterator = ( sValue2 + sLength1 ), sFence = ( sValue2 + sLength2 ) ;
                  ( sIterator < sFence ) ;
                  sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2,
                                   sValue1,
                                   sLength2 );
        if ( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for ( sIterator = ( sValue1 + sLength2 ), sFence = ( sValue1 + sLength1 ) ;
              ( sIterator < sFence ) ;
              sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing to do */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing to do */
    }
    return 0;
}

SInt mtdCharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                   mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    UInt                 sDirectKeyPartialSize;
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                            aValueInfo1->value,
                                                            aValueInfo1->flag,
                                                            mtdChar.staticNull );

    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                            aValueInfo2->value,
                                                            aValueInfo2->flag,
                                                            mtdChar.staticNull );

    sLength2    = sCharValue2->length;

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
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if ( sLength1 >= sLength2 )
        {
            sCompared = idlOS::memcmp( sValue1,
                                       sValue2,
                                       sLength2 );
            if ( sCompared != 0 )
            {
                return sCompared;
            }
            else
            {
                /* nothing to do */
            }
            sExist = ID_FALSE;
            for ( sIterator = ( sValue1 + sLength2 ), sFence = ( sValue1 + sLength1 ) ;
                  ( sIterator < sFence ) ;
                  sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue1,
                                   sValue2,
                                   sLength1 );
        if ( sCompared != 0 )
        {
            return sCompared;
        }
        sExist = ID_FALSE;
        for ( sIterator = ( sValue2 + sLength1 ), sFence = ( sValue2 + sLength2 ) ;
              ( sIterator < sFence ) ;
              sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing to do */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing to do */
    }
    return 0;
}

SInt mtdCharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                    mtdValueInfo * aValueInfo2 )
{
    const mtdCharType  * sCharValue1;
    const mtdCharType  * sCharValue2;
    UShort               sLength1;
    UShort               sLength2;
    const UChar        * sValue1;
    const UChar        * sValue2;    
    UInt                 sDirectKeyPartialSize;
    idBool               sExist;
    SInt                 sCompared;
    const UChar        * sIterator;
    const UChar        * sFence;

    //---------
    // value1
    //---------
    sCharValue1 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                            aValueInfo1->value,
                                                            aValueInfo1->flag,
                                                            mtdChar.staticNull );

    sLength1    = sCharValue1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                            aValueInfo2->value,
                                                            aValueInfo2->flag,
                                                            mtdChar.staticNull );

    sLength2    = sCharValue2->length;

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
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if ( sLength2 >= sLength1 )
        {
            sCompared = idlOS::memcmp( sValue2,
                                       sValue1,
                                       sLength1 );
            if ( sCompared != 0 )
            {
                return sCompared;
            }
            else
            {
                /* nothing to do */
            }
            sExist = ID_FALSE;
            for ( sIterator = ( sValue2 + sLength1 ), sFence = ( sValue2 + sLength2 ) ;
                  ( sIterator < sFence ) ;
                  sIterator++ )
            {
                if ( *sIterator > 0x20 )
                {
                    if ( sExist == ID_FALSE )
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if ( *sIterator < 0x20 )
                {
                    sExist = ID_TRUE;
                }
            }
            return 0;
        }
        sCompared = idlOS::memcmp( sValue2,
                                   sValue1,
                                   sLength2 );
        if ( sCompared != 0 )
        {
            return sCompared;
        }
        else
        {
            /* nothing to do */
        }
        sExist = ID_FALSE;
        for ( sIterator = ( sValue1 + sLength2 ), sFence = ( sValue1 + sLength1 ) ;
              ( sIterator < sFence ) ;
              sIterator++ )
        {
            if ( *sIterator > 0x20 )
            {
                if ( sExist == ID_FALSE )
                {
                    return -1;
                }
                else
                {
                    return 1;
                }
            }
            else if ( *sIterator < 0x20 )
            {
                sExist = ID_TRUE;
            }
        }
        return 0;
    }

    if ( sLength1 < sLength2 )
    {
        return 1;
    }
    else
    {
        /* nothing to do */
    }
    if ( sLength1 > sLength2 )
    {
        return -1;
    }
    else
    {
        /* nothing to do */
    }
    return 0;
}

static IDE_RC mtdCanonize( const mtcColumn * aCanon,
                           void**            aCanonized,
                           mtcEncryptInfo  * /* aCanonInfo */,
                           const mtcColumn * /* aColumn */,
                           void*             aValue,
                           mtcEncryptInfo  * /* aColumnInfo */,
                           mtcTemplate     * /* aTemplate */ )
{
    mtdCharType* sCanonized;
    mtdCharType* sValue;

    sValue = (mtdCharType*)aValue;

    if( sValue->length == aCanon->precision || sValue->length == 0 )
    {
        *aCanonized = aValue;
    }
    else
    {
        IDE_TEST_RAISE( sValue->length > aCanon->precision,
                        ERR_INVALID_LENGTH );
        
        sCanonized = (mtdCharType*)*aCanonized;
        
        sCanonized->length = aCanon->precision;
        
        idlOS::memcpy( sCanonized->value, sValue->value, sValue->length );
        
        idlOS::memset( sCanonized->value + sValue->length, 0x20,
                       sCanonized->length - sValue->length );
    }

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
                                     & mtdChar,
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


SDouble mtdSelectivityChar( void     * aColumnMax,
                            void     * aColumnMin,
                            void     * aValueMax,
                            void     * aValueMin,
                            SDouble    aBoundFactor,
                            SDouble    aTotalRecordCnt )
{
/*----------------------------------------------------------------------
  Name:
  mtdSelectivityChar()
  -- �ִ�, �ּ� ���� �̿��Ͽ� ���� ���� ���� ���õ��� ����,
  -- CHAR(n),VARCHAR(n)

  Arguments:
  aColumnMax  -- Į���� �ִ� �� (MAX)
  aColumnMin  -- Į���� �ּ� �� (MIN)
  aValueMax   -- ���� �ּ� ��   (Y)
  aValueMin   -- ���� �ִ� ��   (X)

  Description: �ִ�, �ּҰ��� �̿��Ͽ� ���� ���� ���� ���õ��� �����Ѵ�.
  <, >, <=, >=, BETWEEN, NOT BETWEEN Predicate�� �ش�Ǹ�,
  LIKE, NOT LIKE�� ��쿡�� prefix match�� ��� �̿� �ش��Ѵ�.

  ��: i1 between X and Y
  ==> selectivity = ( Y - X ) / ( MAX - MIN )

  ���õ��� ����ϴ� �������� ���ڿ��� DOUBLE������ ��ȯ�ؾ� �Ѵ�.
  �� ��, 4���� ���ڰ� ��� 10������ �ǴܵǸ� 10������
  16������ �ǴܵǸ� 16������ ��ȯ�Ѵ�.
  �׷��� ���� ��� ���ڿ� 52��Ʈ�� �����ϴ� ���� ��ȯ�Ѵ�.

  *----------------------------------------------------------------------*/

    // ������ ���� ���� mtdCharType
    const mtdCharType *    sColumnMax;
    const mtdCharType *    sColumnMin;
    const mtdCharType *    sValueMax;
    const mtdCharType *    sValueMin;

    // ������ ���� ���� SDouble ����
    SDouble          sColMaxDouble;
    SDouble          sColMinDouble;
    SDouble          sValMaxDouble;
    SDouble          sValMinDouble;

    // Selectivity
    SDouble          sSelectivity;

    // 10����, 16����, �Ϲݹ��ڿ� ���θ� ǥ���ϴ� ����
    vSLong           sStringType;

    // ���� �ʱ�ȭ
    sStringType = 0;
    sColumnMax = (mtdCharType*) aColumnMax;
    sColumnMin = (mtdCharType*) aColumnMin;
    sValueMax  = (mtdCharType*) aValueMax;
    sValueMin  = (mtdCharType*) aValueMin;

    //------------------------------------------------------
    // Data�� ��ȿ�� �˻�
    //     NULL �˻� : ����� �� ����
    //------------------------------------------------------
    
    // BUG-22064
    IDE_DASSERT( aColumnMax == idlOS::align( aColumnMax, MTD_CHAR_ALIGN ) );
    IDE_DASSERT( aColumnMin == idlOS::align( aColumnMin, MTD_CHAR_ALIGN ) );
    IDE_DASSERT( aValueMax == idlOS::align( aValueMax, MTD_CHAR_ALIGN ) );
    IDE_DASSERT( aValueMin == idlOS::align( aValueMin, MTD_CHAR_ALIGN ) );
    
    if ( ( mtdIsNull( NULL, aColumnMax ) == ID_TRUE ) ||
         ( mtdIsNull( NULL, aColumnMin ) == ID_TRUE ) ||
         ( mtdIsNull( NULL, aValueMax  ) == ID_TRUE ) ||
         ( mtdIsNull( NULL, aValueMin  ) == ID_TRUE ) )
    {
        // Data�� NULL �� ���� ���
        // �ε�ȣ�� Default Selectivity�� 1/3�� Setting��
        sSelectivity = MTD_DEFAULT_SELECTIVITY;
    }
    else
    {
        //------------------------------------------------------------
        // ���ڸ� �ǹ��ϴ� ���ڿ����� �Ǵ�
        //  sStringType�� ������ �÷��� ����
        //------------------------------------------------------------
        sStringType |= mtdStringType(sColumnMax);
        sStringType |= mtdStringType(sColumnMin);
        sStringType |= mtdStringType(sValueMax);
        sStringType |= mtdStringType(sValueMin);

        //---------------------------------------------------
        // 10������ �Ǵ�
        //   ��� �÷��׷� �����Ǿ� ���� �ʾƾ� �Ѵ�.
        // 10������ ��ȯ
        //   mtdDigitsToDouble�Լ��� �̿��Ͽ� 10������ ��ȯ�Ѵ�.
        //---------------------------------------------------
        if( sStringType == MTD_DECIMAL )
        {
            sColMaxDouble = mtdDigitsToDouble( sColumnMax, 10 );
            sColMinDouble = mtdDigitsToDouble( sColumnMin, 10 );
            sValMaxDouble = mtdDigitsToDouble( sValueMax, 10 );
            sValMinDouble = mtdDigitsToDouble( sValueMin, 10 );
        }
        //---------------------------------------------------
        // 16������ �Ǵ�
        //   MTD_HEXA_LOWER �̰ų� MTD_HEXA_UPPER �� ���
        //   �� ���� �� �÷��� ���� ��ġ�ؾ��ϸ�,
        //   �� �÷��װ� �Բ� ������ ���� �Ϲ� ���ڿ��� �Ǵ�
        // 16������ ��ȯ
        //   mtdDigitsToDouble�Լ��� �̿��Ͽ� 16������ ��ȯ�Ѵ�.
        //---------------------------------------------------
        else if( ( sStringType == MTD_HEXA_LOWER ) ||
                 ( sStringType == MTD_HEXA_UPPER ) )
        {
            sColMaxDouble = mtdDigitsToDouble( sColumnMax, 16 );
            sColMinDouble = mtdDigitsToDouble( sColumnMin, 16 );
            sValMaxDouble = mtdDigitsToDouble( sValueMax, 16 );
            sValMinDouble = mtdDigitsToDouble( sValueMin, 16 );
        }
        //------------------------------------------------------------
        // �Ϲ� ���ڿ��� ��� ��ȯ
        //------------------------------------------------------------
        else
        {
            sColMaxDouble = mtdConvertToDouble( sColumnMax );
            sColMinDouble = mtdConvertToDouble( sColumnMin );
            sValMaxDouble = mtdConvertToDouble( sValueMax );
            sValMinDouble = mtdConvertToDouble( sValueMin );
        }

        //---------------------------------------------------------
        // selectivity ���
        //--------------------------------------------------------
        sSelectivity = mtdDouble.selectivity( (void *)&sColMaxDouble,
                                              (void *)&sColMinDouble,
                                              (void *)&sValMaxDouble,
                                              (void *)&sValMinDouble,
                                              aBoundFactor,
                                              aTotalRecordCnt );
    }

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return sSelectivity;

}


vSLong mtdStringType( const mtdCharType * aValue )
{
/*----------------------------------------------------------------------
  Name:
  mtdStringType()
  -- �ش� ���ڿ��� Ÿ���� �Ǵ��Ѵ�.
  10����     : MTD_DECIMAL
  16����     : MTD_HEXA_LOWER, MTD_HEXA_UPPER
  �Ϲ� ���ڿ� : MTD_ORDINARY

  Arguments:
  aValue  -- Ÿ���� �Ǵ��� ���ڿ�

  *----------------------------------------------------------------------*/
    vSLong sLength;
    vSLong sIdx;
    vSLong sStringType;

    sStringType = 0;
    sLength     = (vSLong)(aValue->length);

    for( sIdx = 0; sIdx < sLength; sIdx++ )
    {
        if( (aValue->value[sIdx] >= '0') &&
            (aValue->value[sIdx] <= '9') )
        {
            /* nothing to do...*/
        }
        else if ( (aValue->value[sIdx] >= 'a') &&
                  (aValue->value[sIdx] <= 'f')  )
        {
            sStringType |= MTD_HEXA_LOWER;
        }
        else if( (aValue->value[sIdx] >= 'A') &&
                 (aValue->value[sIdx] <= 'F') )
        {
            sStringType |= MTD_HEXA_UPPER;
        }
        else
        {
            sStringType |= MTD_ORDINARY;
            break;
        }
    }
    return sStringType;

}

SDouble mtdDigitsToDouble( const mtdCharType * aValue, UInt aBase )
{
/*----------------------------------------------------------------------
  Name:
  BUG-16401
  mtdDigitsToDouble()
  -- �ش� ���ڿ��� 15�ڸ� ���ڿ��� ������
  -- SDboubleŸ������ ��ȯ�Ѵ�.

  Arguments:
  aValue  -- ��ȯ�� ���ڿ�
  aBase   -- ����

  *----------------------------------------------------------------------*/

    SDouble  sDoubleVal;
    ULong    sLongVal;
    UInt     sDigit;
    UInt     i;

    static const UChar sHex[256] = {
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };

    sLongVal = 0;

    if ( (aValue->length > 0) && (aBase >= 2) && (aBase <= 16) )
    {
        if ( aValue->length < MTD_CHAR_DIGIT_MAX )
        {
            for ( i = 0; i < aValue->length; i++ )
            {
                sDigit = sHex[ aValue->value[i] ];

                if ( sDigit >= aBase )
                {
                    sLongVal = 0;
                    break;
                }
                else
                {
                    sLongVal = sLongVal * aBase + sDigit;
                }
            }

            if ( sLongVal != 0 )
            {
                for ( ; i < MTD_CHAR_DIGIT_MAX; i++ )
                {
                    sLongVal = sLongVal * aBase;
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            for ( i = 0; i < MTD_CHAR_DIGIT_MAX; i++ )
            {
                sDigit = sHex[ aValue->value[i] ];

                if ( sDigit >= aBase )
                {
                    sLongVal = 0;
                    break;
                }
                else
                {
                    sLongVal = sLongVal * aBase + sDigit;
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    sDoubleVal = ID_ULTODB( sLongVal );

    return sDoubleVal;
}

SDouble mtdConvertToDouble( const mtdCharType * aValue )
{
/*----------------------------------------------------------------------
  Name:
  mtdConvertToDouble()
  -- �ش� ���ڿ��� SDboubleŸ������ ��ȯ�Ѵ�.

  Arguments:
  aValue  -- ��ȯ�� ���ڿ�

  *----------------------------------------------------------------------*/

#if defined(ENDIAN_IS_BIG_ENDIAN)
    //----------------------------------------------------
    // Big endian �� ���
    //----------------------------------------------------
    SDouble    sDoubleVal;       // ��ȯ�� Double ��
    ULong      sLongVal;         // ULong���� ��ȯ �� ����� ����

    sLongVal = 0;                // �ʱ�ȭ

    // mtdCharType�� ULong���� ��ȯ
    idlOS::memcpy( (UChar*)&sLongVal,
                   aValue->value,
                   MTD_MIN( aValue->length, ID_SIZEOF(sLongVal) ) );

    // �պκ� 52��Ʈ�� ����� ��ȯ
    sLongVal   = sLongVal >> 12;
    sDoubleVal = ID_ULTODB( sLongVal );

    return sDoubleVal;

#else
    //----------------------------------------------------
    // Little endian �� ���
    //----------------------------------------------------
    SDouble    sDoubleVal;       // ��ȯ�� Double ��
    ULong      sLongVal;         // ULong���� ��ȯ �� ����� ����
    ULong      sEndian;          // byte ordering ��ȯ�� ����� �ӽ� ����
    UChar    * sSrc;             // byte ordering ��ȯ�� ���� ������
    UChar    * sDest;            // byte ordering ��ȯ�� ���� ������

    sLongVal = 0;                // �ʱ�ȭ

    // mtdCharType�� ULong���� ��ȯ
    idlOS::memcpy( (UChar*)&sLongVal,
                   aValue->value,
                   MTD_MIN( aValue->length, ID_SIZEOF(sLongVal) ) );

    // byte ordering ����
    sSrc     = (UChar*)&sLongVal;
    sDest    = (UChar*)&sEndian;
    sDest[0] = sSrc[7];
    sDest[1] = sSrc[6];
    sDest[2] = sSrc[5];
    sDest[3] = sSrc[4];
    sDest[4] = sSrc[3];
    sDest[5] = sSrc[2];
    sDest[6] = sSrc[1];
    sDest[7] = sSrc[0];

    // �պκ� 52��Ʈ�� ����� ��ȯ
    sEndian    = sEndian >> 12;
    sDoubleVal = ID_ULTODB( sEndian );

    return sDoubleVal;

#endif

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

    sValueOffset = idlOS::align( *aValueOffset, MTD_CHAR_ALIGN );

    if( aOracleLength < 0 )
    {
        aOracleLength = 0;
    }

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdChar,
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

    mtdCharType* sCharValue;

    sCharValue = (mtdCharType*)aDestValue;
    
    if( ( aDestValueOffset == 0 ) && ( aLength == 0 ) )
    {
        // NULL ����Ÿ
        sCharValue->length = 0;
    }
    else
    {
        IDE_TEST_RAISE( (aDestValueOffset + aLength + mtdHeaderSize()) > aColumnSize, ERR_INVALID_STORED_VALUE );

        sCharValue->length = (UShort)(aDestValueOffset + aLength);
        idlOS::memcpy( (UChar*)sCharValue + mtdHeaderSize() + aDestValueOffset, aValue, aLength );
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
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
 *******************************************************************/
    return mtdActualSize( NULL, &mtdCharNull );
}

static UInt mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdCharType( UShort length; UChar value[1] ) ����
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/

    return ID_SIZEOF(UShort);
}

static UInt mtdStoreSize( const smiColumn * aColumn ) 
{
/***********************************************************************
 * PROJ-2399 row tmaplate 
 * sm�� ����Ǵ� �������� ũ�⸦ ��ȯ�Ѵ�.
 * variable Ÿ���� ������ Ÿ���� ID_UINT_MAX�� ��ȯ
 * mtheader�� sm�� ����Ȱ�찡 �ƴϸ� mtheaderũ�⸦ ���� ��ȯ
 **********************************************************************/

    return aColumn->size - mtdHeaderSize();
}
