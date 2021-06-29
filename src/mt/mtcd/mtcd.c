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
 * $Id: mtd.cpp 36964 2009-11-26 01:41:30Z linkedlist $
 **********************************************************************/

#include <mtce.h>
#include <mtcd.h>
#include <mtcl.h>
#include <mtcc.h>

#include <mtcdTypes.h>
#include <mtclCollate.h>

extern mtdModule mtcdBigint;
extern mtdModule mtcdBinary;
extern mtdModule mtcdBit;
extern mtdModule mtcdVarbit;
extern mtdModule mtcdBlob;
extern mtdModule mtcdBoolean;
extern mtdModule mtcdChar;
extern mtdModule mtcdDate;
extern mtdModule mtcdDouble;
extern mtdModule mtcdFloat;
extern mtdModule mtcdInteger;
extern mtdModule mtcdInterval;
extern mtdModule mtcdList;
extern mtdModule mtcdNull;
extern mtdModule mtcdNumber;
extern mtdModule mtcdNumeric;
extern mtdModule mtcdReal;
extern mtdModule mtcdSmallint;
extern mtdModule mtcdVarchar;
extern mtdModule mtcdByte;
extern mtdModule mtcdNibble;
extern mtdModule mtcdClob;
//extern mtdModule mtdBlobLocator;  // PROJ-1362
//extern mtdModule mtdClobLocator;  // PROJ-1362
extern mtdModule mtcdNchar;        // PROJ-1579 NCHAR
extern mtdModule mtcdNvarchar;     // PROJ-1579 NCHAR
extern mtdModule mtcdEchar;        // PROJ-2002 Column Security
extern mtdModule mtcdEvarchar;     // PROJ-2002 Column Security
extern mtdModule mtcdVarbyte;      // BUG-40973

const mtlModule* mtlDBCharSet;
const mtlModule* mtlNationalCharSet;

mtdModule * mtdAllModule[MTD_MAX_DATATYPE_CNT];
const mtdModule* mtdInternalModule[] = {
    &mtcdBigint,
    &mtcdBit,
    &mtcdVarbit,
    &mtcdBlob,
    &mtcdBoolean,
    &mtcdChar,
    &mtcdDate,
    &mtcdDouble,
    &mtcdFloat,
    &mtcdInteger,
    &mtcdInterval,
    &mtcdList,
    &mtcdNull,
    &mtcdNumber,
    &mtcdNumeric,
    &mtcdReal,
    &mtcdSmallint,
    &mtcdVarchar,
    &mtcdByte,
    &mtcdNibble,
    &mtcdClob,
//    &mtdBlobLocator,  // PROJ-1362
//    &mtdClobLocator,  // PROJ-1362
    &mtcdBinary,       // PROJ-1583, PR-15722
    &mtcdNchar,        // PROJ-1579 NCHAR
    &mtcdNvarchar,     // PROJ-1579 NCHAR
    &mtcdEchar,        // PROJ-2002 Column Security
    &mtcdEvarchar,     // PROJ-2002 Column Security
    &mtcdVarbyte,
    NULL
};

const mtdCompareFunc mtdCompareNumberGroupBigintFuncs[3][2] = {
    {
        mtdCompareNumberGroupBigintMtdMtdAsc,
        mtdCompareNumberGroupBigintMtdMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupBigintStoredMtdAsc,
        mtdCompareNumberGroupBigintStoredMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupBigintStoredStoredAsc,
        mtdCompareNumberGroupBigintStoredStoredDesc,
    }
};

const mtdCompareFunc mtdCompareNumberGroupDoubleFuncs[3][2] = {
    {
        mtdCompareNumberGroupDoubleMtdMtdAsc,
        mtdCompareNumberGroupDoubleMtdMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupDoubleStoredMtdAsc,
        mtdCompareNumberGroupDoubleStoredMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupDoubleStoredStoredAsc,
        mtdCompareNumberGroupDoubleStoredStoredDesc,
    }
};

const mtdCompareFunc mtdCompareNumberGroupNumericFuncs[3][2] = {
    {
        mtdCompareNumberGroupNumericMtdMtdAsc,
        mtdCompareNumberGroupNumericMtdMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupNumericStoredMtdAsc,
        mtdCompareNumberGroupNumericStoredMtdDesc,
    }
    ,
    {
        mtdCompareNumberGroupNumericStoredStoredAsc,
        mtdCompareNumberGroupNumericStoredStoredDesc,
    }
};

typedef struct mtdIdIndex {
    const mtdModule* module;
} mtdIdIndex;

typedef struct mtdNameIndex {
    const mtcName*   name;
    const mtdModule* module;
} mtdNameIndex;

static acp_uint32_t  mtdNumberOfModules;

static mtdIdIndex*   mtdModulesById = NULL;

static acp_uint32_t  mtdNumberOfModulesByName;

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
const acp_uint32_t mtdComparisonNumberType[4][4] = {
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

    sOrder = mtcStrMatch( aModule1->name->string,
                          aModule1->name->length,
                          aModule2->name->string,
                          aModule2->name->length );
    return sOrder;
}

acp_uint32_t mtdGetNumberOfModules( void )
{
    return mtdNumberOfModules;
}

ACI_RC mtdIsTrueNA( acp_bool_t*      aTemp1,
                    const mtcColumn* aTemp2,
                    const void*      aTemp3,
                    acp_uint32_t     aTemp4)
{
    ACP_UNUSED(aTemp1);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    ACP_UNUSED(aTemp4);
    
    aciSetErrorCode(mtERR_FATAL_NOT_APPLICABLE);

    return ACI_FAILURE;
}

