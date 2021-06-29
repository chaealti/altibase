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
 * $Id: mtd.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtd.h>
#include <mtl.h>
#include <mtc.h>
#include <mtv.h>
#include <ida.h>
#include <mtdTypes.h>
#include <mtuProperty.h>
#include <mtlCollate.h>
#include <smi.h>

extern mtdModule mtdBigint;
extern mtdModule mtdBinary;
extern mtdModule mtdBit;
extern mtdModule mtdVarbit;
extern mtdModule mtdBlob;
extern mtdModule mtdBoolean;
extern mtdModule mtdChar;
extern mtdModule mtdDate;
extern mtdModule mtdDouble;
extern mtdModule mtdFloat;
extern mtdModule mtdInteger;
extern mtdModule mtdInterval;
extern mtdModule mtdList;
extern mtdModule mtdNull;
extern mtdModule mtdNumber;
extern mtdModule mtdNumeric;
extern mtdModule mtdReal;
extern mtdModule mtdSmallint;
extern mtdModule mtdVarchar;
extern mtdModule mtdByte;
extern mtdModule mtdNibble;
extern mtdModule mtdClob;
extern mtdModule mtsFile;
extern mtdModule mtdBlobLocator;  // PROJ-1362
extern mtdModule mtdClobLocator;  // PROJ-1362
extern mtdModule mtdNchar;        // PROJ-1579 NCHAR
extern mtdModule mtdNvarchar;     // PROJ-1579 NCHAR
extern mtdModule mtdEchar;        // PROJ-2002 Column Security
extern mtdModule mtdEvarchar;     // PROJ-2002 Column Security
extern mtdModule mtdUndef;        // PROJ-2163 ���ε� ������
extern mtdModule mtdVarbyte;      // BUG-40973
extern mtdModule mtsConnect;      // BUG-40854

const mtlModule* mtl::mDBCharSet;
const mtlModule* mtl::mNationalCharSet;

mtdModule * mtd::mAllModule[MTD_MAX_DATATYPE_CNT];
const mtdModule* mtd::mInternalModule[] = {
    &mtdBigint,
    &mtdBit,
    &mtdVarbit,
    &mtdBlob,
    &mtdBoolean,
    &mtdChar,
    &mtdDate,
    &mtdDouble,
    &mtdFloat,
    &mtdInteger,
    &mtdInterval,
    &mtdList,
    &mtdNull,
    &mtdNumber,
    &mtdNumeric,
    &mtdReal,
    &mtdSmallint,
    &mtdVarchar,
    &mtdByte,
    &mtdNibble,
    &mtdClob,
    &mtsFile,
    &mtdBlobLocator,  // PROJ-1362
    &mtdClobLocator,  // PROJ-1362
    &mtdBinary,       // PROJ-1583, PR-15722
    &mtdNchar,        // PROJ-1579 NCHAR
    &mtdNvarchar,     // PROJ-1579 NCHAR
    &mtdEchar,        // PROJ-2002 Column Security
    &mtdEvarchar,     // PROJ-2002 Column Security
    &mtdUndef,        // PROJ-2163 ���ε� ������
    &mtdVarbyte,      // BUG-40973
    &mtsConnect,      // BUG-40854
    NULL
};

const mtdCompareFunc mtd::compareNumberGroupBigintFuncs[MTD_COMPARE_FUNC_MAX_CNT][2] = {
    {
        mtd::compareNumberGroupBigintMtdMtdAsc,
        mtd::compareNumberGroupBigintMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupBigintMtdMtdAsc,
        mtd::compareNumberGroupBigintMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupBigintStoredMtdAsc,
        mtd::compareNumberGroupBigintStoredMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupBigintStoredStoredAsc,
        mtd::compareNumberGroupBigintStoredStoredDesc,
    }    
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL */
        mtd::compareNumberGroupBigintMtdMtdAsc,
        mtd::compareNumberGroupBigintMtdMtdDesc, 
    }
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_MTDVAL */
        mtd::compareNumberGroupBigintMtdMtdAsc,
        mtd::compareNumberGroupBigintMtdMtdDesc,
    }
};

const mtdCompareFunc mtd::compareNumberGroupDoubleFuncs[MTD_COMPARE_FUNC_MAX_CNT][2] = {
    {
        mtd::compareNumberGroupDoubleMtdMtdAsc,
        mtd::compareNumberGroupDoubleMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupDoubleMtdMtdAsc,
        mtd::compareNumberGroupDoubleMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupDoubleStoredMtdAsc,
        mtd::compareNumberGroupDoubleStoredMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupDoubleStoredStoredAsc,
        mtd::compareNumberGroupDoubleStoredStoredDesc,
    }    
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL */
        mtd::compareNumberGroupDoubleMtdMtdAsc,
        mtd::compareNumberGroupDoubleMtdMtdDesc,
    }
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_MTDVAL */
        mtd::compareNumberGroupDoubleMtdMtdAsc,
        mtd::compareNumberGroupDoubleMtdMtdDesc,
    }
};

const mtdCompareFunc mtd::compareNumberGroupNumericFuncs[MTD_COMPARE_FUNC_MAX_CNT][2] = {
    {
        mtd::compareNumberGroupNumericMtdMtdAsc,
        mtd::compareNumberGroupNumericMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupNumericMtdMtdAsc,
        mtd::compareNumberGroupNumericMtdMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupNumericStoredMtdAsc,
        mtd::compareNumberGroupNumericStoredMtdDesc,
    }
    ,
    {
        mtd::compareNumberGroupNumericStoredStoredAsc,
        mtd::compareNumberGroupNumericStoredStoredDesc,
    }    
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL */
        mtd::compareNumberGroupNumericMtdMtdAsc,
        mtd::compareNumberGroupNumericMtdMtdDesc,
    }
    ,
    {
        /* PROJ-2433 : MTD_COMPARE_INDEX_KEY_MTDVAL */
        mtd::compareNumberGroupNumericMtdMtdAsc,
        mtd::compareNumberGroupNumericMtdMtdDesc,
    }
};

typedef struct mtdIdIndex {
    const mtdModule* module;
} mtdIdIndex;

typedef struct mtdNameIndex {
    const mtcName*   name;
    const mtdModule* module;
} mtdNameIndex;

static UInt          mtdNumberOfModules;

static mtdIdIndex*   mtdModulesById = NULL;

static UInt          mtdNumberOfModulesByName;

static mtdNameIndex* mtdModulesByName = NULL;

// PROJ-1364
// ������ �迭�� �񱳽� �Ʒ� conversion matrix�� ���Ͽ�
// �񱳱��� data type�� ��������.
// ------------------------------------------
//        |  N/A   | ������ | �Ǽ��� | ������
// ------------------------------------------
//   N/A  |  N/A   |  N/A   |  N/A   |  N/A
// ------------------------------------------
// ������ |  N/A   | ������ | �Ǽ��� | ������
// ------------------------------------------
// �Ǽ��� |  N/A   | �Ǽ��� | �Ǽ��� | �Ǽ���
// ------------------------------------------
// ������ |  N/A   | ������ | �Ǽ��� | ������
// ------------------------------------------
const UInt mtd::comparisonNumberType[4][4] = {
    { MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_NONE },
    { MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_BIGINT,     // ������
      MTD_NUMBER_GROUP_TYPE_DOUBLE,     // �Ǽ���
      MTD_NUMBER_GROUP_TYPE_NUMERIC },  // ������
    { MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_DOUBLE,     // �Ǽ���
      MTD_NUMBER_GROUP_TYPE_DOUBLE,     // �Ǽ���
      MTD_NUMBER_GROUP_TYPE_DOUBLE },   // �Ǽ���
    { MTD_NUMBER_GROUP_TYPE_NONE,
      MTD_NUMBER_GROUP_TYPE_NUMERIC,    // ������
      MTD_NUMBER_GROUP_TYPE_DOUBLE,     // �Ǽ���
      MTD_NUMBER_GROUP_TYPE_NUMERIC }   // ������
};


