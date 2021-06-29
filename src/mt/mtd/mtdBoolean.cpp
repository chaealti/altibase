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
 * $Id: mtdBoolean.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtk.h>
#include <mtdTypes.h>

#define MTD_BOOLEAN_ALIGN   (ID_SIZEOF(mtdBooleanType))
#define MTD_BOOLEAN_SIZE    (ID_SIZEOF(mtdBooleanType))

extern mtdModule mtdBoolean;

static mtdBooleanType mtdBooleanNull = MTD_BOOLEAN_NULL;

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

static IDE_RC mtdIsTrue( idBool*          aResult,
                         const mtcColumn* aColumn,
                         const void*      aRow );

static SInt mtdBooleanMtdMtdKeyComp( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * aValueInfo2 );

static void mtdEndian( void* aValue );

static IDE_RC mtdValidate( mtcColumn * aColumn,
                           void      * aValue,
                           UInt        aValueSize);

static mtcName mtdTypeName[1] = {
    { NULL, 7, (void*)"BOOLEAN" },
};

static mtcColumn mtdColumn;

static UInt mtdBooleanHeaderSize();

static UInt mtdBooleanNullValueSize();

static IDE_RC mtdBooleanStoredValue2MtdValue(UInt         aColumnSize,
                                             void       * aDestValue,
                                             UInt      /* aDestValueOffset */,
                                             UInt         aLength,
                                             const void * aValue);

mtdModule mtdBoolean = {
    mtdTypeName,
    &mtdColumn,
    MTD_BOOLEAN_ID,
    0,
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    MTD_BOOLEAN_ALIGN,
    MTD_GROUP_MISC|
      MTD_CANON_NEEDLESS|
      MTD_CREATE_DISABLE|
      MTD_COLUMN_TYPE_FIXED|
      MTD_SELECTIVITY_DISABLE|
      MTD_SEARCHABLE_PRED_BASIC|
      MTD_UNSIGNED_ATTR_TRUE|
      MTD_NUM_PREC_RADIX_TRUE|
      MTD_VARIABLE_LENGTH_TYPE_FALSE| // PROJ-1705
      MTD_DATA_STORE_DIVISIBLE_FALSE| // PROJ-1705
      MTD_DATA_STORE_MTDVALUE_FALSE|  // PROJ-1705
      MTD_PSM_TYPE_ENABLE, // PROJ-1904
    1,
    0,
    0,
    &mtdBooleanNull,
    mtdInitialize,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtd::getPrecisionNA,
    mtdSetNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrue,
    {
        mtdBooleanMtdMtdKeyComp,           // Logical Comparison
        mtdBooleanMtdMtdKeyComp
    },
    {
        // Key Comparison (GROUP BY TRUE)
        {
            mtdBooleanMtdMtdKeyComp,         // Ascending Key Comparison
            mtdBooleanMtdMtdKeyComp          // Descending Key Comparison
        }
        ,
        {
            mtdBooleanMtdMtdKeyComp,         // Ascending Key Comparison
            mtdBooleanMtdMtdKeyComp          // Descending Key Comparison
        }
        ,
        {
            // ������� �ʴ� type�̹Ƿ� �� �κ����� ���ü� ����
            mtd::compareNA,
            mtd::compareNA
        }
        ,
        {
            // ������� �ʴ� type�̹Ƿ� �� �κ����� ���ü� ����
            mtd::compareNA,
            mtd::compareNA            
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� fixed mt value�� ���� compare */
            mtdBooleanMtdMtdKeyComp,
            mtdBooleanMtdMtdKeyComp
        }
        ,
        {
            /* PROJ-2433 : index Direct key�� mt value�� ���� compare */
            mtdBooleanMtdMtdKeyComp,
            mtdBooleanMtdMtdKeyComp
        }
    },
    mtd::canonizeDefault,
    mtdEndian,
    mtdValidate,
    mtd::selectivityNA,
    NULL,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtd::valueFromOracleDefault,
    mtd::makeColumnInfoDefault,

    // BUG-28934
    mtk::mergeAndRangeNA,
    mtk::mergeOrRangeListNA,

    {    
        // PROJ-1705
        mtdBooleanStoredValue2MtdValue, 
        // PROJ-2429
        NULL 
    }, 
    mtdBooleanNullValueSize,
    mtdBooleanHeaderSize,

    // PROJ-2399
    // header size�� 0������ default�Լ��� ���
    mtd::mtdStoreSizeDefault
};

