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
 * $Id: mtcdDouble.cpp 36509 2009-11-04 03:26:25Z sungminee $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>

#include <mtcdTypes.h>

#include <math.h>

#define MTD_DOUBLE_ALIGN   (sizeof(acp_uint64_t))
#define MTD_DOUBLE_SIZE    (sizeof(mtdDoubleType))

extern mtdModule mtcdDouble;

acp_uint64_t mtcdDoubleNull = ( MTD_DOUBLE_EXPONENT_MASK |
                               MTD_DOUBLE_FRACTION_MASK );

static ACI_RC mtdInitializeDouble( acp_uint32_t aNo );

static ACI_RC mtdEstimate( acp_uint32_t* aColumnSize,
                           acp_uint32_t* aArguments,
                           acp_sint32_t* aPrecision,
                           acp_sint32_t* aScale );

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

static acp_sint32_t mtdDoubleLogicalComp( mtdValueInfo* aValueInfo1,
                                          mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                               mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                                  mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                   mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                                     mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDoubleStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                                      mtdValueInfo* aValueInfo2 );

static void mtdEndian( void* aValue );

static ACI_RC mtdValidate( mtcColumn*   aColumn,
                           void*        aValue,
                           acp_uint32_t aValueSize);

static acp_double_t mtdSelectivityDouble( void* aColumnMax,
                                          void* aColumnMin, 
                                          void* aValueMax,  
                                          void* aValueMin );

static ACI_RC mtdEncode( mtcColumn*    aColumn,
                         void*         aValue,
                         acp_uint32_t  aValueSize,
                         acp_uint8_t*  aCompileFmt,
                         acp_uint32_t  aCompileFmtLen,
                         acp_uint8_t*  aText,
                         acp_uint32_t* aTextLen,
                         ACI_RC*       aRet );

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue );

static acp_uint32_t mtdNullValueSize();

static mtcName mtdTypeName[1] = {
    { NULL, 6, (void*)"DOUBLE" },
};

static mtcColumn mtdColumn;

mtdModule mtcdDouble = {
    mtdTypeName,
    &mtdColumn,
    MTD_DOUBLE_ID,
    0,
    { 0,
      0,
      0, 0, 0, 0, 0 },
    MTD_DOUBLE_ALIGN,
    MTD_GROUP_NUMBER|
    MTD_CANON_NEEDLESS|
    MTD_CREATE_ENABLE|
    MTD_COLUMN_TYPE_FIXED|
    MTD_SELECTIVITY_ENABLE|
    MTD_NUMBER_GROUP_TYPE_DOUBLE|
    MTD_SEARCHABLE_PRED_BASIC|
    MTD_UNSIGNED_ATTR_TRUE|
    MTD_NUM_PREC_RADIX_TRUE|
    MTD_VARIABLE_LENGTH_TYPE_FALSE| // PROJ-1705
    MTD_DATA_STORE_DIVISIBLE_FALSE| // PROJ-1705
    MTD_DATA_STORE_MTDVALUE_FALSE,  // PROJ-1705
    15,
    0,
    0,
    &mtcdDoubleNull,
    mtdInitializeDouble,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecisionNA,
    mtdNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrueNA,
    mtdDoubleLogicalComp,      // Logical Comparison
    {
        // Key Comparison
        {
            // mt value�� ���� compare
            mtdDoubleMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdDoubleMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare 
            mtdDoubleStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdDoubleStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare 
            mtdDoubleStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdDoubleStoredStoredKeyDescComp // Descending Key Comparison
        }
    },
    mtdCanonizeDefault,
    mtdEndian,
    mtdValidate,
    mtdSelectivityDouble,
    mtdEncode,
    mtdDecodeDefault,
    mtdCompileFmtDefault,
    mtdValueFromOracleDefault,
    mtdMakeColumnInfoDefault,

    // PROJ-1705
    mtdStoredValue2MtdValue,
    mtdNullValueSize,
    mtdHeaderSizeDefault
};

