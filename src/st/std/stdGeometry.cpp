/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: stdGeometry.cpp 18883 2006-11-14 01:48:40Z sabbra $
 *
 * Description: stdGeometry ��� ����
 **********************************************************************/

#include <ste.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>

#include <stdTypes.h>
#include <stdUtils.h>
#include <stdParsing.h>
#include <stdPrimitive.h>
#include <stfBasic.h>
#include <stfRelation.h>
#include <stdGeometry.h>
#include <stm.h>
#include <stix.h>
#include <stcDef.h>
#include <stk.h>
#include <stuProperty.h>

extern mtdModule mtdDouble;

static mtcColumn stdColumn;

stdGeometryHeader stdGeometryNull;
stdGeometryHeader stdGeometryEmpty;

static IDE_RC stdEncode( mtcColumn  * aColumn,
                         void       * aValue,
                         UInt         aValueSize,
                         UChar      * aCompileFmt,
                         UInt         aCompileFmtLen,
                         UChar      * aText,
                         UInt       * aTextLen,
                         IDE_RC     * aRet );

static UInt stdStoreSize( const smiColumn * aColumn );

mtdModule stdGeometry = {
    stdTypeName,
    &stdColumn,
    STD_GEOMETRY_ID,
    0,
    { SMI_ADDITIONAL_RTREE_INDEXTYPE_ID, 0,
//      STI_ADDITIONAL_3DRTREE_INDEXTYPE_ID,
      0, 0, 0, 0, 0, 0 },
    STD_GEOMETRY_ALIGN,
    MTD_GROUP_MISC|
    MTD_CANON_NEED|
    MTD_CREATE_ENABLE|
    MTD_COLUMN_TYPE_VARIABLE|
    MTD_SELECTIVITY_ENABLE|
    MTD_CREATE_PARAM_PRECISION|
    MTD_SEARCHABLE_PRED_BASIC|
    MTD_VARIABLE_LENGTH_TYPE_TRUE|  // PROJ-1705
    MTD_DATA_STORE_DIVISIBLE_TRUE|  // PROJ-1705
    MTD_DATA_STORE_MTDVALUE_TRUE|   // PROJ-1705
    MTD_PSM_TYPE_ENABLE,            // PROJ-1904
    STD_GEOMETRY_STORE_PRECISION_MAXIMUM,  // BUG-28921
    0,
    0,      
    &stdGeometryNull,
    stdInitialize,
    stdEstimate,
    stdValue,
    stdActualSize,
    stdGetPrecision,
    stdNull,
    stdHash,
    stdIsNull,
    mtd::isTrueNA,
    {
        mtd::compareNA,
        mtd::compareNA
    },
    {                         // Key Comparison
        {
            stdGeometryKeyComp,     // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            stdGeometryKeyComp,     // Ascending Key Comparison
            mtd::compareNA          // Descending Key Comparison
        }
        ,
        {
            mtd::compareNA,
            mtd::compareNA
        }
        ,
        {
            mtd::compareNA,
            mtd::compareNA
        }
    },
    stdCanonize,
    stdEndian,
    mtd::validateDefault,
    mtd::selectivityDefault,
    stdEncode,
    mtd::decodeDefault,
    mtd::compileFmtDefault,
    mtd::valueFromOracleDefault,
    stm::makeAndSetStmColumnInfo,

    // BUG-28934
    stk::mergeAndRange,
    stk::mergeOrRangeList,
    
    {
        // PROJ-1705
        stdStoredValue2StdValue,
        // PROJ-2429
        NULL
    },
    stdNullValueSize,
    stdHeaderSize,

    // PROJ-2399
    stdStoreSize
};
 
