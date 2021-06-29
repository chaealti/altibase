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
#include <ulnCursor.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnFetchOpAsync.h>

#define ULN_ABS(x) ( (acp_uint32_t) (((x) < 0) ? (-(x)) : (x)) )

void ulnCursorInitialize(ulnCursor *aCursor, ulnStmt *aParentStmt)
{
    aCursor->mParentStmt            = aParentStmt;
    aCursor->mPosition              = ULN_CURSOR_POS_BEFORE_START;
    aCursor->mState                 = ULN_CURSOR_STATE_CLOSED;
    aCursor->mServerCursorState     = ULN_CURSOR_STATE_CLOSED;

    /*
     * BUGBUG : ODBC ���忡�� �����ϴ� ����Ʈ ���� SQL_UNSPECIFIED ������,
     *          ���� ul �� cm �� mm ���� sensitive �� Ŀ���� �������� ���ϱ� ������
     *          INSENSITIVE �� �⺻���� �ߴ�
     *
     *          ����.. INSENSITIVE �� �´���, �ƴϸ� UNSPECIFIED �� �´��� �𸣰ڴ�.
     */
    aCursor->mAttrCursorSensitivity = SQL_INSENSITIVE;

    /*
     * note: M$DN ODBC30 �� ����Ʈ ���� �ȳ��´�. ������� NON_UNIQUE �� ��
     */
    aCursor->mAttrSimulateCursor    = SQL_SC_NON_UNIQUE;

    aCursor->mAttrCursorType        = SQL_CURSOR_FORWARD_ONLY;
    aCursor->mAttrCursorScrollable  = SQL_NONSCROLLABLE;
    aCursor->mAttrConcurrency       = SQL_CONCUR_READ_ONLY;

    /* PROJ-1789 Updatable Scrollable Cursor */
    aCursor->mAttrHoldability       = SQL_CURSOR_HOLD_DEFAULT;
    aCursor->mRowsetPos             = 1;
}

acp_uint32_t ulnCursorGetSize(ulnCursor *aCursor)
{
    return ulnStmtGetAttrRowArraySize(aCursor->mParentStmt);
}

void ulnCursorSetConcurrency(ulnCursor *aCursor, acp_uint32_t aConcurrency)
{
    aCursor->mAttrConcurrency = aConcurrency;
}

acp_uint32_t ulnCursorGetConcurrency(ulnCursor *aCursor)
{
    return aCursor->mAttrConcurrency;
}

void ulnCursorSetSensitivity(ulnCursor *aCursor, acp_uint32_t aSensitivity)
{
    aCursor->mAttrCursorSensitivity = aSensitivity;
}

acp_uint32_t ulnCursorGetSensitivity(ulnCursor *aCursor)
{
    return aCursor->mAttrCursorSensitivity;
}

/*
 * ==========================================
 * Cursor OPEN / CLOSE
 * ==========================================
 */

void ulnCursorOpen(ulnCursor *aCursor)
{
    ulnCursorSetState(aCursor, ULN_CURSOR_STATE_OPEN);
    ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
}

