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

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnFetchOpAsync.h>

ACI_RC ulnSFID_39_40(ulnFnContext *aFnContext, ulnStmtState aBackPreparedState)
{
    ulnResult *sResult;

    if (aFnContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS ||
            ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS_WITH_INFO)
        {
            /* [s]
             *
             * Note : MoreResult �Լ� ��ü���� next result ��
             *        current result �� ���������Ƿ�
             *        ulnStmtGetNextResult() �� �ƴ�
             *        ulnStmtGetCurrentResult() �� ȣ���ؾ�
             *        next result �� ���´�.
             */
            sResult = ulnStmtGetCurrentResult(aFnContext->mHandle.mStmt);
            ACE_ASSERT(sResult != NULL); /* NULL�̸� NO_DATA �� �ڵ带 Ÿ���� */

            if (ulnStmtHasResultSet(aFnContext->mHandle.mStmt) == ACP_TRUE)
            {
                /* [3] The next result is a result set */
                ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, ULN_S_S5);
            }
            else if (ulnStmtHasRowCount(aFnContext->mHandle.mStmt) == ACP_TRUE)
            {
                /* [2] The next result is a row count */
                ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, ULN_S_S4);
            }
            else
            {
                /* BUGBUG (2014-04-18) #MultiStmt
                 * ����� ������ SQL_NO_DATA�� �������Ƿ� ���⸦ Ÿ�� �ȵȴ�.
                 * multi-stmt������ RowCount�� ResultSet�� ������ ������
                 * MoreResults�� ��ȯ�� ��ȿ�� result�� ���� �ʱ� ����. (MsSql-like)
                 * ����� �׷� ó���� ���� �ϴٸ�, multi-stmt�� ������Ƿ� �׳� ASSERT ó���Ѵ�. */
                ACE_ASSERT(0);
            }
        }
        else
        {
            if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
            {
                /* [nf] */
                if (ulnStmtIsLastResult(aFnContext->mHandle.mStmt) == ACP_TRUE)
                {
                    /* [4] The current result is the last result */
                    if (ulnStmtIsPrepared(aFnContext->mHandle.mStmt) == ACP_TRUE)
                    {
                        /* [p] */
                        ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, aBackPreparedState);
                    }
                    else
                    {
                        /* [np] */
                        ULN_OBJ_SET_STATE(aFnContext->mHandle.mStmt, ULN_S_S1);
                    }
                }
            }
        }
    }

    return ACI_SUCCESS;
}

/*
 * ULN_SFID_39
 * SQLGetStmtAttr(), STMT, S1, S2-S3, S4, S5
 *      -- [s] and [2]
 *      S5 [s] and [3]
 *
 *      S1 [nf], [np], and [4]
 *      S2 [nf], [p], and [4]
 *
 *      S11 [x]
 *  where
 *      [1] The function always returns SQL_NO_DATA in this state.
 *      [2] The next result is a row count.
 *      [3] The next result is a result set.
 *      [4] The current result is the last result.
 */
ACI_RC ulnSFID_39(ulnFnContext *aFnContext)
{
    return ulnSFID_39_40(aFnContext, ULN_S_S2);
}

/*
 * ULN_SFID_40
 * SQLGetStmtAttr(), STMT, S1, S2-S3, S4, S5
 *      S1 [nf], [np], and [4]
 *      S3 [nf], [p] and [4]
 *
 *      S4 [s] and [2]
 *      S5 [s] and [3]
 *
 *      S11 [x]
 *  where
 *      [1] The function always returns SQL_NO_DATA in this state.
 *      [2] The next result is a row count.
 *      [3] The next result is a result set.
 *      [4] The current result is the last result.
 */
ACI_RC ulnSFID_40(ulnFnContext *aFnContext)
{
    return ulnSFID_39_40(aFnContext, ULN_S_S3);
}

