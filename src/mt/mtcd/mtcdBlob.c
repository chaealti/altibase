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
 * $Id: mtcdBlob.cpp 35486 2009-09-10 00:26:55Z elcarim $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>

#include <mtcdTypes.h>

extern mtdModule mtcdBlob;

#define MTD_BLOB_ALIGN             ( (acp_sint32_t) sizeof(acp_uint32_t) )
#define MTD_BLOB_PRECISION_DEFAULT (0)
#define MTD_BLOB_PRECISION_MINIMUM (0) // To Fix BUG-12597
// BUG-19925 : ��� ������Ÿ���� (GEOMETRY����) �ִ� ũ�⸦
// constant tuple�� �ִ� ũ���� 65536���� ������
#define MTD_BLOB_PRECISION_MAXIMUM ( (acp_sint32_t) (65536 - sizeof(acp_sint32_t)) )

static mtdBlobType mtdBlobNull = { 0, {'\0',} };

static ACI_RC mtdInitializeBlob( acp_uint32_t aNo );

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

static void mtdEndian( void* aValue );

static ACI_RC mtdValidate( mtcColumn*   aColumn,
                           void*        aValue,
                           acp_uint32_t aValueSize);

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue );

static acp_uint32_t mtdNullValueSize();

static acp_uint32_t mtdHeaderSize();

static mtcName mtdTypeName[1] = {
    { NULL, 4, (void*)"BLOB" }
};

static mtcColumn mtdColumn;

mtdModule mtcdBlob = {
    mtdTypeName,
    &mtdColumn,
    MTD_BLOB_ID,
    0,
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    MTD_BLOB_ALIGN,
    MTD_GROUP_MISC|
    MTD_CANON_NEED|
    MTD_CREATE_ENABLE|
    MTD_COLUMN_TYPE_LOB|
    MTD_SELECTIVITY_DISABLE|
    MTD_LITERAL_TRUE|
    MTD_VARIABLE_LENGTH_TYPE_TRUE|  // PROJ-1705
    MTD_DATA_STORE_DIVISIBLE_TRUE|  // PROJ-1705
    MTD_DATA_STORE_MTDVALUE_FALSE,  // PROJ-1705
    ACP_SINT32_MAX,  // BUG-16493 �����δ� 4G�� ����������
    // odbc���� ���� ��ȸ����(V$DATATYPE)���θ� ����
    0,
    0,
    &mtdBlobNull,
    mtdInitializeBlob,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecisionNA,
    mtdNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrueNA,
    NULL,           // Logical Comparison
    {
        // Key Comparison
        {
            NULL,         // Ascending Key Comparison
            NULL          // Descending Key Comparison
        }
        ,
        {
            NULL,         // Ascending Key Comparison
            NULL          // Descending Key Comparison
        }
        ,
        {
            NULL,         // Ascending Key Comparison
            NULL          // Descending Key Comparison
        }
    },
    mtdCanonizeDefault,
    mtdEndian,
    mtdValidate,
    mtdSelectivityNA,
    mtdEncodeNA,
    mtdDecodeDefault,
    mtdCompileFmtDefault,
    mtdValueFromOracleDefault,
    mtdMakeColumnInfoDefault,

    // PROJ-1705
    mtdStoredValue2MtdValue,
    mtdNullValueSize,
    mtdHeaderSize
};

