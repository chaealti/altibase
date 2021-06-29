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
 * $Id: mtdVarchar.cpp 21249 2007-04-06 01:39:26Z leekmo $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>
#include <mtuProperty.h>

extern mtdModule mtdNvarchar;
extern mtdModule mtdNchar;

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

static SInt mtdNvarcharLogicalAscComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharLogicalDescComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                    mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                          mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                               mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                                mtdValueInfo * aValueInfo2 );

/* PROJ-2433 */
static SInt mtdNvarcharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                                    mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 );

static SInt mtdNvarcharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
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
                                       void            * aDestVale,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue );

static UInt mtdNullValueSize();

static UInt mtdHeaderSize();

static UInt mtdStoreSize( const smiColumn * aColumn );

static mtcName mtdTypeName[1] = {
    { NULL, 8, (void*)"NVARCHAR"  }
};

static mtcColumn mtdColumn;

mtdModule mtdNvarchar = {
    mtdTypeName,
    &mtdColumn,
    MTD_NVARCHAR_ID,
    0,
    { SMI_BUILTIN_B_TREE_INDEXTYPE_ID,
      SMI_BUILTIN_B_TREE2_INDEXTYPE_ID,
      0, 0, 0, 0, 0 },
    MTD_NCHAR_ALIGN,
    MTD_GROUP_TEXT|
      MTD_CANON_NEED|
      MTD_CREATE_ENABLE|
      MTD_COLUMN_TYPE_VARIABLE|
      MTD_SELECTIVITY_DISABLE|
      MTD_CREATE_PARAM_PRECISION|
      MTD_CASE_SENS_TRUE|
      MTD_SEARCHABLE_SEARCHABLE|
      MTD_LITERAL_TRUE|
      MTD_VARIABLE_LENGTH_TYPE_TRUE|  // PROJ-1705
      MTD_DATA_STORE_DIVISIBLE_TRUE|  // PROJ-1705
      MTD_DATA_STORE_MTDVALUE_FALSE|  // PROJ-1705
      MTD_PSM_TYPE_ENABLE, // PROJ-1904
    0, // mtd::modifyNls4MtdModule�ÿ� ������
    0,
    0,
    (void*)&mtdNcharNull,
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
        mtdNvarcharLogicalAscComp,    // Logical Comparison
        mtdNvarcharLogicalDescComp
    },
    {
        // Key Comparison
        {
            // mt value�� ���� compare
            mtdNvarcharFixedMtdFixedMtdKeyAscComp, // Ascending Key Comparison
            mtdNvarcharFixedMtdFixedMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� ���� compare
            mtdNvarcharMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdNvarcharMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare
            mtdNvarcharStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdNvarcharStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare
            mtdNvarcharStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdNvarcharStoredStoredKeyDescComp // Descending Key Comparison
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� fixed mt value�� ���� compare */
            mtdNvarcharIndexKeyFixedMtdKeyAscComp,
            mtdNvarcharIndexKeyFixedMtdKeyDescComp
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� mt value�� ���� compare */
            mtdNvarcharIndexKeyMtdKeyAscComp,
            mtdNvarcharIndexKeyMtdKeyDescComp
        }
    },
    mtdCanonize,
    mtdEndian,
    mtdValidate,
    mtd::selectivityDefault,
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
    IDE_TEST( mtd::initializeModule( &mtdNvarchar, aNo )
              != IDE_SUCCESS );
    
    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdNvarchar,
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
        *aPrecision = MTD_NVARCHAR_PRECISION_DEFAULT;
    }

    IDE_TEST_RAISE( *aArguments != 1, ERR_INVALID_SCALE );

    if( mtl::mNationalCharSet->id == MTL_UTF8_ID )
    {
        IDE_TEST_RAISE( (*aPrecision < (SInt)MTD_NCHAR_PRECISION_MINIMUM) ||
                        (*aPrecision > (SInt)MTD_UTF8_NCHAR_PRECISION_MAXIMUM),
                        ERR_INVALID_LENGTH );

        *aColumnSize = ID_SIZEOF(UShort) + (*aPrecision * MTL_UTF8_PRECISION);
    }
    else if( mtl::mNationalCharSet->id == MTL_UTF16_ID )
    {
        IDE_TEST_RAISE( (*aPrecision < (SInt)MTD_NCHAR_PRECISION_MINIMUM) ||
                        (*aPrecision > (SInt)MTD_UTF16_NCHAR_PRECISION_MAXIMUM),
                        ERR_INVALID_LENGTH );

        *aColumnSize = ID_SIZEOF(UShort) + (*aPrecision * MTL_UTF16_PRECISION);
    }
    else
    {
        ideLog::log( IDE_ERR_0,
                     "mtl::mNationalCharSet->id : %u\n",
                     mtl::mNationalCharSet->id );

        IDE_ASSERT( 0 );
    }
    
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
    UInt                sValueOffset;
    mtdNcharType      * sValue;
    UChar             * sToken;
    UChar             * sTokenFence;
    UChar             * sIterator;
    UChar             * sTempToken;
    UChar             * sFence;
    const mtlModule   * sColLanguage;
    const mtlModule   * sTokenLanguage;
    idBool              sIsSame = ID_FALSE;
    UInt                sNcharCnt = 0;
    idnCharSetList      sColCharSet;
    idnCharSetList      sTokenCharSet;
    SInt                sSrcRemain;
    SInt                sDestRemain;
    SInt                sTempRemain;
    UChar               sSize;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_NCHAR_ALIGN );

    sValue = (mtdNcharType*)( (UChar*)aValue + sValueOffset );

    *aResult = IDE_SUCCESS;

    sColLanguage = aColumn->language;

    // NCHAR ���ͷ��� ����ߴٸ� NVARCHAR Ÿ�����δ� �νĵ��� �ʴ´�.
    // ������ DB ĳ���� ���̴�.
    sTokenLanguage = mtl::mDBCharSet;

    // To fix BUG-13444
    // tokenFence�� RowFence�� ������ �˻�����̹Ƿ�,
    // ���� RowFence�˻� �� TokenFence�˻縦 �ؾ� �Ѵ�.
    sIterator  = sValue->value;
    sSrcRemain = sTokenLanguage->maxPrecision( aTokenLength );
    sFence     = (UChar*)aValue + aValueSize;

    if( sIterator >= sFence )
    {
        *aResult = IDE_FAILURE;
    }
    else
    {    
        sColCharSet = mtl::getIdnCharSet( sColLanguage );
        sTokenCharSet = mtl::getIdnCharSet( sTokenLanguage );

        sDestRemain = aTokenLength * MTL_MAX_PRECISION;

        for( sToken      = (UChar *)aToken,
             sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
           )
        {
            if( sIterator >= sFence )
            {
                *aResult = IDE_FAILURE;
                break;
            }
            
            sSize = mtl::getOneCharSize( sToken,
                                         sTokenFence,
                                         sTokenLanguage );
            
            sIsSame = mtc::compareOneChar( sToken,
                                           sSize,
                                           sTokenLanguage->specialCharSet[MTL_QT_IDX],
                                           sTokenLanguage->specialCharSize );

            if( sIsSame == ID_TRUE )
            {
                (void)sTokenLanguage->nextCharPtr( & sToken, sTokenFence );
                
                sSize = mtl::getOneCharSize( sToken,
                                             sTokenFence,
                                             sTokenLanguage );
                
                sIsSame = mtc::compareOneChar( sToken,
                                               sSize,
                                               sTokenLanguage->specialCharSet[MTL_QT_IDX],
                                               sTokenLanguage->specialCharSize );
                
                IDE_TEST_RAISE( sIsSame != ID_TRUE,
                                ERR_INVALID_LITERAL );
            }

            sTempRemain = sDestRemain;
            
            if( sColLanguage->id != sTokenLanguage->id )
            {    
                IDE_TEST( convertCharSet( sTokenCharSet,
                                          sColCharSet,
                                          sToken,
                                          sSrcRemain,
                                          sIterator,
                                          & sDestRemain,
                                          MTU_NLS_NCHAR_CONV_EXCP )
                          != IDE_SUCCESS );

                sTempToken = sToken;

                (void)sTokenLanguage->nextCharPtr( & sToken, sTokenFence );
            }
            else
            {
                sTempToken = sToken;

                (void)sTokenLanguage->nextCharPtr( & sToken, sTokenFence );
                
                idlOS::memcpy( sIterator, sTempToken, sToken - sTempToken );

                sDestRemain -= (sToken - sTempToken);
            }

            if( sTempRemain - sDestRemain > 0 )
            {
                sIterator += (sTempRemain - sDestRemain);
                sNcharCnt++;
            }
            else
            {
                // Nothing to do.
            }

            sSrcRemain -= ( sToken - sTempToken );
        }
    }

    if( *aResult == IDE_SUCCESS )
    {
        sValue->length           = sIterator - sValue->value;

        // precision, scale �� ���� ��, estimate�� semantic �˻�
        aColumn->flag            = 1;

        //aColumn->precision   = sValue->length != 0 ?
        //        (sNcharCnt * MTL_NCHAR_PRECISION(mtl::mNationalCharSet)) : 1;

        aColumn->precision = sValue->length != 0 ? sNcharCnt : 1;

        aColumn->scale           = 0;

        IDE_TEST( mtdEstimate( & aColumn->column.size,
                               & aColumn->flag,
                               & aColumn->precision,
                               & aColumn->scale )
                  != IDE_SUCCESS );
        
        aColumn->column.offset   = sValueOffset;

        *aValueOffset = sValueOffset + ID_SIZEOF(UShort) +
                        sValue->length;
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
    return ID_SIZEOF(UShort) + ((mtdNcharType*)aRow)->length;
}