ACI_RC mtdCanonizeDefault( const mtcColumn* aCanon ,
                           void**           aCanonized,
                           mtcEncryptInfo * aCanonInfo,
                           const mtcColumn* aColumn,
                           void*            aValue,
                           mtcEncryptInfo*  aColumnInfo,
                           mtcTemplate*     aTemplate)
{
    ACP_UNUSED(aCanon);
    ACP_UNUSED(aCanonInfo);
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aColumnInfo);
    ACP_UNUSED(aTemplate);  
  
    *aCanonized = aValue;

    return ACI_SUCCESS;
}

ACI_RC mtdInitializeModule( mtdModule*   aModule,
                            acp_uint32_t aNo )
{
    /***********************************************************************
     *
     * Description : mtd module�� �ʱ�ȭ
     *
     * Implementation :
     *
     ***********************************************************************/
    aModule->no = aNo;

    return ACI_SUCCESS;

    //ACI_EXCEPTION_END;

    //return ACI_FAILURE;
}

ACI_RC mtdInitialize( mtdModule*** aExtTypeModuleGroup,
                      acp_uint32_t aGroupCnt )
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

    acp_uint32_t   i;
    acp_uint32_t   sStage = 0;
    acp_sint32_t   sModuleIndex = 0;

    mtdModule**    sModule;
    const mtcName* sName;

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

    for( sModule = (mtdModule**) mtdInternalModule; *sModule != NULL; sModule++ )
    {
        mtdAllModule[mtdNumberOfModules] = *sModule;
    
        (*sModule)->initialize( mtdNumberOfModules );
        mtdNumberOfModules++;

        ACE_ASSERT( mtdNumberOfModules < MTD_MAX_DATATYPE_CNT );

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
            mtdAllModule[mtdNumberOfModules] = *sModule;
            (*sModule)->initialize( mtdNumberOfModules );
            mtdNumberOfModules++;
            ACE_ASSERT( mtdNumberOfModules < MTD_MAX_DATATYPE_CNT );

            for( sName = (*sModule)->names;
                 sName != NULL;
                 sName = sName->next )
            {
                mtdNumberOfModulesByName++;
            }
        }
    }

    mtdAllModule[mtdNumberOfModules] = NULL;

//---------------------------------------------------------
// mtdModulesById �� mtdModulesByName�� ����
//---------------------------------------------------------

    ACI_TEST(acpMemCalloc((void**)&mtdModulesById,
                          128,
                          sizeof(mtdIdIndex))
             != ACP_RC_SUCCESS);

    sStage = 1;

    ACI_TEST(acpMemAlloc((void**)&mtdModulesByName,
                         sizeof(mtdNameIndex) *
                         mtdNumberOfModulesByName)
             != ACP_RC_SUCCESS);
    sStage = 2;
    
    mtdNumberOfModulesByName = 0;
    for( sModule = (mtdModule**) mtdAllModule; *sModule != NULL; sModule++ )
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
    acpSortQuickSort( mtdModulesByName, mtdNumberOfModulesByName,
                      sizeof(mtdNameIndex), (acp_sint32_t(*)(const void *, const void *)) mtdCompareByName );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    switch( sStage )
    {
        case 2:
            acpMemFree(mtdModulesByName);
            mtdModulesByName = NULL;
        case 1:
            acpMemFree(mtdModulesById);
            mtdModulesById = NULL;
        default:
            break;
    }

    return ACI_FAILURE;
}

ACI_RC mtdFinalize( void )
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
        acpMemFree(mtdModulesByName);

        mtdModulesByName = NULL;
    }
    if( mtdModulesById != NULL )
    {
        acpMemFree(mtdModulesById);

        mtdModulesById = NULL;
    }

    return ACI_SUCCESS;

/*    ACI_EXCEPTION_END;

return ACI_FAILURE;*/
}