IDE_RC mtdInitialize( UInt aNo )
{
    IDE_TEST( mtd::initializeModule( &mtdBoolean, aNo ) != IDE_SUCCESS );
    
    // IDE_TEST( mtdEstimate( &mtdColumn, 0, 0, 0 ) != IDE_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & mtdColumn,
                                     & mtdBoolean,
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
    IDE_TEST_RAISE( *aArguments != 0, ERR_INVALID_LENGTH );

    *aColumnSize = MTD_BOOLEAN_SIZE;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    
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
    UInt            sValueOffset;
    mtdBooleanType* sValue;
    const UChar*    sToken;
    
    sValueOffset = idlOS::align( *aValueOffset, MTD_BOOLEAN_ALIGN );
    
    *aResult = IDE_FAILURE;

    if( sValueOffset + MTD_BOOLEAN_SIZE <= aValueSize )
    {
        sValue = (mtdBooleanType*)( (UChar*)aValue + sValueOffset );
        sToken = (const UChar*)aToken;

        if( aTokenLength == 5                        &&
            ( sToken[0] == 'F' || sToken[0] == 'f' ) &&
            ( sToken[1] == 'A' || sToken[1] == 'a' ) &&
            ( sToken[2] == 'L' || sToken[2] == 'l' ) &&
            ( sToken[3] == 'S' || sToken[3] == 's' ) &&
            ( sToken[4] == 'E' || sToken[4] == 'e' )  )
        {
            *sValue  = MTD_BOOLEAN_FALSE;
            *aResult = IDE_SUCCESS;
        }
        else if( aTokenLength == 4                        &&
                 ( sToken[0] == 'T' || sToken[0] == 't' ) &&
                 ( sToken[1] == 'R' || sToken[1] == 'r' ) &&
                 ( sToken[2] == 'U' || sToken[2] == 'u' ) &&
                 ( sToken[3] == 'E' || sToken[3] == 'e' )  )
        {
            *sValue  = MTD_BOOLEAN_TRUE;
            *aResult = IDE_SUCCESS;
        }
        else if( ( aTokenLength == 0 )                      ||
                 ( aTokenLength == 4                        &&
                   ( sToken[0] == 'N' || sToken[0] == 'n' ) &&
                   ( sToken[1] == 'U' || sToken[1] == 'u' ) &&
                   ( sToken[2] == 'L' || sToken[2] == 'l' ) &&
                   ( sToken[3] == 'L' || sToken[3] == 'l' ) ) )
        {
            *sValue  = MTD_BOOLEAN_NULL;
            *aResult = IDE_SUCCESS;
        }
        else
        {
            IDE_RAISE( ERR_INVALID_LITERAL );
        }
        if( *aResult == IDE_SUCCESS )
        {   
            aColumn->column.offset   = sValueOffset;
            *aValueOffset            = sValueOffset + MTD_BOOLEAN_SIZE;
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

UInt mtdActualSize( const mtcColumn*,
                    const void* )
{
    return MTD_BOOLEAN_SIZE;
}

void mtdSetNull( const mtcColumn* /* aColumn */,
                 void*            aRow )
{
    if( aRow != NULL)
    {
        *(mtdBooleanType*)aRow = MTD_BOOLEAN_NULL;
    }
}

UInt mtdHash( UInt             aHash,
              const mtcColumn* ,
              const void*      aRow )
{
    return mtc::hash( aHash, (const UChar*)aRow, MTD_BOOLEAN_SIZE );
}

idBool mtdIsNull( const mtcColumn* ,
                  const void*      aRow )
{
    return (*(mtdBooleanType*)aRow == MTD_BOOLEAN_NULL) ? ID_TRUE : ID_FALSE ;
}

IDE_RC mtdIsTrue( idBool*          aResult,
                  const mtcColumn* ,
                  const void*      aRow )
{
    *aResult = (*(mtdBooleanType*)aRow == MTD_BOOLEAN_TRUE) ? ID_TRUE : ID_FALSE ;

    return IDE_SUCCESS;
}

SInt mtdBooleanMtdMtdKeyComp( mtdValueInfo * aValueInfo1,
                              mtdValueInfo * aValueInfo2 )
{
    const mtdBooleanType * sValue1;
    const mtdBooleanType * sValue2;

    //---------
    // value1
    //---------
    sValue1 = (const mtdBooleanType*)
                mtd::valueForModule( (smiColumn*)aValueInfo1->column, 
                                     aValueInfo1->value, 
                                     aValueInfo1->flag,
                                     mtdBoolean.staticNull );

    //---------
    // value2
    //---------
    sValue2 = (const mtdBooleanType*)
                mtd::valueForModule( (smiColumn*)aValueInfo2->column, 
                                     aValueInfo2->value, 
                                     aValueInfo2->flag,
                                     mtdBoolean.staticNull );

    //---------
    // compare
    //---------
    
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

void mtdEndian( void* )
{
    return;
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
        
    mtdBooleanType * sVal = (mtdBooleanType*)aValue;
    IDE_TEST_RAISE( sVal == NULL, ERR_INVALID_NULL );
    
    IDE_TEST_RAISE( aValueSize != ID_SIZEOF(mtdBooleanType),
                    ERR_INVALID_LENGTH);

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    IDE_TEST( mtc::initializeColumn( aColumn,
                                     & mtdBoolean,
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

static UInt mtdBooleanHeaderSize()
{
    return 0;
}

static UInt mtdBooleanNullValueSize()
{
    return 0;
}

static IDE_RC mtdBooleanStoredValue2MtdValue(UInt         aColumnSize,
                                             void       * aDestValue,
                                             UInt      /* aDestValueOffset */,
                                             UInt         aLength,
                                             const void * aValue)
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdBooleanType* sValue;

    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����.

    sValue = (mtdBooleanType*)aDestValue;

    if( aLength == 0 )
    {
        // NULL ����Ÿ
        *sValue = mtdBooleanNull;
    }
    else
    {
        IDE_TEST_RAISE( aLength != aColumnSize, ERR_INVALID_STORED_VALUE );

        *sValue = *(mtdBooleanType*)aValue;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
 
