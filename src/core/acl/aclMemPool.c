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
 
/*******************************************************************************
 * $Id: aclMemPool.c 9884 2010-02-03 06:50:04Z gurugio $
 ******************************************************************************/

#include <acpMem.h>
#include <acpSpinLock.h>
#include <acpSys.h>
#include <aceAssert.h>
#include <acpList.h>
#include <aclMemPool.h>


/*
 *       Pool                Stores                 Chunks
 *    +-----------+
 *    |  ...      |       +--------------+
 *    |  Store[]  | ----> |  ...         |       +-----------+
 *    +-----------+       |  Chunk List  | ----> |  ...      |
 *                        +--------------+       |  Block 1  |
 *                        |  ...         |       |  Block 2  |
 *                        |  Chunk List  |       |  Block n  |
 *                        +--------------+       +-----------+
 *
 * Pool�� �ϳ� �Ǵ� �ټ��� Store Array�� ������.
 * Store������ aclMemPoolCreate�� aParallelFactor�� ���� �����ȴ�.
 *
 * Store�� Chunk List�� ������ Alloc Chunk List�� Free Chunk List�� �����Ѵ�.
 * Free Chunk List���� �ϳ��̻��� Free Block�� ������ Chunk���� �Ŵ޸���.
 *
 * Chunk�� �ټ��� Block Array�� ������.
 * Block������ aclMemPoolCreate�� aElementCount�� ���� �����ȴ�.
 *
 * Chunk�� Free Block List�� �����Ѵ�.
 */


/*
 * Chunk �޸� ũ�� =
 * Chunk ����ü ũ�� + (Block ����ü ũ�� + aElementSize) * aElementCount
 */
#define ACL_MEM_POOL_SIZEOF_CHUNK(aPool)                                       \
    (ACP_ALIGN8(sizeof(aclMemPoolChunk)) +                                     \
     (ACP_ALIGN8(sizeof(aclMemPoolBlock)) +                                 \
      ACP_ALIGN8((aPool)->mElementSize)) *                                  \
     (aPool)->mElementCount)

/*
 * Block �ּ� =
 * Chunk �ּ� + Chunk ����ü ũ�� + (Block ����ü ũ�� + aElementSize) * Index
 */
#define ACL_MEM_POOL_BLOCK_AT(aPool, aChunk, aIndex)                           \
    (aclMemPoolBlock *)((acp_char_t *)(aChunk) +                            \
                        ACP_ALIGN8(sizeof(aclMemPoolChunk)) +                  \
                        (ACP_ALIGN8(sizeof(aclMemPoolBlock)) +                 \
                         ACP_ALIGN8((aPool)->mElementSize)) * (aIndex))

#define ACL_MEM_POOL_USER_ADDR_FROM_BLOCK(aBlock)                              \
    (void *)((acp_char_t *)(sBlock) + ACP_ALIGN8(sizeof(aclMemPoolBlock)))

#define ACL_MEM_POOL_BLOCK_FROM_USER_ADDR(aAddr)                        \
    (void *)((acp_char_t *)(aAddr) - ACP_ALIGN8(sizeof(aclMemPoolBlock)))


typedef struct aclMemPoolStore aclMemPoolStore;
typedef struct aclMemPoolChunk aclMemPoolChunk;
typedef struct aclMemPoolBlock aclMemPoolBlock;

struct aclMemPoolStore
{
    acl_mem_pool_t  *mPool;              /* pointer to the parent pool        */
    acp_spin_lock_t  mLock;              /* access lock for multithread-safe  */

    acp_list_t       mFullChunkList;     /* chunks that have no free block    */
    acp_list_t       mFreeChunkList;     /* chunks that have free block(s)    */
};

struct aclMemPoolChunk
{
    aclMemPoolStore *mStore;             /* pointer to the parent chunk store */
    acp_list_node_t  mChunkListNode;     /* node for chunk list of store      */

    acp_uint32_t     mInitBlockCount;    /* number of initialized blocks      */
    acp_uint32_t     mFreeBlockCount;    /* number of free blocks             */
    aclMemPoolBlock *mLastFreeBlock;     /* last freed block                  */

                                         /* and, followed by block array      */
};

struct aclMemPoolBlock
{
    aclMemPoolChunk *mChunk;             /* pointer to the parent chunk       */
    aclMemPoolBlock *mNextFreeBlock;     /* next block to reuse               */

