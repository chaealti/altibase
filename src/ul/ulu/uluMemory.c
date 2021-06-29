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

#include <acp.h>
#include <acl.h>
#include <ace.h>
#include <aciErrorMgr.h>
#include <ulu.h>
#include <uluMemory.h>

#if defined(ALTIBASE_MEMORY_CHECK)
static const uluMemoryType gUluMemoryType = ULU_MEMORY_TYPE_DIRECT;
#else
static const uluMemoryType gUluMemoryType = ULU_MEMORY_TYPE_MANAGER;
#endif

extern uluChunkPoolOpSet gUluChunkPoolOp[];
extern uluMemoryOpSet    gUluMemoryOp[];

/*
 *      a memory chunk
 *      +---------------+
 *      |mList          |
 *      |mBuffer -----------+
 *      |mChunkSize     |   |
 *      |mSavepoints[0] |   |
 *      |mSavepoints[1] |   |
 *      |      ...      |   |
 *      |mSavepoints[n] |   |
 *      +---------------+ <-+
 *      |      ...      |
 *      |               |
 *      +---------------+
 */

/*
 * manager alloc ����� �� ���� chunk
 */

struct uluChunk
{
    acp_list_node_t  mList;

    acp_uint8_t     *mBuffer;
    acp_uint32_t     mChunkSize;        /* mBuffer �� ����Ű�� �� ���� ûũ�� �������� ũ�� */
    acp_uint32_t     mOffset;           /* Current offset */
    acp_uint32_t     mFreeMemSize;      /* �� ûũ�� ���� free �޸��� ũ�� */
    acp_uint32_t     mSavepoints[1];    /* �迭�� ���� ���� mBuffer �κ����� offset */
};

/*
 * direct alloc ����� �� ���� chunk
 */

struct uluChunk_DIRECT
{
    acp_list_node_t  mList;
    void            *mBuffer;
    acp_uint32_t     mBufferSize;
};


/*
 * ==============================
 *
 * uluChunk
 *
 * ==============================
 */

static void uluInitSavepoints(uluChunk *aChunk, acp_uint32_t aNumOfSavepoints)
{
    acp_uint32_t sIndex;

    /*
     * mSavepoints[] �迭�� ���� 0 �� �ִ� �Լ�.
     */

    for (sIndex = 0; sIndex < aNumOfSavepoints + 1; sIndex++)
    {
        aChunk->mSavepoints[sIndex] = 0;
    }
}

/*
 * �Ҵ�Ǵ� �޸��� ��
 *
 *      sizeof(uluChunk) + (Savepoint ����)���� acp_uint32_t + sizeof(uluMemory) + align8(aSizeRequested)
 *
 * uluMemory �� �ν��Ͻ��� �� �ڸ��� ������ �� �Ҵ������ν� ����ڰ�
 * ûũ Ǯ�� ���� �� �ִ�� ������ �縸ŭ�� �޸𸮸� �Ҵ��� �� �ֵ��� �����Ѵ�.
 */
static uluChunk *uluCreateChunk(acp_uint32_t aSizeRequested, acp_uint32_t aNumOfSavepoints)
{
    uluChunk     *sNewChunk;
    acp_uint32_t  sSizeChunkHeader;
    acp_uint32_t  sSizeUluMemory;

    aSizeRequested   = ACP_ALIGN8(aSizeRequested);
    sSizeChunkHeader = ACP_ALIGN8(ACI_SIZEOF(uluChunk) + (ACI_SIZEOF(acp_uint32_t) * aNumOfSavepoints));
    sSizeUluMemory   = ACP_ALIGN8(ACI_SIZEOF(uluMemory));

    ACI_TEST(acpMemAlloc((void**)&sNewChunk,
                         sSizeChunkHeader + sSizeUluMemory + aSizeRequested)
             != ACP_RC_SUCCESS);

    /*
     * uluChunk �� mSavepoints �迭�� 0 ���� �ʱ�ȭ
     */
    acpMemSet(sNewChunk, 0, sSizeChunkHeader);

    acpListInitObj(&sNewChunk->mList, sNewChunk);

    sNewChunk->mBuffer      = (acp_uint8_t *)sNewChunk + sSizeChunkHeader;
    sNewChunk->mChunkSize   = aSizeRequested + sSizeUluMemory;
    sNewChunk->mFreeMemSize = aSizeRequested + sSizeUluMemory;
    sNewChunk->mOffset      = 0;

    return sNewChunk;

    ACI_EXCEPTION_END;

    return NULL;
}

static void uluDestroyChunk(uluChunk *sChunk)
{
    acpMemFree(sChunk);
}

/*
 * ====================================
 *
 * uluChunkPool
 *
 * ====================================
 */

