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
#include <ulnFetch.h>
#include <ulnExtendedFetch.h>

/*
 * ULN_SFID_52
 * SQLExtendedFetch, STMT, S5
 *      S7 [s] or [nf]
 *      S11 [x]
 *  where
 *      [s]  Success. The function returned SQL_SUCCESS_WITH_INFO or SQL_SUCCESS.
 *      [nf] Not found. The function returned SQL_NO_DATA.
 *           This does not apply when SQLExecDirect, SQLExecute, or SQLParamData returns
 *           SQL_NO_DATA after executing a searched update or delete statement.
 */
ACI_RC ulnSFID_52(ulnFnContext *aFnContext)
{
    if (aFnContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS_WITH_INFO ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
        {
            /* [s] or [nf] */
            ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, ULN_S_S7);
        }
    }

    return ACI_SUCCESS;
}

/*
 * ===========================================
 * SQLExtendedFetch()
 * ===========================================
 */

/*
 * Note : 64bit odbc ���� SQLROWSETSIZE �� SQLUINTEGER �̴�. �� 32��Ʈ �����̴�.
 *        ExtendedFetch �� 4��° parameter �� 64��Ʈ�� �ƴ϶� 32��Ʈ�̴�.
 */

SQLRETURN ulnExtendedFetch(ulnStmt      *aStmt,
                           acp_uint16_t  aOrientation,
                           ulvSLen       aOffset,
                           acp_uint32_t *aRowCountPtr,
                           acp_uint16_t *aRowStatusArray)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);
    ULN_FLAG(sNeedRecoverRowStatusPtr);

    ulnFnContext  sFnContext;
    acp_uint32_t  sNumberOfRowsFetched = 0;
    acp_uint16_t *sOriginalRowStatusArray;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_EXTENDEDFETCH, aStmt, ULN_OBJ_TYPE_STMT);

    /*
     * enter
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /*
     * ===========================================
     * INITIALIZE
     * ===========================================
     */

    /*
     * Note : �Ʒ��� ���� �ٲ�ġ�Ⱑ ������ ������ stmt �� �������� �ʱ� �����̴�.
     *        dbc �������� exclusive �� lock �� ��� ������ ����� �� stmt �� �ΰ��� �Լ���
     *        ������� �ʴ´�.
     *        ���� �Ʒ�ó�� �ٲ�ġ�� �ؼ� �����ϰ� ����� �� �ִ�.
     */

    /*
     * �ٲ�ġ�� 1 : SQL_ATTR_ROW_STATUS_PTR
     * ------------------------------------
     * Row status array 4 extended fetch �� �����Ѵ�.
     * stmt �� �Ӽ����ν�, SQLFetchScroll() �Լ��� ����ϴ� SQL_ATTR_ROW_STATUS_PTR ����
     * �ٸ� ���۸� ��� �Ѵٰ� ODBC ���� �̾߱��ϰ� �ִ�.
     *
     * �ϴ� �ٲ�ġ�⸦ �� ��, ���߿� �������� �θ� �ȴ�.
     */

    sOriginalRowStatusArray = ulnStmtGetAttrRowStatusPtr(aStmt);
    ulnStmtSetAttrRowStatusPtr(aStmt, aRowStatusArray);

    ULN_FLAG_UP(sNeedRecoverRowStatusPtr);

    // fix BUG-20745
    // http://msdn.microsoft.com/en-us/library/ms713591.aspx
    // SQLExtendedFetch������ SQL_ATTR_ROW_BIND_OFFSET_PTR�� ����� �� ������
    // MSDASQL���� �̷��� ȣ���ϰ� ����.
    // ���� SQLExtendedFetch������ SQL_ATTR_ROW_BIND_OFFSET_PTR�� ����� �� �ְ� ������

    /*
     * ===========================================
     * MAIN
     * ===========================================
     */

    ACI_TEST(ulnCheckFetchOrientation(&sFnContext, aOrientation) != ACI_SUCCESS);

    /*
     * Protocol Context �ʱ�ȭ
     */
    //fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(aStmt->mParentDbc->mPtContext),
                                          &(aStmt->mParentDbc->mSession)) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);

    if (aRowCountPtr != NULL)
    {
        *aRowCountPtr = sNumberOfRowsFetched;
    }
    //fix BUG-17722
    ACI_TEST(ulnFetchCore(&sFnContext,
                          &(aStmt->mParentDbc->mPtContext),
                          aOrientation,
                          aOffset,
                          &sNumberOfRowsFetched) != ACI_SUCCESS);

    /*
     * ����ڿ��� ��� row �� ��ġ�� �Դ��� ������ �ش�.
     */
    if (aRowCountPtr != NULL)
    {
        *aRowCountPtr = sNumberOfRowsFetched;
    }

    /*
     * �����͸� fetch �� ���� ���� row ���� status array �� SQL_ROW_NOROW �� ����� �ش�.
     *
     * block cursor �� ����� ��,
     * ���� 10 ���� ����ڰ� ��ġ�ߴµ�, �����ʹ� 5�ٸ� ���� ���,
     * ������ 5 ���� row status ptr ���� SQL_ROW_NOROW �� �־� ��� �Ѵ�.
     */
    ulnStmtInitRowStatusArrayValue(aStmt,
                                   sNumberOfRowsFetched,
                                   ulnStmtGetAttrRowArraySize(aStmt),
                                   SQL_ROW_NOROW);

    /*
     * Note : ���� ���Ͽ� �־ ��� row �� error �̸� extended fetch �� SQL_SUCCESS_WITH_INFO ��
     *        �����ϰ�, SQL_ERROR �� �������� �ʴ´�.
     *
     *        �׷���, ��� row �� error �� ��Ȳ�� row �ϳ��� fetch �Ǿ��µ�, �� row �� error ��
     *        ��Ȳ�ۿ� ����.
     *        �� ���, SQLFetchScroll() �� SQL_ERROR �� �����ϰ� �Ǿ� �ֱ� �ѵ�,...
     *
     * BUGBUG : �̷��� ��츦 ó���ϵ��� �ؾ� �Ѵ�.
     */

    /*
     * Protocol Context ����
     */
    ULN_FLAG_DOWN(sNeedFinPtContext);
    //fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext,
                &(aStmt->mParentDbc->mPtContext)) != ACI_SUCCESS);

    /*
     * ===========================================
     * FINALIZE
     * ===========================================
     */

    /*
     * �ٲ�ġ�� �� �ξ��� SQL_ATTR_ROW_STATUS_PTR �� ���󺹱��Ѵ�.
     */
    ulnStmtSetAttrRowStatusPtr(aStmt, sOriginalRowStatusArray);

    /*
     * Exit
     */
    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_HIGH, NULL, 0,
            "%-18s| [orient: %"ACI_UINT32_FMT"] %"ACI_INT32_FMT,
            "ulnExtendedFetch", aOrientation, sFnContext.mSqlReturn);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedRecoverRowStatusPtr)
    {
        /*
         * �ٲ�ġ�� �� �ξ��� SQL_ATTR_ROW_STATUS_PTR �� ���󺹱��Ѵ�.
         */
        ulnStmtSetAttrRowStatusPtr(aStmt, sOriginalRowStatusArray);
    }

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        //fix BUG-17722
        ulnFinalizeProtocolContext(&sFnContext, &(aStmt->mParentDbc->mPtContext));
    }

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [orient: %"ACI_UINT32_FMT"] fail",
            "ulnExtendedFetch", aOrientation);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

