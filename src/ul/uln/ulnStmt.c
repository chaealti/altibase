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
#include <ulnStmt.h>
#include <ulnCache.h>
#include <ulsdStmtInline.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnSemiAsyncPrefetch.h>
#include <ulnFetchOpAsync.h>

ACI_RC ulnStmtCreate(ulnDbc *aParentDbc, ulnStmt **aOutputStmt)
{
    ULN_FLAG(sNeedDestroyMemory);
    ULN_FLAG(sNeedDestroyARD);
    ULN_FLAG(sNeedDestroyAPD);
    ULN_FLAG(sNeedDestroyIPD);
    ULN_FLAG(sNeedDestroyIRD);

    ulnStmt         *sStmt;

    uluMemory       *sMemory;
    uluChunkPool    *sPool;

    sPool = aParentDbc->mObj.mPool;

    /*
     * �޸� �ν��Ͻ� ����. ûũǮ�� DBC�� ���� ���.
     */

    ACI_TEST(uluMemoryCreate(sPool, &sMemory) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyMemory);

    /*
     * --------------------
     * STMT
     * --------------------
     */

    ACI_TEST(sMemory->mOp->mMalloc(sMemory, (void **)&sStmt, ACI_SIZEOF(ulnStmt)) != ACI_SUCCESS);
    ACI_TEST(sMemory->mOp->mMarkSP(sMemory) != ACI_SUCCESS);

    ulnObjectInitialize((ulnObject *)sStmt,
                        ULN_OBJ_TYPE_STMT,
                        ULN_DESC_TYPE_NODESC,
                        ULN_S_S1,
                        sPool,
                        sMemory);

    sStmt->mObj.mLock = aParentDbc->mObj.mLock;

    /*
     * Diagnostic Header �ʱ�ȭ
     * ���н� sMemory �� �����ϴ� ������ ���. DBC �� ûũǮ�� ��ӹ����Ƿ� ������ �޸� ����.
     */

    ACI_TEST(ulnCreateDiagHeader((ulnObject *)sStmt, aParentDbc->mObj.mDiagHeader.mPool) 
             != ACI_SUCCESS);

    /*
     * --------------------
     * APD
     * --------------------
     */

    ACI_TEST(ulnDescCreate((ulnObject *)sStmt,
                           &sStmt->mAttrApd,
                           ULN_DESC_TYPE_APD,
                           ULN_DESC_ALLOCTYPE_IMPLICIT) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyAPD);

    ulnDescInitialize(sStmt->mAttrApd, (ulnObject *)sStmt);
    ulnDescInitializeUserPart(sStmt->mAttrApd);

    /*
     * --------------------
     * ARD
     * --------------------
     */

    ACI_TEST(ulnDescCreate((ulnObject *)sStmt,
                           &sStmt->mAttrArd,
                           ULN_DESC_TYPE_ARD,
                           ULN_DESC_ALLOCTYPE_IMPLICIT) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyARD);

    ulnDescInitialize(sStmt->mAttrArd, (ulnObject *)sStmt);
    ulnDescInitializeUserPart(sStmt->mAttrArd);

    /*
     * --------------------
     * IPD
     * --------------------
     */

    ACI_TEST(ulnDescCreate((ulnObject *)sStmt,
                           &sStmt->mAttrIpd,
                           ULN_DESC_TYPE_IPD,
                           ULN_DESC_ALLOCTYPE_IMPLICIT) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyIPD);

    ulnDescInitialize(sStmt->mAttrIpd, (ulnObject *)sStmt);
    ulnDescInitializeUserPart(sStmt->mAttrIpd);

    /*
     * --------------------
     * IRD
     * --------------------
     */

    ACI_TEST(ulnDescCreate((ulnObject *)sStmt,
                           &sStmt->mAttrIrd,
                           ULN_DESC_TYPE_IRD,
                           ULN_DESC_ALLOCTYPE_IMPLICIT) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyIRD);

    ulnDescInitialize(sStmt->mAttrIrd, (ulnObject *)sStmt);
    ulnDescInitializeUserPart(sStmt->mAttrIrd);

    /*
     * stmt �ڵ� ����
     */

    ACI_TEST(sMemory->mOp->mMalloc(sMemory,
                                   (void **)&sStmt->mChunk.mData,
                                   ULN_STMT_MAX_MEMORY_CHUNK_SIZE) != ACI_SUCCESS);

    sStmt->mChunk.mCursor = 0;
    sStmt->mChunk.mRowCount = 0;
    sStmt->mChunk.mSize = ULN_STMT_MAX_MEMORY_CHUNK_SIZE;

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    sStmt->mLobCache = NULL;

    *aOutputStmt = sStmt;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedDestroyIRD)
    {
        ulnDescDestroy(sStmt->mAttrIrd);
    }

    ULN_IS_FLAG_UP(sNeedDestroyIPD)
    {
        ulnDescDestroy(sStmt->mAttrIpd);
    }

    ULN_IS_FLAG_UP(sNeedDestroyARD)
    {
        ulnDescDestroy(sStmt->mAttrArd);
    }

    ULN_IS_FLAG_UP(sNeedDestroyAPD)
    {
        ulnDescDestroy(sStmt->mAttrApd);
    }

    ULN_IS_FLAG_UP(sNeedDestroyMemory)
    {
        sMemory->mOp->mDestroyMyself(sMemory);
    }

    return ACI_FAILURE;
}

