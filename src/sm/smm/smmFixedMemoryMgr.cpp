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
 * $Id: smmFixedMemoryMgr.cpp 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smmShmKeyMgr.h>
#include <smmReq.h>

iduMemListOld   smmFixedMemoryMgr::mSCHMemList;

smmFixedMemoryMgr::smmFixedMemoryMgr()
{
}

IDE_RC smmFixedMemoryMgr::initializeStatic()
{
    
    IDE_TEST( smmShmKeyMgr::initializeStatic() != IDE_SUCCESS );
    
    IDE_TEST( mSCHMemList.initialize(IDU_MEM_SM_SMM,
                                     0,
                                     (SChar*) "SCH_LIST",
                                     ID_SIZEOF(smmSCH),
                                     32,
                                     8 )
             != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smmFixedMemoryMgr::destroyStatic()
{
    IDE_TEST( mSCHMemList.destroy() != IDE_SUCCESS );

    IDE_TEST( smmShmKeyMgr::destroyStatic() != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

IDE_RC smmFixedMemoryMgr::initialize(smmTBSNode * aTBSNode)
{

    SChar sBuffer[128];
    key_t sShmKey;
    
    sShmKey = aTBSNode->mTBSAttr.mMemAttr.mShmKey;
    aTBSNode->mBaseSCH.m_shm_id = 0;
    aTBSNode->mBaseSCH.m_header = NULL;
    aTBSNode->mBaseSCH.m_next   = NULL;
    aTBSNode->mTailSCH          = NULL;

    idlOS::memset(&aTBSNode->mTempMemBase, 0, ID_SIZEOF(smmTempMemBase));
    aTBSNode->mTempMemBase.m_first_free_temp_page  = NULL;
    aTBSNode->mTempMemBase.m_alloc_temp_page_count = 0;
    aTBSNode->mTempMemBase.m_free_temp_page_count  = 0;

    // To Fix PR-12974
    // SBUG-3 Mutex Naming
    idlOS::memset(sBuffer, 0, 128);

    idlOS::snprintf( sBuffer,
                     128,
                     "SMM_FIXED_PAGE_POOL_MUTEX_FOR_SHMKEY_%"ID_UINT32_FMT,
                     (UInt) sShmKey );
    
    IDE_TEST(aTBSNode->mTempMemBase.m_mutex.initialize(
                 sBuffer,
                 IDU_MUTEX_KIND_NATIVE,
                 IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS);
    
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC smmFixedMemoryMgr::checkExist(key_t         aShmKey,
                                     idBool&       aExist,
                                     smmShmHeader *aHeader)
{

    smmShmHeader *sShmHeader;
    SInt          sFlag;
    PDL_HANDLE    sShmID;

#if !defined (NTO_QNX) && !defined (VC_WIN32)
    sFlag       = 0600; /*|IPC_CREAT|IPC_EXCL*/
#elif defined (USE_WIN32IPC_DAEMON)
    sFlag       = 0600;
#else
    sFlag       = IPC_CREAT | 0600;
#endif
    aExist     = ID_FALSE;

    /* [1] shmid ��� */
    sShmID = idlOS::shmget( aShmKey, SMM_CACHE_ALIGNED_SHM_HEADER_SIZE, sFlag );


    if (sShmID == PDL_INVALID_HANDLE)
    {
        switch(errno)
        {
        case ENOENT: // �ش� Key�� ���� ID ���� : ���� => ����
            break;
        case EACCES: // ����������, ������ ���� : ����!
            IDE_RAISE(no_permission_error);
        default:
            IDE_RAISE(shmget_error);
        }
    }
    else
    {
        /* [2] attach ����  */
        sShmHeader = (smmShmHeader *)idlOS::shmat(sShmID, 0, sFlag);
        IDE_TEST_RAISE(sShmHeader == (smmShmHeader *)-1, shmat_error);

        if (aHeader != NULL)
        {
        /* [3] shm header copy  */
            idlOS::memcpy(aHeader, sShmHeader, ID_SIZEOF(smmShmHeader) );
        }
#if defined (NTO_QNX) || (defined (VC_WIN32) && !defined (USE_WIN32IPC_DAEMON))
        /* [5] control(IPC_RMID) */
        IDE_TEST_RAISE(idlOS::shmctl(sShmID, IPC_RMID, NULL) < 0, shmdt_error);
#endif
        /* [4] detach */
        IDE_TEST_RAISE(idlOS::shmdt( (char*)sShmHeader ) < 0, shmdt_error);

#if !defined (NTO_QNX) && !defined (VC_WIN32)
        aExist = ID_TRUE; // �̹� ���� �޸𸮰� ������. OK
#elif defined (USE_WIN32IPC_DAEMON)
        aExist = ID_TRUE; // �̹� ���� �޸𸮰� ������. OK
#else
        aExist = ID_FALSE;
#endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(shmget_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmGet));
    }
    IDE_EXCEPTION(shmat_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmAt));
    }
    IDE_EXCEPTION(no_permission_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Shm_No_Permission));
    }
    IDE_EXCEPTION(shmdt_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_SysShmDt));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 ----------------------------------------------------------------------------
    checkExist() ���Ŀ� ȣ��ǹǷ�, ������ errno �˻� �ʿ� ����!
    attach for creation whole database size
 
 ----------------------------------------------------------------------------
 -  PROJ-1548 Memory Tablespace
   - �ٸ� Tablespace�� �����޸� Chunk�� Attach�Ǵ� ��Ȳ Detect
     - ������ �ٸ� Tablespace�� �����޸𸮸� Attach�� ���� ����.
     - ������ Ȯ���� üũ�ϴ� �ǹ̷� �����޸� Attach��
       Tablespace ID�� üũ�Ѵ�.
     - ��� : �����޸����(smmShmHeader)�� TBSID�� �־� 
       Attach�Ϸ��� TBSID�� �����޸��� TBSID�� ������ üũ�Ѵ�.
*/

IDE_RC smmFixedMemoryMgr::attach(smmTBSNode * aTBSNode,
                                 smmShmHeader *aRsvShmHeader)
{


    smmShmHeader *sNewShmHeader = NULL;
    SInt          sFlag;
    PDL_HANDLE    sShmID;
    smmSCH       *sSCH;
    key_t         sCurKey;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aRsvShmHeader != NULL);
    
    sFlag       = 0600; /*|IPC_CREAT|IPC_EXCL*/

    /* smmFixedMemoryMgr_attach_alloc_SCH.tc */
    IDU_FIT_POINT("smmFixedMemoryMgr::attach::alloc::SCH");
    IDE_TEST(mSCHMemList.alloc((void **)&sSCH) != IDE_SUCCESS);

    
    /* ------------------------------------------------
     * [1] Base SHM Chunk�� ���� attach
     * ----------------------------------------------*/
    sCurKey = aTBSNode->mTBSAttr.mMemAttr.mShmKey;
    
    sShmID  = idlOS::shmget(sCurKey,
                            SMM_SHM_DB_SIZE(aRsvShmHeader->m_page_count),
                            sFlag );
    IDE_TEST_RAISE( sShmID == PDL_INVALID_HANDLE, not_exist_error);

    sNewShmHeader  = (smmShmHeader *)idlOS::shmat(sShmID, 0, sFlag);
    IDE_TEST_RAISE(sNewShmHeader == (smmShmHeader *)-1, shmat_error);

    // �����޸� Key�����ڿ��� ���� ������� Key�� �˷��ش�.
    IDE_TEST( smmShmKeyMgr::notifyUsedKey( sCurKey )
              != IDE_SUCCESS );
    
    sSCH->m_header = sNewShmHeader;
    sSCH->m_shm_id = (SInt)sShmID;
    sSCH->m_next   = NULL;

    aTBSNode->mBaseSCH.m_next = sSCH;
    aTBSNode->mTailSCH        = sSCH;
    //mMemBase       = (smmMemBase *)((UChar *)sNewShmHeader + SMM_MEMBASE_OFFSET);

    IDE_TEST_RAISE(sNewShmHeader->m_versionID != smVersionID,
                   version_mismatch_error);
    IDE_TEST_RAISE(sNewShmHeader->m_valid_state != ID_TRUE,
                   invalid_shm_error);

    //  PROJ-1548 Memory Tablespace
    // �ٸ� Tablespace�� �����޸� Chunk�� Attach�Ǵ� ��Ȳ Detect
    IDE_TEST_RAISE(sNewShmHeader->mTBSID != aTBSNode->mHeader.mID,
                   tbsid_mismatch_error);
    

    /* ------------------------------------------------
     * [2] Sub SHM Chunk attach
     * smmShmHeader �� ������ �����޸𸮿� �����ϴ� �����̰�,
     * �̰Ϳ� ���� �����޸��� key�� ����Ǿ��ִ�.
     * �� Ű�� �������� ���� ���� �޸��� ��ġ�� ��Ȯ�ϰ� attach�� �� �ִ�.
     * smmSCH�� ������ ������ �������� ���� setting�ϸ鼭 �����Ѵ�.
     * ----------------------------------------------*/
    while (aTBSNode->mTailSCH->m_header->m_next_key != 0)
    {
        smmShmHeader  sTempShmHeader;
        idBool        sExist = ID_FALSE;
        smmSCH       *sNewSCH;

        IDE_TEST(checkExist(aTBSNode->mTailSCH->m_header->m_next_key,
                            sExist,
                            &sTempShmHeader) != IDE_SUCCESS);
        IDE_TEST_RAISE(sExist == ID_FALSE, not_exist_error);

        /* smmFixedMemoryMgr_attach_alloc_NewSCH.tc */
        IDU_FIT_POINT("smmFixedMemoryMgr::attach::alloc::NewSCH");
        IDE_TEST(mSCHMemList.alloc((void **)&sNewSCH)
                 != IDE_SUCCESS);
        // ������.
        sCurKey = aTBSNode->mTailSCH->m_header->m_next_key;
        sShmID  = idlOS::shmget(aTBSNode->mTailSCH->m_header->m_next_key,
                                SMM_SHM_DB_SIZE(sTempShmHeader.m_page_count),
                                sFlag );
        
        IDE_TEST_RAISE( sShmID == PDL_INVALID_HANDLE, shmget_error);

        IDE_TEST( smmShmKeyMgr::notifyUsedKey( sCurKey )
                  != IDE_SUCCESS );
        
        sNewShmHeader  = (smmShmHeader *)idlOS::shmat(sShmID, 0, sFlag);
        IDE_TEST_RAISE(sNewShmHeader == (smmShmHeader *)-1, shmat_error);

        IDE_TEST_RAISE(sNewShmHeader->m_versionID != smVersionID,
                       version_mismatch_error);

        IDE_TEST_RAISE(sNewShmHeader->m_valid_state != ID_TRUE,
                       invalid_shm_error);

        IDE_TEST_RAISE(sNewShmHeader->mTBSID != aTBSNode->mHeader.mID,
                       tbsid_mismatch_error);
        
        sSCH->m_next      = sNewSCH;
        sNewSCH->m_header = sNewShmHeader;
        sNewSCH->m_shm_id = (SInt)sShmID;
        sNewSCH->m_next   = NULL;

        aTBSNode->mTailSCH = sNewSCH;
        sSCH               = sNewSCH;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(invalid_shm_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Shm_Link_Invalid_State, (UInt)sCurKey));
    }
    IDE_EXCEPTION(version_mismatch_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Shm_Link_Invalid_Version, (UInt)sCurKey));
    }
    IDE_EXCEPTION(tbsid_mismatch_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Shm_Link_Invalid_TBSID,
                                (UInt)sNewShmHeader->mTBSID,
                                (UInt)aTBSNode->mHeader.mID));
    }
    IDE_EXCEPTION(not_exist_error); // �Ϻ� ��ũ�� ������ ����.
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Shm_Link_Not_Exist));
    }
    IDE_EXCEPTION(shmget_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmGet));
    }
    IDE_EXCEPTION(shmat_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmAt));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

