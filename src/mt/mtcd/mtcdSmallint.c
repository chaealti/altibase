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
 * $Id: mtcdSmallint.cpp 36509 2009-11-04 03:26:25Z sungminee $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>

#include <mtcdTypes.h>

#define MTD_SMALLINT_ALIGN (sizeof(mtdSmallintType))
#define MTD_SMALLINT_SIZE  (sizeof(mtdSmallintType))

extern mtdModule mtcdSmallint;

static const mtdSmallintType mtdSmallintNull = MTD_SMALLINT_NULL;

static ACI_RC mtdInitializeSmallint( acp_uint32_t aNo );

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

static acp_sint32_t mtdSmallintLogicalComp( mtdValueInfo* aValueInfo1,
                                            mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                                 mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                  mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                                    mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                     mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                                       mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdSmallintStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                                        mtdValueInfo* aValueInfo2 );

static void mtdEndian( void* aValue );

static ACI_RC mtdValidate( mtcColumn*   aColumn,
                           void*        aValue,
                           acp_uint32_t aValueSize);

static acp_double_t mtdSelectivitySmallint( void* aColumnMax,
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

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t     aColumnSize,
                                       void*            aDestValue,
                                       acp_uint32_t     aDestValueOffset,
                                       acp_uint32_t     aLength,
                                       const void*      aValue );

static acp_uint32_t mtdNullValueSize();

static mtcName mtdTypeName[1] = {
    { NULL, 8, (void*)"SMALLINT" },
};

static mtcColumn mtdColumn;

mtdModule mtcdSmallint = {
    mtdTypeName,
    &mtdColumn,
    MTD_SMALLINT_ID,
    0,
    { 0,
      0,
      0, 0, 0, 0, 0 },
    MTD_SMALLINT_ALIGN,
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
    MTD_DATA_STORE_MTDVALUE_FALSE,  // PROJ-1705
    5,
    0,
    0,
    (void*)&mtdSmallintNull,
    mtdInitializeSmallint,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecisionNA,
    mtdNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrueNA,
    mtdSmallintLogicalComp,  // Logical Comparison
    {
        // Key Comparison
        {
            // mt value�� ���� compare 
            mtdSmallintMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdSmallintMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare 
            mtdSmallintStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdSmallintStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare 
            mtdSmallintStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdSmallintStoredStoredKeyDescComp // Descending Key Comparison
        }
    },
    mtdCanonizeDefault,
    mtdEndian,
    mtdValidate,
    mtdSelectivitySmallint,
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

ACI_RC mtdInitializeSmallint( acp_uint32_t aNo )
{
    ACI_TEST( mtdInitializeModule( &mtcdSmallint, aNo ) != ACI_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( & mtdColumn,
                                   & mtcdSmallint,
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
    ACP_UNUSED(aPrecision);
    ACP_UNUSED(aScale);
    
    ACI_TEST_RAISE( *aArguments != 0, ERR_INVALID_PRECISION );

    *aColumnSize = MTD_SMALLINT_SIZE;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_PRECISION );
    aciSetErrorCode(mtERR_ABORT_INVALID_PRECISION);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtdValue( mtcTemplate*  aTemplate ,
                 mtcColumn*    aColumn ,
                 void*         aValue,
                 acp_uint32_t* aValueOffset,
                 acp_uint32_t  aValueSize,
                 const void*   aToken,
                 acp_uint32_t  aTokenLength,
                 ACI_RC*       aResult )
{
    acp_uint32_t           sValueOffset;
    mtdSmallintType*       sValue;
    const acp_uint8_t*     sToken;
    const acp_uint8_t*     sFence;
    acp_sint32_t           sIntermediate;

    ACP_UNUSED( aTemplate);
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_SMALLINT_ALIGN );
    
    sToken = (const acp_uint8_t*)aToken;
    
    if( sValueOffset + MTD_SMALLINT_SIZE <= aValueSize )
    {
        sValue = (mtdSmallintType*)( (acp_uint8_t*)aValue + sValueOffset );
        if( aTokenLength == 0 )
        {
            *sValue = MTD_SMALLINT_NULL;
        }
        else
        {
            sFence = sToken + aTokenLength;
            if( *sToken == '-' )
            {
                sToken++;
                ACI_TEST_RAISE( sToken >= sFence, ERR_INVALID_LITERAL );
                for( sIntermediate = 0; sToken < sFence; sToken++ )
                {
                    ACI_TEST_RAISE( *sToken < '0' || *sToken > '9',
                                    ERR_INVALID_LITERAL );
                    sIntermediate = sIntermediate * 10 - ( *sToken - '0' );
                    ACI_TEST_RAISE( sIntermediate < MTD_SMALLINT_MINIMUM,
                                    ERR_VALUE_OVERFLOW );
                }
                *sValue = sIntermediate;
            }
            else
            {
                if( *sToken == '+' )
                {
                    sToken++;
                }
                ACI_TEST_RAISE( sToken >= sFence, ERR_INVALID_LITERAL );
                for( sIntermediate = 0; sToken < sFence; sToken++ )
                {
                    ACI_TEST_RAISE( *sToken < '0' || *sToken > '9',
                                    ERR_INVALID_LITERAL );
                    sIntermediate = sIntermediate * 10 + ( *sToken - '0' );
                    ACI_TEST_RAISE( sIntermediate > MTD_SMALLINT_MAXIMUM,
                                    ERR_VALUE_OVERFLOW );
                }
                *sValue = sIntermediate;
            }
        }

        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset + MTD_SMALLINT_SIZE;
        
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
                            acp_uint32_t     aTemp3)
{
    ACP_UNUSED(aTemp);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    
    return (acp_uint32_t)MTD_SMALLINT_SIZE;
}

void mtdNull( const mtcColumn* aColumn,
              void*            aRow,
              acp_uint32_t     aFlag )
{
    mtdSmallintType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (mtdSmallintType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           NULL );
    
    if( sValue != NULL )
    {
        *sValue = MTD_SMALLINT_NULL;
    }
}

acp_uint32_t mtdHash( acp_uint32_t     aHash,
                      const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdSmallintType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdSmallint.staticNull );

    return mtcHash( aHash, (const acp_uint8_t*)sValue, MTD_SMALLINT_SIZE );
}

acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdSmallintType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdSmallint.staticNull );
    
    return (*sValue == MTD_SMALLINT_NULL) ? ACP_TRUE : ACP_FALSE ;
}

