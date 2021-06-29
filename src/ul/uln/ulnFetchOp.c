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
#include <ulnCache.h>
#include <ulnFetchOp.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnFetchOpAsync.h>
#include <ulnSemiAsyncPrefetch.h>

/*
 * FETCH �� �ϱ� ���� �Ϸ��� �������� �Լ���
 *      ulnFetchRequestFetch
 *      ulnFetchReceiveFetchResult
 *      ulnFetchUpdateAfterFetch
 *
 * Note : �� �Լ����� �׻� ��Ʈ�� �ҷ��� �ϱ� ������ Ȥ�ö� �Ǽ��� ���� �� �ִ�.
 *        �׷��� �ٸ� ������ ȣ������ ���ϵ��� static ���� ��Ҵ�.
 *        �ٸ� ������ ȣ���Ϸ��� �ݵ�� ulnFetchOperationSet() �Լ��� �̿��ؾ� �Ѵ�.
 *
 * Note : ����
 *        SELECT ���� ����� ���ؼ� execute �ÿ� fetch request ���� �����Ѵ�.
 *        �̶�����
 *
 *              ulnFetchRequestFetch()
 *
 *        �� ���� ȣ���Ѵ�.
 */

ACI_RC ulnFetchRequestFetch(ulnFnContext *aFnContext,
                            ulnPtContext *aPtContext,
                            acp_uint32_t  aNumberOfRowsToFetch,
                            acp_uint16_t  aColumnNumberToStartFrom,
                            acp_uint16_t  aColumnNumberToFetchUntil)
{
    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    ulnFetchInitForFetchResult(aFnContext);

    /* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
    ACI_TEST(ulnWriteFetchREQ(aFnContext,
                              aPtContext,
                              aNumberOfRowsToFetch,
                              aColumnNumberToStartFrom,
                              aColumnNumberToFetchUntil) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
void ulnFetchInitForFetchResult(ulnFnContext *aFnContext)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    ulnStmtSetFetchedRowCountFromServer(sStmt, 0);
    ulnStmtSetFetchedBytesFromServer(sStmt, 0);
    //fix BUG-17820
    sStmt->mFetchedColCnt = 0;
}

static ACI_RC ulnFetchReceiveFetchResult(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc  *sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    if (cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST( ulnReadProtocolIPCDA(aFnContext,
                                       aPtContext,
                                       sDbc->mConnTimeoutValue) != ACI_SUCCESS );
    }
    else
    {
        ACI_TEST(ulnReadProtocol(aFnContext,
                                 aPtContext,
                                 sDbc->mConnTimeoutValue) != ACI_SUCCESS);
    }

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    ACI_TEST(ulnFetchUpdateAfterFetch(aFnContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnFetchUpdateAfterFetch(ulnFnContext *aFnContext)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    /*
     * Note : IMPORTANT
     *        ���� ����� ���� prepare exec fetch �� �ѹ��� �����Ѵ�.
     *        ����, �Ʒ��� ��Ȳ������ �� �Լ��� ������ �� �ִ�.
     *
     *          1. cache �� ��������� ���� ��Ȳ
     *          2. cursor �� open ���� ���� ��Ȳ
     */

    if (ulnCursorGetState(&sStmt->mCursor) == ULN_CURSOR_STATE_OPEN)
    {
        /*
         * Cursor �� ��ġ�� ���� �ѹ� ������ �����ν�, ���� ��ġ��
         * ULN_CURSOR_POS_AFTER_END ������ üũ�ؼ� cursor->mPosition �� �����Ѵ�.
         */

        ulnCursorSetPosition(&sStmt->mCursor,
                             ulnCursorGetPosition(&sStmt->mCursor));
    }

    if (ulnStmtGetFetchedRowCountFromServer(sStmt) == 0)
    {
        if (ulnStmtGetAttrCursorType(sStmt) == SQL_CURSOR_KEYSET_DRIVEN)
        {
            ulnKeysetSetFullFilled(ulnStmtGetKeyset(sStmt), ACP_TRUE);
        }
        else
        {
            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_NO_DATA);
        }
    }

    return ACI_SUCCESS;
}

