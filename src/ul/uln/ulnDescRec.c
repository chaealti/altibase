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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnDescRec.h>
#include <ulnPDContext.h>

ACI_RC ulnDescRecInitialize(ulnDesc *aParentDesc, ulnDescRec *aRecord, acp_uint16_t aIndex)
{
    aRecord->mIndex         = aIndex;
    aRecord->mParentDesc    = aParentDesc;

    acpListInit(&aRecord->mList);

    /*
     * Meta ���� �ʱ�ȭ
     */
    ulnMetaInitialize(&aRecord->mMeta);

    ulnPDContextCreate(&aRecord->mPDContext);

    /*
     * ODBC ǥ�� �Ӽ���
     */
    aRecord->mInOutType          = ULN_PARAM_INOUT_TYPE_MAX;
    aRecord->mDisplaySize        = 0;

    /*
     * BUGBUG : M$ ODBC ���� ���ǵǾ� ������, ���� uln ������ ���ǰ� ���� ���� �Ӽ�.
     *          ��� �ؾ� �ҷ����� �� �𸣰ڴ�.
     */
    aRecord->mAutoUniqueValue   = 0;
    aRecord->mCaseSensitive     = 0;

    aRecord->mDataPtr           = NULL;
    aRecord->mIndicatorPtr      = NULL;
    aRecord->mOctetLengthPtr    = NULL;
    aRecord->mFileOptionsPtr    = NULL;

    aRecord->mDisplayName[0]    = '\0';
    aRecord->mColumnName[0]     = '\0';
    aRecord->mBaseColumnName[0] = '\0';
    aRecord->mTableName[0]      = '\0';
    aRecord->mBaseTableName[0]  = '\0';
    aRecord->mSchemaName[0]     = '\0';
    aRecord->mCatalogName[0]    = '\0';

    aRecord->mTypeName          = (acp_char_t *)"";
    aRecord->mLocalTypeName     = (acp_char_t *)"";
    aRecord->mLiteralPrefix     = (acp_char_t *)"";
    aRecord->mLiteralSuffix     = (acp_char_t *)"";

    aRecord->mRowVer            = 0;
    aRecord->mSearchable        = 0;
    aRecord->mUnnamed           = 0;
    aRecord->mUnsigned          = SQL_FALSE;    // ��Ƽ���̽��� ��� ���ڰ� signed �̴�.
    aRecord->mUpdatable         = SQL_ATTR_READONLY;

    /* PROJ-2616 */
    aRecord->mMaxByteSize       = 0;

    /* PROJ-2739 */
    aRecord->mShardLocator      = NULL;
    
    return ACI_SUCCESS;
}

void ulnDescRecInitOutParamBuffer(ulnDescRec *aDescRec)
{
    aDescRec->mOutParamBufferSize = 0;
    aDescRec->mOutParamBuffer     = NULL;
}

void ulnDescRecFinalizeOutParamBuffer(ulnDescRec *aDescRec)
{
    aDescRec->mOutParamBufferSize = 0;

    if (aDescRec->mOutParamBuffer != NULL)
    {
        acpMemFree(aDescRec->mOutParamBuffer);
    }

    aDescRec->mOutParamBuffer = NULL;
}

