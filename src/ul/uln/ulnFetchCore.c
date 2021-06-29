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
#include <ulnFetch.h>
#include <ulnGetData.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnSemiAsyncPrefetch.h>

static ACI_RC ulnFetchCoreRetrieveAllRowsFromServer(ulnFnContext *aFnContext,
                                                    ulnPtContext *aPtContext)
{
    SQLRETURN  sOriginalRC;

    /*
     * Trick ; ��� ����-_- ������,
     * �ϴ� �������� ��� �����͸� �� ���� ��
     */

    sOriginalRC = ULN_FNCONTEXT_GET_RC(aFnContext);

    ACI_TEST(ulnFetchAllFromServer(aFnContext, aPtContext) != ACI_SUCCESS);

    /*
     * ulnFetchAllFromServer() �Լ��� �ϴ� �� ��ġ�� ���� SQL_NO_DATA �� �̿��ؼ�
     * ��� ��ġ�� �Դ����� ���θ� �Ǵ��ϹǷ�
     * ��ġ�� �� ����� �־ SQL_NO_DATA �� ���õǾ� �ִ�.
     * ���󺹱��� ��� �Ѵ�.
     */

    if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_NO_DATA)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, sOriginalRC);
    }

    /*
     * �� �Լ� ȣ�� �� Ŀ���� ��ġ�� ������ �� ��� �Ѵ�.
     * ��, �ѹ� �� LAST Ȥ�� ABSOLUTE(x) �� Ŀ���� �ű��. �׸��� �Ʒ�����
     * ulnFetchFromCache() �Լ��� ĳ���� record ���� �����ش�.
     *
     * BUGBUG : �������� result set �� ũ�Ⱑ ACP_SINT32_MAX �� ���,
     *          ������ ������ ������ �׽�Ʈ �� ���� ���ߴ�. �ٽ��ѹ� ������ ���� ���װ�
     *          ���� �� ���⵵ �ϰ�... rowset size ��ŭ ���ؼ� ������ request
     *          �ϹǷ�...
     */

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnFetchCoreForIPCDASimpleQuery(ulnFnContext *aFnContext,
                                       ulnStmt      *aStmt,
                                       acp_uint32_t *aFetchedRowCount)
{
    ulnDescRec         *sDescRecArd  = NULL;
    ulnDescRec         *sDescRecIrd  = NULL;
    acp_uint8_t        *sSrc         = NULL;

    acp_uint16_t        sColumnId    = 0;
    acp_uint32_t        sDataRowSize = 0;
    acp_uint32_t        sNextDataPos = 0;

    *aFetchedRowCount = 0;

    ACI_TEST(aStmt->mCacheIPCDA.mFetchBuffer == NULL);

    sSrc = aStmt->mCacheIPCDA.mFetchBuffer;

    if (aStmt->mCacheIPCDA.mRemainingLength == 0)
    {
        /*�� �̻� ���� �����Ͱ� ����*/
    }
    else
    {
        sSrc += aStmt->mCacheIPCDA.mReadLength;

        // �ο��� ��ü ����
        sDataRowSize = *((acp_uint32_t*)sSrc);
        sSrc +=8;
        aStmt->mCacheIPCDA.mReadLength      += 8;
        aStmt->mCacheIPCDA.mRemainingLength -= 8;

        // ���� �������� ��ġ
        sNextDataPos = aStmt->mCacheIPCDA.mReadLength + sDataRowSize - 8;

        while (aStmt->mCacheIPCDA.mReadLength < sNextDataPos)
        {
            /* read ColumnId */
            sColumnId = *((acp_uint16_t*)sSrc);
            sSrc += 8;

            sDescRecArd = ulnDescGetDescRec(aStmt->mAttrArd, sColumnId);
            sDescRecIrd = ulnDescGetDescRec(aStmt->mAttrIrd, sColumnId);
            ACI_TEST(sDescRecIrd == NULL);

            ACI_TEST(ulnCopyToUserBufferForSimpleQuery(aFnContext,
                                                       aStmt,
                                                       sSrc,
                                                       sDescRecArd,
                                                       sDescRecIrd)
                     != ACI_SUCCESS );

            sSrc += sDescRecIrd->mMaxByteSize;
            aStmt->mCacheIPCDA.mReadLength      += (sDescRecIrd->mMaxByteSize + 8);
            aStmt->mCacheIPCDA.mRemainingLength -= (sDescRecIrd->mMaxByteSize + 8);
        }
        *aFetchedRowCount = 1;
    }

    if (*aFetchedRowCount == 0)
    {
        /*
         * BUGBUG : ulnFetchFromCache() �Լ� �ּ��� ���� �Ʒ��� ���� ���� �ִ� :
         *
         * SQL_NO_DATA �� ������ �ֱ� ���ؼ� ������ �ؾ� ������,
         * �̹� ulnFetchUpdateAfterFetch() �Լ����� �����ع��� ����̴�.
         */
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_NO_DATA);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_FNCONTEXT_SET_RC(aFnContext, SQL_ERROR);

    return ACI_FAILURE;
}