ACI_RC mtdInitializeDouble( acp_uint32_t aNo )
{
    ACI_TEST_RAISE( MTD_DOUBLE_SIZE != 8, ERR_INCOMPATIBLE_TYPE );
    
    ACI_TEST( mtdInitializeModule( &mtcdDouble, aNo ) != ACI_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( & mtdColumn,
                                   & mtcdDouble,
                                   0,   // arguments
                                   0,   // precision
                                   0 )  // scale
              != ACI_SUCCESS );
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INCOMPATIBLE_TYPE );
    aciSetErrorCode(mtERR_FATAL_INCOMPATIBLE_TYPE);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtdEstimate( acp_uint32_t* aColumnSize,
                    acp_uint32_t* aArguments,
                    acp_sint32_t* aPrecision,
                    acp_sint32_t* aScale )
{
    ACP_UNUSED(aPrecision);
    ACP_UNUSED(aScale);

    ACI_TEST_RAISE( *aArguments != 0, ERR_INVALID_PRECISION );

    * aColumnSize = MTD_DOUBLE_SIZE;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_PRECISION );
    aciSetErrorCode(mtERR_ABORT_INVALID_PRECISION);
    
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
    acp_uint32_t   sValueOffset;
    mtdDoubleType* sValue;
    acp_sint32_t   sSign = 1;
    acp_uint64_t   sTemp;

    ACP_UNUSED(aTemplate);
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_DOUBLE_ALIGN );
    
    if( sValueOffset + MTD_DOUBLE_SIZE <= aValueSize )
    {
        sValue = (mtdDoubleType*)( (acp_uint8_t*)aValue + sValueOffset );
        if( aTokenLength == 0 )
        {
            mtcdDouble.null( mtcdDouble.column, sValue, MTD_OFFSET_USELESS );
        }
        else
        {
            char  sBuffer[1024];
            ACI_TEST_RAISE( aTokenLength >= sizeof(sBuffer),
                            ERR_INVALID_LITERAL );
            acpMemCpy( sBuffer, aToken, aTokenLength );
            sBuffer[aTokenLength] = 0;

            acpCStrToInt64( (acp_char_t*)sBuffer,
                            acpCStrLen((acp_char_t*)sBuffer, ACP_UINT32_MAX),
                            &sSign,
                            &sTemp,
                            10,
                            (char**)NULL);

            *sValue = sTemp;

            /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
            if( *sValue == 0 )
            {
                *sValue = 0;
            }
            /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */
            ACI_TEST_RAISE( mtdIsNull( mtcdDouble.column,
                                       sValue,
                                       MTD_OFFSET_USELESS ) == ACP_TRUE,
                            ERR_VALUE_OVERFLOW );

             // To fix BUG-12281
            // underflow �˻�

            if ( (*sValue < MTD_DOUBLE_MINIMUM) && (*sValue > -MTD_DOUBLE_MINIMUM))
            {
                *sValue = 0;
            }
            else
            {
                //nothing to do
            }
        }
     
        // to fix BUG-12582
        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset + MTD_DOUBLE_SIZE;
        
        *aResult = ACI_SUCCESS;
    }
    else
    {
        *aResult = ACI_FAILURE;
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);

    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

acp_uint32_t mtdActualSize( const mtcColumn* aTemp,
                            const void*      aTemp2,
                            acp_uint32_t     aTemp3 )
{
    ACP_UNUSED(aTemp);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    
    return (acp_uint32_t)MTD_DOUBLE_SIZE;
}

void mtdNull( const mtcColumn* aColumn,
              void*            aRow,
              acp_uint32_t     aFlag )
{
    static acp_uint64_t sNull = ( MTD_DOUBLE_EXPONENT_MASK |
                                  MTD_DOUBLE_FRACTION_MASK );
    
    mtdDoubleType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (mtdDoubleType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           NULL );

    if( sValue != NULL )
    {
        acpMemCpy( sValue, &sNull, MTD_DOUBLE_SIZE );
    }
}

acp_uint32_t mtdHash( acp_uint32_t     aHash,
                      const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdDoubleType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdDouble.staticNull );

    if( mtcdDouble.isNull( aColumn, sValue, MTD_OFFSET_USELESS ) != ACP_TRUE )
    {
        aHash = mtcHash( aHash, (const acp_uint8_t*)sValue, MTD_DOUBLE_SIZE );
    }
    
    return aHash;
}

acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdDoubleType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdDouble.staticNull );

    return ( *(acp_uint64_t*)sValue & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
}

acp_sint32_t
mtdDoubleLogicalComp( mtdValueInfo* aValueInfo1,
                      mtdValueInfo* aValueInfo2 )
{
    return mtdDoubleMtdMtdKeyAscComp( aValueInfo1,
                                      aValueInfo2 );
}