ACI_RC ulnStmtDestroy(ulnStmt *aStmt)
{
    acp_bool_t     sIsSuccessful = ACP_FALSE;

    ACE_ASSERT(ULN_OBJ_GET_TYPE(aStmt) == ULN_OBJ_TYPE_STMT);

    /*
     * plan tree �޸� ����
     */

    if (aStmt->mPlanTree != NULL)
    {
        ulnStmtFreePlanTree(aStmt);
    }

    /*
     * DiagHeader �ı�
     */

    ACI_TEST(ulnDestroyDiagHeader(&aStmt->mObj.mDiagHeader, ULN_DIAG_HDR_NOTOUCH_CHUNKPOOL)
             != ACI_SUCCESS);

    sIsSuccessful = ACP_TRUE;

    /*
     * Result Cache ����
     */

    if (aStmt->mCache != NULL)
    {
        ulnCacheDestroy(aStmt->mCache);
        aStmt->mCache = NULL;
    }

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    if (aStmt->mLobCache != NULL)
    {
        ulnLobCacheDestroy(&aStmt->mLobCache);
    }
    else
    {
        /* Nothing */
    }

    /*
     * Note : STMT �� ���� ��� Explicit ��ũ������ ��ũ�� �����ϰ� DBC �� ������
     *        �ʿ������ ����.
     *
     *        ����, Explicit ��ũ���͸� ������ ������, STMT �� �� explicit ��ũ������
     *        mAssociatedStmtList ���� ������ �ִ� ������ ����ϴ�.
     *
     *        ������ Explicit DESC �� mParentObject �� ������ DBC �̸� DBC �� �̹� �ڽſ���
     *        ���� ���� DESC �� ����Ʈ�� ������ �ֱ� �����̴�.
     *
     *        STMT �� ���� �����͸� �����Ѵ�.
     */

    if (aStmt->mOrigArd != NULL && aStmt->mAttrArd != NULL)
    {
        /*
         * mOrigArd �� NULL �� �ƴ϶�� ���� Explicit Descriptor �� �����Ǿ� �ִٴ� ��.
         * STMT �� ������Ƿ� Explicit Descriptor �� mAssociatedStmtList ������
         * STMT �� �����ؾ� ��.
         */
        ulnDescRemoveStmtFromAssociatedStmtList(aStmt, aStmt->mAttrArd);
    }

    if (aStmt->mOrigApd != NULL && aStmt->mAttrApd != NULL)
    {
        /*
         * mOrigApd �� NULL �� �ƴ϶�� ���� Explicit Descriptor �� �����Ǿ� �ִٴ� ��.
         * STMT �� ������Ƿ� Explicit Descriptor �� mAssociatedStmtList ������
         * STMT �� �����ؾ� ��.
         */
        ulnDescRemoveStmtFromAssociatedStmtList(aStmt, aStmt->mAttrApd);
    }

    /*
     * STMT �� ���� ��� Implicit ��ũ���͵��� �ı�
     * BUGBUG: �Ѱ����� �����ϸ� �ѹ��� ����� �����Ƿ� �׳� ����.
     */

    if (ulnDescDestroy(aStmt->mAttrIrd) != ACI_SUCCESS)
    {
        sIsSuccessful = ACP_FALSE;
    }

    if (ulnDescDestroy(aStmt->mAttrIpd) != ACI_SUCCESS)
    {
        sIsSuccessful = ACP_FALSE;
    }

    if (ulnDescDestroy(aStmt->mAttrArd) != ACI_SUCCESS)
    {
        sIsSuccessful = ACP_FALSE;
    }

    if (ulnDescDestroy(aStmt->mAttrApd) != ACI_SUCCESS)
    {
        sIsSuccessful = ACP_FALSE;
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    if (aStmt->mIrd4KeysetDriven != NULL)
    {
        if (ulnDescDestroy(aStmt->mIrd4KeysetDriven) != ACI_SUCCESS)
        {
            sIsSuccessful = ACP_FALSE;
        }
    }

    ACI_TEST(sIsSuccessful != ACP_TRUE);

    /*
     * STMT �� ���ֱ� ������ �Ǽ��� ���� ������ �����ϱ� ���ؼ� ulnObject �� ǥ�ø� �� �д�.
     * BUG-15894 �� ���� ����� ���� ���α׷��� ���� ���׸� �����ϱ� ���ؼ��̴�.
     */

    aStmt->mObj.mType = ULN_OBJ_TYPE_MAX;

    /*
     * STMT �� ���� uluMemory �� �ı��Ѵ�.
     */

    aStmt->mObj.mMemory->mOp->mDestroyMyself(aStmt->mObj.mMemory);

    /* PROJ-1789 Updatable Scrollable Cursor */

    if (aStmt->mKeyset != NULL)
    {
        ulnKeysetDestroy(aStmt->mKeyset);
        aStmt->mKeyset = NULL;
    }

    /* Note. mRowsetStmt�� FreeStmt���� ó���Ѵ�. */

    if (aStmt->mPrepareTextBuf != NULL)
    {
        acpMemFree(aStmt->mPrepareTextBuf);

        aStmt->mPrepareTextBuf       = NULL;
        aStmt->mPrepareTextBufMaxLen = 0;
    }

    if (aStmt->mQstrForRowset != NULL)
    {
        acpMemFree(aStmt->mQstrForRowset);

        aStmt->mQstrForRowset         = NULL;
        aStmt->mQstrForRowsetMaxLen   = 0;
        aStmt->mQstrForRowsetLen      = 0;
        aStmt->mQstrForRowsetParamPos = 0;
        aStmt->mQstrForRowsetParamCnt = 0;
    }

    if (aStmt->mQstrForInsUpd != NULL)
    {
        acpMemFree(aStmt->mQstrForInsUpd);

        aStmt->mQstrForInsUpd       = NULL;
        aStmt->mQstrForInsUpdMaxLen = 0;
        aStmt->mQstrForInsUpdLen    = 0;
    }

    if (aStmt->mQstrForDelete != NULL)
    {
        acpMemFree(aStmt->mQstrForDelete);

        aStmt->mQstrForDelete       = NULL;
        aStmt->mQstrForDeleteMaxLen = 0;
        aStmt->mQstrForDeleteLen    = 0;
    }

    if (aStmt->mRowsetParamBuf != NULL)
    {
        acpMemFree(aStmt->mRowsetParamBuf);

        aStmt->mRowsetParamBuf       = NULL;
        aStmt->mRowsetParamBufMaxLen = 0;
    }

    if (aStmt->mRowsetParamStatusBuf != NULL)
    {
        acpMemFree(aStmt->mRowsetParamStatusBuf);

        aStmt->mRowsetParamStatusBuf       = NULL;
        aStmt->mRowsetParamStatusBufMaxCnt = 0;
    }

    if (aStmt->mColumnIgnoreFlagsBuf != NULL)
    {
        acpMemFree(aStmt->mColumnIgnoreFlagsBuf);

        aStmt->mColumnIgnoreFlagsBuf       = NULL;
        aStmt->mColumnIgnoreFlagsBufMaxCnt = 0;
    }

    /* PROJ-1721 Name-based Binding */
    if (aStmt->mAnalyzeStmt != NULL)
    {
        ulnAnalyzeStmtDestroy(&aStmt->mAnalyzeStmt);
    }

    /*PROJ-2616*/
    ulnCacheDestoryIPCDA(aStmt);

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    if (aStmt->mSemiAsyncPrefetch != NULL)
    {
        ulnFreeSemiAsyncPrefetch(aStmt->mSemiAsyncPrefetch);
        aStmt->mSemiAsyncPrefetch = NULL;
    }
    else
    {
        /* nothing to do */
    }

    /* PROJ-2646 New shard analyzer */
    ulnShardStmtFreeAllShardValueInfo( aStmt );

    aStmt->mShardStmtCxt.mShardValueCnt     = 0;
    aStmt->mShardStmtCxt.mShardIsShardQuery = ACP_FALSE;
    aStmt->mShardStmtCxt.mShardCoordQuery   = ACP_FALSE;
    aStmt->mShardStmtCxt.mNodeDbcIndexCount = 0;
    aStmt->mShardStmtCxt.mNodeDbcIndexCur   = -1;

    /* BUG-46100 Session SMN Update */
    aStmt->mShardStmtCxt.mShardMetaNumber  = 0;

    /* PROJ-2655 Composite shard key */
    aStmt->mShardStmtCxt.mShardSubValueCnt = 0;
    aStmt->mShardStmtCxt.mShardIsSubKeyExists = ACP_FALSE;

    /* PROJ-2739 Client-side Sharding LOB */
    aStmt->mShardStmtCxt.mHasLocatorInBoundParam = ACP_FALSE;
    aStmt->mShardStmtCxt.mHasLocatorParamToCopy  = ACP_FALSE;

    if (aStmt->mDeferredQstr != NULL)
    {
        acpMemFree(aStmt->mDeferredQstr);

        aStmt->mDeferredQstr       = NULL;
        aStmt->mDeferredQstrMaxLen = 0;
        aStmt->mDeferredQstrLen    = 0;
    }
    
    // PROJ-2727
    if (aStmt->mAttributeStr != NULL)
    {
        acpMemFree(aStmt->mAttributeStr);

        aStmt->mAttributeID   = 0;
        aStmt->mAttributeLen  = 0;
        aStmt->mAttributeStr  = NULL;
    }
    
#ifdef COMPILE_SHARDCLI
    /* PROJ-2658 altibase sharding */
    ulsdModuleStmtDestroy(aStmt);
#endif /* COMPILE_SHARDCLI */

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnStmtInitialize(ulnStmt *aStmt)
{
    aStmt->mOrigArd                = NULL;
    aStmt->mOrigApd                = NULL;
    aStmt->mIsPrepared             = ACP_FALSE;

    // PROJ-1697
    aStmt->mMaxBindDataSize        = 0;
    aStmt->mInOutType              = ULN_PARAM_INOUT_TYPE_INIT;
    aStmt->mInOutTypeWithoutBLOB   = ULN_PARAM_INOUT_TYPE_INIT;
    aStmt->mBuildBindInfo          = ACP_TRUE;

    // BUG-17592
    aStmt->mResultType             = ULN_RESULT_TYPE_UNKNOWN;
    aStmt->mStatementID            = 0;
    aStmt->mStatementType          = 0;

    aStmt->mHasMemBoundLobParam    = ACP_FALSE;
    aStmt->mHasMemBoundLobColumn   = ACP_FALSE;

    aStmt->mAttrAsyncEnable        = SQL_ASYNC_ENABLE_OFF;

    aStmt->mAttrEnableAutoIPD      = SQL_FALSE;

    aStmt->mAttrNoscan             = SQL_NOSCAN_OFF;

    aStmt->mAttrQueryTimeout       = 0;
    aStmt->mAttrRetrieveData       = SQL_RD_ON;

    aStmt->mAttrUseBookMarks       = SQL_UB_OFF;
    aStmt->mAttrFetchBookmarkPtr   = NULL;

    aStmt->mAttrRowsetSize         = 1;

    aStmt->mTotalAffectedRowCount  = ACP_UINT64_LITERAL(0);
    // PROJ-1518
    aStmt->mAtomicArray            = SQL_FALSE;

    aStmt->mFetchedRowCountFromServer = 0;
    aStmt->mFetchedBytesFromServer = 0; /* PROJ-2625 */

    aStmt->mParamCount             = 0;
    aStmt->mResultSetCount         = 0;
    aStmt->mCurrentResultSetID     = 0;

    /* ������ getdata() �� �÷��� ��ȣ�� ����� �־� �� */
    aStmt->mGDColumnNumber         = ULN_GD_COLUMN_NUMBER_INIT_VALUE;

    ulnStmtResetPD(aStmt);

    aStmt->mPlanTree               = NULL;

    aStmt->mCurrentResultRowNumber = 0;
    aStmt->mCurrentResult          = NULL;
    acpListInit(&aStmt->mResultList);
    acpListInit(&aStmt->mResultFreeList);

    /*
     * SES �� -n �ɼ��� ���� �Ӽ���. �� �Ӽ��� ACP_FALSE �� �Ǿ� ������
     * SQLBindParameter() ���� indicator �� NULL �̶� SQL_NTS �� �������� �ʴ´�.
     * default �� ACP_TRUE �� �� �־� �Ϲ����� ��� ����� �����Ѵ�.
     */
    aStmt->mAttrInputNTS           = ACP_TRUE;

    aStmt->mCache                  = NULL;

    aStmt->mAttrPrefetchRows       = ULN_CACHE_OPTIMAL_PREFETCH_ROWS;
    aStmt->mAttrPrefetchBlocks     = 0;
    aStmt->mAttrPrefetchMemory     = 0;
    aStmt->mAttrCacheBlockSizeThreshold = ULN_CACHE_ROW_BLOCK_SIZE_THRESHOLD;

    ulnCursorInitialize(&aStmt->mCursor, aStmt);

    /* PROJ-2177 User Interface - Cancel */
    aStmt->mStmtCID                = ULN_STMT_CID_NONE;
    aStmt->mLastFetchFuncID        = ULN_FID_NONE;
    aStmt->mNeedDataFuncID         = ULN_FID_NONE;

    /* PROJ-1789 Updatable Scrollable Cursor */
    aStmt->mRowsetStmt             = SQL_NULL_HSTMT;
    aStmt->mParentStmt             = SQL_NULL_HSTMT;
    aStmt->mKeyset                 = NULL;
    aStmt->mPrepareTextBuf         = NULL;
    aStmt->mPrepareTextBufMaxLen   = 0;
    aStmt->mQstrForRowset          = NULL;
    aStmt->mQstrForRowsetMaxLen    = 0;
    aStmt->mQstrForRowsetLen       = 0;
    aStmt->mQstrForRowsetParamPos  = 0;
    aStmt->mQstrForRowsetParamCnt  = 0;
    aStmt->mQstrForInsUpd          = NULL;
    aStmt->mQstrForInsUpdMaxLen    = 0;
    aStmt->mQstrForInsUpdLen       = 0;
    aStmt->mQstrForDelete          = NULL;
    aStmt->mQstrForDeleteMaxLen    = 0;
    aStmt->mQstrForDeleteLen       = 0;
    aStmt->mIrd4KeysetDriven       = NULL;
    aStmt->mRowsetParamBuf         = NULL;
    aStmt->mRowsetParamBufMaxLen   = 0;
    aStmt->mRowsetParamStatusBuf   = NULL;
    aStmt->mRowsetParamStatusBufMaxCnt = 0;
    aStmt->mColumnIgnoreFlagsBuf   = NULL;
    aStmt->mColumnIgnoreFlagsBufMaxCnt = 0;
    aStmt->mTableNameForUpdate[0]  = '\0';
    aStmt->mTableNameForUpdateLen  = 0;

    /* PROJ-1721 Name-based Binding */
    aStmt->mAnalyzeStmt = NULL;

   /* PROJ-1891 Deferred Prepare */
    aStmt->mAttrDeferredPrepare      = ULN_CONN_DEFERRED_PREPARE_DEFAULT;
    aStmt->mDeferredPrepareStateFunc = NULL;
    // bug-35198: rowset (row array) size�� ���ϸ�, �ѹ��� ������
    aStmt->mPrevRowSetSize           = 0;

    aStmt->mExecutedParamSetMaxSize  = 0;  /* BUG-42096 */

    /* PROJ-2616 */
    aStmt->mIsSimpleQuery                          = ACP_FALSE;
    aStmt->mIsSimpleQuerySelectExecuted            = ACP_FALSE;
    aStmt->mCacheIPCDA.mRemainingLength            = 0;
    aStmt->mCacheIPCDA.mReadLength                 = 0;
    aStmt->mCacheIPCDA.mIsParamDataInList          = ACP_FALSE;
    aStmt->mCacheIPCDA.mIsAllocFetchBuffer         = ACP_FALSE;
    aStmt->mCacheIPCDA.mFetchBuffer                = NULL;

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    aStmt->mAttrPrefetchAsync          = ULN_STMT_PREFETCH_ASYNC_DEFAULT;
    aStmt->mAttrPrefetchAutoTuning     = ULN_STMT_PREFETCH_AUTO_TUNING_DEFAULT;
    aStmt->mAttrPrefetchAutoTuningInit = ULN_CACHE_OPTIMAL_PREFETCH_ROWS;
    aStmt->mAttrPrefetchAutoTuningMin  = ULN_STMT_PREFETCH_AUTO_TUNING_MIN_DEFAULT;
    aStmt->mAttrPrefetchAutoTuningMax  = ULN_STMT_PREFETCH_AUTO_TUNING_MAX_DEFAULT;

    aStmt->mSemiAsyncPrefetch = NULL;

    aStmt->mAttrPrepareWithDescribeParam = ACP_FALSE;  /* BUG-44858 */

    /* BUG-44957 */
    aStmt->mAttrParamsRowCountsPtr = NULL;
    aStmt->mAttrParamsSetRowCounts = SQL_ROW_COUNTS_OFF;

    aStmt->mDeferredQstr       = NULL;
    aStmt->mDeferredQstrMaxLen = 0;
    aStmt->mDeferredQstrLen    = 0;

    // PROJ-2727
    aStmt->mAttributeID   = CMP_DB_PROPERTY_MAX;
    aStmt->mAttributeLen  = 0;
    aStmt->mAttributeStr  = NULL;
    
    ulnShardStmtContextInitialize(aStmt);

    return ACI_SUCCESS;
}




/*
 * ========================================================
 * Set / Get APD, ARD, IPD, IRD
 * ========================================================
 */

ACI_RC ulnStmtSetApd(ulnStmt *aStmt, ulnDesc *aDesc)
{
    if (aStmt->mOrigApd == NULL)
    {
        /*
         * �� ���� ���õ� Explicit Descriptor �� ����.
         * Implicit Descriptor �� ����� ����.
         */
        aStmt->mOrigApd = aStmt->mAttrApd;
    }
    else
    {
        /*
         * �� ���� ���õ� Explicit Descriptor �� �ִ�.
         * �� ���� Explicit Descriptor �� mAssociatedStmtList ���� STMT �� ������ ��� �Ѵ�.
         */
        ulnDescRemoveStmtFromAssociatedStmtList(aStmt, aStmt->mAttrApd);
    }

    aStmt->mAttrApd = aDesc;

    return ACI_SUCCESS;
}

ulnDesc *ulnStmtGetApd(ulnStmt *aStmt)
{
    return aStmt->mAttrApd;
}

ACI_RC ulnStmtSetArd(ulnStmt *aStmt, ulnDesc *aDesc)
{
    if (aStmt->mOrigArd == NULL)
    {
        /*
         * �� ���� ���õ� Explicit Descriptor �� ����.
         * Implicit Descriptor �� ����� ����.
         */
        aStmt->mOrigArd = aStmt->mAttrArd;
    }
    else
    {
        /*
         * �� ���� ���õ� Explicit Descriptor �� �ִ�.
         * �� ���� Explicit Descriptor �� mAssociatedStmtList ���� STMT �� ������ ��� �Ѵ�.
         */
        ulnDescRemoveStmtFromAssociatedStmtList(aStmt, aStmt->mAttrArd);
    }

    aStmt->mAttrArd = aDesc;

    return ACI_SUCCESS;
}

ulnDesc *ulnStmtGetArd(ulnStmt *aStmt)
{
    return aStmt->mAttrArd;
}

ACI_RC ulnStmtSetIpd(ulnStmt *aStmt, ulnDesc *aDesc)
{
    aStmt->mAttrIpd = aDesc;
    return ACI_SUCCESS;
}

ulnDesc *ulnStmtGetIpd(ulnStmt *aStmt)
{
    return aStmt->mAttrIpd;
}

ulnDesc *ulnStmtGetIrd(ulnStmt *aStmt)
{
    return aStmt->mAttrIrd;
}

ACI_RC ulnStmtSetIrd(ulnStmt *aStmt, ulnDesc *aDesc)
{
    aStmt->mAttrIrd = aDesc;
    return ACI_SUCCESS;
}


/*
 * ==============================================
 * Recovering Original Descriptor
 * ==============================================
 */

ACI_RC ulnRecoverOriginalApd(ulnStmt *aStmt)
{
    ACE_ASSERT(aStmt != NULL);

    aStmt->mAttrApd = aStmt->mOrigApd;
    aStmt->mOrigApd = NULL;

    return ACI_SUCCESS;
}

ACI_RC ulnRecoverOriginalArd(ulnStmt *aStmt)
{
    ACE_ASSERT(aStmt != NULL);

    aStmt->mAttrArd = aStmt->mOrigArd;
    aStmt->mOrigArd = NULL;

    return ACI_SUCCESS;
}

acp_uint16_t ulnStmtGetColumnCount(ulnStmt *aStmt)
{
    acp_uint16_t sDescRecCount;

    /*
     * IRD �� �ִ� column count �̴�.
     */
    if (ulnStmtGetAttrCursorType(aStmt) == SQL_CURSOR_KEYSET_DRIVEN)
    {
        /* _PROWID�� Count���� �����Ѵ�. */
        sDescRecCount = aStmt->mIrd4KeysetDriven->mDescRecCount - 1;
    }
    else
    {
        sDescRecCount = aStmt->mAttrIrd->mDescRecCount;
        if (ulnStmtGetIrdRec(aStmt, 0) != NULL)
        {
            /* BOOKMARK �÷��� column count�� �������� �ʴ´�. */
            sDescRecCount--;
        }
    }

    return sDescRecCount;
}

ulnDescRec *ulnStmtGetIrdRec(ulnStmt *aStmt, acp_uint16_t aColumnNumber)
{
    ulnDescRec *sDescRec;

    if (ulnStmtGetAttrCursorType(aStmt) == SQL_CURSOR_KEYSET_DRIVEN)
    {
        /* _PROWID�� ���ԵǾ������Ƿ� 2���� ��´�. */
        sDescRec = ulnDescGetDescRec(aStmt->mIrd4KeysetDriven, aColumnNumber + 1);
    }
    else
    {
        sDescRec = ulnDescGetDescRec(aStmt->mAttrIrd, aColumnNumber);
    }

    return sDescRec;
}

void ulnStmtSetAttrParamBindOffsetPtr(ulnStmt *aStmt, ulvULen *aOffsetPtr)
{
    /*
     * APD �� SQL_DESC_BIND_OFFSET_PTR �� ����
     */
    ulnDescSetBindOffsetPtr(aStmt->mAttrApd, aOffsetPtr);
}

ulvULen *ulnStmtGetAttrParamBindOffsetPtr(ulnStmt *aStmt)
{
    /*
     * APD �� SQL_DESC_BIND_OFFSET_PTR �� ����
     */
    return ulnDescGetBindOffsetPtr(aStmt->mAttrApd);
}

ulvULen ulnStmtGetParamBindOffsetValue(ulnStmt *aStmt)
{
    ulvULen *sOffsetPtr;

    /*
     * ���� ����ڰ� Bind Offset �� �����Ͽ��ٸ� �� ���� ���´�.
     */
    sOffsetPtr = ulnStmtGetAttrParamBindOffsetPtr(aStmt);

    if (sOffsetPtr != NULL)
    {
        return *sOffsetPtr;
    }
    else
    {
        return 0;
    }
}

void ulnStmtSetAttrRowBindOffsetPtr(ulnStmt *aStmt, ulvULen *aOffsetPtr)
{
    /*
     * ARD �� SQL_DESC_BIND_OFFSET_PTR �� ����
     */
    ulnDescSetBindOffsetPtr(aStmt->mAttrArd, aOffsetPtr);
}

ulvULen *ulnStmtGetAttrRowBindOffsetPtr(ulnStmt *aStmt)
{
    /*
     * ARD �� SQL_DESC_BIND_OFFSET_PTR �� ����
     */
    return ulnDescGetBindOffsetPtr(aStmt->mAttrArd);
}

ulvULen ulnStmtGetRowBindOffsetValue(ulnStmt *aStmt)
{
    ulvULen *sOffsetPtr;

    /*
     * ���� ����ڰ� Bind Offset �� �����Ͽ��ٸ� �� ���� ���´�.
     */
    sOffsetPtr = ulnStmtGetAttrRowBindOffsetPtr(aStmt);

    if (sOffsetPtr != NULL)
    {
        return *sOffsetPtr;
    }
    else
    {
        return 0;
    }
}

acp_uint32_t ulnStmtGetAttrParamBindType(ulnStmt *aStmt)
{
    /*
     * APD �� SQL_DESC_BIND_TYPE ���� ����
     */
    return ulnDescGetBindType(aStmt->mAttrApd);
}

void ulnStmtSetAttrParamBindType(ulnStmt *aStmt, acp_uint32_t aType)
{
    /*
     * APD �� SQL_DESC_BIND_TYPE ���� ����
     */
    ulnDescSetBindType(aStmt->mAttrApd, aType);
}

acp_uint16_t ulnStmtGetAttrParamOperationValue(ulnStmt *aStmt, acp_uint32_t aRow)
{
    acp_uint16_t *sOperationPtr;

    sOperationPtr = ulnStmtGetAttrParamOperationPtr(aStmt);

    if (sOperationPtr == NULL)
    {
        return SQL_PARAM_PROCEED;
    }
    else
    {
        return *(sOperationPtr + aRow);
    }
}

acp_uint16_t *ulnStmtGetAttrParamOperationPtr(ulnStmt *aStmt)
{
    /*
     * APD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    return ulnDescGetArrayStatusPtr(aStmt->mAttrApd);
}

void ulnStmtSetAttrParamOperationPtr(ulnStmt *aStmt, acp_uint16_t *aOperationPtr)
{
    /*
     * APD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    ulnDescSetArrayStatusPtr(aStmt->mAttrApd, aOperationPtr);
}

void ulnStmtSetAttrParamStatusValue(ulnStmt *aStmt, acp_uint32_t aRow, acp_uint16_t aValue)
{
    acp_uint16_t *sStatusPtr;

    sStatusPtr = ulnStmtGetAttrParamStatusPtr(aStmt);

    if (sStatusPtr != NULL)
    {
        *(sStatusPtr + aRow) = aValue;
    }
}

acp_uint16_t *ulnStmtGetAttrParamStatusPtr(ulnStmt *aStmt)
{
    return ulnDescGetArrayStatusPtr(aStmt->mAttrIpd);
}

void ulnStmtSetAttrParamStatusPtr(ulnStmt *aStmt, acp_uint16_t *aStatusPtr)
{
    ulnDescSetArrayStatusPtr(aStmt->mAttrIpd, aStatusPtr);
}

void ulnStmtSetAttrParamsProcessedPtr(ulnStmt *aStmt, ulvULen *aProcessedPtr)
{
    /*
     * IPD �� SQL_DESC_ROWS_PROCESSED_PTR
     */
    ulnDescSetRowsProcessedPtr(aStmt->mAttrIpd, aProcessedPtr);
}

ulvULen *ulnStmtGetAttrParamsProcessedPtr(ulnStmt *aStmt)
{
    /*
     * IPD �� SQL_DESC_ROWS_PROCESSED_PTR
     */
    return ulnDescGetRowsProcessedPtr(aStmt->mAttrIpd);
}

void ulnStmtSetParamsProcessedValue(ulnStmt *aStmt, ulvULen aValue)
{
    ulnDescSetRowsProcessedPtrValue(aStmt->mAttrIpd, aValue);
}

ACI_RC ulnStmtIncreaseParamsProcessedValue(ulnStmt *aStmt)
{
    ulvULen *sParamsProcessedPtr;

    sParamsProcessedPtr = ulnStmtGetAttrParamsProcessedPtr(aStmt);

    if (sParamsProcessedPtr != NULL)
    {
        *(sParamsProcessedPtr) = *(sParamsProcessedPtr) + 1;
    }

    return ACI_SUCCESS;
}

/*
 * ulnStmt[Set|Get]AttrConcurrency.
 *
 * SQL_ATTR_CONCURRENCY statement attribute �� �����ϰų� ������ �Լ�.
 */
void ulnStmtSetAttrConcurrency(ulnStmt *aStmt, acp_uint32_t aConcurrency)
{
    ulnCursorSetConcurrency(&aStmt->mCursor, aConcurrency);
}

acp_uint32_t ulnStmtGetAttrConcurrency(ulnStmt *aStmt)
{
    return ulnCursorGetConcurrency(&aStmt->mCursor);
}

/*
 * ulnStmt[Set|Get]AttrCursorScrollable.
 *
 * SQL_ATTR_CURSOR_SCROLLABLE statement attr �� �����ϰų� ������ �Լ�.
 */
acp_uint32_t ulnStmtGetAttrCursorScrollable(ulnStmt *aStmt)
{
    return ulnCursorGetScrollable(&aStmt->mCursor);
}

ACI_RC ulnStmtSetAttrCursorScrollable(ulnStmt *aStmt, acp_uint32_t aScrollable)
{
    ulnCursorSetScrollable(&aStmt->mCursor, aScrollable);

    return ACI_SUCCESS;
}

/*
 * ulnStmt[Get|Set]AttrCursorSensitivity.
 *
 * SQL_ATTR_CURSOR_SENSITIVITY statement attr �� �����ϰų� ������ �Լ�.
 *
 * Note
 *  ����ν�� ������ SQL_INSENSITIVE �� �����Ѵ�.
 *  ���� �ٸ� ���� ��Ƽ���� �ʵ��� �����ؾ� �ϴ�.
 */
acp_uint32_t ulnStmtGetAttrCursorSensitivity(ulnStmt *aStmt)
{
    return ulnCursorGetSensitivity(&aStmt->mCursor);
}

ACI_RC ulnStmtSetAttrCursorSensitivity(ulnStmt *aStmt, acp_uint32_t aSensitivity)
{
    ACI_TEST(aSensitivity == SQL_UNSPECIFIED);

    /*
     * BUGBUG : ����Ʈ ���� SQL_UNSPECIFIED ������, ���� ul �� cm �� mm ����
     *          sensitive �� Ŀ���� �������� ���ϱ� ������ INSENSITIVE �� �⺻���� �ߴ�
     *
     *          ����.. INSENSITIVE �� �´���, �ƴϸ� UNSPECIFIED �� �´��� �𸣰ڴ�.
     */
    ulnCursorSetSensitivity(&aStmt->mCursor, aSensitivity);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnStmt[Get|Set]AttrCursorType.
 *
 * SQL_ATTR_CURSOR_TYPE statement attr �� �����ϰų� ������ �Լ�.
 */
acp_uint32_t ulnStmtGetAttrCursorType(ulnStmt *aStmt)
{
    return ulnCursorGetType(&aStmt->mCursor);
}

void ulnStmtSetAttrCursorType(ulnStmt *aStmt, acp_uint32_t aType)
{
    ulnCursorSetType(&aStmt->mCursor, aType);
}

/*
 * ulnStmt[Get|Set]AttrParamsetSize.
 *
 * SQL_ATTR_PARAMSET_SIZE statement attr �� �����ϰų� ������ �Լ�.
 * �� �Ӽ��� Param �� Array Binding �ÿ� Array �� �ִ� element �� �����̴�.
 */
acp_uint32_t ulnStmtGetAttrParamsetSize(ulnStmt *aStmt)
{
    /*
     * APD �� SQL_DESC_ARRAY_SIZE �� ����
     */
    return ulnDescGetArraySize(aStmt->mAttrApd);
}

void ulnStmtSetAttrParamsetSize(ulnStmt *aStmt, acp_uint32_t aSize)
{
    /*
     * APD �� SQL_DESC_ARRAY_SIZE �� ����
     */
    ulnDescSetArraySize(aStmt->mAttrApd, aSize);
}

/*
 * ulnStmt[Set|Get]AttrQueryTimeout.
 *
 * �� �Լ����� ȣ���Ͽ� �ش� �Ӽ��� �����ϴ� �Լ�����
 * Datasource �� max/min timeout �� �����ؾ� �Ѵ�.
 */
ACI_RC ulnStmtSetAttrQueryTimeout(ulnStmt *aStmt, acp_uint32_t aTimeout)
{
    aStmt->mAttrQueryTimeout = aTimeout;
    return ACI_SUCCESS;
}

ACI_RC ulnStmtGetAttrQueryTimeout(ulnStmt *aStmt, acp_uint32_t *aTimeout)
{
    *aTimeout = aStmt->mAttrQueryTimeout;
    return ACI_SUCCESS;
}

ACI_RC ulnStmtGetAttrRetrieveData(ulnStmt *aStmt, acp_uint32_t *aRetrieve)
{
    *aRetrieve = aStmt->mAttrRetrieveData;
    return ACI_SUCCESS;
}

ACI_RC ulnStmtSetAttrRetrieveData(ulnStmt *aStmt, acp_uint32_t aRetrieve)
{
    aStmt->mAttrRetrieveData = aRetrieve;
    return ACI_SUCCESS;
}

/*
 * ulnStmt[Set|Get]AttrRowBindType.
 */
acp_uint32_t ulnStmtGetAttrRowBindType(ulnStmt *aStmt)
{
    /*
     * ARD �� SQL_DESC_BIND_TYPE ���� ����
     */
    return ulnDescGetBindType(aStmt->mAttrArd);
}

void ulnStmtSetAttrRowBindType(ulnStmt *aStmt, acp_uint32_t aType)
{
    /*
     * ARD �� SQL_DESC_BIND_TYPE ���� ����
     */
    ulnDescSetBindType(aStmt->mAttrArd, aType);
}

void ulnStmtSetAttrRowOperationPtr(ulnStmt *aStmt, acp_uint16_t *aOperationPtr)
{
    /*
     * ARD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    ulnDescSetArrayStatusPtr(aStmt->mAttrArd, aOperationPtr);
}

acp_uint16_t *ulnStmtGetAttrRowOperationPtr(ulnStmt *aStmt)
{
    /*
     * ARD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    return ulnDescGetArrayStatusPtr(aStmt->mAttrArd);
}

void ulnStmtInitRowStatusArrayValue(ulnStmt      *aStmt,
                                    acp_uint32_t  aStartIndex,
                                    acp_uint32_t  aArraySize,
                                    acp_uint16_t  aValue)
{
    ulnDescInitStatusArrayValues(aStmt->mAttrIrd,
                                 aStartIndex,
                                 aArraySize,
                                 aValue);
}

void ulnStmtSetAttrRowStatusValue(ulnStmt *aStmt, acp_uint32_t aRow, acp_uint16_t aValue)
{
    acp_uint16_t *sStatusPtr;

    sStatusPtr = ulnStmtGetAttrRowStatusPtr(aStmt);

    if (sStatusPtr != NULL)
    {
        if (aStmt->mObj.mExecFuncID == ULN_FID_EXTENDEDFETCH)
        {
            switch(aValue)
            {
                case SQL_ROW_SUCCESS_WITH_INFO:
                    aValue = SQL_ROW_SUCCESS;
                    break;

                case SQL_ROW_ERROR:
                    aValue = SQL_ROW_SUCCESS_WITH_INFO;
                    break;

                default:
                    break;
            }
        }

        *(sStatusPtr + aRow) = aValue;
    }
}

void ulnStmtSetAttrRowStatusPtr(ulnStmt *aStmt, acp_uint16_t *aStatusPtr)
{
    /*
     * IRD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    ulnDescSetArrayStatusPtr(aStmt->mAttrIrd, aStatusPtr);
}

acp_uint16_t ulnStmtGetAttrRowStatusValue(ulnStmt *aStmt, acp_uint32_t aRow)
{
    acp_uint16_t *sStatusPtr;

    sStatusPtr = ulnStmtGetAttrRowStatusPtr(aStmt);

    if (sStatusPtr != NULL)
    {
        return *(sStatusPtr + aRow);
    }
    else
    {
        return 0;
    }
}

acp_uint16_t *ulnStmtGetAttrRowStatusPtr(ulnStmt *aStmt)
{
    /*
     * IRD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    return ulnDescGetArrayStatusPtr(aStmt->mAttrIrd);
}

void ulnStmtSetRowsFetchedValue(ulnStmt *aStmt, ulvULen aValue)
{
    ulnDescSetRowsProcessedPtrValue(aStmt->mAttrIrd, aValue);
}

ulvULen *ulnStmtGetAttrRowsFetchedPtr(ulnStmt *aStmt)
{
    /*
     * IRD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    return ulnDescGetRowsProcessedPtr(aStmt->mAttrIrd);
}

void ulnStmtSetAttrRowsFetchedPtr(ulnStmt *aStmt, ulvULen *aFetchedPtr)
{
    /*
     * IRD �� SQL_DESC_ARRAY_STATUS_PTR �� ���ε�.
     */
    ulnDescSetRowsProcessedPtr(aStmt->mAttrIrd, aFetchedPtr);
}

acp_bool_t ulnStmtIsCursorOpen(ulnStmt *aStmt)
{
    if (ulnCursorGetState(&aStmt->mCursor) == ULN_CURSOR_STATE_OPEN)
    {
        return ACP_TRUE;
    }
    else
    {
        return ACP_FALSE;
    }
}

/*
 * stmt �� ���� �ִ� ��� ARD, APD ���� ���󰡸鼭 BINDINFO SENT FLAG �� Ŭ���� ���� �ش�.
 * �������� prepare �� �ϸ� ������ ���ε� �������� �� ������������ �ٲ�鼭 �ʿ��ϰ� �Ǿ���.
 */
void ulnStmtClearBindInfoSentFlagAll(ulnStmt *aStmt)
{
    acp_list_node_t *sIterator;

    ACP_LIST_ITERATE(&aStmt->mAttrArd->mDescRecList, sIterator)
    {
        ulnBindInfoSetType(&(((ulnDescRec *)sIterator)->mBindInfo), ULN_MTYPE_MAX);
    }

    ACP_LIST_ITERATE(&aStmt->mAttrApd->mDescRecList, sIterator)
    {
        ulnBindInfoSetType(&(((ulnDescRec *)sIterator)->mBindInfo), ULN_MTYPE_MAX);
    }
}

ACI_RC ulnStmtAllocPlanTree(ulnStmt *aStmt, acp_uint32_t aSize)
{
    if (aStmt->mPlanTree != NULL)
    {
        acpMemFree(aStmt->mPlanTree);
    }

    ACI_TEST(acpMemAlloc((void**)&aStmt->mPlanTree, aSize) != ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnStmtFreePlanTree(ulnStmt *aStmt)
{
    acpMemFree(aStmt->mPlanTree);

    aStmt->mPlanTree = NULL;

    return ACI_SUCCESS;
}

/*
 * ===============================================
 * BOOKMARKS
 * ===============================================
 */

void ulnStmtSetAttrUseBookMarks(ulnStmt *aStmt, acp_uint32_t mUseBookMarks)
{
    aStmt->mAttrUseBookMarks = mUseBookMarks;
}

acp_uint32_t ulnStmtGetAttrUseBookMarks(ulnStmt *aStmt)
{
    return aStmt->mAttrUseBookMarks;
}

ACI_RC ulnStmtCreateCache(ulnStmt *aStmt)
{
    ulnCache *sCache;

    ACI_TEST(ulnCacheCreate(&sCache) != ACI_SUCCESS);

    sCache->mParentStmt = aStmt;
    aStmt->mCache       = sCache;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_uint32_t ulnStmtGetAttrRowBlockSizeThreshold(ulnStmt *aStmt)
{
    return aStmt->mAttrCacheBlockSizeThreshold;
}

/*
 * ---------------------------------------------
 *  Result handling routines
 * ---------------------------------------------
 */

ulnResult *ulnResultCreate(ulnStmt *aParentStmt)
{
    ulnResult *sResult = NULL;
    uluMemory *sMemory = aParentStmt->mObj.mMemory;

    ACI_TEST(sMemory->mOp->mMalloc(sMemory,
                                   (void **)&sResult,
                                   ACI_SIZEOF(ulnResult)) != ACI_SUCCESS);

    acpListInitObj(&sResult->mList, sResult);

    sResult->mType              = ULN_RESULT_TYPE_UNKNOWN;
    sResult->mAffectedRowCount  = ULN_DEFAULT_ROWCOUNT;
    sResult->mFromRowNumber     = 0;
    sResult->mToRowNumber       = 0;

    return sResult;

    ACI_EXCEPTION_END;

    return NULL;
}

acp_bool_t ulnStmtIsLastResult(ulnStmt *aStmt)
{
    if (aStmt->mCurrentResult == NULL)
    {
        return ACP_TRUE;
    }

    if (aStmt->mCurrentResult->mList.mNext == &aStmt->mResultList)
    {
        if( aStmt->mCurrentResultRowNumber >= aStmt->mCurrentResult->mToRowNumber )
        {
            if (aStmt->mResultSetCount == 0 ||
                aStmt->mCurrentResultSetID == aStmt->mResultSetCount)
            {
                return ACP_TRUE;
            }
        }
    }

    return ACP_FALSE;
}

ulnResult *ulnStmtGetNextResult(ulnStmt *aStmt)
{
    ulnResult *sNextResult = NULL;
    ulnResult *sCurrResult = NULL;

    sCurrResult = aStmt->mCurrentResult;

    if (sCurrResult == NULL)
    {
        if (acpListIsEmpty(&aStmt->mResultList) == ACP_TRUE)
        {
            sNextResult = NULL;
        }
        else
        {
            sNextResult = (ulnResult *)(aStmt->mResultList.mNext);
            aStmt->mCurrentResultRowNumber = sNextResult->mFromRowNumber;
            aStmt->mCurrentResultRowNumber++;
        }
    }
    else
    {
        if (acpListIsEmpty(&aStmt->mResultList) == ACP_TRUE)
        {
            sNextResult = NULL;
        }
        else
        {
            if( aStmt->mCurrentResultRowNumber > sCurrResult->mToRowNumber )
            {
                if( ((acp_list_t *)sCurrResult)->mNext == &aStmt->mResultList )
                {
                    /*
                     * ������ result
                     */
                    sNextResult = NULL;
                }
                else
                {
                    sNextResult = (ulnResult *)(((acp_list_t *)sCurrResult)->mNext);
                    aStmt->mCurrentResultRowNumber = sNextResult->mFromRowNumber;
                    aStmt->mCurrentResultRowNumber++;
                }
            }
            else
            {
                aStmt->mCurrentResultRowNumber++;
                sNextResult = sCurrResult;
            }
        }
    }

    return sNextResult;
}

/*
 * =======================================================
 * Chunk Handling Routines ( PROJ-1697 )
 * =======================================================
 */

#if 0
void ulnStmtChunkAppendParamInfo( ulnStmt      *aStmt,
                                  void         *aBindInfo,
                                  acp_uint32_t  aParamNumber)
{
    cmpCollectionDBParamInfoSet  sInfoSet;
    ulnBindInfo                     *sBindInfo;

    sBindInfo = (ulnBindInfo*)aBindInfo;

    sInfoSet.mParamNumber = aParamNumber;
    sInfoSet.mDataType    = ulnTypeMap_MTYPE_MTD(ulnBindInfoGetType(sBindInfo));
    sInfoSet.mLanguage    = ulnBindInfoGetLanguage(sBindInfo);
    sInfoSet.mArguments   = ulnBindInfoGetArguments(sBindInfo);
    sInfoSet.mPrecision   = ulnBindInfoGetPrecision(sBindInfo);
    sInfoSet.mScale       = ulnBindInfoGetScale(sBindInfo);
    sInfoSet.mInOutType   = ulnBindInfoCmParamInOutType(sBindInfo);

    cmtCollectionWriteParamInfoSet( aStmt->mChunk.mData,
                                    &aStmt->mChunk.mCursor,
                                    &sInfoSet );
}
#endif

static const acp_uint8_t gUlnParamTypeMap[ULN_PARAM_INOUT_TYPE_MAX] =
{
    99,
    CMP_DB_PARAM_INPUT,
    CMP_DB_PARAM_OUTPUT,
    CMP_DB_PARAM_INPUT_OUTPUT
};

acp_uint8_t ulnStmtGetCmParamInOutType( ulnParamInOutType aType )
{
    return gUlnParamTypeMap[aType];
}

ACI_RC ulnStmtAllocChunk( ulnStmt       *aStmt,
                          acp_uint32_t   aSize,
                          acp_uint8_t  **aData)
{
    uluMemory *sMemory = aStmt->mObj.mMemory;

    if( aSize > aStmt->mChunk.mSize )
    {
        ACI_TEST(sMemory->mOp->mMalloc(sMemory,
                                       (void **)&aStmt->mChunk.mData,
                                       aSize)
                 != ACI_SUCCESS);

        aStmt->mChunk.mSize = aSize;  /* BUG-42096 */
    }

    *aData = aStmt->mChunk.mData;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

//PROJ-1645 UL-FailOver.
void ulnStmtSetPrepared(ulnStmt    *aStmt,
                        acp_bool_t  aIsPrepared)
{
    aStmt->mIsPrepared = aIsPrepared;

}

/* PROJ-1381 Fetch Across Commit */

/**
 * Prepare Mode�� ��´�.
 *
 * @param[in] aStmt     statement handle
 * @param[in] aExecMode Execute Mode
 *
 * @return Prepare Mode
 */
acp_uint8_t ulnStmtGetPrepareMode(ulnStmt *aStmt, acp_uint8_t aExecMode)
{
    acp_uint8_t sMode = aExecMode;

    if (ulnCursorGetHoldability(&aStmt->mCursor) == SQL_CURSOR_HOLD_ON)
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_HOLD_MASK)
              | CMP_DB_PREPARE_MODE_HOLD_ON;
    }
    else
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_HOLD_MASK)
              | CMP_DB_PREPARE_MODE_HOLD_OFF;
    }

    /* PROJ-1789 Updatable Scrollable Cursor */
    if (ulnStmtGetAttrCursorType(aStmt) == SQL_CURSOR_KEYSET_DRIVEN)
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_KEYSET_MASK)
              | CMP_DB_PREPARE_MODE_KEYSET_ON;
    }
    else
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_KEYSET_MASK)
              | CMP_DB_PREPARE_MODE_KEYSET_OFF;
    }

    /* TASK-7219 Non-shard DML */
    if ( ulsdStmtGetPartialExecType( aStmt ) == ULN_SHARD_PARTIAL_EXEC_TYPE_COORD )
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_MASK)
            | CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_COORD;
    }
    else if ( ulsdStmtGetPartialExecType( aStmt ) == ULN_SHARD_PARTIAL_EXEC_TYPE_QUERY )
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_MASK)
            | CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_QUERY;
    }
    else
    {
        sMode = (sMode & ~CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_MASK)
            | CMP_DB_PREPARE_MODE_SHARD_PARTIAL_EXEC_NONE;
    }

    return sMode;
}