static IDE_RC mtdGetPrecision( const mtcColumn * aColumn,
                               const void      * aRow,
                               SInt            * aPrecision,
                               SInt            * aScale )
{
    const mtlModule    * sLanguage;
    const mtdNcharType * sValue = ((mtdNcharType*)aRow);
    UChar              * sValueIndex;
    UChar              * sValueFence;
    UShort               sValueCharCnt = 0;

    sLanguage = aColumn->language;

    // --------------------------
    // Value�� ���� ����
    // --------------------------
    
    sValueIndex = (UChar*) sValue->value;
    sValueFence = sValueIndex + sValue->length;

    while ( sValueIndex < sValueFence )
    {
        IDE_TEST_RAISE( sLanguage->nextCharPtr( & sValueIndex, sValueFence )
                        != NC_VALID,
                        ERR_INVALID_CHARACTER );

        sValueCharCnt++;
    }
    
    *aPrecision = sValueCharCnt;
    *aScale = 0;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_CHARACTER );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_CHARACTER));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mtdSetNull( const mtcColumn* /*aColumn*/,
                 void*            aRow )
{
    if( aRow != NULL )
    {
        ((mtdNcharType*)aRow)->length = 0;
    }
}

UInt mtdHash( UInt             aHash,
              const mtcColumn* ,
              const void*      aRow )
{
    return mtc::hashWithoutSpace( aHash, ((mtdNcharType*)aRow)->value, ((mtdNcharType*)aRow)->length );
}