ACI_RC ulnCursorClose(ulnFnContext *aFnContext, ulnCursor *aCursor)
{
    ULN_FLAG(sNeedFinPtContext);

    ulnDbc       *sDbc;
    ulnStmt      *sStmt;
    ulnStmt      *sRowsetStmt;
    ulnCache     *sCache;
    ulnKeyset    *sKeyset;
    acp_uint16_t  sResultSetCount;
    acp_uint16_t  sCurrentResultSetID;
    ulnFnContext  sTmpFnContext;

    sStmt = aFnContext->mHandle.mStmt;

    // To Fix BUG-18358
    // Cursor Close�� �̿� ���õ� Statement �ڷ� ������
    // GD(GetData) Column Number�� �ʱ�ȭ�Ͽ��� �Ѵ�.
    sStmt->mGDColumnNumber = ULN_GD_COLUMN_NUMBER_INIT_VALUE;

    /*
     * uln �� Ŀ���� �ݴ´�.
     */
    ulnCursorSetState(aCursor, ULN_CURSOR_STATE_CLOSED);

    sDbc = sStmt->mParentDbc;
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    sResultSetCount = ulnStmtGetResultSetCount(sStmt);
    sCurrentResultSetID = ulnStmtGetCurrentResultSetID(sStmt);

    /*
     * ������ Ŀ���� ���� ���� �ְ�, ������ ����� ������ ������ ������ close cursor ����
     */
    if (ulnDbcIsConnected(sDbc) == ACP_TRUE)
    {
        // BUG-17514
        // ResultSetID�� 0���� �����ϹǷ�
        // ������ ResultSetID�� ��ü ResultSet�� �������� 1 �۴�.
        if (ulnCursorGetServerCursorState(aCursor) == ULN_CURSOR_STATE_OPEN ||
            sCurrentResultSetID < (sResultSetCount - 1))
        {
            /*
             * ������ close cursor ��� ����
             *
             * Note : ������ Ŀ���� ���� �ִ� �����̰ų� �������� �������� CLOSE CURSOR REQ ��
             *        ������ �����ϴ���, ������ �׳� �����Ѵ�.
             *
             *        ���� �̰��� üũ�ϴ� ������, I/O transaction �� �ѹ��̶� �ٿ�����
             *        �ؼ��̴�.
             */
            if (sStmt->mIsSimpleQuerySelectExecuted != ACP_TRUE)
            {
                /* PROJ-2616 */
                /* IPCDA-SimpleQuery-Execute�� Ŀ���� ������� ����.
                 * ����, IPCDA-SimpleQuery-Execute�� �ƴ� ��쿡�� ȣ�� ��.*/
                ACI_TEST(ulnFreeHandleStmtSendFreeREQ(aFnContext, sDbc, CMP_DB_FREE_CLOSE)
                         != ACI_SUCCESS);
            }

            /* PROJ-1381, BUG-32932 FAC : Close ���� ��� */
            ulnCursorSetServerCursorState(aCursor, ULN_CURSOR_STATE_CLOSED);
        }
    }

    /* PROJ-2616 */
    /* SimpleQuery-Select-Execute �� lob�� �������� ������,
     * cache�� ������� �ʴ´�.
     * SHM�� �����͸� �ٷ� �о ������� ���ۿ� �ִ´�.
     * ����, �� ����� �����ϱ�� �Ѵ�.
     */
    ACI_TEST_RAISE(sStmt->mIsSimpleQuerySelectExecuted == ACP_TRUE,
                   ContCursorClose);

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    if (ulnDbcIsAsyncPrefetchStmt(sDbc, sStmt) == ACP_TRUE)
    {
        ACI_TEST(ulnFetchEndAsync(aFnContext, &sDbc->mPtContext) != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    /*
     * ĳ�� �޸𸮰� ������ ��� ���ʿ� ������� ���·� �ʱ�ȭ���ѹ�����.
     * ��, �޸� ������ ���� �ʴ´�.
     */
    sCache = ulnStmtGetCache(sStmt);

    if (sCache != NULL)
    {
        /*
         * row �� lob �÷��� ���� ��� lob column �� ã�Ƽ� lob ���� close �� �ش�.
         */

        if (ulnCacheHasLob(sCache) == ACP_TRUE)
        {
            //fix BUG-17722
            ACI_TEST(ulnInitializeProtocolContext(aFnContext,
                                                  &(sDbc->mPtContext),
                                                  &(sDbc->mSession)) != ACI_SUCCESS);
            ULN_FLAG_UP(sNeedFinPtContext);
        }

        /*
         * BUGBUG : �߸��ϸ� �װڴ�...
         */
        ACI_TEST(ulnCacheCloseLobInCurrentContents(aFnContext,
                                                   &(sDbc->mPtContext),
                                                   sCache)
                 != ACI_SUCCESS);

        ULN_IS_FLAG_UP(sNeedFinPtContext)
        {
            ULN_FLAG_DOWN(sNeedFinPtContext);
            //fix BUG-17722
            ACI_TEST(ulnFinalizeProtocolContext(aFnContext,&(sDbc->mPtContext) ) != ACI_SUCCESS);
        }

        /*
         * Note : CloseCursor() �� �� �̻� fetch �� ���� �ʰڴٴ� �̾߱��̴�.
         *  fix BUG-18260  row-cache�� �ʱ�ȭ �Ѵ�.
         */
        ACI_TEST( ulnCacheInitialize(sCache) != ACI_SUCCESS );
    }

    ACI_EXCEPTION_CONT(ContCursorClose);

    sKeyset = ulnStmtGetKeyset(sStmt);
    if (sKeyset!= NULL)
    {
        ACI_TEST_RAISE(ulnKeysetInitialize(sKeyset) != ACI_SUCCESS,
                       LABEL_MEM_MAN_ERR);
    }

    /* PROJ-2177: Fetch�� ������ �ʱ�ȭ */
    ulnStmtResetLastFetchFuncID(sStmt);

    /* PROJ-1789 Updatable Scrollable Cursor
     * Rowset Cache�� ���� Stmt Close */
    sRowsetStmt = sStmt->mRowsetStmt;
    if ( (ulnCursorGetType(&sStmt->mCursor) == SQL_CURSOR_KEYSET_DRIVEN) &&  
         (sRowsetStmt != SQL_NULL_HSTMT) &&  
         (sStmt->mIsSimpleQuerySelectExecuted != ACP_TRUE) )
    {
        ULN_INIT_FUNCTION_CONTEXT(sTmpFnContext, ULN_FID_CLOSECURSOR,
                                  sRowsetStmt, ULN_OBJ_TYPE_STMT);

        ACI_TEST_RAISE(ulnCursorClose(&sTmpFnContext,
                                      &(sRowsetStmt->mCursor))
                       != ACI_SUCCESS, LABEL_ERRS_IN_ROWSETSTMT);
    }

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    if (sStmt->mLobCache != NULL)
    {
        ACI_TEST(ulnLobCacheReInitialize(sStmt->mLobCache) != ACI_SUCCESS);
    }
    else
    {
        /* Nothing */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnWriteProtocol : object type is neither DBC nor STMT.");
    }
    ACI_EXCEPTION(LABEL_ERRS_IN_ROWSETSTMT)
    {
        if (ULN_FNCONTEXT_GET_RC(&sTmpFnContext) != SQL_SUCCESS)
        {
            ulnDiagRecMoveAll( &(sStmt->mObj), &(sRowsetStmt->mObj) );
            ULN_FNCONTEXT_SET_RC(aFnContext, ULN_FNCONTEXT_GET_RC(&sTmpFnContext));
        }
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        //fix BUG-17722
        ulnFinalizeProtocolContext(aFnContext, &(sDbc->mPtContext));
    }

    return ACI_FAILURE;
}

void ulnCursorSetPosition(ulnCursor *aCursor, acp_sint64_t aPosition)
{
    acp_sint64_t sResultSetSize;

    switch (aPosition)
    {
        case ULN_CURSOR_POS_BEFORE_START:
        case ULN_CURSOR_POS_AFTER_END:
            aCursor->mPosition = aPosition;
            break;

        default:
            ACE_ASSERT(aPosition > 0);

            sResultSetSize = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);

            if (aCursor->mPosition > sResultSetSize)
            {
                aCursor->mPosition = ULN_CURSOR_POS_AFTER_END;
            }
            else
            {
                aCursor->mPosition = aPosition;
            }
            break;
    }
}

/*
 * ��� row �� ����� ���۷� �����ؾ� �ϴ��� ����ϴ� �Լ�
 */
acp_uint32_t ulnCursorCalcRowCountToCopyToUser(ulnCursor *aCursor)
{
    acp_sint64_t sCursorPosition;
    acp_sint64_t sResultSetSize;
    acp_sint32_t sRowCountToCopyToUser;
    acp_uint32_t sCursorSize;

    sCursorPosition = ulnCursorGetPosition(aCursor);

    ACE_ASSERT(sCursorPosition != 0);

    if (sCursorPosition < 0)
    {
        /*
         * ULN_CURSOR_POS_AFTER_END or ULN_CURSOR_POS_BEFORE_START
         */
        sRowCountToCopyToUser = 0;
    }
    else
    {

        /*
         * BUGBUG : ulnCacheGetResultSetSize() �� �׻� ������ ���� �����ϰ� �����ؾ� �Ѵ�.
         */

        sResultSetSize = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);
        sCursorSize    = ulnCursorGetSize(aCursor);

        /*
         * Note : row number �� 1 ���� �����Ѵٴ� ���� ����Ѵ�.
         *        ���� cursor �� mPosition �� 1 �� ù��° row �� ����Ų��.
         */

        if (sCursorPosition + sCursorSize - 1 > sResultSetSize)
        {
            sRowCountToCopyToUser = sResultSetSize - sCursorPosition + 1;

            /*
             * Ŀ�� position �� 20, ������ ��ġ�� �õ��� ���� result set �� 16 �ۿ� ������.
             * �׷� ��� ���� ���� ����ϸ� ������ ���´�.
             * ����ڿ��� ������ �� row �� ������ 0
             */
            if (sRowCountToCopyToUser < 0)
            {
                sRowCountToCopyToUser = 0;
            }
        }
        else
        {
            sRowCountToCopyToUser = sCursorSize;
        }
    }

    return (acp_uint32_t)sRowCountToCopyToUser;
}

