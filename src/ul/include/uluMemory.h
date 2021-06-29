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

#ifndef _O_ULU_MEMORY_H_
#define _O_ULU_MEMORY_H_  1

#include <acp.h>
#include <acl.h>
#include <aciTypes.h>

typedef enum uluMemoryType
{
    ULU_MEMORY_TYPE_NOT_SET,
    ULU_MEMORY_TYPE_MANAGER,
    ULU_MEMORY_TYPE_DIRECT,
    ULU_MEMORY_TYPE_MAX
} uluMemoryType;

typedef struct uluChunk         uluChunk;
typedef struct uluChunk_DIRECT  uluChunk_DIRECT;
typedef struct uluChunkPool     uluChunkPool;
typedef struct uluMemory        uluMemory;

/*
 * =============================
 * uluChunkPool
 * =============================
 */

typedef acp_uint32_t  uluChunkPoolGetRefCntFunc           (uluChunkPool *aPool);
typedef uluChunk     *uluChunkPoolGetChunkFunc            (uluChunkPool *aPool, acp_uint32_t aChunkSize);
typedef void          uluChunkPoolReturnChunkFunc         (uluChunkPool *aPool, uluChunk *aChunk);
typedef void          uluChunkPoolDestroyFunc             (uluChunkPool *aPool);
typedef ACI_RC        uluChunkPoolCreateInitialChunksFunc (uluChunkPool *aPool);

typedef struct uluChunkPoolOpSet
{
    uluChunkPoolCreateInitialChunksFunc *mCreateInitialChunks;

    uluChunkPoolDestroyFunc             *mDestroyMyself;

    uluChunkPoolGetChunkFunc            *mGetChunk;
    uluChunkPoolReturnChunkFunc         *mReturnChunk;

    uluChunkPoolGetRefCntFunc           *mGetRefCnt;

} uluChunkPoolOpSet;



struct uluChunkPool
{
    acp_list_t         mFreeChunkList;     /* Free Chunk ���� ����Ʈ */

    acp_list_t         mFreeBigChunkList;  /* default chunk size ���� ū ũ���� �޸𸮸� ������
                                              ûũ���� free list */

    acp_list_t         mMemoryList;        /* �� ûũ Ǯ�� �ν��Ͻ����� ûũ�� �Ҵ� �޾�
                                             ����ϰ� �ִ� uluMemory ���� ����Ʈ */

    acp_uint32_t       mMemoryCnt;         /* �� ûũ Ǯ�� �ҽ��� ��� uluMemory�� ���� */
    acp_uint32_t       mFreeChunkCnt;      /* Free Chunk �� ���� */
    acp_uint32_t       mFreeBigChunkCnt;   /* Free Big Chunk �� ���� */
    acp_uint32_t       mMinChunkCnt;       /* �ּҷ� ������ free chunk �� ���� */

    acp_uint32_t       mDefaultChunkSize;  /* �ϳ��� ûũ�� ����Ʈ ũ��  */
    acp_uint32_t       mActualSizeOfDefaultSingleChunk;

    acp_uint32_t       mNumberOfSP;        /* �� ûũ Ǯ�� �ִ� ûũ���� ������ ���̺� ����Ʈ�� ����.
                                              ûũ�� mSavepoints[] �迭��
                                              0 ���� mNumberOfSP+1 ������ ���Ҹ� ������.
                                              ������ ���� current offset. */

    uluChunkPoolOpSet *mOp;
};

