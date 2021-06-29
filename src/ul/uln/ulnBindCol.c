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
#include <ulnBindCol.h>

ACI_RC ulnBindColBody(ulnFnContext *aFnContext,
                      ulnDescRec   *aDescRecArd,
                      acp_sint16_t  aTargetType,
                      void         *aTargetValuePtr,
                      ulvSLen       aBufferLength,
                      ulvSLen      *aStrLenOrIndPtr)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    ulnMetaInitialize(&aDescRecArd->mMeta);

    /*
     * BUGBUG : �ƹ��� �ǹ̰� ������ �ʱ�ȭ�� �ǹ̷� �ϴ� �� ����.
     */
    ulnDescRecSetParamInOut(aDescRecArd, ULN_PARAM_INOUT_TYPE_MAX);

    /*
     * ����� ������ Ÿ���� �����ϴ� meta ����ü �۾�
     */
    ulnMetaBuild4ArdApd(&aDescRecArd->mMeta,
                        ulnTypeMap_SQLC_CTYPE(aTargetType),
                        aTargetType,
                        aBufferLength);

    /* BUG-37101 */
    ulnMetaSetPrecision(&aDescRecArd->mMeta, ULN_NUMERIC_UNDEF_PRECISION);
    ulnMetaSetScale(&aDescRecArd->mMeta, ULN_NUMERIC_UNDEF_SCALE);

    /*
     * ����� ������ �޸𸮸� �����ϴ� desc rec ��� ����
     */
    ulnDescRecSetDataPtr(aDescRecArd, aTargetValuePtr);
    ulnDescRecSetOctetLengthPtr(aDescRecArd, aStrLenOrIndPtr);
    ulnDescRecSetIndicatorPtr(aDescRecArd, aStrLenOrIndPtr);

    /*
     * ARD record �� ARD�� �Ŵޱ�
     */
    ACI_TEST_RAISE(ulnDescAddDescRec(sStmt->mAttrArd, aDescRecArd) != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnBindColBody");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnBindColCheckArgs(ulnFnContext *aContext,
                                  acp_uint16_t  aColumnNumber,
                                  acp_sint16_t  aTargetType,
                                  ulvSLen       aBufferLength)
{
    if (aColumnNumber == 0)
    {
        ACI_TEST_RAISE((aTargetType != SQL_C_BOOKMARK) &&
                       (aTargetType != SQL_C_VARBOOKMARK),
                       LABEL_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK);
    }

    ACI_TEST_RAISE(ulnTypeMap_SQLC_CTYPE(aTargetType) == ULN_CTYPE_MAX,
                   LABEL_INVALID_SQL_C_TYPE);

    /*
     * Note : MS ODBC ������ BufferLength �� 0 �̶� ��� ������,
     *        ������ 0 �̸� ������ �ɰ�������. �Դٰ� MS ODBC ������ X/Open CLI �� ����
     *        ����̹��� ��쿡�� 0 �� �ָ� �ȵȴٰ� ���� �ξ���.
     */
    ACI_TEST_RAISE((aTargetType == SQL_C_CHAR && aBufferLength <= ULN_vLEN(0)) ||
                   (aTargetType == SQL_C_BINARY && aBufferLength <= ULN_vLEN(0)),
                   LABEL_INVALID_BUFF_LEN_0);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK)
    {
        ulnError(aContext, ulERR_ABORT_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK);
    }

    ACI_EXCEPTION(LABEL_INVALID_SQL_C_TYPE)
    {
        /* HY003 */
        ulnError(aContext, ulERR_ABORT_INVALID_APP_BUFFER_TYPE, aTargetType);
    }

    ACI_EXCEPTION(LABEL_INVALID_BUFF_LEN_0)
    {
        /* HY090 */
        ulnError(aContext, ulERR_ABORT_INVALID_BUFFER_LEN, aBufferLength);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnBindCol.
 *
 * SQLBindCol() �Լ��� 1:1 �� ���εǴ� �Լ�.
 *
 * @param[in] aColumnNumber
 *  ODBC 3.0 �� ���� : Number of the result set column to bind.
 *  0 ���� ���۵ȴ�. 0 �� �ϸ�ũ �÷��̴�.
 *  ���� �ϸ�ũ�� ������ ������, ��, SQL_ATTR_USE_BOOKMARKS Statement Attribute ��
 *  SQL_UB_OFF �̸� column number �� 1 ���� ���۵ȴ�.
 * @param[in] aTargetType
 *  The identifier of the C data type of the *TargetValuePtr buffer.
 */
SQLRETURN ulnBindCol(ulnStmt      *aStmt,
                     acp_uint16_t  aColumnNumber,
                     acp_sint16_t  aTargetType,
                     void         *aTargetValuePtr,
                     ulvSLen       aBufferLength,
                     ulvSLen      *aStrLenOrIndPtr)
{
    ULN_FLAG(sNeedExit);

    ulnDescRec   *sDescRecArd;
    ulnFnContext  sFnContext;
    ulnPtContext *sPtContext = NULL;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_BINDCOL, aStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    sPtContext = &(aStmt->mParentDbc->mPtContext);
    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    ACI_TEST_RAISE( (aTargetType == SQL_C_BLOB_LOCATOR ||
                     aTargetType == SQL_C_CLOB_LOCATOR ||
                     aTargetType == SQL_CLOB           ||
                     aTargetType == SQL_BLOB) &&
                    (cmiGetLinkImpl(&sPtContext->mCmiPtContext) == CMI_LINK_IMPL_IPCDA),
                    IPCDANotSupportColType );

    ACI_TEST(ulnBindColCheckArgs(&sFnContext,
                                 aColumnNumber,
                                 aTargetType,
                                 aBufferLength) != ACI_SUCCESS);

    /* ARD record �غ�*/
    ACI_TEST_RAISE(ulnBindArrangeNewDescRec(aStmt->mAttrArd, aColumnNumber, &sDescRecArd)
                   != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    ACI_TEST(ulnBindColBody(&sFnContext,
                            sDescRecArd,
                            aTargetType,
                            aTargetValuePtr,
                            aBufferLength,
                            aStrLenOrIndPtr) != ACI_SUCCESS);

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_MID, NULL, 0,
                  "%-18s| [%2"ACI_UINT32_FMT" ctype: %3"ACI_INT32_FMT
                  " buf: %p len: %3"ACI_INT32_FMT"]", "ulnBindCol",
                  aColumnNumber, aTargetType,
                  aTargetValuePtr, (acp_sint32_t)aBufferLength);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnBindCol");
    }
    ACI_EXCEPTION(IPCDANotSupportColType)
    {
        ulnError(&sFnContext, ulERR_ABORT_IPCDA_UNSUPPORTED_C_DATA_TYPE, aTargetType);
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" ctype: %3"ACI_INT32_FMT
            " buf: %p len: %3"ACI_INT32_FMT"] fail", "ulnBindCol",
            aColumnNumber, aTargetType,
            aTargetValuePtr, (acp_sint32_t)aBufferLength);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