/*
 * Note : �Ʒ��� ulnCursorMoveXXX �Լ����� ��� SQLFetchScroll ��
 *        fetch orientation �� �ɼ� �ϳ����� �ش��ϴ� �Լ����̴�.
 *        rowset ������ Ŀ���� �����̴� �Լ����̴�.
 */

void ulnCursorMoveAbsolute(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset)
{
    acp_uint32_t sRowsetSize;
    acp_sint64_t sResultSetSize;

    sRowsetSize    = ulnCursorGetSize(aCursor);
    sResultSetSize = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);

    if (aOffset < 0)
    {
        if (aOffset * (-1) <= sResultSetSize)
        {
            /*
             * FetchOffset < 0 AND | FetchOffset | <= LastResultRow
             */
            ulnCursorSetPosition(aCursor, sResultSetSize + aOffset + 1);
        }
        else
        {
            if ((acp_uint32_t)(aOffset * (-1)) > sRowsetSize)
            {
                /*
                 * FetchOffset < 0 AND
                 *      | FetchOffset | > LastResultRow AND
                 *      | FetchOffset | > RowsetSize
                 */
                ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
            }
            else
            {
                /*
                 * FetchOffset < 0 AND
                 *      | FetchOffset | > LastResultRow AND
                 *      | FetchOffset | <= RowsetSize
                 */
                ulnCursorSetPosition(aCursor, 1);

                ulnError(aFnContext, ulERR_IGNORE_FETCH_BEFORE_RESULTSET);
            }
        }
    }
    else if (aOffset == 0)
    {
        ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
    }
    else
    {
        if (aOffset <= sResultSetSize)
        {
            /*
             * 1 <= FetchOffset <= LastResultRow
             */
            ulnCursorSetPosition(aCursor, aOffset);
        }
        else
        {
            /*
             * FetchOffset > LastResultRow
             */
            ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
        }
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_NEXT);
}

