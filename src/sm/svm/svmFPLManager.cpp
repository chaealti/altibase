/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smu.h>
#include <smuUtility.h>
#include <svmReq.h>
#include <svmManager.h>
#include <svmDatabase.h>
#include <svmFPLManager.h>
#include <svmExpandChunk.h>
#include <sctTableSpaceMgr.h>


/*======================================================================
 *
 *  PUBLIC MEMBER FUNCTIONS
 *
 *======================================================================*/

iduMutex svmFPLManager::mGlobalAllocChunkMutex;

/*
    Static Initializer
 */
IDE_RC svmFPLManager::initializeStatic()
{
    IDE_TEST( mGlobalAllocChunkMutex.initialize(
                                         (SChar*)"SVM_GLOBAL_ALLOC_CHUNK_MUTEX",
                                         IDU_MUTEX_KIND_POSIX,
                                         IDV_WAIT_INDEX_NULL)
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Static Destroyer
 */
IDE_RC svmFPLManager::destroyStatic()
{
    IDE_TEST( mGlobalAllocChunkMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
 * Free Page List �����ڸ� �ʱ�ȭ�Ѵ�.
 */
IDE_RC svmFPLManager::initialize( svmTBSNode * aTBSNode )
{
    UInt i ;
    SChar sMutexName[128];

    idlOS::memset(&aTBSNode->mMemBase, 0, ID_SIZEOF(svmMemBase));

    // �� Free Page List�� Mutex�� �ʱ�ȭ
    /* svmFPLManager_initialize_malloc_ArrFPLMutex.tc */
    IDU_FIT_POINT("svmFPLManager::initialize::malloc::ArrFPLMutex");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SVM,
                                 ID_SIZEOF(iduMutex) *
                                 SVM_FREE_PAGE_LIST_COUNT,
                                 (void **) & aTBSNode->mArrFPLMutex,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    /* BUG-31881 �� Free Page List�� ���� PageReservation ����ü �޸� �Ҵ� */
    aTBSNode->mArrPageReservation = NULL;
    /* svmFPLManager_initialize_malloc_ArrPageReservation.tc */
    IDU_FIT_POINT("svmFPLManager::initialize::malloc::ArrPageReservation");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SVM,
                                 ID_SIZEOF( svmPageReservation ) *
                                 SVM_FREE_PAGE_LIST_COUNT,
                                 (void **) & aTBSNode->mArrPageReservation,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    for ( i =0; i < SVM_FREE_PAGE_LIST_COUNT ; i++ )
    {
        // BUGBUG kmkim check NULL charackter
        idlOS::snprintf( sMutexName,
                         128,
                         "SVM_FREE_PAGE_LIST_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_MUTEX",
                         aTBSNode->mHeader.mID,
                         i );

        new( & aTBSNode->mArrFPLMutex[i] ) iduMutex();
        IDE_TEST( aTBSNode->mArrFPLMutex[i].initialize(
                      sMutexName,
                      IDU_MUTEX_KIND_NATIVE,
                      IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );

        aTBSNode->mArrPageReservation[ i ].mSize = 0;
    }

    idlOS::snprintf( sMutexName,
                     128,
                     "SVM_ALLOC_CHUNK_%"ID_UINT32_FMT"_MUTEX",
                     aTBSNode->mHeader.mID );

    IDE_TEST( aTBSNode->mAllocChunkMutex.initialize(
                  sMutexName,
                  IDU_MUTEX_KIND_NATIVE,
                  IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Free Page List �����ڸ� �ı��Ѵ�.
 *
 */
IDE_RC svmFPLManager::destroy(svmTBSNode * aTBSNode)
{
    UInt i;

    IDE_DASSERT( aTBSNode->mArrFPLMutex != NULL );

    // Chunk �Ҵ� Mutex�� �ı�.
    IDE_TEST( aTBSNode->mAllocChunkMutex.destroy() != IDE_SUCCESS );

    // �� Free Page List�� Mutex�� �ı�
    for ( i =0; i < SVM_FREE_PAGE_LIST_COUNT; i++ )
    {
        IDE_TEST( aTBSNode->mArrFPLMutex[i].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free( aTBSNode->mArrFPLMutex ) != IDE_SUCCESS );
    aTBSNode->mArrFPLMutex = NULL ;

    IDE_TEST( iduMemMgr::free( aTBSNode->mArrPageReservation ) 
              != IDE_SUCCESS );
    aTBSNode->mArrPageReservation = NULL ;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Create Volatile Tablespace�� DML�� ���� ChunkȮ�� �߻��� Mutex���
 */
IDE_RC svmFPLManager::lockGlobalAllocChunkMutex()
{
    IDE_TEST( mGlobalAllocChunkMutex.lock(NULL) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Create Volatile Tablespace�� DML�� ���� ChunkȮ�� �߻��� MutexǮ��
 */
IDE_RC svmFPLManager::unlockGlobalAllocChunkMutex()
{
    IDE_TEST( mGlobalAllocChunkMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Free Page�� ���� ���� ���� �����ͺ��̽� Free Page List�� ã�Ƴ���.
 *
 * ����! ���ü� ��� ���� �ʱ� ������, �� �Լ��� ������ Free Page List��
 *       ���� ���� Free Page List�� ������ ���� ���� ���� �ִ�.
 *
 * aFPLNo [OUT] ã�Ƴ� Free Page List
 */
IDE_RC svmFPLManager::getShortestFreePageList( svmTBSNode * aTBSNode,
                                               svmFPLNo *   aFPLNo )
{
    UInt     i;
    svmFPLNo sShortestFPL         ;
    vULong   sShortestFPLPageCount;

    IDE_DASSERT( aFPLNo != NULL );

    sShortestFPL          = 0;
    sShortestFPLPageCount = aTBSNode->mMemBase.mFreePageLists[ 0 ].mFreePageCount ;

    for ( i=1; i < aTBSNode->mMemBase.mFreePageListCount; i++ )
    {
        if ( aTBSNode->mMemBase.mFreePageLists[ i ].mFreePageCount <
             sShortestFPLPageCount )
        {
            sShortestFPL = i;
            sShortestFPLPageCount =
                aTBSNode->mMemBase.mFreePageLists[ i ].mFreePageCount;
        }
    }

    *aFPLNo = sShortestFPL ;

    return IDE_SUCCESS;

//    IDE_EXCEPTION_END;
//    return IDE_FAILURE;
}

/* Free Page�� ���� ���� ���� �����ͺ��̽� Free Page List�� ã�Ƴ���.
 *
 * ����! ���ü� ��� ���� �ʱ� ������, �� �Լ��� ������ Free Page List��
 *       ���� ���� Free Page List�� ������ ���� ���� ���� �ִ�.
 *
 * aFPLNo [OUT] ã�Ƴ� Free Page List
 */
IDE_RC svmFPLManager::getLargestFreePageList( svmTBSNode * aTBSNode,
                                              svmFPLNo *   aFPLNo )
{
    UInt     i;
    svmFPLNo sLargestFPL          ;
    vULong   sLargestFPLPageCount ;

    IDE_DASSERT( aFPLNo != NULL );

    sLargestFPL          = 0;
    sLargestFPLPageCount = aTBSNode->mMemBase.mFreePageLists[ 0 ].mFreePageCount ;

    for ( i=1; i < aTBSNode->mMemBase.mFreePageListCount; i++ )
    {
        if ( aTBSNode->mMemBase.mFreePageLists[ i ].mFreePageCount >
             sLargestFPLPageCount )
        {
            sLargestFPL = i;
            sLargestFPLPageCount =
                aTBSNode->mMemBase.mFreePageLists[ i ].mFreePageCount;
        }
    }

    *aFPLNo = sLargestFPL ;

    return IDE_SUCCESS;
//    IDE_EXCEPTION_END;
//    return IDE_FAILURE;
}





/* �ϳ��� Free Page���� �����ͺ��̽� Free Page List�� ���� �й��Ѵ�.
 *
 * Chunk�� ���� Page�� ���������� ������ Free Page List�� �������� ���ļ�
 * �й��Ѵ�.
 *
 * �̸� �ѹ��� �й����� �ʴ� ������, �ѹ��� �й��� ���
 * ��� Free Page List�κ��� Page�� �Ҵ�޴� ��� ���������Ͽ� Ŀ�ٶ�
 * Hole�� ����µ�, �̴� DB ������ �޸𸮷� �����ϴµ� �־
 * �� ���� ����� �ʿ���Ѵ�.
 *
 * Free Page List�� �ѹ��� �й��� Page�� ���� �ʹ� ������ Page�� Locality
 * �� �״��� ���� �ʾƼ� ��߿� ������ ���ϵ� �� �ִ�.
 *
 * Free Page List�� �ѹ��� �й��� Page�� ���� �ʹ� ������, �����ͺ��̽�
 * ���Ͽ� Hole�� ���� �����ͺ��̽� �⵿�ÿ�
 * �����ͺ��̽� ������ �޸𸮷� �о���� ���� I/O����� �����Ѵ�.
 *
 * �� �Լ��� svmManager::allocNewExpandChunk�� ����
 * Expand Chunk�Ҵ�ÿ� ȣ��Ǵ� ��ƾ�̸�,
 * Expand Chunk�Ҵ��� Logical Redo�ǹǷ�,
 * Expand Chunk�Ҵ� ������ ���ؼ� �α��� �������� �ʴ´�.
 *
 * �� �Լ��� membase�� �������� �ʱ� ������ ���ü� ��� ���ʿ��ϴ�.
 *
 * aChunkFirstFreePID [IN] ���� �߰��� Chunk�� ù��° Free Page ID
 * aChunkLastFreePID  [IN] ���� �߰��� Chunk�� ������ Free Page ID
 * aArrFreePageList   [OUT] �й�� Free Page�� List���� ���� �迭
 */
IDE_RC svmFPLManager::distributeFreePages( svmTBSNode *  aTBSNode,
                                           scPageID      aChunkFirstFreePID,
                                           scPageID      aChunkLastFreePID,
                                           idBool        aSetNextFreePage,
                                           svmPageList * aArrFreePageList)
{
    scPageID      sPID ;
    // �ϳ��� Free Page List�� �й��� ù��° Free Page
    scPageID      sDistHeadPID;
    vULong        sLinkedPageCnt = 0;
    svmFPLNo      sFPLNo;
    UInt          i;
    // Expand Chunk�Ҵ�� �� Free Page�� �ѹ��� ��� Page���� �й��� ������.
    vULong        sDistPageCnt = SVM_PER_LIST_DIST_PAGE_COUNT ;
    vULong        sChunkDataPageCount =
                      aChunkLastFreePID - aChunkFirstFreePID + 1;

    svmPageList * sFreeList ;

    IDE_DASSERT( svmExpandChunk::isDataPageID( aTBSNode,
                                               aChunkFirstFreePID )
                 == ID_TRUE);
    IDE_DASSERT( svmExpandChunk::isDataPageID( aTBSNode,
                                               aChunkLastFreePID )
                 == ID_TRUE);

    IDE_ASSERT( aArrFreePageList != NULL );
    IDE_ASSERT( aChunkFirstFreePID <= aChunkLastFreePID );
    IDE_ASSERT( aTBSNode->mMemBase.mFreePageListCount > 0 );

    // Expand Chunk���� ���������� ��� Free Page List �ȿ�
    // �ּ��� �ѹ��� �� �� �������� ����� ������ �ٽ��ѹ� �˻�
    // ( svmManager ���� �ʱ�ȭ ���߿� �̹� �˻����� )
    //
    // Free List Info Page�� ������ Chunk���� ��� Page����
    // 2 * PER_LIST_DIST_PAGE_COUNT * m_membase->mFreePageListCount
    // ���� ũ�ų� ������ üũ�Ͽ���.
    //
    // Chunk���� Free List Info Page�� ��ü�� 50%�� ������ ���� �����Ƿ�,
    // Chunk���� �������������� ( Free List Info Page�� ����)��
    // PER_LIST_DIST_PAGE_COUNT * m_membase->mFreePageListCount ���� �׻�
    // ũ�ų� ������ ������ �� �ִ�.

    IDE_ASSERT( sChunkDataPageCount >=
                sDistPageCnt * aTBSNode->mMemBase.mFreePageListCount );

    sFPLNo = 0;
    // Free Page List �迭�� �ʱ�ȭ
    for ( i = 0 ;
          i < aTBSNode->mMemBase.mFreePageListCount ;
          i ++ )
    {
        SVM_PAGELIST_INIT( & aArrFreePageList[ i ] );
    }

    // �� Expand Chunk���� ��� Data Page�� ���ؼ�
    for ( // Free Page List�� �й��� ù��° Page�� �Բ� �ʱ�ȭ�Ѵ�.
          sPID = sDistHeadPID = aChunkFirstFreePID;
          sPID <= aChunkLastFreePID ;
          sPID ++ )
    {
             // Free Page List�� �й��� ������ �������� ���
        if ( sLinkedPageCnt == sDistPageCnt - 1 ||
             sPID == aChunkLastFreePID )
        {
            if ( aSetNextFreePage == ID_TRUE )
            {
                /*
                   �й��� ������ �������̹Ƿ� Next Free Page�� NULL�̴�.

                   PRJ-1548 User Memory Tablespace ���䵵��
                   [1] ��߿� chunk Ȯ���
                   [2] restart recovery�� Logical Redo
                   [3] media recovery�� Logical Redo
                       ������ Chunk�� �����ؾ��ϴ� ���
                */

                IDE_TEST( svmExpandChunk::setNextFreePage(
                          aTBSNode,
                          sPID,
                          SM_NULL_PID ) != IDE_SUCCESS );
            }
            else
            {
                /*
                   media recovery�� Logical Redo �߿���
                   chunk ������ �ʿ���� ���
                */
            }

            sFreeList = & aArrFreePageList[ sFPLNo ];

            IDE_DASSERT( svmFPLManager::isValidFPL(
                             aTBSNode->mTBSAttr.mID,
                             sFreeList->mHeadPID,
                             sFreeList->mPageCount ) == ID_TRUE );

            // sDistHeadPID ~ sPID ������ Free List�� �Ŵ޾��ش�.
            if ( sFreeList->mHeadPID == (scPageID)0 )
            {
                IDE_DASSERT( sFreeList->mTailPID == (scPageID)0 );
                sFreeList->mHeadPID   = sDistHeadPID ;
                sFreeList->mTailPID   = sPID ;
                sFreeList->mPageCount = sLinkedPageCnt + 1;
            }
            else
            {
                IDE_DASSERT( sFreeList->mTailPID != (scPageID)0 );

                // Free List �� �� �� (Tail) �� �Ŵܴ�.

                if ( aSetNextFreePage == ID_TRUE )
                {
                    IDE_TEST( svmExpandChunk::setNextFreePage(
                                aTBSNode,
                                sFreeList->mTailPID,
                                sDistHeadPID )
                            != IDE_SUCCESS );
                }
                else
                {
                    // Nothing To do
                }

                sFreeList->mTailPID    = sPID ;
                sFreeList->mPageCount += sLinkedPageCnt + 1;
            }

            IDE_DASSERT( svmFPLManager::isValidFPL(
                             aTBSNode->mTBSAttr.mID,
                             sFreeList->mHeadPID,
                             sFreeList->mPageCount ) == ID_TRUE );

            // �������� Free Page�� ���� Free Page List ����
            sFPLNo = (sFPLNo + 1) % aTBSNode->mMemBase.mFreePageListCount ;

            // ���� Page���� �� �ٸ� Free Page List�� �й��� ù��° Free Page�� �ȴ�
            sDistHeadPID = sPID + 1;
            sLinkedPageCnt = 0;
        }
        else
        {

            if ( aSetNextFreePage == ID_TRUE )
            {
                // Chunk���� ���� Page�� Next Free Page�� �����Ѵ�.
                IDE_TEST( svmExpandChunk::setNextFreePage(
                            aTBSNode,
                            sPID,
                            sPID + 1 )
                        != IDE_SUCCESS );
            }
            else
            {
                // Nothing To do
            }

            sLinkedPageCnt ++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Free Page ���� ������ Free Page List�� �� �տ� append�Ѵ�.
 *
 * �������� Free Page���� distributeFreePages�� Free Page List����ŭ��
 * Page List�� �ɰ��� ����� ����, �� �Լ��� �̸� �޾Ƽ�
 * membase�� ������ Free Page List�� append�Ѵ�.
 *
 * Expand Chunk�Ҵ�ÿ� ȣ��Ǹ�, �α����� ����.
 * ( Expand Chunk�Ҵ��� Logical Redo�Ǳ� ���� )
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *
 * aArrFreePageList [IN] membase�� �� Free Page List �鿡 ���� Page List
 *                       ���� ����� �迭
 *                       Free Page List Info Page �� Next��ũ�� ����Ǿ��ִ�.
 *
 */
IDE_RC svmFPLManager::appendPageLists2FPLs( svmTBSNode *  aTBSNode,
                                            svmPageList * aArrFreePageList )
{

    svmFPLNo      sFPLNo ;

    IDE_DASSERT( aArrFreePageList != NULL );

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase.mFreePageListCount  ;
          sFPLNo ++ )
    {
        // Redo �߿� �Ҹ��� �Լ��̹Ƿ�, FPL�� Validityüũ�� �ؼ��� �ȵȴ�.
        IDE_TEST( appendFreePagesToList(
                      aTBSNode,
                      NULL, // Chunk �Ҵ��̱� ������, PageReservation�� �������
                      sFPLNo,
                      aArrFreePageList[sFPLNo].mPageCount,
                      aArrFreePageList[sFPLNo].mHeadPID,
                      aArrFreePageList[sFPLNo].mTailPID )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
 * ��� Free Page�� �ִ� Free Page ���� �����Ѵ�.
 *
 * �� �Լ��� Latch�� ���� �ʱ� ������ ��Ȯ�� Free Page���� �������� ���Ѵ�.
 *
 * aTotalPageCount [OUT] ��� Free Page List�� ���������� ����
 *
 */
IDE_RC svmFPLManager::getTotalFreePageCount(svmTBSNode * aTBSNode,
                                               vULong *  aTotalFreePageCnt)
{
    UInt i ;
    vULong sFreePageCnt = 0;

    IDE_DASSERT( aTotalFreePageCnt != NULL );

    for ( i=0 ; i <  aTBSNode->mMemBase.mFreePageListCount ; i ++ )
    {
        sFreePageCnt += aTBSNode->mMemBase.mFreePageLists[i].mFreePageCount ;
    }

    *aTotalFreePageCnt = sFreePageCnt;

    return IDE_SUCCESS;
}



/*======================================================================
 *
 *  PRIVATE MEMBER FUNCTIONS
 *
 *======================================================================*/



/*
 * Free Page List �� ��ġ�� �Ǵ�.
 *
 * aFPLNo [IN] Latch�� �ɷ��� �ϴ� Free Page List
 */
IDE_RC svmFPLManager::lockFreePageList( svmTBSNode * aTBSNode,
                                        svmFPLNo     aFPLNo )
{
    IDE_ASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );

    return aTBSNode->mArrFPLMutex[ aFPLNo ].lock(NULL);
}


/*
 * Free Page List �� ��ġ�� Ǭ��.
 *
 * aFPLNo [IN] Latch�� Ǯ���� �ϴ� Free Page List
 */
IDE_RC svmFPLManager::unlockFreePageList( svmTBSNode * aTBSNode,
                                          svmFPLNo     aFPLNo )
{
    IDE_ASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );

    return aTBSNode->mArrFPLMutex[ aFPLNo ].unlock();
}




/*
 * ��� Free Page List�� latch�� ��´�.
 */
IDE_RC svmFPLManager::lockAllFPLs( svmTBSNode * aTBSNode )
{

    svmFPLNo      sFPLNo ;

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase.mFreePageListCount  ;
          sFPLNo ++ )
    {

        // �ٱ����� Ư�� List�� �̹� latch�� ����ä�� �� �Լ��� �θ� �� �ִ�.
        // �̹� latch�� ���� Free Page List��� Latch�� ���� �ʴ´�.
        IDE_TEST( lockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL( "Can't lock mutex of DB Free Page List." );

    return IDE_FAILURE;
}


/*
 * ��� Free Page List���� latch�� Ǭ��.
 */
IDE_RC svmFPLManager::unlockAllFPLs( svmTBSNode * aTBSNode )
{

    svmFPLNo      sFPLNo ;

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase.mFreePageListCount  ;
          sFPLNo ++ )
    {

        // �ٱ����� Ư�� List�� �̹� latch�� ����ä�� �� �Լ��� �θ� �� �ִ�.
        // �̹� latch�� ���� Free Page List��� Latch�� ���� �ʴ´�.
        IDE_TEST( unlockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL( "Can't unlock mutex of DB Free Page List." );

    return IDE_FAILURE;
}



/*
 * Free Page List�� �����Ѵ�.
 *
 * Free Page List ���� ���� �α뵵 �Բ� �ǽ��Ѵ�.
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *
 * aFPLNo           [IN] ������ Free Page List
 * aFirstFreePageID [IN] ���� ������ ù��° Free Page
 * aFreePageCount   [IN] ���� ������ Free Page�� ��
 *
 */
IDE_RC svmFPLManager::setFreePageList( svmTBSNode * aTBSNode,
                                       svmFPLNo     aFPLNo,
                                       scPageID     aFirstFreePageID,
                                       vULong       aFreePageCount )
{
    svmDBFreePageList * sFPL ;

    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );

    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID,
                                            aFirstFreePageID )
                 == ID_TRUE );

    sFPL = & aTBSNode->mMemBase.mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mTBSAttr.mID, sFPL )
                 == ID_TRUE );

    sFPL->mFirstFreePageID = aFirstFreePageID;
    sFPL->mFreePageCount = aFreePageCount;

    IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL ) == ID_TRUE );

    return IDE_SUCCESS;
}




/*
 * Free Page List ���� �ϳ��� Free Page�� �����.
 *
 * aHeadPAge���� aTailPage����
 * Free List Info Page�� Next Free Page ID�� ����� ä�� �����Ѵ�.
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *       ����, �ּ��� aPageCount��ŭ�� Free Page��
 *       aFPLNo Free Page List�� �������� Ȯ���� �� �� �Լ��� ȣ���ؾ� �Ѵ�.
 *
 * aFPLNo     [IN] Free Page�� �Ҵ���� Free Page List
 * aPageCount [IN] �Ҵ�ް��� �ϴ� Page�� ��
 * aHeadPage  [OUT] �Ҵ���� ù��° Page
 * aTailPage  [OUT] �Ҵ���� ������ Page
 */
IDE_RC svmFPLManager::removeFreePagesFromList( svmTBSNode * aTBSNode,
                                               void *       aTrans,
                                               svmFPLNo     aFPLNo,
                                               vULong       aPageCount,
                                               scPageID *   aHeadPID,
                                               scPageID *   aTailPID )
{
    UInt                 i ;
    svmDBFreePageList  * sFPL ;
    scPageID             sSplitPID ;
    scPageID             sNewFirstFreePageID ;
    UInt                 sSlotNo;
    svmPageReservation * sPageReservation;
    UInt                 sState = 0;


    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );

    IDE_DASSERT( aHeadPID != NULL );
    IDE_DASSERT( aTailPID != NULL );

    sFPL = & aTBSNode->mMemBase.mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL )
                 == ID_TRUE );

    /* BUG-31881 ���� �ڽ��� Page�� �����ص״ٸ�, TBS���� FreePage��
     * �������鼭 ������ ��ŭ ������ ����� */
    sPageReservation = &( aTBSNode->mArrPageReservation[ aFPLNo ] );
    IDE_TEST( findPageReservationSlot( sPageReservation,
                                       aTrans,
                                       &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SVM_PAGE_RESERVATION_NULL )
    {
        /* ������ �������� ���� ����ߴٸ� 0���� ����. */
        if( sPageReservation->mPageCount[sSlotNo] < (SInt)aPageCount )
        {
            sPageReservation->mPageCount[sSlotNo] = 0;
        }
        else
        {
            sPageReservation->mPageCount[sSlotNo] -= aPageCount;
        }
        sState = 1;
    }

    // aPageCount��° Page�� ã�Ƽ� �� ID�� sSplitPID�� �����Ѵ�.
    for ( i = 1, sSplitPID = sFPL->mFirstFreePageID;
          i < aPageCount;
          i++ )
    {
        IDE_ASSERT( sSplitPID != SM_NULL_PID );

        IDE_DASSERT( svmExpandChunk::isDataPageID( aTBSNode, sSplitPID )
                     == ID_TRUE );

        IDE_TEST( svmExpandChunk::getNextFreePage( aTBSNode,
                                                   sSplitPID,
                                                   &sSplitPID )
                  != IDE_SUCCESS );
    }

    // sSplitPID���� �����, �� ������ Page���� Free Page List�� ���� ���´�.
    IDE_TEST( svmExpandChunk::getNextFreePage( aTBSNode,
                                               sSplitPID,
                                               & sNewFirstFreePageID )
              != IDE_SUCCESS );

    * aHeadPID = sFPL->mFirstFreePageID ;
    * aTailPID = sSplitPID ;

    IDE_TEST( svmFPLManager::setFreePageList(
                  aTBSNode,
                  aFPLNo,
                  sNewFirstFreePageID,
                  sFPL->mFreePageCount - aPageCount )
              != IDE_SUCCESS );

    IDE_TEST( svmExpandChunk::setNextFreePage( aTBSNode,
                                               sSplitPID,
                                               SM_NULL_PID )
              != IDE_SUCCESS );

    IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID, *aHeadPID, aPageCount )
                 == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 ) 
    {
        sPageReservation->mPageCount[sSlotNo] += aPageCount;
    }

    return IDE_FAILURE;
}



/*
 * Free Page List �� �ϳ��� Free Page List�� ���δ�.( Tx�� Free Page �ݳ� )
 *
 * aHeadPAge���� aTailPage����
 * Free List Info Page�� Next Free Page ID�� ����Ǿ� �־�� �Ѵ�..
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *
 * aFPLNo     [IN] Free Page�� �ݳ��� Free Page List
 * aPageCount [IN] �ݳ��ϰ��� �ϴ� Page�� ��
 * aHeadPage  [OUT] �ݳ��� ù��° Page
 * aTailPage  [OUT] �ݳ��� ������ Page
 */
IDE_RC svmFPLManager::appendFreePagesToList  ( svmTBSNode * aTBSNode,
                                               void *       aTrans,
                                               svmFPLNo     aFPLNo,
                                               vULong       aPageCount,
                                               scPageID     aHeadPID,
                                               scPageID     aTailPID )
{
    svmDBFreePageList   * sFPL ;
    UInt                  sSlotNo;
    svmPageReservation * sPageReservation;
    UInt                  sState = 0;


    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );

    IDE_DASSERT( aPageCount != 0 );
    IDE_DASSERT( svmExpandChunk::isDataPageID( aTBSNode,
                                               aHeadPID )
                 == ID_TRUE );
    IDE_DASSERT( svmExpandChunk::isDataPageID( aTBSNode,
                                               aTailPID )
                 == ID_TRUE );


    sFPL = & aTBSNode->mMemBase.mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL )
                 == ID_TRUE );

    /* BUG-31881 ���� �ڽ��� Page�� �����Ϸ��ߴٸ�,
     * TBS�� Page �ݳ��ϸ鼭 ���ÿ� ������ */
    sPageReservation = &( aTBSNode->mArrPageReservation[ aFPLNo ] );
    IDE_TEST( findPageReservationSlot( sPageReservation,
                                       aTrans,
                                       &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SVM_PAGE_RESERVATION_NULL )
    {
        sPageReservation->mPageCount[sSlotNo] += aPageCount;
        sState = 1;
    }

    // ���� Free Page List �� ù��° Free Page��
    // ���ڷ� ���� Tail Page�� �������� �Ŵܴ�.
    IDE_TEST( svmExpandChunk::setNextFreePage( aTBSNode,
                                               aTailPID,
                                               sFPL->mFirstFreePageID )
              != IDE_SUCCESS );

    IDE_TEST( setFreePageList( aTBSNode,
                               aFPLNo,
                               aHeadPID,
                               sFPL->mFreePageCount + aPageCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 ) 
    {
        if( sPageReservation->mPageCount[sSlotNo] < (SInt)aPageCount )
        {
            sPageReservation->mPageCount[sSlotNo] = 0;
        }
        else
        {
            sPageReservation->mPageCount[sSlotNo] -= aPageCount;
        }
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * BUG-31881 [sm-mem-resource] When executing alter table in MRDB and 
 * using space by other transaction,
 * The server can not do restart recovery. 
 *
 * ���ݺ��� �� Transaction�� ��ȯ�ϴ� Page���� �̻��� �ݵ�� FreePage��
 * ������ �ֵ��� �Ͽ�, �� Transaction�� �ٽ� Page�� �䱸������ �ݵ��
 * �ش� ���������� ����� �� �ֵ��� �����Ѵ�.
 *
 * [IN] aTBSNode - ����� Tablespace Node
 * [IN] aTrans   - �����ϴ� Transaction
 ************************************************************************/
IDE_RC svmFPLManager::beginPageReservation( svmTBSNode * aTBSNode,
                                            void       * aTrans )
{
    UInt                 sFPLNo      = 0;
    UInt                 sSlotNo     = 0;
    svmPageReservation * sPageReservation;
    UInt                 sState      = 0;
    idBool               sSuccess    = ID_FALSE;
    PDL_Time_Value       sTV;
    UInt                 sSleepTime  = 5000;

    smLayerCallback::allocRSGroupID( aTrans, &sFPLNo );

    while( sSuccess == ID_FALSE )
    {
        IDE_TEST( lockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );
        sState = 1;

        sPageReservation = &( aTBSNode->mArrPageReservation[ sFPLNo ] );

        /* Ȥ�� �̹� ��ϵǾ��ִ��� Ȯ���Ѵ�. */
        IDE_TEST( findPageReservationSlot( sPageReservation,
                                           aTrans,
                                           &sSlotNo )
                  != IDE_SUCCESS );

        if( sSlotNo == SVM_PAGE_RESERVATION_NULL )
        {
            sSlotNo       = sPageReservation->mSize;
            if( sSlotNo < SVM_PAGE_RESERVATION_MAX )
            {
                sPageReservation->mTrans[ sSlotNo ]     = aTrans;
                sPageReservation->mPageCount[ sSlotNo ] = 0;
                sPageReservation->mSize ++;
                sSuccess = ID_TRUE;
            }
            else
            {
                sSuccess = ID_FALSE;
            }
        }
        else
        {
            /* �̹� ��ϵǾ��ִٸ� �� ��ü�� ����. ������ ������� ��Ȳ
             * �̱⿡, Debug���� ���������� ��Ų��. */
#if defined(DEBUG)
            IDE_RAISE( error_internal );
#endif
            sSuccess = ID_TRUE;
        }

        IDE_TEST( unlockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );

        if( sSuccess == ID_FALSE )
        {
            /* 3600�� (1�ð�) �̻� ��õ� �ߴµ��� �ȵǴ� ���� ����
             * �ִ� ����̴�. ���� ����������. */
            IDE_TEST_RAISE( sSleepTime > 3600000, error_internal );
 
            /* ���ÿ� �� TBS�� SVM_PAGE_RESERVATION_MAX ����(64)�̻��� 
             * AlterTable�� �Ͼ ���ɼ��� ���� ����.
             * ���� ��ٷ����� ��õ� �Ѵ�. */
            sTV.set(0, sSleepTime );
            idlOS::sleep(sTV);

            sSleepTime <<= 1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_internal );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "sFPLNo     : %u\n"
                     "sSlotNo    : %u\n",
                     sFPLNo,
                     sSlotNo );

        dumpPageReservation( sPageReservation );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        IDE_DASSERT( 0 );
    }

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) unlockFreePageList( aTBSNode, sFPLNo );
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * ������ ������ �����Ѵ�.
 *
 * [IN] aTBSNode - �����ߴ� Tablespace Node
 * [IN] aTrans   - �����ߴ� Transaction
 ************************************************************************/
IDE_RC svmFPLManager::endPageReservation( svmTBSNode * aTBSNode,
                                          void       * aTrans )
{
    UInt                 sFPLNo;
    UInt                 sSlotNo;
    UInt                 sLastSlotNo;
    svmPageReservation * sPageReservation;
    UInt                 sState = 0;

    smLayerCallback::allocRSGroupID( aTrans, &sFPLNo );

    IDE_TEST( lockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );
    sState = 1;

    sPageReservation = &( aTBSNode->mArrPageReservation[ sFPLNo ] );

    IDE_TEST( findPageReservationSlot( sPageReservation,
                                       aTrans,
                                       &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SVM_PAGE_RESERVATION_NULL ) 
    {
        /* ������ ���������� �������� ���� �ְ� ������ ���� �ִ�.
         * NewTable�� OldTable���� Page�� �� �����������, �� ������� ����
         * �ֱ� �����̴�. */
        /* ������ Slot�� ������� Slot���� �̵����� ���� ����� ������*/
        IDE_TEST_RAISE( sPageReservation->mSize == 0, error_internal );

        sLastSlotNo = sPageReservation->mSize - 1;
        sPageReservation->mTrans[     sSlotNo ] 
            = sPageReservation->mTrans[     sLastSlotNo ];
        sPageReservation->mPageCount[ sSlotNo ] 
            = sPageReservation->mPageCount[ sLastSlotNo ];

        sPageReservation->mSize --;
    }
    else
    {
        /* ���� RestartRecovery���� ���, Backup�� ���ϰ� restore�� �ϰ� �ȴ�.
         * �׷��� Runtime�����θ� �����ϴ� PageReservation�� ���⿡ �����Ȳ
         * �̴�. */
        /* ����� Volatile������, Alter Table�� DDL�����̱⿡ Logging�ȴ�*/
        /* BUG-39689  alter table�� �����Ͽ� PageReservation ������ ����������
         * tran abort�� �Ǹ� PageReservation�� ���� �� �ִ�. ���� */
    }

    sState = 0;
    IDE_TEST( unlockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_internal );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "sFPLNo     : %u\n"
                     "sSlotNo    : %u\n",
                     sFPLNo,
                     sSlotNo );

        dumpPageReservation( sPageReservation );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        IDE_DASSERT( 0 );
    }

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) unlockFreePageList( aTBSNode, sFPLNo );
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * �ش� Transaction�� ������ Slot�� ã�´�.
 *
 *  << mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� ��!! >>
 *
 * [IN]  aPageReservation - �ش� PageList�� PageReservation ��ü
 * [IN]  aTrans           - �����ߴ� Transaction
 * [OUT] aSlotNo          - �ش� Transaction�� ����� Slot�� ��ȣ
 ************************************************************************/
IDE_RC svmFPLManager::findPageReservationSlot( 
    svmPageReservation  * aPageReservation,
    void                * aTrans,
    UInt                * aSlotNo )
{
    UInt   sSlotNo; 
    UInt   i;

    /* TSB Ȯ����� ������, aTrans�� Null�� �� ����. */
    IDE_TEST_RAISE( aPageReservation == NULL, error_argument );
    IDE_TEST_RAISE( aSlotNo          == NULL, error_argument );

    sSlotNo = SVM_PAGE_RESERVATION_NULL ;

    for( i = 0 ; i < aPageReservation->mSize ; i ++ )
    {
        if( aPageReservation->mTrans[ i ] == aTrans )
        {
            sSlotNo = i;
            break;
        }
    }
    
    *aSlotNo = sSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_argument );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "aPageReservation : %lu\n"
                     "aTrans           : %lu\n"
                     "aSlotNo          : %u\n",
                     aPageReservation,
                     aTrans,
                     aSlotNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        IDE_DASSERT( 0 ); 
    }
 
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * �ڽ� �� �ٸ� Transaction���� �����صּ� ���ܵ־��ϴ� ��� ���ϴ� Page
 * �� ������ �����´�.
 *
 *  << mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� ��!! >>
 *
 * [IN]  aPageReservation   - �ش� PageList�� PageReservation ��ü
 * [IN]  aTrans             - �����ߴ� Transaction
 * [OUT] aUnusablePageCount - �ڽ��� ��� ���ϴ� �������� ����
 ************************************************************************/
IDE_RC svmFPLManager::getUnusablePageCount( 
    svmPageReservation  * aPageReservation,
    void                * aTrans,
    UInt                * aUnusablePageCount )
{
    UInt   sUnusablePageCount; 
    UInt   i;

    /* Trans�� Null�̾ ��. */
    IDE_TEST_RAISE( aPageReservation   == NULL, error_argument );
    IDE_TEST_RAISE( aUnusablePageCount == NULL, error_argument );

    sUnusablePageCount = 0;

    for( i = 0 ; i < aPageReservation->mSize ; i ++ )
    {
        if( ( aPageReservation->mTrans[ i ] != aTrans ) &&
            ( aPageReservation->mPageCount[ i ] > 0 ) )
        {
            sUnusablePageCount += aPageReservation->mPageCount[ i ];
        }    
    }
    
    *aUnusablePageCount = sUnusablePageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_argument );
    {
        ideLog::logCallStack( IDE_SM_0 );
        ideLog::log( IDE_SM_0,
                     "aPageReservation   : %lu\n"
                     "aUnusablePageCount : %u\n",
                     aPageReservation,
                     aUnusablePageCount );
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        IDE_DASSERT( 0 ); 
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Ȥ�� �� Transaction�� ���õ� ���� �������� ������, ��� �����Ѵ�.
 *
 * [IN]  aTrans             - Ž���� Transaction
 ************************************************************************/
IDE_RC svmFPLManager::finalizePageReservation( void      * aTrans,
                                               scSpaceID   aSpaceID )
{
    sctTableSpaceNode   * sCurTBS;
    svmTBSNode          * sVolTBS;
    UInt                  sFPLNo;
    svmPageReservation  * sPageReservation;
    UInt                  sSlotNo;
    UInt                  sState = 0;

    smLayerCallback::allocRSGroupID( aTrans, &sFPLNo );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sCurTBS )
              != IDE_SUCCESS );

    if( (sCurTBS != NULL ) &&
        (sctTableSpaceMgr::isVolatileTableSpace( sCurTBS->mID ) == ID_TRUE ) )
    {
        sVolTBS = (svmTBSNode*)sCurTBS;
        IDE_TEST( lockFreePageList( sVolTBS, sFPLNo ) != IDE_SUCCESS );
        sState = 1;

        sPageReservation = &( sVolTBS->mArrPageReservation[ sFPLNo ] );

        IDE_TEST( findPageReservationSlot( sPageReservation,
                                           aTrans,
                                           &sSlotNo )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( unlockFreePageList( sVolTBS, sFPLNo ) != IDE_SUCCESS );

        /* ���� ������ ���, Release��忡���� �������ְ� Debug���
         * ������ ������ ������� ��Ȳ�� �����Ѵ�. */
        if( sSlotNo != SVM_PAGE_RESERVATION_NULL )
        {
            IDE_DASSERT( 0 );
            IDE_TEST( endPageReservation( sVolTBS, aTrans )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) unlockFreePageList( sVolTBS, sFPLNo );
    }

    return IDE_FAILURE;
}

IDE_RC svmFPLManager::dumpPageReservationByBuffer( 
    svmPageReservation * aPageReservation,
    SChar              * aOutBuf,
    UInt                 aOutSize )
{
    UInt i;

    IDE_TEST_RAISE( aPageReservation == NULL, error_argument );
    IDE_TEST_RAISE( aOutBuf          == NULL, error_argument );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "dump - VolPageReservation\n"
                     "size : %"ID_UINT32_FMT"\n",
                     aPageReservation->mSize );
 
    for( i = 0 ; i < SVM_PAGE_RESERVATION_MAX ; i ++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%2"ID_UINT32_FMT"] "
                             "TransPtr  :8%"ID_vULONG_FMT" "
                             "PageCount :4%"ID_INT32_FMT"\n",
                             i,
                             aPageReservation->mTrans[ i ],
                             aPageReservation->mPageCount [ i ] );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_argument );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "aPageReservation   : %lu\n"
                     "aOutBuf            : %lu\n"
                     "aOutSize           : %u\n",
                     aPageReservation,
                     aOutBuf,
                     aOutSize );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        IDE_DASSERT( 0 );
    }   
    IDE_EXCEPTION_END;

    return IDE_FAILURE;   
}

void svmFPLManager::dumpPageReservation( 
    svmPageReservation * aPageReservation )
{
    SChar          * sTempBuffer;

    if( iduMemMgr::calloc( IDU_MEM_SM_SVM,
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        (void) dumpPageReservationByBuffer( aPageReservation, 
                                            sTempBuffer,
                                            IDE_DUMP_DEST_LIMIT );

        ideLog::log( IDE_SERVER_0,
                     "%s\n",
                     sTempBuffer );

        (void) iduMemMgr::free( sTempBuffer );
    }
}

/*
 * Ư�� Free Page List�� Latch�� ȹ���� �� Ư�� ���� �̻���
 * Free Page�� ������ �����Ѵ�.
 */
IDE_RC svmFPLManager::lockListAndPreparePages( svmTBSNode * aTBSNode,
                                               void *       aTrans,
                                               svmFPLNo     aFPLNo,
                                               vULong       aPageCount )
{
    svmPageReservation * sPageReservation;
    UInt              sUnusablePageSize = 0;
    UInt              sStage            = 0;

    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase.mFreePageListCount );
    IDE_DASSERT( aPageCount != 0 );

    IDE_TEST( lockFreePageList( aTBSNode, aFPLNo ) != IDE_SUCCESS );
    sStage = 1;

    /* �ٸ� Transaction���� �����ص� Page�� ����� �� ����. */
    sPageReservation = &( aTBSNode->mArrPageReservation[ aFPLNo ] );
    IDE_TEST( getUnusablePageCount( sPageReservation,
                                    aTrans,
                                    &sUnusablePageSize )
              != IDE_SUCCESS );

    // ���� Free Page���� ���ڶ�ٸ�,
    // Free Page List�� Free Page�� ���� �޾��ش�.
    // �̶� ��� �Ұ����� ������ �̻����� FreePage�� �־�� �Ѵ�.
    while ( aTBSNode->mMemBase.mFreePageLists[ aFPLNo ].mFreePageCount 
            < aPageCount + sUnusablePageSize )
    {
        IDE_DASSERT( svmFPLManager::isValidFPL(
                         aTBSNode->mHeader.mID,
                         & aTBSNode->mMemBase.mFreePageLists[ aFPLNo ] ) == ID_TRUE );

        sStage = 0;
        IDE_TEST( unlockFreePageList( aTBSNode, aFPLNo ) != IDE_SUCCESS );

        IDE_TEST( expandFreePageList( aTBSNode, 
                                      aTrans,
                                      aFPLNo, 
                                      aPageCount + sUnusablePageSize )
                  != IDE_SUCCESS );

        IDE_TEST( lockFreePageList( aTBSNode, aFPLNo ) != IDE_SUCCESS );
        sStage = 1;
    }

    IDE_DASSERT( svmFPLManager::isValidFPL(
                     aTBSNode->mHeader.mID,
                     & aTBSNode->mMemBase.mFreePageLists[ aFPLNo ] ) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 1:
            IDE_ASSERT( unlockFreePageList(aTBSNode, aFPLNo) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*
 * Free Page List�� Free Page���� append�Ѵ�.
 *
 * �켱, �ٸ� Free Page List �� Free Page�� ����� ���ٸ�,
 * �ش� Free Page List���� Free Page ������ �� ����ٰ� ���δ�.
 *
 * �׷��� �ʴٸ�, �����ͺ��̽��� Ȯ���Ѵ�.
 *
 * ����! �� �Լ��� �ʿ��� Page��ŭ Free Page�� Ȯ������ �������� ���Ѵ�.
 * �ֳ��ϸ� aShortFPLNo�� ���� Latch�� Ǯ�� �����ϱ� ������.
 *
 * ��� Free Page List�� ���� Latch�� Ǯ��ä�� �Ҹ����.
 *
 * aShortFPLNo        [IN] Free Page�� �ʿ�� �ϴ� Free Page List
 * aRequiredFreePageCnt [IN] �ʿ��� Free Page List�� ��
 */
IDE_RC svmFPLManager::expandFreePageList( svmTBSNode * aTBSNode,
                                          void       * aTrans,
                                          svmFPLNo     aExpandingFPLNo,
                                          vULong       aRequiredFreePageCnt )
{
    svmFPLNo            sLargestFPLNo ;
    svmDBFreePageList * sLargestFPL ;
    svmDBFreePageList * sExpandingFPL ;
    vULong              sSplitPageCnt;
    scPageID            sSplitHeadPID, sSplitTailPID;
    UInt                sStage = 0;
    svmFPLNo            sListNo1 = 0, sListNo2 = 0;

    IDE_DASSERT( aExpandingFPLNo < aTBSNode->mMemBase.mFreePageListCount );
    IDE_DASSERT( aRequiredFreePageCnt != 0 );

    sExpandingFPL = & aTBSNode->mMemBase.mFreePageLists[ aExpandingFPLNo ];

    // Latch ���� ���� ���·� Free Page���� ���� ���� Free Page List�� �����´�
    IDE_TEST( getLargestFreePageList( aTBSNode, & sLargestFPLNo )
              != IDE_SUCCESS );

    sLargestFPL = & aTBSNode->mMemBase.mFreePageLists[ sLargestFPLNo ] ;

    // �켱 Latch �� ���� ä�� Free Page List Split�� �������� �˻��غ���.
    if (
          // Free Page�� �ʿ�� �ϴ� FPL ( aExpandingFPLNo )��
          // Free Page�� ���� ���� ���
          // sLargestFPLNo ����� ���� aExpandingFPLNo �� ������ �� ����..
          // �� ������ Free Page�� ���Ѿ� �� ���� ����.
          aExpandingFPLNo != sLargestFPLNo
          &&
          // Free Page List�� Split������ �ּ����� Page�� �����ִ��� �˻�
          // (�� �˻縦 ���� ������ Free Page�� � ���� Free Page List����
          //  Split�� ����ϰ� �ϰ� �Ǿ� �ý��� ������ ������ �� �ִ� )
          ( sLargestFPL->mFreePageCount > SVM_FREE_PAGE_SPLIT_THRESHOLD )
          &&
          // ������ ������ �� �ʿ��� Free Page���� ���� �� �ִ��� �˻�
          ( sLargestFPL->mFreePageCount > aRequiredFreePageCnt * 2 ) )
    {
        // ����� ������ ���� Mutex�� No�� ���� List���� ��´�.
        if ( aExpandingFPLNo < sLargestFPLNo )
        {
            sListNo1 = aExpandingFPLNo ;
            sListNo2 = sLargestFPLNo ;
        }
        else
        {
            sListNo1 = sLargestFPLNo ;
            sListNo2 = aExpandingFPLNo ;
        }


        IDE_TEST( lockFreePageList( aTBSNode, sListNo1 ) != IDE_SUCCESS );
        sStage = 1;

        IDE_TEST( lockFreePageList( aTBSNode, sListNo2 ) != IDE_SUCCESS );
        sStage = 2;

        IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sLargestFPL ) == ID_TRUE );
        IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sExpandingFPL ) == ID_TRUE );

        // List Split�� �������� ������ �ٽ� �˻��Ѵ�.
        // (������ ���� �˻��� ���� Latch�� ����� �˻��� ���̶�
        //  Split�� ���������� Ȯ���� �� ����. )
        if ( ( sLargestFPL->mFreePageCount > SVM_FREE_PAGE_SPLIT_THRESHOLD )
             &&
             ( sLargestFPL->mFreePageCount > aRequiredFreePageCnt * 2 ) )
        {
            // Free Page List�� Page ������ �����.
            sSplitPageCnt = sLargestFPL->mFreePageCount / 2 ;

            IDE_TEST( removeFreePagesFromList( aTBSNode,
                                               aTrans,
                                               sLargestFPLNo,
                                               sSplitPageCnt,
                                               & sSplitHeadPID,
                                               & sSplitTailPID )
                      != IDE_SUCCESS );

            // Free Page �� �ʿ�� �ϴ� Free Page List�� �ٿ��ش�.
            IDE_TEST( appendFreePagesToList ( aTBSNode,
                                              aTrans,
                                              aExpandingFPLNo,
                                              sSplitPageCnt,
                                              sSplitHeadPID,
                                              sSplitTailPID )
                      != IDE_SUCCESS );
        }

        IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sLargestFPL ) == ID_TRUE );
        IDE_DASSERT( svmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sExpandingFPL ) == ID_TRUE );

        sStage = 1;
        IDE_TEST( unlockFreePageList( aTBSNode, sListNo2 ) != IDE_SUCCESS );

        sStage = 0;
        IDE_TEST( unlockFreePageList( aTBSNode, sListNo1 ) != IDE_SUCCESS );
    }


    // �ʿ��� Page ����ŭ Ȯ���� �� ������
    // Expand Chunk�� �߰��ذ��� �����ͺ��̽��� Ȯ���Ѵ�.
    if( sExpandingFPL->mFreePageCount < aRequiredFreePageCnt )
    {
        // Tablespace�� NEXTũ�⸸ŭ ChunkȮ���� �õ��Ѵ�.
        // MEM_MAX_DB_SIZE�� Tablespace�� MAXSIZE���ѿ� �ɸ� ��� �����߻�
        IDE_TEST( expandOrWait( aTBSNode ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 2 :
                IDE_ASSERT( unlockFreePageList( aTBSNode,
                                                sListNo2 ) == IDE_SUCCESS );
            case 1 :
                IDE_ASSERT( unlockFreePageList( aTBSNode,
                                                sListNo1 ) == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*
   svmFPLManager::getTotalPageCount4AllTBS�� ���� Action�Լ�

   [IN]  aTBSNode   - Tablespace Node
   [OUT] aActionArg - Total Page ���� ����� ������ ������
 */
IDE_RC svmFPLManager::aggregateTotalPageCountAction(
                          idvSQL*             /*aStatistics*/,
                          sctTableSpaceNode * aTBSNode,
                          void * aActionArg  )
{
    scPageID * sTotalPageCount = (scPageID*) aActionArg;

    if ( sctTableSpaceMgr::isVolatileTableSpace(aTBSNode->mID) == ID_TRUE )
    {
        if ( sctTableSpaceMgr::hasState( aTBSNode,
                                         SCT_SS_SKIP_COUNTING_TOTAL_PAGES )
             == ID_TRUE )
        {
            // do nothing
            // DROP��  Tablespace��
            // ������� Page Memory�� �����Ƿ�,
            // TOTAL PAGE���� ���� �ʴ´�.
        }
        else
        {
            *sTotalPageCount += (UInt) svmDatabase::getAllocPersPageCount(
                &((svmTBSNode*)aTBSNode)->mMemBase);
        }
    }

    return IDE_SUCCESS;

}



/*
   ��� Tablepsace�� ���� OS�κ��� �Ҵ��� Page�� ���� ������ ��ȯ�Ѵ�.

   [OUT] aTotalPageCount - ��� Tablespace�� �Ҵ�� Page���� ����
 */
IDE_RC svmFPLManager::getTotalPageCount4AllTBS( scPageID * aTotalPageCount )
{
    scPageID sTotalPageCount = 0;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                    NULL, /* idvSQL* */
                                    aggregateTotalPageCountAction,
                                    & sTotalPageCount, /* Action Argument*/
                                    SCT_ACT_MODE_NONE)
              != IDE_SUCCESS );

    * aTotalPageCount = sTotalPageCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    Tablespace�� NEXT ũ�⸸ŭ ChunkȮ���� �õ��Ѵ�.

    MEM_MAX_DB_SIZE�� �Ѱ質 Tablespace�� MAXSIZE�Ѱ踦 �Ѿ�� ����

    [IN] aTBSNode - ChunkȮ���Ϸ��� Tablespace Node

    [���ü�] Mutex��� ���� : TBSNode.mAllocChunkMutex => mTBSAllocChunkMutex
 */
IDE_RC svmFPLManager::tryToExpandNextSize(svmTBSNode * aTBSNode)
{
    UInt       sState = 0;
    SChar    * sTBSName;
    scPageID   sTotalPageCount   = 0;
    scPageID   sTBSNextPageCount = 0;
    scPageID   sTBSMaxPageCount  = 0;


    vULong     sNewChunkCount;
    ULong      sTBSCurrentSize;
    ULong      sTBSNextSize;
    ULong      sTBSMaxSize;

    scPageID sExpandChunkPageCount = smuProperty::getExpandChunkPageCount();

    // BUGBUG-1548 aTBSNode->mTBSAttr.mName�� 0���� ������ ���ڿ��� �����
    sTBSName          = aTBSNode->mHeader.mName;
    sTBSNextPageCount = aTBSNode->mTBSAttr.mVolAttr.mNextPageCount;
    sTBSMaxPageCount  = aTBSNode->mTBSAttr.mVolAttr.mMaxPageCount;

    IDE_TEST_RAISE( aTBSNode->mTBSAttr.mVolAttr.mIsAutoExtend == ID_FALSE,
                    error_unable_to_expand_cuz_auto_extend_mode );


    // �ý��ۿ��� ���� �ϳ��� Tablespace����
    // ChunkȮ���� �ϵ��� �ϴ� Mutex
    // => �� ���� Tablespace�� ���ÿ� ChunkȮ���ϴ� ��Ȳ������
    //    ��� Tablespace�� �Ҵ��� Page ũ�Ⱑ MEM_MAX_DB_SIZE����
    //    ���� �� �˻��� �� ���� ����
    IDE_TEST( lockGlobalAllocChunkMutex() != IDE_SUCCESS );
    sState = 1;

    // Tablespace�� Ȯ���� Database�� ��� Tablespace�� ����
    // �Ҵ�� Page���� ������ MEM_MAX_DB_SIZE ������Ƽ�� ����
    // ũ�⺸�� �� ũ�� ����
    IDE_TEST( getTotalPageCount4AllTBS( & sTotalPageCount ) != IDE_SUCCESS );

    IDE_TEST_RAISE( ( sTotalPageCount + sTBSNextPageCount ) >
                    ( smuProperty::getVolMaxDBSize() / SM_PAGE_SIZE ),
                    error_unable_to_expand_cuz_mem_max_db_size );


    // ChunKȮ���� ũ�Ⱑ Tablespace�� MAXSIZE�Ѱ踦 �Ѿ�� ����
    sTBSCurrentSize =
        svmDatabase::getAllocPersPageCount( &aTBSNode->mMemBase ) *
        SM_PAGE_SIZE;

    sTBSNextSize = sTBSNextPageCount * SM_PAGE_SIZE;
    sTBSMaxSize  = sTBSMaxPageCount * SM_PAGE_SIZE;

    IDE_TEST_RAISE( ( sTBSCurrentSize  + sTBSNextSize )
                    > sTBSMaxSize,
                    error_unable_to_expand_cuz_tbs_max_size );

    // Next Page Count�� Expand Chunk Page���� ������ �������� �Ѵ�.
    IDE_ASSERT( (sTBSNextPageCount % sExpandChunkPageCount) == 0 );
    sNewChunkCount = sTBSNextPageCount / sExpandChunkPageCount ;

    // �����ͺ��̽��� Ȯ��Ǹ鼭 ��� Free Page List��
    // ���� Free Page���� �й�Ǿ� �Ŵ޸���.
    IDE_TEST( svmManager::allocNewExpandChunks( aTBSNode,
                                                sNewChunkCount )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mGlobalAllocChunkMutex.unlock() != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION( error_unable_to_expand_cuz_auto_extend_mode );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_EXTEND_CHUNK_WHEN_AUTO_EXTEND_OFF, sTBSName ));

        /* BUG-40980 : AUTOEXTEND OFF���¿��� TBS max size�� �����Ͽ� extend �Ұ���
         *             error �޽����� altibase_sm.log���� ����Ѵ�. */
        ideLog::log( IDE_SM_0, 
                     "Unable to extend the tablespace(%s) when AUTOEXTEND mode is OFF",
                     sTBSName);

    
    }
    IDE_EXCEPTION( error_unable_to_expand_cuz_mem_max_db_size );
    {
        // KB ������ MEM_MAX_DB_SIZE ������Ƽ��
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_EXTEND_CHUNK_MORE_THAN_VOLATILE_MAX_DB_SIZE, sTBSName, (ULong) (smuProperty::getVolMaxDBSize()/1024)  ));
    }
    IDE_EXCEPTION( error_unable_to_expand_cuz_tbs_max_size );
    {
        // BUG-28521 [SM] ���ڵ� ������ ���� �������� ������ �߻��Ҷ�
        //                ���� �޼����� �̻��մϴ�.
        //
        // Size ������ kilobyte������ �����ϱ� ���ؼ� 1024�� �����ϴ�.
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_EXTEND_CHUNK_MORE_THAN_TBS_MAXSIZE, sTBSName, (sTBSCurrentSize/1024), (sTBSMaxSize/1024) ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sState )
        {
            case 1:

                IDE_ASSERT( mGlobalAllocChunkMutex.unlock() == IDE_SUCCESS );

                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}




