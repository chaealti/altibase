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
 * $Id: mtdBlob.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>
#include <mtuProperty.h>

extern mtdModule mtdBlob;

#define MTD_BLOB_ALIGN             (MTD_LOB_ALIGN)

#define MTD_BLOB_PRECISION_MINIMUM (0) // To Fix BUG-12597

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
#define MTD_BLOB_PRECISION_MAXIMUM (104857600)
#define MTD_BLOB_PRECISION_DEFAULT (MTU_LOB_OBJECT_BUFFER_SIZE)

static mtdBlobType mtdBlobNull = { MTD_LOB_NULL_LENGTH, {'\0',} };

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
                        void*            aRow);

static UInt mtdHash( UInt             aHash,
                     const mtcColumn* aColumn,
                     const void*      aRow );

static idBool mtdIsNull( const mtcColumn* aColumn,
                         const void*      aRow );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
static IDE_RC mtdCanonize( const mtcColumn  * aCanon,
                           void            ** aCanonizedValue,
                           mtcEncryptInfo   * aCanonInfo,
                           const mtcColumn  * aColumn,
                           void             * aValue,
                           mtcEncryptInfo   * aColumnInfo,
                           mtcTemplate      * aTemplate );

static void mtdEndian( void* aValue );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static IDE_RC mtdStoredValue2MtdValue( UInt              aColumnSize,
                                       void            * aDestValue,
                                       UInt              aDestValueOffset,
                                       UInt              aLength,
                                       const void      * aValue );

static UInt mtdNullValueSize();

static UInt mtdHeaderSize();

static UInt mtdStoreSize( const smiColumn * aColumn );

static mtcName mtdTypeName[1] = {
    { NULL, 4, (void*)"BLOB" }
};

static mtcColumn mtdColumn;

mtdModule mtdBlob = {
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
      MTD_DATA_STORE_MTDVALUE_FALSE|  // PROJ-1705
      MTD_PSM_TYPE_ENABLE, // PROJ-1904
    ID_SINT_MAX,  // BUG-16493 �����δ� 4G�� ����������
                  // odbc���� ���� ��ȸ����(V$DATATYPE)���θ� ����
    0,
    0,
    &mtdBlobNull,
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
    mtdCanonize,
    mtdEndian,
    mtdValidate,
    mtd::selectivityNA,
    mtd::encodeNA,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtd::valueFromOracleDefault,
    mtd::makeColumnInfoDefault,

    // BUG-28934
    mtk::mergeAndRangeNA,
    mtk::mergeOrRangeListNA,

    {    
        // PROJ-1705
        mtdStoredValue2MtdValue,
        // PROJ-2429
        NULL 
    }, 
    mtdNullValueSize,
    mtdHeaderSize,

    // PROJ-2399
    mtdStoreSize
};

IDE_RC mtdInitialize( UInt aNo )
{
    IDE_TEST( mtd::initializeModule( &mtdBlob, aNo ) != IDE_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdBlob,
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
                    SInt * aScale )
{
    if( *aArguments == 0 )
    {
        *aArguments = 1;
        *aPrecision = MTD_BLOB_PRECISION_DEFAULT;
    }

    IDE_TEST_RAISE( *aArguments != 1, ERR_INVALID_PRECISION );

    IDE_TEST_RAISE( *aPrecision < MTD_BLOB_PRECISION_MINIMUM ||
                    *aPrecision > MTD_BLOB_PRECISION_MAXIMUM,
                    ERR_INVALID_LENGTH );

    *aColumnSize = ID_SIZEOF(SLong) + *aPrecision;
    *aScale = 0;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));

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
    UInt         sValueOffset;
    UInt         sValueSize;
    mtdBlobType* sValue;
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    
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
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_BLOB_ALIGN );

    sValue = (mtdBlobType*)( (UChar*)aValue + sValueOffset );

    *aResult = IDE_SUCCESS;
    
    sIterator = sValue->value;
    
    if( ( (aTokenLength+1) >> 1 ) <= (UChar*)aValue - sIterator + aValueSize )
    {
        for( sToken      = (const UChar*)aToken,
             sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
             sIterator++, sToken += 2 )
        {
            IDE_TEST_RAISE( sHex[sToken[0]] > 15, ERR_INVALID_LITERAL );
            *sIterator = sHex[sToken[0]]<<4;
            if( sToken + 1 < sTokenFence )
            {
                IDE_TEST_RAISE( sHex[sToken[1]] > 15, ERR_INVALID_LITERAL );
                *sIterator |= sHex[sToken[1]];
            }
        }

        sValueSize = sIterator - sValue->value;
        
        if ( sValueSize == 0 )
        {
            // null
            sValue->length = MTD_LOB_NULL_LENGTH;
        }
        else
        {
            sValue->length = sValueSize;
        }

        // precision, scale �� ���� ��, estimate�� semantic �˻�
        aColumn->flag            = 1;
        aColumn->precision       = sValueSize;
        aColumn->scale           = 0;

        IDE_TEST( mtdEstimate( & aColumn->column.size,
                               & aColumn->flag,
                               & aColumn->precision,
                               & aColumn->scale )
                  != IDE_SUCCESS );

        aColumn->column.offset   = sValueOffset;
        *aValueOffset            = sValueOffset
                                   + ID_SIZEOF(SLong) + sValueSize;
    }
    else
    {
        *aResult = IDE_FAILURE;
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
    if ( ((mtdBlobType *)aRow)->length == MTD_LOB_NULL_LENGTH )
    {
        /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
        return ID_SIZEOF(SLong);
    }
    else
    {
        return ID_SIZEOF(SLong) + ((mtdBlobType*)aRow)->length;
    }
}

