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
 * $Id: mtl.cpp 36685 2009-11-13 05:31:00Z raysiasd $
 **********************************************************************/

#include <mtce.h>
#include <mtcl.h>
#include <mtcnCaseConvMap.h>
#include <mtcdTypes.h>

#include <acpSearch.h>

extern mtlModule mtclUTF8;
extern mtlModule mtclUTF16;
extern mtlModule mtclAscii;
extern mtlModule mtclKSC5601;
extern mtlModule mtclShiftJIS;
extern mtlModule mtclEUCJP;
extern mtlModule mtclGB231280;
extern mtlModule mtclBig5;
extern mtlModule mtclMS949;
/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
extern mtlModule mtclMS936;
/* PROJ-2590 [��ɼ�] CP932 database character set ���� */
extern mtlModule mtclMS932;
extern mtnNlsCaseConvMap gNlsCaseConvMap[MTN_NLS_CASE_UNICODE_MAX];
extern mtlNCRet mtlUTF8NextCharClobForClient( acp_uint8_t ** aSource,
                                              acp_uint8_t  * acp_uint8_t );

const mtlModule* mtlModules[] = {
    & mtclUTF8,
    & mtclUTF16,
    & mtclAscii,
    & mtclKSC5601,
    & mtclShiftJIS,
    & mtclEUCJP,
    & mtclGB231280,
    & mtclBig5,
    & mtclMS949,
    /* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
    & mtclMS936,
    /* PROJ-2590 [��ɼ�] CP932 database character set ���� */
    & mtclMS932,
    NULL
};

const mtlModule* mtlModulesForClient[] = {
    & mtclUTF8,
    & mtclUTF16,
    & mtclAscii,
    & mtclKSC5601,
    & mtclShiftJIS,
    & mtclEUCJP,
    & mtclGB231280,
    & mtclBig5,
    & mtclMS949,
    /* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
    & mtclMS936,
    /* PROJ-2590 [��ɼ�] CP932 database character set ���� */
    & mtclMS932,
    NULL
};

const mtlModule* mtlDefModule = & mtclAscii;

typedef struct mtlIdIndex {
    acp_uint32_t             id;
    const mtlModule* module;
} mtlIdIndex;

typedef struct mtlNameIndex {
    const mtcName*   name;
    const mtlModule* module;
} mtlNameIndex;

static acp_uint32_t mtlNumberOfModulesById;
static mtlIdIndex* mtlModulesById = NULL;

static acp_uint32_t mtlNumberOfModulesByName;
static mtlNameIndex* mtlModulesByName = NULL;

const acp_uint8_t mtclPC = '%';
const acp_uint8_t mtclUB = '_';
const acp_uint8_t mtclSP = ' ';
const acp_uint8_t mtclTB = '\t';
const acp_uint8_t mtclNL = '\n';
const acp_uint8_t mtclQT = '\'';
const acp_uint8_t mtclBS = '\\';

mtlU16Char mtcl2BPC = { 0x00, '%' };
mtlU16Char mtcl2BUB = { 0x00, ' ' };
mtlU16Char mtcl2BSP = { 0x00, '-' };
mtlU16Char mtcl2BTB = { 0x00, '\t'};
mtlU16Char mtcl2BNL = { 0x00, '\n'};
mtlU16Char mtcl2BQT = { 0x00, '\''};
mtlU16Char mtcl2BBS = { 0x00, '\\'};

// SpecialCharSet�� ��ȸ�� ����ϴ� index�� mtl.h�� ����.
// mtlSpecialCharType.

acp_uint8_t* mtcl1BYTESpecialCharSet[] =
{
    (acp_uint8_t*)&mtclPC,
    (acp_uint8_t*)&mtclUB,
    (acp_uint8_t*)&mtclSP,
    (acp_uint8_t*)&mtclTB,
    (acp_uint8_t*)&mtclNL,
    (acp_uint8_t*)&mtclQT,
    (acp_uint8_t*)&mtclBS
};

acp_uint8_t* mtcl2BYTESpecialCharSet[] =
{
    (acp_uint8_t*)&mtcl2BPC,
    (acp_uint8_t*)&mtcl2BUB,
    (acp_uint8_t*)&mtcl2BSP,
    (acp_uint8_t*)&mtcl2BTB,
    (acp_uint8_t*)&mtcl2BNL,
    (acp_uint8_t*)&mtcl2BQT,
    (acp_uint8_t*)&mtcl2BBS,
};

