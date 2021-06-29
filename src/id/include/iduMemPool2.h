/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemPool2.h 32048 2009-04-08 06:35:00Z newdaily $
 **********************************************************************/

#ifndef _O_IDU_MEM_POOL2_H_
#define _O_IDU_MEM_POOL2_H_ 1

#include <idl.h>
#include <idu.h>
#include <iduMutex.h>
#include <iduList.h>

#if defined (VC_WINCE)
#include <WinCE_Assert.h>
#endif /*VC_WINCE*/

struct iduMemPool2ChunkHeader;
struct iduMemPool2FreeInfo;
class iduMemPool2;

/* Free Block List ������ ��� */
typedef struct iduMemPool2FreeInfo
{
    iduMemPool2FreeInfo    *mNext;
    SChar                  *mBlock;
    iduMemPool2ChunkHeader *mMyChunk;
} iduMemPool2FreeInfo;


/* i��° Block�� Free Info�� �����´�. */
#define IDU_MEMPOOL2_BLOCK_FREE_INFO( aFreeChunk, i ) \
( (iduMemPool2FreeInfo*)( (SChar*)(aFreeChunk) + (aFreeChunk)->mChunkSize - ID_SIZEOF( iduMemPool2FreeInfo ) * (i + 1) ) )

/* Chunk�� ù��° Block�� Chunk�������� ��ġ */
#define IDU_MEMPOOL2_FST_BLOCK( aFreeChunk ) \
( (SChar*)idlOS::align( (SChar*)(aFreeChunk) + ID_SIZEOF( iduMemPool2ChunkHeader ), (aFreeChunk)->mAlignSize ) )

/* Chunk�� n��° Block */
#define IDU_MEMPOOL2_NTH_BLOCK( aFreeChunk, n ) ( IDU_MEMPOOL2_FST_BLOCK( aFreeChunk ) + aFreeChunk->mBlockSize * (n) )

/*
                   Chunk ����
   *------------------------------------------*
   |         iduMemPool2ChunkHeader           |
   |------------------------------------------|
   |                                          |
   |         Block1, Block2 ... Block n       |
   |                                          |
   |------------------------------------------|
   |        iduMemPool2FreeInfo List          |
   *------------------------------------------*
*/
typedef struct iduMemPool2ChunkHeader
{
    /* �Ҵ�� Chunk List */
    iduList                 mChunkNode;

    /* Chunk Matrix�� ���� Linked List */
    iduList                 mMtxNode;

    /* Chunk�� ������ �ִ� iduMemPool2�� ���� Pointer */
    iduMemPool2            *mOwner;

    /* Chunk Size */
    UInt                    mChunkSize;

    /* Chunk���� �ѹ��̶� �Ҵ��� Block�� ���� */
    UInt                    mNewAllocCnt;

    /* Chunk�� ���� �� �ִ� Total Block �� */
    UInt                    mTotalBlockCnt;

    /* Alloc�� Ƚ�� */
    UInt                    mFreeBlockCnt;

    /* Free�� Block�� ���� ���� �۾�����. */
    UInt                    mFullness;

    /* ���������� Free�� Block�� FreeInfo�� ����Ű�� �ִ�. */
    iduMemPool2FreeInfo*    mFstFreeInfo;

    /* Block Size */
    UInt                    mBlockSize;

    /* �� Block�� Alignũ��. */
    UInt                    mAlignSize;
} iduMemPool2ChunkHeader;


/*******************************************************************
      iduMemPool2 ������

      |-This - Child 1 |
      |-Child 2        |
      |-Child 3        |  alloc
      |-Child 4        |  ---->  Client
      |-Child 5        |  free
      |- ...           |  <----
      *-Child n        |

      Child 1�� �ڱ��ڽ��̴�. 
      Child 1�� ������ ������ MemPool���� ���� �����Ѵ�.

      Fullness: Chunk�� �󸶳� ���� Block�� �Ҵ��ߴ����� ����
                Lev�� ���Ѱ����� Ŭ���� ���� Block�� �Ҵ��� ���̴�.

      Matrix[n]: �� Fullness Level�� ���� Chunk List�� �����ߴ�.

      �Ҵ�� Fullness�� ���� Chunk�� ���� �Ҵ��ϱ� ���� Matrix��
      n�� ���� ū ������ ���� ������ ���鼭 Chunk�� �ִ��� �����Ѵ�.

********************************************************************/

class iduMemPool2
{
private:
    /**
     * The number of groups of blocks we maintain based on what
     * fraction of the superblock is empty. NB: This number must be at
     * least 2, and is 1 greater than the EMPTY_FRACTION.
    */
    enum { BLOCK_FULLNESS_GROUP = 9 };

    /**
     * A thread heap must be at least 1/EMPTY_FRACTION empty before we
     * start returning blocks to the process heap.
     */
    enum { EMPTY_FRACTION = BLOCK_FULLNESS_GROUP - 1 };