static void uluInsertNewChunkToPool(uluChunk *aChunk, uluChunkPool *aPool)
{
    ACE_DASSERT(aChunk != NULL);
    ACE_DASSERT(aPool != NULL);

    acpListPrependNode(&(aPool->mFreeChunkList), &(aChunk->mList));
    aPool->mFreeChunkCnt++;
}

static void uluDeleteChunkFromPool(uluChunk *aChunk)
{
    ACE_DASSERT(aChunk != NULL);

    acpListDeleteNode((acp_list_t *)aChunk);
    uluDestroyChunk(aChunk);
}

static void uluChunkPoolAddMemory(uluChunkPool *aPool, uluMemory *aMemory)
{
    acpListPrependNode(&aPool->mMemoryList, &aMemory->mList);
    aPool->mMemoryCnt++;
}

static void uluChunkPoolRemoveMemory(uluChunkPool * aPool, uluMemory *aMemory)
{
    acpListDeleteNode((acp_list_t *)aMemory);
    aPool->mMemoryCnt--;
}

/*
 * ====================================
 * uluChunkPool : Get Chunk From Pool
 * ====================================
 */

static uluChunk *uluGetChunkFromPool_MANAGER(uluChunkPool *aPool, acp_uint32_t aChunkSize)
{
    acp_list_node_t *sIterator;
    acp_list_node_t *sIteratorSafe;
    uluChunk        *sNewChunk;

    ACE_DASSERT(aPool != NULL);

    sNewChunk = NULL;

    /*
     * ��û�� ûũ�� ũ�Ⱑ ����Ʈ���� ũ�� ������ ûũ �Ҵ��Ѵ�.
     * ûũ �������� �˻������ ���̱� �����̴�.
     *
     * Note : ����Ʈ ûũ ����� �����ϰ� �� �����ؾ� �ʿ���� malloc �� ���� ������ ��
     *        �ִ�.
     */
    if (aChunkSize <= aPool->mDefaultChunkSize)
    {
        /*
         * uluChunkPool �� ������ �ִ� free chunk �� ������ ������
         * ûũ�� �� �Ҵ��ؾ� �ϴ����� �Ǵ��ؼ� �ʿ��� ��ŭ�� �� �Ҵ��Ѵ�.
         */
        if (aPool->mFreeChunkCnt == 0)
        {
            if (aPool->mFreeBigChunkCnt == 0)
            {
                /*
                 * ���� free chunk �� ����.
                 */
                sNewChunk = uluCreateChunk(aPool->mDefaultChunkSize, aPool->mNumberOfSP);

                ACI_TEST(sNewChunk == NULL);
            }
            else
            {
                /*
                 * �׳��� Big chunk list �� free chunk �� �ִ�.
                 */
                sNewChunk = (uluChunk *)(aPool->mFreeBigChunkList.mNext);
                ACI_TEST(sNewChunk == NULL);

                acpListDeleteNode((acp_list_t *)sNewChunk);
                aPool->mFreeBigChunkCnt--;
            }
        }
        else
        {
            sNewChunk = (uluChunk *)(aPool->mFreeChunkList.mNext);
            ACI_TEST(sNewChunk == NULL);

            acpListDeleteNode((acp_list_t *)sNewChunk);
            aPool->mFreeChunkCnt--;
        }
    }
    else
    {
        /*
         * ����Ʈ ûũ ������ ���� ū ũ���� �޸𸮸� ��û.
         */

        /*
         * free big chunk list �˻�,
         */
        ACP_LIST_ITERATE_SAFE(&(aPool->mFreeBigChunkList), sIterator, sIteratorSafe)
        {
            /*
             * ��û�� ������� ū ûũ�� ������ �װ��� ����.
             */
            if (((uluChunk *)sIterator)->mChunkSize >= aChunkSize)
            {
                ACE_ASSERT(aPool->mFreeBigChunkCnt != 0);

                sNewChunk = (uluChunk *)sIterator;

                acpListDeleteNode((acp_list_t *)sNewChunk);
                aPool->mFreeBigChunkCnt--;
                break;
            }
        }

        /*
         * ������ �ش� ������� ûũ ����, ����.
         */
        if (sNewChunk == NULL)
        {
            sNewChunk = uluCreateChunk(aChunkSize, aPool->mNumberOfSP);
            ACI_TEST(sNewChunk == NULL);
        }
    }

    return sNewChunk;

    ACI_EXCEPTION_END;

    return NULL;
}

static uluChunk *uluGetChunkFromPool_DIRECT(uluChunkPool *aPool,
                                            acp_uint32_t  aChunkSize)
{
    ACP_UNUSED(aPool);
    ACP_UNUSED(aChunkSize);

    return NULL;
}

/*
 * ====================================
 * uluChunkPool : Return Chunk To Pool
 * ====================================
 */