static ACI_RC ulnFetchOperationSet(ulnFnContext *aFnContext,
                                   ulnPtContext *aPtContext,
                                   acp_uint32_t  aNumberOfRecordsToFetch,
                                   acp_uint16_t  aColumnNumberToStartFrom,
                                   acp_uint16_t  aColumnNumberToFetchUntil)
{
    ACI_TEST(ulnFetchRequestFetch(aFnContext,
                                  aPtContext,
                                  aNumberOfRecordsToFetch,
                                  aColumnNumberToStartFrom,
                                  aColumnNumberToFetchUntil) != ACI_SUCCESS);

    ACI_TEST(ulnFetchReceiveFetchResult(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ===================================================================
 *
 * �ܺη� export �Ǵ� ������ �Լ�
 *
 *      > �������� fetch �� ���� �Լ� �ΰ���
 *        ulnFetchMoreFromServer()
 *        ulnFetchAllFromServer()
 *
 *      > ĳ���κ��� fetch �� ���� �Լ�
 *        ulnFetchFromCache()
 *
 * ===================================================================
 */

ACI_RC ulnFetchMoreFromServer(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              acp_uint32_t  aNumberOfRowsToGet,
                              acp_uint32_t  aNumberOfPrefetchRows)
{
    acp_uint32_t  sTotalNumberOfRows = 0;
    acp_uint32_t  sNumberOfPrefetchRows;
    ulnStmt      *sStmt;
    ulnCache     *sCache;
    SQLRETURN     sOriginalRC;

    sStmt   = aFnContext->mHandle.mStmt;
    sCache  = ulnStmtGetCache(sStmt);

    /*
     * Note : sNumberOfPrefetchRows�� ��Ż� �ִ� ���� ROW�� ����(ACP_SINT32_MAX)����
     *        Ŭ�� �ֱ� ������, �ݺ��ؼ� ����Ǿ�� �Ѵ�.
     */
    sNumberOfPrefetchRows = aNumberOfPrefetchRows;

    if (sNumberOfPrefetchRows > ACP_SINT32_MAX)
    {
        sNumberOfPrefetchRows = ACP_SINT32_MAX;
    }

    while (sTotalNumberOfRows < aNumberOfRowsToGet)
    {
        sOriginalRC = ULN_FNCONTEXT_GET_RC(aFnContext);

        /* BUG-27119
         * ���� Cache �� �������� ����� SQL_SUCCESS�� �ʱ�ȭ �Ѵ�.
         * �׷��� ���� ��쿡�� ������ ���õ� aFnContext->mSqlReturn ���� ��� ����ϰ� �ȴ�.
         */
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_SUCCESS);

        ACI_TEST(ulnFetchOperationSet(aFnContext,
                                      aPtContext,
                                      sNumberOfPrefetchRows,
                                      1,
                                      ulnStmtGetColumnCount(sStmt))
                 != ACI_SUCCESS);

        sTotalNumberOfRows += ulnStmtGetFetchedRowCountFromServer(sStmt);

        if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
        {
            /* �ϳ��� Fetch�� �Դٸ� SQL_NO_DATA�� �ƴϴ�. */
            if (sTotalNumberOfRows > 0)
            {
                ULN_FNCONTEXT_SET_RC(aFnContext, sOriginalRC);
            }
            break;
        }
        else if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS)
        {
            /* BUG-27119 
             * �� �Լ��� ���� aFnContext�� �������� ���� ���� ����̴�.
             * �̸� Cache�Ǿ��ִ� �����͸� Fetch�߾��� �� �ֱ� ������
             * Original ������ �����ؾ� �Ѵ�.
             */
            ULN_FNCONTEXT_SET_RC(aFnContext, sOriginalRC);
        }

        if (sCache->mServerError != NULL)
        {
            break;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnFetchAllFromServer(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    acp_uint32_t sTotalNumberOfRows = 0;
    acp_uint16_t sNumberOfRowsToGet = ACP_UINT16_MAX;

    while (sTotalNumberOfRows < ACP_SINT32_MAX) // ������ġ
    {
        ACI_TEST(ulnFetchMoreFromServer(aFnContext,
                                        aPtContext,
                                        sNumberOfRowsToGet,
                                        sNumberOfRowsToGet)
                 != ACI_SUCCESS);

        /* BUG-37642 Improve performance to fetch. */
        sTotalNumberOfRows +=
            ulnStmtGetFetchedRowCountFromServer(aFnContext->mHandle.mStmt);

        if(ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA) // ������ �� ������.
        {
            break;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
void ulnFetchCalcPrefetchRows(ulnCache     *aCache,
                              ulnCursor    *aCursor,
                              acp_uint32_t *aNumberOfPrefetchRows,
                              acp_sint32_t *aNumberOfRowsToGet)
{
    acp_uint32_t sBlockSizeOfOneFetch;

    ACE_DASSERT(aCache != NULL);
    ACE_DASSERT(aCursor != NULL);
    ACE_DASSERT(aNumberOfPrefetchRows != NULL);
    ACE_DASSERT(aNumberOfRowsToGet != NULL);

    sBlockSizeOfOneFetch   = ulnCacheCalcBlockSizeOfOneFetch(aCache, aCursor);
    *aNumberOfPrefetchRows = ulnCacheCalcPrefetchRowSize(aCache, aCursor);

    *aNumberOfRowsToGet    = ulnCacheCalcNumberOfRowsToGet(aCache,
                                                           aCursor,
                                                           sBlockSizeOfOneFetch);

    /*
     * To fix BUG-20372
     * ����ڰ� PREFETCH ATTR�� �����ߴٸ�, ������ ������ PREFETCH
     * �ؾ� �ϰ�, �׷��� ���� ���� OPTIMAL ������ FETCH �ؾ� �Ѵ�.
     */
    if (*aNumberOfPrefetchRows != ULN_CACHE_OPTIMAL_PREFETCH_ROWS &&
        *aNumberOfRowsToGet > 0)
    {
        *aNumberOfPrefetchRows = *aNumberOfRowsToGet;
    }
    else
    {
        /* nothing to do */
    }
}

ACI_RC ulnFetchFromCache(ulnFnContext *aFnContext,
                         ulnPtContext *aPtContext,
                         ulnStmt      *aStmt,
                         acp_uint32_t *aFetchedRowCount)
{
    acp_uint32_t  i;
    acp_sint32_t  sNumberOfRowsToGet = 0;
    acp_uint32_t  sNumberOfPrefetchRows = 0;

    acp_sint64_t  sNextFetchStart;
    acp_uint32_t  sFetchedRowCount = 0;

    ulnCursor    *sCursor     = ulnStmtGetCursor(aStmt);
    ulnCache     *sCache      = ulnStmtGetCache(aStmt);
    acp_uint32_t  sCursorSize = ulnStmtGetAttrRowArraySize(sCursor->mParentStmt);
    ulnRow       *sRow;

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    acp_bool_t    sIsSemiAsyncPrefetch = ulnDbcIsAsyncPrefetchStmt(aStmt->mParentDbc, aStmt);
    acp_bool_t    sIsAutoTuning        = ulnCanSemiAsyncPrefetchAutoTuning(aFnContext);

    sNextFetchStart = ulnCursorGetPosition(sCursor);

    for (i = 0; i < sCursorSize; i++ )
    {
        if( (sNextFetchStart < 0) ||  // AFTER_END or BEFORE_START
            ((sNextFetchStart + i) > ulnCacheGetResultSetSize(sCache)) )
        {
            /*
             * Cursor �� ��ġ�� AFTER END �̰ų� BEFORE START �� ���
             * Cursor + i �� ��ġ�� result set size ���� Ŭ ��� ������ ����Ѵ�.
             */

            if (sCache->mServerError != NULL)
            {
                /*
                 * FetchResult ���Ž� ����� ������ �ִٸ� ������ stmt �� ������ �ܴ�.
                 * fetch �� Error �� �߻��ϸ� ���������� �� �̻� result �� ������ �ʰ�,
                 * fetch end �� �����Ƿ� result set size �� ��������.
                 *
                 * ��, error �� ���� �ٷ� �� row �� �������� �Ǵ� ���̴�.
                 */

                sCache->mServerError->mRowNumber = i + 1;

                ulnDiagHeaderAddDiagRec(&aStmt->mObj.mDiagHeader, sCache->mServerError);
                ULN_FNCONTEXT_SET_RC(aFnContext,
                                     ulnErrorDecideSqlReturnCode(sCache->mServerError->mSQLSTATE));

                sCache->mServerError = NULL;
                ACI_TEST(!(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext))));
            }

            break;
        }

        sRow = ulnCacheGetCachedRow(sCache, sNextFetchStart + i);

        if(sRow == NULL)
        {
            /*
             * ĳ�� MISS
             */

            if(ulnCursorGetType(sCursor) == SQL_CURSOR_FORWARD_ONLY)
            {
                /*
                 * scrollable cursor �� �ƴϸ�,
                 *      1. ĳ�� ��ġ�� 1 �� �ٽ� �ʱ�ȭ��Ų��.
                 *      2. lob �� ĳ���Ǿ� ������ close �Ѵ�.
                 *      3. ĳ���� mRowCount �� �ʱ�ȭ��Ų��.
                 */

                // To Fix BUG-20481
                // FORWARD_ONLY�̰� ����� Buffer�� ����Ǿ����� ����ǹǷ�
                // Cache ������ ������ ������ �� �ִ�.

                // To Fix BUG-20409
                // Cursor�� Logical Position�� �����Ű�� �ʴ´�.
                // ulnCursorSetPosition(sCursor, 1);

                ACI_TEST(ulnCacheCloseLobInCurrentContents(aFnContext,
                                                           aPtContext,
                                                           sCache)
                         != ACI_SUCCESS);

                /* ���ŵ� Cache Row�� ������ŭ StartPosition ���� */
                ulnCacheAdjustStartPosition( sCache );

                ulnCacheInitRowCount(sCache);

                // BUG-21746 �޸𸮸� �����ϰ� �Ѵ�.
                ACI_TEST(ulnCacheBackChunkToMark( sCache ) != ACI_SUCCESS);
                // ������ ����ϰ� �ִ� RPA�� �籸���Ѵ�.
                ACI_TEST(ulnCacheReBuildRPA( sCache ) != ACI_SUCCESS);

                /* PROJ-2047 Strengthening LOB - LOBCACHE */
                ACI_TEST(ulnLobCacheReInitialize(aStmt->mLobCache)
                         != ACI_SUCCESS);
            }

            /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
            ulnFetchCalcPrefetchRows(sCache,
                                     sCursor,
                                     &sNumberOfPrefetchRows,
                                     &sNumberOfRowsToGet);

            ACI_TEST_RAISE(sNumberOfRowsToGet < 0, LABEL_MEM_MAN_ERR);

            if (sIsSemiAsyncPrefetch == ACP_FALSE)
            {
                ACI_TEST(ulnFetchMoreFromServer(aFnContext,
                                                aPtContext,
                                                (acp_uint32_t)sNumberOfRowsToGet,
                                                sNumberOfPrefetchRows)
                         != ACI_SUCCESS);
            }
            else
            {
                if (sIsAutoTuning == ACP_FALSE)
                {
                    ACI_TEST(ulnFetchMoreFromServerAsync(
                                aFnContext,
                                aPtContext,
                                (acp_uint32_t)sNumberOfRowsToGet,
                                sNumberOfPrefetchRows) != ACI_SUCCESS);
                }
                else
                {
                    ACI_TEST(ulnFetchMoreFromServerAsyncWithAutoTuning(
                                aFnContext,
                                aPtContext,
                                (acp_uint32_t)sNumberOfRowsToGet,
                                sNumberOfPrefetchRows) != ACI_SUCCESS);
                }
            }

            /*
             * RETRY
             */
            sRow = ulnCacheGetCachedRow(aStmt->mCache, sNextFetchStart + i);
        }

        /*
         * �������� ����� �� �����Դµ��� �ұ��ϰ�, ĳ�� MISS
         */
        if(sRow == NULL)
        {
            /*
             * ���������� �� �̻� ������ ���� ���� ��� �ǰڴ�.
             * ulnFetchUpdateAfterFetch() �Լ����� result cache ���ٰ� ResultsetSize �����ߴ�.
             *
             * �̰��� ���� ���� cursor size �� result set size �� ����� �̷���� ��쿡
             * �̹� result set size ��ŭ fetch �� �� �� �� �� (���� SQL_NO_DATA �� ���� �ȵ�)
             * �ѹ� �� fetch �� �ϰ� �Ǹ� ���� �̰��� �ɸ��� �ȴ�.
             *
             * SQL_NO_DATA �� ������ �ֱ� ���ؼ� ������ �ؾ� ������,
             * �̹� ulnFetchUpdateAfterFetch() �Լ����� �����ع��� ����̴�.
             *
             * ���� cursor size �� result set �� ������ ��谡 ���� ���ļ�, ���� ���, Ŀ�� ������
             * 10 �ε�, 6 record �� ��ġ�Ǿ� �Դٸ�, �̹� ulnFetchUpdateAfterFetch() ����
             * ulnCache �� mResultsetSize �� ������ ��������, �� ������ ��������
             * break �Ǿ� Ƣ�� ������ �ȴ�.
             */
        }
        else
        {
            /*
             * ĳ�� HIT
             */
            ACI_TEST(ulnCacheRowCopyToUserBuffer(aFnContext,
                                                 aPtContext,
                                                 sRow,
                                                 sCache,
                                                 aStmt,
                                                 i + 1)
                     != ACI_SUCCESS);

            aStmt->mGDTargetPosition = sNextFetchStart + i;

            sFetchedRowCount++;
        }
    } /* for (cursor size) */

    *aFetchedRowCount = sFetchedRowCount;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "Number of rows to get is subzero.");
    }

    ACI_EXCEPTION_END;

    *aFetchedRowCount = sFetchedRowCount;

    return ACI_FAILURE;
}

/* PROJ-1789 Updatable Scrollable Cursor */

/**
 * ������ Position���� ä�������� �������� Key�� �� ������ Keyset�� ä���.
 *
 * @param[in] aFnContext function context
 * @param[in] aPtContext protocol context
 * @param[in] aTargetPos target position
 *
 * @return ACP_SUCCESS if successful, or ACP_FAILURE otherwise
 */
ACI_RC ulnFetchMoreFromServerForKeyset(ulnFnContext *aFnContext,
                                       ulnPtContext *aPtContext)
{
    ulnStmt      *sStmt;
    ulnKeyset    *sKeyset;
    SQLRETURN     sOriginalRC;

    sStmt   = aFnContext->mHandle.mStmt;
    sKeyset = ulnStmtGetKeyset(sStmt);

    ACE_DASSERT(ulnKeysetIsFullFilled(sKeyset) != ACP_TRUE);

    sOriginalRC = ULN_FNCONTEXT_GET_RC(aFnContext);
    ULN_FNCONTEXT_SET_RC(aFnContext, SQL_SUCCESS);

    ACI_TEST( ulnFetchOperationSet(aFnContext,
                                   aPtContext,
                                   0, /* ��Ź��ۿ� �ִ��� ä���� ��´�. */
                                   1,
                                   ulnStmtGetColumnCount(sStmt))
              != ACI_SUCCESS );

    if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
    {
        ulnKeysetSetFullFilled(sKeyset, ACP_TRUE);
    }
    else if (ULN_FNCONTEXT_GET_RC(aFnContext) != SQL_SUCCESS)
    {
        /* Error occured. RC ������ ó���ϹǷ� �Լ��� ACI_SUCCESS ��ȯ. */
    }
    else
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, sOriginalRC);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * Keyset-driven�� ���� Prefetch�� �����Ѵ�.
 *
 * @param[in] aFnContext     function context
 * @param[in] aPtContext     protocol context
 * @param[in] aStartPosition Fetch�� ������ Position (inclusive)
 * @param[in] aFetchCount    Fetch �� Row ����
 * @param[in] aCacheMode     Cache�� ��� ��������
 *
 * @return ACP_SUCCESS if successful, or ACP_FAILURE otherwise
 */
ACI_RC ulnFetchFromServerForSensitive(ulnFnContext     *aFnContext,
                                      ulnPtContext     *aPtContext,
                                      acp_sint64_t      aStartPosition,
                                      acp_uint32_t      aFetchCount,
                                      ulnStmtFetchMode  aFetchMode)
{
    ulnFnContext  sTmpFnContext;
    ulnStmt      *sKeysetStmt;
    ulnStmt      *sRowsetStmt;
    ulnKeyset    *sKeyset;
    acp_uint8_t  *sKeyValue;
    acp_uint32_t  i;
    acp_uint32_t  sActFetchCount = aFetchCount;

    ulnDescRec   *sDescRecArd;
    acp_sint64_t  sBookmark;

    sKeysetStmt  = aFnContext->mHandle.mStmt;
    sKeyset      = ulnStmtGetKeyset(sKeysetStmt);
    sRowsetStmt  = sKeysetStmt->mRowsetStmt;

    ULN_INIT_FUNCTION_CONTEXT(sTmpFnContext, ULN_FID_EXECDIRECT, sRowsetStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnFreeStmtClose(&sTmpFnContext, sRowsetStmt) != ACI_SUCCESS);

    /* Bind */

    if (aFetchMode == ULN_STMT_FETCHMODE_BOOKMARK)
    {
        ACE_DASSERT(aStartPosition == 1); /* ���ο����� ���Ƿ� �׻� 1 */

        sDescRecArd = ulnStmtGetArdRec(sKeysetStmt, 0);
        ACE_ASSERT(sDescRecArd != NULL); /* ������ Ȯ���ϰ� ���´�. */

        for (i = 0; i < sActFetchCount; i++)
        {
            if (ulnStmtGetAttrUseBookMarks(sKeysetStmt) == SQL_UB_VARIABLE)
            {
                sBookmark = *((acp_sint64_t *) ulnBindCalcUserDataAddr(sDescRecArd, i));
            }
            else
            {
                sBookmark = *((acp_sint32_t *) ulnBindCalcUserDataAddr(sDescRecArd, i));
            }
            sKeyValue = ulnKeysetGetKeyValue(sKeyset, sBookmark);
            ACI_TEST_RAISE(sKeyValue == NULL, INVALID_BOOKMARK_VALUE_EXCEPTION);
            ACI_TEST(ulnBindParamBody(&sTmpFnContext,
                                      i + 1,
                                      NULL,
                                      SQL_PARAM_INPUT,
                                      SQL_C_SBIGINT,
                                      SQL_BIGINT,
                                      0,
                                      0,
                                      sKeyValue,
                                      ULN_KEYSET_MAX_KEY_SIZE,
                                      NULL) != ACI_SUCCESS);
        }
    }
    else /* is ULN_STMT_FETCHMODE_ROWSET */
    {
        if ((aStartPosition + sActFetchCount - 1) > ulnKeysetGetKeyCount(sKeyset))
        {
            sActFetchCount = ulnKeysetGetKeyCount(sKeyset) - aStartPosition + 1;
        }

        for (i = 0; i < sActFetchCount; i++)
        {
            sKeyValue = ulnKeysetGetKeyValue(sKeyset, aStartPosition + i);
            ACE_ASSERT(sKeyValue != NULL);
            ACI_TEST(ulnBindParamBody(&sTmpFnContext,
                                      i + 1,
                                      NULL,
                                      SQL_PARAM_INPUT,
                                      SQL_C_SBIGINT,
                                      SQL_BIGINT,
                                      0,
                                      0,
                                      sKeyValue,
                                      ULN_KEYSET_MAX_KEY_SIZE,
                                      NULL) != ACI_SUCCESS);
        }
    }

    /* ExecDirect */

    ACI_TEST(ulnPrepBuildSelectForRowset(aFnContext, sActFetchCount)
             != ACI_SUCCESS);

    ACI_TEST(ulnPrepareCore(&sTmpFnContext, aPtContext,
                            sKeysetStmt->mQstrForRowset,
                            sKeysetStmt->mQstrForRowsetLen,
                            CMP_DB_PREPARE_MODE_EXEC_DIRECT) != ACI_SUCCESS);

    ACI_TEST(ulnExecuteCore(&sTmpFnContext, aPtContext) != ACI_SUCCESS);

    ACI_TEST(ulnFlushAndReadProtocol(&sTmpFnContext, aPtContext,
                                     sKeysetStmt->mParentDbc->mConnTimeoutValue)
             != ACI_SUCCESS);

    /* Fetch */

    ulnStmtSetFetchMode(sRowsetStmt, aFetchMode);

    /* ������� �״°� �ƴϱ� ������ Cache �������� �˾ƾ� �Ѵ�. */
    sRowsetStmt->mRowsetCacheStartPosition = aStartPosition;

    ACI_TEST(ulnFetchAllFromServer(&sTmpFnContext, aPtContext) != ACI_SUCCESS);

    if ((ULN_FNCONTEXT_GET_RC(&sTmpFnContext) != SQL_SUCCESS)
     && (ULN_FNCONTEXT_GET_RC(&sTmpFnContext) != SQL_NO_DATA))
    {
        ulnDiagRecMoveAll( &(sKeysetStmt->mObj), &(sRowsetStmt->mObj) );
        ULN_FNCONTEXT_SET_RC(aFnContext, ULN_FNCONTEXT_GET_RC(&sTmpFnContext));
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(INVALID_BOOKMARK_VALUE_EXCEPTION)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_BOOKMARK_VALUE);
    }
    ACI_EXCEPTION_END;

    if ((ULN_FNCONTEXT_GET_RC(&sTmpFnContext) != SQL_SUCCESS)
     && (ULN_FNCONTEXT_GET_RC(&sTmpFnContext) != SQL_NO_DATA))
    {
        ulnDiagRecMoveAll( &(sKeysetStmt->mObj), &(sRowsetStmt->mObj) );
        ULN_FNCONTEXT_SET_RC(aFnContext, ULN_FNCONTEXT_GET_RC(&sTmpFnContext));

        ulnCacheInvalidate(sRowsetStmt->mCache);
    }

    return ACI_FAILURE;
}

ACI_RC ulnFetchFromCacheForKeyset(ulnFnContext *aFnContext,
                                  ulnPtContext *aPtContext,
                                  ulnStmt      *aStmt,
                                  acp_sint64_t  aTargetPos)
{
    ulnRow       *sRow;
    acp_uint8_t   sKeyValueBuf[ULN_KEYSET_MAX_KEY_SIZE] = {0, };
    acp_uint8_t  *sKeyValuePtr = sKeyValueBuf;
    acp_uint32_t  i;
    acp_uint32_t  sFetchedKeyCount = 0;

    ulnKeyset    *sKeyset = ulnStmtGetKeyset(aStmt);
    ulnCache     *sCache  = ulnStmtGetCache(aStmt);
    acp_sint64_t  sNextKeysetStart = ulnKeysetGetKeyCount(sKeyset) + 1;

    ACI_TEST_RAISE((aTargetPos != ULN_KEYSET_FILL_ALL) &&
                   (aTargetPos < sNextKeysetStart),
                   LABEL_ALREADY_EXISTS_CONT);

    for (i = 0; (sNextKeysetStart + i) <= aTargetPos; i++)
    {
        if ( (sNextKeysetStart + i) > ulnCacheGetResultSetSize(sCache) )
        {
            if (sCache->mServerError != NULL)
            {
                sCache->mServerError->mRowNumber = i + 1;

                ulnDiagHeaderAddDiagRec(&aStmt->mObj.mDiagHeader, sCache->mServerError);
                ULN_FNCONTEXT_SET_RC(aFnContext,
                                     ulnErrorDecideSqlReturnCode(sCache->mServerError->mSQLSTATE));

                sCache->mServerError = NULL;
                ACI_TEST(!(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext))));
            }

            break;
        }

        sRow = ulnCacheGetCachedRow(sCache, sNextKeysetStart + i);
        if (sRow == NULL)
        {
            ACI_TEST(ulnFetchMoreFromServerForKeyset(aFnContext, aPtContext)
                     != ACI_SUCCESS);
            sRow = ulnCacheGetCachedRow(sCache, sNextKeysetStart + i);
        }
        ACE_DASSERT(sRow != NULL);

        // only PROWID
        CM_ENDIAN_ASSIGN8(sKeyValuePtr, sRow->mRow);

        ACI_TEST( ulnKeysetAddNewKey(sKeyset, sKeyValuePtr)
                  != ACI_SUCCESS );

        sFetchedKeyCount++;
    }

    if (sFetchedKeyCount == 0)
    {
        ulnKeysetSetFullFilled(sKeyset, ACP_TRUE);
    }

    ACI_EXCEPTION_CONT(LABEL_ALREADY_EXISTS_CONT);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * Cache���� ����Ÿ�� Fetch�Ѵ�.
 * Cache�� �����ϸ� Cache�� ä�� �� Fetch�Ѵ�.
 *
 * @param[in] aFnContext       function context
 * @param[in] aPtContext       protocol context
 * @param[in] aStmt            statement handle
 * @param[in] aFetchedRowCount Fetch�� row ����
 *
 * @return ACP_SUCCESS if successful, or ACP_FAILURE otherwise
 */
ACI_RC ulnFetchFromCacheForSensitive(ulnFnContext *aFnContext,
                                     ulnPtContext *aPtContext,
                                     ulnStmt      *aKeysetStmt,
                                     acp_uint32_t *aFetchedRowCount)
{
    ULN_FLAG(sHoleExistFlag);

    acp_uint32_t  i;
    acp_sint64_t  sNextFetchStart;
    acp_sint64_t  sNextFetchEnd;
    acp_sint64_t  sNextPrefetchStart;
    acp_sint64_t  sNextPrefetchEnd;
    acp_uint32_t  sPrefetchSize;
    acp_sint64_t  sRowCount = ULN_ROWCOUNT_UNKNOWN;

    ulnCursor    *sCursor       = ulnStmtGetCursor(aKeysetStmt);
    ulnKeyset    *sKeyset       = ulnStmtGetKeyset(aKeysetStmt);
    ulnStmt      *sRowsetStmt   = aKeysetStmt->mRowsetStmt;
    ulnCache     *sRowsetCache  = ulnStmtGetCache(sRowsetStmt);
    ulnRow       *sRow;
    acp_uint16_t *sRowStatusPtr = ulnStmtGetAttrRowStatusPtr(aKeysetStmt);

    acp_uint32_t  sFetchedRowCount = 0;

    sNextFetchStart = ulnCursorGetPosition(sCursor);
    if (sNextFetchStart < 0) /* AFTER_END or BEFORE_START */
    {
        ACI_RAISE(LABEL_CONT_NO_DATA_FOUND);
    }

    sNextFetchEnd   = sNextFetchStart + ulnCursorGetSize(sCursor) - 1;

    if (ulnKeysetIsFullFilled(sKeyset) == ACP_TRUE)
    {
        sRowCount = ulnKeysetGetKeyCount(sKeyset);
    }

    /* PROJ-1789 Updatable Scrollable Cursor
     * Sensitive�� ���� Cache�� �״� ����� ���� �ٸ���.
     * Hole�� �����ؾ��ϹǷ� Cach MISS�� ���� �� Prefetch�� �ٽ��ϸ� �ȵǰ�,
     * �̸� Cache�� �ʱ�ȭ�� �� Row ��ġ�� �´°��� ���� �־�� �Ѵ�. */
    if ((ulnCacheIsInvalidated(sRowsetCache) == ACP_TRUE)
     || (ulnCacheCheckRowsCached(sRowsetCache, sNextFetchStart, sNextFetchEnd) != ACP_TRUE))
    {
        sPrefetchSize = ulnCacheCalcBlockSizeOfOneFetch(ulnStmtGetCache(aKeysetStmt),
                                                        ulnStmtGetCursor(aKeysetStmt));
        // NextPrefetchRange ���
        if (ulnCursorGetDirection(sCursor) == ULN_CURSOR_DIR_NEXT)
        {
            sNextPrefetchStart = sNextFetchStart;
            sNextPrefetchEnd   = sNextPrefetchStart + sPrefetchSize - 1;
        }
        else /* is ULN_CURSOR_DIR_PREV */
        {
            sNextPrefetchEnd   = sNextFetchEnd;
            sNextPrefetchStart = sNextPrefetchEnd - sPrefetchSize + 1;
        }

        if (sNextPrefetchStart > sRowCount)
        {
            ACI_RAISE(LABEL_CONT_NO_DATA_FOUND);
        }

        if (ulnKeysetIsFullFilled(sKeyset) != ACP_TRUE)
        {
            ACI_TEST(ulnFetchFromCacheForKeyset(aFnContext, aPtContext,
                                                aKeysetStmt, sNextPrefetchEnd)
                     != ACI_SUCCESS);
            sRowCount = ulnKeysetGetKeyCount(sKeyset);
        }

        /* PrefetchSize ���� ���� */
        ulnStmtSetAttrPrefetchRows(sRowsetStmt, ulnStmtGetAttrPrefetchRows(aKeysetStmt));
        ulnStmtSetAttrPrefetchBlocks(sRowsetStmt, ulnStmtGetAttrPrefetchBlocks(aKeysetStmt));
        ulnStmtSetAttrPrefetchMemory(sRowsetStmt, ulnStmtGetAttrPrefetchMemory(aKeysetStmt));

        ACI_TEST(ulnFetchFromServerForSensitive(aFnContext, aPtContext,
                                                sNextPrefetchStart,
                                                sPrefetchSize,
                                                ULN_STMT_FETCHMODE_ROWSET)
                 != ACI_SUCCESS);
    }

    for (i = 0; i < ulnCursorGetSize(sCursor); i++ )
    {
        if( (sNextFetchStart < 0) ||  /* AFTER_END or BEFORE_START */
            ((sNextFetchStart + i) > sRowCount) )
        {
            if (sRowsetCache->mServerError != NULL)
            {
                /*
                 * FetchResult ���Ž� ����� ������ �ִٸ� ������ stmt �� ������ �ܴ�.
                 * fetch �� Error �� �߻��ϸ� ���������� �� �̻� result �� ������ �ʰ�,
                 * fetch end �� �����Ƿ� result set size �� ��������.
                 *
                 * ��, error �� ���� �ٷ� �� row �� �������� �Ǵ� ���̴�.
                 */

                sRowsetCache->mServerError->mRowNumber = i + 1;

                ulnDiagHeaderAddDiagRec(&aKeysetStmt->mObj.mDiagHeader, sRowsetCache->mServerError);
                ULN_FNCONTEXT_SET_RC(aFnContext,
                                     ulnErrorDecideSqlReturnCode(sRowsetCache->mServerError->mSQLSTATE));

                sRowsetCache->mServerError = NULL;
                ACI_TEST(!(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext))));
            }

            break;
        }

        sRow = ulnCacheGetCachedRow(sRowsetCache, sNextFetchStart + i);
        /* PROJ-1789 Updatable Scrollable Cursor */
        /* RPA�� �̸� ����Ƿ� NULL�̸� �ȵɰ� ������, CachedRow�� ���� ��
         * RowNumber�� RowCount���� ũ�� NULL�� �ֵ��� �Ǿ������Ƿ�,
         * Rowset�� ������ Row�� HOLE�̶�� sRow ��ü�� NULL�� �ȴ�. */
        if ((sRow == NULL) || (sRow->mRow == NULL))
        {
            if ((sNextFetchStart + i) <= sRowCount)
            {
                if (sRowStatusPtr != NULL)
                {
                    ulnStmtSetAttrRowStatusValue(aKeysetStmt,
                                                 i, SQL_ROW_DELETED);
                }

                /* row-status indicators�� �������� �ʾҾ,
                 * ��ȿ�� row ����Ÿ�� ����� ���ۿ� �������ٰŴ�.
                 * �׸� ����, ���⼭�� Hole �߻� ���θ� ����صд�. */
                ULN_FLAG_UP(sHoleExistFlag);
            }

            /*  no continue. Hole�� Fetch�� Row�� �ϳ��� ����. */
        }
        else
        {
            ACI_TEST(ulnCacheRowCopyToUserBuffer(aFnContext, aPtContext,
                                                 sRow, sRowsetCache,
                                                 sRowsetStmt, i + 1)
                     != ACI_SUCCESS);

            aKeysetStmt->mGDTargetPosition = sNextFetchStart + i;
        }

        sFetchedRowCount++;
    } /* for (cursor size) */

    ULN_IS_FLAG_UP(sHoleExistFlag)
    {
        if (sRowStatusPtr == NULL)
        {
            ulnError(aFnContext, ulERR_IGNORE_HOLE_DETECTED_BUT_NO_INDICATOR);
        }
#if defined(USE_HOLE_DETECTED_WARNING)
        else
        {
            ulnError(aFnContext, ulERR_IGNORE_HOLE_DETECTED);
        }
#endif
    }

    ACI_EXCEPTION_CONT(LABEL_CONT_NO_DATA_FOUND);

    *aFetchedRowCount = sFetchedRowCount;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    *aFetchedRowCount = sFetchedRowCount;

    return ACI_FAILURE;
}