ACI_RC ulnDescRecPrepareOutParamBuffer(ulnDescRec *aDescRec, acp_uint32_t aNewSize, ulnMTypeID aMTYPE)
{
    uluMemory *sMemory;
    ulnColumn *sOutParamBuffer = NULL;

    // PROJ-1752 LIST PROTOCOL
    if( aMTYPE == ULN_MTYPE_BLOB_LOCATOR ||
        aMTYPE == ULN_MTYPE_CLOB_LOCATOR )
    {
        aNewSize = ACI_SIZEOF(ulnLob);
    }

    if (aDescRec->mOutParamBufferSize < aNewSize)
    {
        /*
         * BUGBUG : ���� �޸𸮸� �Ҵ��� �ʿ䰡 ���� ��쿡�� ������ �Ҵ��� ���� �׳�
         *          �����ϰ� ���� �Ҵ��Ѵ�.
         *          �̷� ���� ulnDesc �� ����� �����ִ� �޸𸮴� ū ������ ���� ��������
         *          �Ǵ��ϰ� �ϴ� �����Ѵ�.
         *
         *          ���Ŀ� ul �� �޸� ������ ������ �� �Բ� �ذ��ϵ��� �Ѵ�.
         */

        sMemory = aDescRec->mParentDesc->mObj.mMemory;

        ACI_TEST(sMemory->mOp->mMalloc(sMemory,
                                       (void **)&sOutParamBuffer,
                                       ACI_SIZEOF(ulnColumn) + aNewSize) != ACI_SUCCESS);

        aDescRec->mOutParamBuffer     = sOutParamBuffer;
        aDescRec->mOutParamBufferSize = aNewSize;
    }

    /*
     * BUGBUG : aDescRec->mOutParamBuffer �� null ���� üũ�� �ؾ� �ϳ� ���ƾ� �ϳ�...
     */
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mColumnNumber  = aDescRec->mIndex;
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mMtype         = aMTYPE;
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mGDPosition    = 0;
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mRemainTextLen = 0;
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mBuffer        = (acp_uint8_t*)(aDescRec->mOutParamBuffer + 1);
    ((ulnColumn *)(aDescRec->mOutParamBuffer))->mBuffer[aDescRec->mOutParamBufferSize] = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void ulnDescRecInitLobArray(ulnDescRec *aDescRec)
{
    aDescRec->mLobArray = NULL;
}

ACI_RC ulnDescRecCreate(ulnDesc *aParentDesc, ulnDescRec **aDescRec, acp_uint16_t aIndex)
{
    uluMemory   *sParentMemory = aParentDesc->mObj.mMemory;
    ulnDescType  sDescType     = ULN_OBJ_GET_DESC_TYPE(aParentDesc);
    ulnDescRec  *sDescRec      = NULL;

    /* PROJ-1789 Updatable Scrollable Cursor
     * BOOKMARK ���ε��� �� �� �ֵ��� ���� ����.
     * unsigned�̹Ƿ� >= 0 ������ �׻� �����̴�. �׷��Ƿ� �����ص� OK */
    ACP_UNUSED(aIndex);

    /*
     * Desc���ð� �Ѱ��� �༮�� ������ Descriptor�̸� ��ȿ�� ������ Descriptor���� Ȯ��
     */

    ACE_ASSERT(ULN_OBJ_GET_TYPE(aParentDesc) == ULN_OBJ_TYPE_DESC);

    ACE_ASSERT(ULN_DESC_TYPE_NODESC < sDescType &&
                                      sDescType < ULN_DESC_TYPE_MAX);

    *aDescRec = NULL;

    /* BUG-44858 mFreeDescRecList�� ���� Ȯ������. */
    if (acpListIsEmpty(&aParentDesc->mFreeDescRecList) != ACP_TRUE)
    {
        sDescRec = (ulnDescRec *)acpListGetFirstNode(&aParentDesc->mFreeDescRecList);
        acpListDeleteNode((acp_list_node_t *)sDescRec);
    }
    else
    {
        ACI_TEST(sParentMemory->mOp->mMalloc(sParentMemory,
                                             (void **)&sDescRec,
                                             ACI_SIZEOF(ulnDescRec)) != ACI_SUCCESS);
    }

    *aDescRec = sDescRec;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnDescRec::mLobArray ���� �Լ���
 */

ulnLob *ulnDescRecGetLobElement(ulnDescRec *aDescRec, acp_uint32_t aRowNumber)
{
    ulnLob *aReturnLob;

    /*
     * Note : ULU_ARRAY_IGNORE �̸� ������ ACI_SUCCESS �� �����Ѵ�.
     */
    uluArrayGetElement(aDescRec->mLobArray, aRowNumber, (void **)&aReturnLob, ULU_ARRAY_IGNORE);

    return aReturnLob;
}

ACI_RC ulnDescRecArrangeLobArray(ulnDescRec *aDescRec, acp_uint32_t aArrayCount)
{
    uluMemory *sMemory;

    if (aDescRec->mLobArray == NULL)
    {
        sMemory = aDescRec->mParentDesc->mObj.mMemory;

        ACI_TEST(uluArrayCreate(sMemory,
                                &aDescRec->mLobArray,
                                ACI_SIZEOF(ulnLob),
                                20,
                                NULL) != ACI_SUCCESS);
    }

    /*
     * Array �� aArrayCount ���� element �� �� ���� Ȯ��.
     */
    ACI_TEST(uluArrayReserve(aDescRec->mLobArray, aArrayCount) != ACI_SUCCESS);

    /*
     * ��� ������ 0 ���� �ʱ�ȭ
     */
    uluArrayInitializeContent(aDescRec->mLobArray);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * �÷��� � �������� �Ǵ��ϴ� �Լ���.
 *      1. Memory bound LOB column
 *      2. DATA AT EXEC column
 */

acp_bool_t ulnDescRecIsDataAtExecParam(ulnDescRec *aAppDescRec, acp_uint32_t aRow)
{
    ulvSLen *sUserOctetLengthPtr = NULL;

    sUserOctetLengthPtr = ulnBindCalcUserOctetLengthAddr(aAppDescRec, aRow);

    if (sUserOctetLengthPtr != NULL)
    {
        if (*sUserOctetLengthPtr <= SQL_LEN_DATA_AT_EXEC_OFFSET ||
            *sUserOctetLengthPtr == SQL_DATA_AT_EXEC)
        {
            return ACP_TRUE;
        }
    }

    return ACP_FALSE;
}

/* PROJ-1789 Updatable Scrollable Cursor */

/**
 * DisplayName�� �����Ѵ�.
 *
 * @param[in] aRecord         IRD Record
 * @param[in] aDisplayName    DisplayName
 * @param[in] aDisplayNameLen DisplayName ����
 */
void ulnDescRecSetDisplayName(ulnDescRec *aRecord, acp_char_t *aDisplayName, acp_size_t aDisplayNameLen)
{
    acp_size_t sLen = ACP_MIN(aDisplayNameLen, ACI_SIZEOF(aRecord->mDisplayName) - 1);
    acpMemCpy(aRecord->mDisplayName, aDisplayName, sLen);
    aRecord->mDisplayName[sLen] = '\0';
}

/**
 * ColumnName�� �����Ѵ�.
 *
 * @param[in] aRecord        IRD Record
 * @param[in] aColumnName    ColumnName
 * @param[in] aColumnNameLen ColumnName ����
 */
void ulnDescRecSetColumnName(ulnDescRec *aRecord, acp_char_t *aColumnName, acp_size_t aColumnNameLen)
{
    acp_size_t sLen = ACP_MIN(aColumnNameLen, ACI_SIZEOF(aRecord->mColumnName) - 1);
    acpMemCpy(aRecord->mColumnName, aColumnName, sLen);
    aRecord->mColumnName[sLen] = '\0';

    /* SQL_DESC_UNNAMED */
    if (sLen == 0)
    {
        ulnDescRecSetUnnamed(aRecord, SQL_UNNAMED);
    }
    else
    {
        ulnDescRecSetUnnamed(aRecord, SQL_NAMED);
    }
}

/**
 * BaseColumnName�� �����Ѵ�.
 *
 * @param[in] aRecord            IRD Record
 * @param[in] aBaseColumnName    BaseColumnName
 * @param[in] aBaseColumnNameLen BaseColumnName ����
 */
void ulnDescRecSetBaseColumnName(ulnDescRec *aRecord, acp_char_t *aBaseColumnName, acp_size_t aBaseColumnNameLen)
{
    acp_size_t sLen = ACP_MIN(aBaseColumnNameLen, ACI_SIZEOF(aRecord->mBaseColumnName) - 1);
    acpMemCpy(aRecord->mBaseColumnName, aBaseColumnName, sLen);
    aRecord->mBaseColumnName[sLen] = '\0';
}

/**
 * TableName�� �����Ѵ�.
 *
 * @param[in] aRecord       IRD Record
 * @param[in] aTableName    TableName
 * @param[in] aTableNameLen TableName ����
 */
void ulnDescRecSetTableName(ulnDescRec *aRecord, acp_char_t *aTableName, acp_size_t aTableNameLen)
{
    acp_size_t sLen = ACP_MIN(aTableNameLen, ACI_SIZEOF(aRecord->mTableName) - 1);
    acpMemCpy(aRecord->mTableName, aTableName, sLen);
    aRecord->mTableName[sLen] = '\0';
}

/**
 * BaseTableName�� �����Ѵ�.
 *
 * @param[in] aRecord           IRD Record
 * @param[in] aBaseTableName    BaseTableName
 * @param[in] aBaseTableNameLen BaseTableName ����
 */
void ulnDescRecSetBaseTableName(ulnDescRec *aRecord, acp_char_t *aBaseTableName, acp_size_t aBaseTableNameLen)
{
    acp_size_t sLen = ACP_MIN(aBaseTableNameLen, ACI_SIZEOF(aRecord->mBaseTableName) - 1);
    acpMemCpy(aRecord->mBaseTableName, aBaseTableName, sLen);
    aRecord->mBaseTableName[sLen] = '\0';
}

/**
 * SchemaName(User Name)�� �����Ѵ�.
 *
 * @param[in] aRecord        IRD Record
 * @param[in] aSchemaName    SchemaName
 * @param[in] aSchemaNameLen SchemaName ����
 */
void ulnDescRecSetSchemaName(ulnDescRec *aRecord, acp_char_t *aSchemaName, acp_size_t aSchemaNameLen)
{
    acp_size_t sLen = ACP_MIN(aSchemaNameLen, ACI_SIZEOF(aRecord->mSchemaName) - 1);
    acpMemCpy(aRecord->mSchemaName, aSchemaName, sLen);
    aRecord->mSchemaName[sLen] = '\0';
}

/**
 * CatalogName(DB Name)�� �����Ѵ�.
 *
 * @param[in] aRecord         IRD Record
 * @param[in] aCatalogName    CatalogName
 * @param[in] aCatalogNameLen CatalogName ����
 */
void ulnDescRecSetCatalogName(ulnDescRec *aRecord, acp_char_t *aCatalogName, acp_size_t aCatalogNameLen)
{
    acp_size_t sLen = ACP_MIN(aCatalogNameLen, ACI_SIZEOF(aRecord->mCatalogName) - 1);
    acpMemCpy(aRecord->mCatalogName, aCatalogName, sLen);
    aRecord->mCatalogName[sLen] = '\0';
}