void ulnCursorMoveRelative(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset)
{
    acp_sint64_t sCurrentPosition;
    acp_uint32_t sRowSetSize;
    acp_sint64_t sResultSetSize;

    sRowSetSize      = ulnCursorGetSize(aCursor);
    sCurrentPosition = ulnCursorGetPosition(aCursor);
    sResultSetSize   = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);

    switch(sCurrentPosition)
    {
        case ULN_CURSOR_POS_BEFORE_START:
            if (aOffset > 0)
            {
                /*
                 * (Before start AND FetchOffset > 0) OR (After end AND FetchOffset < 0)
                 */
                ulnCursorMoveAbsolute(aFnContext, aCursor, aOffset);
            }
            else
            {
                /*
                 * BeforeStart AND FetchOffset <= 0
                 */
                ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
            }
            break;

        case ULN_CURSOR_POS_AFTER_END:
            if (aOffset < 0)
            {
                /*
                 * (Before start AND FetchOffset > 0) OR (After end AND FetchOffset < 0)
                 */
                ulnCursorMoveAbsolute(aFnContext, aCursor, aOffset);
            }
            else
            {
                /*
                 * After end AND FetchOffset >= 0
                 */
                ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
            }
            break;

        default:
            ACE_ASSERT(sCurrentPosition > 0);

            if (1 <= sCurrentPosition + aOffset)
            {
                if (sCurrentPosition + aOffset <= sResultSetSize)
                {
                    /*
                     * 1 <= CurrRowsetStart + FetchOffset <= LastResultRow (M$ODBC ǥ�� 6��° ��)
                     */
                    ulnCursorSetPosition(aCursor, sCurrentPosition + aOffset);
                }
                else
                {
                    /*
                     * CurrRowsetStart + FetchOffset > LastResultRow (M$ ODBC ǥ�� 7��° ��)
                     */
                    ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
                }
            }
            else
            {
                if (sCurrentPosition == 1 && aOffset < 0)
                {
                    /*
                     * CurrRowsetStart = 1 AND FetchOffset < 0 (M$ ODBC ǥ�� 3��° ��)
                     */
                    ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
                }
                else
                {
                    if (1 > sCurrentPosition + aOffset)
                    {
                        if (ULN_ABS(aOffset) > sRowSetSize)
                        {
                            /*
                             * CurrRowsetStart > 1 AND CurrRowsetStart + FetchOffset < 1 AND
                             *      | FetchOffset | > RowsetSize
                             * (M$ ODBC ǥ�� 4��° ��)
                             */
                            ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
                        }
                        else
                        {
                            /*
                             * CurrRowsetStart > 1 AND CurrRowsetStart + FetchOffset < 1 AND
                             *      | FetchOffset | <= RowsetSize[3]
                             * (M$ ODBC ǥ�� 5���� ��)
                             */
                            ulnCursorSetPosition(aCursor, 1);

                            ulnError(aFnContext, ulERR_IGNORE_FETCH_BEFORE_RESULTSET);
                        }
                    }
                    else
                    {
                        /*
                         * BUGBUG : ���Ⱑ M$ ODBC SQLFetchScroll() �Լ� ������ �����
                         *          Cursor positioning rule �� �ִ� SQL_FETCH_RELATIVE �� �ִ�
                         *          ǥ �󿡼��� ������ �����ε�,
                         *          ���� �����ϱ�? �Ӹ�������. �׳� ����. ������ �׳�?
                         *
                         *          ���۰��� ��������, �ǻ� ������ �ƴϴ�.
                         *              if (1 <= sCurrentPosition + aOffset) {}
                         *              else
                         *              .... if (1 > sCurrentPosition + aOffset)
                         */
                        ACE_ASSERT(0);
                    }
                }
            }
            break;
    } /* switch(sCurrentPosition) */

    /* PROJ-1789 Updatable Scrollable Cursor */
    if (aOffset < 0)
    {
        ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_PREV);
    }
    else if (aOffset > 0)
    {
        ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_NEXT);
    }
    else /* aOffset == 0 */
    {
        /* do nothing. keep prev direction. */
    }
}