acp_sint32_t
mtdSmallintLogicalComp( mtdValueInfo* aValueInfo1,
                        mtdValueInfo* aValueInfo2 )
{
    return mtdSmallintMtdMtdKeyAscComp( aValueInfo1,
                                        aValueInfo2 );
}

acp_sint32_t
mtdSmallintMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                             mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdSmallintType* sValue1;
    const mtdSmallintType* sValue2;

    //---------
    // value1
    //---------
    sValue1 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdSmallint.staticNull );

    //---------
    // value2
    //---------
    sValue2 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdSmallint.staticNull );

    //---------
    // compare
    //---------

    if( ( *sValue1 != MTD_SMALLINT_NULL ) &&
        ( *sValue2 != MTD_SMALLINT_NULL ) )
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

acp_sint32_t
mtdSmallintMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                              mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtdSmallintType* sValue1;
    const mtdSmallintType* sValue2;

    //---------
    // value1
    //---------
    sValue1 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdSmallint.staticNull );
    
    //---------
    // value2
    //---------
    sValue2 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdSmallint.staticNull );

    //---------
    // compare
    //---------

    if( ( *sValue1 != MTD_SMALLINT_NULL ) &&
        ( *sValue2 != MTD_SMALLINT_NULL ) )
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

acp_sint32_t
mtdSmallintStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtdSmallintType        sValue1;
    const mtdSmallintType* sValue2;
    
    //---------
    // value1
    //---------        
    MTC_SHORT_BYTE_ASSIGN( &sValue1, aValueInfo1->value );

    //---------
    // value2
    //---------        
    sValue2 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdSmallint.staticNull );
    
    //---------
    // compare
    //---------

    if( ( sValue1 != MTD_SMALLINT_NULL ) &&
        ( *sValue2 != MTD_SMALLINT_NULL ) )
    {
        if( sValue1 > *sValue2 )
        {
            return 1;
        }
        if( sValue1 < *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( sValue1 < *sValue2 )
    {
        return 1;
    }
    if( sValue1 > *sValue2 )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdSmallintStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                 mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtdSmallintType         sValue1;
    const mtdSmallintType * sValue2;

    //---------
    // value1
    //---------
    MTC_SHORT_BYTE_ASSIGN( &sValue1, aValueInfo1->value );


    //---------
    // value2
    //---------
    sValue2 = (const mtdSmallintType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdSmallint.staticNull );
    
    //---------
    // compare
    //---------

    if( ( sValue1 != MTD_SMALLINT_NULL ) &&
        ( *sValue2 != MTD_SMALLINT_NULL ) )
    {
        if( sValue1 < *sValue2 )
        {
            return 1;
        }
        if( sValue1 > *sValue2 )
        {
            return -1;
        }
        return 0;
    }
    
    if( sValue1 < *sValue2 )
    {
        return 1;
    }
    if( sValue1 > *sValue2 )
    {
        return -1;
    }
    return 0;
}


acp_sint32_t
mtdSmallintStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                   mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdSmallintType   sValue1;
    mtdSmallintType   sValue2;

    //---------
    // value1
    //---------
    MTC_SHORT_BYTE_ASSIGN( &sValue1, aValueInfo1->value );

    //---------
    // value2
    //---------
    MTC_SHORT_BYTE_ASSIGN( &sValue2, aValueInfo2->value );

    //---------
    // compare
    //---------

    if( ( sValue1 != MTD_SMALLINT_NULL ) &&
        ( sValue2 != MTD_SMALLINT_NULL ) )
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

acp_sint32_t
mtdSmallintStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                    mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Stored Key�� ���� descending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdSmallintType   sValue1;
    mtdSmallintType   sValue2;

    //---------
    // value1
    //---------
    MTC_SHORT_BYTE_ASSIGN( &sValue1, aValueInfo1->value );

    //---------
    // value2
    //---------
    MTC_SHORT_BYTE_ASSIGN( &sValue2, aValueInfo2->value );

    //---------
    // compare
    //---------

    if( ( sValue1 != MTD_SMALLINT_NULL ) &&
        ( sValue2 != MTD_SMALLINT_NULL ) )
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

void mtdEndian( void* aValue )
{
    acp_uint8_t* sValue;
    acp_uint8_t  sIntermediate;
    
    sValue        = (acp_uint8_t*)aValue;
    sIntermediate = sValue[0];
    sValue[0]     = sValue[1];
    sValue[1]     = sIntermediate;
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
        
    ACI_TEST_RAISE( aValue == NULL, ERR_INVALID_NULL );
    
    ACI_TEST_RAISE( aValueSize != sizeof(mtdSmallintType),
                    ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ���� 
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdSmallint,
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


acp_double_t mtdSelectivitySmallint( void* aColumnMax,
                                     void* aColumnMin,
                                     void* aValueMax,
                                     void* aValueMin )
{
/***********************************************************************
 *
 * Description :
 *    SMALLINT �� Selectivity ���� �Լ�
 *
 * Implementation :
 *
 *    Selectivity = (aValueMax - aValueMin) / (aColumnMax - aColumnMin)
 *    0 < Selectivity <= 1 �� ���� ������
 *
 ***********************************************************************/
    
    mtdSmallintType* sColumnMax;
    mtdSmallintType* sColumnMin;
    mtdSmallintType* sValueMax;
    mtdSmallintType* sValueMin;
    acp_double_t     sSelectivity;
    acp_double_t     sDenominator;  // �и�
    acp_double_t     sNumerator;    // ���ڰ�

    mtdValueInfo     sValueInfo1;
    mtdValueInfo     sValueInfo2;
    mtdValueInfo     sValueInfo3;
    mtdValueInfo     sValueInfo4;
    
    sColumnMax = (mtdSmallintType*) aColumnMax;
    sColumnMin = (mtdSmallintType*) aColumnMin;
    sValueMax  = (mtdSmallintType*) aValueMax;
    sValueMin  = (mtdSmallintType*) aValueMin;

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

        if ( ( mtdSmallintLogicalComp( &sValueInfo1,
                                       &sValueInfo2 ) > 0 )
             ||
             ( mtdSmallintLogicalComp( &sValueInfo3,
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

            if ( mtdSmallintLogicalComp( &sValueInfo1,
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
            
            if ( mtdSmallintLogicalComp(  &sValueInfo1,
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
            
            if ( sDenominator <= 0.0 )
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
                
                if ( sNumerator <= 0.0 )
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
                    
                    sSelectivity = sNumerator / sDenominator;
                    
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

ACI_RC mtdEncode( mtcColumn*    aColumn ,
                  void*         aValue,
                  acp_uint32_t  aValueSize ,
                  acp_uint8_t*  aCompileFmt ,
                  acp_uint32_t  aCompileFmtLen ,
                  acp_uint8_t*  aText,
                  acp_uint32_t* aTextLen,
                  ACI_RC*       aRet )
{
    acp_uint32_t sStringLen;

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
        sStringLen = aciVaAppendFormat( (acp_char_t*) aText,
                                        (acp_size_t) aTextLen,
                                        "%"ACI_INT32_FMT,
                                        *(mtdSmallintType*) aValue );
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
                                       acp_uint32_t aDestValueOffset ,
                                       acp_uint32_t aLength,
                                       const void*  aValue )
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdSmallintType  * sSmallintValue;

    ACP_UNUSED(aDestValueOffset);
    
    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����. 

    sSmallintValue = (mtdSmallintType*)aDestValue;
    
    if( aLength == 0 )
    {
        // NULL ����Ÿ
        *sSmallintValue = mtdSmallintNull;
    }
    else
    {
        ACI_TEST_RAISE( aLength != aColumnSize, ERR_INVALID_STORED_VALUE );        

        acpMemCpy( sSmallintValue, aValue, aLength );        
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
                          & mtdSmallintNull,
                          MTD_OFFSET_USELESS );   
}

