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

/*
 * ===========================================
 * SQLFetchScroll()
 * ===========================================
 */

SQLRETURN ulnFetchScroll(ulnStmt *aStmt, acp_sint16_t aFetchOrientation, ulvSLen aFetchOffset)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);

    ulnFnContext  sFnContext;

    acp_uint32_t  sNumberOfRowsFetched;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_FETCHSCROLL, aStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedExit);

    /*
     * =============================================
     * Function Body BEGIN
     * =============================================
     */

    ACI_TEST(ulnCheckFetchOrientation(&sFnContext, aFetchOrientation) != ACI_SUCCESS);

    //fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(aStmt->mParentDbc->mPtContext),
                                          &(aStmt->mParentDbc->mSession)) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);

    /*
     * ---------------------------------------
     * Protocol context
     */

    // ulnDescSetRowsProcessedPtrValue(aStmt->mAttrIrd, 0);

    //fix BUG-17722
    ACI_TEST(ulnFetchCore(&sFnContext,
                          &(aStmt->mParentDbc->mPtContext),
                          aFetchOrientation,
                          aFetchOffset,
                          &sNumberOfRowsFetched) != ACI_SUCCESS);

    /*
     * ����ڿ��� ��ġ�� Row �� ���� ����.
     */
    ulnDescSetRowsProcessedPtrValue(aStmt->mAttrIrd, sNumberOfRowsFetched);

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
     * Protocol context
     * ---------------------------------------
     */

    ULN_FLAG_DOWN(sNeedFinPtContext);
    //fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext,
                      &(aStmt->mParentDbc->mPtContext)) != ACI_SUCCESS);

    /*
     * =============================================
     * Function Body END
     * =============================================
     */

    ULN_FLAG_DOWN(sNeedExit);

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_HIGH, NULL, 0,
            "%-18s| [orient: %"ACI_INT32_FMT"] %"ACI_INT32_FMT,
            "ulnFetchScroll", aFetchOrientation, sFnContext.mSqlReturn);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION_END;

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
            "%-18s| [orient: %"ACI_INT32_FMT"] fail",
            "ulnFetchScroll", aFetchOrientation);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
