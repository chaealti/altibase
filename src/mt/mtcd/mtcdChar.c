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
 * $Id: mtcdChar.cpp 36510 2009-11-04 03:26:46Z sungminee $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>

#include <mtcdTypes.h>
#include <mtclCollate.h>



extern mtdModule mtcdChar;
extern mtdModule mtcdDouble;


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
const mtdCharType mtcdCharNull = { 0, {'\0',} };

static ACI_RC mtdInitializeChar( acp_uint32_t aNo );

static ACI_RC mtdEstimate( acp_uint32_t * aColumnSize,
                           acp_uint32_t * aArguments,
                           acp_sint32_t * aPrecision,
                           acp_sint32_t * aScale );

static ACI_RC mtdValue( mtcTemplate*  aTemplate,
                        mtcColumn*    aColumn,
                        void*         aValue,
                        acp_uint32_t* aValueOffset,
                        acp_uint32_t  aValueSize,
                        const void*   aToken,
                        acp_uint32_t  aTokenLength,
                        ACI_RC*       aResult );

static acp_uint32_t mtdActualSize( const mtcColumn* aColumn,
                                   const void*      aRow,
                                   acp_uint32_t     aFlag );

static ACI_RC mtdGetPrecision( const mtcColumn* aColumn,
                               const void*      aRow,
                               acp_uint32_t     aFlag,
                               acp_sint32_t*    aPrecision,
                               acp_sint32_t*    aScale );

static void mtdNull( const mtcColumn* aColumn,
                     void*            aRow,
                     acp_uint32_t     aFlag );

static acp_uint32_t mtdHash( acp_uint32_t     aHash,
                             const mtcColumn* aColumn,
                             const void*      aRow,
                             acp_uint32_t     aFlag );

static acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                             const void*      aRow,
                             acp_uint32_t     aFlag );

static acp_sint32_t mtdCharLogicalComp( mtdValueInfo* aValueInfo1,
                                        mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                             mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                              mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                                mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                 mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                                   mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdCharStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                                    mtdValueInfo* aValueInfo2 );

static ACI_RC mtdCanonize( const mtcColumn* aCanon,
                           void**           aCanonized,
                           mtcEncryptInfo*  aCanonInfo,
                           const mtcColumn* aColumn,
                           void*            aValue,
                           mtcEncryptInfo*  aColumnInfo,
                           mtcTemplate*     aTemplate );

static void mtdEndian( void* aValue );

static ACI_RC mtdValidate( mtcColumn*   aColumn,
                           void*        aValue,
                           acp_uint32_t aValueSize);

static acp_double_t mtdSelectivityChar( void* aColumnMax,
                                        void* aColumnMin,
                                        void* aValueMax,
                                        void* aValueMin );

static acp_slong_t mtdStringType( const mtdCharType * aValue );

static acp_double_t mtdDigitsToDouble( const mtdCharType* aValue,
                                       acp_uint32_t       aBase );

static acp_double_t mtdConvertToDouble( const mtdCharType * aValue );

static ACI_RC mtdValueFromOracle( mtcColumn*    aColumn,
                                  void*         aValue,
                                  acp_uint32_t* aValueOffset,
                                  acp_uint32_t  aValueSize,
                                  const void*   aOracleValue,
                                  acp_sint32_t  aOracleLength,
                                  ACI_RC*       aResult );

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue );

static acp_uint32_t mtdNullValueSize();

static acp_uint32_t mtdHeaderSize();

static mtcName mtdTypeName[1] = {
    { NULL, 4, (void*)"CHAR" }
};

static mtcColumn mtdColumn;