static void uluReturnChunkToPool_MANAGER(uluChunkPool *aPool, uluChunk *aChunk)
{
    ACE_DASSERT(aPool != NULL);
    ACE_DASSERT(aChunk != NULL);

    /*
     * Note: ó�� �����ϴ� ûũ�� ������ ��������,
     *       ��ȯ�� ûũ�� �����ϴ� ��� ��� �ʵ尡 �ʱ� ���´� �ƴԿ� Ʋ������.
     *       ���� ûũ�� �ʵ带 �ʱ�ȭ�ϴ� ���� �ʿ��ϴ�.
     *       �׷��� �Ͽ� ûũ Ǯ�� �ִ� ��� ûũ�� �������� ��� �̿� �����ϵ��� �Ѵ�.
     */
    uluInitSavepoints(aChunk, aPool->mNumberOfSP);
    aChunk->mFreeMemSize = aChunk->mChunkSize;
    aChunk->mOffset      = 0;

    if (aChunk->mChunkSize > aPool->mActualSizeOfDefaultSingleChunk)
    {
        /*
         * ũ�Ⱑ default chunk size ���� ū ûũ�� ������.
         */
        acpListPrependNode(&(aPool->mFreeBigChunkList), (acp_list_t *)aChunk);
        aPool->mFreeBigChunkCnt++;
    }
    else
    {
        /*
         * ũ�Ⱑ default chunk size �� ûũ�� ������.
         */
        acpListPrependNode(&(aPool->mFreeChunkList), (acp_list_t *)aChunk);
        aPool->mFreeChunkCnt++;
    }
}

static void uluReturnChunkToPool_DIRECT(uluChunkPool *aPool, uluChunk *aChunk)
{
    ACP_UNUSED(aPool);
    ACP_UNUSED(aChunk);

    return;
}

/*
 * ==========================================================
 * uluChunkPool : Initialize Chunk Pool : ���� ���� ûũ ����
 * ==========================================================
 */