// To Remove Warning
/*
  static int mtdCompareById( const mtdIdIndex* aModule1,
  const mtdIdIndex* aModule2 )
  {
  if( aModule1->module->id.type > aModule2->module->id.type )
  {
  return 1;
  }
  if( aModule1->module->id.type < aModule2->module->id.type )
  {
  return -1;
  }
  if( aModule1->module->id.language > aModule2->module->id.language )
  {
  return 1;
  }
  if( aModule1->module->id.language < aModule2->module->id.language )
  {
  return -1;
  }

  return 0;
  }
*/


static int mtdCompareByName( const mtdNameIndex* aModule1,
                             const mtdNameIndex* aModule2 )
{
    int sOrder;

    sOrder = idlOS::strCompare( aModule1->name->string,
                                aModule1->name->length,
                                aModule2->name->string,
                                aModule2->name->length );
    return sOrder;
}

UInt mtd::getNumberOfModules( void )
{
    return mtdNumberOfModules;
}

IDE_RC mtd::isTrueNA( idBool*,
                      const mtcColumn*,
                      const void* )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));

    return IDE_FAILURE;
}

SInt mtd::compareNA( mtdValueInfo * /* aValueInfo1 */,
                     mtdValueInfo * /* aValueInfo2 */ )
{
    IDE_ASSERT( 0 );

    return 0;
}

IDE_RC mtd::canonizeDefault( const mtcColumn  *  /* aCanon */,
                             void             ** aCanonized,
                             mtcEncryptInfo   *  /* aCanonInfo */,
                             const mtcColumn  *  /* aColumn */,
                             void             *  aValue,
                             mtcEncryptInfo   *  /* aColumnInfo */,
                             mtcTemplate      *  /* aTemplate */ )
{
    *aCanonized = aValue;

    return IDE_SUCCESS;
}

