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
* $Id: svmManager.cpp 37235 2009-12-11 01:56:06Z elcarim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smiFixedTable.h>
#include <sctTableSpaceMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smu.h>
#include <smuUtility.h>
#include <svmReq.h>
#include <svmDef.h>
#include <svmDatabase.h>
#include <svmFPLManager.h>
#include <svmExpandChunk.h>
#include <svmManager.h>
#include <svrLogMgr.h>
#include <svpVarPageList.h>
#include <smiMain.h>


svmManager::svmManager()
{
}

/************************************************************************
 * Description : svmManager�� �ʱ�ȭ �Ѵ�.
 *
 *   svm ��� �߿� �ʱ�ȭ�Ǿ�� �ϴ� ������ ��� ���⼭ �ʱ�ȭ�ȴ�.
 ************************************************************************/
IDE_RC svmManager::initializeStatic()
{
    /* Volatile log manager�� �ʱ�ȭ�Ѵ�. */
    /* volatile log�� ���� �α� ���� �޸𸮸� �Ϻ� �Ҵ�޴´�. */
    IDE_TEST( svrLogMgr::initializeStatic() != IDE_SUCCESS );

    IDE_TEST( svmFPLManager::initializeStatic() != IDE_SUCCESS );

    svpVarPageList::initAllocArray();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/************************************************************************
 * Description : svm ����� ����ߴ� �ڿ��� �����Ѵ�.
 ************************************************************************/
IDE_RC svmManager::destroyStatic()
{
    IDE_TEST( svrLogMgr::destroyStatic() != IDE_SUCCESS );

    IDE_TEST( svmFPLManager::destroyStatic() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Tablespace Node�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 *
 *  [IN] aTBSNode    - �ʱ�ȭ�� Tablespace Node
 *  [IN] aTBSAttr    - log anchor�κ��� ���� TBSAttr
 ************************************************************************/
IDE_RC svmManager::allocTBSNode(svmTBSNode        **aTBSNode,
                                smiTableSpaceAttr  *aTBSAttr)
{
    IDE_DASSERT( aTBSAttr != NULL );

    /* svmManager_allocTBSNode_calloc_TBSNode.tc */
    IDU_FIT_POINT("svmManager::allocTBSNode::calloc::TBSNode");
    IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SVM,
                               1,
                               ID_SIZEOF(svmTBSNode),
                               (void**)aTBSNode)
                 != IDE_SUCCESS);

    // Memory, Volatile, Disk TBS ���� �ʱ�ȭ
    IDE_TEST(sctTableSpaceMgr::initializeTBSNode(&((*aTBSNode)->mHeader),
                                                 aTBSAttr)
             != IDE_SUCCESS);

    // TBSNode�� TBSAttr�� �� ����
    idlOS::memcpy(&((*aTBSNode)->mTBSAttr),
                  aTBSAttr,
                  ID_SIZEOF(smiTableSpaceAttr));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Tablespace Node�� �ı��Ѵ�.
 *
 *   aTBSNode [IN] ������ svmTBSNode
 ************************************************************************/
IDE_RC svmManager::destroyTBSNode(svmTBSNode *aTBSNode)
{
    IDE_DASSERT(aTBSNode != NULL );

    // Lock������ ������ TBSNode�� ��� ������ �ı�
    IDE_TEST(sctTableSpaceMgr::destroyTBSNode(&aTBSNode->mHeader)
             != IDE_SUCCESS);

    IDE_TEST( iduMemMgr::free( aTBSNode ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description :
 *    Volatile Tablespace�� �ʱ�ȭ�Ѵ�.
 *    aTBSNode�� initTBSNode()�� ���� �ʱ�ȭ�Ǿ� �־�� �Ѵ�.
 *
 ************************************************************************/
IDE_RC svmManager::initTBS(svmTBSNode *aTBSNode)
{
    if ( SMI_TBS_IS_DROPPED(aTBSNode->mHeader.mState) )
    {
        // Drop�� TBS�� �ڿ��� �ʱ�ȭ�� �ʿ䰡 ����.
    }
    else
    {
        aTBSNode->mDBMaxPageCount =
            calculateDbPageCount(smuProperty::getVolMaxDBSize(),
                                 smuProperty::getExpandChunkPageCount());

        aTBSNode->mAnchorOffset = SCT_UNSAVED_ATTRIBUTE_OFFSET;

        // Free Page List������ �ʱ�ȭ
        IDE_TEST( svmFPLManager::initialize( aTBSNode ) != IDE_SUCCESS );

        // TablespaceȮ�� ChunK������ �ʱ�ȭ
        IDE_TEST( svmExpandChunk::initialize( aTBSNode ) != IDE_SUCCESS );

        // PCH Array�ʱ�ȭ
        // PCH Array�� ���� alloc�� page �������� SVM_TBS_FIRST_PAGE_ID�� �� ���ƾ� �Ѵ�.
        // mPCHArray[TBS_ID][0]�� ������ �ʴ� PCH�̴�.
        // �� m_page�� alloc���� �ʴ´�.
        /* svmManager_initTBS_calloc_PCHArray.tc */
        IDU_FIT_POINT("svmManager::initTBS::calloc::PCHArray");
        IDE_TEST( smmManager::allocPCHArray( aTBSNode->mHeader.mID,
                                             aTBSNode->mDBMaxPageCount +
                                             SVM_TBS_FIRST_PAGE_ID ) != IDE_SUCCESS );

        /*
         * BUG-24292 create/drop volatile tablespace ���� �ݺ��� �޸� ������ ������ �߻�!!
         *
         * Database Ȯ���� ExpandGlobal Mutex�� ��� �ϱ� ������
         * ���ķ� ����Ǵ� ��찡 ����. ������ Free Page List�� 1�� �ϸ�ȴ�.
         */
        
        // BUG-47487: FLI ������ MemPool �ʱ�ȭ ( Volatile ) 
        IDE_TEST(aTBSNode->mFLIMemPagePool.initialize(
                    IDU_MEM_SM_SVM,
                    (SChar*)"TEMP_MEMORY_POOL",
                    1,
                    SM_PAGE_SIZE,
                    smuProperty::getTempPageChunkCount(),
                    IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                    ID_TRUE,							/* UseMutex */
                    IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                    ID_FALSE,							/* ForcePooling */
                    ID_TRUE,							/* GarbageCollection */
                    ID_TRUE,                           /* HWCacheLine */
                    IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
                != IDE_SUCCESS);			

        // MemPagePool �ʱ�ȭ
        IDE_TEST(aTBSNode->mMemPagePool.initialize(
                     IDU_MEM_SM_SVM,
                     (SChar*)"TEMP_MEMORY_POOL",
                     1,
                     SM_PAGE_SIZE,
                     smuProperty::getTempPageChunkCount(),
                     IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                     ID_TRUE,							/* UseMutex */
                     IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                     ID_FALSE,							/* ForcePooling */
                     ID_TRUE,							/* GarbageCollection */
                     ID_TRUE,                           /* HWCacheLine */
                     IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
                 != IDE_SUCCESS);			

        // PCH Memory Pool�ʱ�ȭ
        IDE_TEST(aTBSNode->mPCHMemPool.initialize(
                     IDU_MEM_SM_SVM,
                     (SChar*)"PCH_MEM_POOL",
                     1,    // ����ȭ ���� �ʴ´�.
                     ID_SIZEOF(svmPCH),
                     1024, // �ѹ��� 1024���� PCH������ �� �ִ� ũ��� �޸𸮸� Ȯ���Ѵ�.
                     IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                     ID_TRUE,							/* UseMutex */
                     IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                     ID_FALSE,							/* ForcePooling */
                     ID_TRUE,							/* GarbageCollection */
                     ID_TRUE,                           /* HWCacheLine */
                     IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
                != IDE_SUCCESS);			
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Tablespace�� �ڿ����� �����Ѵ�.
 *               Drop TBS�� ȣ��ȴ�.
 ************************************************************************/
IDE_RC svmManager::finiTBS( svmTBSNode *aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    /* BUG-39806 Valgrind Warning
     * - svmTBSDrop::dropTableSpacePending() �� ó���� ���ؼ�, DROPPED �˻縦 ��
     *   �� �ܺο��� �մϴ�.
     */

    // Free Page List ������ ����
    IDE_TEST( svmFPLManager::destroy( aTBSNode ) != IDE_SUCCESS );

    // Expand Chunk ������ ����
    IDE_TEST( svmExpandChunk::destroy( aTBSNode ) != IDE_SUCCESS );

    // BUGBUG-1548 �Ϲݸ޸��϶��� ���� ���ص� �������.
    //             ������ �޸� Pool��ü�� destroy�ϱ� ����
    IDE_TEST( freeAll( aTBSNode ) != IDE_SUCCESS );

    IDE_TEST( aTBSNode->mPCHMemPool.destroy() != IDE_SUCCESS );

    // BUG-47487: FLI ������ memPool �ı� ( Volatile ) 
    IDE_TEST( aTBSNode->mFLIMemPagePool.destroy() != IDE_SUCCESS );

    // Page Pool �����ڸ� �ı�
    IDE_TEST( aTBSNode->mMemPagePool.destroy() != IDE_SUCCESS );

    IDE_TEST( smmManager::freePCHArray( aTBSNode->mHeader.mID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ����ڰ� �����Ϸ��� �����ͺ��̽� ũ�⿡ �����ϴ�
 * �����ͺ��̽� ������ ���� ������ Page ���� ����Ѵ�.
 *
 * ����ڰ� ������ ũ��� ��Ȯ�� ��ġ�ϴ� �����ͺ��̽��� ������ �� ����
 * ������, �ϳ��� �����ͺ��̽��� �׻� Expand Chunkũ���� �����
 * �����Ǳ� �����̴�.
 *
 * aDbSize         [IN] �����Ϸ��� �����ͺ��̽� ũ��
 * aChunkPageCount [IN] �ϳ��� Expand Chunk�� ���ϴ� Page�� ��
 *
 */
ULong svmManager::calculateDbPageCount( ULong aDbSize, ULong aChunkPageCount )
{
    vULong sCalculPageCount;
    vULong sRequestedPageCount;

    IDE_DASSERT( aDbSize > 0 );
    IDE_DASSERT( aChunkPageCount > 0  );

    sRequestedPageCount = aDbSize  / SM_PAGE_SIZE;

    // Expand Chunk Page���� ����� �ǵ��� ����.
    // BUG-15288 �� Max DB SIZE�� ���� �� ����.
    sCalculPageCount =
        aChunkPageCount * (sRequestedPageCount / aChunkPageCount);

    return sCalculPageCount ;
}

/*
   Tablespace�� Meta Page�� �ʱ�ȭ�ϰ� Free Page���� �����Ѵ�.

   ChunkȮ�忡 ���� �α��� �ǽ��Ѵ�.
   
   aCreatePageCount [IN] ������ �����ͺ��̽��� ���� Page�� ��
                         Membase�� ��ϵǴ� �����ͺ��̽�
                         Meta Page ���� �����Ѵ�.
                         �� ���� smiMain::smiCalculateDBSize()�� ���ؼ� 
                         ������ ���̾�� �ϰ� smiGetMaxDBPageCount()����
                         ���� ������ Ȯ���� ���� ���̾�� �Ѵ�.
   aDbFilePageCount [IN] �ϳ��� �����ͺ��̽� ������ ���� Page�� ��
   aChunkPageCount  [IN] �ϳ��� Expand Chunk�� ���� Page�� ��


   [ �˰��� ]
   - (010) PCH(Page Control Header) Memory ������(Pool)�� �ʱ�ȭ
   - (020) 0�� Meta Page (Membase ����)�� �ʱ�ȭ
   - (030) Expand Chunk������ �ʱ�ȭ
   - (040) �ʱ� Tablespaceũ�⸸ŭ Tablespace Ȯ��(Expand Chunk �Ҵ�)
 */

IDE_RC svmManager::createTBSPages(
    svmTBSNode      * aTBSNode,
    SChar           * aDBName,
    scPageID          aCreatePageCount)
{
    UInt         sState = 0;
    scPageID     sTotalPageCount;
    
    vULong       sNewChunks;
    scPageID     sChunkPageCount =
                     (scPageID)smuProperty::getExpandChunkPageCount();

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aCreatePageCount > 0 );
    
    ////////////////////////////////////////////////////////////////
    // aTBSNode�� membase �ʱ�ȭ
    IDE_TEST( svmDatabase::initializeMembase(
                  aTBSNode,
                  aDBName,
                  sChunkPageCount )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (020) Expand Chunk������ �ʱ�ȭ
    IDE_TEST( svmExpandChunk::setChunkPageCnt( aTBSNode,
                                               sChunkPageCount )
              != IDE_SUCCESS );

    // Expand Chunk�� ���õ� Property �� üũ
    IDE_TEST( svmDatabase::checkExpandChunkProps(&aTBSNode->mMemBase)
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////////
    // (030) �ʱ� Tablespaceũ�⸸ŭ Tablespace Ȯ��(Expand Chunk �Ҵ�)
    // ������ �����ͺ��̽� Page���� ���� ���� �Ҵ��� Expand Chunk �� ���� ����
    sNewChunks = svmExpandChunk::getExpandChunkCount(
                     aTBSNode,
                     aCreatePageCount );


    // To Fix BUG-17381     create tablespace��
    //                      VOLATILE_MAX_DB_SIZE�̻����� �˴ϴ�.
    
    // �ý��ۿ��� ���� �ϳ��� Tablespace����
    // ChunkȮ���� �ϵ��� �ϴ� Mutex
    // => �� ���� Tablespace�� ���ÿ� ChunkȮ���ϴ� ��Ȳ������
    //    ��� Tablespace�� �Ҵ��� Page ũ�Ⱑ VOLATILE_MAX_DB_SIZE����
    //    ���� �� �˻��� �� ���� ����
    
    IDE_TEST( svmFPLManager::lockGlobalAllocChunkMutex()
              != IDE_SUCCESS );
    sState = 1;
        
    IDE_TEST( svmFPLManager::getTotalPageCount4AllTBS( & sTotalPageCount )
              != IDE_SUCCESS );
    
    IDE_TEST_RAISE( ( sTotalPageCount +
                      ( sNewChunks * sChunkPageCount ) ) >
                    ( smuProperty::getVolMaxDBSize() / SM_PAGE_SIZE ),
                    error_unable_to_create_cuz_vol_max_db_size );
    
    // Ʈ������� NULL�� �Ѱܼ� �α��� ���� �ʵ��� �Ѵ�.
    // �ִ� Page���� �Ѿ���� �� �ӿ��� üũ�Ѵ�.
    IDE_TEST( allocNewExpandChunks( aTBSNode,
                                    sNewChunks )
              != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( svmFPLManager::unlockGlobalAllocChunkMutex()
              != IDE_SUCCESS );
    
        
    IDE_ASSERT( aTBSNode->mMemBase.mAllocPersPageCount <=
                aTBSNode->mDBMaxPageCount );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( error_unable_to_create_cuz_vol_max_db_size );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_CREATE_CUZ_VOL_MAX_DB_SIZE,
                     aTBSNode->mHeader.mName,
                     (ULong) ( ( aCreatePageCount * SM_PAGE_SIZE) / 1024),
                     (ULong) (smuProperty::getVolMaxDBSize()/1024),
                     (ULong) ( (sTotalPageCount * SM_PAGE_SIZE ) / 1024 )
                ));
    }
    // fix BUG-29682 IDE_EXCEPTION_END�� �߸��Ǿ� ���ѷ����� �ֽ��ϴ�.
    IDE_EXCEPTION_END;
    IDE_PUSH();
    {
        switch( sState )
        {
            case 1:
                
                IDE_ASSERT( svmFPLManager::unlockGlobalAllocChunkMutex()
                            == IDE_SUCCESS );
                break;
            
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/* �������� Expand Chunk�� �߰��Ͽ� �����ͺ��̽��� Ȯ���Ѵ�.
 * aExpandChunkCount [IN] Ȯ���ϰ��� �ϴ� Expand Chunk�� ��
 */
IDE_RC svmManager::allocNewExpandChunks( svmTBSNode *  aTBSNode,
                                         UInt          aExpandChunkCount )
{
    UInt i;
    scPageID sChunkFirstPID;
    scPageID sChunkLastPID;

    // �� �Լ��� Normal Processing������ �Ҹ���Ƿ�
    // Expand ChunkȮ�� �� �����ͺ��̽� Page����
    // �ִ� Page������ �Ѿ���� üũ�Ѵ�.
    IDE_TEST_RAISE(aTBSNode->mMemBase.mAllocPersPageCount
                     + aExpandChunkCount * aTBSNode->mMemBase.mExpandChunkPageCnt
                   > aTBSNode->mDBMaxPageCount,
                   max_page_error);

    // Expand Chunk�� �����ͺ��̽��� �߰��Ͽ� �����ͺ��̽��� Ȯ���Ѵ�.
    for (i=0; i<aExpandChunkCount; i++ )
    {
        // ���� �߰��� Chunk�� ù��° Page ID�� ����Ѵ�.
        // ���ݱ��� �Ҵ��� ��� Chunk�� �� Page���� �� Chunk�� ù��° Page ID�� �ȴ�.
        sChunkFirstPID = aTBSNode->mMemBase.mCurrentExpandChunkCnt *
                         aTBSNode->mMemBase.mExpandChunkPageCnt +
                         SVM_TBS_FIRST_PAGE_ID;
        // ���� �߰��� Chunk�� ������ Page ID�� ����Ѵ�.
        sChunkLastPID  = sChunkFirstPID +
                         aTBSNode->mMemBase.mExpandChunkPageCnt - 1;

        IDE_TEST( allocNewExpandChunk( aTBSNode,
                                       sChunkFirstPID,
                                       sChunkLastPID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(max_page_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TooManyPage,
                                (ULong)aTBSNode->mDBMaxPageCount ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���� �Ҵ�� Expand Chunk�ȿ� ���ϴ� Page���� PCH Entry�� �Ҵ��Ѵ�.
 * Chunk���� Free List Info Page�� Page Memory�� �Ҵ��Ѵ�.
 *
 * ����! 1. Alloc Chunk�� Logical Redo����̹Ƿ�, Physical �α����� �ʴ´�.
 *          Chunk���� Free Page�鿡 ���ؼ��� Page Memory�� �Ҵ����� ����
 *
 * aNewChunkFirstPID [IN] Chunk���� ù��° Page
 * aNewChunkLastPID  [IN] Chunk���� ������ Page
 */
IDE_RC svmManager::fillPCHEntry4AllocChunk(svmTBSNode * aTBSNode,
                                           scPageID     aNewChunkFirstPID,
                                           scPageID     aNewChunkLastPID )
{
    UInt       sFLIPageCnt = 0 ;
    scSpaceID  sSpaceID;
    scPageID   sPID = 0 ;

    sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aNewChunkFirstPID )
                 == ID_TRUE );
    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aNewChunkLastPID )
                 == ID_TRUE );
    
    for ( sPID = aNewChunkFirstPID ;
          sPID <= aNewChunkLastPID ;
          sPID ++ )
    {
        if ( smmManager::getPCHBase( sSpaceID, sPID ) == NULL )
        {
            // PCH Entry�� �Ҵ��Ѵ�.
            IDE_TEST( allocPCHEntry( aTBSNode, sPID ) != IDE_SUCCESS );
        }

        // Free List Info Page�� Page �޸𸮸� �Ҵ��Ѵ�.
        if ( sFLIPageCnt < svmExpandChunk::getChunkFLIPageCnt(aTBSNode) )
        {
            sFLIPageCnt ++ ;

            // Restart Recovery�߿��� �ش� Page�޸𸮰� �̹� �Ҵ�Ǿ�
            // ���� �� �ִ�.
            //
            // allocAndLinkPageMemory ���� �̸� ����Ѵ�.
            // �ڼ��� ������ allocPageMemory�� �ּ��� ����
            
            IDE_TEST( allocAndLinkPageMemory( aTBSNode,
                                              sPID,          // PID
                                              SM_NULL_PID,   // prev PID
                                              SM_NULL_PID )  // next PID
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Ư�� ������ ������ŭ �����ͺ��̽��� Ȯ���Ѵ�.
 *
 * ��� Free Page List�� ���� Latch�� ������ ���� ä�� �� �Լ��� ȣ��ȴ�.
 *
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ù��° Page ID
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ������ Page ID
 */
IDE_RC svmManager::allocNewExpandChunk(svmTBSNode * aTBSNode,
                                       scPageID     aNewChunkFirstPID,
                                       scPageID     aNewChunkLastPID )
{
    UInt              sStage = 0;
    svmPageList       sArrFreeList[SVM_MAX_FPL_COUNT];

    IDE_DASSERT( aTBSNode != NULL );

    // Page ID�� SVM_TBS_FIRST_PAGE_ID���� ����� �����̱� ������
    // mDBMaxPageCount(page ����)�� ���� �� SVM_TBS_FIRST_PAGE_ID��ŭ
    // �� ���� ���ؾ� �Ѵ�.
    IDE_DASSERT( aNewChunkFirstPID - SVM_TBS_FIRST_PAGE_ID
                 < aTBSNode->mDBMaxPageCount );
    IDE_DASSERT( aNewChunkLastPID - SVM_TBS_FIRST_PAGE_ID
                 < aTBSNode->mDBMaxPageCount );

    // ��� Free Page List�� Latchȹ��
    IDE_TEST( svmFPLManager::lockAllFPLs(aTBSNode) != IDE_SUCCESS );
    sStage = 1;

    // �ϳ��� Expand Chunk�� ���ϴ� Page���� PCH Entry���� �����Ѵ�.
    IDE_TEST( fillPCHEntry4AllocChunk( aTBSNode,
                                       aNewChunkFirstPID,
                                       aNewChunkLastPID )
              != IDE_SUCCESS );

    // Logical Redo �� ���̹Ƿ� Physical Update( Next Free Page ID ����)
    // �� ���� �α��� ���� ����.
    IDE_TEST( svmFPLManager::distributeFreePages(
                  aTBSNode,
                  // Chunk���� ù��° Free Page
                  // Chunk�� �պκ��� Free List Info Page���� �����ϹǷ�,
                  // Free List Info Page��ŭ �ǳʶپ�� Free Page�� ���´�.
                  aNewChunkFirstPID + 
                  svmExpandChunk::getChunkFLIPageCnt(aTBSNode),
                  // Chunk���� ������ Free Page
                  aNewChunkLastPID,
                  ID_TRUE, // set next free page, PRJ-1548
                  sArrFreeList  )
              != IDE_SUCCESS );

    // ���ݱ��� �����ͺ��̽��� �Ҵ�� �� �������� ����
    aTBSNode->mMemBase.mAllocPersPageCount = aNewChunkLastPID - SVM_TBS_FIRST_PAGE_ID + 1;
    aTBSNode->mMemBase.mCurrentExpandChunkCnt++;

    IDE_TEST( svmFPLManager::appendPageLists2FPLs( aTBSNode, sArrFreeList )
              != IDE_SUCCESS );

    sStage = 0;
    IDE_TEST( svmFPLManager::unlockAllFPLs(aTBSNode) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1 :
            IDE_ASSERT( svmFPLManager::unlockAllFPLs(aTBSNode) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
 * Page���� �޸𸮸� �Ҵ��Ѵ�.
 *
 * FLI Page�� Next Free Page ID�� ��ũ�� �������鿡 ����
 * PCH���� Page �޸𸮸� �Ҵ��ϰ� Page Header�� Prev/Next�����͸� �����Ѵ�.
 *
 * Free List Info Page���� Next Free Page ID�� �������
 * PCH�� Page���� Page Header�� Prev/Next��ũ�� �����Ѵ�.
 *
 * Free List Info Page�� Ư���� ���� �����Ͽ� �Ҵ�� ���������� ǥ���Ѵ�.
 *
 * Free Page�� ���� �Ҵ�Ǳ� ���� �Ҹ���� ��ƾ����, ���� Free Page����
 * PCH�� Page �޸𸮸� �Ҵ��Ѵ�.
 *
 * aHeadPID   [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPID   [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC svmManager::allocFreePageMemoryList( svmTBSNode * aTBSNode,
                                            scPageID     aHeadPID,
                                            scPageID     aTailPID,
                                            vULong     * aPageCount )
{
    scPageID   sPrevPID = SM_NULL_PID;
    scPageID   sNextPID;
    scPageID   sPID;

    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID, aHeadPID )
                 == ID_TRUE );
    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID, aTailPID )
                 == ID_TRUE );
    IDE_DASSERT( aPageCount != NULL );

    vULong   sProcessedPageCnt = 0;

    // BUGBUG kmkim �� Page�� Latch�� �ɾ�� �ϴ��� ��� �ʿ�.
    // ��¦�� �����غ� ����δ� Latch���� �ʿ� ����..

    // sHeadPage���� sTailPage������ ��� Page�� ����
    sPID = aHeadPID;
    while ( sPID != SM_NULL_PID )
    {
        // Next Page ID ����
        if ( sPID == aTailPID )
        {
            // ���������� link�� Page��� ���� Page�� NULL
            sNextPID = SM_NULL_PID ;
        }
        else
        {
            // �������� �ƴ϶�� ���� Page Free Page ID�� ���´�.
            IDE_TEST( svmExpandChunk::getNextFreePage( aTBSNode,
                                                       sPID,
                                                       & sNextPID )
                      != IDE_SUCCESS );
        }

        // Free Page�̴��� PCH�� �Ҵ�Ǿ� �־�� �Ѵ�.
        IDE_ASSERT( smmManager::getPCHBase( aTBSNode->mTBSAttr.mID, sPID )  != NULL );

        // ������ �޸𸮸� �Ҵ��ϰ� �ʱ�ȭ
        IDE_TEST( allocAndLinkPageMemory( aTBSNode,
                                          sPID,
                                          sPrevPID,
                                          sNextPID ) != IDE_SUCCESS );
        
        // ���̺� �Ҵ�Ǿ��ٴ� �ǹ̷�
        // Page�� Next Free Page�� Ư���� ���� ����صд�.
        // ���� �⵿�� Page�� ���̺� �Ҵ�� Page����,
        // Free Page���� ���θ� �����ϱ� ���� ���ȴ�.
        IDE_TEST( svmExpandChunk::setNextFreePage(
                      aTBSNode,
                      sPID,
                      SVM_FLI_ALLOCATED_PID )
                  != IDE_SUCCESS );

        sProcessedPageCnt ++ ;


        sPrevPID = sPID ;

        // sPID �� aTailPID�� ��,
        // ���⿡�� sPID�� SM_NULL_PID �� �����Ǿ� loop ����
        sPID = sNextPID ;
    }


    * aPageCount = sProcessedPageCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PCH�� Page���� Page Header�� Prev/Next�����͸� �������
 * FLI Page�� Next Free Page ID�� �����Ѵ�.
 *
 * ���̺� �Ҵ�Ǿ��� Page�� Free Page�� �ݳ��Ǳ� ���� ����
 * �Ҹ���� ��ƾ�̴�.
 *
 * aHeadPage  [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPage  [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC svmManager::linkFreePageList( svmTBSNode * aTBSNode,
                                     void       * aHeadPage,
                                     void       * aTailPage,
                                     vULong     * aPageCount )
{
    scPageID    sPID, sTailPID, sNextPID;
    vULong      sProcessedPageCnt = 0;
    void      * sPagePtr;

    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );
    IDE_DASSERT( aPageCount != NULL );

    sPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );
    // sHeadPage���� sTailPage������ ��� Page�� ����

    do
    {
        if ( sPID == sTailPID ) // ������ �������� ���
        {
            sNextPID = SM_NULL_PID ;
        }
        else  // ������ �������� �ƴ� ���
        {
            // Free List Info Page�� Next Free Page ID�� ����Ѵ�.
            IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                                    sPID,
                                                    &sPagePtr )
                        == IDE_SUCCESS );
            sNextPID = smLayerCallback::getNextPersPageID( sPagePtr );

        }

        // Free List Info Page�� Next Free Page ID ����
        IDE_TEST( svmExpandChunk::setNextFreePage( aTBSNode,
                                                   sPID,
                                                   sNextPID )
                  != IDE_SUCCESS );

        sProcessedPageCnt ++ ;

        sPID = sNextPID ;
    } while ( sPID != SM_NULL_PID );

    * aPageCount = sProcessedPageCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* PCH�� Page���� Page Header�� Prev/Next�����͸� �������
 * PCH �� Page �޸𸮸� �ݳ��Ѵ�.
 *
 * ���̺� �Ҵ�Ǿ��� Page�� Free Page�� �ݳ��� �Ŀ�
 * �Ҹ���� ��ƾ����, Page���� PCH�� Page �޸𸮸� �����Ѵ�.
 *
 * aHeadPage  [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPage  [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC svmManager::freeFreePageMemoryList( svmTBSNode * aTBSNode,
                                           void       * aHeadPage,
                                           void       * aTailPage,
                                           vULong     * aPageCount )
{
    scPageID    sPID, sTailPID, sNextPID;
    vULong      sProcessedPageCnt = 0;
    void      * sPagePtr;

    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );
    IDE_DASSERT( aPageCount != NULL );

    sPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );
    // sHeadPage���� sTailPage������ ��� Page�� ����

    do
    {
        if ( sPID == sTailPID ) // ������ �������� ���
        {
            sNextPID = SM_NULL_PID ;
        }
        else  // ������ �������� �ƴ� ���
        {
            // Free List Info Page�� Next Free Page ID�� ����Ѵ�.
            IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                                    sPID,
                                                    &sPagePtr )
                        == IDE_SUCCESS );
            sNextPID = smLayerCallback::getNextPersPageID( sPagePtr );
        }

        IDE_TEST( freePageMemory( aTBSNode, sPID ) != IDE_SUCCESS );

        sProcessedPageCnt ++ ;

        sPID = sNextPID ;
    } while ( sPID != SM_NULL_PID );

    * aPageCount = sProcessedPageCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


#if 0  //not used
/** DB�κ��� �ϳ��� Page�� �Ҵ�޴´�.
 *
 * aAllocatedPage  [OUT] �Ҵ���� ������
 */
IDE_RC svmManager::allocatePersPage (void        *aTrans,
                                     scSpaceID    aSpaceID,
                                     void       **aAllocatedPage)
{
    IDE_ASSERT( aAllocatedPage != NULL);

    IDE_TEST( allocatePersPageList( aTrans,
                                    aSpaceID,
                                    1,
                                    aAllocatedPage,
                                    aAllocatedPage )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
#endif

/** DB�κ��� Page�� ������ �Ҵ�޴´�.
 *
 * �������� Page�� ���ÿ� �Ҵ������ DB Page�Ҵ� Ƚ���� ���� �� ������,
 * �̸� ���� DB Free Page List ���� ���ü��� ����ų �� �ִ�.
 *
 * ���� Page���� ���� �����ϱ� ���� aHeadPage���� aTailPage����
 * Page Header�� Prev/Next�����ͷ� �������ش�.
 *
 * aPageCount [IN] �Ҵ���� �������� ��
 * aHeadPage  [OUT] �Ҵ���� ������ �� ù��° ������
 * aTailPage  [OUT] �Ҵ���� ������ �� ������ ������
 */
IDE_RC svmManager::allocatePersPageList (void      *  aTrans,
                                         scSpaceID    aSpaceID,
                                         UInt         aPageCount,
                                         void     **  aHeadPage,
                                         void     **  aTailPage,
                                         UInt      *  aAllocPageCnt )
{

    scPageID  sHeadPID = SM_NULL_PID;
    scPageID  sTailPID = SM_NULL_PID;
    vULong    sLinkedPageCount;
    UInt      sPageListID;
    svmTBSNode * sTBSNode;
    UInt      sPageCount = 0;
    UInt      sTotalPageCount = 0;
    UInt      sState = 0;

    IDE_DASSERT( aPageCount != 0 );
    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );

    // Ʈ������� RSGroupID�� �̿��Ͽ� �������� Free Page List�� �ϳ��� �����Ѵ�.
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    if( aPageCount != 1 )
    {
        /* BUG-46861 TABLE_ALLOC_PAGE_COUNT ������Ƽ�� ����� �ټ��� �������� �Ҵ��� ��� �ټ��� ������ �Ҵ�����
           �Ҵ�� �������� MEM_MAX_DB_SIZE�� �Ѿ�� �ʵ��� �����ؾ� �Ѵ�.
           ����, TBS lock�� ���� �ʰ� ���� ������� DB�� ũ�⸦ üũ�ϱ� ������
           ���� ������� ����+���� �Ҵ��� ������ �ִ� ��� ���� ������ �������� �ʾƵ�
           �׿� ������ ���(1�� Chunk������ ������ ���̳� ���)���� ����ó�� 1���� �Ҵ��Ѵ�. */
        IDE_TEST( svmFPLManager::lockGlobalAllocChunkMutex() != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( svmFPLManager::getTotalPageCount4AllTBS( & sTotalPageCount ) != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( svmFPLManager::unlockGlobalAllocChunkMutex() != IDE_SUCCESS );

        if( ( smiGetStartupPhase() == SMI_STARTUP_SERVICE ) &&
            ( ( sTotalPageCount + aPageCount ) < 
              ( ( smuProperty::getVolMaxDBSize() / SM_PAGE_SIZE ) - sTBSNode->mMemBase.mExpandChunkPageCnt ) ) )
        {
            sPageCount = aPageCount;
        }
        else
        {
            /* �Ҵ��� ����+�Ҵ��� ������ VOLATILE_MAX_DB_SIZE�� �����ϰų� ���� ���� �߿��� ����ó�� �ϳ��� �Ҵ��Ѵ�. */
            sPageCount = 1;
        }
    }
    else
    {
        sPageCount = 1;
    }

    // sPageListID�� �ش��ϴ� Free Page List�� �ּ��� aPageCount����
    // Free Page�� �������� �����ϸ鼭 latch�� ȹ���Ѵ�.
    //
    // aPageCount��ŭ Free Page List�� ������ �����ϱ� ���ؼ�
    //
    // 1. Free Page List���� Free Page���� �̵���ų �� �ִ�. => Physical �α�
    // 2. Expand Chunk�� �Ҵ��� �� �ִ�.
    //     => Chunk�Ҵ��� Logical �α�.
    //       -> Recovery�� svmManager::allocNewExpandChunkȣ���Ͽ� Logical Redo
    IDE_TEST( svmFPLManager::lockListAndPreparePages( sTBSNode,
                                                      aTrans,
                                                      (svmFPLNo)sPageListID,
                                                      sPageCount )
              != IDE_SUCCESS );

    // Ʈ������� ����ϴ� Free Page List���� Free Page���� �����.
    // DB Free Page List�� ���� �α��� ���⿡�� �̷������.
    IDE_TEST( svmFPLManager::removeFreePagesFromList( sTBSNode,
                                                      aTrans,
                                                      (svmFPLNo)sPageListID,
                                                      sPageCount,
                                                      & sHeadPID,
                                                      & sTailPID )
              != IDE_SUCCESS );

    // Head���� Tail���� ��� Page�� ����
    // Page Header�� Prev/Next ��ũ�� ���� �����Ų��.
    IDE_TEST( allocFreePageMemoryList ( sTBSNode,
                                        sHeadPID,
                                        sTailPID,
                                        & sLinkedPageCount )
              != IDE_SUCCESS );

    IDE_ASSERT( sLinkedPageCount == sPageCount );

    IDE_ASSERT( smmManager::getPersPagePtr( sTBSNode->mTBSAttr.mID, 
                                            sHeadPID,
                                            aHeadPage )
                == IDE_SUCCESS );
    IDE_ASSERT( smmManager::getPersPagePtr( sTBSNode->mTBSAttr.mID, 
                                            sTailPID,
                                            aTailPage )
                == IDE_SUCCESS );


    // �������� �Ҵ���� Free Page List�� Latch�� Ǯ���ش�.
    IDE_TEST( svmFPLManager::unlockFreePageList( sTBSNode,
                                                 (svmFPLNo)sPageListID )
              != IDE_SUCCESS );

    *aAllocPageCnt = sPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1: 
            IDE_ASSERT( svmFPLManager::unlockGlobalAllocChunkMutex()
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

#if 0
/*
 * �ϳ��� Page�� �����ͺ��̽��� �ݳ��Ѵ�
 *
 * aToBeFreePage [IN] �ݳ��� Page
 */
IDE_RC svmManager::freePersPage (void       * aTrans,
                                 scSpaceID    aSpaceID,
                                 void        *aToBeFreePage )
{
    IDE_DASSERT( aToBeFreePage != NULL );

    IDE_TEST( freePersPageList( aTrans,
                                aSpaceID,
                                aToBeFreePage,
                                aToBeFreePage )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

/*
 * �������� Page�� �Ѳ����� �����ͺ��̽��� �ݳ��Ѵ�.
 *
 * aHeadPage���� aTailPage����
 * Page Header�� Prev/Next�����ͷ� ����Ǿ� �־�� �Ѵ�.
 *
 * ���� Free Page���� ���� ���� Free Page List �� Page�� Free �Ѵ�.
 *
 * aTrans    [IN] Page�� �ݳ��Ϸ��� Ʈ�����
 * aHeadPage [IN] �ݳ��� ù��° Page
 * aHeadPage [IN] �ݳ��� ������ Page
 *
 */
IDE_RC svmManager::freePersPageList (void       * aTrans,
                                     scSpaceID    aSpaceID,
                                     void       * aHeadPage,
                                     void       * aTailPage)
{
    UInt       sPageListID;
    vULong     sLinkedPageCount    ;
    vULong     sFreePageCount    ;
    scPageID   sHeadPID, sTailPID ;
    UInt       sStage = 0;
    svmTBSNode * sTBSNode;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );

    IDE_TEST(sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                       (void**)&sTBSNode )
             != IDE_SUCCESS);

    sHeadPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );

    // Free List Info Page�ȿ� Free Page���� Link�� ����Ѵ�.
    IDE_TEST( linkFreePageList( sTBSNode,
                                aHeadPage,
                                aTailPage,
                                & sLinkedPageCount )
              != IDE_SUCCESS );

    // Free Page�� �ݳ��Ѵ�.
    // �̷��� �ص� allocFreePage���� Page���� ���� Free Page List����
    // ������ ������� ��ƾ������ Free Page List���� �뷱���� �ȴ�.

    // Ʈ������� RSGroupID�� �̿��Ͽ� �������� Free Page List�� �ϳ��� �����Ѵ�.
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    // Page�� �ݳ��� Free Page List�� Latchȹ��
    IDE_TEST( svmFPLManager::lockFreePageList(sTBSNode, (svmFPLNo)sPageListID)
              != IDE_SUCCESS );
    sStage = 1;

    // Free Page List �� Page�� �ݳ��Ѵ�.
    // DB Free Page List�� ���� �α��� �߻��Ѵ�.
    IDE_TEST( svmFPLManager::appendFreePagesToList( sTBSNode,
                                                    aTrans,
                                                    sPageListID,
                                                    sLinkedPageCount,
                                                    sHeadPID,
                                                    sTailPID )
              != IDE_SUCCESS );

    IDE_TEST( freeFreePageMemoryList( sTBSNode,
                                      aHeadPage,
                                      aTailPage,
                                      & sFreePageCount )
              != IDE_SUCCESS );

    IDE_ASSERT( sFreePageCount == sLinkedPageCount );

    // ����! Page �ݳ� ���� Logging�� �ϴ�
    // Page Free�� �ϱ� ������ Flush�� �� �ʿ䰡 ����.

    sStage = 0;
    // Free Page List���� LatchǬ��
    IDE_TEST( svmFPLManager::unlockFreePageList( sTBSNode,
                                                 (svmFPLNo)sPageListID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1:
            IDE_ASSERT( svmFPLManager::unlockFreePageList(
                            sTBSNode,
                            (svmFPLNo)sPageListID )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }
    sStage = 0;

    IDE_POP();

    return IDE_FAILURE;
}


/* --------------------------------------------------------------------------
 *  SECTION : Latch Control
 * -------------------------------------------------------------------------*/

/*
 * Ư�� Page�� S��ġ�� ȹ���Ѵ�. ( ����� X��ġ�� �����Ǿ� �ִ� )
 */
IDE_RC
svmManager::holdPageSLatch(scSpaceID aSpaceID,
                           scPageID  aPageID )
{
    svmPCH    * sPCH ;

    IDE_DASSERT( smmManager::isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    sPCH = (svmPCH*)smmManager::getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH != NULL );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.lockRead( NULL,/* idvSQL* */
                                  NULL ) /* sWeArgs*/
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * Ư�� Page�� X��ġ�� ȹ���Ѵ�.
 */
IDE_RC
svmManager::holdPageXLatch(scSpaceID aSpaceID,
                           scPageID  aPageID)
{
    svmPCH    * sPCH;

    IDE_DASSERT( smmManager::isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    sPCH = (svmPCH*)smmManager::getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH != NULL );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.lockWrite( NULL,/* idvSQL* */
                                   NULL ) /* sWeArgs*/
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * Ư�� Page���� ��ġ�� Ǯ���ش�.
 */
IDE_RC
svmManager::releasePageLatch(scSpaceID aSpaceID,
                             scPageID  aPageID)
{
    svmPCH * sPCH;

    IDE_DASSERT( smmManager::isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    sPCH = (svmPCH*)smmManager::getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.unlock( ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* ------------------------------------------------
 *  alloc & free PCH + Page
 * ----------------------------------------------*/

IDE_RC svmManager::allocPCHEntry(svmTBSNode *  aTBSNode,
                                 scPageID      aPageID)
{

    SChar       sMutexName[128];
    svmPCH    * sCurPCH;
    smPCSlot * sPCHSlot;
    scSpaceID   sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_ASSERT(sSpaceID < SC_MAX_SPACE_COUNT);
    
    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPageID ) == ID_TRUE );

    sPCHSlot = smmManager::getPCHSlot( sSpaceID, aPageID );
    IDE_ASSERT( sPCHSlot->mPCH == NULL);

    /* svmManager_allocPCHEntry_alloc_CurPCH.tc */
    IDU_FIT_POINT("svmManager::allocPCHEntry::alloc::CurPCH");
    IDE_TEST( aTBSNode->mPCHMemPool.alloc((void **)&sCurPCH) != IDE_SUCCESS);
    sPCHSlot->mPCH = sCurPCH;

    /* ------------------------------------------------
     * [] mutex �ʱ�ȭ
     * ----------------------------------------------*/

    idlOS::snprintf( sMutexName,
                     128,
                     "SVMPCH_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_PAGE_MEMORY_LATCH",
                     (UInt) sSpaceID,
                     (UInt) aPageID );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_ASSERT( sCurPCH->mPageMemLatch.initialize(  
                                      sMutexName,
                                      IDU_LATCH_TYPE_NATIVE )
                == IDE_SUCCESS );

    sPCHSlot->mPagePtr = NULL;
    sCurPCH->mNxtScanPID       = SM_NULL_PID;
    sCurPCH->mPrvScanPID       = SM_NULL_PID;
    sCurPCH->mModifySeqForScan = 0;
    sCurPCH->mFreePageHeader   = NULL ;
    
    // svmPCH.mFreePageHeader �ʱ�ȭ
    IDE_TEST( smLayerCallback::initializeFreePageHeader( sSpaceID, aPageID )
              != IDE_SUCCESS );
    IDE_ASSERT( sCurPCH->mFreePageHeader != NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/* PCH ����
 *
 * aPID      [IN] PCH�� �����ϰ��� �ϴ� Page ID
 * aPageFree [IN] PCH�Ӹ� �ƴ϶� �� ���� Page �޸𸮵� ������ ������ ����
 */

IDE_RC svmManager::freePCHEntry(svmTBSNode * aTBSNode,
                                scPageID     aPID)
{
    svmPCH    * sCurPCH;
    smPCSlot * sPCHSlot;
    scSpaceID   sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPID ) == ID_TRUE );

    sPCHSlot = smmManager::getPCHSlot( sSpaceID, aPID );
    sCurPCH = sPCHSlot->mPCH;

    IDE_ASSERT(sCurPCH != NULL);

    // svmPCH.mFreePageHeader ����
    IDE_TEST( smLayerCallback::destroyFreePageHeader( sSpaceID, aPID )
              != IDE_SUCCESS );

    // Free Page��� Page�޸𸮰� �̹� �ݳ��Ǿ�
    // �޸𸮸� Free���� �ʾƵ� �ȴ�.
    // Page �޸𸮰� �ִ� ��쿡�� �޸𸮸� �ݳ��Ѵ�.
    if ( sPCHSlot->mPagePtr != NULL )
    {
        IDE_TEST( freePageMemory( aTBSNode, aPID ) != IDE_SUCCESS );
    }

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sCurPCH->mPageMemLatch.destroy() != IDE_SUCCESS);

    sCurPCH->mNxtScanPID       = SM_NULL_PID;
    sCurPCH->mPrvScanPID       = SM_NULL_PID;
    sCurPCH->mModifySeqForScan = 0;

    IDE_TEST( aTBSNode->mPCHMemPool.memfree(sCurPCH) != IDE_SUCCESS);

    sPCHSlot->mPCH = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * �����ͺ��̽��� PCH, Page Memory�� ��� Free�Ѵ�.
 */
IDE_RC svmManager::freeAll(svmTBSNode * aTBSNode)
{
    scPageID i;

    for (i = 0; i < aTBSNode->mDBMaxPageCount; i++)
    {
        svmPCH *sPCH = (svmPCH*)smmManager::getPCH(aTBSNode->mTBSAttr.mID, i);

        if (sPCH != NULL)
        {
            IDE_TEST( freePCHEntry(aTBSNode, i) != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* �����ͺ��̽� Ÿ�Կ� ���� �����޸𸮳� �Ϲ� �޸𸮸� ������ �޸𸮷� �Ҵ��Ѵ�
 *
 * aPage [OUT] �Ҵ�� Page �޸�
 */
IDE_RC svmManager::allocPage( svmTBSNode*   aTBSNode, 
                              svmTempPage** aPage,
                              idBool        aIsDataPage )
{
    IDE_DASSERT( aPage != NULL );

    /* svmManager_allocPage_alloc_Page.tc */
    IDU_FIT_POINT("svmManager::allocPage::alloc::Page");
    
    // BUG-47487: DATA / FLI ������ �޸� alloc �и� ( Volatile ) 
    if ( aIsDataPage == ID_TRUE )
    {
        IDE_TEST( aTBSNode->mMemPagePool.alloc( (void **)aPage )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( aTBSNode->mFLIMemPagePool.alloc( (void **)aPage )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �����ͺ��̽�Ÿ�Կ����� Page�޸𸮸� �����޸𸮳� �Ϲݸ޸𸮷� �����Ѵ�.
 *
 * aPage [IN] ������ Page �޸�
 */
IDE_RC svmManager::freePage( svmTBSNode*  aTBSNode, 
                             svmTempPage* aPage,
                             idBool       aIsDataPage )
{
    IDE_DASSERT( aPage != NULL );

    // BUG-47487: DATA / FLI ������ �޸� free �и� ( Volatile )
    if ( aIsDataPage == ID_TRUE )
    {
        IDE_TEST( aTBSNode->mMemPagePool.memfree( aPage )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( aTBSNode->mFLIMemPagePool.memfree( aPage )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Ư�� Page�� PCH���� Page Memory�� �Ҵ��Ѵ�.
 *
 * aPID [IN] Page Memory�� �Ҵ��� Page�� ID
 */
IDE_RC svmManager::allocPageMemory( svmTBSNode * aTBSNode, scPageID aPID )
{
    scSpaceID sSpaceID = aTBSNode->mTBSAttr.mID;
    smPCSlot * sPCHSlot;

    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPID ) == ID_TRUE );
   
    sPCHSlot = smmManager::getPCHSlot( sSpaceID, aPID );
    
    IDE_ASSERT( sPCHSlot->mPCH != NULL );

    // Page Memory�� �Ҵ�Ǿ� ���� �ʾƾ� �Ѵ�.
    IDE_ASSERT( sPCHSlot->mPagePtr == NULL );

    if ( sPCHSlot->mPagePtr == NULL )
    {
        // BUG-47487: DATA / FLI ������ ��� ������ alloc ( Volatile )
        if ( ( svmExpandChunk::isFLIPageID ( aTBSNode, aPID ) == ID_TRUE ) )
        {
            //FLI
            IDE_TEST( allocPage( aTBSNode, 
                                 (svmTempPage **) & sPCHSlot->mPagePtr,
                                 ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            //DATA
            IDE_TEST( allocPage( aTBSNode, 
                                 (svmTempPage **) & sPCHSlot->mPagePtr )
                      != IDE_SUCCESS );
        }   
    }

#ifdef DEBUG_SVM_FILL_GARBAGE_PAGE
    idlOS::memset( sPCH->m_page, 0x43, SM_PAGE_SIZE );
#endif // DEBUG

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * ������ �޸𸮸� �Ҵ��ϰ�, �ش� Page�� �ʱ�ȭ�Ѵ�.
 * �ʿ��� ���, ������ �ʱ�ȭ�� ���� �α��� �ǽ��Ѵ�
 *
 * aPID     [IN] ������ �޸𸮸� �Ҵ��ϰ� �ʱ�ȭ�� ������ ID
 * aPrevPID [IN] �Ҵ��� �������� ���� Page ID
 * aNextPID [IN] �Ҵ��� �������� ���� Page ID
 */
IDE_RC svmManager::allocAndLinkPageMemory( svmTBSNode * aTBSNode,
                                           scPageID     aPID,
                                           scPageID     aPrevPID,
                                           scPageID     aNextPID )
{
    scSpaceID  sSpaceID = aTBSNode->mTBSAttr.mID;
    smPCSlot * sPCHSlot;
    IDE_DASSERT( aPID != SM_NULL_PID );
    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPID ) == ID_TRUE );

#ifdef DEBUG
    if ( aPrevPID != SM_NULL_PID )
    {
        IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPrevPID ) == ID_TRUE );
    }

    if ( aNextPID != SM_NULL_PID )
    {
        IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aNextPID ) == ID_TRUE );
    }
#endif
    sPCHSlot = smmManager::getPCHSlot( sSpaceID, aPID );
    IDE_ASSERT( sPCHSlot->mPCH != NULL );

    // Page Memory�� �Ҵ��Ѵ�.
    IDE_TEST( allocPageMemory( aTBSNode, aPID ) != IDE_SUCCESS );

    smLayerCallback::linkPersPage( sPCHSlot->mPagePtr,
                                   aPID,
                                   aPrevPID,
                                   aNextPID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Ư�� Page�� PCH���� Page Memory�� �����Ѵ�.
 *
 * aPID [IN] Page Memory�� �ݳ��� Page�� ID
 */
IDE_RC svmManager::freePageMemory( svmTBSNode * aTBSNode, scPageID aPID )
{
    scSpaceID sSpaceID = aTBSNode->mTBSAttr.mID;
    smPCSlot * sPCHSlot;

    IDE_DASSERT( smmManager::isValidPageID( sSpaceID, aPID ) == ID_TRUE );
    
    sPCHSlot = smmManager::getPCHSlot( sSpaceID, aPID );
    
    IDE_ASSERT( sPCHSlot->mPCH != NULL );

    // Page Memory�� �Ҵ�Ǿ� �־�� �Ѵ�.
    IDE_ASSERT( sPCHSlot->mPagePtr != NULL );

    // BUG-47487: DATA / FLI ������ ��� ������ free ( Volatile )
    if ( ( svmExpandChunk::isFLIPageID( aTBSNode, aPID ) == ID_TRUE ) )
    {
        //FLI
        IDE_TEST( freePage( aTBSNode, 
                            (svmTempPage*) sPCHSlot->mPagePtr, 
                            ID_FALSE )
                  != IDE_SUCCESS );
    }
    else
    {
        //DATA
        IDE_TEST( freePage( aTBSNode, 
                            (svmTempPage*) sPCHSlot->mPagePtr )
                  != IDE_SUCCESS );
    }

    sPCHSlot->mPagePtr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svmManager::allocPageAlignedPtr( UInt    a_nSize,
                                        void**  a_pMem,
                                        void**  a_pAlignedMem )
{
    static UInt s_nPageSize = idlOS::getpagesize();
    void  * s_pMem;
    void  * s_pAlignedMem;
    UInt    sState  = 0;

    s_pMem = NULL;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SVM,
                               a_nSize + s_nPageSize - 1,
                               &s_pMem)
             != IDE_SUCCESS);
    sState = 1;

    idlOS::memset(s_pMem, 0, a_nSize + s_nPageSize - 1);

    s_pAlignedMem = idlOS::align(s_pMem, s_nPageSize);

    *a_pMem = s_pMem;
    *a_pAlignedMem = s_pAlignedMem;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( s_pMem ) == IDE_SUCCESS );
            s_pMem = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

// Base Page ( 0�� Page ) �� Latch�� �Ǵ�
// 0�� Page�� �����ϴ� Transaction���� ������ �����Ѵ�.
IDE_RC svmManager::lockBasePage(svmTBSNode * aTBSNode)
{
    IDE_TEST( svmFPLManager::lockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );

    IDE_TEST( svmFPLManager::lockAllFPLs(aTBSNode) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL("Fatal Error at svmManager::lockBasePage");

    return IDE_FAILURE;
}

// Base Page ( 0�� Page ) ���� Latch�� Ǭ��.
// lockBasePage�� ���� Latch�� ��� �����Ѵ�
IDE_RC svmManager::unlockBasePage(svmTBSNode * aTBSNode)
{
    IDE_TEST( svmFPLManager::unlockAllFPLs(aTBSNode) != IDE_SUCCESS);
    IDE_TEST( svmFPLManager::unlockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL("Fatal Error at svmManager::unlockBasePage");

    return IDE_FAILURE;
}