void ulnCursorMoveNext(ulnFnContext *aFnContext, ulnCursor *aCursor)
{

#if 0
    /*
     * BUGBUG : ���� �Ȱ�����?
     */
    ulnCursorMoveRelative(aFnContext, aCursor, ulnCursorGetSize(aCursor));

#else

    acp_sint64_t sCurrentPosition;
    acp_uint32_t sRowSetSize;
    ulnStmt*     sStmt = aCursor->mParentStmt;

    ACP_UNUSED(aFnContext);

    sRowSetSize      = ulnCursorGetSize(aCursor);
    // bug-35198: row array(rowset) size�� ������� fetch�ϴ� ���
    // ������ size�� �ѹ��� ����ؼ� cursor�� �������� �Ѵ�
    // MovePrior�� ��� ����� �ʿ� ���� (msdn�� ���� ����)
    if (sStmt->mPrevRowSetSize != 0)
    {
        sRowSetSize = sStmt->mPrevRowSetSize;
        sStmt->mPrevRowSetSize = 0; // �ѹ� ��������� clear
    }
    sCurrentPosition = ulnCursorGetPosition(aCursor);

    switch(sCurrentPosition)
    {
        case ULN_CURSOR_POS_BEFORE_START:
            ulnCursorSetPosition(aCursor, 1);
            break;

        case ULN_CURSOR_POS_AFTER_END:
            ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
            break;

        default:

            /*
             * BUGBUG : ulnCacheGetResultSetSize() �� �׻� ������ ���� �����ϰ� �����ؾ� �Ѵ�.
             */

            if ( (sCurrentPosition + sRowSetSize) >
                 ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache) )
            {
                /*
                 * CurrRowsetStart + RowsetSize > LastResultRow : AFTER END
                 */
                ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
            }
            else
            {
                /*
                 * CurrRowsetStart + RowsetSize <= LastResultRow : CurrRowsetStart + RowsetSize
                 */
                ulnCursorSetPosition(aCursor, sCurrentPosition + sRowSetSize);
            }
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_NEXT);
#endif
}

