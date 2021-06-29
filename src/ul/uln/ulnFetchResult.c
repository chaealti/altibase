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

/*
 * ======================================
 * Fetch �Ǿ� �� �����͸� ó���ϴ� �Լ���
 * ======================================
 */

ACI_RC ulnCallbackFetchResult(cmiProtocolContext *aProtocolContext,
                              cmiProtocol        *aProtocol,
                              void               *aServiceSession,
                              void               *aUserContext)
{
    ulnFnContext  *sFnContext = (ulnFnContext*)aUserContext;
    ulnDbc        *sDbc;
    ulnStmt       *sStmt      = sFnContext->mHandle.mStmt;
    ulnCache      *sCache     = ulnStmtGetCache(sStmt);
    acp_uint8_t   *sRow;
    acp_uint32_t   sRowSize;
    acp_sint64_t   sPosition;
    acp_sint64_t   sPRowID;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    CMI_RD4(aProtocolContext, &sRowSize);

    ACI_TEST_RAISE(sCache == NULL, LABEL_MEM_MANAGE_ERR);

    ACI_TEST_RAISE( ulnCacheAllocChunk( sCache,
                                        sRowSize,
                                        &sRow )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM );

    if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST_RAISE( cmiSplitReadIPCDA(aProtocolContext,
                                          sRowSize,
                                          &sRow,
                                          sRow) != ACI_SUCCESS,
                        cm_error );
        sStmt->mCacheIPCDA.mRemainingLength = sRowSize;
    }
    else
    {
        ACI_TEST_RAISE( cmiSplitRead( aProtocolContext,
                                      sRowSize,
                                      sRow,
                                      sDbc->mConnTimeoutValue )
                        != ACI_SUCCESS, cm_error );
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    if (sStmt->mParentStmt != NULL)
    {
        /* _PROWID�� �������� �����Ƿ� Cache �������� �˾ƾ� �Ѵ�. */
        ulnCacheSetStartPosition( sCache, sStmt->mRowsetCacheStartPosition );

        // only read _PROWID
        CM_ENDIAN_ASSIGN8(&sPRowID, sRow);

        sPosition = ulnKeysetGetSeq( sStmt->mParentStmt->mKeyset,
                                     (acp_uint8_t *) &sPRowID);
        ACE_ASSERT(sPosition > 0);

        ACI_TEST(ulnCacheSetRPByPosition( sCache,
                                          sRow + ACI_SIZEOF(acp_sint64_t),
                                          sPosition )
                 != ACI_SUCCESS );
    }
    else
    {
        ACI_TEST_RAISE( ulnCacheAddNewRP(sCache, sRow)
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM );
    }

    sStmt->mFetchedRowCountFromServer += 1;

    /**
     * PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
     *
     *   5 = OpID(1) + row size(4)
     *
     *   +---------+-------------+---------------------------------+
     *   | OpID(1) | row size(4) | row data(row size)              |
     *   +---------+-------------+---------------------------------+
     *
     *   Caution : mFetchedBytesFromServer isn't contained CM header (16).
     */
    sStmt->mFetchedBytesFromServer += sRowSize + 5;

    ulnCacheInitInvalidated(sCache);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(sFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnCallbackFetchResult : Cache is NULL");
    }
    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_ALLOC_ERROR,
                 "ulnCallbackFetchResult");
    }
    ACI_EXCEPTION(cm_error)
    {
        return ACI_FAILURE;
    }
    ACI_EXCEPTION_END;

    if( sRowSize != 0 )
    {
        ACI_TEST_RAISE( cmiSplitSkipRead( aProtocolContext,
                                          sRowSize,
                                          sDbc->mConnTimeoutValue )
                        != ACI_SUCCESS, cm_error );
    }

    /*
     * Note : ACI_SUCCESS �� �����ϴ� ���� ���װ� �ƴϴ�.
     *        cm �� �ݹ��Լ��� ACI_FAILURE �� �����ϸ� communication error ��
     *        ��޵Ǿ� ������ ������ ������ ACI_SUCCESS �� �����ؾ� �Ѵ�.
     *
     *        ����Ǿ��� ����, Function Context �� ����� mSqlReturn �� �Լ� ���ϰ���
     *        ����ǰ� �� ���̸�, uln �� cmi ���� �Լ��� ulnReadProtocol() �Լ� �ȿ���
     *        Function Context �� mSqlReturn �� üũ�ؼ� ������ ��ġ�� ���ϰ� �� ���̴�.
     */
    return ACI_SUCCESS;
}

ACI_RC ulnCallbackFetchBeginResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext)
{
    ulnFnContext  *sFnContext = (ulnFnContext*)aUserContext;
    ulnStmt       *sStmt      = sFnContext->mHandle.mStmt;
    ulnCache      *sCache     = ulnStmtGetCache(sStmt);
    ulnKeyset     *sKeyset    = ulnStmtGetKeyset(sStmt);
    ulnDescRec    *sDescRecIrd;

    acp_uint16_t   sColumnNumber;
    acp_sint16_t   sColumnNumberAdjust;
    acp_uint16_t   sColumnCount    = ulnStmtGetColumnCount(sStmt);
    acp_uint32_t   sColumnPosition;
    acp_uint32_t   sColumnDataSize = 0;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_SKIP_READ_BLOCK( aProtocolContext, 6);

    /*
     * Note : �� �ݹ��� �������� ������ ���ο� fetch �� ���۵Ǿ��� ������ ȣ��ȴ�.
     *        ����, �̹� �Ҵ��� �޸𸮸� �״�� �ΰ� ĳ���� ������ �ʱ�ȭ �ع����� �Ѵ�.
     *
     *        ���±����� fetch �� ����� �� ������.
     *
     *        ĳ���� �Ŵ޸� ulnLob ���� CloseCursor() �ϰų�, ĳ�� �̽��� �߻��ϸ� free �ϹǷ�
     *        ���⼭ �Ű澲�� �ʾƵ� �ȴ�.
     */
    ACI_TEST( ulnCacheInitialize(sCache) != ACI_SUCCESS );

    if (sKeyset != NULL)
    {
        ACI_TEST(ulnKeysetInitialize(sKeyset) != ACI_SUCCESS );
    }

    if (sStmt->mParentStmt != NULL)
    {
        ACI_TEST(ulnCacheRebuildRPAForSensitive(sStmt, sCache)
                 != ACI_SUCCESS);
    }

    /*
     * column info �� ������ �迭�� �غ��Ѵ�.
     */
    ACI_TEST_RAISE(ulnCachePrepareColumnInfoArray(sCache, sColumnCount)
                   != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    /*
     * ĳ���� �÷� ����� ��� �Ҵ� ������ �����Ѵ�.
     */

    sColumnPosition = 0;

    /* PROJ-1789 Updatable Scrollable Cursor
     * Bookmark ó���� ���� 0��° �÷� ������ �ʱ�ȭ �ؾ��Ѵ�.
     * ���⼭ MType�� USE_BOOKMARK�� ���� �ٲ� �� �ִµ�, �ϴ� BIGINT�� �صд�.
     * (@seealso ulnDataBuildColumnZero) */
    ulnCacheSetColumnInfo(sCache, 0, ULN_MTYPE_BIGINT, 0);

    /* PROJ-1789 Updatable Scrollable Cursor
     * Keyset-driven�̸� ù��° �÷����� _PROWID�� �߰��ȴ�. �̰� ������Ѵ�. */
    if (sStmt->mParentStmt != SQL_NULL_HSTMT) /* is RowsetStmt ? */
    {
        sColumnNumber       = 2;
        sColumnNumberAdjust = -1;
    }
    else
    {
        sColumnNumber       = 1;
        sColumnNumberAdjust = 0;
    }
    for (; sColumnNumber <= sColumnCount; sColumnNumber++)
    {
        /*
         * BUG-28623 [CodeSonar]Null Pointer Dereference
         * sDescRecIrd�� ���� NULL �˻簡 ����
         * ���� ACI_TEST �˻� �߰�
         */
        sDescRecIrd = ulnDescGetDescRec(sStmt->mAttrIrd, sColumnNumber);
        ACI_TEST( sDescRecIrd == NULL );

        ulnCacheSetColumnInfo(sCache, sColumnNumber + sColumnNumberAdjust,
                              ulnMetaGetMTYPE(&sDescRecIrd->mMeta),
                              sColumnPosition);

        sColumnDataSize = sDescRecIrd->mMeta.mOctetLength;

        sColumnPosition += ACP_ALIGN8(ACI_SIZEOF(ulnColumn)) + ACP_ALIGN8(sColumnDataSize);
    }

    ulnCacheSetSingleRowSize(sCache,
                             ACP_ALIGN8(ACI_SIZEOF(ulnRow)) + ACP_ALIGN8(sColumnPosition));

    /*
     * Ŀ�� ���� ����
     */
    ulnCursorSetServerCursorState(&sStmt->mCursor, ULN_CURSOR_STATE_OPEN);

    ulnCacheInitInvalidated(sCache);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "CallbackFetchBeginResult");
    }

    ACI_EXCEPTION_END;

    /*
     * Note : ACI_SUCCESS �� �����ϴ� ���� ���װ� �ƴϴ�.
     *        cm �� �ݹ��Լ��� ACI_FAILURE �� �����ϸ� communication error �� ��޵Ǿ� ������
     *        �����̴�.
     *
     *        ����Ǿ��� ����, Function Context �� ����� mSqlReturn �� �Լ� ���ϰ���
     *        ����ǰ� �� ���̸�, uln �� cmi ���� �Լ��� ulnReadProtocol() �Լ� �ȿ���
     *        Function Context �� mSqlReturn �� üũ�ؼ� ������ ��ġ�� ���ϰ� �� ���̴�.
     */
    return ACI_SUCCESS;
}

