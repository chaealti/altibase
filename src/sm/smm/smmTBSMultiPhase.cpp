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
* $Id: smmTBSMultiPhase.cpp 19201 2006-11-30 00:54:40Z kmkim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <smm.h>
#include <smmReq.h>
#include <sctTableSpaceMgr.h>
#include <smmTBSMultiPhase.h>

smmTBSMultiPhase::smmTBSMultiPhase()
{
}

/* Server Shutdown�ÿ� Tablespace�� ���¿� ����
   Tablespace�� �ʱ�ȭ�� �ܰ���� �ı��Ѵ�.

   [IN] aTBSNode - Tablespace��  Node
   
   ex> DISCARDED, OFFLINE������ TBS�� MEDIA, STATE�ܰ踦 �ı�
   ex> ONLINE������ TBS�� PAGE,MEDIA,STATE�ܰ踦 �ı�
 */
IDE_RC smmTBSMultiPhase::finiTBS( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( finiToStatePhase( aTBSNode )
              != IDE_SUCCESS );

    if ( sctTableSpaceMgr::hasState( & aTBSNode->mHeader,
                                     SCT_SS_NEED_STATE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( finiStatePhase( aTBSNode ) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/* STATE Phase �κ��� Tablespace�� ���¿� ����
   �ʿ��� �ܰԱ��� �ٴܰ� �ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node
   [IN] aTBSAttr - Tablespace��  Attribute
   
   ex> DISCARDED, OFFLINE������ TBS�� MEDIA, STATE�ܰ踦 �ı�
   ex> ONLINE������ TBS��  STATE, MEDIA, PAGE�ܰ踦 �ϳ��� �ʱ�ȭ
 */
IDE_RC smmTBSMultiPhase::initTBS( smmTBSNode        * aTBSNode,
                                  smiTableSpaceAttr * aTBSAttr )
{
    IDE_DASSERT( aTBSNode != NULL );

    // STATE Phase �ʱ�ȭ
    if ( sctTableSpaceMgr::isStateInSet( aTBSAttr->mTBSStateOnLA,
                                         SCT_SS_NEED_STATE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( initStatePhase( aTBSNode,
                                  aTBSAttr ) != IDE_SUCCESS );
    }

    // MEDIA, PAGE Phase �ʱ�ȭ
    if ( sctTableSpaceMgr::isStateInSet( aTBSNode->mHeader.mState,
                                         SCT_SS_NEED_MEDIA_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( initFromStatePhase( aTBSNode ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* STATE�ܰ���� �ʱ�ȭ�� Tablespace�� �ٴܰ��ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node
   
   ex> OFFLINE������ TBS�� MEDIA�ܰ���� �ʱ�ȭ
   ex> ONLINE������ TBS�� PAGE,MEDIA�ܰ���� �ʱ�ȭ
   
 */
  
IDE_RC smmTBSMultiPhase::initFromStatePhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    UInt sTBSState = aTBSNode->mHeader.mState;

    // ���� Tablespace�� ���´� STATE�ܰ������ �ʱ�ȭ��
    // �ʿ�� �ϴ� ���¿��� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isStateInSet( sTBSState,
                                                SCT_SS_NEED_STATE_PHASE )
                == ID_TRUE );
    
    // MEDIA Phase�ʱ�ȭ
    if ( sctTableSpaceMgr::isStateInSet( sTBSState,
                                         SCT_SS_NEED_MEDIA_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( initMediaPhase( aTBSNode ) != IDE_SUCCESS );
    }

    
    // PAGE Phase �ʱ�ȭ
    if ( sctTableSpaceMgr::isStateInSet( sTBSState,
                                         SCT_SS_NEED_PAGE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( initPagePhase( aTBSNode ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* �ٴܰ� ������ ���� STATE�ܰ�� ����

   [IN] aTBSNode - Tablespace��  Node
   
   ex> OFFLINE������ TBS�� MEDIA�ܰ踦 �ı�
   ex> ONLINE������ TBS�� PAGE,MEDIA�ܰ踦 �ı�

   => sTBSNode���� State�� �̿����� �ʰ� ���ڷ� �Ѿ�� State�� �̿��Ѵ�
 */
IDE_RC smmTBSMultiPhase::finiToStatePhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    UInt sTBSState = aTBSNode->mHeader.mState;
    
    
    // PAGE Phase �ı�
    if ( sctTableSpaceMgr::isStateInSet( sTBSState,
                                         SCT_SS_NEED_PAGE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( finiPagePhase( aTBSNode ) != IDE_SUCCESS );
    }

    // MEDIA Phase�ı�
    if ( sctTableSpaceMgr::isStateInSet( sTBSState,
                                         SCT_SS_NEED_MEDIA_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( finiMediaPhase( aTBSNode ) != IDE_SUCCESS );
    }

    // ���� Tablespace�� ���´� STATE�ܰ������ �ʱ�ȭ��
    // �ʿ�� �ϴ� ���¿��� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isStateInSet( sTBSState,
                                                SCT_SS_NEED_STATE_PHASE )
                == ID_TRUE );
    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �ٴܰ� ������ ���� MEDIA�ܰ�� ����

   [IN] aTBSState - Tablespace�� State
   [IN] aTBSNode - Tablespace��  Node
   
   ex> ONLINE������ TBS�� ���� PAGE�ܰ踦 �ı�

   => sTBSNode���� State�� �̿����� �ʰ� ���ڷ� �Ѿ�� State�� �̿��Ѵ�
 */
IDE_RC smmTBSMultiPhase::finiToMediaPhase( UInt         aTBSState,
                                           smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // PAGE Phase �ı�
    if ( sctTableSpaceMgr::isStateInSet( aTBSState, SCT_SS_NEED_PAGE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( finiPagePhase( aTBSNode ) != IDE_SUCCESS );
    }

    // ���� Tablespace�� ���´� MEDIA�ܰ������ �ʱ�ȭ��
    // �ʿ�� �ϴ� ���¿��� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isStateInSet( aTBSState,
                                                SCT_SS_NEED_MEDIA_PHASE )
                == ID_TRUE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Tablespace�� STATE�ܰ踦 �ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node
   [IN] aTBSAttr - Tablespace��  Attribute
 */
IDE_RC smmTBSMultiPhase::initStatePhase( smmTBSNode * aTBSNode,
                                         smiTableSpaceAttr * aTBSAttr )
{
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aTBSAttr != NULL );

    // Memory Tablespace Node�� �ʱ�ȭ �ǽ�
    IDE_TEST( smmManager::initMemTBSNode( aTBSNode,
                                          aTBSAttr )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� MEDIA�ܰ踦 �ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node

   ���� : STATE�ܰ���� �ʱ�ȭ�� �Ǿ� �־�� �Ѵ�.
 */
IDE_RC smmTBSMultiPhase::initMediaPhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // Memory Tablespace Node�� �ʱ�ȭ �ǽ�
    IDE_TEST( smmManager::initMediaSystem( aTBSNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� PAGE�ܰ踦 �ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node

   ���� : MEDIA�ܰ�����ʱ�ȭ�� �Ǿ� �־�� �Ѵ�.
 */
IDE_RC smmTBSMultiPhase::initPagePhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // Memory Tablespace Node�� �ʱ�ȭ �ǽ�
    IDE_TEST( smmManager::initPageSystem( aTBSNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� STATE�ܰ踦 �ı�

   [IN] aTBSNode - Tablespace��  Node

   ���� : STATE�ܰ谡 �ʱ�ȭ�� ���¿��� �Ѵ�.
   ���� : PAGE,MEDIA�ܰ谡 �ı��Ǿ� �־�� �Ѵ�.
 */
IDE_RC smmTBSMultiPhase::finiStatePhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmManager::finiMemTBSNode( aTBSNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� MEDIA�ܰ踦 �ı�

   [IN] aTBSNode - Tablespace��  Node

   ���� : STATE, MEDIA�ܰ谡 ��� �ʱ�ȭ�� ���¿��� �Ѵ�.
   ���� : PAGE�ܰ谡 �ı� �Ǿ� �־�� �Ѵ�.
 */
IDE_RC smmTBSMultiPhase::finiMediaPhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmManager::finiMediaSystem( aTBSNode )
              != IDE_SUCCESS );


    // To Fix BUG-18100, 18099
    //        Shutdown�� Checkpoint Path Node���� Memory Leak�߻� ���� �ذ�
    IDE_TEST( smmTBSChkptPath::freeAllChkptPathNode( aTBSNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Tablespace�� PAGE�ܰ踦 �ı�

   [IN] aTBSNode - Tablespace��  Node

   ���� : STATE, MEDIA, PAGE�ܰ谡 ��� �ʱ�ȭ�� ���¿��� �Ѵ�.
 */
IDE_RC smmTBSMultiPhase::finiPagePhase( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmManager::finiPageSystem( aTBSNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
