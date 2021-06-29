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
 

/***********************************************************************
* $Id: smmFPLManager.cpp 90259 2021-03-19 01:22:22Z emlee $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smm.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smu.h>
#include <smuUtility.h>
#include <smmReq.h>
#include <smmManager.h>
#include <smmFPLManager.h>
#include <smmExpandChunk.h>
#include <sctTableSpaceMgr.h>


/*======================================================================
 *
 *  PUBLIC MEMBER FUNCTIONS
 *
 *======================================================================*/

iduMutex smmFPLManager::mGlobalPageCountCheckMutex;

/*
    Static Initializer
 */
IDE_RC smmFPLManager::initializeStatic()
{
    IDE_TEST( mGlobalPageCountCheckMutex.initialize(
                  (SChar*)"SMM_GLOBAL_ALLOC_CHUNK_MUTEX",
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
IDE_RC smmFPLManager::destroyStatic()
{
    IDE_TEST( mGlobalPageCountCheckMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
 * Free Page List �����ڸ� �ʱ�ȭ�Ѵ�.
 */
IDE_RC smmFPLManager::initialize( smmTBSNode * aTBSNode )
{
    UInt  i ;
    SChar sMutexName[ 128 ];

    aTBSNode->mMemBase = NULL;

    // �� Free Page List�� Mutex�� �ʱ�ȭ
    /* smmFPLManager_initialize_malloc_ArrFPLMutex.tc */
    IDU_FIT_POINT("smmFPLManager::initialize::malloc::ArrFPLMutex");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 ID_SIZEOF(iduMutex) *
                                 SMM_FREE_PAGE_LIST_COUNT,
                                 (void **) & aTBSNode->mArrFPLMutex,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    /* BUG-31881 �� Free Page List�� ���� PageReservation ����ü �޸� �Ҵ� */
    aTBSNode->mArrPageReservation = NULL;

    /* smmFPLManager_initialize_malloc_ArrPageReservation.tc */
    IDU_FIT_POINT("smmFPLManager::initialize::malloc::ArrPageReservation");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 ID_SIZEOF( smmPageReservation ) *
                                 SMM_FREE_PAGE_LIST_COUNT,
                                 (void **) & aTBSNode->mArrPageReservation,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );


    for ( i =0; i < SMM_FREE_PAGE_LIST_COUNT ; i++ )
    {
        // BUGBUG kmkim check NULL charackter
        idlOS::snprintf( sMutexName,
                         128,
                         "SMM_FREE_PAGE_LIST_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_MUTEX",
                         aTBSNode->mHeader.mID,
                         i );

        new( & aTBSNode->mArrFPLMutex[ i ] ) iduMutex();
        IDE_TEST( aTBSNode->mArrFPLMutex[ i ].initialize(
                                      sMutexName,
                                      IDU_MUTEX_KIND_NATIVE,
                                      IDV_WAIT_INDEX_NULL) 
                  != IDE_SUCCESS );

        aTBSNode->mArrPageReservation[ i ].mSize = 0;
    }

    idlOS::snprintf( sMutexName,
                     128,
                     "SMM_ALLOC_CHUNK_%"ID_UINT32_FMT"_MUTEX",
                     aTBSNode->mHeader.mID );

    IDE_TEST( aTBSNode->mAllocChunkMutex.initialize(
                                      sMutexName,
                                      IDU_MUTEX_KIND_NATIVE,
                                      IDV_WAIT_INDEX_NULL) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Free Page List �����ڸ� �ı��Ѵ�.
 *
 */
IDE_RC smmFPLManager::destroy(smmTBSNode * aTBSNode)
{
    UInt i;

    IDE_DASSERT( aTBSNode->mArrFPLMutex != NULL );

    // Chunk �Ҵ� Mutex�� �ı�.
    IDE_TEST( aTBSNode->mAllocChunkMutex.destroy() != IDE_SUCCESS );

    // �� Free Page List�� Mutex�� �ı�
    for ( i =0; i < SMM_FREE_PAGE_LIST_COUNT; i++ )
    {
        IDE_TEST( aTBSNode->mArrFPLMutex[ i ].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free( aTBSNode->mArrFPLMutex     ) != IDE_SUCCESS );
    aTBSNode->mArrFPLMutex = NULL ;


    IDE_TEST( iduMemMgr::free( aTBSNode->mArrPageReservation )
              != IDE_SUCCESS );
    aTBSNode->mArrPageReservation = NULL ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smmFPLManager::lockGlobalPageCountCheckMutex()
{
    IDE_TEST( mGlobalPageCountCheckMutex.lock( NULL  /* idvSQL* */ )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smmFPLManager::unlockGlobalPageCountCheckMutex()
{
    IDE_TEST( mGlobalPageCountCheckMutex.unlock() != IDE_SUCCESS );

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
IDE_RC smmFPLManager::getLargestFreePageList( smmTBSNode * aTBSNode,
                                              smmFPLNo   * aFPLNo )
{
    UInt     i;
    smmFPLNo sLargestFPL          ;
    vULong   sLargestFPLPageCount ;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    IDE_DASSERT( aFPLNo != NULL );

    sLargestFPL          = 0;
    sLargestFPLPageCount = aTBSNode->mMemBase->mFreePageLists[ 0 ].mFreePageCount ;

    for ( i=1; i < aTBSNode->mMemBase->mFreePageListCount; i++ )
    {
        if ( aTBSNode->mMemBase->mFreePageLists[ i ].mFreePageCount >
             sLargestFPLPageCount )
        {
            sLargestFPL = i;
            sLargestFPLPageCount =
                aTBSNode->mMemBase->mFreePageLists[ i ].mFreePageCount;
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
 * �� �Լ��� smmManager::allocNewExpandChunk�� ����
 * Expand Chunk�Ҵ�ÿ� ȣ��Ǵ� ��ƾ�̸�,
 * Expand Chunk�Ҵ��� Logical Redo�ǹǷ�,
 * Expand Chunk�Ҵ� ������ ���ؼ� �α��� �������� �ʴ´�.
 *
 * �� �Լ��� membase�� �������� �ʱ� ������ ���ü� ��� ���ʿ��ϴ�.
 *
 * aChunkFirstFreePID [IN] ���� �߰��� Chunk�� ù��° Free Page ID
 * aChunkLastFreePID  [IN] ���� �߰��� Chunk�� ������ Free Page ID
 * aArrFreePageListCount [IN]  �й�� Free Page�� List���� ���� �迭 ����
 * aArrFreePageList   [OUT] �й�� Free Page�� List���� ���� �迭
 */
IDE_RC smmFPLManager::distributeFreePages( smmTBSNode *  aTBSNode,
                                           scPageID      aChunkFirstFreePID,
                                           scPageID      aChunkLastFreePID,
                                           idBool        aSetNextFreePage,
                                           UInt          aArrFreePageListCount,
                                           smmPageList * aArrFreePageList)
{
    scPageID      sPID ;
    // �ϳ��� Free Page List�� �й��� ù��° Free Page
    scPageID      sDistHeadPID;
    vULong        sLinkedPageCnt = 0;
    smmFPLNo      sFPLNo;
    UInt          i;
    // Expand Chunk�Ҵ�� �� Free Page�� �ѹ��� ��� Page���� �й��� ������.
    vULong        sDistPageCnt = SMM_PER_LIST_DIST_PAGE_COUNT ;
#if defined(DEBUG)
    vULong        sChunkDataPageCount =
                      aChunkLastFreePID - aChunkFirstFreePID + 1;
#endif

    smmPageList * sFreeList ;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    IDE_DASSERT( smmExpandChunk::isDataPageID( aTBSNode,
                                               aChunkFirstFreePID )
                 == ID_TRUE);
    IDE_DASSERT( smmExpandChunk::isDataPageID( aTBSNode,
                                               aChunkLastFreePID )
                 == ID_TRUE);

    IDE_ASSERT( aArrFreePageList != NULL );
    IDE_ASSERT( aChunkFirstFreePID <= aChunkLastFreePID );
    IDE_ASSERT( aArrFreePageListCount > 0 );

    // Expand Chunk���� ���������� ��� Free Page List �ȿ�
    // �ּ��� �ѹ��� �� �� �������� ����� ������ �ٽ��ѹ� �˻�
    // ( smmManager ���� �ʱ�ȭ ���߿� �̹� �˻����� )
    //
    // Free List Info Page�� ������ Chunk���� ��� Page����
    // 2 * PER_LIST_DIST_PAGE_COUNT * aArrFreePageListCount
    // ���� ũ�ų� ������ üũ�Ͽ���.
    //
    // Chunk���� Free List Info Page�� ��ü�� 50%�� ������ ���� �����Ƿ�,
    // Chunk���� �������������� ( Free List Info Page�� ����)��
    // PER_LIST_DIST_PAGE_COUNT * aArrFreePageListCount ���� �׻�
    // ũ�ų� ������ ������ �� �ִ�.

    IDE_DASSERT( sChunkDataPageCount >=
                 sDistPageCnt * aArrFreePageListCount );

    sFPLNo = 0;
    // Free Page List �迭�� �ʱ�ȭ
    for ( i = 0 ;
          i < aArrFreePageListCount ;
          i ++ )
    {
        SMM_PAGELIST_INIT( & aArrFreePageList[ i ] );
    }

    // �� Expand Chunk���� ��� Data Page�� ���ؼ�
    for ( // Free Page List�� �й��� ù��° Page�� �Բ� �ʱ�ȭ�Ѵ�.
          sPID = sDistHeadPID = aChunkFirstFreePID;
          sPID <= aChunkLastFreePID ;
          sPID ++ )
    {
             // Free Page List�� �й��� ������ �������� ���
        if ( ( sLinkedPageCnt == (sDistPageCnt - 1) ) ||
             ( sPID == aChunkLastFreePID ) )
        {
            if ( aSetNextFreePage == ID_TRUE )
            {
                /*
                   �й��� ������ �������̹Ƿ� Next Free Page�� NULL�̴�.

                   PRJ-1548 User Memory Tablespace ���䵵��
                   [1] ��߿� chunk Ȯ���
                   [2] restart recovery�� Logical Redo
                   [3] media recovery�� Logical Redo
                       Chunk�� �����ؾ��ϴ� ���
                */

                IDE_TEST( smmExpandChunk::logAndSetNextFreePage(
                                                      aTBSNode,
                                                      NULL, // �α����
                                                      sPID,
                                                      SM_NULL_PID ) 
                          != IDE_SUCCESS );
            }
            else
            {
                /*
                   media recovery�� Logical Redo �߿���
                   chunk ������ �ʿ���� ���
                */
            }

            sFreeList = & aArrFreePageList[ sFPLNo ];

            IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mTBSAttr.mID,
                                                    sFreeList->mHeadPID,
                                                    sFreeList->mPageCount ) 
                         == ID_TRUE );

            // sDistHeadPID ~ sPID ������ Free List�� �Ŵ޾��ش�.
            if ( sFreeList->mHeadPID == SMM_MEMBASE_PAGEID )
            {
                IDE_DASSERT( sFreeList->mTailPID == SMM_MEMBASE_PAGEID );
                sFreeList->mHeadPID   = sDistHeadPID ;
                sFreeList->mTailPID   = sPID ;
                sFreeList->mPageCount = sLinkedPageCnt + 1;
            }
            else
            {
                IDE_DASSERT( sFreeList->mTailPID != SMM_MEMBASE_PAGEID );

                // Free List �� �� �� (Tail) �� �Ŵܴ�.

                if ( aSetNextFreePage == ID_TRUE )
                {
                    IDE_TEST( smmExpandChunk::logAndSetNextFreePage(
                                                    aTBSNode,
                                                    NULL,  // �α����
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

            IDE_DASSERT( smmFPLManager::isValidFPL(
                                                 aTBSNode->mTBSAttr.mID,
                                                 sFreeList->mHeadPID,
                                                 sFreeList->mPageCount ) 
                         == ID_TRUE );

            // �������� Free Page�� ���� Free Page List ����
            sFPLNo = (sFPLNo + 1) % aArrFreePageListCount ;

            // ���� Page���� �� �ٸ� Free Page List�� �й��� ù��° Free Page�� �ȴ�
            sDistHeadPID = sPID + 1;
            sLinkedPageCnt = 0;
        }
        else
        {

            if ( aSetNextFreePage == ID_TRUE )
            {
                // Chunk���� ���� Page�� Next Free Page�� �����Ѵ�.
                IDE_TEST( smmExpandChunk::logAndSetNextFreePage(
                                                aTBSNode,
                                                NULL,  // �α����
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
 * aTrans           [IN] Free Page List ��ũ ������ �α��� Ʈ�����
 * aArrFreePageList [IN] membase�� �� Free Page List �鿡 ���� Page List
 *                       ���� ����� �迭
 *                       Free Page List Info Page �� Next��ũ�� ����Ǿ��ִ�.
 *
 */
IDE_RC smmFPLManager::appendPageLists2FPLs(
                            smmTBSNode *  aTBSNode,
                            smmPageList * aArrFreePageList,
                            idBool        aSetFreeListOfMembase,
                            idBool        aSetNextFreePageOfFPL )
{

    smmFPLNo      sFPLNo ;

    IDE_DASSERT( aArrFreePageList != NULL );

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase->mFreePageListCount  ;
          sFPLNo ++ )
    {
        // Redo �߿� �Ҹ��� �Լ��̹Ƿ�, FPL�� Validityüũ�� �ؼ��� �ȵȴ�.
        IDE_TEST( appendFreePagesToList(
                              aTBSNode,
                              NULL, // �α� ����
                              sFPLNo,
                              aArrFreePageList[ sFPLNo ].mPageCount,
                              aArrFreePageList[ sFPLNo ].mHeadPID,
                              aArrFreePageList[ sFPLNo ].mTailPID,
                              aSetFreeListOfMembase,
                              aSetNextFreePageOfFPL )
                  != IDE_SUCCESS );

    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
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
IDE_RC smmFPLManager::lockFreePageList( smmTBSNode * aTBSNode,
                                        smmFPLNo     aFPLNo )
{
    IDE_ASSERT( aTBSNode->mMemBase != NULL );
    IDE_ASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );

    return aTBSNode->mArrFPLMutex[ aFPLNo ].lock(NULL /* idvSQL* */);
}


/*
 * Free Page List �� ��ġ�� Ǭ��.
 *
 * aFPLNo [IN] Latch�� Ǯ���� �ϴ� Free Page List
 */
IDE_RC smmFPLManager::unlockFreePageList( smmTBSNode * aTBSNode,
                                          smmFPLNo     aFPLNo )
{
    IDE_ASSERT( aTBSNode->mMemBase != NULL );
    IDE_ASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );

    return aTBSNode->mArrFPLMutex[ aFPLNo ].unlock();
}




/*
 * ��� Free Page List�� latch�� ��´�.
 */
IDE_RC smmFPLManager::lockAllFPLs( smmTBSNode * aTBSNode )
{

    smmFPLNo      sFPLNo ;


    IDE_ASSERT( aTBSNode->mMemBase != NULL );


    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase->mFreePageListCount  ;
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
IDE_RC smmFPLManager::unlockAllFPLs( smmTBSNode * aTBSNode )
{

    smmFPLNo      sFPLNo ;


    IDE_ASSERT( aTBSNode->mMemBase != NULL );


    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase->mFreePageListCount  ;
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
 * ���� ! aTrans�� NULL�̸� �α����� �ʴ´�.
 *        Expand Chunk�Ҵ��� Logical Redo����̾ Normal Processing��
 *        Physical update�� ���� �α����� �ʴ´�..

 * aTrans           [IN] Free Page List�� �����Ϸ��� Ʈ�����
 * aFPLNo           [IN] ������ Free Page List
 * aFirstFreePageID [IN] ���� ������ ù��° Free Page
 * aFreePageCount   [IN] ���� ������ Free Page�� ��
 *
 */
IDE_RC smmFPLManager::logAndSetFreePageList( smmTBSNode * aTBSNode,
                                             void *       aTrans,
                                             smmFPLNo     aFPLNo,
                                             scPageID     aFirstFreePageID,
                                             vULong       aFreePageCount )
{
    smmDBFreePageList * sFPL ;

    // �α��� ���� �ʴ� ��� aTrans�� NULL�� �� �� �ִ�.

    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );

    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID,
                                            aFirstFreePageID )
                 == ID_TRUE );

    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    sFPL = & aTBSNode->mMemBase->mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mTBSAttr.mID, sFPL )
                 == ID_TRUE );

    if ( aTrans != NULL )
    {
        IDE_TEST( smLayerCallback::allocPersListAtMembase(
                                      NULL, /* idvSQL* */
                                      aTrans,
                                      aTBSNode->mHeader.mID,
                                      aTBSNode->mMemBase,
                                      aFPLNo,
                                      aFirstFreePageID,
                                      aFreePageCount )
                  != IDE_SUCCESS );

    }

    sFPL->mFirstFreePageID = aFirstFreePageID;
    sFPL->mFreePageCount   = aFreePageCount;

    IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL ) == ID_TRUE );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( aTBSNode->mHeader.mID,
                                             SMM_MEMBASE_PAGEID ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}




/*
 * Free Page List ���� �ϳ��� Free Page�� �����.
 *
 * aHeadPAge���� aTailPage����
 * Free List Info Page�� Next Free Page ID�� ����� ä�� �����Ѵ�.
 *
 * ���� ! aTrans�� NULL�̸� �α����� �ʴ´�.
 *        Expand Chunk�Ҵ��� Logical Redo����̾ Normal Processing��
 *        Physical update�� ���� �α����� �ʴ´�..
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *       ����, �ּ��� aPageCount��ŭ�� Free Page��
 *       aFPLNo Free Page List�� �������� Ȯ���� �� �� �Լ��� ȣ���ؾ� �Ѵ�.
 *
 * aTrans     [IN] Free Page�� �Ҵ��Ϸ��� Ʈ�����
 * aFPLNo     [IN] Free Page�� �Ҵ���� Free Page List
 * aPageCount [IN] �Ҵ�ް��� �ϴ� Page�� ��
 * aHeadPage  [OUT] �Ҵ���� ù��° Page
 * aTailPage  [OUT] �Ҵ���� ������ Page
 */
IDE_RC smmFPLManager::removeFreePagesFromList( smmTBSNode * aTBSNode,
                                               void *       aTrans,
                                               smmFPLNo     aFPLNo,
                                               vULong       aPageCount,
                                               scPageID *   aHeadPID,
                                               scPageID *   aTailPID )
{
    UInt                 i ;
    smmDBFreePageList  * sFPL ;
    scPageID             sSplitPID ;
    scPageID             sNewFirstFreePageID ;
    UInt                 sSlotNo;
    smmPageReservation * sPageReservation;
    UInt                 sState = 0;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );

    IDE_DASSERT( aHeadPID != NULL );
    IDE_DASSERT( aTailPID != NULL );

    sFPL = & aTBSNode->mMemBase->mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL )
                 == ID_TRUE );


    /* BUG-31881 ���� �ڽ��� Page�� �����ص״ٸ�, TBS���� FreePage��
     * �������鼭 ������ ��ŭ ������ ����� */
    sPageReservation = &( aTBSNode->mArrPageReservation[ aFPLNo ] );
    IDE_TEST( findPageReservationSlot( sPageReservation,
                                       aTrans,
                                       &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SMM_PAGE_RESERVATION_NULL )
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

        IDE_DASSERT( smmExpandChunk::isDataPageID( aTBSNode, sSplitPID )
                     == ID_TRUE );

        IDE_TEST( smmExpandChunk::getNextFreePage( aTBSNode,
                                                   sSplitPID,
                                                   &sSplitPID )
                  != IDE_SUCCESS );
    }

    // sSplitPID���� �����, �� ������ Page���� Free Page List�� ���� ���´�.
    IDE_TEST( smmExpandChunk::getNextFreePage( aTBSNode,
                                               sSplitPID,
                                               & sNewFirstFreePageID )
              != IDE_SUCCESS );

    * aHeadPID = sFPL->mFirstFreePageID ;
    * aTailPID = sSplitPID ;

    IDE_TEST( smmFPLManager::logAndSetFreePageList(
                                              aTBSNode,
                                              aTrans,
                                              aFPLNo,
                                              sNewFirstFreePageID,
                                              sFPL->mFreePageCount - aPageCount )
              != IDE_SUCCESS );

    IDE_TEST( smmExpandChunk::logAndSetNextFreePage( aTBSNode,
                                                     aTrans,
                                                     sSplitPID,
                                                     SM_NULL_PID )
              != IDE_SUCCESS );

    IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID, *aHeadPID, aPageCount )
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
 * ���� ! aTrans�� NULL�̸� �α����� �ʴ´�.
 *        Expand Chunk�Ҵ��� Logical Redo����̾ Normal Processing��
 *        Physical update�� ���� �α����� �ʴ´�..
 *
 * ����! ���ü� ������ å���� �� �Լ��� ȣ���ڿ��� �ִ�.
 *       �� �Լ������� ���ü� ��� ���� �ʴ´�.
 *
 * aTrans     [IN] Free Page�� �ݳ��Ϸ��� Ʈ�����
 * aFPLNo     [IN] Free Page�� �ݳ��� Free Page List
 * aPageCount [IN] �ݳ��ϰ��� �ϴ� Page�� ��
 * aHeadPage  [OUT] �ݳ��� ù��° Page
 * aTailPage  [OUT] �ݳ��� ������ Page
 */
IDE_RC smmFPLManager::appendFreePagesToList  (
                            smmTBSNode * aTBSNode,
                            void *       aTrans,
                            smmFPLNo     aFPLNo,
                            vULong       aPageCount,
                            scPageID     aHeadPID,
                            scPageID     aTailPID ,
                            idBool       aSetFreeListOfMembase,
                            idBool       aSetNextFreePageOfFPL )
{
    smmDBFreePageList   * sFPL ;
    UInt                  sSlotNo;
    smmPageReservation  * sPageReservation;
    UInt                  sState = 0;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );
    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );

    IDE_DASSERT( aPageCount != 0 );
    IDE_DASSERT( smmExpandChunk::isDataPageID( aTBSNode,
                                               aHeadPID )
                 == ID_TRUE );
    IDE_DASSERT( smmExpandChunk::isDataPageID( aTBSNode,
                                               aTailPID )
                 == ID_TRUE );


    sFPL = & aTBSNode->mMemBase->mFreePageLists[ aFPLNo ] ;

    IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID, sFPL )
                 == ID_TRUE );

    /* BUG-31881 ���� �ڽ��� Page�� �����Ϸ��ߴٸ�,
     * TBS�� Page �ݳ��ϸ鼭 ���ÿ� ������ */
    sPageReservation = &( aTBSNode->mArrPageReservation[ aFPLNo ] );
    IDE_TEST( findPageReservationSlot( sPageReservation,
                                       aTrans,
                                       &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SMM_PAGE_RESERVATION_NULL )
    {
        sPageReservation->mPageCount[sSlotNo] += aPageCount;
        sState = 1;
    }

    if ( aSetNextFreePageOfFPL == ID_TRUE )
    {
        // ���� Free Page List �� ù��° Free Page��
        // ���ڷ� ���� Tail Page�� �������� �Ŵܴ�.
        IDE_TEST( smmExpandChunk::logAndSetNextFreePage( aTBSNode,
                                            aTrans,
                                            aTailPID,
                                            sFPL->mFirstFreePageID )
                  != IDE_SUCCESS );
    }

    if ( aSetFreeListOfMembase == ID_TRUE )
    {
        IDE_TEST( logAndSetFreePageList( aTBSNode,
                                         aTrans,
                                         aFPLNo,
                                         aHeadPID,
                                         sFPL->mFreePageCount + aPageCount )
                  != IDE_SUCCESS );
    }


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
IDE_RC smmFPLManager::beginPageReservation( smmTBSNode * aTBSNode,
                                            void       * aTrans )
{
    UInt                 sFPLNo      = 0;
    UInt                 sSlotNo     = 0;
    smmPageReservation * sPageReservation;
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

        if( sSlotNo == SMM_PAGE_RESERVATION_NULL )
        {
            sSlotNo       = sPageReservation->mSize;
            if( sSlotNo < SMM_PAGE_RESERVATION_MAX )
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

            /* ���ÿ� �� TBS�� SMM_PAGE_RESERVATION_MAX ����(64)�̻���
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
IDE_RC smmFPLManager::endPageReservation( smmTBSNode * aTBSNode,
                                          void       * aTrans )
{
    UInt                 sFPLNo;
    UInt                 sSlotNo;
    UInt                 sLastSlotNo;
    smmPageReservation * sPageReservation;
    UInt                  sState = 0;

    smLayerCallback::allocRSGroupID( aTrans, &sFPLNo );

    IDE_TEST( lockFreePageList( aTBSNode, sFPLNo ) != IDE_SUCCESS );
    sState = 1;

    sPageReservation = &( aTBSNode->mArrPageReservation[ sFPLNo ] );

    IDE_TEST( findPageReservationSlot( sPageReservation,
                                    aTrans,
                                    &sSlotNo )
              != IDE_SUCCESS );

    if( sSlotNo != SMM_PAGE_RESERVATION_NULL )
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
         * �׷��� Runtime�����θ� �����ϴ� PageReservation�� ���� �� �ִ�.
         * ���� ���� ��Ȳ�̴�. */
        /* BUG-39689  alter table�� �����Ͽ� PageReservation ������ ����������
         * tran abort�� �Ǹ� PageReservation�� ���� �� �ִ�. ���� ��Ȳ   
         */
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
IDE_RC smmFPLManager::findPageReservationSlot(
                                    smmPageReservation  * aPageReservation,
                                    void                * aTrans,
                                    UInt                * aSlotNo )
{
    UInt   sSlotNo;
    UInt   i;

    /* TSB Ȯ����� ������, aTrans�� Null�� �� ����. */
    IDE_TEST_RAISE( aPageReservation == NULL, error_argument );
    IDE_TEST_RAISE( aSlotNo          == NULL, error_argument );

    sSlotNo = SMM_PAGE_RESERVATION_NULL ;

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
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );

        ideLog::log( IDE_SM_0,
                     "aPageReservation : %lu\n"
                     "aTrans           : %lu\n"
                     "aSlotNo          : %u\n",
                     aPageReservation,
                     aTrans,
                     aSlotNo );

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
IDE_RC smmFPLManager::getUnusablePageCount(
                                smmPageReservation  * aPageReservation,
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
IDE_RC smmFPLManager::finalizePageReservation( void      * aTrans,
                                               scSpaceID   aSpaceID )
{
    sctTableSpaceNode   * sCurTBS;
    smmTBSNode          * sMemTBS;
    UInt                  sFPLNo;
    smmPageReservation  * sPageReservation;
    UInt                  sSlotNo;
    UInt                  sState = 0;

    smLayerCallback::allocRSGroupID( aTrans, &sFPLNo );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sCurTBS )
              != IDE_SUCCESS );

    if( (sCurTBS != NULL ) &&
        ( sctTableSpaceMgr::isMemTableSpace( sCurTBS->mID ) == ID_TRUE ) )
    {
        sMemTBS = (smmTBSNode*)sCurTBS;

        /* BUG-32050 [sm-mem-resource] The finalize operation of
         * AlterTable does not consider the OfflineTablespace. */
        if( sMemTBS->mRestoreType !=
            SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
        {
            IDE_TEST( lockFreePageList( sMemTBS, sFPLNo ) != IDE_SUCCESS );
            sState = 1;

            sPageReservation = &( sMemTBS->mArrPageReservation[ sFPLNo ] );

            IDE_TEST( findPageReservationSlot( sPageReservation,
                                               aTrans,
                                               &sSlotNo )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( unlockFreePageList( sMemTBS, sFPLNo ) !=
                      IDE_SUCCESS );

            /* ���� ������ ���, Release��忡���� �������ְ� Debug���
             * ������ ������ ������� ��Ȳ�� �����Ѵ�. */
            if( sSlotNo != SMM_PAGE_RESERVATION_NULL )
            {
                IDE_DASSERT( 0 );
                IDE_TEST( endPageReservation( sMemTBS, aTrans )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) unlockFreePageList( sMemTBS, sFPLNo );
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * PageReservation ��ü�� Dump�Ѵ�.
 *
 * [IN]  aPageReservation   - Dump�� ���
 * [OUT] aOutBuf            - Dump ������ ������ ����
 * [OUT] aOutSize           - Dump������ ũ��
 ************************************************************************/
IDE_RC smmFPLManager::dumpPageReservationByBuffer(
    smmPageReservation * aPageReservation,
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

    for( i = 0 ; i < SMM_PAGE_RESERVATION_MAX ; i ++ )
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

/***************************************************************************
 * PageReservation ��ü�� boot trc�� Dump�Ѵ�.
 *
 * [IN]  aPageReservation   - Dump�� ���
 **************************************************************************/
void smmFPLManager::dumpPageReservation(
    smmPageReservation * aPageReservation )
{
    SChar          * sTempBuffer;

    if( iduMemMgr::calloc( IDU_MEM_SM_SMM,
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        (void )dumpPageReservationByBuffer( aPageReservation,
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
IDE_RC smmFPLManager::lockListAndPreparePages( smmTBSNode * aTBSNode,
                                               void *       aTrans,
                                               smmFPLNo     aFPLNo,
                                               vULong       aPageCount )
{
    smmPageReservation * sPageReservation;
    UInt                 sUnusablePageSize = 0;
    UInt                 sStage            = 0;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );
    IDE_DASSERT( aFPLNo < aTBSNode->mMemBase->mFreePageListCount );
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
    while ( aTBSNode->mMemBase->mFreePageLists[ aFPLNo ].mFreePageCount
             < aPageCount + sUnusablePageSize )
    {
        IDE_DASSERT( smmFPLManager::isValidFPL(
                         aTBSNode->mHeader.mID,
                         & aTBSNode->mMemBase->mFreePageLists[ aFPLNo ] ) == ID_TRUE );

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

    IDE_DASSERT( smmFPLManager::isValidFPL(
                     aTBSNode->mHeader.mID,
                     & aTBSNode->mMemBase->mFreePageLists[ aFPLNo ] ) == ID_TRUE );


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
 * aTrans             [IN] Free Page List���� �����Ϸ��� Ʈ�����
 * aShortFPLNo        [IN] Free Page�� �ʿ�� �ϴ� Free Page List
 * aRequiredFreePageCnt [IN] �ʿ��� Free Page List�� ��
 */
IDE_RC smmFPLManager::expandFreePageList(  smmTBSNode * aTBSNode,
                                           void *       aTrans,
                                           smmFPLNo     aExpandingFPLNo,
                                           vULong       aRequiredFreePageCnt )
{
    smmFPLNo            sLargestFPLNo ;
    smmDBFreePageList * sLargestFPL ;
    smmDBFreePageList * sExpandingFPL ;
    vULong              sSplitPageCnt;
    scPageID            sSplitHeadPID, sSplitTailPID;
    UInt                sStage = 0;
    smLSN               sNTALSN;
    smmFPLNo            sListNo1 = 0xFFFFFFFF;
    smmFPLNo            sListNo2 = 0xFFFFFFFF;

    IDE_ASSERT( aTBSNode->mMemBase != NULL );
    IDE_DASSERT( aExpandingFPLNo < aTBSNode->mMemBase->mFreePageListCount );
    IDE_DASSERT( aRequiredFreePageCnt != 0 );

    sExpandingFPL = & aTBSNode->mMemBase->mFreePageLists[ aExpandingFPLNo ];

    // Latch ���� ���� ���·� Free Page���� ���� ���� Free Page List�� �����´�
    IDE_TEST( getLargestFreePageList( aTBSNode, & sLargestFPLNo )
              != IDE_SUCCESS );

    sLargestFPL = & aTBSNode->mMemBase->mFreePageLists[ sLargestFPLNo ] ;

    // �켱 Latch �� ���� ä�� Free Page List Split�� �������� �˻��غ���.
    if (
          /* PageList�� ����ȭ �Ǿ� �������� Ȯ���Ѵ�. */
          ( smuProperty::getPageListGroupCount() > 1 )
          &&
          // Free Page�� �ʿ�� �ϴ� FPL ( aExpandingFPLNo )��
          // Free Page�� ���� ���� ���
          // sLargestFPLNo ����� ���� aExpandingFPLNo �� ������ �� ����..
          // �� ������ Free Page�� ���Ѿ� �� ���� ����.
          aExpandingFPLNo != sLargestFPLNo
          &&
          // Free Page List�� Split������ �ּ����� Page�� �����ִ��� �˻�
          // (�� �˻縦 ���� ������ Free Page�� � ���� Free Page List����
          //  Split�� ����ϰ� �ϰ� �Ǿ� �ý��� ������ ������ �� �ִ� )
          ( sLargestFPL->mFreePageCount > SMM_FREE_PAGE_SPLIT_THRESHOLD )
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

        IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sLargestFPL ) == ID_TRUE );
        IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sExpandingFPL ) == ID_TRUE );

        // List Split�� �������� ������ �ٽ� �˻��Ѵ�.
        // (������ ���� �˻��� ���� Latch�� ����� �˻��� ���̶�
        //  Split�� ���������� Ȯ���� �� ����. )
        if ( ( sLargestFPL->mFreePageCount > SMM_FREE_PAGE_SPLIT_THRESHOLD )
             &&
             ( sLargestFPL->mFreePageCount > aRequiredFreePageCnt * 2 ) )
        {
            // Free Page List�� Page ������ �����.
            sSplitPageCnt = sLargestFPL->mFreePageCount / 2 ;

            // NTA �� ���� +++++++++++++++++++
            // Page���� List���� ����� & Page ���� List�� �Ŵޱ�
            // �� NTA�� ó��.
            sNTALSN =  smLayerCallback::getLstUndoNxtLSN( aTrans );
            sStage = 3;


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
                                              sSplitTailPID,
                                              ID_TRUE, // aSetFreeListOfMembase
                                              ID_TRUE ) // aSetNextFreePageOfFPL
                      != IDE_SUCCESS );

            // write NTA
            IDE_TEST( smLayerCallback::writeNullNTALogRec( NULL, /* idvSQL* */
                                                           aTrans,
                                                           &sNTALSN )
                      != IDE_SUCCESS );

            sStage = 2;

            /* TASK-6549 ���� LFG�� ���ŵǾ�
             * LFG���� �׻� 1�̴�.
             * BUG-14352�� ������ �����Ͽ� LFG�� 1�϶��� Log Flush�� �ʿ���⿡
             * �α� Flush�� �����Ѵ�. */
        }

        IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
                                                sLargestFPL ) == ID_TRUE );
        IDE_DASSERT( smmFPLManager::isValidFPL( aTBSNode->mHeader.mID,
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
        IDE_TEST( expandOrWait( aTBSNode,
                                aTrans ) != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 3 :
                IDE_ASSERT( smLayerCallback::undoTrans( NULL, /* idvSQL* */
                                                        aTrans,
                                                        & sNTALSN )
                            == IDE_SUCCESS );
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
   smmFPLManager::getTotalPageCount4AllTBS�� ���� Action�Լ�

   [���ü� ���� ]
      GlobalPageCountCheckMutex �� ����ä�� ���´�.
      - GlobalPageCountCheckMutex�� ��� ���
        1. Expand Chunk�Ҵ��
        2. ALTER TBS ONLINE�� ( Page�ܰ踦 �ø��� Alter TBS Online������ )
        3. ALTER TBS OFFLINE�� ( Page�ܰ踦 ������ Pending ����ȿ��� )

   [IN]  aTBSNode   - Tablespace Node
   [OUT] aActionArg - Total Page ���� ����� ������ ������
 */
IDE_RC smmFPLManager::aggregateTotalPageCountAction(
                          idvSQL            * /*aStatistics */,
                          sctTableSpaceNode * aTBSNode,
                          void * aActionArg  )
{
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    idBool     sCountPage;

    scPageID * sTotalPageCount = (scPageID*) aActionArg;

    if ( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE )
    {
        if ( sctTableSpaceMgr::hasState( aTBSNode,
                                         SCT_SS_SKIP_COUNTING_TOTAL_PAGES )
             == ID_TRUE )
        {
            // do nothing
            // DROP, DISCARDED Tablespace��
            // ������� Page Memory�� �����Ƿ�,
            // TOTAL PAGE���� ���� �ʴ´�.
        }
        else
        {
            // To Fix BUG-17145 alloc slot�ϴٰ� tablespace����
            //                  Total Page Count�� ���ٰ�
            //                  Offline Tablespace�� ����
            //                  ����ó���� ���� �ʾ� �������
            //
            // OFFLINE������ ���
            //    ONLINE���·� �������� ��� => Total Page Count�� ����
            //    �� ���� ��� => Total Page Count�� ���Ծ���

            // Page Counting�� ���� ����
            sCountPage = ID_TRUE;

            if ( SMI_TBS_IS_OFFLINE(aTBSNode->mState) )
            {
                if ( (aTBSNode->mState & SMI_TBS_SWITCHING_TO_ONLINE )
                     == SMI_TBS_SWITCHING_TO_ONLINE )
                {
                    // ONLINE���·� �������� ��� =>
                    //     Total Page Count�� ���Խ�Ŵ
                    sCountPage = ID_TRUE;
                }
                else
                {
                    // �Ϲ� OFFLINE������ ���
                    //     Total Page Count�� ���Ծ���
                    sCountPage = ID_FALSE;
                }
            }

            if ( sCountPage == ID_TRUE )
            {
                *sTotalPageCount += (UInt) smmDatabase::getAllocPersPageCount(
                    ((smmTBSNode*)aTBSNode)->mMemBase);
            }
        }
    }

    return IDE_SUCCESS;

}



IDE_RC smmFPLManager::getTotalPageCount4AllTBS( scPageID  * aTotalPageCount )
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
    [IN] aTrans   - Chunk�� Ȯ���Ϸ��� Transaction

    [���ü�] Mutex��� ���� : TBSNode.mAllocChunkMutex => mTBSAllocChunkMutex
 */
IDE_RC smmFPLManager::tryToExpandNextSize(smmTBSNode * aTBSNode,
                                          void *       aTrans)
{
    UInt       sState = 0;
    SChar    * sTBSName;
    scPageID   sTotalPageCount;
    scPageID   sTBSNextPageCount;
    scPageID   sTBSMaxPageCount;


    vULong     sNewChunkCount;
    ULong      sTBSCurrentSize;
    ULong      sTBSNextSize;
    ULong      sTBSMaxSize;

    UInt       sExpandChunkPageCount = smuProperty::getExpandChunkPageCount();

    // BUGBUG-1548 aTBSNode->mTBSAttr.mName�� 0���� ������ ���ڿ��� �����
    sTBSName          = aTBSNode->mHeader.mName;
    sTBSNextPageCount = aTBSNode->mTBSAttr.mMemAttr.mNextPageCount;
    sTBSMaxPageCount  = aTBSNode->mTBSAttr.mMemAttr.mMaxPageCount;

    if( aTBSNode->mTBSAttr.mMemAttr.mIsAutoExtend == ID_FALSE )
    {
        /* BUG-32632 User Memory Tablesapce���� Max size�� �����ϴ�
         * ���� Property�߰� */
        IDE_TEST_RAISE( smuProperty::getIgnoreMemTbsMaxSize() == ID_FALSE ,
                        error_unable_to_expand_cuz_auto_extend_mode );

        /* ������ NextPageCount�� ������ */
        sTBSNextPageCount = sExpandChunkPageCount;
    }

    // �ý��ۿ��� ���� �ϳ��� Tablespace����
    // ChunkȮ���� �ϵ��� �ϴ� Mutex
    // => �� ���� Tablespace�� ���ÿ� ChunkȮ���ϴ� ��Ȳ������
    //    ��� Tablespace�� �Ҵ��� Page ũ�Ⱑ MEM_MAX_DB_SIZE����
    //    ���� �� �˻��� �� ���� ����
    IDE_TEST( lockGlobalPageCountCheckMutex() != IDE_SUCCESS );
    sState = 1;

    /* 
     * BUG- 35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE 
     */
    if( smuProperty::getSeparateDicTBSSizeEnable() == ID_TRUE ) 
    {
        if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
        {
            IDE_TEST( getDicTBSPageCount( &sTotalPageCount ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( getTotalPageCountExceptDicTBS( &sTotalPageCount )
                      != IDE_SUCCESS );
        }

        /* 
         * SYS_TBS_MEM_DIC or SYS_TBS_MEM_DATA ���̺����̽��� 
         * MEM_MAX_DB_SIZE��ŭ �����Ҽ� �ִ�.
         * ���� MEM_MAX_DB_SIZE�� memory chunk count�� ������ �������� ������ 
         * smmManager::allocNewExpandChunk()���� ������ ���� �Ҽ��ִ�. 
         * �̸� ���� �ϱ����� Ȯ���� SYS_TBS_MEM_DIC�� ����������
         * mDBMaxPageCount���� ū�� �˻��Ѵ�.
         */
        IDE_TEST_RAISE( ( sTotalPageCount + sTBSNextPageCount ) >
                        ( aTBSNode->mDBMaxPageCount ),
                        error_unable_to_expand_cuz_mem_max_db_size );
    }
    else
    {
        // Tablespace�� Ȯ���� Database�� ��� Tablespace�� ����
        // �Ҵ�� Page���� ������ MEM_MAX_DB_SIZE ������Ƽ�� ����
        // ũ�⺸�� �� ũ�� ����
        
        IDE_TEST( getTotalPageCount4AllTBS( & sTotalPageCount ) != IDE_SUCCESS );

        IDE_TEST_RAISE( ( sTotalPageCount + sTBSNextPageCount ) >
                        ( smuProperty::getMaxDBSize() / SM_PAGE_SIZE ),
                        error_unable_to_expand_cuz_mem_max_db_size );
    }

    // ChunKȮ���� ũ�Ⱑ Tablespace�� MAXSIZE�Ѱ踦 �Ѿ�� ����
    sTBSCurrentSize =
        smmDatabase::getAllocPersPageCount( aTBSNode->mMemBase ) *
        SM_PAGE_SIZE;

    sTBSNextSize = sTBSNextPageCount * SM_PAGE_SIZE;
    sTBSMaxSize  = sTBSMaxPageCount * SM_PAGE_SIZE;

    // sTBSMaxSize�� MAXSIZE�� �ش��ϴ� ũ�⸦ ���Ѵ�.
    // �׷���, ������ ���� ����ڰ� ������ MAXSIZE��
    // Expand Chunkũ��� ������ �������� ���, META PAGE������
    // MAXSIZE���ѿ� �ɷ��� Ȯ���� ���� ���ϴ� ������ �߻��Ѵ�.
    //
    // CREATE MEMORY TABLESPACE MEM_TBS SIZE 8M
    // AUTOEXTEND ON NEXT 8M MAXSIZE 16M;
    //
    // ���� ��� �ʱ�ũ�� 8M, Ȯ��ũ�� 8M������,
    // 32K(META PAGEũ��) + 8M + 8M > 16M ���� Ȯ���� ���� �ʰ� �ȴ�.
    // �Ҵ�� Page������ METAPAGE���� ���� MAXSIZEüũ�� �ؾ�
    // ���� ������ 8M ���� 8M�� �� Ȯ�� �����ϴ�.
    sTBSCurrentSize -= SMM_DATABASE_META_PAGE_CNT * SM_PAGE_SIZE ;

    /* BUG-32632 User Memory Tablesapce���� Max size�� �����ϴ�
     * ���� Property�߰� */
    IDE_TEST_RAISE( ( ( sTBSCurrentSize  + sTBSNextSize ) > sTBSMaxSize ) &&
                    ( smuProperty::getIgnoreMemTbsMaxSize() == ID_FALSE ),
                    error_unable_to_expand_cuz_tbs_max_size );

    // Next Page Count�� Expand Chunk Page���� ������ �������� �Ѵ�.
    IDE_ASSERT( (sTBSNextPageCount % sExpandChunkPageCount) == 0 );
    sNewChunkCount = sTBSNextPageCount / sExpandChunkPageCount ;

    // �����ͺ��̽��� Ȯ��Ǹ鼭 ��� Free Page List��
    // ���� Free Page���� �й�Ǿ� �Ŵ޸���.
    IDE_TEST( smmManager::allocNewExpandChunks( aTBSNode,
                                                aTrans,
                                                sNewChunkCount )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlockGlobalPageCountCheckMutex() != IDE_SUCCESS );


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
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_EXTEND_CHUNK_MORE_THAN_MEM_MAX_DB_SIZE, sTBSName, (ULong) (smuProperty::getMaxDBSize()/1024)  ));
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

                IDE_ASSERT( unlockGlobalPageCountCheckMutex() == IDE_SUCCESS );

                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*
    BUG-35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
    MEM_MAX_DB_SIZE

    __SEPARATE_DIC_TBS_SIZE_ENABLE ������Ƽ�� �����ְ� 
    SYS_TBS_MEM_DIC�� Ȯ���Ϸ��� �Ұ�� SYS_TBS_MEM_DIC�� 
    ������ ���� ���Ѵ�.
 */
IDE_RC smmFPLManager::getDicTBSPageCount( scPageID   * aTotalPageCount )
{
    sctTableSpaceNode * sDictionaryTBSNode;
    scPageID            sDictionaryTBSPageCount = 0;

    IDE_ASSERT( aTotalPageCount != NULL );

    /* MEM_DIC ���̺����̽��� �Ҵ�� ���������� ���Ѵ�. */
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                    (void**)&sDictionaryTBSNode )
             != IDE_SUCCESS );

    IDE_TEST( aggregateTotalPageCountAction( 
                                    NULL, 
                                    sDictionaryTBSNode, 
                                    &sDictionaryTBSPageCount )
              != IDE_SUCCESS );

    *aTotalPageCount = sDictionaryTBSPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    BUG-35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
    MEM_MAX_DB_SIZE

    __SEPARATE_DIC_TBS_SIZE_ENABLE ������Ƽ�� �����ְ� 
    SYS_TBS_MEM_DIC�� �ƴ� ���̺����̽��� Ȯ���Ϸ����Ұ��
    ��� ���̺� �����̽��� �Ҵ�� ������������ SYS_TBS_MEM_DIC�� ������ ����
    �� ���������� ���Ѵ�.
 */
IDE_RC smmFPLManager::getTotalPageCountExceptDicTBS( 
                                    scPageID   * aTotalPageCount )
{
    scPageID            sDictionaryTBSPageCount = 0;
    scPageID            sTotalPageCount         = 0; 

    IDE_ASSERT( aTotalPageCount != NULL );

    IDE_TEST( getDicTBSPageCount( &sDictionaryTBSPageCount ) 
              != IDE_SUCCESS );
    
    IDE_TEST( getTotalPageCount4AllTBS( & sTotalPageCount ) != IDE_SUCCESS );

    /* ��� ���̺����̽��� ������ ������ SYS_TBS_MEM_DIC ���������� ����. */
    *aTotalPageCount = sTotalPageCount - sDictionaryTBSPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * �ٸ� Ʈ����ǿ� ���ؼ��̴�,
 * aTrans���ؼ� �̴� ������� Tablespace�� NEXT ũ�⸸ŭ��
 * Chunk�� �������� �����Ѵ�.
 *
 * ���ÿ� �ΰ��� Ʈ������� Chunk�� �Ҵ�޴� ���� �����Ѵ�.
 *
 * ��� Free Page List�� ���� latch�� Ǯ��ä�� �� �Լ��� �Ҹ���.
 *
 * [IN] aTBSNode       - Chunk�� Ȯ���� Tablespace�� Node
 * [IN] aTrans         - Chunk�� �Ҵ�������� Ʈ�����
 * [IN] aNewChunkCount - ���� Ȯ���� Chunk�� ��
 *
 * [���ü�] Mutex��� ���� : TBSNode.mAllocChunkMutex => mTBSAllocChunkMutex
 */

IDE_RC smmFPLManager::expandOrWait(smmTBSNode * aTBSNode,
                                   void *       aTrans)
{

    idBool sIsLocked ;

    IDE_DASSERT( aTrans != NULL );

    IDE_TEST( aTBSNode->mAllocChunkMutex.trylock( sIsLocked ) != IDE_SUCCESS );

    if ( sIsLocked == ID_TRUE ) // ���� Expand Chunk�Ҵ����� Ʈ����� ���� ��
    {
        // Tablespace�� NEXT ũ�⸸ŭ ChunkȮ�� �ǽ�
        // MEM_MAX_DB_SIZE�� �Ѱ質
        // Tablespace�� MAXSIZE�Ѱ踦 �Ѿ�� ���� �߻�
        IDE_TEST( tryToExpandNextSize( aTBSNode, aTrans )
                  != IDE_SUCCESS );

        sIsLocked = ID_FALSE;
        IDE_TEST( aTBSNode->mAllocChunkMutex.unlock() != IDE_SUCCESS );
    }
    else // ���� Expand Chunk�� �Ҵ����� Ʈ������� ���� ��
    {
        // Expand Chunk�Ҵ��� �����⸦ ��ٸ���.
        IDE_TEST( aTBSNode->mAllocChunkMutex.lock( NULL /* idvSQL* */ )
                  != IDE_SUCCESS );

        // �ٸ� Ʈ������� Expand Chunk�Ҵ��� �Ͽ����Ƿ�, ������
        // Expand Chunk�Ҵ��� ���� �ʴ´�.
        IDE_TEST( aTBSNode->mAllocChunkMutex.unlock() != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sIsLocked == ID_TRUE )
        {
            IDE_ASSERT( aTBSNode->mAllocChunkMutex.unlock() == IDE_SUCCESS );
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
idBool smmFPLManager::isValidFPL(scSpaceID    aSpaceID,
                                 scPageID     aFirstPID,
                                 vULong       aPageCount )
{
    idBool    sIsValid = ID_TRUE;
    smmTBSNode * sTBSNode;

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
        IDE_DASSERT( smmExpandChunk::isDataPageID( sTBSNode,
                                                   aFirstPID ) == ID_TRUE );
    }

#if defined( DEBUG_SMM_PAGE_LIST_CHECK )
    // DEBUG_SMM_PAGE_LIST_CHECK �� �����ϰ� Media Recovery�� �����ϸ�
    // ASSERT �߻��� �� �ִ�. �ֳ��ϸ� Free Page List�� Consistent ����
    // ���ϱ� �����̴�.
    // FATAL !!

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

            IDE_ASSERT( smmExpandChunk::getNextFreePage( sPID, & sNextPID )
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
                            (ULong)smmExpandChunk::getFLIPageID( sPID ) );
            idlOS::fprintf( stdout, "Slot offset in FLI Page of Last PID : %"ID_UINT32_FMT"\n",
                            (UInt) smmExpandChunk::getSlotOffsetInFLIPage( sPID ) );
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
#endif // DEBUG_SMM_PAGE_LIST_CHECK

    return sIsValid;
}


/*
 * Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
 *
 * aFPL - [IN] �˻��ϰ��� �ϴ� Free Page List
 */
idBool smmFPLManager::isValidFPL( scSpaceID           aSpaceID,
                                  smmDBFreePageList * aFPL )
{
    return isValidFPL( aSpaceID,
                       aFPL->mFirstFreePageID,
                       aFPL->mFreePageCount );
}

/*
 * ��� Free Page List�� Valid���� üũ�Ѵ�
 *
 */
idBool smmFPLManager::isAllFPLsValid( smmTBSNode * aTBSNode )
{
    smmFPLNo      sFPLNo ;


    IDE_ASSERT( aTBSNode->mMemBase != NULL );


    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase->mFreePageListCount  ;
          sFPLNo ++ )
    {
        IDE_ASSERT( isValidFPL( aTBSNode->mHeader.mID,
                                &aTBSNode->mMemBase->mFreePageLists[ sFPLNo ] )
                    == ID_TRUE );
    }

    return ID_TRUE;
}

/*
 * ��� Free Page List�� ������ ��´�.
 *
 */
void smmFPLManager::dumpAllFPLs(smmTBSNode * aTBSNode)
{
    smmFPLNo            sFPLNo ;
    smmDBFreePageList * sFPL ;



    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    idlOS::fprintf( stdout, "===========================================================================\n" );

    for ( sFPLNo = 0;
          sFPLNo < aTBSNode->mMemBase->mFreePageListCount  ;
          sFPLNo ++ )
    {
        sFPL = & aTBSNode->mMemBase->mFreePageLists[ sFPLNo ];

        idlOS::fprintf(stdout, "FPL[%"ID_UINT32_FMT"]ID=%"ID_UINT64_FMT",COUNT=%"ID_UINT64_FMT"\n",
                       sFPLNo,
                       (ULong) sFPL->mFirstFreePageID,
                       (ULong) sFPL->mFreePageCount );
    }

    idlOS::fprintf( stdout, "===========================================================================\n" );

    idlOS::fflush( stdout );

    IDE_DASSERT( isAllFPLsValid(aTBSNode) == ID_TRUE );
}