ACI_RC ulnMoreResultsCore(ulnFnContext *aFnContext,
                          ulnPtContext *aPtContext)
{
    ulnStmt      *sStmt;
    acp_uint16_t  sResultSetCount;
    acp_uint16_t  sCurrentResultSetID;
    ulnCursor    *sCursor;
    ulnResult    *sResult;
    ulnDesc      *sDescIrd;

    acp_uint32_t  sPrefetchCnt;

    sStmt = aFnContext->mHandle.mStmt;

    /* PROJ-1381, BUG-32902, BUG-33037 FAC
     * FetchEnd �� �������� Close�ϸ� ���� ����� ���� �� �ٽ�
     * OPEN ���·� �ٲٹǷ� ���⼭ FetchEnd�� ����� CLOSED ���·� �ٲ��ش�.
     * ������, Holdable Statement�� CloseCursor ������ ���Ƿ� ������ �����Ƿ�
     * ���¸� �ٲ��� �ʾƾ� �Ѵ�. */
    if (ulnStmtGetAttrCursorHold(sStmt) == SQL_CURSOR_HOLD_OFF)
    {
        sCursor = ulnStmtGetCursor(sStmt);
        ulnCursorSetServerCursorState(sCursor, ULN_CURSOR_STATE_CLOSED);
    }

    sResult = ulnStmtGetNextResult(sStmt);
    if (sResult != NULL)
    {
        /*
         * Array of Parameter�� ���� ����� ����
         */
        ulnStmtSetCurrentResult(sStmt, sResult);
        ulnDiagSetRowCount(&sStmt->mObj.mDiagHeader, sResult->mAffectedRowCount);
    }
    else
    {
        sCurrentResultSetID = ulnStmtGetCurrentResultSetID(sStmt);
        sResultSetCount = ulnStmtGetResultSetCount(sStmt);

        if (sCurrentResultSetID < sResultSetCount - 1)
        {
            /*
             * ���� Result Set�� ����
             */

            // CurrentResultSetID ����
            sCurrentResultSetID++;
            ulnStmtSetCurrentResultSetID(sStmt, sCurrentResultSetID);

            // ResultSet �ʱ�ȭ
            sResult = ulnStmtGetCurrentResult(sStmt);
            ulnResultSetType(sResult, ULN_RESULT_TYPE_RESULTSET);

            // IRD �ʱ�ȭ
            sDescIrd = ulnStmtGetIrd(sStmt);
            ACI_TEST_RAISE(sDescIrd == NULL, LABEL_MEM_MAN_ERR);
            ACI_TEST_RAISE(ulnDescRollBackToInitial(sDescIrd) != ACI_SUCCESS, LABEL_MEM_MAN_ERR);
            ACI_TEST_RAISE(ulnDescInitialize(sDescIrd, (ulnObject *)sStmt) != ACI_SUCCESS,
                           LABEL_NOT_ENOUGH_MEM);

            // ĳ�� �ʱ�ȭ
            ACI_TEST( ulnCacheInitialize(sStmt->mCache) != ACI_SUCCESS );

            if (sStmt->mKeyset != NULL)
            {
                ACI_TEST(ulnKeysetInitialize(sStmt->mKeyset) != ACI_SUCCESS );
            }

            // Ŀ�� �ʱ�ȭ
            ulnCursorSetPosition(&sStmt->mCursor, ULN_CURSOR_POS_BEFORE_START);

            // fix BUG-21737
            sStmt->mGDColumnNumber = ULN_GD_COLUMN_NUMBER_INIT_VALUE;

            // ColumnInfoGet RES
            ACI_TEST(ulnWriteColumnInfoGetREQ(aFnContext, aPtContext, 0) != ACI_SUCCESS);

            // Fetch RES
            // To Fix BUG-20390
            // Prefetch Cnt�� ����Ͽ� ��û��.
            sPrefetchCnt = ulnCacheCalcPrefetchRowSize( sStmt->mCache,
                                                        & sStmt->mCursor );

            ACI_TEST(ulnFetchRequestFetch(aFnContext,
                                          aPtContext,
                                          sPrefetchCnt,
                                          0,
                                          0) != ACI_SUCCESS);

            ACI_TEST(ulnFlushAndReadProtocol(aFnContext,
                                             aPtContext,
                                             sStmt->mParentDbc->mConnTimeoutValue) != ACI_SUCCESS);

            /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
            if (ulnDbcIsAsyncPrefetchStmt(sStmt->mParentDbc, sStmt) == ACP_TRUE)
            {
                ulnFetchNextAsync(aFnContext, aPtContext);
            }
            else
            {
                /* synchronous prefetch */
            }
        }
        else
        {
            /*
             * �� �̻� Result Set�� ����
             */
            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_NO_DATA);
        }
    }

    /* PROJ-1789: �ٸ� ��������� �Ѿ�Ƿ� �ʱ�ȭ���־�� �Ѵ�. */
    ulnStmtResetQstrForDelete(sStmt);
    ulnStmtResetTableNameForUpdate(sStmt);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnMoreResultsCore");
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnMoreResultsCore");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * Multiple Result �� �����ϸ鼭 �����ؾ� �� ����Ʈ��
 *
 *      1. PREPARE RESULT ���Ž�
 *         explicit batch ������ �� ����Ʈ�� ����ؾ� �� �ʿ䰡 ����.
 *
 *      2. SQLExecute() / SQLExecDirect() �Լ� ���Խ�
 *         ulnStmtFreeAllResult(sStmt);
 *         sStmt->mTotalAffectedRowCount = ID_ULONG(0); <-- SQL_NO_DATA ������ ���� �ʿ���.
 *
 *      3. EXECUTE RESULT ���Ž�
 *         Add new result
 *
 *      4. SQLExecute() / SQLExecDirect() �Լ� Ż���
 *         Current result �� ó�� result �� ����.
 *         Diag header �� row count �� ����
 *
 *      5. SQLCloseCursor() ȣ��� (SQLFreeStmt(SQL_CLOSE))
 *         ulnStmtFreeAllResult(sStmt);
 */

SQLRETURN ulnMoreResults(ulnStmt *aStmt)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);

    ulnFnContext  sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_MORERESULTS, aStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /*
     * ===========================================
     * Function BEGIN
     * ===========================================
     */
    //fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(aStmt->mParentDbc->mPtContext),
                                          &aStmt->mParentDbc->mSession) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);
    //fix BUG-17722
    ACI_TEST(ulnMoreResultsCore(&sFnContext,
                               &(aStmt->mParentDbc->mPtContext)) != ACI_SUCCESS);

    ULN_FLAG_DOWN(sNeedFinPtContext);
    //fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext, &(aStmt->mParentDbc->mPtContext)) != ACI_SUCCESS);

    /*
     * ===========================================
     * Function END
     * ===========================================
     */

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        //fix BUG-17722
        ulnFinalizeProtocolContext(&sFnContext,&(aStmt->mParentDbc->mPtContext));
    }

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