/*
 * ���� �������� ����
 *
 * (ûũǮ ������ ����,����)             (�޸� �Ҵ�/������ ���� ����)
 *  uluChunk.mChunkSize ---------+           uluChunk.mFreeMemSize
 *                               |                            |
 *                         |_____v____________________________|________|
 *                         |                            ______v________|
 *                         |                           |               |
 *  +----------+----+------+-----------+-------------------------------+
 *  | uluChunk : SP |SP    | uluMemory | data1 | data2 |   f r e e     |
 *  |          : [0]|[1..n]|           |       |       |               |
 *  +----------+----+------+-----------+-------------------------------+
 *                                     |_______________________________|
 *                                          |
 *  uluMemory.mMaxAllocatableSize ----------+
 *  (uluMemory ������ ����.����)
 *
 * =====================================
 * The Location Of uluMemory in a Chunk
 * =====================================
 *
 *             +-mBuffer-------+
 *             |               |
 *             |               v
 * +-----------|---+-----------+-----------+---------------------
 * | uluChunk  |   | Savepoints| uluMemory |Actual
 * | {..mBuffer..} |           | {       } |Available Memory ...
 * |               |[0,1,...,n]|           |
 * +---------------+-|---------+-----------+---------------------
 *                   |                     ^
 *                   |                     |
 *                   +---------------------+
 *          mBuffer + mSavepoints[0]
 *
 * ���� ���� ������ ������ ûũ�� uluMemory �� Create �ϸ鼭 ���ʷ� ûũ Ǯ�κ���
 * �Ҵ�޴� ûũ ���̴�.
 *
 * �ٸ� ûũ�� �Ʒ��� ���� ������ ������ �ȴ� :
 *
 *             +---------------+
 *             |               |
 *             |              \|/
 * +-----------|---+-----------+---------------------
 * | uluChunk  |   | Savepoints|Actual
 * | {..mBuffer..} |           |Available Memory ...
 * |               |[0,1,...,n]|
 * +---------------+-----------+---------------------
 */

/*
 * =============================
 * uluMemory
 * =============================
 */

typedef uluMemory    *uluMemoryCreateFunc      (uluChunkPool *aSourcePool);
typedef ACI_RC        uluMemoryMallocFunc      (uluMemory *aMemory, void **aPtr, acp_uint32_t aSize);
typedef ACI_RC        uluMemoryFreeToSPFunc    (uluMemory *aMemory, acp_uint32_t aSavepointIndex);
typedef ACI_RC        uluMemoryMarkSPFunc      (uluMemory *aMemory);
typedef ACI_RC        uluMemoryDeleteLastSPFunc(uluMemory *aMemory);
typedef acp_uint32_t  uluMemoryGetCurrentSPFunc(uluMemory *aMemory);
typedef void          uluMemoryDestroyFunc     (uluMemory *aMemory);

typedef struct uluMemoryOpSet
{
    uluMemoryCreateFunc         *mCreate;
    uluMemoryDestroyFunc        *mDestroyMyself;

    uluMemoryMallocFunc         *mMalloc;
    uluMemoryFreeToSPFunc       *mFreeToSP;

    uluMemoryMarkSPFunc         *mMarkSP;

    uluMemoryDeleteLastSPFunc   *mDeleteLastSP;
    uluMemoryGetCurrentSPFunc   *mGetCurrentSPIndex;

} uluMemoryOpSet;



struct uluMemory
{
    acp_list_node_t   mList;              /* ���� chunk Pool �� uluMemory �� ����Ʈ */
    uluChunkPool     *mChunkPool;         /* uluMemory �� �޸𸮸� ������ �� ChunkPool */
    acp_list_t        mChunkList;         /* uluMemory �� �����ϰ� �ִ� Chunk�� ����Ʈ */

    acp_uint32_t      mCurrentSPIndex;    /* ���������� Mark �� SP �� �ε���.
                                             ó�� �޸𸮰� �����Ǹ� 0 �̴�.
                                             ó�� �޸� �����Ǿ��� �� ��ŷ�� 0 �� SP �ε�������
                                             uluMemory�� ����� ����.
                                             0 �� �ε����� uluMemory �� ����ִ� initial chunk��
                                             �Ǽ��� �ݳ��ϴ� ���� ���� ���� reserve �Ǿ� �ִ�. */

    acp_uint32_t      mAllocCount;        /* ��������� ���� �ʵ� : ���ϰ��� ���µ�, �׳� �ñ��ؼ� */
    acp_uint32_t      mFreeCount;

    acp_uint32_t      mMaxAllocatableSize;

    acp_list_node_t **mSPArray;           /* direct alloc �� ���� ����Ѵ� */

    uluMemoryOpSet   *mOp;

};

/*
 * ==============================
 * Exported Function Declarations
 * ==============================
 */

uluChunkPool *uluChunkPoolCreate(acp_uint32_t aDefaultChunkSize, acp_uint32_t aNumOfSavepoints, acp_uint32_t aMinChunkCnt);
ACI_RC        uluMemoryCreate(uluChunkPool *aSourcePool, uluMemory **aNewMem);

#endif /* _O_ULU_MEMORY_H_ */