static acp_sint32_t mtlCompareByName( const mtlNameIndex* aIndex1,
                                      const mtlNameIndex* aIndex2 )
{
/***********************************************************************
 *
 * Description : language name ��
 *
 * Implementation :
 *
 ***********************************************************************/

    return mtcStrMatch( aIndex1->name->string, aIndex1->name->length,aIndex2->name->string, aIndex2->name->length );
}

acp_bool_t mtlIsQuotedName( acp_char_t * aSrcName, acp_sint32_t aSrcLen )
{
/***********************************************************************
 *
 * Description :
 *
 *    �Էµ� name�� Quoted Name���� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 **********************************************************************/

    acp_bool_t sResult;

    sResult = ACP_FALSE;
    
    if ( aSrcName != NULL )
    {
        if ( aSrcLen < 2 )
        {
            // Noting to do.
        }
        else
        {
            // To Fix BUG-17869
            // Ordinary name���� single quoted name�� ����� �� ����.
            if ( ( (aSrcName[0] == '"') && (aSrcName[aSrcLen-1] == '"') ) ||
                 ( (aSrcName[0] == '\'') && (aSrcName[aSrcLen-1] == '\'') ) )
            {
                sResult = ACP_TRUE;
            }
            else
            {
                // Noting to do.
            }
        }
    }
    else
    {
        // Noting to do.
    }

    return sResult;
}