IDE_RC smmFixedMemoryMgr::detach(smmTBSNode * aTBSNode)
{
    // To Fix PR-12974
    // SBUG-4 Conding Convention
    smmSCH *sNextSCH = NULL;

    smmSCH *sSCH = aTBSNode->mBaseSCH.m_next;

    while(sSCH != NULL)
    {
        sNextSCH = sSCH->m_next; // to prevent FMR
        IDE_TEST_RAISE(idlOS::shmdt( (char*)sSCH->m_header ) < 0, shmdt_error);
        IDE_TEST(mSCHMemList.memfree(sSCH) != IDE_SUCCESS);
        sSCH = sNextSCH;
    }

    aTBSNode->mBaseSCH.m_next = NULL;
    aTBSNode->mTailSCH        = NULL;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(shmdt_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_SysShmDt));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

inline void smmFixedMemoryMgr::linkShmPage(smmTBSNode *   aTBSNode,
                                           smmTempPage   *aPage)
{
    aPage->m_header.m_self = (smmTempPage *)SMM_SHM_LOCATION_FREE;
    aPage->m_header.m_prev = NULL;
    aPage->m_header.m_next = aTBSNode->mTempMemBase.m_first_free_temp_page;
    aTBSNode->mTempMemBase.m_first_free_temp_page = aPage;
    aTBSNode->mTempMemBase.m_free_temp_page_count++;
}