/*
 * ���ÿ� �ΰ��� Ʈ������� Chunk�� �Ҵ�޴� ���� �����Ѵ�.
 *
 * ��� Free Page List�� ���� latch�� Ǯ��ä�� �� �Լ��� �Ҹ���.
 *
 * [IN] aTBSNode       - Chunk�� Ȯ���� Tablespace�� Node
 *
 * [���ü�] Mutex��� ���� : TBSNode.mAllocChunkMutex => mTBSAllocChunkMutex
 */

IDE_RC svmFPLManager::expandOrWait(svmTBSNode * aTBSNode)
{

    idBool sIsLocked ;

    IDE_TEST( aTBSNode->mAllocChunkMutex.trylock( sIsLocked ) != IDE_SUCCESS );

    if ( sIsLocked == ID_TRUE ) // ���� Expand Chunk�Ҵ����� Ʈ����� ���� ��
    {
        // Tablespace�� NEXT ũ�⸸ŭ ChunkȮ�� �ǽ�
        // MEM_MAX_DB_SIZE�� �Ѱ質
        // Tablespace�� MAXSIZE�Ѱ踦 �Ѿ�� ���� �߻�
        IDE_TEST( tryToExpandNextSize( aTBSNode )
                  != IDE_SUCCESS );

        sIsLocked = ID_FALSE;
        IDE_TEST( aTBSNode->mAllocChunkMutex.unlock() != IDE_SUCCESS );
    }
    else // ���� Expand Chunk�� �Ҵ����� Ʈ������� ���� ��
    {
        // Expand Chunk�Ҵ��� �����⸦ ��ٸ���.
        IDE_TEST( lockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );
        // �ٸ� Ʈ������� Expand Chunk�Ҵ��� �Ͽ����Ƿ�, ������
        // Expand Chunk�Ҵ��� ���� �ʴ´�.
        IDE_TEST( unlockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sIsLocked == ID_TRUE )
        {
            IDE_ASSERT( unlockAllocChunkMutex(aTBSNode) == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*
 * Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
 *
 * aFirstPID  - [IN] Free Page List�� ù��° Page ID
 * aPageCount - [IN] Free Page List�� ���� Page�� ��
 */
idBool svmFPLManager::isValidFPL(scSpaceID    aSpaceID,
                                 scPageID     aFirstPID,
                                 vULong       aPageCount )
{
    idBool    sIsValid = ID_TRUE;
    svmTBSNode * sTBSNode;

    IDE_ASSERT(sctTableSpaceMgr::findSpaceNodeBySpaceID(aSpaceID,
                                                        (void**)&sTBSNode)
               == IDE_SUCCESS);

    if ( aFirstPID == SM_NULL_PID )
    {
        // PID�� NULL�̸鼭 Page���� 0�� �ƴϸ� ����
        if ( aPageCount != 0 )
        {
            sIsValid = ID_FALSE;
        }
    }
    else
    {
        IDE_DASSERT( svmExpandChunk::isDataPageID( sTBSNode,
                                                   aFirstPID ) == ID_TRUE );
    }

#if defined( DEBUG_SVM_PAGE_LIST_CHECK )
    // Recovery�߿��� Free Page List�� ���� ����� Free Page����
    // Free Page List�� ��ϵ� Free Page ���� ������ ������ �� ����.
    if ( aFirstPID != SM_NULL_PID &&
         smLayerCallback::isRestartRecoveryPhase() == ID_FALSE )
    {
        sPageCount = 0;

        sPID = aFirstPID;

        for(;;)
        {
            sPageCount ++;

            IDE_ASSERT( svmExpandChunk::getNextFreePage( sPID, & sNextPID )
                        == IDE_SUCCESS );
            if ( sNextPID == SM_NULL_PID )
            {
                break;
            }
            sPID = sNextPID ;
        }
#if defined( DEBUG_PAGE_ALLOC_FREE )
        if ( sPageCount != aPageCount )
        {
            idlOS::fprintf( stdout, "===========================================================================\n" );
            idlOS::fprintf( stdout, "Page Count Mismatch(List:%"ID_UINT64_FMT",Count:%"ID_UINT64_FMT")\n", (ULong)sPageCount, (ULong)aPageCount );
            idlOS::fprintf( stdout, "List First PID : %"ID_UINT64_FMT", List Last PID : %"ID_UINT64_FMT"\n", (ULong)aFirstPID, (ULong)sPID );
            idlOS::fprintf( stdout, "FLI Page ID of Last PID : %"ID_UINT64_FMT"\n",
                            (ULong)svmExpandChunk::getFLIPageID( sPID ) );
            idlOS::fprintf( stdout, "Slot offset in FLI Page of Last PID : %"ID_UINT32_FMT"\n",
                            (UInt) svmExpandChunk::getSlotOffsetInFLIPage( sPID ) );
            idlOS::fprintf( stdout, "===========================================================================\n" );
            idlOS::fflush( stdout );
        }
#endif
        // List�� ��ũ�� ���󰡼� �� ������ ����
        // ���ڷ� ���� ������ ���� �ٸ��� ����
        if ( sPageCount != aPageCount )
        {
            sIsValid = ID_FALSE;
        }
    }
#endif // DEBUG_SVM_PAGE_LIST_CHECK

    return sIsValid;
}


/*
 * Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
 *
 * aFPL - [IN] �˻��ϰ��� �ϴ� Free Page List
 */
idBool svmFPLManager::isValidFPL( scSpaceID           aSpaceID,
                                  svmDBFreePageList * aFPL )
{
    return isValidFPL( aSpaceID,
                       aFPL->mFirstFreePageID,
                       aFPL->mFreePageCount );
}

/*
 * ��� Free Page List�� Valid���� üũ�Ѵ�
 *
 */
idBool svmFPLManager::isAllFPLsValid( svmTBSNode * aTBSNode )
{
    svmFPLNo      sFPLNo ;

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase.mFreePageListCount  ;
          sFPLNo ++ )
    {
        IDE_ASSERT( isValidFPL( aTBSNode->mHeader.mID,
                                &aTBSNode->mMemBase.mFreePageLists[ sFPLNo ] )
                    == ID_TRUE );
    }

    return ID_TRUE;
}

/*
 * ��� Free Page List�� ������ ��´�.
 *
 */
void svmFPLManager::dumpAllFPLs(svmTBSNode * aTBSNode)
{
    svmFPLNo            sFPLNo ;
    svmDBFreePageList * sFPL ;

    idlOS::fprintf( stdout, "===========================================================================\n" );

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase.mFreePageListCount  ;
          sFPLNo ++ )
    {
        sFPL = & aTBSNode->mMemBase.mFreePageLists[ sFPLNo ];

        idlOS::fprintf(stdout, "FPL[%"ID_UINT32_FMT"]ID=%"ID_UINT64_FMT",COUNT=%"ID_UINT64_FMT"\n",
                       sFPLNo,
                       (ULong) sFPL->mFirstFreePageID,
                       (ULong) sFPL->mFreePageCount );
    }

    idlOS::fprintf( stdout, "===========================================================================\n" );

    idlOS::fflush( stdout );

    IDE_DASSERT( isAllFPLsValid(aTBSNode) == ID_TRUE );
}