IDE_RC mtd::initializeModule( mtdModule* aModule,
                              UInt       aNo )
{
/***********************************************************************
 *
 * Description : mtd module�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    aModule->no = aNo;

    return IDE_SUCCESS;

    //IDE_EXCEPTION_END;

    //return IDE_FAILURE;
}

IDE_RC mtd::initialize( mtdModule *** aExtTypeModuleGroup,
                        UInt          aGroupCnt )
{
/***********************************************************************
 *
 * Description : mtd initialize
 *
 * Implementation : data type ���� ����
 *
 *   mtdModulesById
 *    --------------------------
 *   | mtdIdIndex     | module -|---> mtdBigInt
 *    --------------------------
 *   | mtdIdIndex     | module -|---> mtdBlob
 *    --------------------------
 *   |      ...                 | ...
 *    --------------------------
 *
 *   mtdModuleByName
 *    ------------------------
 *   | mtdNameIndex | name   -|---> BIGINT
 *   |              | module -|---> mtdBigInt
 *    ------------------------
 *   | mtdNameIndex | name   -|---> BLOB
 *   |              | module -|---> mtdBlob
 *    ------------------------
 *   |      ...               | ...
 *    ------------------------
 *
 ***********************************************************************/

    UInt              i;
    UInt              sStage = 0;
    SInt              sModuleIndex = 0;

    mtdModule**       sModule;
    const mtcName*    sName;

    //---------------------------------------------------------
    // ���� mtdModule�� ����, mtd module name�� ���� mtdModule ���� ����
    // mtdModule�� �Ѱ� �̻��� �̸��� ���� �� ����
    //     ex ) mtdNumeric :  NUMERIC�� DECIMAL �� ���� name�� ����
    //          mtdVarchar : VARCHAR�� VARCHAR2 �� ���� name�� ����
    //---------------------------------------------------------

    mtdNumberOfModules       = 0;
    mtdNumberOfModulesByName = 0;

    //---------------------------------------------------------------
    // ���� Data Type�� �ʱ�ȭ
    //---------------------------------------------------------------

    for( sModule = (mtdModule**) mInternalModule; *sModule != NULL; sModule++ )
    {
        mAllModule[mtdNumberOfModules] = *sModule;

        (*sModule)->initialize( mtdNumberOfModules );
        mtdNumberOfModules++;

        IDE_ASSERT_MSG( mtdNumberOfModules < MTD_MAX_DATATYPE_CNT,
                        "mtdNumberOfModules : %"ID_UINT32_FMT"\n",
                        mtdNumberOfModules );

        for( sName = (*sModule)->names; sName != NULL; sName = sName->next )
        {
            mtdNumberOfModulesByName++;
        }
    }

    //---------------------------------------------------------------
    // �ܺ� Data Type�� �ʱ�ȭ
    //---------------------------------------------------------------

    for ( i = 0; i < aGroupCnt; i++ )
    {
        for( sModule   = aExtTypeModuleGroup[i];
             *sModule != NULL;
             sModule++ )
        {
            mAllModule[mtdNumberOfModules] = *sModule;
            (*sModule)->initialize( mtdNumberOfModules );
            mtdNumberOfModules++;
            IDE_ASSERT_MSG( mtdNumberOfModules < MTD_MAX_DATATYPE_CNT,
                            "mtdNumberOfModules : %"ID_UINT32_FMT"\n",
                            mtdNumberOfModules );

            for( sName = (*sModule)->names;
                 sName != NULL;
                 sName = sName->next )
            {
                mtdNumberOfModulesByName++;
            }
        }
    }

    mAllModule[mtdNumberOfModules] = NULL;

    //---------------------------------------------------------
    // mtdModulesById �� mtdModulesByName�� ����
    //---------------------------------------------------------

    // BUG-34209
    // aix������ calloc�� ������ �� �־� malloc�� ����Ѵ�.
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtdIdIndex) * 128,
                               (void**)&mtdModulesById)
             != IDE_SUCCESS);
    idlOS::memset( (void*)mtdModulesById,
                   0x00,
                   ID_SIZEOF(mtdIdIndex) * 128 );
    sStage = 1;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtdNameIndex) *
                               mtdNumberOfModulesByName,
                               (void**)&mtdModulesByName)
             != IDE_SUCCESS);
    sStage = 2;

    mtdNumberOfModulesByName = 0;
    for( sModule = (mtdModule**) mAllModule; *sModule != NULL; sModule++ )
    {
        // mtdModuleById ����
        sModuleIndex = ( (*sModule)->id & 0x003F );
        mtdModulesById[sModuleIndex].module = *sModule;

        // mtdModuleByName ����
        for( sName = (*sModule)->names; sName != NULL; sName = sName->next )
        {
            mtdModulesByName[mtdNumberOfModulesByName].name   = sName;
            mtdModulesByName[mtdNumberOfModulesByName].module = *sModule;
            mtdNumberOfModulesByName++;
        }
    }

    idlOS::qsort( mtdModulesByName, mtdNumberOfModulesByName,
                  ID_SIZEOF(mtdNameIndex),(PDL_COMPARE_FUNC)mtdCompareByName );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 2:
            (void)iduMemMgr::free(mtdModulesByName);
            mtdModulesByName = NULL;
        case 1:
            (void)iduMemMgr::free(mtdModulesById);
            mtdModulesById = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC mtd::finalize( void )
{
/***********************************************************************
 *
 * Description : mtd finalize
 *
 * Implementation : data type ���� ����� �޸� ���� ����
 *
 ***********************************************************************/

    if( mtdModulesByName != NULL )
    {
        IDE_TEST(iduMemMgr::free(mtdModulesByName)
                 != IDE_SUCCESS);

        mtdModulesByName = NULL;
    }
    if( mtdModulesById != NULL )
    {
        IDE_TEST(iduMemMgr::free(mtdModulesById)
                 != IDE_SUCCESS);

        mtdModulesById = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::moduleByName( const mtdModule** aModule,
                          const void*       aName,
                          UInt              aLength )
{
/***********************************************************************
 *
 * Description : data type name���� mtd module �˻�
 *
 * Implementation :
 *    mtdModuleByName���� �ش� data type name�� ������ mtd module�� ã����
 *
 ***********************************************************************/

    mtdNameIndex sKey;
    mtdModule    sModule;
    mtcName      sName;
    SInt         sMinimum;
    SInt         sMaximum;
    SInt         sMedium;
    SInt         sFound;

    sName.length        = aLength;
    sName.string        = (void*)aName;
    sKey.name           = &sName;
    sKey.module         = &sModule;

    sMinimum = 0;
    sMaximum = mtdNumberOfModulesByName - 1;

    do{
        sMedium = (sMinimum + sMaximum) >> 1;
        if( mtdCompareByName( mtdModulesByName + sMedium, &sKey ) >= 0 )
        {
            sMaximum = sMedium - 1;
            sFound   = sMedium;
        }
        else
        {
            sMinimum = sMedium + 1;
            sFound   = sMinimum;
        }
    }while( sMinimum <= sMaximum );

    IDE_TEST_RAISE( sFound >= (SInt)mtdNumberOfModulesByName, ERR_NOT_FOUND );

    *aModule = mtdModulesByName[sFound].module;

    // PROJ-2163 : ���ο����� ���̴� Ÿ��
    IDE_TEST_RAISE( ( (*aModule)->flag & MTD_INTERNAL_TYPE_MASK )
                    == MTD_INTERNAL_TYPE_TRUE,
                    ERR_NOT_FOUND );

    IDE_TEST_RAISE( idlOS::strMatch( mtdModulesByName[sFound].name->string,
                                     mtdModulesByName[sFound].name->length,
                                     aName,
                                     aLength ) != 0,
                    ERR_NOT_FOUND );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        char sNameBuffer[256];

        if( aLength >= ID_SIZEOF(sNameBuffer) )
        {
            aLength = ID_SIZEOF(sNameBuffer) - 1;
        }
        idlOS::memcpy( sNameBuffer, aName, aLength );
        sNameBuffer[aLength] = '\0';
        // idlOS::sprintf( sBuffer,
        //                "(Name=\"%s\")",
        //                sNameBuffer );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "(Name=\"%s\")",
                         sNameBuffer );

        IDE_SET(ideSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                                sBuffer));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::moduleById( const mtdModule** aModule,
                        UInt              aId )
{
/***********************************************************************
 *
 * Description : data type���� mtd module �˻�
 *
 * Implementation :
 *    mtdModuleById���� �ش� data type�� ������ mtd module�� ã����
 *
 ***********************************************************************/

    SInt       sFound;

    sFound = ( aId & 0x003F );

    *aModule = mtdModulesById[sFound].module;

    IDE_TEST_RAISE(*aModule == NULL, ERR_NOT_FOUND);
    IDE_TEST_RAISE( (*aModule)->id != aId, ERR_NOT_FOUND );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        // idlOS::sprintf( sBuffer, "(type=%"ID_UINT32_FMT")", aId );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "(type=%"ID_UINT32_FMT")", aId );
        IDE_SET(ideSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                                sBuffer));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::moduleByNo( const mtdModule** aModule,
                        UInt              aNo )
{
/***********************************************************************
 *
 * Description : data type no�� mtd module �˻�
 *
 * Implementation :
 *    mtd module number�� �ش� mtd module�� ã����
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aNo >= mtdNumberOfModules, ERR_NOT_FOUND );

    *aModule = mAllModule[aNo];

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        // idlOS::sprintf( sBuffer, "(no=%"ID_UINT32_FMT")", aNo );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "(no=%"ID_UINT32_FMT")", aNo );
        IDE_SET(ideSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                                sBuffer));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool mtd::isNullDefault( const mtcColumn*,
                           const void* )
{
    return ID_FALSE;
}

IDE_RC mtd::encodeCharDefault( mtcColumn  * /* aColumn */,
                               void       * aValue,
                               UInt         /* aValueSize */,
                               UChar      * /* aCompileFmt */,
                               UInt         /* aCompileFmtLen */,
                               UChar      * aText,
                               UInt       * aTextLen,
                               IDE_RC     * aRet )
{
    UInt          sStringLen;
    mtdCharType * sCharVal;

    //----------------------------------
    // Parameter Validation
    //----------------------------------

    IDE_ASSERT( aValue != NULL );
    IDE_ASSERT( aText != NULL );
    IDE_ASSERT( *aTextLen > 0 );
    IDE_ASSERT( aRet != NULL );

    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;
    sCharVal = (mtdCharType*)aValue;

    //----------------------------------
    // Set String
    //----------------------------------

    // To Fix BUG-16801
    if ( mtdChar.isNull( NULL, aValue ) == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sStringLen = ( *aTextLen > sCharVal->length )
            ? sCharVal->length : (*aTextLen - 1);
        idlOS::memcpy(aText, sCharVal->value, sStringLen );
    }

    //----------------------------------
    // Finalization
    //----------------------------------

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    IDE_TEST( sStringLen < sCharVal->length );

    *aRet = IDE_SUCCESS;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRet = IDE_FAILURE;

    return IDE_SUCCESS;
}

IDE_RC mtd::encodeNumericDefault( mtcColumn  * /* aColumn */,
                                  void       * aValue,
                                  UInt         /* aValueSize */,
                                  UChar      * /* aCompileFmt */,
                                  UInt         /* aCompileFmtLen */,
                                  UChar      * aText,
                                  UInt       * aTextLen,
                                  IDE_RC     * aRet )
{
    UInt sStringLen;

    //----------------------------------
    // Parameter Validation
    //----------------------------------

    IDE_ASSERT( aValue != NULL );
    IDE_ASSERT( aText != NULL );
    IDE_ASSERT( *aTextLen > 0 );
    IDE_ASSERT( aRet != NULL );

    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;

    //----------------------------------
    // Set String
    //----------------------------------

    // To Fix BUG-16801
    if ( mtdNumeric.isNull( NULL, aValue ) == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        // To Fix BUG-16802 : copy from mtvCalculate_Float2Varchar()
        IDE_TEST( mtv::float2String( aText,
                                     47,
                                     & sStringLen,
                                     (mtdNumericType*) aValue )
                  != IDE_SUCCESS );
    }

    //----------------------------------
    // Finalization
    //----------------------------------

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    *aRet = IDE_SUCCESS;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRet = IDE_FAILURE;

    return IDE_FAILURE;
}

IDE_RC mtd::decodeDefault( mtcTemplate * aTemplate,
                           mtcColumn   * aColumn,
                           void        * aValue,
                           UInt        * aValueSize,
                           UChar       * /* aCompileFmt */,
                           UInt          /* aCompileFmtLen */,
                           UChar       * aText,
                           UInt          aTextLen,
                           IDE_RC      * aRet )
{
    UInt sValueOffset = 0;

    return aColumn->module->value(aTemplate,
                                  aColumn,
                                  aValue,
                                  &sValueOffset,
                                  *aValueSize,
                                  aText,
                                  aTextLen,
                                  aRet) ;
}

IDE_RC mtd::encodeNA( mtcColumn  * /* aColumn */,
                      void       * /* aValue */,
                      UInt         /* aValueSize */,
                      UChar      * /* aCompileFmt */,
                      UInt         /* aCompileFmtLen */,
                      UChar      * aText,
                      UInt       * aTextLen,
                      IDE_RC     * aRet )
{
    // To fix BUG-14235
    // return is IDE_SUCCESS.
    // but encoding result is IDE_FAILURE.

    aText[0] = '\0';
    *aTextLen = 0;

    *aRet = IDE_FAILURE;

    return IDE_SUCCESS;
}

IDE_RC mtd::compileFmtDefault( mtcColumn  * /* aColumn */,
                               UChar      * /* aCompiledFmt */,
                               UInt       * /* aCompiledFmtLen */,
                               UChar      * /* aFormatString */,
                               UInt         /* aFormatStringLength */,
                               IDE_RC     * aRet )
{
    *aRet = IDE_SUCCESS;
    return IDE_SUCCESS;
}

IDE_RC mtd::valueFromOracleDefault( mtcColumn*  /* aColumn       */,
                                    void*       /* aValue        */,
                                    UInt*       /* aValueOffset  */,
                                    UInt        /* aValueSize    */,
                                    const void* /* aOracleValue  */,
                                    SInt        /* aOracleLength */,
                                    IDE_RC*     /* aResult       */ )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));

    return IDE_FAILURE;
}