ACI_RC mtdModuleByName( const mtdModule** aModule,
                        const void*       aName,
                        acp_uint32_t      aLength )
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
    acp_sint32_t sMinimum;
    acp_sint32_t sMaximum;
    acp_sint32_t sMedium;
    acp_sint32_t sFound;

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

    ACI_TEST_RAISE( sFound >= (acp_sint32_t)mtdNumberOfModulesByName, ERR_NOT_FOUND );

    *aModule = mtdModulesByName[sFound].module;

    ACI_TEST_RAISE( mtcStrMatch( mtdModulesByName[sFound].name->string,
                                 mtdModulesByName[sFound].name->length,
                                 aName,
                                 aLength ) != 0,
                    ERR_NOT_FOUND );
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        char sNameBuffer[256];

        if( aLength >= sizeof(sNameBuffer) )
        {
            aLength = sizeof(sNameBuffer) - 1;
        }
        acpMemCpy( sNameBuffer, aName, aLength );
        sNameBuffer[aLength] = '\0';
        acpSnprintf( sBuffer, sizeof(sBuffer),
                     "(Name=\"%s\")",
                     sNameBuffer );
        
        aciSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                        sBuffer);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdModuleById( const mtdModule** aModule,
                      acp_uint32_t      aId )
{
/***********************************************************************
 *
 * Description : data type���� mtd module �˻�
 *
 * Implementation :
 *    mtdModuleById���� �ش� data type�� ������ mtd module�� ã����
 *
 ***********************************************************************/
    acp_sint32_t       sFound;

    sFound = ( aId & 0x003F );

    *aModule = mtdModulesById[sFound].module;

    ACI_TEST_RAISE(*aModule == NULL, ERR_NOT_FOUND);
    ACI_TEST_RAISE( (*aModule)->id != aId, ERR_NOT_FOUND );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        acpSnprintf( sBuffer, sizeof(sBuffer),
                     "(type=%"ACI_UINT32_FMT")", aId );
        aciSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                        sBuffer);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdModuleByNo( const mtdModule** aModule,
                      acp_uint32_t      aNo )
{
/***********************************************************************
 *
 * Description : data type no�� mtd module �˻�
 *
 * Implementation :
 *    mtd module number�� �ش� mtd module�� ã����
 *
 ***********************************************************************/

    ACI_TEST_RAISE( aNo >= mtdNumberOfModules, ERR_NOT_FOUND );

    *aModule = mtdAllModule[aNo];

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        acpSnprintf( sBuffer, sizeof(sBuffer),
                     "(no=%"ACI_UINT32_FMT")", aNo );
        aciSetErrorCode(mtERR_ABORT_DATATYPE_MODULE_NOT_FOUND,
                        sBuffer);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_bool_t mtdIsNullDefault( const mtcColumn* aTemp,
                             const void*      aTemp2,
                             acp_uint32_t     aTemp3)
{
    ACP_UNUSED(aTemp);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    
    return ACP_FALSE;
}

ACI_RC mtdEncodeCharDefault( mtcColumn*    aColumn, 
                             void*         aValue,
                             acp_uint32_t  aValueSize ,
                             acp_uint8_t*  aCompileFmt ,
                             acp_uint32_t  aCompileFmtLen ,
                             acp_uint8_t*  aText,
                             acp_uint32_t* aTextLen,
                             ACI_RC*       aRet )
{
    acp_uint32_t sStringLen;
    mtdCharType* sCharVal;

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValueSize);
    ACP_UNUSED(aCompileFmt);
    ACP_UNUSED(aCompileFmtLen);
    
    //----------------------------------
    // Parameter Validation
    //----------------------------------

    ACE_ASSERT( aValue != NULL );
    ACE_ASSERT( aText != NULL );
    ACE_ASSERT( *aTextLen > 0 );
    ACE_ASSERT( aRet != NULL );

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
    if ( mtcdChar.isNull( NULL, aValue, MTD_OFFSET_USELESS ) == ACP_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sStringLen = ( *aTextLen > sCharVal->length )
            ? sCharVal->length : (*aTextLen - 1);
        acpMemCpy(aText, sCharVal->value, sStringLen );
    }

    //----------------------------------
    // Finalization
    //----------------------------------

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    ACI_TEST( sStringLen < sCharVal->length );

    *aRet = ACI_SUCCESS;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    *aRet = ACI_FAILURE;

    return ACI_SUCCESS;
}

ACI_RC mtdEncodeNumericDefault( mtcColumn*    aColumn ,
                                void*         aValue,
                                acp_uint32_t  aValueSize ,
                                acp_uint8_t*  aCompileFmt ,
                                acp_uint32_t  aCompileFmtLen ,
                                acp_uint8_t*  aText,
                                acp_uint32_t* aTextLen,
                                ACI_RC*       aRet )
{
    acp_uint32_t sStringLen;

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValueSize);
    ACP_UNUSED(aCompileFmt);
    ACP_UNUSED(aCompileFmtLen);
    
    //----------------------------------
    // Parameter Validation
    //----------------------------------

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
    if ( mtcdNumeric.isNull( NULL, aValue, MTD_OFFSET_USELESS ) == ACP_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        // To Fix BUG-16802 : copy from mtvCalculate_Float2Varchar()
        ACI_TEST( mtdFloat2String( aText,
                                   47,
                                   & sStringLen,
                                   (mtdNumericType*) aValue )
                  != ACI_SUCCESS );
    }

    //----------------------------------
    // Finalization
    //----------------------------------

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    *aRet = ACI_SUCCESS;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    *aRet = ACI_FAILURE;

    return ACI_FAILURE;
}

ACI_RC mtdDecodeDefault( mtcTemplate*  aTemplate,
                         mtcColumn*    aColumn,
                         void*         aValue,
                         acp_uint32_t* aValueSize,
                         acp_uint8_t*  aCompileFmt ,
                         acp_uint32_t  aCompileFmtLen ,
                         acp_uint8_t*  aText,
                         acp_uint32_t  aTextLen,
                         ACI_RC*        aRet )
{
    acp_uint32_t sValueOffset = 0;

    ACP_UNUSED(aCompileFmt);
    ACP_UNUSED(aCompileFmtLen);
        
    return aColumn->module->value(aTemplate,
                                  aColumn,
                                  aValue,
                                  &sValueOffset,
                                  *aValueSize,
                                  aText,
                                  aTextLen,
                                  aRet) ;
}

ACI_RC mtdEncodeNA( mtcColumn*    aColumn ,
                    void*         aValue ,
                    acp_uint32_t  aValueSize ,
                    acp_uint8_t*  aCompileFmt ,
                    acp_uint32_t  aCompileFmtLen ,
                    acp_uint8_t*  aText,
                    acp_uint32_t* aTextLen,
                    ACI_RC*       aRet )
{
    // To fix BUG-14235
    // return is ACI_SUCCESS.
    // but encoding result is ACI_FAILURE.
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValue);
    ACP_UNUSED(aValueSize);
    ACP_UNUSED(aCompileFmt);
    ACP_UNUSED(aCompileFmtLen);

    aText[0] = '\0';
    *aTextLen = 0;

    *aRet = ACI_FAILURE;

    return ACI_SUCCESS;
}

ACI_RC mtdCompileFmtDefault( mtcColumn*    aColumn ,
                             acp_uint8_t*  aCompiledFmt ,
                             acp_uint32_t* aCompiledFmtLen ,
                             acp_uint8_t*  aFormatString ,
                             acp_uint32_t  aFormatStringLength ,
                             ACI_RC*       aRet )
{
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aCompiledFmt);
    ACP_UNUSED(aCompiledFmtLen);    
    ACP_UNUSED(aFormatString);
    ACP_UNUSED(aFormatStringLength);

    *aRet = ACI_SUCCESS;
    return ACI_SUCCESS;
}