acp_sint32_t mtdDoubleMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                        mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdDoubleType* sValue1;
    const mtdDoubleType* sValue2;
    acp_bool_t           sNull1;
    acp_bool_t           sNull2;

    //---------
    // value1
    //---------        
    sValue1 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdDouble.staticNull );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // value2
    //---------        
    sValue2 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDouble.staticNull );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    
    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 > *sValue2 )
        {
            return 1;
        }
        if( *sValue1 < *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDoubleMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                            mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdDoubleType* sValue1;
    const mtdDoubleType* sValue2;
    acp_bool_t           sNull1;
    acp_bool_t           sNull2;

    //---------
    // value1
    //---------
    sValue1 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdDouble.staticNull );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // value2
    //---------
    sValue2 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDouble.staticNull );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    
    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 < *sValue2 )
        {
            return 1;
        }
        if( *sValue1 > *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t mtdDoubleStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                           mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDoubleType        sDoubleValue1;
    mtdDoubleType*       sValue1;
    const mtdDoubleType* sValue2;
    acp_bool_t           sNull1;
    acp_bool_t           sNull2;
    
    //---------
    // value1
    //---------
    
    sValue1 = &sDoubleValue1;

    MTC_DOUBLE_BYTE_ASSIGN( sValue1, aValueInfo1->value );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // value2
    //---------
    sValue2 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDouble.staticNull );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    
    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 > *sValue2 )
        {
            return 1;
        }
        if( *sValue1 < *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDoubleStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                               mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDoubleType        sDoubleValue1;
    mtdDoubleType*       sValue1;
    const mtdDoubleType* sValue2;
    acp_bool_t           sNull1;
    acp_bool_t           sNull2;
    
    //---------
    // value1
    //---------

    sValue1 = &sDoubleValue1;

    MTC_DOUBLE_BYTE_ASSIGN( sValue1, aValueInfo1->value );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    

    //---------
    // value2
    //---------        
    sValue2 = (const mtdDoubleType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDouble.staticNull );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 < *sValue2 )
        {
            return 1;
        }
        if( *sValue1 > *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t mtdDoubleStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                              mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDoubleType         sDoubleValue1;
    mtdDoubleType       * sValue1;
    mtdDoubleType         sDoubleValue2;
    mtdDoubleType       * sValue2;
    acp_bool_t            sNull1;
    acp_bool_t            sNull2;

    //---------
    // value1
    //---------

    sValue1 = &sDoubleValue1;
    
    MTC_DOUBLE_BYTE_ASSIGN( sValue1, aValueInfo1->value );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // value2
    //---------

    sValue2 = &sDoubleValue2;

    MTC_DOUBLE_BYTE_ASSIGN( sValue2, aValueInfo2->value );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    
    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 > *sValue2 )
        {
            return 1;
        }
        if( *sValue1 < *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDoubleStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                  mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDoubleType         sDoubleValue1;
    mtdDoubleType       * sValue1;
    mtdDoubleType         sDoubleValue2;
    mtdDoubleType       * sValue2;
    acp_bool_t            sNull1;
    acp_bool_t            sNull2;

    //---------
    // value1
    //---------

    sValue1 = &sDoubleValue1;
    
    MTC_DOUBLE_BYTE_ASSIGN( sValue1, aValueInfo1->value );

    sNull1 = ( *(acp_uint64_t*)sValue1 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;

    //---------
    // value2
    //---------

    sValue2 = &sDoubleValue2;

    MTC_DOUBLE_BYTE_ASSIGN( sValue2, aValueInfo2->value );

    sNull2 = ( *(acp_uint64_t*)sValue2 & MTD_DOUBLE_EXPONENT_MASK )
        == MTD_DOUBLE_EXPONENT_MASK ? ACP_TRUE : ACP_FALSE ;
    
    //---------
    // compare
    //---------
    
    if( (sNull1 != ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        if( *sValue1 < *sValue2 )
        {
            return 1;
        }
        if( *sValue1 > *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( (sNull1 == ACP_TRUE) && (sNull2 != ACP_TRUE) )
    {
        return 1;
    }
    if( (sNull1 != ACP_TRUE) /*&& (sNull2 == ACP_TRUE)*/ )
    {
        return -1;
    }
    return 0;
}

void mtdEndian( void* aValue )
{
    acp_uint32_t sCount;
    acp_uint8_t* sValue;
    acp_uint8_t  sBuffer[MTD_DOUBLE_SIZE];
    
    sValue = (acp_uint8_t*)aValue;
    for( sCount = 0; sCount < MTD_DOUBLE_SIZE; sCount++ )
    {
        sBuffer[MTD_DOUBLE_SIZE-sCount-1] = sValue[sCount];
    }
    for( sCount = 0; sCount < MTD_DOUBLE_SIZE; sCount++ )
    {
        sValue[sCount] = sBuffer[sCount];
    }
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
    ACI_TEST_RAISE( aValue == NULL, ERR_INVALID_NULL);
    
    ACI_TEST_RAISE( aValueSize != sizeof(mtdDoubleType),
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ���� 
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdDouble,
                                   0,   // arguments
                                   0,   // precision
                                   0 )  // scale
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


acp_double_t mtdSelectivityDouble( void* aColumnMax, 
                                   void* aColumnMin, 
                                   void* aValueMax,  
                                   void* aValueMin )
{
/***********************************************************************
 *
 * Description :
 *    DOUBLE �� Selectivity ���� �Լ�
 *
 * Implementation :
 *
 *    Selectivity = (aValueMax - aValueMin) / (aColumnMax - aColumnMin)
 *    0 < Selectivity <= 1 �� ���� ������
 *
 ***********************************************************************/
    
    mtdDoubleType* sColumnMax;
    mtdDoubleType* sColumnMin;
    mtdDoubleType* sValueMax;
    mtdDoubleType* sValueMin;
    mtdDoubleType* sULongPtr;
    acp_double_t   sSelectivity;
    acp_double_t   sDenominator;  // �и�
    acp_double_t   sNumerator;    // ���ڰ�
    mtdValueInfo   sValueInfo1;
    mtdValueInfo   sValueInfo2;
    mtdValueInfo   sValueInfo3;
    mtdValueInfo   sValueInfo4;

    sColumnMax = (mtdDoubleType*) aColumnMax;
    sColumnMin = (mtdDoubleType*) aColumnMin;
    sValueMax  = (mtdDoubleType*) aValueMax;
    sValueMin  = (mtdDoubleType*) aValueMin;

    //------------------------------------------------------
    // Data�� ��ȿ�� �˻�
    //     NULL �˻� : ����� �� ����
    //------------------------------------------------------

    if ( ( mtdIsNull( NULL, aColumnMax, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aColumnMin, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aValueMax, MTD_OFFSET_USELESS ) == ACP_TRUE )  ||
         ( mtdIsNull( NULL, aValueMin, MTD_OFFSET_USELESS ) == ACP_TRUE ) )
    {
        // Data�� NULL �� ���� ���
        // �ε�ȣ�� Default Selectivity�� 1/3�� Setting��
        sSelectivity = MTD_DEFAULT_SELECTIVITY;
    }
    else
    {
        //------------------------------------------------------
        // ��ȿ�� �˻�
        // ������ ���� ������ �߸��� ��� �����̰ų� �Է� ������.
        // Column�� Min������ Value�� Max���� ���� ���
        // Column�� Max������ Value�� Min���� ū ���
        //------------------------------------------------------

        sValueInfo1.column = NULL;
        sValueInfo1.value  = sColumnMin;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = NULL;
        sValueInfo2.value  = sValueMax;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sValueInfo3.column = NULL;
        sValueInfo3.value  = sColumnMax;
        sValueInfo3.flag   = MTD_OFFSET_USELESS;

        sValueInfo4.column = NULL;
        sValueInfo4.value  = sValueMin;
        sValueInfo4.flag   = MTD_OFFSET_USELESS;
        
        if ( ( mtdDoubleLogicalComp( &sValueInfo1,
                                     &sValueInfo2 ) > 0 )
             ||
             ( mtdDoubleLogicalComp( &sValueInfo3,
                                     &sValueInfo4 ) < 0 ) )
        {
            // To Fix PR-11858
            sSelectivity = 0;
        }
        else
        {
            //------------------------------------------------------
            // Value�� ����
            // Value�� Min���� Column�� Min������ �۴ٸ� ����
            // Value�� Max���� Column�� Max������ ũ�ٸ� ����
            //------------------------------------------------------

            sValueInfo1.column = NULL;
            sValueInfo1.value  = sValueMin;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = NULL;
            sValueInfo2.value  = sColumnMin;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            if ( mtdDoubleLogicalComp( &sValueInfo1,
                                       &sValueInfo2 ) < 0 )
            {
                sValueMin = sColumnMin;
            }
            else
            {
                // Nothing To Do
            }

            sValueInfo1.column = NULL;
            sValueInfo1.value  = sValueMax;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = NULL;
            sValueInfo2.value  = sColumnMax;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            if ( mtdDoubleLogicalComp( &sValueInfo1,
                                       &sValueInfo2 ) > 0 )
            {
                sValueMax = sColumnMax;
            }
            else
            {
                // Nothing To Do
            }
            
            //------------------------------------------------------
            // �и� (aColumnMax - aColumnMin) �� ȹ��
            //------------------------------------------------------
        
            sDenominator = (acp_double_t)(*sColumnMax - *sColumnMin);
            sULongPtr = &sDenominator;
    
            if ( (sDenominator <= 0.0) ||
                 (((*(acp_uint64_t*)sULongPtr & MTD_DOUBLE_EXPONENT_MASK)) ==
                  (MTD_DOUBLE_EXPONENT_MASK) ) )
            {
                // �߸��� ��� ������ ����� ���
                sSelectivity = MTD_DEFAULT_SELECTIVITY;
            }
            else
            {
                //------------------------------------------------------
                // ���ڰ� (aValueMax - aValueMin) �� ȹ��
                //------------------------------------------------------
            
                sNumerator = (acp_double_t) (*sValueMax - *sValueMin);

                sSelectivity = sNumerator / sDenominator;
                sULongPtr = &sSelectivity;
                // jhseong, NaN check, PR-9195
                if ( sNumerator <= 0.0 ||
                     (((*(acp_uint64_t*)sULongPtr & MTD_DOUBLE_EXPONENT_MASK)) ==
                      (MTD_DOUBLE_EXPONENT_MASK) ) )
                {
                    // �߸��� �Է� ������ ���
                    // To Fix PR-11858
                    sSelectivity = 0;
                }
                else
                {
                    //------------------------------------------------------
                    // Selectivity ���
                    //------------------------------------------------------
                
                    if ( sSelectivity > 1.0 )
                    {
                        sSelectivity = 1.0;
                    }
                }
            }
        }
    }
    
    return sSelectivity;
}

ACI_RC mtdEncode( mtcColumn*    aColumn,
                  void*         aValue,
                  acp_uint32_t  aValueSize ,
                  acp_uint8_t*  aCompileFmt ,
                  acp_uint32_t  aCompileFmtLen ,
                  acp_uint8_t*  aText,
                  acp_uint32_t* aTextLen,
                  ACI_RC*       aRet )
{
    mtdDoubleType  sDouble;
    acp_uint32_t   sStringLen;

    //----------------------------------
    // Parameter Validation
    //----------------------------------

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValueSize);
    ACP_UNUSED(aCompileFmt);
    ACP_UNUSED(aCompileFmtLen);
    
    ACE_ASSERT( aValue != NULL );
    ACE_ASSERT( aText != NULL );
    ACE_ASSERT( *aTextLen > 0 );
    ACE_ASSERT( aRet != NULL );
    
    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;

    //----------------------------------
    // Set String
    //----------------------------------
    
    // To Fix BUG-16801
    if ( mtdIsNull( NULL, aValue, MTD_OFFSET_USELESS ) == ACP_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sDouble = *(mtdDoubleType*)aValue;
    
        // BUG-17025
        if ( sDouble == (mtdDoubleType)0 )
        {
            sDouble = (mtdDoubleType)0;
        }
        else
        {
            // Nothing to do.
        }

        sStringLen = aciVaAppendFormat( (acp_char_t*) aText,
                                        (acp_size_t)aTextLen,
                                        "%"ACI_DOUBLE_G_FMT,
                                        sDouble);

    }

    //----------------------------------
    // Finalization
    //----------------------------------
    
    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    *aRet = ACI_SUCCESS;

    return ACI_SUCCESS;
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

    mtdDoubleType  * sDoubleValue;

    ACP_UNUSED(aDestValueOffset);
    
    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����. 

    sDoubleValue = (mtdDoubleType*)aDestValue;
    
    if( aLength == 0 )
    {
        // NULL ����Ÿ
        acpMemCpy( sDoubleValue, &mtcdDoubleNull, MTD_DOUBLE_SIZE );
    }
    else
    {
        ACI_TEST_RAISE( aLength != aColumnSize, ERR_INVALID_STORED_VALUE );        

        acpMemCpy( sDoubleValue, aValue, aLength );        
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
 *******************************************************************/
    
    return mtdActualSize( NULL,
                          & mtcdDoubleNull,
                          MTD_OFFSET_USELESS );   
}