IDE_RC mtd::validateDefault( mtcColumn * /* aColumn    */,
                             void      * /* aValue     */,
                             UInt        /* aValueSize */ )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));

    return IDE_FAILURE;
}

IDE_RC
mtd::makeColumnInfoDefault( void * /* aStmt */,
                            void * /* aTableInfo */,
                            UInt   /* aIdx */ )
{
    return IDE_SUCCESS;
}

SDouble mtd::selectivityNA( void     * /* aColumnMax      */,
                            void     * /* aColumnMin      */,
                            void     * /* aValueMax       */,
                            void     * /* aValueMin       */,
                            SDouble    /* aBoundFactor    */,
                            SDouble    /* aTotalRecordCnt */ )
{
    // Selectivity�� ������ �� ���� ����,
    // �� �Լ��� ȣ��Ǿ�� �ȵ�.
    IDE_CALLBACK_FATAL ( "mtd::selectivityNA() : "
                         "This Data Type does not Support Selectivity" );

    return 1;
}

SDouble mtd::selectivityDefault( void     * /* aColumnMax      */,
                                 void     * /* aColumnMin      */,
                                 void     * /* aValueMax       */,
                                 void     * /* aValueMin       */,
                                 SDouble    /* aBoundFactor    */,
                                 SDouble    /* aTotalRecordCnt */ )
{
/*******************************************************************
 * Definition
 *      PROJ-1579 NCHAR
 *      PROJ-2242 GEOMETRY
 *
 * Description
 *      ��Ƽ����Ʈ ĳ���� �¿� ���� selectivity ���� ��,
 *      CHAR Ÿ�Կ��� ����ϰ� �ִ� mtdSelectivityChar() �Լ��� ����� ���
 *      ������ �߸��� ���� ���� �� �ִ�.
 *      ����, ��� ĳ���� �¿� ���� selectivity �� ���ϴ� �Լ��� ����
 *      �����ϱ� �������� ��Ƽ����Ʈ ĳ���� �¿� ���ؼ���
 *      default selectivity �� ����ϵ��� �Ѵ�.
 *
 *******************************************************************/

    return MTD_DEFAULT_SELECTIVITY;
}

void mtd::convertNumeric2DoubleType( UChar           aNumericLength,
                                     UChar         * aSignExponentMantissa,
                                     mtdDoubleType * aDoubleValue )
{
/***********************************************************************
 *
 * Description : NUMERIC type�� value�� DOUBLE type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 ***********************************************************************/    

    mtdNumericType* sNumeric;
    UChar           sNumericBuff[MTD_NUMERIC_SIZE_MAXIMUM];

    if ( aNumericLength == 0 )
    {
        *(ULong*)aDoubleValue = *(ULong*)mtdDouble.staticNull;
    }
    else
    {
        /* PROJ-1517 */
        sNumeric = (mtdNumericType *)sNumericBuff;

        sNumeric->length = aNumericLength;
        idlOS::memcpy( (void*)&(sNumeric->signExponent),
                       aSignExponentMantissa,
                       aNumericLength );
        mtc::numeric2Double( aDoubleValue, sNumeric );
    }
}

void mtd::convertBigint2NumericType( mtdBigintType  * aBigintValue,
                                     mtdNumericType * aNumericValue )
{
/***********************************************************************
 *
 * Description : BIGINT type�� value�� NUMERIC type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 ***********************************************************************/

    if ( *aBigintValue == MTD_BIGINT_NULL )
    {
        aNumericValue->length = 0;
        
    }
    else
    {
        /* PROJ-1517 */
        (void)mtc::makeNumeric( aNumericValue, *(SLong*)aBigintValue );
    }
}

UInt mtd::getDefaultIndexTypeID( const mtdModule * aModule )
{
    return (UInt) aModule->idxType[0];
}

idBool mtd::isUsableIndexType( const mtdModule * aModule, UInt aIndexType )
{
    UInt i;
    idBool sResult;

    sResult = ID_FALSE;

    for ( i = 0; i < MTD_MAX_USABLE_INDEX_CNT; i++ )
    {
        if ( (UInt) aModule->idxType[i] == aIndexType )
        {
            sResult = ID_TRUE;
            break;
        }
        else
        {
            // Nothing To Do
        }
    }

    return sResult;
}