    /** Reset value for the least-empty bin.  The last bin
     * (BLOCK_FULLNESS_GROUP-1) is for completely full blocks,
     * so we use the next-to-last bin.
     */
    enum { RESET_LEAST_EMPTY_BIN = BLOCK_FULLNESS_GROUP - 2 };

public:
    iduMemPool2(){};
    ~iduMemPool2(){};

    IDE_RC initialize( iduMemoryClientIndex aIndex,
                       SChar               *aName,
                       UInt                 aChildCnt,
                       UInt                 aBlockSize,
                       UInt                 aBlockCntInChunk,
                       UInt                 aChildCacheCnt,
                       UInt                 aAlignSize );

    IDE_RC destroy();

    IDE_RC memAlloc( void **aHandle, void **aAllocMemPtr );

    IDE_RC memFree( void* aFreeMemPtr );

    IDE_RC init( iduMemoryClientIndex aIndex,
                 SChar               *aName,
                 UInt                 aBlockSize,
                 UInt                 aBlockCntInChunk,
                 UInt                 aChildCacheCnt,
                 UInt                 aAlignSize );

    IDE_RC dest();

    IDE_RC alloc( void **aHandle,
                  void** aAllocMemPtr );

    void  status();
    ULong getMemSize();
    UInt  getBlockCnt4Chunk() { return mTotalBlockCntInChunk; }
    UInt  getApproximateFBCntByFN( UInt aTh ) { return mApproximateFBCntByFN[ aTh ]; }
    UInt  getFullnessCnt() { return BLOCK_FULLNESS_GROUP; }

/* For Testing */
public:
    void dumpMemPool4UnitTest();
    void dumpChunk4UnitTest( iduMemPool2ChunkHeader *aChunkHeader );

private:
    IDE_RC findAvailableChunk( iduMemPool2ChunkHeader  **aChunkPtr );

    IDE_RC checkAndFreeChunk( iduMemPool2ChunkHeader *aChunk );

    inline void initChunk( iduMemPool2ChunkHeader *aChunk );

    inline IDE_RC lock();
    inline IDE_RC unlock();

    iduMemPool2ChunkHeader* getFreeChunk();
    inline void addChunkToList( iduMemPool2ChunkHeader* aChunk );
    inline void removeChunkFromList( iduMemPool2ChunkHeader* aChunk );
    inline void removeChunkFromMP( iduMemPool2ChunkHeader* aChunk );
    inline void moveChunkToMP( iduMemPool2ChunkHeader* aChunk );

    inline UInt computeFullness( UInt aTotal, UInt aAvailable );
    inline iduMemPool2ChunkHeader* removeMaxChunk();

    inline void addBlkFreeInfo2FreeLst( iduMemPool2FreeInfo *aCurFreeInfo );
    inline void updateFullness( iduMemPool2ChunkHeader* aChunk );
    inline ULong getFreeBlockCnt();

private:
    iduMutex               mLock;

    /* MemPool List Count */
    UInt                   mPoolCnt;

    /* �Ҵ���� Chunk�� ����. */
    ULong                  mChunkCnt;

    /* Chunk ũ��. */
    UInt                   mChunkSize;

    /* Block ũ��. */
    UInt                   mBlockSize;

    /*
      Child�� mChildCacheCnt�̻��� Free Block Count��
      ������ �Ǹ� ���帹�� Free Block�� ���� Chunk�� Parent����
      ��ȯ�Ѵ�.
    */
    ULong                  mCacheBlockCnt;

    /* �ϳ��� Chunk�� ������ �� Block�� */
    UInt                   mTotalBlockCntInChunk;

    /*
      Chunk�� mFullness�� ���� mCHMatrix�� �Ŵ޷� �ִµ� �� ���
      mFullness�� ���� �Ŵ޷� �ִ� Chunk�� ����
    */
    UInt                   mFreeChkCntOfMatrix[ BLOCK_FULLNESS_GROUP ];

    /* Chunk�� mFullness�� ���� ���� �� �ִ� �뷫�� Free Block ���� */
    UInt                   mApproximateFBCntByFN[ BLOCK_FULLNESS_GROUP ];

    /* Chunk Header Matrix
       Chunk�� mFullness�� ���� mCHMarix[mFullness]�� �߰��� */
    iduList                mCHMatrix[ BLOCK_FULLNESS_GROUP ];

    /* �Ҵ�� Chunk List Header */
    iduList                mChunkList;

    /* Free Chunk�� ã���� mCHMatrix���� mLeastBinIndex��
     * ����Ű�� Chunk List���� ã�´�. �� ���� Free Block��
     * ������ ���� Chunk List�� �ǵ��� ����Ű���� �Ѵ�. */
    SInt                   mLeastBinIndex;

    iduMemoryClientIndex   mIndex;