mtdModule mtcdChar = {
    mtdTypeName,
    &mtdColumn,
    MTD_CHAR_ID,
    0,
    { 0,
      0,
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
    MTD_DATA_STORE_MTDVALUE_FALSE,  // PROJ-1705
    32000,
    0,
    0,
    (void*)&mtcdCharNull,
    mtdInitializeChar,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecision,
    mtdNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrueNA,
    mtdCharLogicalComp,    // Logical Comparison
    {
        // Key Comparison
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
    },
    mtdCanonize,
    mtdEndian,
    mtdValidate,
    mtdSelectivityChar,
    mtdEncodeCharDefault,
    mtdDecodeDefault,
    mtdCompileFmtDefault,
    mtdValueFromOracle,
    mtdMakeColumnInfoDefault,

    // PROJ-1705
    mtdStoredValue2MtdValue,
    mtdNullValueSize,
    mtdHeaderSize
};

ACI_RC mtdInitializeChar( acp_uint32_t aNo )
{
    ACI_TEST( mtdInitializeModule( &mtcdChar, aNo ) != ACI_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( & mtdColumn,
                                   & mtcdChar,
                                   0,   // arguments
                                   0,   // precision
                                   0 )  // scale
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdEstimate( acp_uint32_t* aColumnSize,
                    acp_uint32_t* aArguments,
                    acp_sint32_t* aPrecision,
                    acp_sint32_t* aScale )
{
    ACP_UNUSED(aScale);
    
    if( *aArguments == 0 )
    {
        *aArguments = 1;
        *aPrecision = MTD_CHAR_PRECISION_DEFAULT;
    }

    ACI_TEST_RAISE( *aArguments != 1, ERR_INVALID_SCALE );

    ACI_TEST_RAISE( *aPrecision < MTD_CHAR_PRECISION_MINIMUM ||
                    *aPrecision > MTD_CHAR_PRECISION_MAXIMUM,
                    ERR_INVALID_LENGTH );

    *aColumnSize = sizeof(acp_uint16_t) + *aPrecision;

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

    ACI_EXCEPTION( ERR_INVALID_SCALE );
    aciSetErrorCode(mtERR_ABORT_INVALID_SCALE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdValue( mtcTemplate*  aTemplate,
                 mtcColumn*    aColumn,
                 void*         aValue,
                 acp_uint32_t* aValueOffset,
                 acp_uint32_t  aValueSize,
                 const void*   aToken,
                 acp_uint32_t  aTokenLength,
                 ACI_RC*       aResult )
{
    acp_uint32_t       sValueOffset;
    mtdCharType*       sValue;
    const acp_uint8_t* sToken;
    const acp_uint8_t* sTokenFence;
    acp_uint8_t*       sIterator;
    acp_uint8_t*       sFence;

    ACP_UNUSED(aTemplate);
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_CHAR_ALIGN );

    sValue = (mtdCharType*)( (acp_uint8_t*)aValue + sValueOffset );

    *aResult = ACI_SUCCESS;

    // To fix BUG-13444
    // tokenFence�� RowFence�� ������ �˻�����̹Ƿ�,
    // ���� RowFence�˻� �� TokenFence�˻縦 �ؾ� �Ѵ�.
    sIterator = sValue->value;
    sFence    = (acp_uint8_t*)aValue + aValueSize;
    if( sIterator >= sFence )
    {
        *aResult = ACI_FAILURE;
    }
    else
    {
        for( sToken      = (const acp_uint8_t*)aToken,
                 sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
             sIterator++, sToken++ )
        {
            if( sIterator >= sFence )
            {
                *aResult = ACI_FAILURE;
                break;
            }
            if( *sToken == '\'' )
            {
                sToken++;
                ACI_TEST_RAISE( sToken >= sTokenFence || *sToken != '\'',
                                ERR_INVALID_LITERAL );
            }
            *sIterator = *sToken;
        }
    }

    if( *aResult == ACI_SUCCESS )
    {
        sValue->length           = sIterator - sValue->value;

        // precision, scale �� ���� ��, estimate�� semantic �˻�
        aColumn->flag            = 1;
        aColumn->precision       = sValue->length != 0 ? sValue->length : 1;
        aColumn->scale           = 0;

        ACI_TEST( mtdEstimate( & aColumn->column.size,
                               & aColumn->flag,
                               & aColumn->precision,
                               & aColumn->scale )
                  != ACI_SUCCESS );
        
        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset
            + sizeof(acp_uint16_t) + sValue->length;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_uint32_t mtdActualSize( const mtcColumn* aColumn,
                            const void*      aRow,
                            acp_uint32_t     aFlag )
{
    const mtdCharType* sValue;

    ACP_UNUSED( aColumn);

    sValue = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdChar.staticNull );

    return sizeof(acp_uint16_t) + sValue->length;
}

static ACI_RC mtdGetPrecision( const mtcColumn* aColumn,
                               const void*      aRow,
                               acp_uint32_t     aFlag,
                               acp_sint32_t*    aPrecision,
                               acp_sint32_t*    aScale )
{
    const mtdCharType* sValue;

    ACP_UNUSED( aColumn);

    sValue = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdChar.staticNull );

    *aPrecision = sValue->length;
    *aScale = 0;
    
    return ACI_SUCCESS;
}

void mtdNull( const mtcColumn* aColumn,
              void*            aRow,
              acp_uint32_t     aFlag )
{
    mtdCharType* sValue;

    ACP_UNUSED( aColumn);

    sValue = (mtdCharType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           NULL );

    if( sValue != NULL )
    {
        sValue->length = 0;
    }
}

acp_uint32_t mtdHash( acp_uint32_t     aHash,
                      const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdCharType* sValue;

    ACP_UNUSED( aColumn);

    sValue = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdChar.staticNull );

    return mtcHashWithoutSpace( aHash, sValue->value, sValue->length );
}

acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdCharType* sValue;

    ACP_UNUSED( aColumn);

    sValue = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdChar.staticNull );

    return (sValue->length == 0) ? ACP_TRUE : ACP_FALSE;
}

acp_sint32_t
mtdCharLogicalComp( mtdValueInfo* aValueInfo1,
                    mtdValueInfo* aValueInfo2 )
{
    return mtdCharMtdMtdKeyAscComp( aValueInfo1,
                                    aValueInfo2 );
}

acp_sint32_t
mtdCharMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                         mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType* sCharValue1;
    const mtdCharType* sCharValue2;
    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------    
    sCharValue1 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdChar.staticNull );
    sLength1 = sCharValue1->length;

    //---------
    // value2
    //---------    
    sCharValue2 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdChar.staticNull );
    
    sLength2 = sCharValue2->length;
    
    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = acpMemCmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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