ACI_RC mtlInitialize( acp_char_t   * aDefaultNls, acp_bool_t aIsClient )
{
/***********************************************************************
 *
 * Description : mtl initialize
 *
 * Implementation : language ���� ����
 *
 *   mtlModuleByName
 *    ------------------------
 *   | mtlNameIndex | name   -|---> ASCII
 *   |              | module -|---> mtlEnglish
 *    ------------------------
 *   | mtlNameIndex | name   -|---> KO16KSC5601
 *   |              | module -|---> mtlKorean
 *    ------------------------
 *   |      ...               | 
 *    ------------------------
 *
 ***********************************************************************/

    acp_uint32_t      sStage = 0;
    const mtlModule** sModule;
    const mtlModule** sMtlModules;
    const mtcName*    sName;

    mtlNumberOfModulesById = 0;
    mtlNumberOfModulesByName = 0;

    // PROJ-1579 NCHAR
    if( aIsClient == ACP_TRUE )
    {
        sMtlModules = mtlModulesForClient;
    }
    else
    {
        sMtlModules = mtlModules;
    }

    //---------------------------------------------------------
    // ���� mtlModule�� ����, mtl module name�� ���� mtdModule ���� ����
    // mtlModule�� �Ѱ� �̻��� �̸��� ���� �� ����
    //     ex ) English : ENGLISH, ASCII, US7ASCII
    //          Korean  : KOREAN, KSC5601, KO16KSC5601
    //---------------------------------------------------------
    
    for( sModule = sMtlModules; *sModule != NULL; sModule++ )
    {
        mtlNumberOfModulesById++;
        
        for( sName  = (*sModule)->names;
             sName != NULL;
             sName  = sName->next )
        {
            mtlNumberOfModulesByName++;
        }
    }

    /* BUG-43914 [mt-language] ���� �ٸ� ��η� mtl ��� �ʱ�ȭ�� memory leak �� �߻��մϴ�.
     *  �̹� �ʱ�ȭ�� ���¶��, �ʱ�ȭ ó���� �ǳʶڴ�.
     */
    if ( mtlModulesById == NULL )
    {
        //---------------------------------------------------------
        // mtlModulesById �� mtlModulesByName�� ����
        //---------------------------------------------------------

        ACI_TEST(acpMemAlloc((void**)&mtlModulesById,
                             sizeof(mtlIdIndex) * mtlNumberOfModulesById)
                 != ACP_RC_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    sStage = 1;

    /* BUG-43914 [mt-language] ���� �ٸ� ��η� mtl ��� �ʱ�ȭ�� memory leak �� �߻��մϴ�.
     *  �̹� �ʱ�ȭ�� ���¶��, �ʱ�ȭ ó���� �ǳʶڴ�.
     */
    if ( mtlModulesByName == NULL )
    {
        ACI_TEST(acpMemAlloc((void**)&mtlModulesByName,
                             sizeof(mtlNameIndex) * mtlNumberOfModulesByName)
                 != ACP_RC_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
  
    sStage = 2;
    
    mtlNumberOfModulesById = 0;
    mtlNumberOfModulesByName = 0;
    for( sModule  = sMtlModules; *sModule != NULL; sModule++ )
    {
        // mtlModuleById ����
        mtlModulesById[mtlNumberOfModulesById].id = (*sModule)->id;
        mtlModulesById[mtlNumberOfModulesById].module = *sModule;
        mtlNumberOfModulesById++;
        
        // mtlModuleByName ����
        for( sName  = (*sModule)->names; sName != NULL; sName  = sName->next )
        {
            mtlModulesByName[mtlNumberOfModulesByName].name   = sName;
            mtlModulesByName[mtlNumberOfModulesByName].module = *sModule;
            mtlNumberOfModulesByName++;
        }
    }

    acpSortQuickSort( mtlModulesByName, mtlNumberOfModulesByName,
                      sizeof(mtlNameIndex),(acp_sint32_t(*)(const void *, const void *)) mtlCompareByName );

    ACI_TEST( mtlModuleByName( & mtlDefModule,
                               aDefaultNls,
                               acpCStrLen( aDefaultNls, ACP_SINT32_MAX ) )
              != ACI_SUCCESS );

    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    switch( sStage )
    {
        case 2:
            (void)acpMemFree(mtlModulesByName);
            mtlModulesByName = NULL;
        case 1:
            (void)acpMemFree(mtlModulesById);
            mtlModulesById = NULL;
        default:
            break;
    }

    return ACI_FAILURE;

}

ACI_RC mtlFinalize( void )
{
/***********************************************************************
 *
 * Description : mtl finalize
 *
 * Implementation : language ���� ����� �޸� ���� ����
 *
 ***********************************************************************/
    if( mtlModulesByName != NULL )
    {
        (void)acpMemFree(mtlModulesByName);
        
        mtlModulesByName = NULL;
    }
    if( mtlModulesById != NULL )
    {
        (void)acpMemFree(mtlModulesById);
        mtlModulesById = NULL;
    }

    return ACI_SUCCESS;

    /*
      ACI_EXCEPTION_END;

      return ACI_FAILURE;
    */
    
}

const mtlModule* mtlDefaultModule( void )
{
/***********************************************************************
 *
 * Description : default mtl module ��ȯ
 *
 * Implementation : ALTIBASE_NLS_USE ��ȯ
 *
 ***********************************************************************/
    return mtlDefModule;
}

ACI_RC mtlModuleByName( const mtlModule** aModule,
                        const void*       aName,
                        acp_uint32_t      aLength )
{
/***********************************************************************
 *
 * Description : language name���� mtl module �˻�
 *
 * Implementation :
 *    mtlModuleByName���� �ش� language name�� ������ mtl module�� ã����
 *
 ***********************************************************************/

    mtlNameIndex        sIndex;
    mtcName             sName;
    const mtlNameIndex* sFound;
    acp_rc_t            sRc;
    
    sName.length = aLength;
    sName.string = (void*)aName;
    sIndex.name  = &sName;
    
    sRc = acpSearchBinary( &sIndex,
                           mtlModulesByName,
                           mtlNumberOfModulesByName,
                           sizeof(mtlNameIndex),
                           (acp_sint32_t(*)(const void *, const void *)) mtlCompareByName,
                           &sFound);
    
    ACI_TEST_RAISE( sRc != ACP_RC_SUCCESS, ERR_NOT_FOUND );

    *aModule = sFound->module;

    ACI_TEST_RAISE( *aModule == NULL, ERR_NOT_FOUND );

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
                     "(Language=\"%s\")", sNameBuffer );
        aciSetErrorCode(mtERR_ABORT_LANGUAGE_MODULE_NOT_FOUND,
                        sBuffer);
    }
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtlModuleById( const mtlModule** aModule,
                      acp_uint32_t      aId )
{
/***********************************************************************
 *
 * Description : language id�� mtl module �˻�
 *
 * Implementation :
 *    mtlModuleById���� �ش� language id�� ������ mtl module�� ã����
 *    - ENGLISH = 20000
 *    - KOREAN  = 30000
 *
 ***********************************************************************/

    acp_uint32_t i;
    acp_bool_t sExist = ACP_FALSE;

    for ( i = 0; i < mtlNumberOfModulesById; i++ )
    {
        if ( mtlModulesById[i].id == aId )
        {
            *aModule = mtlModulesById[i].module;
            sExist = ACP_TRUE;
            break;
        }
        else
        {
            // nothing to do
        }
    }    

    ACI_TEST_RAISE(sExist == ACP_FALSE, ERR_NOT_FOUND);

    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_NOT_FOUND )
    {
        char sBuffer[1024];
        
        *aModule = NULL;

        acpSnprintf( sBuffer, sizeof(sBuffer),
                     "(language=%"ACI_UINT32_FMT")", aId );
        aciSetErrorCode(mtERR_ABORT_LANGUAGE_MODULE_NOT_FOUND,
                        sBuffer);
    }
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}


void mtlMakeNameInFunc( const mtlModule * aMtlModule,
                        acp_char_t      * aDstName,
                        acp_char_t      * aSrcName,
                        acp_sint32_t      aSrcLen )
{
/***********************************************************************
 *
 * Description :
 *
 *    �Էµ� name�� �Լ����� ����� �̸����� �����Ѵ�.
 *
 * Implementation :
 *
 *    - Quoted Name�� ���
 *      : �״�� ��� - "Quoted Name" ==> "Quoted Name"
 *    - Non-Quoted Name�� ���
 *      : �빮�ڷ� ���� - NonQuotedName ==> NONQUOTEDNAME
 *
 **********************************************************************/

    acp_char_t  * sIndex;
    acp_char_t  * sFence;

    ACE_ASSERT( aDstName != NULL );

    if ( aSrcName != NULL )
    {
        acpCStrCpy( aDstName, aSrcLen +1 , aSrcName , aSrcLen + 1 );
        
        if ( mtlIsQuotedName( aSrcName, aSrcLen ) != ACP_TRUE )
        {
            // Non-Quoted Name �� ��� �빮�ڷ� �����Ѵ�.
            sIndex = aDstName;
            sFence = sIndex + aSrcLen;

            while ( sIndex < sFence )
            {
                *sIndex = acpCharToUpper( *sIndex );
                aMtlModule->nextCharPtr((acp_uint8_t**)&sIndex, (acp_uint8_t*)sFence);
            }
        }
    
        aDstName[aSrcLen] = '\0';
    }
    else
    {
        aDstName[0] = '\0';
    }
}

ACI_RC mtlMakeNameInSQL( const mtlModule * aMtlModule,
                         acp_char_t      * aDstName,
                         acp_char_t      * aSrcName,
                         acp_sint32_t      aSrcLen )
{
/***********************************************************************
 *
 * Description :
 *
 *    �Էµ� name�� SQL ���峻���� ����� �̸����� ����
 *
 * Implementation :
 *
 *    - Quoted Name�� ���
 *      : Quotation�� ���� - "Quoted Name" ==> Quoted Name
 *    - Non-Quoted Name�� ���
 *      : �빮�ڷ� ���� - NonQuotedName ==> NONQUOTEDNAME
 *
 **********************************************************************/

    acp_char_t  * sIndex;
    acp_char_t  * sFence;

    ACE_ASSERT( aDstName != NULL );

    if ( aSrcName != NULL )
    {
        if ( mtlIsQuotedName( aSrcName, aSrcLen ) == ACP_TRUE )
        {
            // Quoted Name�� ��� Quotation�� �����ϰ� �����Ѵ�.
            aSrcLen = aSrcLen - 2;

            acpCStrCpy( aDstName, aSrcLen + 1 , aSrcName + 1, aSrcLen + 1);
        }
        else
        {
            acpCStrCpy (  aDstName, aSrcLen + 1, aSrcName, aSrcLen + 1 );
            // Non-Quoted Name �� ��� �빮�ڷ� �����Ѵ�.

            sIndex = aDstName;
            sFence = sIndex + aSrcLen;

            while (sIndex < sFence)
            {
                *sIndex = acpCharToUpper( *sIndex );
                // �߸��� ���ڰ� ���͵� error ó������ �ʰ� ��� ����.
                aMtlModule->nextCharPtr((acp_uint8_t**)&sIndex, (acp_uint8_t*)sFence);
            }
        }
        ACI_TEST(aSrcLen < 0);
        
        aDstName[aSrcLen] = '\0';
    }
    else
    {
        aDstName[0] = '\0';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void mtlMakeQuotedName( acp_char_t * aDstName, acp_char_t * aSrcName, acp_sint32_t aSrcLen )
{
/***********************************************************************
 *
 * Description :
 *
 *    �Էµ� name�� ���Ͽ� Quoted Name�� �����Ѵ�.
 *    �����κ��� ���� �̸��� ������ �� ����Ѵ�.
 *
 * Implementation :
 *
 *    �յڷ� Double Quotation�� ���δ�.
 *
 **********************************************************************/

    ACE_ASSERT( aDstName != NULL );

    if ( aSrcName != NULL )
    {
        if ( mtlIsQuotedName( aSrcName, aSrcLen ) == ACP_TRUE )
        {
            acpCStrCpy( aDstName, aSrcLen + 1, aSrcName, aSrcLen + 1 );
            aDstName[aSrcLen] = '\0';
        }
        else
        {
            aDstName[0] = '"';
            acpCStrCpy( aDstName + 1, aSrcLen + 1, aSrcName, aSrcLen + 1 );
            aDstName[aSrcLen + 1] = '"';
            aDstName[aSrcLen + 2] = '\0';
        }
    }
    else
    {
        aDstName[0] = '\0';
    }
}

aciConvCharSetList mtlGetIdnCharSet( const mtlModule  * aCharSet )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1579 NCHAR
 *
 *
 * Implementation :
 *
 *
 **********************************************************************/

    aciConvCharSetList  sCharSet;

    switch( aCharSet->id )
    {
        case MTL_UTF8_ID:
            sCharSet = ACICONV_UTF8_ID;
            break;
        case MTL_UTF16_ID:
            sCharSet = ACICONV_UTF16_ID;
            break;
        case MTL_ASCII_ID:
            sCharSet = ACICONV_ASCII_ID;
            break;
        case MTL_KSC5601_ID:
            sCharSet = ACICONV_KSC5601_ID;
            break;
        case MTL_MS949_ID:
            sCharSet = ACICONV_MS949_ID;
            break;
        case MTL_SHIFTJIS_ID:
            sCharSet = ACICONV_SHIFTJIS_ID;
            break;
        case MTL_EUCJP_ID:
            sCharSet = ACICONV_EUCJP_ID;
            break;
        case MTL_GB231280_ID:
            sCharSet = ACICONV_GB231280_ID;
            break;
        case MTL_BIG5_ID:
            sCharSet = ACICONV_BIG5_ID;
            break;
        /* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
        case MTL_MS936_ID:
            sCharSet = ACICONV_MS936_ID;
            break;
        /* PROJ-2590 [��ɼ�] CP932 database character set ���� */
        case MTL_MS932_ID:
            sCharSet = ACICONV_MS932_ID;
            break;
        default:
            ACE_ASSERT( 0 );
            // Release ��带 ���ؼ�, ���� ���� Character Set�� �����Ѵ�.
            sCharSet = ACICONV_ASCII_ID;
            break;
    }

    return sCharSet;
}

void mtlGetUTF16UpperStr(
    mtlU16Char * aDest,
    mtlU16Char * aSrc )
{
    acp_uint16_t     sWc1;
    acp_uint16_t     sWc2;
    
    ACICONV_UTF16BE_TO_WC( sWc1, aSrc );

    if( sWc1 < MTN_NLS_CASE_UNICODE_MAX )
    {
        sWc2 = gNlsCaseConvMap[sWc1].upper;
        ACICONV_WC_TO_UTF16BE( aDest, sWc2 );
    }
    else
    {
        *aDest = *aSrc;
    }
}

void mtlGetUTF16LowerStr(
    mtlU16Char * aDest,
    mtlU16Char * aSrc )
{
    acp_uint16_t     sWc1;
    acp_uint16_t     sWc2;
    
    ACICONV_UTF16BE_TO_WC( sWc1, aSrc );

    if( sWc1 < MTN_NLS_CASE_UNICODE_MAX )
    {
        sWc2 = gNlsCaseConvMap[sWc1].lower;
        ACICONV_WC_TO_UTF16BE( aDest, sWc2 );
    }
    else
    {
        *aDest = *aSrc;
    }
}

acp_uint8_t mtlGetOneCharSize( acp_uint8_t           * aOneCharPtr,
                               acp_uint8_t           * aFence,
                               const mtlModule * aLanguage )
{
    acp_uint8_t   sSize;
    acp_uint8_t * sNextCharPtr;

    ACE_ASSERT( aOneCharPtr != NULL );
    ACE_ASSERT( aFence != NULL );
    ACE_ASSERT( aLanguage != NULL );

    if ( aOneCharPtr >= aFence )
    {
        sSize = 0;
    }
    else
    {
        sNextCharPtr = aOneCharPtr;
        (void)aLanguage->nextCharPtr(&sNextCharPtr,aFence);

        sSize = sNextCharPtr - aOneCharPtr;
    }

    return sSize;
}

mtlNCRet mtlnextCharClobForClient( acp_uint8_t ** aSource,
                                   acp_uint8_t  * aFence )
{
    return mtlUTF8NextCharClobForClient( aSource, aFence );
}