void ulnCursorMovePrior(ulnFnContext *aFnContext, ulnCursor *aCursor)
{
#if 0
    /*
     * BUGBUG : ���� �Ȱ�����?
     */
    ulnCursorMoveRelative(aFnContext, aCursor, ulnCursorGetSize(aCursor) * (-1));

#else

    acp_sint64_t sCurrentPosition;
    acp_uint32_t sRowSetSize;

    sRowSetSize      = ulnCursorGetSize(aCursor);
    sCurrentPosition = ulnCursorGetPosition(aCursor);

    switch(sCurrentPosition)
    {
        case ULN_CURSOR_POS_BEFORE_START:
        case 1:
            ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
            break;

        case ULN_CURSOR_POS_AFTER_END:

            /*
             * BUGBUG : ulnCacheGetResultSetSize() �� �׻� ������ ���� �����ϰ� �����ؾ� �Ѵ�.
             */

            if (sRowSetSize > ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache))
            {
                /*
                 * LastResultRow < RowsetSize : 1
                 */
                ulnCursorSetPosition(aCursor, 1);

                ulnError(aFnContext, ulERR_IGNORE_FETCH_BEFORE_RESULTSET);
            }
            else
            {
                /*
                 * LastResultRow >= RowsetSize : LastResultRow - RowsetSize + 1
                 */
                ulnCursorSetPosition(aCursor,
                                     ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache) -
                                     sRowSetSize +
                                     1);
            }
            break;

        default:
            ACE_ASSERT(sCurrentPosition > 1);

            if ((acp_uint32_t)sCurrentPosition <= sRowSetSize)
            {
                /*
                 * 1 < CurrRowsetStart <= RowsetSize : 1
                 */
                ulnCursorSetPosition(aCursor, 1);

                ulnError(aFnContext, ulERR_IGNORE_FETCH_BEFORE_RESULTSET);
            }
            else
            {
                /*
                 * CurrRowsetStart > RowsetSize : CurrRowsetStart - RowsetSize
                 */
                ulnCursorSetPosition(aCursor, sCurrentPosition - sRowSetSize);
            }
            break;
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_PREV);
#endif
}

void ulnCursorMoveFirst(ulnCursor *aCursor)
{
    ulnCursorSetPosition(aCursor, 1);

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_NEXT);
}

