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

/*
 * BUGBUG : ���� �ڵ鰣�� ���� ���̿� ���õ� �κ��� ������ ���� ���� �����
 *          Get/Set DescRec/DescField �Լ����� �������̴� ������ �Ұ����ϴ�.
 */

SQLRETURN ulnGetDescRec(ulnDesc      *aDesc,
                        acp_sint16_t  aRecordNumber,
                        acp_char_t   *aName,
                        acp_sint16_t  aBufferLength,
                        acp_sint16_t *aStringLengthPtr,
                        acp_sint16_t *aTypePtr,
                        acp_sint16_t *aSubTypePtr,
                        ulvSLen      *aLengthPtr,
                        acp_sint16_t *aPrecisionPtr,
                        acp_sint16_t *aScalePtr,
                        acp_sint16_t *aNullablePtr)
{
    ULN_FLAG(sNeedExit);
    ulnFnContext  sFnContext;

    ulnDescRec   *sDescRec    = NULL;

    ulnStmt      *sParentStmt = NULL;
    ulnDbc       *sParentDbc  = NULL;

    acp_uint32_t  sLength;
    acp_char_t   *sSourceBuffer;
    acp_sint16_t  sSQLTYPE    = 0;

    acp_bool_t    sLongDataCompat = ACP_FALSE;

    ulnDescType   sDescType;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_GETDESCREC, aDesc, ULN_OBJ_TYPE_DESC);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /*
     * ====================================
     * Function Body BEGIN
     * ====================================
     */

    sDescType = ULN_OBJ_GET_DESC_TYPE(aDesc);

    ACI_TEST_RAISE(sDescType != ULN_DESC_TYPE_ARD &&
                   sDescType != ULN_DESC_TYPE_APD &&
                   sDescType != ULN_DESC_TYPE_IRD &&
                   sDescType != ULN_DESC_TYPE_IPD,
                   LABEL_INVALID_HANDLE);

    switch (ULN_OBJ_GET_TYPE(aDesc->mParentObject))
    {
        case ULN_OBJ_TYPE_STMT:
            sParentStmt = (ulnStmt *)aDesc->mParentObject;

            if (sDescType == ULN_DESC_TYPE_IRD)
            {
                ACI_TEST_RAISE(ulnCursorGetState(&sParentStmt->mCursor) == ULN_CURSOR_STATE_CLOSED,
                               LABEL_NO_DATA);
            }

            sLongDataCompat = ulnDbcGetLongDataCompat(sParentStmt->mParentDbc);

            break;

        case ULN_OBJ_TYPE_DBC:
            sParentDbc      = (ulnDbc *)aDesc->mParentObject;
            sLongDataCompat = ulnDbcGetLongDataCompat(sParentDbc);
            break;

        default:
            ACI_RAISE(LABEL_INVALID_HANDLE);
            break;
    }

    ACI_TEST_RAISE(aRecordNumber > ulnDescGetHighestBoundIndex(aDesc), LABEL_NO_DATA);
    ACI_TEST_RAISE(aRecordNumber <= 0, LABEL_INVALID_INDEX);

    sDescRec = ulnDescGetDescRec(aDesc, aRecordNumber);
    ACI_TEST_RAISE(sDescRec == NULL, LABEL_MEM_MAN_ERR2);

    /*
     * --------------------------------------------------------------
     *  Name             : SQL_DESC_NAME �ʵ��� �� ����
     *  aBufferLength    : aName �� ����Ű�� ������ size
     *  aStringLengthPtr : ���� SQL_DESC_NAME �ʵ忡 �ִ� �̸��� ����
     * --------------------------------------------------------------
     */

    if (aName != NULL)
    {
        ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);        //BUG-28623 [CodeSonar]Ignored Return Value
        /*
         * BUGBUG : ulnColAttribute() �� SQL_DESC_NAME �κа� �ߺ��� �ڵ�
         */
        /* BUGBUG (BUG-33625) */
        sSourceBuffer = ulnDescRecGetDisplayName(sDescRec);
        if(sSourceBuffer == NULL)
        {
            sLength = 0;
        }
        else
        {
            sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
        }

        ulnDataWriteStringToUserBuffer(&sFnContext,
                                       sSourceBuffer,
                                       sLength,
                                       aName,
                                       aBufferLength,
                                       aStringLengthPtr);
    }

    /*
     * --------------------------------------------------------------
     *  aTypePtr : SQL_DESC_TYPE �ʵ��� �� ����
     * --------------------------------------------------------------
     */

    if (aTypePtr != NULL)
    {
        /*
         * BUGBUG : ulnColAttribute() �� SQL_DESC_NAME �κа� �ߺ��� �ڵ�
         *
         * verbose data type �� �����ؾ� �Ѵ�.
         * ulnMeta �� odbc type ���� verbose type �� ����.
         */

        /*
         * Note : ulnTypeMap_LOB_SQLTYPE :
         *        ��ó�� ulnTypes.cpp ���� �ٺ������� �������� �ʰ�,
         *        ����ڿ��� Ÿ���� �����ִ� �Լ����� �׶��׶� long type �� �����ϴ�
         *        �Լ��� ȣ���ϴ� ���� ������ ������ �ְ� ������ ��������,
         *        ulnTypes.cpp �� function context, dbc, stmt ���� �������� �ٸ�
         *        �͵��� �޴� �Լ��� ����� ���� �ʾƼ� ���� �̿Ͱ��� �ߴ�.
         */

        sSQLTYPE = ulnMetaGetOdbcType(&sDescRec->mMeta);
        /* PROJ-2638 shard native linker */
        if ( sSQLTYPE >= ULSD_INPUT_RAW_MTYPE_NULL )
        {
            sSQLTYPE = ulnTypeMap_MTYPE_SQL( sSQLTYPE - ULSD_INPUT_RAW_MTYPE_NULL );
        }
        else
        {
            /* Do Nothing. */
        }
        sSQLTYPE = ulnTypeMap_LOB_SQLTYPE(sSQLTYPE, sLongDataCompat);
        // BUG-21730: wrong type(SInt) casting
        *aTypePtr = sSQLTYPE;
    }

    /*
     * ----------------------------------------------------------------------------
     *  aSubTypePtr : type �� SQL_DATETIME �̳� SQL_INTERVAL �� ��
     *                SQL_DESC_DATETIME_INTERVAL_CODE �� �����Ѵ�.
     *
     *  msdn �� ������ �о��, type �� SQL_DATETIME �� ������ �����ϴ� �� ����.
     *                           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     * ----------------------------------------------------------------------------
     */

    // To Fix BUG-20651
    if (aSubTypePtr != NULL)
    {
        if ( (sSQLTYPE == SQL_DATETIME) || (sSQLTYPE == SQL_INTERVAL) )
        {
            *aSubTypePtr = ulnMetaGetOdbcDatetimeIntCode(&sDescRec->mMeta);
        }
        else
        {
            *aSubTypePtr = 0;
        }
    }

    /*
     * ----------------------------------------------------------------------------
     *  aLengthPtr : SQL_DESC_OCTET_LENGTH �� ����
     * ----------------------------------------------------------------------------
     */

    if (aLengthPtr != NULL)
    {
        /*
         * BUGBUG : ulnColAttribute() �� SQL_DESC_NAME �κа� �ߺ��� �ڵ�
         *
         * IRD �� octet length �� null �����ڸ� ������ ���̰� ����.
         * �׷��� SQLColAttribute �Լ��� ������ �� field id �� ���ϵǴ� ������
         * null �������� ���̱��� �����Ѵٰ� �Ǿ� �ִ�.
         */
        *aLengthPtr = ulnMetaGetOctetLength(&sDescRec->mMeta) +
                                                ULN_SIZE_OF_NULLTERMINATION_CHARACTER;
    }

    /*
     * ----------------------------------------------------------------------------
     *  aPrecisionPtr : SQL_DESC_PRECISION �� ����
     * ----------------------------------------------------------------------------
     */

    if (aPrecisionPtr != NULL)
    {
        *aPrecisionPtr = ulnMetaGetPrecision(&sDescRec->mMeta);
    }

    /*
     * ----------------------------------------------------------------------------
     *  aScalePtr : SQL_DESC_SCALE �� ����
     * ----------------------------------------------------------------------------
     */

    if (aScalePtr != NULL)
    {
        *aScalePtr = ulnMetaGetScale(&sDescRec->mMeta);
    }

    /*
     * ----------------------------------------------------------------------------
     *  aNullablePtr : SQL_DESC_NULLABLE �� ����
     * ----------------------------------------------------------------------------
     */

    if (aNullablePtr != NULL)
    {
        *aNullablePtr = ulnMetaGetNullable(&sDescRec->mMeta);
    }

    /*
     * ====================================
     * Function Body END
     * ====================================
     */

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_INVALID_BUFFERSIZE)
    {
        /*
         * HY090 : ����� �ϴ� ������ ���� Ÿ���ε�, aBufferLength �� ������ �־��� ���
         */
        ulnError(&sFnContext, ulERR_ABORT_INVALID_BUFFER_LEN, aBufferLength);
    }

    ACI_EXCEPTION(LABEL_NO_DATA)
    {
        ULN_FNCONTEXT_SET_RC(&sFnContext, SQL_NO_DATA);
    }

    ACI_EXCEPTION(LABEL_INVALID_HANDLE)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_HANDLE);
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR2)
    {
        /*
         * BUGBUG : �̿Ͱ��� ��쿡�� ����Ʈ���� �����ؾ� ������
         *          ������ ���� ����� �ϴ� �޸� �Ŵ��� ������ �����Ѵ�.
         */
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnColAttribute2");
    }

    ACI_EXCEPTION(LABEL_INVALID_INDEX)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aRecordNumber);
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