/* ------------------------------------------------
 * [] �� �Լ��� ������ �����޸� DB����
 *    ���������� ���Ḧ ���� ���, �ش�
 *    �����޸��� ��ũ�� �ùٸ����� ��Ÿ����
 *    �÷��׸� �����ϴ� ���̴�.
 *    restart�� attach()�� ������ ��
 *    validate flag�� �˻�ȴ�.
 * ----------------------------------------------*/

void smmFixedMemoryMgr::setValidation(smmTBSNode * aTBSNode, idBool aFlag)
{


    smmSCH *sSCH = aTBSNode->mBaseSCH.m_next;

    if (sSCH != NULL)
    {
        sSCH->m_header->m_valid_state = aFlag;
    }

}


/*
   Tablespace�� ù��° �����޸� Chunk�� �����Ѵ�.

   [IN] aTBSNode   - �����޸� �����ڸ� ������ Tablespace Node
   [IN] aPageCount - ù��° Chunk�� ũ�� ( ����:Page�� )
                     - 0 �̸� SHM_PAGE_COUNT_PER_KEY������Ƽ��
                       ������ Page����ŭ �Ҵ� 
   
   - Use Case
     - Startup�� �����޸� ������ Restore�ϴ� ��� 
     - � ���� �ű� Tablespace������
   
   - PROJ-1548 User Memory Tablespace ======================================
     - [Design]
     
       - �����޸� Key �����ڷκ��� �ĺ� �����޸� Key�� �Ҵ�޾�
         �����޸� ������ �õ��Ѵ�.
         
       - �����޸� ���� ������ �����޸� Key��
         Log Anchor�� �����Ѵ�.
         
       - Log Anchor�� Flush�� �� �Լ��� ȣ���� �Լ��� ó���ؾ��Ѵ�.
 */