/**
 * Cursor Hold �Ӽ��� �����Ѵ�.
 *
 * @param[in] aStmt       statement handle
 * @param[in] aCursorHold holdablility. SQL_CURSOR_HOLD_ON or SQL_CURSOR_HOLD_OFF.
 */
void ulnStmtSetAttrCursorHold(ulnStmt *aStmt, acp_uint32_t aCursorHold)
{
    ulnCursorSetHoldability(&aStmt->mCursor, aCursorHold);
}

/**
 * Cursor Hold �Ӽ��� ��´�.
 *
 * @param[in] aStmt statement handle
 *
 * @return Holdable�̸� SQL_CURSOR_HOLD_ON, �ƴϸ� SQL_CURSOR_HOLD_OFF
 */
acp_uint32_t ulnStmtGetAttrCursorHold(ulnStmt *aStmt)
{
    return ulnCursorGetHoldability(&aStmt->mCursor);
}

/**
 * FetchBookmarkPtr�� ������ �ϸ�ũ ���� ��´�.
 *
 * @param[in] aStmt statement handle
 *
 * @return FetchBookmarkPtr�� ������ �ϸ�ũ ��. ������ ���� ������ 0
 */
acp_sint64_t ulnStmtGetAttrFetchBookmarkVal(ulnStmt *aStmt)
{
    acp_sint64_t sBookmarkVal = 0;

    if (aStmt->mAttrFetchBookmarkPtr != NULL)
    {
        /* VARIABLE�� ���� �ִ밪�� 64bit signed int�� �����ϴ� ������
         * CursorPosition�� sint64�� �� �̻��� �ǹ̰� ���� ����. */
        if (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_VARIABLE)
        {
            sBookmarkVal = *((acp_sint64_t *) aStmt->mAttrFetchBookmarkPtr);
        }
        else /* is SQL_UB_FIXED (== SQL_UB_ON) */
        {
            sBookmarkVal = *((acp_uint32_t *) aStmt->mAttrFetchBookmarkPtr);
        }
    }

    return sBookmarkVal;
}