void ulnCursorMoveLast(ulnCursor *aCursor)
{
    acp_uint32_t sRowSetSize;
    acp_sint64_t sResultSetSize;

    sRowSetSize    = ulnCursorGetSize(aCursor);
    sResultSetSize = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);


    /*
     * BUGBUG : ulnCacheGetResultSetSize() �� �׻� ������ ���� �����ϰ� �����ؾ� �Ѵ�.
     */

    if (sRowSetSize > sResultSetSize)
    {
        ulnCursorSetPosition(aCursor, 1);
    }
    else
    {
        ulnCursorSetPosition(aCursor, sResultSetSize - sRowSetSize + 1);
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_PREV);
}

/* PROJ-1789 Updatable Scrollable Cursor */

/**
 * Bookmark�� �������� Ŀ���� �����δ�.
 *
 * @param[in] aFnContext function context
 * @param[in] aCursor    Ŀ��
 * @param[in] aOffset    Bookmark row�� ������ ��� ��ġ
 */
void ulnCursorMoveByBookmark(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset)
{
    acp_sint64_t sResultSetSize;
    acp_sint64_t sBookmarkPosition;
    acp_sint64_t sTargetPosition;

    ACP_UNUSED(aFnContext);

    sResultSetSize = ulnCacheGetResultSetSize(aCursor->mParentStmt->mCache);
    sBookmarkPosition = ulnStmtGetAttrFetchBookmarkVal(aCursor->mParentStmt);
    sTargetPosition = sBookmarkPosition + aOffset;

    if (sTargetPosition < 1)
    {
        /* BookmarkRow + FetchOffset < 1 */
        ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_BEFORE_START);
    }
    else if (sTargetPosition > sResultSetSize)
    {
        /* BookmarkRow + FetchOffset > LastResultRow  */
        ulnCursorSetPosition(aCursor, ULN_CURSOR_POS_AFTER_END);
    }
    else /* if ((1 <= sTargetPosition) && (sTargetPosition <= sResultSetSize)) */
    {
        /* 1 <= BookmarkRow + FetchOffset <= LastResultRow  */
        ulnCursorSetPosition(aCursor, sTargetPosition);
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorSetDirection(aCursor, ULN_CURSOR_DIR_NEXT);
}

/**
 * Ŀ�� ������ ��´�.
 *
 * @param[in] aCursor cursor object
 *
 * @return Ŀ�� ����
 */
ulnCursorDir ulnCursorGetDirection(ulnCursor *aCursor)
{
    return aCursor->mDirection;
}

/**
 * Ŀ�� ������ �����Ѵ�.
 *
 * @param[in] aCursor    cursor object
 * @param[in] aDirection Ŀ�� ����
 */
void ulnCursorSetDirection(ulnCursor *aCursor, ulnCursorDir aDirection)
{
    aCursor->mDirection = aDirection;
}

acp_bool_t ulnCursorHasNoData(ulnCursor *aCursor)
{
    ulnStmt    * sStmt   = aCursor->mParentStmt;
    acp_bool_t   sResult = ACP_FALSE;

    /* SELECT�� �ƴ� ���, Result Set Count�� 0 �̴�.
     * SQLCloseCursor()�� ������ ���, ulnCursorGetState()�� ULN_CURSOR_STATE_CLOSED�� ��ȯ�Ѵ�.
     */
    if ( ( ulnStmtGetResultSetCount( sStmt ) == 0 ) ||
         ( ulnCursorGetState( aCursor ) == ULN_CURSOR_STATE_CLOSED ) )
    {
        sResult = ACP_TRUE;
    }
    else
    {
        /* Result Set�� ���� ���� ���, ������ Result Set���� Ȯ���Ѵ�. (SQLMoreResults)
         * ���� Result Set�� ��� ����ߴ��� Ȯ���Ѵ�. (SQLFetch)
         */
        if ( ( ulnStmtGetCurrentResultSetID( sStmt ) >= ( ulnStmtGetResultSetCount( sStmt ) - 1 ) ) &&
             ( ulnCursorGetServerCursorState( aCursor ) == ULN_CURSOR_STATE_CLOSED ) )
        {
            sResult = ACP_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return sResult;
}