IDE_RC smmFixedMemoryMgr::createFirstChunk( smmTBSNode * aTBSNode,
                                            UInt         aPageCount /* = 0 */ )
{
    key_t sChunkShmKey;

    IDE_DASSERT( aTBSNode != NULL );

    // �����޸� Chunk�� �Ҵ�ް� �����޸� Key�� sChunkShmKey�� ����
    IDE_TEST( extendShmPage(aTBSNode,
                            aPageCount,
                            & sChunkShmKey ) != IDE_SUCCESS );

    // ù��° �����޸� Chunk�� Key�� TBSNode�� ����
    aTBSNode->mTBSAttr.mMemAttr.mShmKey = sChunkShmKey ;

    // Restore��ġ�� Resatrt Redo/Undo���� �Ϸ���
    // �����޸� Key�� Log Anchor�� Flush�Ѵ�.
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smmFixedMemoryMgr::allocNewChunk(smmTBSNode * aTBSNode,
                                        key_t        aShmKey,
                                        UInt         aPageCount)
{
    scPageID      i;
    PDL_HANDLE    sShmID;
    SInt          sFlag;
    smmShmHeader *sShmHeader;
    smOID         sPersSize;
    UChar        *sPersPage;
    smmSCH       *sSCH;
    SInt          sState = 0;


    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aShmKey != 0 );
    IDE_DASSERT( aPageCount > 0 );
    
    /* [0] arg ��� */
    sFlag   = 0600 | IPC_CREAT | IPC_EXCL;
    sPersSize = SMM_SHM_DB_SIZE(aPageCount);

    /* smmFixedMemoryMgr_allocNewChunk_alloc_SCH.tc */
    IDU_FIT_POINT("smmFixedMemoryMgr::allocNewChunk::alloc::SCH");
    IDE_TEST(mSCHMemList.alloc((void **)&sSCH) != IDE_SUCCESS);
    sState = 1;

    /* [1] shmid ��� */
    sShmID = idlOS::shmget(aShmKey, sPersSize, sFlag );
    if (sShmID == PDL_INVALID_HANDLE)
    {
        IDE_TEST_RAISE(errno == EEXIST, already_exist_error);
        IDE_TEST_RAISE(errno == ENOSPC, no_space_error);
        IDE_RAISE(shmget_error);
    }
    /* [1] attach ����  */
    sShmHeader  = (smmShmHeader *)idlOS::shmat(sShmID, 0, sFlag);
    IDE_TEST_RAISE(sShmHeader == (smmShmHeader *)-1, shmat_error);
    idlOS::memset(sShmHeader, 0, sPersSize);

    /* [2] SCH assign  */
    sSCH->m_next   = NULL;
    sSCH->m_shm_id = (SInt)sShmID;
    sSCH->m_header = sShmHeader;

    sShmHeader->m_valid_state = ID_FALSE;
    sShmHeader->m_versionID   = smVersionID;
    sShmHeader->m_page_count  = aPageCount;
    // PROJ-1548 Memory Tablespace
    // �ٸ� Tablespace�� �����޸� Chunk�� Attach�Ǵ� ��Ȳ Detect���� �ʿ�
    sShmHeader->mTBSID        = aTBSNode->mHeader.mID;
    sShmHeader->m_next_key    = (key_t)0;
    sPersPage                 = ((UChar *)(sShmHeader)
                                          + SMM_CACHE_ALIGNED_SHM_HEADER_SIZE);

    // Free List�� �Է�
    for (i = 0; i < aPageCount; i++)
    {
        linkShmPage(aTBSNode, (smmTempPage *)(sPersPage + (i * SM_PAGE_SIZE)));
    }

    /* ------------------------------------------------
     * [��ũ ����] Tail <--> New
     * ----------------------------------------------*/

    if (aTBSNode->mTailSCH == NULL)
    {
        aTBSNode->mBaseSCH.m_next = sSCH;
        aTBSNode->mTailSCH        = sSCH;
    }
    else
    {
        smmSCH       *sTempTail;

        sTempTail = aTBSNode->mTailSCH;
        sTempTail->m_header->m_valid_state = ID_FALSE;
        
        // To Fix PR-12974
        // SBUG-5
        // Code ������ ����� ���, server restart ��
        // �߸��� Shared Memory ������ ������ ���� �� �ִ�. 
        IDL_MEM_BARRIER;
        {
            sTempTail->m_header->m_next_key = aShmKey;
            sTempTail->m_next               = sSCH;
            aTBSNode->mTailSCH              = sSCH;
        }
        IDL_MEM_BARRIER;
        sTempTail->m_header->m_valid_state = ID_TRUE;
    }

    aTBSNode->mTempMemBase.m_alloc_temp_page_count += aPageCount;
    
    // To Fix PR-12974
    // SBUG-5 kmkim �� �ǰ��� ����.
    IDL_MEM_BARRIER;
    aTBSNode->mTailSCH->m_header->m_valid_state    = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(shmget_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmGet));
    }
    IDE_EXCEPTION(already_exist_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_already_created));
    }
    IDE_EXCEPTION(no_space_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoMore_SHM_Page));
    }
    IDE_EXCEPTION(shmat_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SysShmAt));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    if (sState == 1)
    {
        IDE_ASSERT( mSCHMemList.memfree(sSCH) == IDE_SUCCESS );
    }
    
    IDE_POP();

    return IDE_FAILURE;

}