/**
 * SQL_FETCH_BOOKMARK �� �� ����� �ϸ�ũ�� ���� �����͸� ��´�.
 *
 * @param[in] aStmt statement handle
 *
 * @return SQL_FETCH_BOOKMARK �� �� ����� �ϸ�ũ�� ���� ������
 */
acp_uint8_t* ulnStmtGetAttrFetchBookmarkPtr(ulnStmt *aStmt)
{
    return aStmt->mAttrFetchBookmarkPtr;
}

/**
 * SQL_FETCH_BOOKMARK �� �� ����� �ϸ�ũ�� ���� �����͸� �����Ѵ�.
 *
 * @param[in] aStmt             statement handle
 * @param[in] aFetchBookmarkPtr SQL_FETCH_BOOKMARK �� �� ����� �ϸ�ũ�� ���� ������
 */
void ulnStmtSetAttrFetchBookmarkPtr(ulnStmt *aStmt, acp_uint8_t *aFetchBookmarkPtr)
{
    aStmt->mAttrFetchBookmarkPtr = aFetchBookmarkPtr;
}

ACI_RC ulnStmtCreateKeyset(ulnStmt *aStmt)
{
    ulnKeyset *sKeyset;

    ACI_TEST(ulnKeysetCreate(&sKeyset) != ACI_SUCCESS)

    aStmt->mKeyset       = sKeyset;
    sKeyset->mParentStmt = aStmt;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * Cache�� ���� ����� Ȯ���Ѵ�.
 *
 * @param[in] aStmt statement handle
 *
 * @return Cache�� ���� ���
 */
ulnStmtFetchMode ulnStmtGetFetchMode(ulnStmt *aStmt)
{
    return aStmt->mFetchMode;
}

/**
 * Fetch ���� ��, Cache�� � ������� ���� �������� �����Ѵ�.
 *
 * @param[in] aStmt      statement handle
 * @param[in] aFetchMode Cache�� ���� ���
 */
void ulnStmtSetFetchMode(ulnStmt *aStmt, ulnStmtFetchMode aFetchMode)
{
    aStmt->mFetchMode = aFetchMode;
}

/**
 * PrepareText�� �����س� ���۸� �Ҵ��Ѵ�.
 * ����, �̹� ����� �޸𸮰� �Ҵ�Ǿ��ִٸ� ������ �Ѿ��.
 *
 * ������ �ִ� ���� ��������.
 *
 * @param[in] aStmt   statement handle
 * @param[in] aBufLen ���� ����
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnStmtEnsureAllocPrepareTextBuf(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mPrepareTextBufMaxLen < aBufLen)
    {
        if (aStmt->mPrepareTextBuf != NULL)
        {
            acpMemFree(aStmt->mPrepareTextBuf);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mPrepareTextBuf, aBufLen) != ACP_RC_SUCCESS);
        aStmt->mPrepareTextBufMaxLen = aBufLen;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mPrepareTextBuf       = NULL;
    aStmt->mPrepareTextBufMaxLen = 0;

    return ACI_FAILURE;
}

/**
 * Rowset�� �״µ� �������� �������� �����س� ���۸� ���Ҵ��Ѵ�.
 * ����, �̹� ����� �޸𸮰� �Ҵ�Ǿ��ִٸ� ������ �Ѿ��.
 *
 * ������ �ִ� ���� �����Ѵ�.
 *
 * @param[in] aStmt   statement handle
 * @param[in] aBufLen ���� ����
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnStmtEnsureReallocQstrForRowset(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mQstrForRowsetMaxLen < aBufLen)
    {
        ACI_TEST(acpMemRealloc((void**)&aStmt->mQstrForRowset, aBufLen) != ACP_RC_SUCCESS);
        aStmt->mQstrForRowsetMaxLen = aBufLen;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnStmtEnsureAllocQstrForUpdate(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mQstrForInsUpdMaxLen < aBufLen)
    {
        if (aStmt->mQstrForInsUpd != NULL)
        {
            acpMemFree(aStmt->mQstrForInsUpd);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mQstrForInsUpd, aBufLen) != ACP_RC_SUCCESS);
        aStmt->mQstrForInsUpdMaxLen = aBufLen;
    }

    aStmt->mQstrForInsUpdLen = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mQstrForInsUpd       = NULL;
    aStmt->mQstrForInsUpdMaxLen = 0;
    aStmt->mQstrForInsUpdLen    = 0;

    return ACI_FAILURE;
}

ACI_RC ulnStmtEnsureAllocQstrForDelete(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mQstrForDeleteMaxLen < aBufLen)
    {
        if (aStmt->mQstrForDelete != NULL)
        {
            acpMemFree(aStmt->mQstrForDelete);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mQstrForDelete, aBufLen) != ACP_RC_SUCCESS);
        aStmt->mQstrForDeleteMaxLen = aBufLen;
    }

    aStmt->mQstrForDeleteLen = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mQstrForDelete       = NULL;
    aStmt->mQstrForDeleteMaxLen = 0;
    aStmt->mQstrForDeleteLen    = 0;

    return ACI_FAILURE;
}

ACI_RC ulnStmtEnsureAllocRowsetParamBuf(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mRowsetParamBufMaxLen < aBufLen)
    {
        if (aStmt->mRowsetParamBuf != NULL)
        {
            acpMemFree(aStmt->mRowsetParamBuf);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mRowsetParamBuf, aBufLen) != ACP_RC_SUCCESS);
        aStmt->mRowsetParamBufMaxLen = aBufLen;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mRowsetParamBuf       = NULL;
    aStmt->mRowsetParamBufMaxLen = 0;

    return ACI_FAILURE;
}

ACI_RC ulnStmtEnsureAllocRowsetParamStatusBuf(ulnStmt      *aStmt,
                                              acp_sint32_t  aCount)
{
    if (aStmt->mRowsetParamStatusBufMaxCnt < aCount)
    {
        if (aStmt->mRowsetParamStatusBuf != NULL)
        {
            acpMemFree(aStmt->mRowsetParamStatusBuf);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mRowsetParamStatusBuf,
                             ACI_SIZEOF(acp_uint16_t) * aCount)
                 != ACP_RC_SUCCESS);
        aStmt->mRowsetParamStatusBufMaxCnt = aCount;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mRowsetParamStatusBuf       = NULL;
    aStmt->mRowsetParamStatusBufMaxCnt = 0;

    return ACI_FAILURE;
}

ACI_RC ulnStmtEnsureAllocColumnIgnoreFlagsBuf(ulnStmt      *aStmt,
                                              acp_sint32_t  aCount)
{
    if (aStmt->mColumnIgnoreFlagsBufMaxCnt < aCount)
    {
        if (aStmt->mColumnIgnoreFlagsBuf != NULL)
        {
            acpMemFree(aStmt->mColumnIgnoreFlagsBuf);
        }

        ACI_TEST(acpMemAlloc((void**)&aStmt->mColumnIgnoreFlagsBuf,
                             ACI_SIZEOF(ulnColumnIgnoreFlag) * aCount)
                 != ACP_RC_SUCCESS);
        aStmt->mColumnIgnoreFlagsBufMaxCnt = aCount;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mColumnIgnoreFlagsBuf       = NULL;
    aStmt->mColumnIgnoreFlagsBufMaxCnt = 0;

    return ACI_FAILURE;
}

/**
 * Updatable�� ����� TableName�� �����.
 *
 * �̹� ������ٸ� ������ �Ѿ��.
 *
 * @return Updatable�� ����� TableName�� �� ��������� ACI_SUCCESS,
 *         �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnStmtBuildTableNameForUpdate(ulnStmt *aStmt)
{
    ulnDescRec  *sDescRecIrd;
    acp_char_t  *sBaseTableName;
    acp_sint32_t sBaseTableNameLen;
    acp_char_t  *sSchemaName;
    acp_sint32_t sSchemaNameLen;

    ACI_TEST_RAISE(ulnStmtIsTableNameForUpdateBuilt(aStmt) == ACP_TRUE,
                   LABEL_CONT_ALREAY_BUILD);

    sDescRecIrd = ulnStmtGetIrdRec(aStmt, 1);
    ACI_TEST(sDescRecIrd == NULL);

    sBaseTableName = ulnDescRecGetBaseTableName(sDescRecIrd);
    ACI_TEST(sBaseTableName == NULL);
    sBaseTableNameLen = acpCStrLen(sBaseTableName, ACP_SINT32_MAX);
    ACI_TEST(sBaseTableNameLen == 0);

    sSchemaName = ulnDescRecGetSchemaName(sDescRecIrd);
    if (sSchemaName == NULL)
    {
        sSchemaNameLen = 0;
    }
    else
    {
        sSchemaNameLen = acpCStrLen(sSchemaName, ACP_SINT32_MAX);
    }

    /* SchemaName�� ���� ���� �ִ�. �������, V$SESSION ���� ��?
     * �̷��� ���°� �����̹Ƿ� ������ �Ѿ���Ѵ�. */
    if (sSchemaNameLen == 0)
    {
        ACI_TEST(acpSnprintf(aStmt->mTableNameForUpdate,
                             ACI_SIZEOF(aStmt->mTableNameForUpdate),
                             "\"%s\"", sBaseTableName)
                 != ACI_SUCCESS);
        aStmt->mTableNameForUpdateLen = sBaseTableNameLen + 2;
    }
    else
    {
        ACI_TEST(acpSnprintf(aStmt->mTableNameForUpdate,
                             ACI_SIZEOF(aStmt->mTableNameForUpdate),
                             "\"%s\".\"%s\"", sSchemaName, sBaseTableName)
                 != ACI_SUCCESS);
        aStmt->mTableNameForUpdateLen = sBaseTableNameLen + sSchemaNameLen + 5;
    }

    ACI_EXCEPTION_CONT(LABEL_CONT_ALREAY_BUILD);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
ACI_RC ulnStmtSetAttrPrefetchAsync(ulnFnContext *aFnContext,
                                   acp_uint32_t  aAsyncMethod)
{
    ulnStmt   *sStmt = aFnContext->mHandle.mStmt;
    ulnStmt   *sAsyncPrefetchStmt = ulnDbcGetAsyncPrefetchStmt(sStmt->mParentDbc);
    ulnCursor *sCursor = ulnStmtGetCursor(sStmt);

    switch (aAsyncMethod)
    {
        case ALTIBASE_PREFETCH_ASYNC_OFF:
            if (ulnStmtIsCursorOpen(sStmt) == ACP_TRUE)
            {
                if (ulnDbcIsAsyncPrefetchStmt(sStmt->mParentDbc, sStmt) == ACP_TRUE)
                {
                    /* end asynchronous prefetch */
                    ACI_TEST(ulnFetchEndAsync(aFnContext,
                                              &sStmt->mParentDbc->mPtContext)
                            != ACI_SUCCESS);
                }
                else
                {
                    /* already synchronous prefetch */
                }
            }
            else
            {
                /* cursor not opened */
            }

            sStmt->mAttrPrefetchAsync = aAsyncMethod;
            break;

        case ALTIBASE_PREFETCH_ASYNC_PREFERRED: /* semi-async prefetch */
            ACI_TEST_RAISE((ulnDbcGetCmiLinkImpl(sStmt->mParentDbc) != CMI_LINK_IMPL_TCP) &&
                           (ulnDbcGetCmiLinkImpl(sStmt->mParentDbc) != CMI_LINK_IMPL_SSL), LABEL_CANNOT_BE_SET_NOW);

            /* allocate semi-async prefetch data structure */
            if (sStmt->mSemiAsyncPrefetch == NULL)
            {
                ACI_TEST_RAISE(ulnAllocSemiAsyncPrefetch(&sStmt->mSemiAsyncPrefetch) != ACI_SUCCESS,
                               LABEL_NOT_ENOUGH_MEM);

                ulnInitSemiAsyncPrefetch(sStmt);
            }
            else
            {
                /* already allocated */
            }

            if (ulnStmtIsCursorOpen(sStmt) == ACP_TRUE)
            {
                ACI_TEST_RAISE(ulnCursorGetType(sCursor) != SQL_CURSOR_FORWARD_ONLY, LABEL_CANNOT_BE_SET_NOW);
                ACI_TEST_RAISE(ulnStmtGetAttrConcurrency(sStmt) != SQL_CONCUR_READ_ONLY, LABEL_CANNOT_BE_SET_NOW);

                if (sAsyncPrefetchStmt == NULL)
                {
                    /* begin asynchronous prefetch */
                    ACI_TEST(ulnFetchBeginAsync(aFnContext,
                                                &sStmt->mParentDbc->mPtContext)
                            != ACI_SUCCESS);
                }
                else if (sAsyncPrefetchStmt != sStmt)
                {
                    /* cannot be prefetched asynchronous and will be prefetched synchronous */
                    ulnError(aFnContext, ulERR_IGNORE_CANNOT_BE_PREFETCHED_ASYNC);
                    ULN_FNCONTEXT_SET_RC(aFnContext, SQL_SUCCESS_WITH_INFO);
                }
                else
                {
                    /* already asynchronous prefetch statement */
                }
            }
            else
            {
                /* cursor not opened */
            }

            sStmt->mAttrPrefetchAsync = aAsyncMethod;
            break;

        default:
            ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnStmtSetAttrPrefetchAsync");
    }
    ACI_EXCEPTION(LABEL_CANNOT_BE_SET_NOW)
    {
        ulnError(aFnContext, ulERR_ABORT_ATTRIBUTE_CANNOT_BE_SET_NOW);
    }
    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void ulnStmtSetAttrPrefetchAutoTuning(ulnFnContext *aFnContext,
                                      acp_bool_t    aIsPrefetchAutoTuning)
{
#if defined(ALTI_CFG_OS_LINUX)

    ulnStmt *sStmt = aFnContext->mHandle.mStmt;


    if (ulnStmtIsCursorOpen(sStmt) == ACP_TRUE)
    {
        if (ulnDbcIsAsyncPrefetchStmt(sStmt->mParentDbc, sStmt) == ACP_TRUE)
        {
            if ((sStmt->mAttrPrefetchAutoTuning == ACP_TRUE) && (aIsPrefetchAutoTuning == ACP_FALSE))
            {
                /* auto-tuning : OFF */
                (void)ulnFetchEndAutoTuning(aFnContext);
            }
            else if ((sStmt->mAttrPrefetchAutoTuning == ACP_FALSE) && (aIsPrefetchAutoTuning == ACP_TRUE))
            {
                /* auto-tuning : ON */
                ulnFetchBeginAutoTuning(aFnContext);
            }
        }
        else
        {
            /* other statement is doing the asynchronous prefetch */
        }
    }
    else
    {
        /* cursor not opened */
    }

    sStmt->mAttrPrefetchAutoTuning = aIsPrefetchAutoTuning;

#else /* ALTI_CFG_OS_LINUX */

    if (aIsPrefetchAutoTuning == ACP_TRUE)
    {
        ulnError(aFnContext,
                 ulERR_IGNORE_OPTION_VALUE_CHANGED,
                 "ALTIBASE_PREFETCH_AUTO_TUNING changed to OFF");
    }
    else
    {
        /* nothing to do */
    }

#endif /* ALTI_CFG_OS_LINUX */
}

void ulnStmtSetAttrPrefetchAutoTuningMin(ulnFnContext *aFnContext,
                                         acp_uint32_t  aPrefetchAutoTuningMin)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    ULN_PREFETCH_ASYNC_STAT_TRCLOG(aFnContext,
                                   "auto-tuning : ALTIBASE_PREFETCH_AUTO_TUNING_MIN = %"ACI_UINT32_FMT"",
                                   aPrefetchAutoTuningMin);

    sStmt->mAttrPrefetchAutoTuningMin = aPrefetchAutoTuningMin;
}

void ulnStmtSetAttrPrefetchAutoTuningMax(ulnFnContext *aFnContext,
                                         acp_uint32_t  aPrefetchAutoTuningMax)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    ULN_PREFETCH_ASYNC_STAT_TRCLOG(aFnContext,
                                   "auto-tuning : ALTIBASE_PREFETCH_AUTO_TUNING_MAX = %"ACI_UINT32_FMT"",
                                   aPrefetchAutoTuningMax);

    sStmt->mAttrPrefetchAutoTuningMax = aPrefetchAutoTuningMax;
}

ACI_RC ulnStmtEnsureAllocDeferredQstr(ulnStmt *aStmt, acp_sint32_t aBufLen)
{
    if (aStmt->mDeferredQstrMaxLen < aBufLen)
    {
        if (aStmt->mDeferredQstr != NULL)
        {
            acpMemFree(aStmt->mDeferredQstr);
        }

        ACI_TEST( acpMemAlloc((void**)&aStmt->mDeferredQstr, aBufLen)
                  != ACP_RC_SUCCESS );
        aStmt->mDeferredQstrMaxLen = aBufLen;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aStmt->mDeferredQstr       = NULL;
    aStmt->mDeferredQstrMaxLen = 0;
    aStmt->mDeferredQstrLen    = 0;

    return ACI_FAILURE;
}