ACI_RC mtdInitializeBlob( acp_uint32_t aNo )
{
    ACI_TEST( mtdInitializeModule( &mtcdBlob, aNo ) != ACI_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( & mtdColumn,
                                   & mtcdBlob,
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
    if( *aArguments == 0 )
    {
        *aArguments = 1;
        *aPrecision = MTD_BLOB_PRECISION_DEFAULT;
    }

    ACI_TEST_RAISE( *aArguments != 1, ERR_INVALID_PRECISION );

    ACI_TEST_RAISE( *aPrecision < MTD_BLOB_PRECISION_MINIMUM ||
                    *aPrecision > MTD_BLOB_PRECISION_MAXIMUM,
                    ERR_INVALID_LENGTH );

    *aColumnSize = sizeof(acp_uint32_t) + *aPrecision;
    *aScale = 0;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

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
    acp_uint32_t       sValueOffset;
    mtdBlobType*       sValue;
    const acp_uint8_t* sToken;
    const acp_uint8_t* sTokenFence;
    acp_uint8_t*       sIterator;
    
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

    ACP_UNUSED(aTemplate);
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_BLOB_ALIGN );

    sValue = (mtdBlobType*)( (acp_uint8_t*)aValue + sValueOffset );

    *aResult = ACI_SUCCESS;
    
    sIterator = sValue->value;
    
    if( ( (aTokenLength+1) >> 1 ) <= (acp_uint8_t*)aValue - sIterator + aValueSize )
    {
        for( sToken      = (const acp_uint8_t*)aToken,
                 sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
             sIterator++, sToken += 2 )
        {
            ACI_TEST_RAISE( sHex[sToken[0]] > 15, ERR_INVALID_LITERAL );
            *sIterator = sHex[sToken[0]]<<4;
            if( sToken + 1 < sTokenFence )
            {
                ACI_TEST_RAISE( sHex[sToken[1]] > 15, ERR_INVALID_LITERAL );
                *sIterator |= sHex[sToken[1]];
            }
        }
	
        sValue->length         = sIterator - sValue->value;

        // precision, scale �� ���� ��, estimate�� semantic �˻�
        aColumn->flag            = 1;
        aColumn->precision       = sValue->length;
        aColumn->scale           = 0;

        ACI_TEST( mtdEstimate( & aColumn->column.size,
                               & aColumn->flag,
                               & aColumn->precision,
                               & aColumn->scale )
                  != ACI_SUCCESS );

        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset
            + sizeof(acp_uint32_t) + sValue->length;
    }
    else
    {
        *aResult = ACI_FAILURE;
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
    const mtdBlobType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdBlobType*)
        mtdValueForModule( NULL, 
                           aRow, 
                           aFlag,
                           mtcdBlob.staticNull );
    
    return sizeof(acp_uint32_t) + sValue->length;
}

void mtdNull( const mtcColumn* aColumn,
              void*            aRow,
              acp_uint32_t     aFlag )
{
    mtdBlobType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (mtdBlobType*)
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
    const mtdBlobType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdBlobType*)
        mtdValueForModule( NULL,
                           aRow, 
                           aFlag,
                           mtcdBlob.staticNull );

    return mtcHashSkip( aHash, sValue->value, sValue->length );
}

acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdBlobType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (const mtdBlobType*)
        mtdValueForModule( NULL,
                           aRow, 
                           aFlag,
                           mtcdBlob.staticNull );

    return (sValue->length == 0) ? ACP_TRUE : ACP_FALSE;
}

void mtdEndian( void* aValue )
{
    acp_uint8_t* sLength;
    acp_uint8_t  sIntermediate;

    sLength = (acp_uint8_t*)&(((mtdBlobType*)aValue)->length);
    sIntermediate = sLength[0];
    sLength[0]    = sLength[3];
    sLength[3]    = sIntermediate;
    sIntermediate = sLength[1];
    sLength[1]    = sLength[2];
    sLength[2]    = sIntermediate;
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
    
    mtdBlobType * sVal = (mtdBlobType*)aValue;
    
    ACI_TEST_RAISE( aValue == NULL, ERR_INVALID_NULL );
    
    ACI_TEST_RAISE((aValueSize < sizeof(acp_sint32_t)) ||
                   (sVal->length + sizeof(acp_sint32_t) != aValueSize),
                   ERR_INVALID_LENGTH );

    ACI_TEST_RAISE( sVal->length > aColumn->column.size, ERR_INVALID_VALUE );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdBlob,
                                   1,            // arguments
                                   sVal->length, // precision
                                   0 )           // scale
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

    ACI_EXCEPTION( ERR_INVALID_VALUE );
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                void*        aDestValue,
                                acp_uint32_t aDestValueOffset,
                                acp_uint32_t aLength,
                                const void*  aValue )
{
/***********************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 **********************************************************************/

    // LOB �÷��� ���ڵ忡 LOB�÷������ �����. 
    // �÷� ��ġ�ÿ��� ���ڵ忡 ����� LOB�÷������ �а�
    // ��������Ÿ�� �����ϴ� �κп��� �о�´�.
    ACP_UNUSED(aColumnSize);
    
    ACI_TEST_RAISE( !( ( aDestValueOffset == 0 ) && ( aLength > 0 ) ), ERR_INVALID_STORED_VALUE );

    acpMemCpy( aDestValue, aValue, aLength );

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
/***********************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ    
 * �� ) mtdLobType( acp_uint32_t length; acp_uint8_t value[1] ) ����
 *      length Ÿ���� acp_uint32_t�� ũ�⸦ ��ȯ
 **********************************************************************/

    return mtdActualSize( NULL,
                          & mtdBlobNull,
                          MTD_OFFSET_USELESS );   
}

static acp_uint32_t mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdLobType( acp_uint32_t length; acp_uint8_t value[1] ) ����
 *      length Ÿ���� acp_uint32_t�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/

    return sizeof(acp_uint32_t);
}