/*
   TBSNode�� ������ �����޸� ������ ���� �κ��� �����Ѵ�.
 */
IDE_RC smmFixedMemoryMgr::destroy(smmTBSNode * aTBSNode)
{
    // �̹� detach�ǰų� remove�� ���¿��� �Ѵ�.
    IDE_ASSERT( aTBSNode->mBaseSCH.m_next == NULL );
    IDE_ASSERT( aTBSNode->mTailSCH == NULL );

    IDE_TEST( aTBSNode->mTempMemBase.m_mutex.destroy()
              != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
   �����޸𸮸� �����Ѵ�.

   aTBSNode [IN] �����޸𸮸� ������ Tablespace

   - [ PROJ-1548 ] User Memory Tablespace -------------------------------
   
   Tablespace drop�ÿ� Tablespace���� ������̴� �����޸� Key��
   �ٸ� Tablespace���� ��Ȱ���� �� �־�� �Ѵ�.

   ���� : Tablespace �� drop�ɶ� �ش� Tablespace���� ������̴�
          �����޸� Key�� ��Ȱ������ ���� ���
          Tablespace create/drop�� �ݺ��ϸ� �����޸� Key��
          ���Ǵ� ������ ����

   �ذ�å : Tablespace�� drop�ɶ� �ش� Tablespace���� ������̴�
             �����޸� Key�� ��Ȱ���Ѵ�.
  
 */
IDE_RC smmFixedMemoryMgr::remove(smmTBSNode * aTBSNode)
{
    smmSCH *sSCH = NULL;

    IDE_DASSERT( aTBSNode != NULL );
    
    // PROJ-1548
    // �� �̻� ������� �ʴ� �����޸� Key�� ��Ȱ��� �� �ֵ���
    // �����޸� Ű �����ڿ��� �˷��־�� �Ѵ�.

    IDE_DASSERT( aTBSNode->mTBSAttr.mMemAttr.mShmKey != 0 );
    
    // ù ��° �����޸� Key�� �ݳ�
    IDE_TEST( smmShmKeyMgr::notifyUnusedKey(
                  aTBSNode->mTBSAttr.mMemAttr.mShmKey )
              != IDE_SUCCESS );
    
    sSCH = aTBSNode->mBaseSCH.m_next;
    while(sSCH != NULL)
    {
        if ( sSCH->m_header->m_next_key != 0 )
        {
            IDE_TEST( smmShmKeyMgr::notifyUnusedKey(
                          sSCH->m_header->m_next_key )
                      != IDE_SUCCESS );
        }
            
        smmSCH *sNextSCH = sSCH->m_next; // to prevent FMR

        IDE_TEST_RAISE(idlOS::shmdt( (char*)sSCH->m_header ) < 0, shmdt_error);
        IDE_TEST_RAISE(idlOS::shmctl((PDL_HANDLE)sSCH->m_shm_id, IPC_RMID, NULL) == -1, shmctl_error);
        IDE_TEST(mSCHMemList.memfree(sSCH) != IDE_SUCCESS);

        sSCH = sNextSCH;
        aTBSNode->mBaseSCH.m_next = sNextSCH;
    }

    aTBSNode->mTailSCH = NULL;
    return IDE_SUCCESS;

    IDE_EXCEPTION(shmdt_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_SysShmDt));
    }
    IDE_EXCEPTION(shmctl_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_SysShmCtl));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;


}