void mtdSetNull( const mtcColumn* /* aColumn */,
                 void*            aRow )
{
    if( aRow != NULL )
    {
        ((mtdBlobType*)aRow)->length =  MTD_LOB_NULL_LENGTH;
    }
}

UInt mtdHash( UInt             aHash,
              const mtcColumn* ,
              const void*      aRow )
{
    return mtc::hashSkip( aHash, ((mtdBlobType*)aRow)->value, ((mtdBlobType*)aRow)->length );
}

idBool mtdIsNull( const mtcColumn* ,
                  const void*      aRow )
{
    return (((mtdBlobType*)aRow)->length == MTD_LOB_NULL_LENGTH) ? ID_TRUE : ID_FALSE;
}

void mtdEndian( void* aValue )
{
    UChar* sLength;
    UChar  sIntermediate;

    sLength = (UChar*)&(((mtdBlobType*)aValue)->length);
    sIntermediate = sLength[0];
    sLength[0]    = sLength[7];
    sLength[7]    = sIntermediate;
    sIntermediate = sLength[1];
    sLength[1]    = sLength[6];
    sLength[6]    = sIntermediate;
    sIntermediate = sLength[2];
    sLength[2]    = sLength[5];
    sLength[5]    = sIntermediate;
    sIntermediate = sLength[3];
    sLength[3]    = sLength[4];
    sLength[4]    = sIntermediate;
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
    
    mtdBlobType * sVal = (mtdBlobType*)aValue;
    
    IDE_TEST_RAISE( aValue == NULL, ERR_INVALID_NULL );
    
    IDE_TEST_RAISE((aValueSize < ID_SIZEOF(SLong)) ||
                   (sVal->length + ID_SIZEOF(SLong) != aValueSize),
                   ERR_INVALID_LENGTH );

    IDE_TEST_RAISE( sVal->length > aColumn->column.size, ERR_INVALID_VALUE );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdBlob,
                                     1,            // arguments
                                     sVal->length, // precision
                                     0 )           // scale
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
    IDE_EXCEPTION( ERR_INVALID_VALUE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  mtdStoredValue2MtdValue( UInt           /* aColumnSize */,
                                 void            * aDestValue,
                                 UInt              aDestValueOffset,
                                 UInt              aLength,
                                 const void      * aValue )
{
/***********************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 **********************************************************************/

    // LOB �÷��� ���ڵ忡 LOB�÷������ �����. 
    // �÷� ��ġ�ÿ��� ���ڵ忡 ����� LOB�÷������ �а�
    // ��������Ÿ�� �����ϴ� �κп��� �о�´�.

    IDE_TEST_RAISE( !(( aDestValueOffset == 0 ) && ( aLength > 0 )),
                    ERR_INVALID_STORED_VALUE );

    idlOS::memcpy( aDestValue, aValue, aLength );
    
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
/***********************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ    
 * �� ) mtdLobType( UInt length; UChar value[1] ) ����
 *      length Ÿ���� UInt�� ũ�⸦ ��ȯ
 **********************************************************************/

    return mtdActualSize( NULL,&mtdBlobNull );
}

static UInt mtdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * �� ) mtdLobType( UInt length; UChar value[1] ) ����
 *      length Ÿ���� UInt�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/

    return ID_SIZEOF(SLong);
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

static IDE_RC mtdCanonize( const mtcColumn  * aCanon,
                           void            ** aCanonizedValue,
                           mtcEncryptInfo   * /* aCanonInfo */,
                           const mtcColumn  * /* aColumn */,
                           void             * aValue,
                           mtcEncryptInfo   * /* aColumnInfo */,
                           mtcTemplate      * /* aTemplate */ )
{
    /* BUG-36429 LOB Column�� ���ؼ��� precision�� �˻����� �ʴ´�. */
    if ( aCanon->precision != 0 )
    {
        IDE_TEST_RAISE( ((mtdBlobType *)aValue)->length > (SLong)aCanon->precision,
                        ERR_INVALID_LENGTH );
    }
    else
    {
        /* Nothing to do */
    }

    *aCanonizedValue = aValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