                                         /* and, followed by user memory      */
};


/*
 * Chunk�� �����Ҵ� �� Store�� �߰�
 */
ACP_INLINE acp_rc_t aclMemPoolAllocChunk(acl_mem_pool_t   *aPool,
                                         aclMemPoolStore  *aStore,
                                         aclMemPoolChunk **aChunk)
{
    aclMemPoolChunk *sChunk = NULL;
    acp_rc_t         sRC;

    /*
     * Chunk �޸� �Ҵ�
     */
    sRC = acpMemAlloc((void **)&sChunk, ACL_MEM_POOL_SIZEOF_CHUNK(aPool));

    if (ACP_RC_NOT_SUCCESS(sRC))
    {
        return sRC;
    }
    else
    {
        *aChunk = sChunk;
    }

    /*
     * Chunk ����ü �ʱ�ȭ
     */
    sChunk->mStore          = aStore;
    sChunk->mInitBlockCount = 0;
    sChunk->mFreeBlockCount = aPool->mElementCount;
    sChunk->mLastFreeBlock  = NULL;

    acpListInitObj(&sChunk->mChunkListNode, sChunk);

    /*
     * Store�� Chunk List�� �߰�
     */
    acpListPrependNode(&aStore->mFreeChunkList, &sChunk->mChunkListNode);

    return ACP_RC_SUCCESS;
}

/*
 * Chunk �Ҵ� ���� �� Store���� ����
 */
ACP_INLINE void aclMemPoolFreeChunk(aclMemPoolChunk *aChunk)
{
    acpListDeleteNode(&aChunk->mChunkListNode);

    acpMemFree(aChunk);
}

/*
 * Store���� Block �Ҵ�
 */