IDE_RC smmFixedMemoryMgr::freeShmPage(smmTBSNode * aTBSNode,
                                      smmTempPage *aPage)
{

    IDE_TEST( aTBSNode->mTempMemBase.m_mutex.lock( NULL /* idvSQL* */ )
              != IDE_SUCCESS);
    
    linkShmPage(aTBSNode, aPage);

    IDE_TEST( aTBSNode->mTempMemBase.m_mutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smmFixedMemoryMgr::allocShmPage(smmTBSNode *  aTBSNode,
                                       smmTempPage **aPage)
{

    UInt sState=0;

    IDE_TEST( aTBSNode->mTempMemBase.m_mutex.lock(NULL /* idvSQL* */ )
              != IDE_SUCCESS);
    sState=1;

    if (aTBSNode->mTempMemBase.m_first_free_temp_page == NULL)
    {
        // �����޸� Key �ϳ��� SHM_PAGE_COUNT_PER_KEY ������Ƽ��ŭ��
        // Page�� �Ҵ��ϵ��� �Ѵ�.
        IDE_TEST(extendShmPage(
                     aTBSNode,
                     (scPageID)smuProperty::getShmPageCountPerKey() )
                 != IDE_SUCCESS);
    }

    *aPage = aTBSNode->mTempMemBase.m_first_free_temp_page;

    aTBSNode->mTempMemBase.m_first_free_temp_page = (*aPage)->m_header.m_next;
    aTBSNode->mTempMemBase.m_free_temp_page_count--;

    sState=0;
    IDE_TEST( aTBSNode->mTempMemBase.m_mutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    if(sState==1)
    {
        IDE_ASSERT( aTBSNode->mTempMemBase.m_mutex.unlock() == IDE_SUCCESS );
    }

    IDE_POP();
    
    return IDE_FAILURE;
}

/*
     �����޸� Chunk�� �ϳ� ���� �����Ͽ� ������ �޸𸮸� Ȯ���Ѵ�

     aTBSNode       [IN]  �����޸� Chunk�� ������ Tablespace
     aCount         [IN]  Ȯ���� DB Page�� ��
     aCreatedShmKey [OUT] ������ �����޸� Chunk�� Key��
                          NULL�� �ƴ� ��쿡�� �����Ѵ�.
 */
// BUGBUG : ������ ����ó�� �ʿ���.
IDE_RC smmFixedMemoryMgr::extendShmPage(smmTBSNode * aTBSNode,
                                        UInt         aCount,
                                        key_t      * aCreatedShmKey /* =NULL*/)
{
    IDE_RC rc;
    key_t sShmKeyCandidate;
    
    while(1)
    {
        // �����޸� Key �ĺ��� �����´�.
        IDE_TEST( smmShmKeyMgr::getShmKeyCandidate( & sShmKeyCandidate)
                  != IDE_SUCCESS );

        
        rc = allocNewChunk(aTBSNode, sShmKeyCandidate, aCount);
        if (rc == IDE_SUCCESS)
        {
            break;
        }
        switch(ideGetErrorCode())
        {
        case smERR_ABORT_already_created: // continue finding;
            break;
        case smERR_ABORT_SysShmGet:
        case smERR_ABORT_NoMore_SHM_Page:
        case smERR_ABORT_SysShmAt:
        default:
            return IDE_FAILURE;
        }
    }
    
    if ( aCreatedShmKey != NULL )
    {
        *aCreatedShmKey = sShmKeyCandidate;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smmFixedMemoryMgr::checkDBSignature(smmTBSNode * aTBSNode,
                                           UInt         aDbNumber)
{

    ULong           sBasePage[SM_PAGE_SIZE / ID_SIZEOF(ULong) ];
    smmMemBase      *sDiskMemBase;
    smmMemBase      *sSharedMemBase = aTBSNode->mMemBase;
    smmDatabaseFile *sFile;

    IDE_TEST( smmManager::openAndGetDBFile( aTBSNode,
                                            aDbNumber,
                                            0,  // aDBFileNo
                                            &sFile)
              != IDE_SUCCESS );

    IDE_TEST( sFile->readPage(aTBSNode, SMM_MEMBASE_PAGEID, (UChar*)sBasePage) 
              != IDE_SUCCESS );

    sDiskMemBase = (smmMemBase *)((UChar*)sBasePage + SMM_MEMBASE_OFFSET);

    IDE_TEST_RAISE (idlOS::strcmp(sSharedMemBase->mDBFileSignature,
                                  sDiskMemBase->mDBFileSignature) != 0,
                    signature_error);
    return IDE_SUCCESS;

    IDE_EXCEPTION(signature_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ShmDB_Signature_Mismatch,
                                sSharedMemBase->mDBFileSignature,
                                sDiskMemBase->mDBFileSignature));
        
        ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                    SM_TRC_MEMORY_DB_SIGNATURE_FATAL1,
                    sSharedMemBase->mDBFileSignature);
        ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                    SM_TRC_MEMORY_DB_SIGNATURE_FATAL2,
                    sDiskMemBase->mDBFileSignature);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