static ACI_RC uluChunkPoolCreateInitialChunks_MANAGER(uluChunkPool *aChunkPool)
{
    acp_uint32_t  i;
    uluChunk     *sChunk;

    for (i = 0; i < aChunkPool->mMinChunkCnt; i++)
    {
        sChunk = uluCreateChunk(aChunkPool->mDefaultChunkSize, aChunkPool->mNumberOfSP);

        ACI_TEST(sChunk == NULL);

        uluInsertNewChunkToPool(sChunk, aChunkPool);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC uluChunkPoolCreateInitialChunks_DIRECT(uluChunkPool *aChunkPool)
{
    ACP_UNUSED(aChunkPool);

    return ACI_SUCCESS;
}

/*
 * ====================================
 * uluChunkPool : Destroy Chunk Pool
 * ====================================
 */

static void uluDestroyChunkPool_MANAGER(uluChunkPool *aPool)
{
    acp_list_node_t *sTempHead;
    acp_list_node_t *sTempHeadNext;

    /*
     * aPool �� �ҽ��� ����ϰ� �ִ� ��� uluMemory ��
     * �ν��Ͻ��� �湮�ؼ� �ı��Ѵ�.
     */

    ACP_LIST_ITERATE_SAFE(&aPool->mMemoryList, sTempHead, sTempHeadNext)
    {
        ((uluMemory *)sTempHead)->mOp->mDestroyMyself((uluMemory *)sTempHead);
    }

    /*
     * Free FreeChunks
     */
    ACP_LIST_ITERATE_SAFE(&aPool->mFreeChunkList, sTempHead, sTempHeadNext)
    {
        uluDeleteChunkFromPool((uluChunk *)sTempHead);
    }

    ACP_LIST_ITERATE_SAFE(&aPool->mFreeBigChunkList, sTempHead, sTempHeadNext)
    {
        uluDeleteChunkFromPool((uluChunk *)sTempHead);
    }

    acpMemSet(aPool, 0, ACI_SIZEOF(uluChunkPool));
    acpMemFree(aPool);

    return;
}

static void uluDestroyChunkPool_DIRECT(uluChunkPool *aPool)
{
    acp_list_node_t *sTempHead;
    acp_list_node_t *sTempHeadNext;

    ACP_LIST_ITERATE_SAFE(&aPool->mMemoryList, sTempHead, sTempHeadNext)
    {
        ((uluMemory *)sTempHead)->mOp->mDestroyMyself((uluMemory *)sTempHead);
    }

    acpMemFree(aPool);

    return;
}

/*
 * ==================================================
 * uluChunkPool : Get Reference Count of a Chunk Pool
 * ==================================================
 */

static acp_uint32_t uluChunkPoolGetReferenceCount(uluChunkPool *aPool)
{
    return aPool->mMemoryCnt;
}

/*
 * ==================================================
 * uluChunkPool : Operation Sets
 * ==================================================
 */

uluChunkPool *uluChunkPoolCreate(acp_uint32_t aDefaultChunkSize, acp_uint32_t aNumOfSavepoints, acp_uint32_t aMinChunkCnt)
{
    acp_bool_t    sNeedToFreeChunkPool = ACP_FALSE;
    uluChunkPool *sChunkPool = NULL;

    acp_uint32_t  sSizeUluMemory;
    acp_uint32_t  sActualSizeOfDefaultSingleChunk;

    /*
     * Note : ���̺� ����Ʈ�� ������ ûũ �������� ���� ������ �ǹ̰� ����.
     */
    ACI_TEST(aNumOfSavepoints >= (aDefaultChunkSize >> 1));

    ACI_TEST(acpMemAlloc((void**)&sChunkPool, ACI_SIZEOF(uluChunkPool)) != ACP_RC_SUCCESS);

    sNeedToFreeChunkPool = ACP_TRUE;

    aDefaultChunkSize               = ACP_ALIGN8(aDefaultChunkSize);
    sSizeUluMemory                  = ACP_ALIGN8(ACI_SIZEOF(uluMemory));
    sActualSizeOfDefaultSingleChunk = aDefaultChunkSize + sSizeUluMemory;

    acpListInit(&sChunkPool->mMemoryList);
    acpListInit(&sChunkPool->mFreeChunkList);
    acpListInit(&sChunkPool->mFreeBigChunkList);

    sChunkPool->mFreeChunkCnt        = 0;
    sChunkPool->mMinChunkCnt         = aMinChunkCnt;
    sChunkPool->mFreeBigChunkCnt     = 0;
    sChunkPool->mMemoryCnt           = 0;
    sChunkPool->mNumberOfSP          = aNumOfSavepoints;

    sChunkPool->mDefaultChunkSize    = aDefaultChunkSize;

    sChunkPool->mActualSizeOfDefaultSingleChunk
                                     = sActualSizeOfDefaultSingleChunk;

    sChunkPool->mOp                  = &gUluChunkPoolOp[gUluMemoryType];

    ACI_TEST(sChunkPool->mOp->mCreateInitialChunks(sChunkPool) != ACI_SUCCESS);

    return sChunkPool;

    ACI_EXCEPTION_END;

    if (sNeedToFreeChunkPool)
    {
        sChunkPool->mOp->mDestroyMyself(sChunkPool);
        sChunkPool = NULL;
    }

    return sChunkPool;
}


/*
 * ====================================
 *
 * uluMemory
 *
 * ====================================
 */

/*
 * ------------------------------
 * uluMemory : Alloc
 * ------------------------------
 */

static ACI_RC uluMemoryAlloc_MANAGER(uluMemory *aMemory, void **aPtr, acp_uint32_t aSize)
{
    void            *sAddrForUser;

    acp_bool_t       sFound = ACP_FALSE;

    acp_list_node_t *sTempNode;
    uluChunk        *sChunkForNewMem;

    /*
     * align �� 4 ��� ������ ��,
     * ���� �޸𸮰� 3 �̰�, ��û�� ����� 3 �� ������
     * align �� ���� ���� �޸𸮸� �ʰ��ϹǷ� �׳� ������ ��������.
     */
    aSize = ACP_ALIGN8(aSize);

    if (aMemory->mMaxAllocatableSize >= aSize)
    {
        /*
         * ����� �޸𸮸� ���� ûũ�� ã�Ƽ� �����Ѵ�.
         */
        ACP_LIST_ITERATE(&(aMemory->mChunkList), sTempNode)
        {
            if (((uluChunk *)sTempNode)->mFreeMemSize >= aSize)
            {
                sFound = ACP_TRUE;
                break;
            }
        }
    }

    if (sFound == ACP_FALSE)
    {
        sChunkForNewMem = aMemory->mChunkPool->mOp->mGetChunk(aMemory->mChunkPool, aSize);
        ACI_TEST_RAISE(sChunkForNewMem == NULL, NOT_ENOUGH_MEMORY);

        acpListPrependNode(&(aMemory->mChunkList), (acp_list_t *)sChunkForNewMem);
    }
    else
    {
        sChunkForNewMem = (uluChunk *)sTempNode;
    }

    /*
     * ûũ�� �غ� �������Ƿ�,
     * - ����ڿ��� ������ �޸� �ּҸ� �غ��Ѵ�.
     * - mChunk.mFreeMemSize �� �����Ѵ�.
     * - offset �� �����Ѵ�.
     */

    sAddrForUser                   = sChunkForNewMem->mBuffer + sChunkForNewMem->mOffset;
    sChunkForNewMem->mFreeMemSize -= aSize;
    sChunkForNewMem->mOffset      += aSize;

    aMemory->mAllocCount++;


    /*
     * �޸� �ּ� ����
     */

    *aPtr = sAddrForUser;

    return ACI_SUCCESS;

    ACI_EXCEPTION(NOT_ENOUGH_MEMORY);

    ACI_EXCEPTION_END;

    *aPtr = NULL;

    return ACI_FAILURE;
}

static ACI_RC uluMemoryAlloc_DIRECT(uluMemory *aMemory, void **aBufferPtr, acp_uint32_t aSize)
{
    acp_bool_t       sNeedToFreeMemDesc = ACP_FALSE;
    uluChunk_DIRECT *sMemoryDesc = NULL;

    /*
     * descriptor �Ҵ�
     */

    ACI_TEST(acpMemAlloc((void**)&sMemoryDesc, ACI_SIZEOF(uluChunk_DIRECT))
             != ACP_RC_SUCCESS);
    sNeedToFreeMemDesc = ACP_TRUE;

    acpListInitObj(&sMemoryDesc->mList, sMemoryDesc);

    /*
     * ���� ���� �Ҵ�
     */

    sMemoryDesc->mBufferSize = aSize;

    ACI_TEST(acpMemAlloc((void**)&sMemoryDesc->mBuffer, aSize) != ACP_RC_SUCCESS);

    /*
     * �޸𸮿� �Ŵޱ�
     */

    acpListPrependNode(&aMemory->mChunkList, (acp_list_node_t *)sMemoryDesc);
    aMemory->mAllocCount++;

    /*
     * �Ҵ��� �޸� �����ֱ�
     */

    *aBufferPtr = sMemoryDesc->mBuffer;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if (sNeedToFreeMemDesc == ACP_TRUE)
    {
        acpMemFree(sMemoryDesc);
    }

    *aBufferPtr = NULL;

    return ACI_FAILURE;
}

/*
 * ----------------------------------
 * uluMemory : Destroy uluMemory
 * ----------------------------------
 */

static void uluMemoryDestroy_MANAGER(uluMemory *aMemory)
{
    acp_uint32_t     sFirst;
    acp_list_node_t *sTempHead;
    acp_list_node_t *sTempHeadNext;
    uluChunkPool    *sPool = aMemory->mChunkPool;

    /*
     * �� �ڽ��� ûũ Ǯ�� �޸� ����Ʈ���� �����Ѵ�.
     */

    uluChunkPoolRemoveMemory(sPool, aMemory);

    sFirst = 0;

    /*
     * ûũ�� ��� �ݳ��Ѵ�.
     */

    ACP_LIST_ITERATE_SAFE(&aMemory->mChunkList, sTempHead, sTempHeadNext)
    {
        /*
         * uluMemory �� �ִ� initial chunk �� �����ϴ� ���� ���ϱ� ����.
         */

        if (sFirst == 0)
        {
            sFirst = 1;
            continue;
        }

        acpListDeleteNode(sTempHead);
        sPool->mOp->mReturnChunk(sPool, (uluChunk *)sTempHead);
    }

    /*
     * initial chunk �� ����
     */

    sPool->mOp->mReturnChunk(sPool, (uluChunk *)(aMemory->mChunkList.mNext));
}

static void uluMemoryFreeElement_DIRECT(uluChunk_DIRECT *aMemoryDesc)
{
    /*
     * buffer free
     */

    acpMemFree(aMemoryDesc->mBuffer);
    aMemoryDesc->mBufferSize = 0;

    /*
     * uluChunk_DIRECT free
     */

    acpListDeleteNode((acp_list_node_t *)aMemoryDesc);
    acpMemFree(aMemoryDesc);
}

static void uluMemoryDestroy_DIRECT(uluMemory *aMemory)
{
    uluChunk_DIRECT *sMemDesc;
    acp_list_node_t *sIterator;
    acp_list_node_t *sIterator2;

    /*
     * �� �ڽ��� ûũ Ǯ�� �޸� ����Ʈ���� �����Ѵ�.
     */

    uluChunkPoolRemoveMemory(aMemory->mChunkPool, aMemory);

    /*
     * ������ �ִ� ��� �޸𸮸� free �Ѵ�.
     */

    ACP_LIST_ITERATE_SAFE(&aMemory->mChunkList, sIterator, sIterator2)
    {
        sMemDesc = (uluChunk_DIRECT *)sIterator;

        uluMemoryFreeElement_DIRECT(sMemDesc);

        aMemory->mFreeCount++;
    }

    /*
     * uluMemory �ڽ��� free
     */

    acpMemFree(aMemory->mSPArray);
    acpMemFree(aMemory);
}

/*
 * ----------------------------------
 * uluMemory : Mark Save Point
 * ----------------------------------
 */

/*
 * Note : mChunkPool->mNumberOfSP �� 3 �̸�,
 *        mSavepoints�迭�� 0 ���� 3 ������ �迭 ���ڸ� ���´�.
 *
 *        �̰��� uluMemory ����ü���� ���̺� ����Ʈ 0 ����
 *        3�������� SP (1, 2, 3) �� �ʿ��ϱ� �����̴�.
 */

static ACI_RC uluMemoryMarkSP_MANAGER(uluMemory *aMemory)
{
    acp_uint32_t     sNewSPIndex = aMemory->mCurrentSPIndex + 1;
    acp_uint32_t     sTempSPIndex;
    acp_uint32_t     sCurrSPIndex;
    acp_list_node_t *sTempNode;

    ACI_TEST_RAISE(sNewSPIndex > aMemory->mChunkPool->mNumberOfSP ||
                   sNewSPIndex <= aMemory->mCurrentSPIndex ||
                   sNewSPIndex == 0,
                   INVALID_SP_INDEX);

    sCurrSPIndex = aMemory->mCurrentSPIndex;

    ACP_LIST_ITERATE(&aMemory->mChunkList, sTempNode)
    {
        for (sTempSPIndex = sCurrSPIndex + 1; sTempSPIndex < sNewSPIndex; sTempSPIndex++)
        {
            /*
             * Current SP Index �� ����� offset �� New SP Index �ٷ� ���������� SP Index �鿡
             * ������ �ִ´�.
             */
            ((uluChunk *)sTempNode)->mSavepoints[sTempSPIndex] = ((uluChunk *)sTempNode)->mOffset;
        }

        /*
         * ���ο� SP Index �� ûũ�� ���� offset �� �����Ѵ�.
         */
        ((uluChunk *)sTempNode)->mSavepoints[sTempSPIndex] = ((uluChunk *)sTempNode)->mOffset;
    }

    aMemory->mCurrentSPIndex = sNewSPIndex;

    return ACI_SUCCESS;

    ACI_EXCEPTION(INVALID_SP_INDEX);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC uluMemoryMarkSP_DIRECT(uluMemory *aMemory)
{
    acp_uint32_t sNewSPIndex = aMemory->mCurrentSPIndex + 1;

    ACI_TEST_RAISE(sNewSPIndex > aMemory->mChunkPool->mNumberOfSP ||
                   sNewSPIndex <= aMemory->mCurrentSPIndex ||
                   sNewSPIndex == 0,
                   INVALID_SP_INDEX);

    aMemory->mSPArray[aMemory->mCurrentSPIndex + 1] = aMemory->mChunkList.mPrev;

    return ACI_SUCCESS;

    ACI_EXCEPTION(INVALID_SP_INDEX);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ----------------------------------
 * uluMemory : Delete Last Save Point
 * ----------------------------------
 */

static ACI_RC uluMemoryDeleteLastSP(uluMemory *aMemory)
{
    if (aMemory->mCurrentSPIndex > 0)
    {
        aMemory->mCurrentSPIndex--;
    }

    return ACI_SUCCESS;
}

/*
 * ----------------------------------
 * uluMemory : Get Current Save Point
 * ----------------------------------
 */

static acp_uint32_t uluMemoryGetCurrentSP(uluMemory * aMemory)
{
    return aMemory->mCurrentSPIndex;
}
/*
 * -------------------------------------
 * uluMemory : Free to Save Point
 * -------------------------------------
 */

static ACI_RC uluMemoryFreeToSP_MANAGER(uluMemory *aMemory, acp_uint32_t aSPIndex)
{
    acp_list_node_t *sTempNode;
    acp_list_node_t *sTempNodeNext;

    ACE_DASSERT(aMemory != NULL);

    /*
     * Note sp 0 ���� ����� ���� �޸𸮰� ó�� ������ ���·� ����� ���̴�.
     * ���� üũ�� �ʿ� ����.
     *
     * Note aSPIndex �� mCurrentSPIndex �� ������ �ű���� ������ �ؾ� �Ѵ�.
     */

    ACI_TEST_RAISE(aSPIndex > aMemory->mCurrentSPIndex, INVALID_SP_INDEX);

    /*
     * ���۰���.
     * - uluMemory.mCurrentSPIndex �� aSPIndex �� �����Ѵ�.
     * - uluMemory �� ������ �ִ� mChunkList �� �� ���ƺ��鼭
     *   mSavepoints[aSPIndex] �� 0 �� ���� ������ ûũ Ǯ�� �ݳ��Ѵ�.
     */
    aMemory->mCurrentSPIndex = aSPIndex;

    ACP_LIST_ITERATE_SAFE(&(aMemory->mChunkList), sTempNode, sTempNodeNext)
    {
        if (((uluChunk *)sTempNode)->mSavepoints[aSPIndex] == 0)
        {
            acpListDeleteNode(sTempNode);

            aMemory->mChunkPool->mOp->mReturnChunk(aMemory->mChunkPool, (uluChunk *)sTempNode);
        }
        else
        {
            /*
             * Chunk �� Offset �� ������ savepoint index �� ��� �ִ� ������ �ٲ۴�.
             */
            ((uluChunk *)sTempNode)->mOffset = ((uluChunk *)sTempNode)->mSavepoints[aSPIndex];

            /*
             * Chunk �� mFreeMemSize �� �ٽ� ����ؼ� �����Ѵ�.
             */
            ((uluChunk *)sTempNode)->mFreeMemSize =
                ((uluChunk *)sTempNode)->mChunkSize - ((uluChunk *)sTempNode)->mOffset;
        }
    }

    aMemory->mFreeCount++;

    return ACI_SUCCESS;

    ACI_EXCEPTION(INVALID_SP_INDEX);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC uluMemoryFreeToSP_DIRECT(uluMemory *aMemory, acp_uint32_t aSPIndex)
{
    uluChunk_DIRECT *sMemDesc;
    acp_list_node_t *sIterator1;
    acp_list_node_t *sIterator2;

    acp_list_t       sNewList;
    acp_list_node_t *aNode;

    ACI_TEST_RAISE(aSPIndex > aMemory->mCurrentSPIndex, INVALID_SP_INDEX);

    acpListInit(&sNewList);

    aNode = aMemory->mSPArray[aSPIndex];

    if (aNode != NULL)
    {
        /*
         * SP �� ����Ű�� �޸𸮺���� �����Ǹ� �ȵȴ�.
         * �� ���� ��Ϻ��� ������ �����ؾ� �Ѵ�.
         */

        aNode = aNode->mNext;

        if (aNode != &aMemory->mChunkList)
        {
            acpListSplit(&aMemory->mChunkList, aNode, &sNewList);

            ACP_LIST_ITERATE_SAFE(&sNewList, sIterator1, sIterator2)
            {
                sMemDesc = (uluChunk_DIRECT *)sIterator1;

                /*
                 * Note : uluMemoryFreeElement_DIRECT() ���� LIST_REMOVE ���� �Ѵ�
                 */

                uluMemoryFreeElement_DIRECT(sMemDesc);

                aMemory->mFreeCount++;
            }
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(INVALID_SP_INDEX);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ---------------------------------
 * uluMemory : Create
 * ---------------------------------
 */

uluMemory *uluMemoryCreate_MANAGER(uluChunkPool *aSourcePool)
{
    acp_uint32_t  sSizeUluMemory;
    uluMemory    *sNewMemory = NULL;
    uluChunk     *sInitialChunk;

    /*
     * ���ʿ� �ʿ��� ûũ�� �����´�.
     */
    sInitialChunk = aSourcePool->mOp->mGetChunk(aSourcePool, aSourcePool->mDefaultChunkSize);
    ACI_TEST(sInitialChunk == NULL);

    sSizeUluMemory = ACP_ALIGN8(ACI_SIZEOF(uluMemory));

    /*
     * uluMemory �ν��Ͻ��� ���� ������ �Ҵ��� �ڿ� ûũ�� savepoint index 0 ����
     * uluMemory �� ����� �־� �����ν� �Ǽ��� ���� initial chunk �� �ݳ���
     * �����ؾ� �Ѵ�.
     *
     * ���̺� ����Ʈ �ε����� 1 ���̽��� �Ѵ�.
     * �ֳ��ϸ� �Ǽ��� uluMemory ����ü�� �� �ִ� ûũ�� �ݳ��ϸ� �ȵǱ�
     * �����̴�. uluMemory ����ü�� ����ִ� ûũ�� uluMemory �� �ı��� ������
     * �ݳ����� �ʴ´�. ���� �� ûũ�� savepoint index 0 �� ���� ������ �ȴ�.
     *
     * ���� ������ ûũ�� uluMemory �� �Ҵ������Ƿ�
     * uluChunk.mFreeMemSize �� �����Ѵ�.
     */

    sNewMemory = (uluMemory *)(sInitialChunk->mBuffer);

    sInitialChunk->mOffset         = sSizeUluMemory;
    sInitialChunk->mFreeMemSize   -= sSizeUluMemory;
    sInitialChunk->mSavepoints[0]  = sInitialChunk->mOffset;
    sNewMemory->mCurrentSPIndex    = 0;

    acpListInitObj(&sNewMemory->mList, sNewMemory);
    acpListInit(&sNewMemory->mChunkList);

    sNewMemory->mMaxAllocatableSize = sInitialChunk->mFreeMemSize;
    sNewMemory->mAllocCount         = 0;
    sNewMemory->mFreeCount          = 0;
    sNewMemory->mSPArray            = NULL;

    sNewMemory->mChunkPool          = aSourcePool;

    uluChunkPoolAddMemory(aSourcePool, sNewMemory);

    /*
     * �Ҵ���� ûũ�� uluMemory �� ûũ ����Ʈ�� �Ŵܴ�.
     * �Ŵޱ� ���� ��ũ����� �ʱ�ȭ�� �ݵ�� �� �־�� �Ѵ�.
     */

    acpListPrependNode(&sNewMemory->mChunkList, (acp_list_t *)sInitialChunk);

    return sNewMemory;

    ACI_EXCEPTION_END;

    return sNewMemory;
}

uluMemory *uluMemoryCreate_DIRECT(uluChunkPool *aSourcePool)
{
    acp_uint32_t  sSPArraySize;
    acp_bool_t    sNeedToFreeNewMemory = ACP_FALSE;
    uluMemory    *sNewMemory = NULL;

    /*
     * �ʿ��� �޸� �Ҵ�
     */

    ACI_TEST(acpMemAlloc((void**)&sNewMemory, ACI_SIZEOF(uluMemory)) != ACP_RC_SUCCESS);
    sNeedToFreeNewMemory = ACP_TRUE;

    sSPArraySize = ACI_SIZEOF(acp_list_node_t) * aSourcePool->mNumberOfSP;
    ACI_TEST(acpMemAlloc((void**)&sNewMemory->mSPArray, sSPArraySize) != ACP_RC_SUCCESS);

    acpMemSet(sNewMemory->mSPArray, 0, sSPArraySize);

    /*
     * �ʱ�ȭ
     */

    acpListInitObj(&sNewMemory->mList, sNewMemory);

    sNewMemory->mMaxAllocatableSize = 0;
    sNewMemory->mAllocCount         = 0;
    sNewMemory->mFreeCount          = 0;
    sNewMemory->mCurrentSPIndex     = 0;
    sNewMemory->mChunkPool          = aSourcePool;

    acpListInit(&sNewMemory->mChunkList);

    uluChunkPoolAddMemory(aSourcePool, sNewMemory);

    return sNewMemory;

    ACI_EXCEPTION_END;

    if (sNeedToFreeNewMemory == ACP_TRUE)
    {
        acpMemFree(sNewMemory);
    }

    //BUG-25201 [CodeSonar] �޸� ������ ������ ������.
    return NULL;
}

ACI_RC uluMemoryCreate(uluChunkPool *aSourcePool, uluMemory **aNewMem)
{
    uluMemory *sNewMemory;

    /*
     * create �� initialize �� �и��ϰ� ������
     * �׷��ڸ� ������ ������ �Ұ����ϹǷ� �׳� create �ȿ��� initialize ���� �Ѵ�.
     */

    sNewMemory = gUluMemoryOp[gUluMemoryType].mCreate(aSourcePool);
    ACI_TEST(sNewMemory == NULL);

    sNewMemory->mOp = &gUluMemoryOp[gUluMemoryType];

    *aNewMem = sNewMemory;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    *aNewMem = NULL;

    return ACI_FAILURE;
}

uluMemoryOpSet gUluMemoryOp[ULU_MEMORY_TYPE_MAX] =
{
    {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
    },

    {
        /*
         * ULU_MEMORY_TYPE_MANAGER
         */
        uluMemoryCreate_MANAGER,
        uluMemoryDestroy_MANAGER,

        uluMemoryAlloc_MANAGER,
        uluMemoryFreeToSP_MANAGER,

        uluMemoryMarkSP_MANAGER,

        uluMemoryDeleteLastSP,
        uluMemoryGetCurrentSP
    },

    {
        /*
         * ULU_MEMORY_TYPE_DIRECT
         */
        uluMemoryCreate_DIRECT,
        uluMemoryDestroy_DIRECT,

        uluMemoryAlloc_DIRECT,
        uluMemoryFreeToSP_DIRECT,

        uluMemoryMarkSP_DIRECT,

        uluMemoryDeleteLastSP,
        uluMemoryGetCurrentSP
    }
};

uluChunkPoolOpSet gUluChunkPoolOp[ULU_MEMORY_TYPE_MAX] =
{
    {
        NULL, NULL, NULL, NULL, NULL
    },

    {
        /*
         * ULU_MEMORY_TYPE_MANAGER
         */
        uluChunkPoolCreateInitialChunks_MANAGER,

        uluDestroyChunkPool_MANAGER,
        uluGetChunkFromPool_MANAGER,
        uluReturnChunkToPool_MANAGER,

        uluChunkPoolGetReferenceCount
    },

    {
        /*
         * ULU_MEMORY_TYPE_DIRECT
         */
        uluChunkPoolCreateInitialChunks_DIRECT,

        uluDestroyChunkPool_DIRECT,
        uluGetChunkFromPool_DIRECT,
        uluReturnChunkToPool_DIRECT,

        uluChunkPoolGetReferenceCount
    }
};