static IDE_RC stdInitialize( UInt aNo )
{
    IDE_TEST( mtd::initializeModule( &stdGeometry, aNo )
              != IDE_SUCCESS );
    
    // IDE_TEST( mtdEstimate( &stdColumn, 0, 0, 0 ) != IDE_SUCCESS );
    // stdColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn( & stdColumn,
                                     & stdGeometry,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );
              
    // Fix BUG-15412 mtdModule.isNull ���
    stdUtils::setType( & stdGeometryNull, STD_NULL_TYPE );
    stdGeometryNull.mSize = ID_SIZEOF(stdGeometryHeader);
    mtdDouble.null( NULL, &stdGeometryNull.mMbr.mMinX );
    mtdDouble.null( NULL, &stdGeometryNull.mMbr.mMinY );
    mtdDouble.null( NULL, &stdGeometryNull.mMbr.mMaxX );
    mtdDouble.null( NULL, &stdGeometryNull.mMbr.mMaxY );

    stdUtils::setType( & stdGeometryEmpty, STD_EMPTY_TYPE );
    stdGeometryEmpty.mSize = ID_SIZEOF(stdGeometryHeader);
    mtdDouble.null( NULL, &stdGeometryEmpty.mMbr.mMinX );
    mtdDouble.null( NULL, &stdGeometryEmpty.mMbr.mMinY );
    mtdDouble.null( NULL, &stdGeometryEmpty.mMbr.mMaxX );
    mtdDouble.null( NULL, &stdGeometryEmpty.mMbr.mMaxY );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/*���� ������ Ư���� ���� �����Ͱ� Ȯ������ �ʴ� �̻� �̸� �� ũ�⸦ 
������ �� �����Ƿ� �� �Լ��� ���� �뵵�� �����ؾ� �� ��*/
static IDE_RC stdEstimate( UInt * aColumnSize,
                           UInt * aArguments,
                           SInt * aPrecision,
                           SInt * /*aScale*/ )
{
    // To Fix BUG-15365
    // GEOMETRY Ÿ�� ����� Precision�� ������ �� �ֵ��� ��.
    switch ( *aArguments )
    {
        case 0:
            // BUGBUG ���� BLOB ������ ó���Ǿ�� ��
            *aPrecision = STD_GEOMETRY_PRECISION_DEFAULT;
            break;
        case 1:
            // Nothing To Do
            break;
        default:
            IDE_RAISE( ERR_INVALID_SCALE );
    }

    IDE_TEST_RAISE( ( *aPrecision < (SInt)STD_GEOMETRY_PRECISION_MINIMUM ) ||
                    ( *aPrecision > (SInt)STD_GEOMETRY_PRECISION_MAXIMUM ),
                    ERR_INVALID_LENGTH );

    // BUG-16031 Precisiion �� ������ Header�� ������ ũ��� ����
    *aColumnSize = *aPrecision + ID_SIZEOF(stdGeometryHeader);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_LENGTH));
    }

    IDE_EXCEPTION( ERR_INVALID_SCALE );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_SCALE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC stdValue( mtcTemplate* aTemplate,
                        mtcColumn*   aColumn,
                        void *       aValue,
                        UInt*        aValueOffset,
                        UInt         aValueSize,
                        const void*  aToken,
                        UInt         aTokenLength,
                        IDE_RC*      aResult )
{
    UInt               sValueOffset;
    stdGeometryHeader* sValue;
    qcTemplate       * sQcTmplate;
    iduMemory        * sQmxMem;
    iduMemoryStatus    sQmxMemStatus;
    UInt               sStage = 0;
    
    sValueOffset = idlOS::align( *aValueOffset, STD_GEOMETRY_ALIGN );
    sValue = (stdGeometryHeader*)( (UChar*)aValue + sValueOffset );
    *aResult = IDE_SUCCESS;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
      
    // Memory ������ ���Ͽ� ���� ��ġ ���
    IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
    sStage = 1;
    
    IDE_TEST( stdParsing::stdValue( sQmxMem,
                                    (UChar*)aToken, // WellKnownText
                                    aTokenLength,
                                    sValue,         // Geometry Buffer
                                    (((SChar*)aValue) + aValueSize), //  Fence
                                    aResult,        // Memory Overflow Flag
                                    STU_VALIDATION_ENABLE,
                                    ID_TRUE,        // enable srid
                                    ST_SRID_INIT )
              != IDE_SUCCESS );

    // Memory ������ ���� Memory �̵�
    sStage = 0;
    IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    
    if( *aResult == IDE_SUCCESS )
    {        
        aColumn->column.offset   = sValueOffset;
        aColumn->column.size     = ID_SIZEOF(stdGeometryHeader) + sValue->mSize;
        
        *aValueOffset            = sValueOffset + aColumn->column.size;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
        
    return IDE_FAILURE;    
}

static UInt stdActualSize( const mtcColumn* ,
                           const void*      aRow )
{
    // To Fix BUG-15365
    // sValue->size�� ��ü Size�� �ǹ���.
    // �߸��� Value ũ���� ��������
    // GEOMETRY(72) �� Data�� ������ �� ����. 
    // return ID_SIZEOF(stdGeometryHeader) + sValue->mSize;
    return ((stdGeometryHeader*)aRow)->mSize;
}

static IDE_RC stdGetPrecision( const mtcColumn * ,
                               const void      * aRow,
                               SInt            * aPrecision,
                               SInt            * aScale )
{
    *aPrecision = ((stdGeometryHeader*)aRow)->mSize;
    *aScale = 0;

    return IDE_SUCCESS;
}

static void stdNull( const mtcColumn* ,
                     void*            aRow )
{
    if( aRow != NULL )
    {
        (void)stdUtils::nullify(((stdGeometryHeader*)aRow));
    }
}

static UInt stdHash( UInt             aHash,
                     const mtcColumn* ,
                     const void*      aRow )
{
    return mtc::hash( aHash, (UChar*)&((stdGeometryHeader*)aRow)->mMbr, ID_SIZEOF(stdMBR) );
}

static idBool stdIsNull( const mtcColumn* ,
                         const void*      aRow )
{
    return (stdUtils::isNull(((stdGeometryHeader*)aRow)) == ID_TRUE) ? ID_TRUE : ID_FALSE;
}

static SInt stdGeometryKeyComp( mtdValueInfo * aValueInfo1,
                                mtdValueInfo * aValueInfo2 )
{
    stdGeometryHeader*    sValue1;
    stdGeometryHeader*    sValue2;
    
    sValue1 = (stdGeometryHeader*)
        mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                             aValueInfo1->value,
                             aValueInfo1->flag,
                             stdGeometry.staticNull );

    sValue2 = (stdGeometryHeader*)
        mtd::valueForModule( (smiColumn*)aValueInfo2->column,
                             aValueInfo2->value,
                             aValueInfo2->flag,
                             stdGeometry.staticNull );
                                    
    if ( stdUtils::isMBRWithin( &sValue1->mMbr, &sValue2->mMbr ) == ID_TRUE)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static IDE_RC stdCanonize( const mtcColumn * aCanon,
                           void**            aCanonized,
                           mtcEncryptInfo  * /* aCanonInfo */,
                           const mtcColumn * /* aColumn */,
                           void*             aValue,
                           mtcEncryptInfo  * /* aColumnInfo */,
                           mtcTemplate     * /* aTemplate */ )
{
    stdGeometryHeader  * sValue = NULL;
    SInt                 sSRID = 0;
    
    sValue = (stdGeometryHeader*)aValue;
    
    IDE_TEST_RAISE( sValue->mSize > (UInt)aCanon->column.size,
                    ERR_INVALID_LENGTH );

    /* PROJ-2422 srid */
    if ( aCanon->mColumnAttr.mSridAttr.mSrid != ST_SRID_UNDEFINED )
    {
        sSRID = stfBasic::getSRID( sValue );

        if ( sSRID != ST_SRID_UNDEFINED )
        {
            IDE_TEST_RAISE( aCanon->mColumnAttr.mSridAttr.mSrid != sSRID,
                            ERR_INVALID_SRID );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
    
    *aCanonized = aValue;
     
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    IDE_SET( ideSetErrorCode( stERR_ABORT_INVALID_LENGTH ) );
    
    IDE_EXCEPTION( ERR_INVALID_SRID );
    IDE_SET( ideSetErrorCode(stERR_ABORT_INVALID_SRID ) );
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

static void stdEndian( void* aValue )
{
    (void) stdPrimitive::cvtEndianGeom( (stdGeometryHeader *) aValue);
}

static IDE_RC stdStoredValue2StdValue( UInt              aColumnSize,
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

    stdGeometryHeader*    sValue;
    
    sValue = (stdGeometryHeader*)aDestValue;    
                   
    if( ( aDestValueOffset == 0 ) && ( aLength == 0 ) )
    {
        // NULL ����Ÿ
        (void)stdUtils::nullify(sValue);        
    }
    else
    {
        // stdGeometry type�� mtdDataType���·� ����ǹǷ�
        // length �� header size�� ���ԵǾ� �־� 
        // ���⼭ ������ ����� ���� �ʾƵ� �ȴ�.         
        IDE_TEST_RAISE( (aDestValueOffset + aLength) > aColumnSize, ERR_INVALID_STORED_VALUE );

        idlOS::memcpy( (UChar*) sValue + aDestValueOffset, aValue, aLength );
        // set current size to header(for stdGeometry::actualSize)
        sValue->mSize = aDestValueOffset + aLength;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_STORED_DATA_LENGTH));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

static UInt stdNullValueSize()
{
/*******************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ    
 *******************************************************************/
    return stdActualSize( NULL, stdGeometry.staticNull );
}

static UInt stdHeaderSize()
{
/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 **********************************************************************/

    return ID_SIZEOF(stdGeometryHeader);    
}


// TASK-3171 B-Tree spatial

static IDE_RC stdEncode( mtcColumn  * /* aColumn */,
                         void       * aValue,
                         UInt         /* aValueSize */,
                         UChar      * /* aCompileFmt */,
                         UInt         /* aCompileFmtLen */,
                         UChar      * aText,
                         UInt       * aTextLen,
                         IDE_RC     * aRet )
{
    UInt sStringLen;

    stdMBR *sValue;
    //----------------------------------
    // Parameter Validation
    //----------------------------------

    IDE_DASSERT( aValue != NULL );
    IDE_DASSERT( aText != NULL );
    IDE_DASSERT( *aTextLen > 0 );
    IDE_DASSERT( aRet != NULL );
    
    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;

    //----------------------------------
    // Set String
    //----------------------------------

    sValue = (stdMBR*) aValue;
    
    // To Fix BUG-16801
    if ( stdIsNull( NULL, aValue ) == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sStringLen = idlVA::appendFormat( (SChar*) aText, 
                                          *aTextLen,
                                          "%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT,
                                          sValue->mMinX, sValue->mMinY, sValue->mMaxX, sValue->mMaxY);
    }

    //----------------------------------
    // Finalization
    //----------------------------------
    
    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    *aRet = IDE_SUCCESS;

    return IDE_SUCCESS;
}

static UInt stdStoreSize( const smiColumn * /*aColumn*/ )
{
/***********************************************************************
* PROJ-2399 row tmaplate
* sm�� ����Ǵ� �������� ũ�⸦ ��ȯ�Ѵ�.
* varchar�� ���� variable Ÿ���� ������ Ÿ���� ID_UINT_MAX�� ��ȯ
* mtheader�� sm�� ����Ȱ�찡 �ƴϸ� mtheaderũ�⸦ ���� ��ȯ
 **********************************************************************/

    return ID_UINT_MAX;
}