ACI_RC mtdValueFromOracleDefault( mtcColumn*    aColumn,
                                  void*         aValue,
                                  acp_uint32_t* aValueOffset,
                                  acp_uint32_t  aValueSize,
                                  const void*   aOracleValue,
                                  acp_sint32_t  aOracleLength,
                                  ACI_RC*       aResult )
{
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValue);
    ACP_UNUSED(aValueOffset);    
    ACP_UNUSED(aValueSize);
    ACP_UNUSED(aOracleValue);
    ACP_UNUSED(aOracleLength);
    ACP_UNUSED(aResult);

    aciSetErrorCode(mtERR_ABORT_NOT_APPLICABLE);

    return ACI_FAILURE;
}

ACI_RC
mtdMakeColumnInfoDefault( void*        aStmt ,
                          void*        aTableInfo ,
                          acp_uint32_t aIdx )
{
    ACP_UNUSED(aStmt);
    ACP_UNUSED(aTableInfo);
    ACP_UNUSED(aIdx);
    
    return ACI_SUCCESS;
}

acp_double_t
mtdSelectivityNA( void* aColumnMax ,
                  void* aColumnMin ,
                  void* aValueMax  ,
                  void* aValueMin   )
{
    // Selectivity�� ������ �� ���� ����,
    // �� �Լ��� ȣ��Ǿ�� �ȵ�.
    /*   ACI_CALLBACK_FATAL ( "mtdSelectivityNA() : "
         "This Data Type does not Support Selectivity" );BUGBUG*/
    ACP_UNUSED(aColumnMax);
    ACP_UNUSED(aColumnMin);
    ACP_UNUSED(aValueMax);
    ACP_UNUSED(aValueMin);
    
    return 1;
}

acp_double_t
mtdSelectivityDefault( void* aColumnMax ,
                       void* aColumnMin ,
                       void* aValueMax  ,
                       void* aValueMin  )
{
/*******************************************************************
 * Definition
 *      PROJ-1579 NCHAR
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

    ACP_UNUSED(aColumnMax);
    ACP_UNUSED(aColumnMin);
    ACP_UNUSED(aValueMax);
    ACP_UNUSED(aValueMin);
    
    return MTD_DEFAULT_SELECTIVITY;
}

/*******************************************************************
 * Definition
 *    mtdModule���� ����� value �Լ�
 *
 * Description
 *    mtc::value�� ���� ���̾�(QP, MM)���� ����� �� �ִ� ���� value �Լ�
 *    mtd::valueForModule�� mtd������ ����Ѵ�.
 *    �׹�° ������ aDefaultNull�� mtdModule.staticNull�� assign�Ѵ�.
 *
 * by kumdory, 2005-03-15
 *
 *******************************************************************/
const void* mtdValueForModule( const void*  aColumn,
                               const void*  aRow,
                               acp_uint32_t aFlag,
                               const void*  aDefaultNull )
{
    const void* sValue;

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aDefaultNull);
    
    /*BUG-24052*/
    sValue = NULL;

    if( ( aFlag & MTD_OFFSET_MASK ) == MTD_OFFSET_USELESS )
    {
        sValue = aRow;
    }
    else
    {
        ACE_ASSERT(0);
    }

    return sValue;
}

acp_uint32_t
mtdGetDefaultIndexTypeID( const mtdModule* aModule )
{
    return (acp_uint32_t) aModule->idxType[0];
}

acp_bool_t
mtdIsUsableIndexType( const mtdModule* aModule,
                      acp_uint32_t     aIndexType )
{
    acp_uint32_t i;
    acp_bool_t sResult;

    sResult = ACP_FALSE;

    for ( i = 0; i < MTD_MAX_USABLE_INDEX_CNT; i++ )
    {
        if ( (acp_uint32_t) aModule->idxType[i] == aIndexType )
        {
            sResult = ACP_TRUE;
            break;
        }
        else
        {
            // Nothing To Do
        }
    }

    return sResult;
}