idBool mtdIsNull( const mtcColumn* ,
                  const void*      aRow )
{
    return (((mtdNcharType*)aRow)->length == 0) ? ID_TRUE : ID_FALSE;
}

SInt mtdNvarcharLogicalAscComp( mtdValueInfo * aValueInfo1,
                                mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength1 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharLogicalDescComp( mtdValueInfo * aValueInfo1,
                                 mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_OFFSET_USELESS( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength2 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharFixedMtdFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength1 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharFixedMtdFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength2 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharMtdMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                  mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------    
    sNvarcharValue1 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                            aValueInfo1->value,
                                            aValueInfo1->flag,
                                            mtdNvarchar.staticNull );

    sLength1 = sNvarcharValue1->length;
    sValue1  = sNvarcharValue1->value;

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                            aValueInfo2->value,
                                            aValueInfo2->flag,
                                            mtdNvarchar.staticNull );

    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( ( sLength1 != 0 ) && ( sLength2 != 0 ) )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength1 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                   mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;

    //---------
    // value1
    //---------    
    sNvarcharValue1 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                            aValueInfo1->value,
                                            aValueInfo1->flag,
                                            mtdNvarchar.staticNull );

    sLength1 = sNvarcharValue1->length;
    sValue1  = sNvarcharValue1->value;

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                            aValueInfo2->value,
                                            aValueInfo2->flag,
                                            mtdNvarchar.staticNull );

    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength2 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharStoredMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
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

        sNvarcharValue1 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sNvarcharValue1->length;
        sValue1  = sNvarcharValue1->value;
    }

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                            aValueInfo2->value,
                                            aValueInfo2->flag,
                                            mtdNvarchar.staticNull );

    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

    //---------
    // compare
    //---------    

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength1 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharStoredMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
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

        sNvarcharValue1 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sNvarcharValue1->length;
        sValue1  = sNvarcharValue1->value;
    }

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*)
                       mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                            aValueInfo2->value,
                                            aValueInfo2->flag,
                                            mtdNvarchar.staticNull );
    
    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength2 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharStoredStoredKeyAscComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
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

        sNvarcharValue1 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sNvarcharValue1->length;
        sValue1  = sNvarcharValue1->value;
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

        sNvarcharValue2 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength2 = sNvarcharValue2->length;
        sValue2  = sNvarcharValue2->value;
    }

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength1 > sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength2 ) >= 0 ? 1 : -1 ;
        }
        if( sLength1 < sLength2 )
        {
            return idlOS::memcmp( sValue1, sValue2, sLength1 ) > 0 ? 1 : -1 ;
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

SInt mtdNvarcharStoredStoredKeyDescComp( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
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

        sNvarcharValue1 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo1->value,
                                                 (smiColumn*)aValueInfo1->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength1 = sNvarcharValue1->length;
        sValue1  = sNvarcharValue1->value;
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

        sNvarcharValue2 = (const mtdNcharType*)
                      mtc::getCompressionColumn( aValueInfo2->value,
                                                 (smiColumn*)aValueInfo2->column,
                                                 ID_FALSE, // aUseColumnOffset
                                                 &sDummy );

        sLength2 = sNvarcharValue2->length;
        sValue2  = sNvarcharValue2->value;
    }

    //---------
    // compare
    //---------

    if( sLength1 != 0 && sLength2 != 0 )
    {
        if( sLength2 > sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength1 ) >= 0 ? 1 : -1 ;
        }
        if( sLength2 < sLength1 )
        {
            return idlOS::memcmp( sValue2, sValue1, sLength2 ) > 0 ? 1 : -1 ;
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
SInt mtdNvarcharIndexKeyFixedMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * aValueInfo2 )
{
    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

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
        if ( sLength1 > sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength2 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        if ( sLength1 < sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength1 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        return ( idlOS::memcmp( sValue1,
                                sValue2,
                                sLength1 ) ) ;
    }
    else
    {
        /* nothing to do */
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

SInt mtdNvarcharIndexKeyFixedMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 )
{
    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------
    sNvarcharValue1 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo1 );
    sLength1        = sNvarcharValue1->length;
    sValue1         = sNvarcharValue1->value;
    //---------
    // value2
    //---------
    sNvarcharValue2 = (const mtdNcharType*)MTD_VALUE_FIXED( aValueInfo2 );
    sLength2        = sNvarcharValue2->length;
    sValue2         = sNvarcharValue2->value;

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
        if ( sLength2 > sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength1 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        if ( sLength2 < sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength2 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        return ( idlOS::memcmp( sValue2,
                                sValue1,
                                sLength2 ) ) ;
    }
    else
    {
        /* nothing to do */
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

SInt mtdNvarcharIndexKeyMtdKeyAscComp( mtdValueInfo * aValueInfo1,
                                       mtdValueInfo * aValueInfo2 )
{
    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------    
    sNvarcharValue1 = (const mtdNcharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                                 aValueInfo1->value,
                                                                 aValueInfo1->flag,
                                                                 mtdNvarchar.staticNull );

    sLength1 = sNvarcharValue1->length;
    sValue1  = sNvarcharValue1->value;

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                                 aValueInfo2->value,
                                                                 aValueInfo2->flag,
                                                                 mtdNvarchar.staticNull );

    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

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
        if ( sLength1 > sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength2 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        if ( sLength1 < sLength2 )
        {
            return ( ( idlOS::memcmp( sValue1,
                                      sValue2,
                                      sLength1 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }
        return ( idlOS::memcmp( sValue1, sValue2, sLength1 ) ) ;
    }
    else
    {
        /* nothing to do */
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

SInt mtdNvarcharIndexKeyMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 )
{
    const mtdNcharType * sNvarcharValue1;
    const mtdNcharType * sNvarcharValue2;
    UShort               sLength1 = 0;
    UShort               sLength2 = 0;
    const UChar        * sValue1;
    const UChar        * sValue2;
    UInt                 sDirectKeyPartialSize;

    //---------
    // value1
    //---------    
    sNvarcharValue1 = (const mtdNcharType*) mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                                                 aValueInfo1->value,
                                                                 aValueInfo1->flag,
                                                                 mtdNvarchar.staticNull );

    sLength1 = sNvarcharValue1->length;
    sValue1  = sNvarcharValue1->value;

    //---------
    // value2
    //---------    
    sNvarcharValue2 = (const mtdNcharType*) mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                                                                 aValueInfo2->value,
                                                                 aValueInfo2->flag,
                                                                 mtdNvarchar.staticNull );

    sLength2 = sNvarcharValue2->length;
    sValue2  = sNvarcharValue2->value;

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
        if ( sLength2 > sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength1 ) >= 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }

        if ( sLength2 < sLength1 )
        {
            return ( ( idlOS::memcmp( sValue2,
                                      sValue1,
                                      sLength2 ) > 0 ) ? 1 : -1 ) ;
        }
        else
        {
            /* nothing to do */
        }

        return ( idlOS::memcmp( sValue2,
                                sValue1,
                                sLength2 ) ) ;
    }
    else
    {
        /* nothing to do */
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
                           void**            aCanonizedValue,
                           mtcEncryptInfo  * /* aCanonInfo */,
                           const mtcColumn * /* aColumn */,
                           void*             aValue,
                           mtcEncryptInfo  * /* aColumnInfo */,
                           mtcTemplate     * /* aTemplate */ )
{
    const mtlModule * sLanguage;
    mtdNcharType    * sValue;
    UChar           * sValueIndex;
    UChar           * sValueFence;
    UShort            sValueCharCnt = 0;

    sValue = (mtdNcharType*)aValue;

    sLanguage = aCanon->language;

    // --------------------------
    // Value�� ���� ����
    // --------------------------
    sValueIndex = sValue->value;
    sValueFence = sValueIndex + sValue->length;

    while( sValueIndex < sValueFence )
    {
        (void)sLanguage->nextCharPtr( & sValueIndex, sValueFence );
        
        sValueCharCnt++;
    }

    IDE_TEST_RAISE( sValueCharCnt > (UInt)aCanon->precision,
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

    sLength = (UChar*)&(((mtdNcharType*)aValue)->length);

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
        
    mtdNcharType * sCharVal = (mtdNcharType*)aValue;
    IDE_TEST_RAISE( sCharVal == NULL, ERR_INVALID_NULL );

    IDE_TEST_RAISE( aValueSize < ID_SIZEOF(UShort), ERR_INVALID_LENGTH );

    IDE_TEST_RAISE( sCharVal->length + ID_SIZEOF(UShort) != aValueSize,
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdNvarchar,
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
    UInt           sValueOffset;
    mtdNcharType * sValue;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_NCHAR_ALIGN );
    
    if( aOracleLength < 0 )
    {
        aOracleLength = 0;
    }

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdNvarchar,
                                     1,
                                     ( aOracleLength > 1 ) ? aOracleLength : 1,
                                     0 )
        != IDE_SUCCESS );
    
    if( sValueOffset + aColumn->column.size <= aValueSize )
    {
        sValue = (mtdNcharType*)((UChar*)aValue+sValueOffset);
        
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
                                       void            * aDestVale,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue )
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdNcharType* sNvarcharValue;

    sNvarcharValue = (mtdNcharType*)aDestVale;
    
    if( ( aDestValueOffset == 0 ) && ( aLength == 0 ) )
    {
        // NULL ����Ÿ
        sNvarcharValue->length = 0;
    }
    else
    {
        IDE_TEST_RAISE( (aDestValueOffset + aLength + mtdHeaderSize()) > aColumnSize, ERR_INVALID_STORED_VALUE );

        sNvarcharValue->length = (UShort)(aDestValueOffset + aLength);
        idlOS::memcpy( (UChar*)sNvarcharValue + mtdHeaderSize() + aDestValueOffset, aValue, aLength );
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
 * �� ) mtdNcharType ( UShort length; UChar value[1] ) ����
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
 *******************************************************************/
    return mtdActualSize( NULL, &mtdNcharNull );
}

static UInt mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdNcharType ( UShort length; UChar value[1] ) ����
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
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