acp_sint32_t
mtdCharMtdMtdKeyDescComp( mtdValueInfo * aValueInfo1,
                          mtdValueInfo * aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType* sCharValue1;
    const mtdCharType* sCharValue2;
    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------    
    sCharValue1 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdChar.staticNull );
    sLength1 = sCharValue1->length;

    //---------
    // value2
    //---------    
    sCharValue2 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdChar.staticNull );

    sLength2 = sCharValue2->length;

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = sCharValue1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = acpMemCmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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

acp_sint32_t
mtdCharStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                            mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType* sCharValue2;
    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------
    sLength1 = (acp_uint16_t)aValueInfo1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdChar.staticNull );
    sLength2 = sCharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (acp_uint8_t*)aValueInfo1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = acpMemCmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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

acp_sint32_t
mtdCharStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                             mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdCharType* sCharValue2;
    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------
    sLength1 = (acp_uint16_t)aValueInfo1->length;

    //---------
    // value2
    //---------
    sCharValue2 = (const mtdCharType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdChar.staticNull );
    sLength2 = sCharValue2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (acp_uint8_t*)aValueInfo1->value;
        sValue2  = sCharValue2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = acpMemCmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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

acp_sint32_t
mtdCharStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                               mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------    
    sLength1 = (acp_uint16_t)aValueInfo1->length;

    //---------
    // value2
    //---------    
    sLength2 = (acp_uint16_t)aValueInfo2->length;

    //---------
    // compare
    //---------
    
    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (acp_uint8_t*)aValueInfo1->value;
        sValue2  = (acp_uint8_t*)aValueInfo2->value;
        
        if( sLength1 >= sLength2 )
        {
            sCompared = acpMemCmp( sValue1, sValue2, sLength2 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue1, sValue2, sLength1 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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

acp_sint32_t
mtdCharStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_uint16_t       sLength1;
    acp_uint16_t       sLength2;
    const acp_uint8_t* sValue1;
    const acp_uint8_t* sValue2;
    acp_sint32_t       sCompared;
    const acp_uint8_t* sIterator;
    const acp_uint8_t* sFence;

    //---------
    // value1
    //---------    
    sLength1 = (acp_uint16_t)aValueInfo1->length;

    //---------
    // value2
    //---------    
    sLength2 = (acp_uint16_t)aValueInfo2->length;

    //---------
    // compare
    //---------        

    if( sLength1 != 0 && sLength2 != 0 )
    {
        sValue1  = (acp_uint8_t*)aValueInfo1->value;
        sValue2  = (acp_uint8_t*)aValueInfo2->value;
        
        if( sLength2 >= sLength1 )
        {
            sCompared = acpMemCmp( sValue2, sValue1, sLength1 );
            if( sCompared != 0 )
            {
                return sCompared;
            }
            for( sIterator = sValue2 + sLength1, sFence = sValue2 + sLength2;
                 sIterator < sFence;
                 sIterator++ )
            {
                if( *sIterator != ' ' )
                {
                    return 1;
                }
            }
            return 0;
        }
        sCompared = acpMemCmp( sValue2, sValue1, sLength2 );
        if( sCompared != 0 )
        {
            return sCompared;
        }
        for( sIterator = sValue1 + sLength2, sFence = sValue1 + sLength1;
             sIterator < sFence;
             sIterator++ )
        {
            if( *sIterator != ' ' )
            {
                return -1;
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


static ACI_RC mtdCanonize( const mtcColumn* aCanon,
                           void**           aCanonized,
                           mtcEncryptInfo * aCanonInfo,
                           const mtcColumn* aColumn ,
                           void*            aValue,
                           mtcEncryptInfo*  aColumnInfo,
                           mtcTemplate*     aTemplate)
{
    mtdCharType* sCanonized;
    mtdCharType* sValue;

    ACP_UNUSED(aCanonInfo);
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aColumnInfo);
    ACP_UNUSED(aTemplate);
    
    sValue = (mtdCharType*)aValue;

    if( sValue->length == aCanon->precision || sValue->length == 0 )
    {
        *aCanonized = aValue;
    }
    else
    {
        ACI_TEST_RAISE( sValue->length > aCanon->precision,
                        ERR_INVALID_LENGTH );
        
        sCanonized = (mtdCharType*)*aCanonized;
        
        sCanonized->length = aCanon->precision;
        
        acpMemCpy( sCanonized->value, sValue->value, sValue->length );
        
        acpMemSet( sCanonized->value + sValue->length, ' ',
                   sCanonized->length - sValue->length );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void mtdEndian( void* aValue )
{
    acp_uint8_t* sLength;
    acp_uint8_t  sIntermediate;

    sLength = (acp_uint8_t*)&(((mtdCharType*)aValue)->length);

    sIntermediate = sLength[0];
    sLength[0]    = sLength[1];
    sLength[1]    = sIntermediate;
}


ACI_RC mtdValidate( mtcColumn*   aColumn,
                    void*        aValue,
                    acp_uint32_t aValueSize)
{
/***********************************************************************
 *
 * Description : value�� semantic �˻� �� mtcColum �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdCharType * sCharVal = (mtdCharType*)aValue;
    ACI_TEST_RAISE( sCharVal == NULL, ERR_INVALID_NULL );

    ACI_TEST_RAISE( aValueSize < sizeof(acp_uint16_t), ERR_INVALID_LENGTH);
    ACI_TEST_RAISE( sCharVal->length + sizeof(acp_uint16_t) != aValueSize,
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdChar,
                                   1,                // arguments
                                   sCharVal->length, // precision
                                   0 )               // scale
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_NULL);
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE);
    }
    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


acp_double_t mtdSelectivityChar( void* aColumnMax,
                                 void* aColumnMin,
                                 void* aValueMax,
                                 void* aValueMin )
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
    const mtdCharType* sColumnMax;
    const mtdCharType* sColumnMin;
    const mtdCharType* sValueMax;
    const mtdCharType* sValueMin;

    // ������ ���� ���� acp_double_t ����
    acp_double_t       sColMaxDouble;
    acp_double_t       sColMinDouble;
    acp_double_t       sValMaxDouble;
    acp_double_t       sValMinDouble;
    // Selectivity
    acp_double_t       sSelectivity;

    // 10����, 16����, �Ϲݹ��ڿ� ���θ� ǥ���ϴ� ����
    acp_slong_t           sStringType;

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
    ACE_DASSERT( aColumnMax == ACP_ALIGN_ALL_PTR( aColumnMax, MTD_CHAR_ALIGN ) );
    ACE_DASSERT( aColumnMin == ACP_ALIGN_ALL_PTR( aColumnMin, MTD_CHAR_ALIGN ) );
    ACE_DASSERT( aValueMax == ACP_ALIGN_ALL_PTR( aValueMax, MTD_CHAR_ALIGN ) );
    ACE_DASSERT( aValueMin == ACP_ALIGN_ALL_PTR( aValueMin, MTD_CHAR_ALIGN ) );

    if ( ( mtdIsNull( NULL, aColumnMax, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aColumnMin, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aValueMax,  MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aValueMin,  MTD_OFFSET_USELESS ) == ACP_TRUE ) )
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
        sSelectivity = mtcdDouble.selectivity((void *)&sColMaxDouble,
                                              (void *)&sColMinDouble,
                                              (void *)&sValMaxDouble,
                                              (void *)&sValMinDouble );
    }

    return sSelectivity;

}


acp_slong_t mtdStringType( const mtdCharType * aValue )
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
    acp_slong_t sLength;
    acp_slong_t sIdx;
    acp_slong_t sStringType;

    sStringType = 0;
    sLength     = (acp_slong_t)(aValue->length);

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

acp_double_t mtdDigitsToDouble( const mtdCharType* aValue,
                                acp_uint32_t       aBase )
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

    acp_double_t sDoubleVal;
    acp_uint64_t sLongVal;
    acp_uint32_t sDigit;
    acp_uint32_t i;

    static const acp_uint8_t sHex[256] = {
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

    sDoubleVal = MTC_ULTODB( sLongVal );

    return sDoubleVal;
}

acp_double_t mtdConvertToDouble( const mtdCharType* aValue )
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
    acp_double_t sDoubleVal;       // ��ȯ�� Double ��
    acp_uint64_t sLongVal;         // acp_uint64_t���� ��ȯ �� ����� ����

    sLongVal = 0;                // �ʱ�ȭ

    // mtdCharType�� acp_uint64_t���� ��ȯ
    acpMemCpy( (acp_uint8_t*)&sLongVal,
               aValue->value,
               MTD_MIN( aValue->length, sizeof(sLongVal) ) );

    // �պκ� 52��Ʈ�� ����� ��ȯ
    sLongVal   = sLongVal >> 12;
    sDoubleVal = MTC_ULTODB( sLongVal );

    return sDoubleVal;

#else
    //----------------------------------------------------
    // Little endian �� ���
    //----------------------------------------------------
    acp_double_t sDoubleVal;       // ��ȯ�� Double ��
    acp_uint64_t sLongVal;         // acp_uint64_t���� ��ȯ �� ����� ����
    acp_uint64_t sEndian;          // byte ordering ��ȯ�� ����� �ӽ� ����
    acp_uint8_t* sSrc;             // byte ordering ��ȯ�� ���� ������
    acp_uint8_t* sDest;            // byte ordering ��ȯ�� ���� ������

    sLongVal = 0;                // �ʱ�ȭ

    // mtdCharType�� acp_uint64_t���� ��ȯ
    acpMemCpy( (acp_uint8_t*)&sLongVal,
               aValue->value,
               MTD_MIN( aValue->length, sizeof(sLongVal) ) );

    // byte ordering ����
    sSrc     = (acp_uint8_t*)&sLongVal;
    sDest    = (acp_uint8_t*)&sEndian;
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
    sDoubleVal = MTC_ULTODB( sEndian );

    return sDoubleVal;

#endif

}


ACI_RC mtdValueFromOracle( mtcColumn*    aColumn,
                           void*         aValue,
                           acp_uint32_t* aValueOffset,
                           acp_uint32_t  aValueSize,
                           const void*   aOracleValue,
                           acp_sint32_t  aOracleLength,
                           ACI_RC*       aResult )
{
    acp_uint32_t sValueOffset;
    mtdCharType* sValue;

    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_CHAR_ALIGN );

    if( aOracleLength < 0 )
    {
        aOracleLength = 0;
    }

    // aColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdChar,
                                   1,
                                   ( aOracleLength > 1 ) ? aOracleLength : 1,
                                   0 )
              != ACI_SUCCESS );


    if( sValueOffset + aColumn->column.size <= aValueSize )
    {
        sValue = (mtdCharType*)((acp_uint8_t*)aValue+sValueOffset);
        
        sValue->length = aOracleLength;
        
        acpMemCpy( sValue->value, aOracleValue, aOracleLength );
        
        aColumn->column.offset = sValueOffset;
        
        *aValueOffset = sValueOffset + aColumn->column.size;
        
        *aResult = ACI_SUCCESS;
    }
    else
    {
        *aResult = ACI_FAILURE;
    }
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue )
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
        ACI_TEST_RAISE( (aDestValueOffset + aLength + mtdHeaderSize()) > aColumnSize, ERR_INVALID_STORED_VALUE );

        sCharValue->length = (acp_uint16_t)(aDestValueOffset + aLength);
        acpMemCpy( (acp_uint8_t*)sCharValue + mtdHeaderSize() + aDestValueOffset, aValue, aLength );
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;    
}


acp_uint32_t mtdNullValueSize()
{
/*******************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ    
 * �� ) mtdCharType( acp_uint16_t length; acp_uint8_t value[1] ) ����
 *      lengthŸ���� acp_uint16_t�� ũ�⸦ ��ȯ
 *******************************************************************/
    
    return mtdActualSize( NULL,
                          & mtcdCharNull,
                          MTD_OFFSET_USELESS );   
}

static acp_uint32_t mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdCharType( acp_uint16_t length; acp_uint8_t value[1] ) ����
 *      lengthŸ���� acp_uint16_t�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/

    return sizeof(acp_uint16_t);
}

