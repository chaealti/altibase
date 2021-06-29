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
 * $Id: smmDirtyPageMgr.cpp 90085 2021-02-26 02:14:29Z et16 $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <sctTableSpaceMgr.h>
#include <smm.h>
#include <smmDirtyPageList.h>
#include <smmDirtyPageMgr.h>
#include <smmManager.h>
#include <smmReq.h>

IDE_RC smmDirtyPageMgr::initialize(scSpaceID aSpaceID , SInt a_listCount)
{
    
    SInt i;
    
    mSpaceID    = aSpaceID ;
    m_listCount = a_listCount;
    
    IDE_ASSERT( m_listCount != 0 );
    
    m_list = NULL;
    
    /* TC/FIT/Limit/sm/smm/smmDirtyPageMgr_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "smmDirtyPageMgr::initialize::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 (ULong)ID_SIZEOF(smmDirtyPageList) * m_listCount,
                                 (void**)&m_list ) != IDE_SUCCESS,
                    insufficient_memory );
    
    for( i = 0; i < m_listCount; i++ )
    {
        new (m_list + i) smmDirtyPageList;
        IDE_TEST( m_list[i].initialize(aSpaceID, i) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ��� Dirty Page List��κ��� Dirty Page���� �����Ѵ�.
 */
IDE_RC smmDirtyPageMgr::removeAllDirtyPages()
{
    SInt i;
    
    for (i = 0; i < m_listCount; i++)
    {
        IDE_TEST(m_list[i].open() != IDE_SUCCESS);
        IDE_TEST(m_list[i].clear() != IDE_SUCCESS);
        IDE_TEST(m_list[i].close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}



IDE_RC smmDirtyPageMgr::destroy()
{
    SInt i;
    
    for (i = 0; i < m_listCount; i++)
    {
        IDE_TEST(m_list[i].destroy() != IDE_SUCCESS);
    }
    
    IDE_TEST(iduMemMgr::free(m_list) != IDE_SUCCESS);
    
    m_list = NULL;
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smmDirtyPageMgr::lockDirtyPageList(smmDirtyPageList **a_list)
{
    static SInt sMutexDist = 0;
    
    idBool s_break = ID_FALSE;
    idBool s_locked;
    smmDirtyPageList *s_list = NULL;
    SInt i, sStart;
    
    sStart = sMutexDist;
    do
    {
        for (i = sStart; i < m_listCount; i++)
        {
            s_list = m_list + i;
            
            IDE_TEST(s_list->m_mutex.trylock(s_locked)
                           != IDE_SUCCESS);
            
            if (s_locked == ID_TRUE)
            {
                s_break = ID_TRUE; 
                sMutexDist = i + 1;
                break; // Ż��! lock�� ����
            }
        }
        sStart = 0;
    } while(s_break == ID_FALSE);
    
    *a_list = s_list;
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
    
}

/*
 * Dirty Page ����Ʈ�� ����� Page�� �߰��Ѵ�.
 *
 * aPageID [IN] Dirty Page�� �߰��� Page�� ID
 */
IDE_RC smmDirtyPageMgr::insDirtyPage( scPageID aPageID )
{
    smmDirtyPageList *sList;

    smmPCH      *sPCH;

    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace( mSpaceID ) == ID_TRUE );
    IDE_DASSERT( smmManager::isValidPageID( mSpaceID, aPageID ) == ID_TRUE );
    
    IDL_MEM_BARRIER;
#ifdef DEBUG
    smPCSlot *sPCSlot = smmManager::getPCHSlot( mSpaceID, aPageID );
    sPCH              = (smmPCH*)sPCSlot->mPCH;
#else
    sPCH = smmManager::getPCH( mSpaceID, aPageID );
#endif    
    //IDE_DASSERT( sPCH->m_page != NULL );
    
    /*
      [tx1] Alloc Page#1
      [tx1] commit
      [tx2] Modify Page#1
      [tx2] Free Page#1
      === CHECKPOINT ===
      server dead..
      restart redo.. -> Page#1�� PID���� ����� �Ȱ����� �������
      
      ------------------------------------------------------------
      
      Page���� PID�� smm �ܿ��� ���̺�� Page�� �Ҵ��� �� �����ȴ�.
      
      ���� �������� �Ҵ�Ǿ��ٰ� DROP TABLE���� ���� DB�� �ݳ��ǰ�,
      Checkpoint�� �߻��ϸ�,�ش� Page�� Free Page�̾, Page �޸�����
      ���� ������, Disk�� �������� �ʰ� �ȴ�.
      
      �̶�,Page#1�� �Ҵ��� Ʈ����� tx1�� commit�� Ʈ������̶��,
      Redo�ÿ� tx1�� ����� log���� redo������� ���Ե��� ������,
      Alloc Page#1�� ���� REDO�� ������� �ʰ� �ȴ�.
      
      �׸��� Page#1�� ������ ������ Ʈ����� tx2�� redo�Ǹ鼭
      Page#1�� Dirty Page�� �߰��ϰ� �� ���,
      Page#1�� PID�� ������ ������ �Ǿ� ���� �� �ִ�.
      
      �׷���, Redo�� �� ���� ��Ȳ������ Page#1�� Free�Ǳ� ������
      ���⿡ ���̻� ������ ���� ����.
      
    */
    // Restart Recovery�߿��� Page �޸� ���� PID���� ���� ���Ѵ�.
#ifdef DEBUG
    if ( smLayerCallback::isRestartRecoveryPhase() == ID_FALSE ) 
    {
        IDE_DASSERT( smLayerCallback::getPersPageID( sPCSlot->mPagePtr )
                     == aPageID );
    }
#endif    

    if (sPCH->m_dirty == ID_FALSE)
    {
        sPCH->m_dirty = ID_TRUE;
        
        IDE_TEST(lockDirtyPageList(&sList) != IDE_SUCCESS);
        IDE_TEST_RAISE(sList->addDirtyPage( aPageID )
                       != IDE_SUCCESS, add_error);
        IDE_TEST(sList->m_mutex.unlock() != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(add_error);
    {
        IDE_PUSH();
        IDE_ASSERT( sList->m_mutex.unlock() == IDE_SUCCESS );
        
        IDE_POP();
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

/***********************************************************************
    ���� Tablespace�� ���� ������ �����ϴ� �������̽�
    Refactoring�� ���� ���� ������ Class�� ������ �Ѵ�.
 ***********************************************************************/


/*
    Dirty Page�����ڸ� �����Ѵ�.
 */
IDE_RC smmDirtyPageMgr::initializeStatic()
{

    return IDE_SUCCESS;

}

/*
    Dirty Page�����ڸ� �ı��Ѵ�.
 */

IDE_RC smmDirtyPageMgr::destroyStatic()
{
    
    return IDE_SUCCESS;
}


/*
 * Ư�� Tablespace�� Dirty Page ����Ʈ�� ����� Page�� �߰��Ѵ�.
 *
 * aSpaceID [IN] Dirty Page�� ���� Tablespace�� ID
 * aPageID  [IN] Dirty Page�� �߰��� Page�� ID
 */
IDE_RC smmDirtyPageMgr::insDirtyPage( scSpaceID aSpaceID, scPageID aPageID )
{
    smmDirtyPageMgr * sDPMgr = NULL;

    IDE_TEST( findDPMgr(aSpaceID, &sDPMgr ) != IDE_SUCCESS );

    // �ش� Tablespace�� Dirty Page�����ڰ� ����� �ȵȴ�.
    IDE_ASSERT( sDPMgr != NULL );

    IDE_TEST( sDPMgr->insDirtyPage( aPageID ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC smmDirtyPageMgr::insDirtyPage(scSpaceID aSpaceID, void * a_new_page)
{
    
    IDE_DASSERT( a_new_page != NULL );
    
    return insDirtyPage( aSpaceID,
                         smLayerCallback::getPersPageID( a_new_page ) );
}

/*
    Ư�� Tablespace�� ���� Dirty Page�����ڸ� �����Ѵ�.

    [IN] aSpaceID - �����ϰ��� �ϴ� Dirty Page�����ڰ� ���� Tablespace�� ID
 */
IDE_RC smmDirtyPageMgr::createDPMgr(smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );
    
    smmDirtyPageMgr * sDPMgr;
    
    /* TC/FIT/Limit/sm/smm/smmDirtyPageMgr_createDPMgr_malloc.sql */
    IDU_FIT_POINT_RAISE( "smmDirtyPageMgr::createDPMgr::malloc", 
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMM,
                               ID_SIZEOF(smmDirtyPageMgr),
                               (void**) & sDPMgr ) != IDE_SUCCESS,
                   insufficient_memory );
    
    (void)new ( sDPMgr ) smmDirtyPageMgr;
    
    IDE_TEST( sDPMgr->initialize(aTBSNode->mHeader.mID, ID_SCALABILITY_CPU )
              != IDE_SUCCESS);


    aTBSNode->mDirtyPageMgr = sDPMgr;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
   Ư�� Tablespace�� ���� Dirty Page�����ڸ� ã�Ƴ���.
   ���� Dirty Page �����ڰ� �������� ���� ��� NULL�� ���ϵȴ�.
   
   [IN]  aSpaceID - ã���� �ϴ� DirtyPage�����ڰ� ���� Tablespace�� ID
   [OUT] aDPMgr   - ã�Ƴ� Dirty Page ������
*/

IDE_RC smmDirtyPageMgr::findDPMgr( scSpaceID aSpaceID,
                                   smmDirtyPageMgr ** aDPMgr )
{
    smmTBSNode * sTBSNode;
    
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sTBSNode)
              != IDE_SUCCESS );

    *aDPMgr = sTBSNode->mDirtyPageMgr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Ư�� Tablespace�� Dirty Page�����ڸ� �����Ѵ�.

    [IN] aSpaceID - �����ϰ��� �ϴ� Dirty Page�����ڰ� ���� Tablespace ID
 */
IDE_RC smmDirtyPageMgr::removeDPMgr( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );
    
    smmDirtyPageMgr * sDPMgr;
    
    sDPMgr = aTBSNode->mDirtyPageMgr;

    IDE_TEST( sDPMgr->removeAllDirtyPages() != IDE_SUCCESS );
    
    IDE_TEST( sDPMgr->destroy() != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free( sDPMgr ) != IDE_SUCCESS );
    
    aTBSNode->mDirtyPageMgr = NULL;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}