ACP_INLINE acp_rc_t aclMemPoolAllocBlock(acl_mem_pool_t   *aPool,
                                         aclMemPoolStore  *aStore,
                                         void            **aAddr)
{
    aclMemPoolBlock *sBlock = NULL;
    aclMemPoolChunk *sChunk = NULL;
    acp_rc_t         sRC;

    /*
     * Free Chunk ȹ��
     */
    if (acpListIsEmpty(&aStore->mFreeChunkList) == ACP_TRUE)
    {
        /*
         * Free Chunk�� ������ Chunk �Ҵ�
         */
        sRC = aclMemPoolAllocChunk(aPool, aStore, &sChunk);

        if (ACP_RC_NOT_SUCCESS(sRC))
        {
            return sRC;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        sChunk = aStore->mFreeChunkList.mNext->mObj;
    }

    /*
     * Chunk���� Free Block ȹ��
     */
    if (sChunk->mLastFreeBlock == NULL)
    {
        ACE_ASSERT(sChunk->mInitBlockCount < aPool->mElementCount);

        sBlock = ACL_MEM_POOL_BLOCK_AT(aPool, sChunk, sChunk->mInitBlockCount);

        sBlock->mChunk = sChunk;

        sChunk->mInitBlockCount++;
    }
    else
    {
        sBlock                 = sChunk->mLastFreeBlock;
        sChunk->mLastFreeBlock = sBlock->mNextFreeBlock;
    }

    sBlock->mNextFreeBlock = NULL;

    sChunk->mFreeBlockCount--;

    /*
     * Chunk�� ���̻� Free Block�� ������ Store�� Free Chunk List���� ����
     */
    if (sChunk->mFreeBlockCount == 0)
    {
        acpListDeleteNode(&sChunk->mChunkListNode);
        acpListAppendNode(&aStore->mFullChunkList, &sChunk->mChunkListNode);
    }
    else
    {
        /* do nothing */
    }

    /*
     * Block���� User Memory Address �Ѱ���
     */
    *aAddr = ACL_MEM_POOL_USER_ADDR_FROM_BLOCK(sBlock);

    return ACP_RC_SUCCESS;
}

ACP_INLINE void aclMemPoolFreeBlock(acl_mem_pool_t  *aPool,
                                    aclMemPoolStore *aStore,
                                    aclMemPoolBlock *aBlock)
{
    aclMemPoolChunk *sChunk = aBlock->mChunk;
    aclMemPoolChunk *sChunkToFree = NULL;

    /*
     * Store�� Pool �Ҽ��� �´��� �˻�
     */
    if (aStore->mPool == aPool)
    {
        /* do nothing */
    }
    else
    {
        ACE_DASSERT(aStore->mPool == aPool);
        return;
    }

    /*
     * double-free �˻�
     */
    if (aBlock->mNextFreeBlock == NULL)
    {
        /* do nothing */
    }
    else
    {
        ACE_DASSERT(aBlock->mNextFreeBlock == NULL);
        return;
    }

    /*
     * Chunk�� Free Block List�� �߰�
     */
    aBlock->mNextFreeBlock = sChunk->mLastFreeBlock;
    sChunk->mLastFreeBlock = aBlock;

    /*
     * Chunk�� Free Block�� �ϳ��� �����ٸ� Store�� Free Chunk List�� �߰�
     */
    if (sChunk->mFreeBlockCount == 0)
    {
        acpListDeleteNode(&sChunk->mChunkListNode);
        acpListPrependNode(&aStore->mFreeChunkList, &sChunk->mChunkListNode);
    }
    else
    {
        /* do nothing */
    }

    /*
     * Chunk�� Free Block Count ����
     */
    sChunk->mFreeBlockCount++;

    /*
     * Chunk�� ��� Block�� Free Block�̸�
     */
    if (sChunk->mFreeBlockCount == aPool->mElementCount)
    {
        /*
         * Store�� Free Chunk List�� ������ Chunk ȹ��
         */
        sChunkToFree = aStore->mFreeChunkList.mPrev->mObj;

        if (sChunkToFree != sChunk)
        {
            /*
             * ȹ���� Chunk�� ���� Chunk�� �ƴϰ� ������ �� Chunk��� Chunk Free
             */
            if ((sChunkToFree != NULL) &&
                (sChunkToFree->mFreeBlockCount == aPool->mElementCount))
            {
                aclMemPoolFreeChunk(sChunkToFree);
            }
            else
            {
                /* do nothing */
            }

            /*
             * ���� Chunk�� Store�� Free Chunk List�� ���������� �̵�
             */
            acpListDeleteNode(&sChunk->mChunkListNode);
            acpListAppendNode(&aStore->mFreeChunkList, &sChunk->mChunkListNode);
        }
        else
        {
            /*
             * ȹ���� Chunk�� ���� Chunk��� �� �� ����
             */
        }
    }
    else
    {
        /* do nothing */
    }
}


/**
 * creates a memory pool
 * @param aPool pointer to the memory pool
 * @param aElementSize size of an element
 * @param aElementCount number of elements in an allocated memory chunk
 * @param aParallelFactor parallel factor
 * @return result code
 *
 * if @a aParallelFactor is zero,
 * it will maintain only one list of memory chunks
 * and it will assume single thread environment (it means not multithread safe).
 *
 * if @a aParallelFactor is greater than zero,
 * it will maintain @a aParallelFactor number of memory chunk lists
 * and it will use mutex for multithread safe.
 *
 * if @a aParallelFactor is less than zero,
 * it will take system CPU count to determine @a aParallelFactor.
 */
ACP_EXPORT acp_rc_t aclMemPoolCreate(acl_mem_pool_t *aPool,
                                     acp_size_t      aElementSize,
                                     acp_uint32_t    aElementCount,
                                     acp_sint32_t    aParallelFactor)
{
#if defined(ACP_CFG_MEMORY_CHECK)
    ACP_UNUSED(aParallelFactor);

    aPool->mElementSize    = aElementSize;
    aPool->mElementCount   = aElementCount;
    aPool->mParallelFactor = 0;
    aPool->mStore          = NULL;
#else
    acp_rc_t     sRC;
    acp_sint32_t sStoreCount;
    acp_sint32_t i;

    aPool->mElementSize  = aElementSize;
    aPool->mElementCount = aElementCount;

    if (aParallelFactor < 0)
    {
        sRC = acpSysGetCPUCount((acp_uint32_t *)&aPool->mParallelFactor);

        if (ACP_RC_NOT_SUCCESS(sRC))
        {
            return sRC;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        aPool->mParallelFactor = aParallelFactor;
    }

    sStoreCount = ACP_MAX(aPool->mParallelFactor, 1);

    sRC = acpMemAlloc((void **)&aPool->mStore,
                      sizeof(aclMemPoolStore) * sStoreCount);

    if (ACP_RC_NOT_SUCCESS(sRC))
    {
        return sRC;
    }
    else
    {
        /* do nothing */
    }

    for (i = 0; i < sStoreCount; i++)
    {
        aPool->mStore[i].mPool = aPool;

        acpSpinLockInit(&aPool->mStore[i].mLock, -1);

        acpListInit(&aPool->mStore[i].mFullChunkList);
        acpListInit(&aPool->mStore[i].mFreeChunkList);
    }
#endif

    return ACP_RC_SUCCESS;
}

/**
 * destroys a memory pool
 * @param aPool pointer to the memory pool
 *
 * it deallocates all memory allocated by the pool.
 */
ACP_EXPORT void aclMemPoolDestroy(acl_mem_pool_t *aPool)
{
#if defined(ACP_CFG_MEMORY_CHECK)
    ACP_UNUSED(aPool);
#else
    aclMemPoolStore *sStore = NULL;
    aclMemPoolChunk *sChunk = NULL;
    acp_list_node_t *sIterator = NULL;
    acp_list_node_t *sNextNode = NULL;
    acp_sint32_t     sStoreCount = ACP_MAX(aPool->mParallelFactor, 1);
    acp_sint32_t     i;

    for (i = 0; i < sStoreCount; i++)
    {
        sStore = &aPool->mStore[i];

        ACE_DASSERT(acpListIsEmpty(&sStore->mFullChunkList) == ACP_TRUE);

        ACP_LIST_ITERATE_SAFE(&sStore->mFreeChunkList, sIterator, sNextNode)
        {
            sChunk = sIterator->mObj;

            ACE_DASSERT(sChunk->mFreeBlockCount == aPool->mElementCount);

            aclMemPoolFreeChunk(sChunk);
        }

        ACE_DASSERT(acpListIsEmpty(&sStore->mFreeChunkList) == ACP_TRUE);
    }

    acpMemFree(aPool->mStore);
#endif
}

/**
 * allocates a memory block from the pool
 * @param aPool pointer to the memory pool
 * @param aAddr pointer to a variable to store allocated memory address
 * @return result code
 */
ACP_EXPORT acp_rc_t aclMemPoolAlloc(acl_mem_pool_t *aPool, void **aAddr)
{
    acp_rc_t sRC;

#if defined(ACP_CFG_MEMORY_CHECK)
    ACP_UNUSED(aPool);

    sRC = acpMemAlloc(aAddr, aPool->mElementSize);
#else
    acp_uint32_t sIndex;

    if (aPool->mParallelFactor > 0)
    {
        sIndex = acpThrGetParallelIndex() % aPool->mParallelFactor;

        acpSpinLockLock(&aPool->mStore[sIndex].mLock);
        sRC = aclMemPoolAllocBlock(aPool, &aPool->mStore[sIndex], aAddr);
        acpSpinLockUnlock(&aPool->mStore[sIndex].mLock);
    }
    else
    {
        sRC = aclMemPoolAllocBlock(aPool, aPool->mStore, aAddr);
    }
#endif

    return sRC;
}

/**
 * frees a memory block allocated from the pool
 * @param aPool pointer to the memory pool
 * @param aAddr pointer to the allocated memory
 */
ACP_EXPORT void aclMemPoolFree(acl_mem_pool_t *aPool, void *aAddr)
{
#if defined(ACP_CFG_MEMORY_CHECK)
    ACP_UNUSED(aPool);

    acpMemFree(aAddr);
#else
    aclMemPoolBlock *sBlock = NULL;

    /*
     * User Memory Address���� Block ȹ��
     */
    if (aAddr == NULL)
    {
        return;
    }
    else
    {
        sBlock = ACL_MEM_POOL_BLOCK_FROM_USER_ADDR(aAddr);

        if (aPool->mParallelFactor > 0)
        {
            acpSpinLockLock(&sBlock->mChunk->mStore->mLock);
            aclMemPoolFreeBlock(aPool, sBlock->mChunk->mStore, sBlock);
            acpSpinLockUnlock(&sBlock->mChunk->mStore->mLock);
        }
        else
        {
            aclMemPoolFreeBlock(aPool, sBlock->mChunk->mStore, sBlock);
        }
    }
#endif
}
