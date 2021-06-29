/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemPool2.cpp 66405 2014-08-13 07:15:26Z djin $
 **********************************************************************/

#include <idl.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <iduMemPool2.h>

/*******************************************************************
 * Description: MemPool List�ʱ�ȭ
 *
 *  aIndex           - [IN] iduMemPool2�� ���� Client Index
 *  aName            - [IN] iduMemPool2�� �̸�
 *  aChildCnt        - [IN] MemPool Cnt
 *  aBlockSize       - [IN] Block�� ũ��
 *  aBlockCntInChunk - [IN] Chunk���� Block�� ����
 *  aCacheSize       - [IN] MemPool�� �� ũ���� �޸𸮸� ����. ������
 *                          �޸𸮴� OS���� ��ȯ�Ѵ�.
 *  aAlignSize       - [IN] "�� Block�� ũ��"�� "Block�� ���� �ּ�"
 *                          �� ũ��� Align�ȴ�.
 ********************************************************************/
IDE_RC iduMemPool2::initialize( iduMemoryClientIndex aIndex,
                                SChar               *aName,
                                UInt                 aChildCnt,
                                UInt                 aBlockSize,
                                UInt                 aBlockCntInChunk,
                                UInt                 aCacheSize,
                                UInt                 aAlignSize )
{
    UInt         i;
    iduMemPool2 *sCurMemPool2;

    IDE_ASSERT( aChildCnt  != 0 );
    IDE_ASSERT( aBlockSize != 0 );
    IDE_ASSERT( aAlignSize != 0 );

    /* 1�� �̻��� MemPool������ ���ϰ� �Ǹ� ���� MemPool���� �ٸ�
       MemPool�� �����Ѵ�. */
    IDE_TEST( init( aIndex,
                    aName,
                    aBlockSize,
                    aBlockCntInChunk,
                    aCacheSize / aChildCnt,
                    aAlignSize ) != IDE_SUCCESS );

    if( aChildCnt > 1 )
    {
        IDE_TEST( iduMemMgr::malloc( aIndex,
                                     ID_SIZEOF( iduMemPool2 ) * ( aChildCnt - 1 ),
                                     (void**)&mMemMgrList )
                  != IDE_SUCCESS );

        for( i = 0; i < aChildCnt - 1; i++ )
        {
            sCurMemPool2 = mMemMgrList + i;

#ifdef __CSURF__
            IDE_ASSERT( sCurMemPool2 != NULL );           
 
            new (sCurMemPool2) iduMemPool2;
#else
            sCurMemPool2 = new (sCurMemPool2) iduMemPool2;
#endif

            IDE_TEST( sCurMemPool2->init( aIndex,
                                          aName,
                                          aBlockSize,
                                          aBlockCntInChunk,
                                          aCacheSize / aChildCnt,
                                          aAlignSize )
                      != IDE_SUCCESS );
        }
    }

    /* init�Լ����� �̰��� 0���� �ʱ�ȭ �ϱ⶧���� �ݵ��
       ���Ŀ� �̰��� �ʱ�ȭ �ؾ��� */
    mPoolCnt = aChildCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: �Ҵ�� Chunk�� iduMemMgr���� ��ȯ�ϰ� �Ҵ�� Resource
 *              �� �����Ѵ�.
*******************************************************************/
IDE_RC iduMemPool2::destroy()
{
    UInt           i;
    iduMemPool2   *sCurMemPool2;

    for( i = 0; i < mPoolCnt - 1; i++ )
    {
        sCurMemPool2 = mMemMgrList + i;

        IDE_TEST( sCurMemPool2->dest() != IDE_SUCCESS );
    }

    IDE_TEST( dest() != IDE_SUCCESS );

    if( mPoolCnt > 1 )
    {
        IDE_TEST( iduMemMgr::free( mMemMgrList ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: Parent, Child�� �������� �ʱ�ȭ �Լ��̴�. ������
 *              Member�� �ʱ�ȭ �Ѵ�.
 *
 *  aIndex           - [IN] iduMemPool2�� ���� Client Index
 *  aName            - [IN] iduMemPool2�� �̸�
 *  aBlockSize       - [IN] Block�� ũ��
 *  aBlockCntInChunk - [IN] Chunk���� Block�� ����
 *  aCacheSize       - [IN] MemPool�� �� ũ���� �޸𸮸� ����. ������
 *                     �޸𸮴� OS���� ��ȯ�Ѵ�.
 *  aAlignSize       - [IN] "�� Block�� ũ��"�� "Block�� ���� �ּ�"
 *                     �� ũ��� Align�ȴ�.
 *******************************************************************/
IDE_RC iduMemPool2::init( iduMemoryClientIndex aIndex,
                          SChar               *aName,
                          UInt                 aBlockSize,
                          UInt                 aBlockCntInChunk,
                          UInt                 aCacheSize,
                          UInt                 aAlignSize )
{
    SInt      i;
    SFloat    sFreePercent;

    IDE_TEST( mLock.initialize( aName,
                                IDU_MUTEX_KIND_NATIVE,
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    /* UMR�� ����� ���� ���� */
    mPoolCnt       = 0;
    mChunkCnt      = 0;
    mLeastBinIndex = RESET_LEAST_EMPTY_BIN;
    mAllocBlkCnt   = 0;

    mBlockSize  = idlOS::align( aBlockSize, aAlignSize );
    mChunkSize  = idlOS::align8( ID_SIZEOF( iduMemPool2ChunkHeader ) ) + aAlignSize +
        aBlockCntInChunk * ( mBlockSize + ID_SIZEOF( iduMemPool2FreeInfo ) );
    mTotalBlockCntInChunk = aBlockCntInChunk;

    mIndex = aIndex;

    mCacheBlockCnt  = aCacheSize / mBlockSize;
    mAlignSize      = aAlignSize;

    IDU_LIST_INIT( &mChunkList );

    sFreePercent = 1.0 / ( BLOCK_FULLNESS_GROUP + 1 );

    /* Initialize Matrix */
    for( i = 0; i < BLOCK_FULLNESS_GROUP; i++ )
    {
        IDU_LIST_INIT( mCHMatrix + i );

        mFreeChkCntOfMatrix[i] = 0;
        mApproximateFBCntByFN[i]  = (UInt)( mTotalBlockCntInChunk * ( 1 - sFreePercent * ( i + 1 ) ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: ���� �Ҵ�� ��� Chunk�� OS�� ��ȯ�Ѵ�.
 *******************************************************************/
IDE_RC iduMemPool2::dest()
{
    iduList *sCurNode;
    iduList *sNxtNode;

    IDU_LIST_ITERATE_SAFE( &mChunkList, sCurNode, sNxtNode )
    {
       IDE_ASSERT( mChunkCnt != 0 );

       mChunkCnt--;

       IDE_ASSERT( sCurNode->mObj != NULL );

       IDE_TEST( iduMemMgr::free( sCurNode->mObj ) != IDE_SUCCESS );
    }

    IDE_ASSERT( mChunkCnt == 0 );

    IDE_TEST( mLock.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: �������� MemPool�߿��� �ϳ��� ��� �Ҵ��� ��û�Ѵ�.
 *              �Ҵ�� �ϳ��� Block�� �Ҵ�ȴ�.
 *
 * aMemHandle    - [OUT] �Ҵ�� Memory�� Handle�� �����ȴ�.
 * aAllocMemPtr  - [OUT] �Ҵ�� Memory�� �����ּҰ� �����ȴ�.
 *******************************************************************/
IDE_RC iduMemPool2::memAlloc( void **aMemHandle, void **aAllocMemPtr )
{
    UInt         sChildNo;
    iduMemPool2 *sAllocMemPool;

    IDE_ASSERT( mPoolCnt != 0 );

    sChildNo  = idlOS::getParallelIndex() % mPoolCnt;

    if( sChildNo == 0 )
    {
        sAllocMemPool = this;
    }
    else
    {
        sAllocMemPool = mMemMgrList + sChildNo - 1;
    }

    IDE_TEST( sAllocMemPool->alloc( aMemHandle, aAllocMemPtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: �ϳ��� Block�Ҵ��� ��û�Ѵ�.
 *
 * aMemHandle    - [OUT] �Ҵ�� Memory�� Handle�� �����ȴ�.
 * aAllocMemPtr  - [OUT] �Ҵ�� Memory�� �����ּҰ� �����ȴ�.
 *******************************************************************/
IDE_RC iduMemPool2::alloc( void **aMemHandle,
                           void **aAllocMemPtr )
{
    SInt                    sState = 0;
    SChar                  *sBlockPtr;
    iduMemPool2ChunkHeader *sFreeChunk;
    iduMemPool2FreeInfo    *sCurFreeInfo;

    *aMemHandle      = NULL;

    IDE_ASSERT( aAllocMemPtr != NULL );

    *aAllocMemPtr = NULL;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( findAvailableChunk( &sFreeChunk ) != IDE_SUCCESS );

    IDE_ASSERT( sFreeChunk->mOwner == this );

    IDE_ASSERT( sFreeChunk->mFreeBlockCnt != 0 );

    if( sFreeChunk->mNewAllocCnt != sFreeChunk->mTotalBlockCnt )
    {
        /* ���� �ѹ��� Alloc���� ���� Free Block �� ���� */
        sBlockPtr    = IDU_MEMPOOL2_NTH_BLOCK( sFreeChunk, sFreeChunk->mNewAllocCnt );
        sCurFreeInfo = IDU_MEMPOOL2_BLOCK_FREE_INFO( sFreeChunk, sFreeChunk->mNewAllocCnt );

        sFreeChunk->mNewAllocCnt++;

        sCurFreeInfo->mMyChunk = sFreeChunk;
        sCurFreeInfo->mNext    = NULL;
        sCurFreeInfo->mBlock   = sBlockPtr;
    }
    else
    {
        /* Free Chunk List���� Block�� �Ҵ��Ѵ�. */
        IDE_ASSERT( sFreeChunk->mFstFreeInfo != NULL );

        sCurFreeInfo = sFreeChunk->mFstFreeInfo;

        sFreeChunk->mFstFreeInfo = sCurFreeInfo->mNext;
        sCurFreeInfo->mNext = NULL;

        IDE_ASSERT( sCurFreeInfo->mMyChunk == sFreeChunk );

        sBlockPtr = sCurFreeInfo->mBlock;
    }

    sFreeChunk->mFreeBlockCnt--;

    updateFullness( sFreeChunk );

    *aAllocMemPtr = sBlockPtr;
    *aMemHandle   = sCurFreeInfo;

    mAllocBlkCnt++;

    sState = 0;
    IDE_TEST(unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    *aAllocMemPtr = NULL;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: aMemHandle�� ����Ű�� Block�� Chunk�� Free�Ѵ�.
 *
 * aMemHandle - [IN] �Ҵ�� Memory Block�� ���� Handle�̴�. aMemHandle��
 *                   ������ iduMemPool2FreeInfo�� ���� Pointer�̴�.
*******************************************************************/
IDE_RC iduMemPool2::memFree( void *aMemHandle )
{
    iduMemPool2FreeInfo    *sBlockFreeInfo;
    iduMemPool2ChunkHeader *sCurChunk;
    iduMemPool2            *sOwner;
    SInt                    sState = 0;

    IDE_ASSERT( aMemHandle != NULL );

    sBlockFreeInfo = (iduMemPool2FreeInfo*)aMemHandle;

    sCurChunk = sBlockFreeInfo->mMyChunk;
    sOwner    = sCurChunk->mOwner;

    IDE_TEST( sOwner->lock() != IDE_SUCCESS );
    sState = 1;

    sOwner->addBlkFreeInfo2FreeLst( sBlockFreeInfo );

    sOwner->updateFullness( sCurChunk );

    sOwner->mAllocBlkCnt--;

    if( sCurChunk->mTotalBlockCnt == sCurChunk->mFreeBlockCnt )
    {
        IDE_TEST( sOwner->checkAndFreeChunk( sCurChunk )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sOwner->unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sOwner->unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: Free Block�� ������ �ִ� Chunk�� ã�� �ش�. �̶�
 *              �������� Free Chunk�� �����Ѵٸ� ���� �Ҵ��� ����
 *              �� Chunk�� �ش�. �̷��� �����ν� Chunk�� ��� Block
 *              �� Free�Ǿ� OS�� �ش� Chunk�� Free�� Ȯ���� ���δ�.
 *
 * aChunkPtr - [OUT] �Ҵ�� Chunk�� ���� Pointer
*******************************************************************/
IDE_RC iduMemPool2::findAvailableChunk( iduMemPool2ChunkHeader  **aChunkPtr )
{
    iduMemPool2ChunkHeader *sNewChunkPtr;
    iduMemPool2ChunkHeader *sCurChunkPtr;
    SInt                    sNewFullness;

    sCurChunkPtr = NULL;

    sCurChunkPtr = getFreeChunk();

    if( sCurChunkPtr == NULL )
    {
        sCurChunkPtr = NULL;
        sNewChunkPtr = NULL;

        IDE_TEST( iduMemMgr::malloc( mIndex,
                                     mChunkSize,
                                     (void**)&sNewChunkPtr )
                  != IDE_SUCCESS);

        initChunk( sNewChunkPtr );

        addChunkToList( sNewChunkPtr );

        sCurChunkPtr = sNewChunkPtr;

        sNewFullness = computeFullness( sCurChunkPtr->mTotalBlockCnt,
                                        sCurChunkPtr->mTotalBlockCnt );

        sCurChunkPtr->mFullness = sNewFullness;

        mChunkCnt++;

        moveChunkToMP( sCurChunkPtr );

        mFreeChkCntOfMatrix[ sCurChunkPtr->mFullness ]++;
    }
    else
    {
        IDE_ASSERT( sCurChunkPtr->mOwner == this );
    }

    *aChunkPtr = sCurChunkPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aChunkPtr = NULL;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: ���� MemPool�� Free Block������ Cache Block Count����
 *              ������ ������ ��( Chunk���� ��� Block�� Free��) Chunk
 *              �� Free��Ų��.
 *
 * aChunk - [IN] Free�� Chunk Pointer
*******************************************************************/
IDE_RC iduMemPool2::checkAndFreeChunk( iduMemPool2ChunkHeader *aChunk )
{
    IDE_ASSERT( aChunk->mTotalBlockCnt == aChunk->mFreeBlockCnt );

    if( getFreeBlockCnt() > mCacheBlockCnt )
    {
        removeChunkFromList( aChunk );
        removeChunkFromMP( aChunk );

        IDE_ASSERT( aChunk->mFullness == 0 );
        IDE_ASSERT( mChunkCnt != 0 );

        mFreeChkCntOfMatrix[ aChunk->mFullness ]--;
        mChunkCnt--;

        IDE_TEST( iduMemMgr::free( aChunk ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Description: ���� Block�� ���� Chunk�� ã�´�. �̶� ������
 *              ���� �Ҵ��� Chunk�� ���� ã�´�. ���� Free Block�� ��
 *              �� Chunk�� ���ٸ� NULL�� Return�Ѵ�.
*******************************************************************/
iduMemPool2ChunkHeader* iduMemPool2::getFreeChunk()
{
    SInt                    i;
    iduList                *sCurList;
    iduMemPool2ChunkHeader *sCurChunkPtr = NULL;

    IDE_ASSERT( mLeastBinIndex <= RESET_LEAST_EMPTY_BIN );

    for( i = mLeastBinIndex; i >= 0; i-- )
    {
        sCurList       = mCHMatrix + i;
        mLeastBinIndex = i;

        if( IDU_LIST_IS_EMPTY( sCurList ) == ID_FALSE )
        {
            sCurList = IDU_LIST_GET_NEXT( sCurList );
            sCurChunkPtr = (iduMemPool2ChunkHeader*)( sCurList->mObj );
            break;
        }
    }

    return sCurChunkPtr;
}

/*******************************************************************
 * Description: MemPool�� ����ϰ� �ִ� Memory������ �ѷ��ش�.
*******************************************************************/
void iduMemPool2::status()
{
    SChar sBuffer[IDE_BUFFER_SIZE];

    idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer), "  - Memory Pool Size:%"ID_UINT64_FMT"\n",
                     getMemSize() );

    IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);
}

/*******************************************************************
 * Description: MemPool�� ���������� stdout���� �ѷ��ش�.
*******************************************************************/
void iduMemPool2::dumpMemPool4UnitTest()
{
    iduMemPool2ChunkHeader *sChunkHeader;
    UInt                    i;
    iduList                *sCurNode;

    idlOS::printf( "====== MemPool2 Info ======= \n" );
    idlOS::printf( "# Member Variable  \n" );
    idlOS::printf( "Pool  Count: %"ID_UINT32_FMT" \n"
                   "Chunk Count: %"ID_UINT64_FMT" \n"
                   "Chunk Size : %"ID_UINT32_FMT" \n"
                   "Block Size : %"ID_UINT32_FMT" \n"
                   "Cache Block Count: %"ID_UINT64_FMT" \n"
                   "Total Block Count In Chunk: %"ID_UINT32_FMT" \n"
                   "Align Size : %"ID_UINT32_FMT" \n"
                   "Alloc Block Count: %"ID_UINT32_FMT"\n"
                   "Pool  Count: %"ID_UINT64_FMT" \n",
                   mPoolCnt,
                   mChunkCnt,
                   mChunkSize,
                   mBlockSize,
                   mCacheBlockCnt,
                   mTotalBlockCntInChunk,
                   mAlignSize,
                   mAllocBlkCnt );

    idlOS::printf( "\n# Martrix Info \n" );
    idlOS::printf( "BLOCK_FULLNESS_GROUP Count: %"ID_INT32_FMT" \n"
                   "LeastBinIndex: %"ID_INT32_FMT" \n",
                   BLOCK_FULLNESS_GROUP,
                   mLeastBinIndex );

    for( i = 0; i < BLOCK_FULLNESS_GROUP; i++ )
    {
        idlOS::printf( "Free Chunk Count: %"ID_UINT32_FMT", "
                       "Approxi Free Block Count: %"ID_UINT32_FMT"\n",
                       mFreeChkCntOfMatrix[i],
                       mApproximateFBCntByFN[i] );

        IDU_LIST_ITERATE( mCHMatrix + i, sCurNode )
        {
            sChunkHeader = (iduMemPool2ChunkHeader*)( sCurNode->mObj );
            dumpChunk4UnitTest( sChunkHeader );
        }
    }
}

/*******************************************************************
 * Description: Chunk�� ������ stdout�� �ѷ��ش�.
*******************************************************************/
void iduMemPool2::dumpChunk4UnitTest( iduMemPool2ChunkHeader *aChunkHeader )
{
    UInt                   i;
    iduMemPool2FreeInfo   *sCurFreeInfo;

    idlOS::printf( "## Chunk Header\n" );

    idlOS::printf( "Chunk Size : %"ID_UINT32_FMT"\n"
                   "New Alloc Count: %"ID_UINT32_FMT"\n"
                   "Total Block Count: %"ID_UINT32_FMT"\n"
                   "Free  Block Count: %"ID_UINT32_FMT"\n"
                   "Fullness: %"ID_UINT32_FMT"\n"
                   "Block Size: %"ID_UINT32_FMT"\n"
                   "Align Size: %"ID_UINT32_FMT"\n",
                   aChunkHeader->mChunkSize,
                   aChunkHeader->mNewAllocCnt,
                   aChunkHeader->mTotalBlockCnt,
                   aChunkHeader->mFreeBlockCnt,
                   aChunkHeader->mFullness,
                   aChunkHeader->mBlockSize,
                   aChunkHeader->mAlignSize );

    idlOS::printf( "## Chunk Body\n" );

    sCurFreeInfo = aChunkHeader->mFstFreeInfo;

    for( i = 0 ; i < aChunkHeader->mFreeBlockCnt; i++ )
    {
        IDE_ASSERT( sCurFreeInfo != NULL );

        IDE_ASSERT( sCurFreeInfo->mMyChunk == aChunkHeader );

        sCurFreeInfo = sCurFreeInfo->mNext;
    }
}