    /* MemPool List */
    iduMemPool2           *mMemMgrList;

    UInt                   mAlignSize;

    /* �Ҵ�� Block ���� */
    ULong                  mAllocBlkCnt;
};

inline IDE_RC iduMemPool2::lock()
{
    return mLock.lock( NULL /* idvSQL* */ );
}

inline IDE_RC iduMemPool2::unlock()
{
    return mLock.unlock();
}

/* Chunk�� �ʱ�ȭ �մϴ�. */
inline void iduMemPool2::initChunk( iduMemPool2ChunkHeader *aChunk )
{
    IDU_LIST_INIT_OBJ( &aChunk->mChunkNode, aChunk );
    IDU_LIST_INIT_OBJ( &aChunk->mMtxNode, aChunk );

    aChunk->mOwner         = this;
    aChunk->mChunkSize     = mChunkSize;
    aChunk->mBlockSize     = mBlockSize;
    aChunk->mNewAllocCnt   = 0;
    aChunk->mTotalBlockCnt = mTotalBlockCntInChunk;
    aChunk->mFreeBlockCnt  = mTotalBlockCntInChunk;
    aChunk->mFullness      = 0;

    aChunk->mFstFreeInfo   = NULL;
    aChunk->mAlignSize     = mAlignSize;
}

/* List�� ���� Chunk�� Add�Ѵ�. */
inline void iduMemPool2::addChunkToList( iduMemPool2ChunkHeader* aChunk )
{
    IDU_LIST_ADD_AFTER( &mChunkList, &aChunk->mChunkNode );
}

/* List���� Chunk�� �����Ѵ�. */
inline void iduMemPool2::removeChunkFromList( iduMemPool2ChunkHeader* aChunk )
{
    IDU_LIST_REMOVE( &aChunk->mChunkNode );
}

/* Matrix���� Chunk�� �����Ѵ�. */
inline void iduMemPool2::removeChunkFromMP( iduMemPool2ChunkHeader* aChunk )
{
    IDU_LIST_REMOVE( &aChunk->mMtxNode );
}

/* Chunk�� �ڽ��� Fullness�´� Matrix�� �߰��Ѵ�. */
inline void iduMemPool2::moveChunkToMP( iduMemPool2ChunkHeader* aChunk )
{
    iduList *sCHListOfMatrix;
    SInt     sFullness = aChunk->mFullness;

    removeChunkFromMP( aChunk );

    sCHListOfMatrix = mCHMatrix + sFullness;

    IDU_LIST_ADD_AFTER( sCHListOfMatrix, &aChunk->mMtxNode );
}

/* Fullness�� ����Ѵ� */
inline UInt iduMemPool2::computeFullness( UInt aTotal, UInt aAvailable )
{
    return (((BLOCK_FULLNESS_GROUP - 1)
             * (aTotal - aAvailable)) / aTotal);
}

/* Block�� Free Info�� Free Info List�� �߰��Ѵ�. */
inline void iduMemPool2::addBlkFreeInfo2FreeLst( iduMemPool2FreeInfo *aFreeInfo )
{
    iduMemPool2ChunkHeader *sChunk;

    IDE_ASSERT( aFreeInfo->mNext == NULL );

    sChunk = aFreeInfo->mMyChunk;

    aFreeInfo->mNext = sChunk->mFstFreeInfo;
    sChunk->mFstFreeInfo = aFreeInfo;

    sChunk->mFreeBlockCnt++;
}

/* Chunk�� Fullness���� �����Ѵ�. */
inline void iduMemPool2::updateFullness( iduMemPool2ChunkHeader* aChunk )
{
    SInt sOldFullness;
    SInt sNewFullness;

    sOldFullness = aChunk->mFullness;
    sNewFullness = computeFullness( aChunk->mTotalBlockCnt, aChunk->mFreeBlockCnt );

    if( sOldFullness != sNewFullness )
    {
        mFreeChkCntOfMatrix[ aChunk->mFullness ]--;
        aChunk->mFullness = sNewFullness;
        moveChunkToMP( aChunk );
        mFreeChkCntOfMatrix[ sNewFullness ]++;

        if( ( mLeastBinIndex < sNewFullness ) &&
            ( mLeastBinIndex != RESET_LEAST_EMPTY_BIN ) )
        {
            mLeastBinIndex = sNewFullness;
        }
    }

}

/* ���� MemPool�� Free Block�� ������ ���Ѵ�. */
inline ULong iduMemPool2::getFreeBlockCnt()
{
    return mTotalBlockCntInChunk * mChunkCnt - mAllocBlkCnt;
}

/* ���� MemPool�� ������� Memoryũ�⸦ ���Ѵ� .*/
inline ULong iduMemPool2::getMemSize()
{
    return mChunkCnt * mChunkSize;
}

#endif  // _O_MEM_PORTAL_H_