ACI_RC ulnCheckFetchOrientation(ulnFnContext *aFnContext,
                                acp_sint16_t  aFetchOrientation)
{
    ulnStmt     *sStmt = aFnContext->mHandle.mStmt;

    /*
     * FORWARD ONLY Ŀ���� �� FETCH_NEXT ���� �ٸ������� ȣ���ϸ� ����.
     */
    if ( (ulnCursorGetType(&sStmt->mCursor) == SQL_CURSOR_FORWARD_ONLY)
      || (ulnCursorGetScrollable(&sStmt->mCursor) == SQL_NONSCROLLABLE) )
    {
        ACI_TEST_RAISE(aFetchOrientation != SQL_FETCH_NEXT,
                       LABEL_FETCH_TYPE_OUT_OF_RANGE);
    }

    /* PROJ-1789 Updatable Scrollable Cursor
     * SQL_FETCH_BOOKMARK�� ��, ���� �Ӽ��� ����� �����߳� Ȯ�� */
    if (aFetchOrientation == SQL_FETCH_BOOKMARK)
    {
        ACI_TEST_RAISE(ulnStmtGetAttrUseBookMarks(sStmt) == SQL_UB_OFF,
                       LABEL_FETCH_TYPE_OUT_OF_RANGE);

        ACI_TEST_RAISE( ulnStmtGetAttrFetchBookmarkVal(sStmt) <= 0,
                        LABEL_INVALID_BOOKMARK_VALUE );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FETCH_TYPE_OUT_OF_RANGE)
    {
        /*
         * HY106 : Fetch type out of range
         */
        ulnError(aFnContext, ulERR_ABORT_WRONG_FETCH_TYPE);
    }
    ACI_EXCEPTION(LABEL_INVALID_BOOKMARK_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_BOOKMARK_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnFetchCore(ulnFnContext *aFnContext,
                    ulnPtContext *aPtContext,
                    acp_sint16_t  aFetchOrientation,
                    ulvSLen       aFetchOffset,
                    acp_uint32_t *aNumberOfRowsFetched)
{
    ulnStmt      *sStmt = aFnContext->mHandle.mStmt;
    ulnDescRec   *sDescRecArd;
    acp_sint16_t  sOdbcConciseType;
    acp_uint32_t  sAttrUseBookMarks;
    acp_sint32_t  sFetchOffset;

    sDescRecArd = ulnStmtGetArdRec(sStmt, 0);
    if (sDescRecArd != NULL)
    {
        sOdbcConciseType = ulnMetaGetOdbcConciseType(&sDescRecArd->mMeta);
        sAttrUseBookMarks = ulnStmtGetAttrUseBookMarks(sStmt);

        ACI_TEST_RAISE(sAttrUseBookMarks == SQL_UB_OFF,
                       LABEL_INVALID_DESCRIPTOR_INDEX_0);

        ACI_TEST_RAISE((sOdbcConciseType == SQL_C_BOOKMARK) &&
                       (sAttrUseBookMarks == SQL_UB_VARIABLE),
                       LABEL_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK_TYPE);

        ACI_TEST_RAISE((sOdbcConciseType == SQL_C_VARBOOKMARK) &&
                       (sAttrUseBookMarks != SQL_UB_VARIABLE),
                       LABEL_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK_TYPE);
    }

    ACI_TEST_RAISE(aFetchOffset > ACP_SINT32_MAX, LABEL_FETCH_OFFSET_OUT_OF_RANGE);
    ACI_TEST_RAISE(aFetchOffset < ACP_SINT32_MIN, LABEL_FETCH_OFFSET_OUT_OF_RANGE);

    sFetchOffset = aFetchOffset;

    /*
     * GetData() �� ���� �ʱ�ȭ : Fetch �� �θ������� �Ͼ�� �Ѵ�.
     *
     * ���� ����ڰ� GetData() �� �����͸� ���ݸ� ��������
     * ���� fetch �� �θ���, �� �� getdata() �� �ϴ� ��츦 �����.
     */

    if (sStmt->mGDColumnNumber != ULN_GD_COLUMN_NUMBER_INIT_VALUE)
    {
        ACI_TEST(ulnGDInitColumn(aFnContext, sStmt->mGDColumnNumber) != ACI_SUCCESS);
        sStmt->mGDColumnNumber = ULN_GD_COLUMN_NUMBER_INIT_VALUE;
    }
    sStmt->mGDTargetPosition = ULN_GD_TARGET_POSITION_INIT_VALUE;

    /*
     * ===============================================
     * step 1. Ŀ���� ����ڰ� ������ ��ġ�� �ű��.
     * ===============================================
     */

    switch(aFetchOrientation)
    {
        case SQL_FETCH_NEXT:
            ulnCursorMoveNext(aFnContext, &sStmt->mCursor);
            break;

        case SQL_FETCH_PRIOR:
            ulnCursorMovePrior(aFnContext, &sStmt->mCursor);
            break;

        case SQL_FETCH_RELATIVE:
            ulnCursorMoveRelative(aFnContext, &sStmt->mCursor, sFetchOffset);
            break;

        case SQL_FETCH_ABSOLUTE:
            /* PROJ-1789 Updatable Scrollable Cursor */
            if (ulnCursorGetType(&sStmt->mCursor) == SQL_CURSOR_KEYSET_DRIVEN)
            {
                if (sFetchOffset < 0)
                {
                    ACI_TEST(ulnFetchFromCacheForKeyset(aFnContext, aPtContext,
                                                        sStmt,
                                                        ULN_KEYSET_FILL_ALL)
                             != ACI_SUCCESS);
                }
                else if (sFetchOffset > 0)
                {
                    ACI_TEST(ulnFetchFromCacheForKeyset(aFnContext, aPtContext,
                                                        sStmt,
                                                        sFetchOffset)
                             != ACI_SUCCESS);
                }
                else /* sFetchOffset == 0 */
                {
                    /* do nothing */
                }
            }
            else /* is STATIC */
            {
                // fix BUG-17715
                if (sFetchOffset < 0)
                {
                    ACI_TEST(ulnFetchCoreRetrieveAllRowsFromServer(aFnContext,
                                                                   aPtContext)
                             != ACI_SUCCESS);
                }
            }

            ulnCursorMoveAbsolute(aFnContext, &sStmt->mCursor, sFetchOffset);
            break;

        case SQL_FETCH_FIRST:
            ulnCursorMoveFirst(&sStmt->mCursor);
            break;

        case SQL_FETCH_LAST:
            //ulnCursorMoveLast(&sStmt->mCursor);

            /* PROJ-1789 Updatable Scrollable Cursor */
            if (ulnCursorGetType(&sStmt->mCursor) == SQL_CURSOR_KEYSET_DRIVEN)
            {
                ACI_TEST(ulnFetchFromCacheForKeyset(aFnContext, aPtContext,
                                                    sStmt,
                                                    ULN_KEYSET_FILL_ALL)
                         != ACI_SUCCESS);
            }
            else /* is STATIC */
            {
                ACI_TEST(ulnFetchCoreRetrieveAllRowsFromServer(aFnContext,
                                                               aPtContext)
                         != ACI_SUCCESS);
            }

            ulnCursorMoveLast(&sStmt->mCursor);
            break;

        case SQL_FETCH_BOOKMARK:
            ulnCursorMoveByBookmark(aFnContext, &sStmt->mCursor, sFetchOffset);
            break;

        default:
            ACI_RAISE(LABEL_FETCH_TYPE_OUTOF_RANGE);
            break;
    }

    // bug-35198: row array(rowset) size�� ������� fetch�ϴ� ���.
    // cursor�� �Ű����ٸ� prev rowset size�� ���̻� ����ؼ��� �ȵ�.
    // ex) FETCH_LAST �� �Ŀ� FETCH_NEXT�ϴ� ��� �߰��� clear �ؾ���
    sStmt->mPrevRowSetSize = 0;

    /*
     * ===============================================
         * step 2. ĳ������ fetch�� �´�.
     * ===============================================
     */

    ulnDescSetRowsProcessedPtrValue(sStmt->mAttrIrd, 0);

    if (ulnCursorGetType(&sStmt->mCursor) == SQL_CURSOR_KEYSET_DRIVEN)
    {
        ACI_TEST(ulnFetchFromCacheForSensitive(aFnContext, aPtContext,
                                               sStmt, aNumberOfRowsFetched)
                 != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnFetchFromCache(aFnContext, aPtContext,
                                   sStmt, aNumberOfRowsFetched)
                 != ACI_SUCCESS);
    }

    if (*aNumberOfRowsFetched == 0)
    {
        /*
         * BUGBUG : ulnFetchFromCache() �Լ� �ּ��� ���� �Ʒ��� ���� ���� �ִ� :
         *
         *          SQL_NO_DATA �� ������ �ֱ� ���ؼ� ������ �ؾ� ������,
         *          �̹� ulnFetchUpdateAfterFetch() �Լ����� �����ع��� ����̴�.
         */
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_NO_DATA);
    }

    /* PROJ-2177: Cancel �� �������̸� ����, ���������� Fetch�� �Լ��� ����صд�. */
    ulnStmtSetLastFetchFuncID(sStmt, aFnContext->mFuncID);

    /* PROJ-1789 Rowset Position�� Fetch�ϸ� �ʱ�ȭ�ȴ�. */
    ulnCursorSetRowsetPosition(ulnStmtGetCursor(sStmt), 1);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_DESCRIPTOR_INDEX_0)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, 0);
    }
    ACI_EXCEPTION(LABEL_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK_TYPE)
    {
        ulnError(aFnContext, ulERR_ABORT_RESTRICTED_DATATYPE_VIOLATION_BOOKMARK_TYPE);
    }
    ACI_EXCEPTION(LABEL_FETCH_TYPE_OUTOF_RANGE)
    {
        /* HY106 */
        ulnError(aFnContext, ulERR_ABORT_WRONG_FETCH_TYPE);
    }
    ACI_EXCEPTION(LABEL_FETCH_OFFSET_OUT_OF_RANGE)
    {
        ulnError(aFnContext, ulERR_ABORT_FETCH_OFFSET_OUT_OF_RANGE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