IDE_RC mtd::assignNullValueById( UInt    aId,
                                 void ** aValue,
                                 UInt  * aSize )
{
/***********************************************************************
 *
 * Description : data type���� mtd module�� staticNull ����
 *
 * Implementation : ( PROJ-1558 )
 *
 ***********************************************************************/

    mtdModule  * sModule;

    IDE_TEST( mtd::moduleById( (const mtdModule **) & sModule,
                               aId )
              != IDE_SUCCESS );

    *aValue = sModule->staticNull;
    *aSize  = sModule->actualSize( NULL, *aValue );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::checkNullValueById( UInt     aId,
                                void   * aValue,
                                idBool * aIsNull )
{
/***********************************************************************
 *
 * Description : data type���� mtd module�� isNull �˻�
 *
 * Implementation : ( PROJ-1558 )
 *
 ***********************************************************************/

    mtdModule  * sModule;

    IDE_TEST( mtd::moduleById( (const mtdModule **) & sModule,
                               aId )
              != IDE_SUCCESS );

    if ( sModule->isNull( NULL, aValue ) == ID_TRUE )
    {
        *aIsNull = ID_TRUE;
    }
    else
    {
        *aIsNull = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::mtdStoredValue2MtdValueNA( UInt              /* aColumnSize */, 
                                       void            * /* aDestValue */,
                                       UInt              /* aDestValueOffset */,
                                       UInt              /* aLength */,
                                       const void      * /* aValue */ )
{
/***********************************************************************
 * PROJ-1705
 * ������� �ʴ� Ÿ�Կ� ���� ó��
 ***********************************************************************/

    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));

    return IDE_FAILURE;
}


UInt mtd::mtdNullValueSizeNA()
{

    IDE_ASSERT( 0 );
    
    return 0;    
}

UInt mtd::mtdHeaderSizeNA()
{

    IDE_ASSERT( 0 );
    
    return 0;    
}

UInt mtd::mtdHeaderSizeDefault()
{
    return 0;    
}

UInt mtd::mtdStoreSizeDefault( const smiColumn * aColumn )
{
    return aColumn->size;    
}

IDE_RC mtd::modifyNls4MtdModule()
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1579 NCHAR
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar       * sDBCharSetStr       = NULL;
    SChar       * sNationalCharSetStr = NULL;

    //---------------------------------------------------------
    // DB ĳ���� �°� ���ų� ĳ���� �� ����
    //---------------------------------------------------------

    // CallBack �Լ��� �޷��ִ� smiGetDBCharSet()�� �̿��ؼ� �����´�.
    sDBCharSetStr = mtc::getDBCharSet();
    sNationalCharSetStr = mtc::getNationalCharSet();

    IDE_TEST( mtl::moduleByName( (const mtlModule **) & mtl::mDBCharSet,
                                 sDBCharSetStr,
                                 idlOS::strlen(sDBCharSetStr) )
              != IDE_SUCCESS );

    IDE_TEST( mtl::moduleByName( (const mtlModule **) & mtl::mNationalCharSet,
                                 sNationalCharSetStr,
                                 idlOS::strlen(sNationalCharSetStr) )
              != IDE_SUCCESS );

    /* PROJ-2208 Multi Currency */
    IDE_TEST( mtuProperty::loadNLSCurrencyProperty( ) != IDE_SUCCESS );
    // BUGBUG
    IDE_ASSERT( mtl::mDBCharSet != NULL );
    IDE_ASSERT( mtl::mNationalCharSet != NULL );


    //----------------------
    // language ���� �缳��
    //----------------------

    // CHAR
    mtdChar.column->type.languageId = mtl::mDBCharSet->id;
    mtdChar.column->language = mtl::mDBCharSet;

    // VARCHAR
    mtdVarchar.column->type.languageId = mtl::mDBCharSet->id;
    mtdVarchar.column->language = mtl::mDBCharSet;

    // CLOB
    mtdClob.column->type.languageId = mtl::mDBCharSet->id;
    mtdClob.column->language = mtl::mDBCharSet;

    // CLOBLOCATOR
    mtdClobLocator.column->type.languageId = mtl::mDBCharSet->id;
    mtdClobLocator.column->language = mtl::mDBCharSet;

    // NCHAR
    mtdNchar.column->type.languageId = mtl::mNationalCharSet->id;
    mtdNchar.column->language = mtl::mNationalCharSet;

    // NVARCHAR
    mtdNvarchar.column->type.languageId = mtl::mNationalCharSet->id;
    mtdNvarchar.column->language = mtl::mNationalCharSet;

    // PROJ-2002 Column Security
    // ECHAR
    mtdEchar.column->type.languageId = mtl::mDBCharSet->id;
    mtdEchar.column->language = mtl::mDBCharSet;

    // EVARCHAR
    mtdEvarchar.column->type.languageId = mtl::mDBCharSet->id;
    mtdEvarchar.column->language = mtl::mDBCharSet;

    //----------------------
    // compare �Լ� �缳��
    //----------------------

    //------------------------------------------------
    // �ѱ������� ���, ( BUG-13838 )
    // mtdChar�� compare�Լ��� �� �����  collation compare�Լ��� �ٲ��ش�.
    // �ѱ������� �ʼ��� ������ �ѱ�������.
    // ( KSC5601, MS949�� �����Ǹ�,
    //   MS949�� KSC5601�� super set���� KSC5601�� ��� �ڵ带 �����Ѵ�. )
    //-------------------------------------------------

    if( MTU_NLS_COMP_MODE == MTU_NLS_COMP_ANSI )
    {
        if( (mtl::mDBCharSet->id == MTL_ASCII_ID) ||
            (mtl::mDBCharSet->id == MTL_KSC5601_ID) ||
            (mtl::mDBCharSet->id == MTL_MS949_ID) )
        {
            // CHAR
            mtdChar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateMtdMtdAsc;

            mtdChar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateMtdMtdDesc;

            mtdChar.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateMtdMtdAsc;

            mtdChar.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateMtdMtdDesc;

            mtdChar.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateStoredMtdAsc;

            mtdChar.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateStoredMtdDesc;

            mtdChar.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateStoredStoredAsc;

            mtdChar.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateStoredStoredDesc;

            /* PROJ-2433, BUG-43106 */
            mtdChar.keyCompare[MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateIndexKeyMtdAsc;

            mtdChar.keyCompare[MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateIndexKeyMtdDesc;

            mtdChar.keyCompare[MTD_COMPARE_INDEX_KEY_MTDVAL]
                              [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlCharMS949collateIndexKeyMtdAsc;

            mtdChar.keyCompare[MTD_COMPARE_INDEX_KEY_MTDVAL]
                              [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlCharMS949collateIndexKeyMtdDesc;
            /* PROJ-2433, BUG-43106 end */

            mtdChar.logicalCompare[MTD_COMPARE_ASCENDING] = 
                mtdChar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                  [MTD_COMPARE_ASCENDING];

            mtdChar.logicalCompare[MTD_COMPARE_DESCENDING] =
                mtdChar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                  [MTD_COMPARE_DESCENDING];

            // VARCHAR
            mtdVarchar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateMtdMtdAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateMtdMtdDesc;

            mtdVarchar.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateMtdMtdAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateMtdMtdDesc;

            mtdVarchar.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateStoredMtdAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateStoredMtdDesc;

            mtdVarchar.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateStoredStoredAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateStoredStoredDesc;

            /* PROJ-2433, BUG-43106 */
            mtdVarchar.keyCompare[MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateIndexKeyMtdAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateIndexKeyMtdDesc;

            mtdVarchar.keyCompare[MTD_COMPARE_INDEX_KEY_MTDVAL]
                                 [MTD_COMPARE_ASCENDING]
                = mtlCollate::mtlVarcharMS949collateIndexKeyMtdAsc;

            mtdVarchar.keyCompare[MTD_COMPARE_INDEX_KEY_MTDVAL]
                                 [MTD_COMPARE_DESCENDING]
                = mtlCollate::mtlVarcharMS949collateIndexKeyMtdDesc;
            /* PROJ-2433, BUG-43106 end */

            mtdVarchar.logicalCompare[MTD_COMPARE_ASCENDING] = 
                mtdVarchar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                     [MTD_COMPARE_ASCENDING];

            mtdVarchar.logicalCompare[MTD_COMPARE_DESCENDING] =
                mtdVarchar.keyCompare[MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL]
                                     [MTD_COMPARE_DESCENDING];

        }
        else
        {
            //MTU_NLS_COMP_MODE = MTU_NLS_COMP_BINARY;
        }
    }
    else
    {
        // Nothing to do
    }

    //----------------------
    // BUG-28921
    // nchar, nvarchar�� maxStorePrecision �缳��
    //----------------------

    if ( mtl::mNationalCharSet->id == MTL_UTF8_ID )
    {
        mtdNchar.maxStorePrecision    = MTD_UTF8_NCHAR_STORE_PRECISION_MAXIMUM;
        mtdNvarchar.maxStorePrecision = MTD_UTF8_NCHAR_STORE_PRECISION_MAXIMUM;
    }
    else if ( mtl::mNationalCharSet->id == MTL_UTF16_ID )
    {
        mtdNchar.maxStorePrecision    = MTD_UTF16_NCHAR_STORE_PRECISION_MAXIMUM;
        mtdNvarchar.maxStorePrecision = MTD_UTF16_NCHAR_STORE_PRECISION_MAXIMUM;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_CHARACTER );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_CHARACTER )
    {
        IDE_SET( ideSetErrorCode(mtERR_ABORT_INVALID_CHARACTER) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtd::getPrecisionNA( const mtcColumn * /* aColumn */,
                          const void      * /* aRow */,
                          SInt            * /* aPrecision */,
                          SInt            * /* aScale */ )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));
    return IDE_FAILURE;
}


//---------------------------------------------------------
// compareNumberGroupBigint
//---------------------------------------------------------

void mtd::convertToBigintType4MtdValue( mtdValueInfo   * aValueInfo,
                                        mtdBigintType  * aBigintValue )
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� bigint type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 *  �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *  ������ : BIGINT, INTEGER, SMALLINT
 *
 ***********************************************************************/
    
    const mtcColumn * sColumn;
    void            * sValueTemp;

    sColumn    = aValueInfo->column;

    sValueTemp = (void*)mtd::valueForModule( (smiColumn*)sColumn,
                                             aValueInfo->value,
                                             aValueInfo->flag,
                                             sColumn->module->staticNull );

    if( sColumn->module->isNull( sColumn, sValueTemp ) == ID_TRUE )
    {
        mtdBigint.null( NULL, aBigintValue );
    }
    else
    {
        // BUG-17791
        if ( sColumn->type.dataTypeId == MTD_SMALLINT_ID )
        {
            *aBigintValue = (mtdBigintType)*(mtdSmallintType*)sValueTemp;
        }
        else if ( sColumn->type.dataTypeId == MTD_INTEGER_ID )
        {
            *aBigintValue = (mtdBigintType)*(mtdIntegerType*)sValueTemp;
        }
        else if ( sColumn->type.dataTypeId == MTD_BIGINT_ID )
        {
            *aBigintValue = *(mtdBigintType*)sValueTemp;
        }
        else
        {
            ideLog::log( IDE_ERR_0,
                         "sColumn->type.dataTypeId : %d\n",
                         sColumn->type.dataTypeId );
                         
            IDE_ASSERT( 0 );
        }
    }
}

void
     
mtd::convertToBigintType4StoredValue( mtdValueInfo  * aValueInfo,
                                      mtdBigintType * aBigintValue )
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� bigint type���� conversion
 * 
 * Implementation : ( PROJ-1364 )
 *
 *  �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *  ������ : BIGINT, INTEGER, SMALLINT
 * 
 *  mtdValueInfo�� value�� Page�� ����� value�� ����Ŵ
 *  ���� align ���� �ʾ��� ���� �ֱ� ������ byte assign �Ͽ��� ��
 ***********************************************************************/
    
    const mtcColumn * sColumn;
    mtdSmallintType   sSmallintValue;
    mtdIntegerType    sIntegerValue;
    
    sColumn    = aValueInfo->column;

    if ( sColumn->type.dataTypeId == MTD_SMALLINT_ID )
    {
        ID_SHORT_BYTE_ASSIGN( &sSmallintValue, aValueInfo->value );

        if ( sSmallintValue == *((mtdSmallintType*)mtdSmallint.staticNull) )
        {
            *aBigintValue = *((mtdBigintType*)mtdBigint.staticNull);
        }
        else
        {
            *aBigintValue = (mtdBigintType)sSmallintValue;
        }
    }
    else if ( aValueInfo->column->type.dataTypeId == MTD_INTEGER_ID )
    {
        ID_INT_BYTE_ASSIGN( &sIntegerValue, aValueInfo->value );
        
        if ( sIntegerValue == *((mtdIntegerType*)mtdInteger.staticNull) )
        {
            *aBigintValue = *((mtdBigintType*)mtdBigint.staticNull);
        }
        else
        {
            *aBigintValue = (mtdBigintType)sIntegerValue;
        }
    }
    else if ( aValueInfo->column->type.dataTypeId == MTD_BIGINT_ID )
    {
        ID_LONG_BYTE_ASSIGN( aBigintValue, aValueInfo->value );
    }
    else
    {
        ideLog::log( IDE_ERR_0,
                     "sColumn->type.dataTypeId : %d\n",
                     sColumn->type.dataTypeId );
        IDE_ASSERT( 0 );
    }
}

SInt mtd::compareNumberGroupBigintMtdMtdAsc( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtd::convertToBigintType4MtdValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.logicalCompare[MTD_COMPARE_ASCENDING]( aValueInfo1,
                                                            aValueInfo2 );
}

SInt mtd::compareNumberGroupBigintMtdMtdDesc( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtd::convertToBigintType4MtdValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.logicalCompare[MTD_COMPARE_DESCENDING]( aValueInfo1,
                                                             aValueInfo2 );
}

SInt mtd::compareNumberGroupBigintStoredMtdAsc( mtdValueInfo * aValueInfo1,
                                                mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtd::convertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupBigintStoredMtdDesc( mtdValueInfo * aValueInfo1,
                                                 mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtd::convertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

SInt mtd::compareNumberGroupBigintStoredStoredAsc( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtd::convertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4StoredValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupBigintStoredStoredDesc( mtdValueInfo * aValueInfo1,
                                                    mtdValueInfo * aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtd::convertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtd::convertToBigintType4StoredValue( aValueInfo2, &sBigintValue2 ); 

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdBigint.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

//---------------------------------------------------------
// compareNumberGroupDouble
//---------------------------------------------------------

void mtd::convertToDoubleType4MtdValue( mtdValueInfo  * aValueInfo,
                                        mtdDoubleType * aDoubleValue )
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� double type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 *   �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *   ������ : BIGINT, INTEGER, SMALLINT
 *   �Ǽ��� : DOUBLE, REAL
 *   ������ : �����Ҽ����� : NUMERIC, DECIMAL, NUMBER(P), NUMER(P,S)
 *            �����Ҽ����� : FLOAT, NUMBER
 *
 ***********************************************************************/

    const mtcColumn * sColumn;
    mtdRealType     * sReal;
    mtdNumericType  * sNumericValuePtr;
    SChar             sTempBuf[16];
    void            * sValueTemp;

    sColumn    = aValueInfo->column;

    sValueTemp = (void*)mtd::valueForModule( (smiColumn*)sColumn,
                                             aValueInfo->value,
                                             aValueInfo->flag,
                                             sColumn->module->staticNull );

    if( sColumn->module->isNull( sColumn, sValueTemp ) == ID_TRUE )
    {
        mtdDouble.null( NULL, aDoubleValue );
    }
    else
    {
        // BUG-17791
        if ( sColumn->type.dataTypeId == MTD_SMALLINT_ID )
        {
            *aDoubleValue = (mtdDoubleType)*(mtdSmallintType*)sValueTemp;
        }
        else if ( sColumn->type.dataTypeId == MTD_INTEGER_ID )
        {
            *aDoubleValue = (mtdDoubleType)*(mtdIntegerType*)sValueTemp;
        }
        else if ( sColumn->type.dataTypeId == MTD_BIGINT_ID )
        {
            *aDoubleValue = (mtdDoubleType)*(mtdBigintType*)sValueTemp;
        }
        else if ( sColumn->type.dataTypeId == MTD_REAL_ID )
        {            
            // fix BUG-13674
            sReal = (mtdRealType*)sValueTemp;

            idlOS::snprintf( sTempBuf, ID_SIZEOF(sTempBuf),
                             "%"ID_FLOAT_G_FMT"",
                             *(mtdRealType*)sReal );

            *aDoubleValue = idlOS::strtod( sTempBuf, NULL );
        }
        else if ( sColumn->type.dataTypeId == MTD_DOUBLE_ID )
        {
            *aDoubleValue = *(mtdDoubleType*)sValueTemp;
        }
        else if ( ( sColumn->type.dataTypeId == MTD_NUMERIC_ID ) ||
                  ( sColumn->type.dataTypeId == MTD_FLOAT_ID ) ||
                  ( sColumn->type.dataTypeId == MTD_NUMBER_ID ) )
        {
            sNumericValuePtr = (mtdNumericType*)sValueTemp;

            convertNumeric2DoubleType( sNumericValuePtr->length,
                                       &(sNumericValuePtr->signExponent),
                                       aDoubleValue );
        }
        else
        {
            ideLog::log( IDE_ERR_0,
                         "sColumn->type.dataTypeId :%d\n",
                         sColumn->type.dataTypeId );
            IDE_ASSERT( 0 );
        }
    }
}

void mtd::convertToDoubleType4StoredValue( mtdValueInfo  * aValueInfo,
                                           mtdDoubleType * aDoubleValue )
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� double type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 *   �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *   ������ : BIGINT, INTEGER, SMALLINT
 *   �Ǽ��� : DOUBLE, REAL
 *   ������ : �����Ҽ����� : NUMERIC, DECIMAL, NUMBER(P), NUMER(P,S)
 *            �����Ҽ����� : FLOAT, NUMBER
 *
 ***********************************************************************/

    const mtcColumn * sColumn;
    mtdSmallintType   sSmallintValue;
    mtdIntegerType    sIntegerValue;
    mtdBigintType     sBigintValue;
    mtdRealType       sRealValue;
    SChar             sTempBuf[16];

    sColumn    = aValueInfo->column;
    
    if ( sColumn->type.dataTypeId == MTD_SMALLINT_ID )
    {
        ID_SHORT_BYTE_ASSIGN( &sSmallintValue, aValueInfo->value );

        if ( sSmallintValue == *((mtdSmallintType*)mtdSmallint.staticNull) )
        {
            *aDoubleValue = *((mtdDoubleType*)mtdDouble.staticNull);
        }
        else
        {
            *aDoubleValue = (mtdDoubleType)sSmallintValue;
        }
    }
    else if ( sColumn->type.dataTypeId == MTD_INTEGER_ID )
    {
        ID_INT_BYTE_ASSIGN( &sIntegerValue, aValueInfo->value );
        
        if ( sIntegerValue == *((mtdIntegerType*)mtdInteger.staticNull) )
        {
            *aDoubleValue = *((mtdDoubleType*)mtdDouble.staticNull);
        }
        else
        {
            *aDoubleValue = (mtdDoubleType)sIntegerValue;
        }
    }
    else if ( sColumn->type.dataTypeId == MTD_BIGINT_ID )
    {
        ID_LONG_BYTE_ASSIGN( &sBigintValue, aValueInfo->value );
        
        if ( sBigintValue == *((mtdBigintType*)mtdBigint.staticNull) )
        {
            *aDoubleValue = *((mtdDoubleType*)mtdDouble.staticNull);
        }
        else
        {
            *aDoubleValue = (mtdDoubleType)sBigintValue;
        }   
    }
    else if ( sColumn->type.dataTypeId == MTD_REAL_ID )
    {
        ID_FLOAT_BYTE_ASSIGN( &sRealValue, aValueInfo->value );

        if ( sRealValue == *((mtdRealType*)mtdReal.staticNull) )
        {
            *aDoubleValue = *((mtdDoubleType*)mtdDouble.staticNull);
        }
        else
        {
            idlOS::snprintf( sTempBuf, ID_SIZEOF(sTempBuf),
                             "%"ID_FLOAT_G_FMT"",
                             sRealValue );
            
            *aDoubleValue = idlOS::strtod( sTempBuf, NULL );
        }
    }
    else if ( sColumn->type.dataTypeId == MTD_DOUBLE_ID )
    {
        ID_FLOAT_BYTE_ASSIGN( aDoubleValue, aValueInfo->value );
    }
    else if ( ( sColumn->type.dataTypeId == MTD_NUMERIC_ID ) ||
              ( sColumn->type.dataTypeId == MTD_FLOAT_ID ) ||
              ( sColumn->type.dataTypeId == MTD_NUMBER_ID ) )
    {       
        convertNumeric2DoubleType( (UChar)aValueInfo->length,
                                   (UChar*)aValueInfo->value,
                                   aDoubleValue );
    }
    else
    {
        ideLog::log( IDE_ERR_0,
                     "sColumn->type.dataTypeId : %d\n",
                     sColumn->type.dataTypeId );
                     
        IDE_ASSERT( 0 );
    }
}

SInt mtd::compareNumberGroupDoubleMtdMtdAsc( mtdValueInfo * aValueInfo1,
                                             mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;
    
    mtd::convertToDoubleType4MtdValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.logicalCompare[MTD_COMPARE_ASCENDING]( aValueInfo1,
                                                            aValueInfo2 );
}

SInt mtd::compareNumberGroupDoubleMtdMtdDesc( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;
    
    mtd::convertToDoubleType4MtdValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.logicalCompare[MTD_COMPARE_DESCENDING]( aValueInfo1,
                                                             aValueInfo2 );
}

SInt mtd::compareNumberGroupDoubleStoredMtdAsc( mtdValueInfo * aValueInfo1,
                                                mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtd::convertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupDoubleStoredMtdDesc( mtdValueInfo * aValueInfo1,
                                                 mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtd::convertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

SInt mtd::compareNumberGroupDoubleStoredStoredAsc( mtdValueInfo * aValueInfo1,
                                                   mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;
    
    mtd::convertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4StoredValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupDoubleStoredStoredDesc( mtdValueInfo * aValueInfo1,
                                                    mtdValueInfo * aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;
    
    mtd::convertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtd::convertToDoubleType4StoredValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdDouble.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

//---------------------------------------------------------
// compareNumberGroupNumeric
//---------------------------------------------------------

void mtd::convertToNumericType4MtdValue( mtdValueInfo    * aValueInfo,
                                         mtdNumericType ** aNumericValue )
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� numeric type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 *   �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *   ������ : BIGINT, INTEGER, SMALLINT
 *   ������ : �����Ҽ����� : NUMERIC, DECIMAL, NUMBER(P), NUMER(P,S)
 *            �����Ҽ����� : FLOAT, NUMBER
 *
 *   �������迭�� (������==>BIGINT==>NUMERIC)���� ��ȯ����
 *   �������迭�� ��� mtdNumericType�� ����ϹǷ�
 *   �ش� pointer�� ���´�.
 *
 ***********************************************************************/

    const mtcColumn * sColumn;
    mtdBigintType     sBigintValue;
    
    sColumn    = aValueInfo->column;

    // BUG-17791
    if ( ( sColumn->type.dataTypeId == MTD_SMALLINT_ID ) ||
         ( sColumn->type.dataTypeId == MTD_INTEGER_ID ) ||
         ( sColumn->type.dataTypeId == MTD_BIGINT_ID ) )
    {   
        mtd::convertToBigintType4MtdValue( aValueInfo,
                                           &sBigintValue );
        
        convertBigint2NumericType( &sBigintValue,
                                   *aNumericValue  );
    }
    else if ( ( sColumn->type.dataTypeId == MTD_NUMERIC_ID ) ||
              ( sColumn->type.dataTypeId == MTD_FLOAT_ID ) ||
              ( sColumn->type.dataTypeId == MTD_NUMBER_ID ) )
    {
        *aNumericValue  =  (mtdNumericType*)
            mtd::valueForModule( (smiColumn*)sColumn,
                                 aValueInfo->value,
                                 aValueInfo->flag,
                                 mtdNumeric.staticNull );
        
    }
    else
    {
        ideLog::log( IDE_ERR_0,
                     "sColumn->type.dataTypeId : %d\n",
                     sColumn->type.dataTypeId );
                     
        IDE_ASSERT( 0 );
    }
}

void mtd::convertToNumericType4StoredValue( mtdValueInfo    * aValueInfo,
                                            mtdNumericType ** aNumericValue,
                                            UChar           * aLength,
                                            UChar          ** aSignExponentMantissa)
{
/***********************************************************************
 *
 * Description : �ش� data type�� value�� numeric type���� conversion
 *
 * Implementation : ( PROJ-1364 )
 *
 *   �� �Լ��� ���� �� �ִ� data type�� ������ ����.
 *
 *   ������ : BIGINT, INTEGER, SMALLINT
 *   ������ : �����Ҽ����� : NUMERIC, DECIMAL, NUMBER(P), NUMER(P,S)
 *            �����Ҽ����� : FLOAT, NUMBER
 *
 *   �������迭�� (������==>BIGINT==>NUMERIC)���� ��ȯ����
 *   �������迭�� ��� mtdNumericType�� ����ϹǷ�
 *   �ش� pointer�� ���´�.
 *
 ***********************************************************************/

    const mtcColumn * sColumn;
    mtdBigintType     sBigintValue;

    sColumn    = aValueInfo->column;

    // BUG-17791
    if ( ( sColumn->type.dataTypeId == MTD_SMALLINT_ID ) ||
         ( sColumn->type.dataTypeId == MTD_INTEGER_ID ) ||
         ( sColumn->type.dataTypeId == MTD_BIGINT_ID ) )
    {
        mtd::convertToBigintType4StoredValue( aValueInfo,
                                              &sBigintValue );
        
        convertBigint2NumericType( &sBigintValue,
                                   *aNumericValue );

        *aLength = (*aNumericValue)->length;
        *aSignExponentMantissa = &((*aNumericValue)->signExponent);
    }
    else if ( ( sColumn->type.dataTypeId == MTD_NUMERIC_ID ) ||
              ( sColumn->type.dataTypeId == MTD_FLOAT_ID ) ||
              ( sColumn->type.dataTypeId == MTD_NUMBER_ID ) )
    {
        *aLength = (UChar)aValueInfo->length;
        *aSignExponentMantissa = (UChar*)aValueInfo->value;
    }
    else
    {
        ideLog::log( IDE_ERR_0,
                     "sColumn->type.dataTypeId : %d\n",
                     sColumn->type.dataTypeId );
                     
        IDE_ASSERT( 0 );
    }
}

SInt mtd::compareNumberGroupNumericMtdMtdAsc( mtdValueInfo * aValueInfo1,
                                              mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtd::convertToNumericType4MtdValue( aValueInfo1, &sNumericValuePtr1 );
    mtd::convertToNumericType4MtdValue( aValueInfo2, &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sNumericValuePtr1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.logicalCompare[MTD_COMPARE_ASCENDING]( aValueInfo1,
                                                             aValueInfo2 );
}

SInt mtd::compareNumberGroupNumericMtdMtdDesc( mtdValueInfo * aValueInfo1,
                                               mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtd::convertToNumericType4MtdValue( aValueInfo1,
                                        &sNumericValuePtr1 );
    mtd::convertToNumericType4MtdValue( aValueInfo2,
                                        &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sNumericValuePtr1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.logicalCompare[MTD_COMPARE_DESCENDING]( aValueInfo1,
                                                              aValueInfo2 );
}

SInt mtd::compareNumberGroupNumericStoredMtdAsc( mtdValueInfo * aValueInfo1,
                                                 mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;
    UChar            sLength1;
    UChar          * sSignExponentMantissa1;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;

    mtd::convertToNumericType4StoredValue( aValueInfo1,
                                           &sNumericValuePtr1,
                                           &sLength1,
                                           &sSignExponentMantissa1 );
    
    mtd::convertToNumericType4MtdValue( aValueInfo2,
                                        &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa1;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;
    
    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupNumericStoredMtdDesc( mtdValueInfo * aValueInfo1,
                                                  mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;
    UChar            sLength1;
    UChar          * sSignExponentMantissa1;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;

    mtd::convertToNumericType4StoredValue( aValueInfo1,
                                           &sNumericValuePtr1,
                                           &sLength1,
                                           &sSignExponentMantissa1 );
    
    mtd::convertToNumericType4MtdValue( aValueInfo2,
                                        &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa1;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;
    
    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

SInt mtd::compareNumberGroupNumericStoredStoredAsc( mtdValueInfo * aValueInfo1,
                                                    mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;
    UChar            sLength1;
    UChar          * sSignExponentMantissa1;
    UChar            sLength2;
    UChar          * sSignExponentMantissa2;    

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtd::convertToNumericType4StoredValue( aValueInfo1,
                                           &sNumericValuePtr1,
                                           &sLength1,
                                           &sSignExponentMantissa1 );
    
    mtd::convertToNumericType4StoredValue( aValueInfo2,
                                           &sNumericValuePtr2,
                                           &sLength2,
                                           &sSignExponentMantissa2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa1;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sSignExponentMantissa2;
    aValueInfo1->length = sLength2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

SInt mtd::compareNumberGroupNumericStoredStoredDesc( mtdValueInfo * aValueInfo1,
                                                     mtdValueInfo * aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    SChar            sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    SChar            sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType * sNumericValuePtr1;
    mtdNumericType * sNumericValuePtr2;
    UChar            sLength1;
    UChar          * sSignExponentMantissa1;
    UChar            sLength2;
    UChar          * sSignExponentMantissa2;    

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtd::convertToNumericType4StoredValue( aValueInfo1,
                                           &sNumericValuePtr1,
                                           &sLength1,
                                           &sSignExponentMantissa1 );
    
    mtd::convertToNumericType4StoredValue( aValueInfo2,
                                           &sNumericValuePtr2,
                                           &sLength2,
                                           &sSignExponentMantissa2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa2;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sSignExponentMantissa2;
    aValueInfo2->length = sLength2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

mtdCompareFunc 
mtd::findCompareFunc( mtcColumn * aColumn,
                      mtcColumn * aValue,
                      UInt        aCompValueType,
                      UInt        aDirection )
{
/***********************************************************************
 *
 * Description : ���ϰ迭�� ���ϴ� ���� �ٸ� data type���� ���Լ�
 *               ã���ִ� �Լ�
 *
 * Implementation : ( PROJ-1364 )
 *
 *   �� �迭�� ���Լ��� ȣ���ؼ�, �񱳿����� �����Ѵ�.
 *
 *   1. �������迭
 *      char�� varchar���� ��
 *
 *   2. �������迭
 *      (1) �񱳱��� data type�� ������
 *      (2) �񱳱��� data type�� �Ǽ���
 *      (3) �񱳱��� data type�� ������
 *
 *      ������ �迭�� �񱳽� �Ʒ� conversion matrix�� ���Ͽ�
 *      �񱳱��� data type�� ��������.
 *      ------------------------------------------
 *             |  N/A   | ������ | �Ǽ��� | ������
 *      ------------------------------------------
 *        N/A  |  N/A   |  N/A   |  N/A   |  N/A
 *      ------------------------------------------
 *      ������ |  N/A   | ������ | �Ǽ��� | ������
 *      ------------------------------------------
 *      �Ǽ��� |  N/A   | �Ǽ��� | �Ǽ��� | �Ǽ���
 *      ------------------------------------------
 *      ������ |  N/A   | ������ | �Ǽ��� | ������
 *      ------------------------------------------
 *
 * << data type�� �з� >>
 * -----------------------------------------------------------------------
 *            |                                                | ��ǥtype
 * ----------------------------------------------------------------------
 * �������迭 | CHAR, VARCHAR                                  | VARCHAR
 * ----------------------------------------------------------------------
 * �������迭 | Native | ������ | BIGINT, INTEGER, SMALLINT    | BIGINT
 *            |        |-------------------------------------------------
 *            |        | �Ǽ��� | DOUBLE, REAL                 | DOUBLE
 *            -----------------------------------------------------------
 *            | Non-   | �����Ҽ����� | NUMERIC, DECIMAL,      |
 *            | Native |              | NUMBER(p), NUMBER(p,s) | NUMERIC
 *            |(������)|----------------------------------------
 *            |        | �����Ҽ����� | FLOAT, NUMBER          |
 * ----------------------------------------------------------------------
 *
 ***********************************************************************/
    
    UInt            sType;
    UInt            sColumn1GroupFlag = 0;
    UInt            sColumn2GroupFlag = 0;
    mtdCompareFunc  sCompare;

    if( ( aColumn->module->flag & MTD_GROUP_MASK )
          == MTD_GROUP_TEXT )
    {
        //-------------------------------
        // ������ �迭
        //-------------------------------

        sCompare = mtdVarchar.keyCompare[aCompValueType][aDirection];
    }
    else
    {
        //------------------------------
        // ������ �迭
        //------------------------------

        sColumn1GroupFlag =
            ( aColumn->module->flag &
              MTD_NUMBER_GROUP_TYPE_MASK ) >> MTD_NUMBER_GROUP_TYPE_SHIFT;
        sColumn2GroupFlag =
            ( aValue->module->flag &
              MTD_NUMBER_GROUP_TYPE_MASK ) >> MTD_NUMBER_GROUP_TYPE_SHIFT;

        sType =
            mtd::comparisonNumberType[sColumn1GroupFlag][sColumn2GroupFlag];

        switch( sType )
        {
            case ( MTD_NUMBER_GROUP_TYPE_BIGINT ) :
                // �񱳱��� data type�� ������

                 sCompare = mtd::compareNumberGroupBigintFuncs[aCompValueType]
                                                              [aDirection];
                break;
            case ( MTD_NUMBER_GROUP_TYPE_DOUBLE ) :
                // �񱳱��� data type�� �Ǽ���

                 sCompare = mtd::compareNumberGroupDoubleFuncs[aCompValueType]
                                                              [aDirection];

                break;
            case ( MTD_NUMBER_GROUP_TYPE_NUMERIC ) :
                // �񱳱��� data type�� ������
                
                sCompare = mtd::compareNumberGroupNumericFuncs[aCompValueType]
                                                              [aDirection];
                                
                break;
            default :
                ideLog::log( IDE_ERR_0,
                             "sType : %u\n",
                             sType );
                             
                IDE_ASSERT( 0 );
        }
    }

    return sCompare;
}

// PROJ-2429
IDE_RC mtd::mtdStoredValue2MtdValue4CompressColumn( UInt           /*  aColumnSize */,
                                                    void            * aDestValue,
                                                    UInt           /* aDestValueOffset */,
                                                    UInt              aLength,
                                                    const void      * aValue )
{
/*******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    smOID* sSmOIDValue;

    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����.

    sSmOIDValue = (smOID*)aDestValue;
    
    /* ����� smOID��ũ��� column�� ũ�⺸�� Ŭ �� ����.
      compress column���� null data�� ���� ���� �ʴ´�.( �׻� smOID���� �ִ�.)*/
    IDE_TEST_RAISE( aLength != ID_SIZEOF( smOID ), ERR_INVALID_STORED_VALUE );

    ID_LONG_BYTE_ASSIGN( sSmOIDValue, aValue );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2446 ONE SOURCE */
IDE_RC mtd::setMtcColumnInfo( void * aColumn )
{
    mtcColumn * sMtcColumn = (mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &(sMtcColumn->module),
                               sMtcColumn->type.dataTypeId )
              != IDE_SUCCESS );

    IDE_TEST( mtl::moduleById( &(sMtcColumn->language),
                               sMtcColumn->type.languageId )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