ACI_RC
mtdAssignNullValueById( acp_uint32_t  aId,
                        void**        aValue,
                        acp_uint32_t* aSize )
{
/***********************************************************************
 *
 * Description : data type���� mtd module�� staticNull ����
 *
 * Implementation : ( PROJ-1558 )
 *
 ***********************************************************************/
    mtdModule  * sModule;

    ACI_TEST( mtdModuleById( (const mtdModule **) & sModule,
                             aId )
              != ACI_SUCCESS );

    *aValue = sModule->staticNull;
    *aSize = sModule->actualSize( NULL,
                                  *aValue,
                                  MTD_OFFSET_USELESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC
mtdCheckNullValueById( acp_uint32_t aId,
                       void*        aValue,
                       acp_bool_t*  aIsNull )
{
/***********************************************************************
 *
 * Description : data type���� mtd module�� isNull �˻�
 *
 * Implementation : ( PROJ-1558 )
 *
 ***********************************************************************/
    mtdModule  * sModule;

    ACI_TEST( mtdModuleById( (const mtdModule **) & sModule,
                             aId )
              != ACI_SUCCESS );

    if ( sModule->isNull( NULL,
                          aValue,
                          MTD_OFFSET_USELESS ) == ACP_TRUE )
    {
        *aIsNull = ACP_TRUE;
    }
    else
    {
        *aIsNull = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdStoredValue2MtdValueNA( acp_uint32_t aColumnSize ,
                                  void*        aDestValue ,
                                  acp_uint32_t aDestValueOffset ,
                                  acp_uint32_t aLength ,
                                  const void*  aValue  )
{
/***********************************************************************
 * PROJ-1705
 * ������� �ʴ� Ÿ�Կ� ���� ó��   
 ***********************************************************************/
    
    ACP_UNUSED(aColumnSize);
    ACP_UNUSED(aDestValue);
    ACP_UNUSED(aDestValueOffset);
    ACP_UNUSED(aLength);
    ACP_UNUSED(aValue);
    
    ACE_ASSERT( 0 );

    return ACI_SUCCESS;
}


acp_uint32_t mtdNullValueSizeNA()
{
    aciSetErrorCode(mtERR_ABORT_NOT_APPLICABLE);
    ACE_ASSERT( 0 );
    
    return 0;    
}

acp_uint32_t mtdHeaderSizeNA()
{
    aciSetErrorCode(mtERR_ABORT_NOT_APPLICABLE);
    ACE_ASSERT( 0 );
    
    return 0;    
}

acp_uint32_t mtdHeaderSizeDefault()
{
    return 0;    
}

ACI_RC mtdModifyNls4MtdModule()
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1579 NCHAR
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_char_t       * sDBCharSetStr       = NULL;
    acp_char_t       * sNationalCharSetStr = NULL;

    //---------------------------------------------------------
    // DB ĳ���� �°� ���ų� ĳ���� �� ����
    //---------------------------------------------------------

    // CallBack �Լ��� �޷��ִ� smiGetDBCharSet()�� �̿��ؼ� �����´�.
    sDBCharSetStr = getDBCharSet();
    sNationalCharSetStr = getNationalCharSet();

    ACI_TEST( mtlModuleByName( (const mtlModule **) & mtlDBCharSet,
                               sDBCharSetStr,
                               acpCStrLen(sDBCharSetStr, ACP_UINT32_MAX) )
              != ACI_SUCCESS );

    ACI_TEST( mtlModuleByName( (const mtlModule **) & mtlNationalCharSet,
                               sNationalCharSetStr,
                               acpCStrLen(sNationalCharSetStr, ACP_UINT32_MAX) )
              != ACI_SUCCESS );

    ACE_ASSERT( mtlDBCharSet != NULL );
    ACE_ASSERT( mtlNationalCharSet != NULL );

    //----------------------
    // language ���� �缳��
    //----------------------

    // CHAR
    mtcdChar.column->type.languageId = mtlDBCharSet->id;
    mtcdChar.column->language = mtlDBCharSet;

    // VARCHAR
    mtcdVarchar.column->type.languageId = mtlDBCharSet->id;
    mtcdVarchar.column->language = mtlDBCharSet;

    // CLOB
    mtcdClob.column->type.languageId = mtlDBCharSet->id;
    mtcdClob.column->language = mtlDBCharSet;

    // CLOBLOCATOR
/*    mtdClobLocator.column->type.languageId = mtlDBCharSet->id;
      mtdClobLocator.column->language = mtlDBCharSet;*/

    // NCHAR
    mtcdNchar.column->type.languageId = mtlNationalCharSet->id;
    mtcdNchar.column->language = mtlNationalCharSet;

    // NVARCHAR
    mtcdNvarchar.column->type.languageId = mtlNationalCharSet->id;
    mtcdNvarchar.column->language = mtlNationalCharSet;

    // PROJ-2002 Column Security
    // ECHAR
    mtcdEchar.column->type.languageId = mtlNationalCharSet->id;
    mtcdEchar.column->language = mtlNationalCharSet;

    // EVARCHAR
    mtcdEvarchar.column->type.languageId = mtlNationalCharSet->id;
    mtcdEvarchar.column->language = mtlNationalCharSet;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdGetPrecisionNA( const mtcColumn* aColumn ,
                          const void*      aRow ,
                          acp_uint32_t     aFlag ,
                          acp_sint32_t*    aPrecision ,
                          acp_sint32_t*    aScale  )
{
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aRow);
    ACP_UNUSED(aFlag);
    ACP_UNUSED(aPrecision);
    ACP_UNUSED(aScale);

    aciSetErrorCode(mtERR_ABORT_NOT_APPLICABLE);
    return ACI_FAILURE;
}


//---------------------------------------------------------
// compareNumberGroupBigint
//---------------------------------------------------------

void
mtdConvertToBigintType4MtdValue( mtdValueInfo*  aValueInfo,
                                 mtdBigintType* aBigintValue )
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

    ACP_UNUSED(aValueInfo);
    ACP_UNUSED(aBigintValue);

    ACE_ASSERT(0);
}

void
mtdConvertToBigintType4StoredValue( mtdValueInfo*  aValueInfo,
                                    mtdBigintType* aBigintValue )
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
    
    const mtcColumn* sColumn;
    mtdSmallintType  sSmallintValue;
    mtdIntegerType   sIntegerValue;
    
    sColumn    = aValueInfo->column;

    if ( sColumn->type.dataTypeId == MTD_SMALLINT_ID )
    {
        MTC_SHORT_BYTE_ASSIGN( &sSmallintValue, aValueInfo->value );

        if ( sSmallintValue == *((mtdSmallintType*)mtcdSmallint.staticNull) )
        {
            *aBigintValue = *((mtdBigintType*)mtcdBigint.staticNull);
        }
        else
        {
            *aBigintValue = (mtdBigintType)sSmallintValue;
        }
    }
    else if ( aValueInfo->column->type.dataTypeId == MTD_INTEGER_ID )
    {
        MTC_INT_BYTE_ASSIGN( &sIntegerValue, aValueInfo->value );
        
        if ( sIntegerValue == *((mtdIntegerType*)mtcdInteger.staticNull) )
        {
            *aBigintValue = *((mtdBigintType*)mtcdBigint.staticNull);
        }
        else
        {
            *aBigintValue = (mtdBigintType)sIntegerValue;
        }
    }
    else if ( aValueInfo->column->type.dataTypeId == MTD_BIGINT_ID )
    {
        MTC_LONG_BYTE_ASSIGN( aBigintValue, aValueInfo->value );
    }
    else
    {
        ACE_ASSERT(0);
    }
}

acp_sint32_t
mtdCompareNumberGroupBigintMtdMtdAsc( mtdValueInfo* aValueInfo1,
                                      mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtdConvertToBigintType4MtdValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdBigint.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupBigintMtdMtdDesc( mtdValueInfo* aValueInfo1,
                                       mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtdConvertToBigintType4MtdValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdBigint.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupBigintStoredMtdAsc( mtdValueInfo* aValueInfo1,
                                         mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtdConvertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;

    return mtcdBigint.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupBigintStoredMtdDesc( mtdValueInfo* aValueInfo1,
                                          mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;

    mtdConvertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4MtdValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdBigint.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupBigintStoredStoredAsc( mtdValueInfo* aValueInfo1,
                                            mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtdConvertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4StoredValue( aValueInfo2, &sBigintValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdBigint.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupBigintStoredStoredDesc( mtdValueInfo* aValueInfo1,
                                             mtdValueInfo* aValueInfo2 )
{
    mtdBigintType sBigintValue1;
    mtdBigintType sBigintValue2;
    
    mtdConvertToBigintType4StoredValue( aValueInfo1, &sBigintValue1 );
    mtdConvertToBigintType4StoredValue( aValueInfo2, &sBigintValue2 ); 

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sBigintValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sBigintValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdBigint.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

//---------------------------------------------------------
// compareNumberGroupDouble
//---------------------------------------------------------

void
mtdConvertToDoubleType4MtdValue( mtdValueInfo*  aValueInfo,
                                 mtdDoubleType* aDoubleValue )
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

    ACP_UNUSED(aValueInfo);
    ACP_UNUSED(aDoubleValue);

    ACE_ASSERT(0);
}

void
mtdConvertToDoubleType4StoredValue( mtdValueInfo*  aValueInfo,
                                    mtdDoubleType* aDoubleValue )
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

    ACP_UNUSED(aValueInfo);
    ACP_UNUSED(aDoubleValue);

    ACE_ASSERT(0);
}

acp_sint32_t
mtdCompareNumberGroupDoubleMtdMtdAsc( mtdValueInfo* aValueInfo1,
                                      mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtdConvertToDoubleType4MtdValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdDouble.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupDoubleMtdMtdDesc( mtdValueInfo* aValueInfo1,
                                       mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;
    
    mtdConvertToDoubleType4MtdValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdDouble.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupDoubleStoredMtdAsc( mtdValueInfo* aValueInfo1,
                                         mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtdConvertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdDouble.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupDoubleStoredMtdDesc( mtdValueInfo* aValueInfo1,
                                          mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtdConvertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4MtdValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;

    return mtcdDouble.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupDoubleStoredStoredAsc( mtdValueInfo* aValueInfo1,
                                            mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtdConvertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4StoredValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;

    return mtcdDouble.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupDoubleStoredStoredDesc( mtdValueInfo* aValueInfo1,
                                             mtdValueInfo* aValueInfo2 )
{
    mtdDoubleType sDoubleValue1;
    mtdDoubleType sDoubleValue2;

    mtdConvertToDoubleType4StoredValue( aValueInfo1, &sDoubleValue1 );
    mtdConvertToDoubleType4StoredValue( aValueInfo2, &sDoubleValue2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = &sDoubleValue1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = &sDoubleValue2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;

    return mtcdDouble.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

//---------------------------------------------------------
// compareNumberGroupNumeric
//---------------------------------------------------------

void 
mtdConvertToNumericType4MtdValue( mtdValueInfo*    aValueInfo,
                                  mtdNumericType** aNumericValue )
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

    ACP_UNUSED(aValueInfo);
    ACP_UNUSED(aNumericValue);
    
    ACE_ASSERT(0);
}

void
mtdConvertToNumericType4StoredValue( mtdValueInfo*    aValueInfo,
                                     mtdNumericType** aNumericValue,
                                     acp_uint8_t*     aLength,
                                     acp_uint8_t**    aSignExponentMantissa)
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

    ACP_UNUSED(aValueInfo);
    ACP_UNUSED(aNumericValue);
    ACP_UNUSED(aLength);
    ACP_UNUSED(aSignExponentMantissa);
    
    ACE_ASSERT(0);
}

acp_sint32_t
mtdCompareNumberGroupNumericMtdMtdAsc( mtdValueInfo* aValueInfo1,
                                       mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1;
    mtdNumericType* sNumericValuePtr2;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtdConvertToNumericType4MtdValue( aValueInfo1, &sNumericValuePtr1 );
    mtdConvertToNumericType4MtdValue( aValueInfo2, &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sNumericValuePtr1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupNumericMtdMtdDesc( mtdValueInfo* aValueInfo1,
                                        mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1;
    mtdNumericType* sNumericValuePtr2;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtdConvertToNumericType4MtdValue( aValueInfo1,
                                      &sNumericValuePtr1 );
    mtdConvertToNumericType4MtdValue( aValueInfo2,
                                      &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sNumericValuePtr1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;

    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupNumericStoredMtdAsc( mtdValueInfo* aValueInfo1,
                                          mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1;
    mtdNumericType* sNumericValuePtr2;
    acp_uint8_t     sLength1 = 0;
    acp_uint8_t*    sSignExponentMantissa1 = 0;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;

    mtdConvertToNumericType4StoredValue( aValueInfo1,
                                         &sNumericValuePtr1,
                                         &sLength1,
                                         &sSignExponentMantissa1 );
    
    mtdConvertToNumericType4MtdValue( aValueInfo2,
                                      &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa1;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;
    
    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupNumericStoredMtdDesc( mtdValueInfo* aValueInfo1,
                                           mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1 = NULL;
    mtdNumericType* sNumericValuePtr2 = NULL;
    acp_uint8_t     sLength1 = 0;
    acp_uint8_t*    sSignExponentMantissa1 = 0;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;

    mtdConvertToNumericType4StoredValue( aValueInfo1,
                                         &sNumericValuePtr1,
                                         &sLength1,
                                         &sSignExponentMantissa1 );
    
    mtdConvertToNumericType4MtdValue( aValueInfo2,
                                      &sNumericValuePtr2 );

    aValueInfo1->column = NULL;
    aValueInfo1->value  = sSignExponentMantissa1;
    aValueInfo1->length = sLength1;
    aValueInfo1->flag   = MTD_OFFSET_USELESS;
    
    aValueInfo2->column = NULL;
    aValueInfo2->value  = sNumericValuePtr2;
    aValueInfo2->flag   = MTD_OFFSET_USELESS;
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_MTDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupNumericStoredStoredAsc( mtdValueInfo* aValueInfo1,
                                             mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1;
    mtdNumericType* sNumericValuePtr2;
    acp_uint8_t     sLength1= 0;
    acp_uint8_t*    sSignExponentMantissa1 = 0;
    acp_uint8_t     sLength2 = 0;
    acp_uint8_t*    sSignExponentMantissa2 = 0;    

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtdConvertToNumericType4StoredValue( aValueInfo1,
                                         &sNumericValuePtr1,
                                         &sLength1,
                                         &sSignExponentMantissa1 );
    
    mtdConvertToNumericType4StoredValue( aValueInfo2,
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
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_ASCENDING]( aValueInfo1,
                                 aValueInfo2 );
}

acp_sint32_t
mtdCompareNumberGroupNumericStoredStoredDesc( mtdValueInfo* aValueInfo1,
                                              mtdValueInfo* aValueInfo2 )
{
    // �������迭���� NUMERIC ���� ��ȯ��, �÷�ũ�� ����
    // �������� numeric type������ ��ȯ��
    // ������==>BIGINT==>NUMERIC �� ��ȯ ������ ��ġ��,
    // BIGINT==>NUMERIC������ ��ȯ������,
    // NUMERIC type�� ���� ����
    acp_char_t      sNumericValue1[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_char_t      sNumericValue2[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdNumericType* sNumericValuePtr1;
    mtdNumericType* sNumericValuePtr2;
    acp_uint8_t     sLength1 = 0;
    acp_uint8_t*    sSignExponentMantissa1;
    acp_uint8_t     sLength2 = 0;
    acp_uint8_t*    sSignExponentMantissa2 = 0;

    // �������迭���� NUMERIC type������ ��ȯ�ô�
    // ������ data type ������  mtdNumericType�� ����ϱ⶧����
    // �ش� pointer�� ������ �Ǹ�(memcpy�� ���� �ʾƵ� ��),
    // �������迭���� NUMERIC ���� ��ȯ�ô�
    // numeric type�� ���� ������ �ʿ�.
    sNumericValuePtr1 = (mtdNumericType*)sNumericValue1;
    sNumericValuePtr2 = (mtdNumericType*)sNumericValue2;
    
    mtdConvertToNumericType4StoredValue( aValueInfo1,
                                         &sNumericValuePtr1,
                                         &sLength1,
                                         &sSignExponentMantissa1 );
    
    mtdConvertToNumericType4StoredValue( aValueInfo2,
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
    
    return mtcdNumeric.keyCompare[MTD_COMPARE_STOREDVAL_STOREDVAL]
        [MTD_COMPARE_DESCENDING]( aValueInfo1,
                                  aValueInfo2 );
}

mtdCompareFunc 
mtdFindCompareFunc( mtcColumn*   aColumn,
                    mtcColumn*   aValue,
                    acp_uint32_t aCompValueType,
                    acp_uint32_t aDirection )
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
    
    acp_uint32_t   sType;
    acp_uint32_t   sColumn1GroupFlag = 0;
    acp_uint32_t   sColumn2GroupFlag = 0;
    mtdCompareFunc sCompare = NULL;

    if( ( aColumn->module->flag & MTD_GROUP_MASK )
        == MTD_GROUP_TEXT )
    {
        //-------------------------------
        // ������ �迭
        //-------------------------------

        sCompare = mtcdVarchar.keyCompare[aCompValueType][aDirection];
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
            mtdComparisonNumberType[sColumn1GroupFlag][sColumn2GroupFlag];

        switch( sType )
        {
            case ( MTD_NUMBER_GROUP_TYPE_BIGINT ) :
                // �񱳱��� data type�� ������

                sCompare = mtdCompareNumberGroupBigintFuncs[aCompValueType]
                [aDirection];
                break;
            case ( MTD_NUMBER_GROUP_TYPE_DOUBLE ) :
                // �񱳱��� data type�� �Ǽ���

                sCompare = mtdCompareNumberGroupDoubleFuncs[aCompValueType]
                [aDirection];

                break;
            case ( MTD_NUMBER_GROUP_TYPE_NUMERIC ) :
                // �񱳱��� data type�� ������

                sCompare = mtdCompareNumberGroupNumericFuncs[aCompValueType]
                [aDirection];

                break;
            default :
                ACE_ASSERT(0);
        }
    }

    return sCompare;
}

ACI_RC mtdFloat2String( acp_uint8_t*    aBuffer,
                        acp_uint32_t    aBufferLength,
                        acp_uint32_t*   aLength,
                        mtdNumericType* aNumeric )
{
    acp_uint8_t  sString[50];
    acp_sint32_t sLength;
    acp_sint32_t sExponent;
    acp_sint32_t sIterator;
    acp_sint32_t sFence;

    *aLength = 0;

    if( aNumeric->length != 0 )
    {
        if( aNumeric->length == 1 )
        {
            ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
            aBuffer[0] = '0';
            (*aLength)++;
        }
        else
        {
            if( aNumeric->signExponent & 0x80 )
            {
                sExponent = ( (acp_sint32_t)( aNumeric->signExponent & 0x7F ) - 64 )
                    <<  1;
                sLength = 0;
                if( aNumeric->mantissa[0] < 10 )
                {
                    sExponent--;
                    sString[sLength] = '0' + aNumeric->mantissa[0];
                    sLength++;
                }
                else
                {
                    sString[sLength] = '0' + aNumeric->mantissa[0] / 10;
                    sLength++;
                    sString[sLength] = '0' + aNumeric->mantissa[0] % 10;
                    sLength++;
                }
                for( sIterator = 1, sFence = aNumeric->length - 1;
                     sIterator < sFence;
                     sIterator++ )
                {
                    sString[sLength] = '0' + aNumeric->mantissa[sIterator] / 10;
                    sLength++;
                    sString[sLength] = '0' + aNumeric->mantissa[sIterator] % 10;
                    sLength++;
                }
                if( sString[sLength - 1] == '0' )
                {
                    sLength--;
                }
            }
            else
            {
                ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
                aBuffer[0] = '-';
                aBuffer++;
                (*aLength)++;
                aBufferLength--;
                sExponent = ( 64 - (acp_sint32_t)( aNumeric->signExponent & 0x7F ) )
                    << 1;
                sLength = 0;
                if( aNumeric->mantissa[0] >= 90 )
                {
                    sExponent--;
                    sString[sLength] = '0' + 99 - (acp_sint32_t)aNumeric->mantissa[0];
                    sLength++;
                }
                else
                {
                    sString[sLength] = '0' + ( 99 - (acp_sint32_t)aNumeric->mantissa[0] ) / 10;
                    sLength++;
                    sString[sLength] = '0' + ( 99 - (acp_sint32_t)aNumeric->mantissa[0] ) % 10;
                    sLength++;
                }
                for( sIterator = 1, sFence = aNumeric->length - 1;
                     sIterator < sFence;
                     sIterator++ )
                {
                    sString[sLength] = '0' + ( 99 - (acp_sint32_t)aNumeric->mantissa[sIterator] ) / 10;
                    sLength++;
                    sString[sLength] = '0' + ( 99 - (acp_sint32_t)aNumeric->mantissa[sIterator] ) % 10;
                    sLength++;
                }
                if( sString[sLength - 1] == '0' )
                {
                    sLength--;
                }
            }
            if( sExponent > 0 )
            {
                if( sLength <= sExponent )
                {
                    if( sExponent <= (acp_sint32_t)aBufferLength )
                    {
                        for( sIterator = 0; sIterator < sLength; sIterator++ )
                        {
                            aBuffer[sIterator] = sString[sIterator];
                        }
                        for( ; sIterator < sExponent; sIterator++ )
                        {
                            aBuffer[sIterator] = '0';
                        }
                        *aLength += sExponent;
                        goto success;
                    }
                }
                else
                {
                    if( sLength + 1 <= (acp_sint32_t)aBufferLength )
                    {
                        for( sIterator = 0; sIterator < sExponent; sIterator++ )
                        {
                            aBuffer[sIterator] = sString[sIterator];
                        }
                        aBuffer[sIterator] = '.';
                        aBuffer++;
                        for( ; sIterator < sLength; sIterator++ )
                        {
                            aBuffer[sIterator] = sString[sIterator];
                        }
                        *aLength += sLength + 1;
                        goto success;
                    }
                }
            }
            else
            {
                //fix BUG-18163
                if( sLength - sExponent + 2 <= (acp_sint32_t)aBufferLength )
                {
                    sExponent = -sExponent;

                    aBuffer[0] = '0';
                    aBuffer[1] = '.';
                    aBuffer += 2;

                    for( sIterator = 0; sIterator < sExponent; sIterator++ )
                    {
                        aBuffer[sIterator] = '0';
                    }
                    aBuffer += sIterator;
                    for( sIterator = 0; sIterator < sLength; sIterator++ )
                    {
                        aBuffer[sIterator] = sString[sIterator];
                    }
                    *aLength += sLength + sExponent + 2;
                    goto success;
                }
            }
            sExponent--;
            ACI_TEST_RAISE( (acp_sint32_t)aBufferLength < sLength + 1,
                            ERR_INVALID_LENGTH );
            aBuffer[0] = sString[0];
            aBuffer[1] = '.';
            aBuffer++;
            aBufferLength--;
            for( sIterator = 1; sIterator < sLength; sIterator++ )
            {
                aBuffer[sIterator] = sString[sIterator];
            }
            aBuffer       += sLength;
            aBufferLength -= sLength;
            *aLength      += sLength + 1;
            ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
            aBuffer[0] = 'E';
            aBuffer++;
            aBufferLength--;
            (*aLength)++;
            ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
            if( sExponent >= 0 )
            {
                aBuffer[0] = '+';
            }
            else
            {
                sExponent = -sExponent;
                aBuffer[0] = '-';
            }
            aBuffer++;
            aBufferLength--;
            (*aLength)++;
            if( sExponent >= 100 )
            {
                ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
                aBuffer[0] = '0' + sExponent / 100;
                aBuffer++;
                aBufferLength--;
                (*aLength)++;
            }
            if( sExponent >= 10 )
            {
                ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
                aBuffer[0] = '0' + sExponent / 10 % 10;
                aBuffer++;
                aBufferLength--;
                (*aLength)++;
            }
            ACI_TEST_RAISE( aBufferLength < 1, ERR_INVALID_LENGTH );
            aBuffer[0] = '0' + sExponent % 10;
            aBuffer++;
            aBufferLength--;
            (*aLength)++;
        }
    }

  success:

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
