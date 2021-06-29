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
 * ULN_SFID_24
 * SQLFetch(), SQLFetchScroll(), STMT, S5
 *      S6 [s] or [nf]
 *      S11 [x]
 *  where
 *      [s]  Success. The function returned SQL_SUCCESS_WITH_INFO or SQL_SUCCESS.
 *      [nf] Not found. The function returned SQL_NO_DATA.
 *           This does not apply when SQLExecDirect, SQLExecute, or SQLParamData returns
 *           SQL_NO_DATA after executing a searched update or delete statement.
 */
ACI_RC ulnSFID_24(ulnFnContext *aFnContext)
{
    if(aFnContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        if(ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS_WITH_INFO ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
        {
            /* [s] or [nf] */
            ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, ULN_S_S6);
        }
    }

    return ACI_SUCCESS;
}

/*
 * ULN_SFID_25
 * SQLFetch(), SQLFetchScroll(), STMT ���������Լ� : S6 ����
 *      -- [s] or [nf]
 *      S11 [x]
 *  where
 *      [s]  Success. The function returned SQL_SUCCESS_WITH_INFO or SQL_SUCCESS.
 *      [nf] Not found. The function returned SQL_NO_DATA.
 *           This does not apply when SQLExecDirect, SQLExecute, or SQLParamData returns
 *           SQL_NO_DATA after executing a searched update or delete statement.
 */
ACI_RC ulnSFID_25(ulnFnContext *aFnContext)
{
    if(aFnContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        if(ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS_WITH_INFO ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
        {
            /* [s] or [nf] */
        }
    }

    return ACI_SUCCESS;
}

/*
 * ===========================================
 * SQLFetch()
 * ===========================================
 */

SQLRETURN ulnFetch(ulnStmt *aStmt)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);

    ulnFnContext  sFnContext;

    acp_uint32_t  sNumberOfRowsFetched;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_FETCH, aStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedExit);

    /*
     * =============================================
     * Function body BEGIN
     * =============================================
     */
    //fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(aStmt->mParentDbc->mPtContext),
                                          &aStmt->mParentDbc->mSession) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);

    if (aStmt->mIsSimpleQuerySelectExecuted == ACP_TRUE)
    {
        ACI_TEST(ulnFetchCoreForIPCDASimpleQuery(&sFnContext,
                                                 aStmt,
                                                 &sNumberOfRowsFetched)
                 != ACI_SUCCESS);
        if (sNumberOfRowsFetched == 0)
        {
            /*
             * BUGBUG : ulnFetchFromCache() �Լ� �ּ��� ���� �Ʒ��� ���� ���� �ִ� :
             *
             * SQL_NO_DATA �� ������ �ֱ� ���ؼ� ������ �ؾ� ������,
             * �̹� ulnFetchUpdateAfterFetch() �Լ����� �����ع��� ����̴�.
             */
            ULN_FNCONTEXT_SET_RC(&sFnContext, SQL_NO_DATA);
        }
    }
    else
    {
        /*
         * --------------------------------
         * Protocol context
         */

        ulnDescSetRowsProcessedPtrValue(aStmt->mAttrIrd, 0);

        //fix BUG-17722
        ACI_TEST(ulnFetchCore(&sFnContext,
                              &(aStmt->mParentDbc->mPtContext),
                              SQL_FETCH_NEXT,
                              ULN_vLEN(0),
                              &sNumberOfRowsFetched) != ACI_SUCCESS);
    }

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
    ulnDescInitStatusArrayValues(aStmt->mAttrIrd,
                                 sNumberOfRowsFetched,
                                 ulnStmtGetAttrRowArraySize(aStmt),
                                 SQL_ROW_NOROW);

    /*
     * Protocol context
     * --------------------------------
     */

    ULN_FLAG_DOWN(sNeedFinPtContext);
    //fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext,
                                        &(aStmt->mParentDbc->mPtContext) )
               != ACI_SUCCESS);

    /*
     * =============================================
     * Function body END
     * =============================================
     */

    ULN_FLAG_DOWN(sNeedExit);

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_HIGH, NULL, 0,
            "%-18s| %"ACI_INT32_FMT, "ulnFetch", sFnContext.mSqlReturn);

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
            "%-18s| fail", "ulnFetch");

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