ACI_RC ulnCallbackFetchEndResult(cmiProtocolContext *aProtocolContext,
                                 cmiProtocol        *aProtocol,
                                 void               *aServiceSession,
                                 void               *aUserContext)
{
    ulnFnContext  *sFnContext = (ulnFnContext*)aUserContext;
    ulnStmt       *sStmt      = sFnContext->mHandle.mStmt;
    ulnCache      *sCache     = ulnStmtGetCache(sStmt);
    ulnCursor     *sCursor    = ulnStmtGetCursor(sStmt);

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_SKIP_READ_BLOCK( aProtocolContext, 6);

    /* PROJ-1381, BUG-32902 FAC
     * Holdable Statement�� CloseCursor ������ ���Ƿ� ������ �ʴ´�. */
    /* PROJ-1789 Updatable Scrollable Cursor
     * Keyset-driven�� CloseCursor ������ ���Ƿ� ������ �ʴ´�. */
    if ((ulnCursorGetHoldability(sCursor) != SQL_CURSOR_HOLD_ON)
     && (ulnCursorGetType(sCursor) != SQL_CURSOR_KEYSET_DRIVEN))
    {
        ulnCursorSetServerCursorState(sCursor, ULN_CURSOR_STATE_CLOSED);
    }

    if (sCache != NULL)
    {
        // To Fix BUG-20409
        ulnCacheSetResultSetSize(sCache, ulnCacheGetTotalRowCnt(sCache));
    }

    return ACI_SUCCESS;
}

ACI_RC ulnCallbackFetchMoveResult(cmiProtocolContext *aProtocolContext,
                                  cmiProtocol        *aProtocol,
                                  void               *aServiceSession,
                                  void               *aUserContext )
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

