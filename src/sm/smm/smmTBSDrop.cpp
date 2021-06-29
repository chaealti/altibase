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
* $Id: smmTBSDrop.cpp 19201 2006-11-30 00:54:40Z kmkim $
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
#include <smmTBSDrop.h>
#include <smmTBSMultiPhase.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSDrop::smmTBSDrop()
{

}


/*
    ����ڰ� ������ �޸� Tablespace�� drop�Ѵ�.

  [IN] aTrans      - Tablespace�� Drop�Ϸ��� Transaction
  [IN] aTBSNode    - Drop�Ϸ��� Tablespace node
  [IN] aTouchMode  - Drop�� Checkpoint Image File���� ���� 
 */
IDE_RC smmTBSDrop::dropTBS(void         * aTrans,
                              smmTBSNode   * aTBSNode,
                              smiTouchMode   aTouchMode)
                              
{
    // aStatistics�� NULL�� ���´�.
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( dropTableSpace( aTrans, aTBSNode, aTouchMode ) != IDE_SUCCESS );
    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
  �޸� ���̺� �����̽��� DROP�Ѵ�.

[IN] aTrans      - Tablespace�� Drop�Ϸ��� Transaction
[IN] aTBSNode    - Drop�Ϸ��� Tablespace node
[IN] aTouchMode  - Drop�� Checkpoint Image File���� ���� 
  
[ PROJ-1548 User Memory Tablespace ]

Drop Tablespace ��ü �˰��� ================================================

* Restart Recovery�� Disk Tablespace�� �⺻������ �����ϰ� ó���Ѵ�.

(010) lock TBSNode in X

(020) DROP-TBS-LOG �α�ǽ�

(030) TBSNode.Status |= Dropping

(040) Drop Tablespace Pending ���

- commit : ( pending ó�� )
           �ڼ��� �˰����� smmTBSDrop::dropTableSpacePending�� ����

- abort  : Log�� ���󰡸� Undo�ǽ�

           (a-010) DROP-TBS-LOG�� UNDO �ǽ� 
              - ����Ǵ� ���� : TBSNode.Status &= ~Dropping
           // Log Anchor�� TBSNode�� Flush�� �ʿ䰡 ����
           // Status |= Dropping ������ ä�� Flush�� ���߱� ����

- restart recover������ : # (��3)
           if TBSNode.Status == Dropped then
              remove TBSNode from TBS List
           fi

-(��3) : redo/undo������ �ý����� ��� TBS�� Log Anchor�� flush�ϸ鼭 
         Status�� Dropped�� TBS�� ������� �ʴ´�.
         ����, �ý����� TBS List������ �����Ѵ�.
 */
IDE_RC smmTBSDrop::dropTableSpace(void             * aTrans,
                                     smmTBSNode       * aTBSNode,
                                     smiTouchMode       aTouchMode)
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );

    sctPendingOp * sPendingOp;


    ////////////////////////////////////////////////////////////////
    // (010) lock TBSNode in X
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                  aTrans,
                  & aTBSNode->mHeader,
                  ID_FALSE,   /* intent */
                  ID_TRUE,    /* exclusive */
                  SCT_VAL_DROP_TBS) /* validation */
              != IDE_SUCCESS );

    
    ////////////////////////////////////////////////////////////////
    // (020) DROP-TBS-LOG �α�ǽ�
    IDE_TEST( smLayerCallback::writeMemoryTBSDrop( NULL, /* idvSQL* */
                                                   aTrans,
                                                   aTBSNode->mHeader.mID,
                                                   aTouchMode )
              != IDE_SUCCESS );

    ////////////////////////////////////////////////////////////////
    // (030) TBSNode.Status |= Dropping
    // Tx Commit�������� DROPPING���� �����ϰ�
    // Commit�ÿ� Pending���� DROPPED�� �����ȴ�.
    //
    aTBSNode->mHeader.mState |= SMI_TBS_DROPPING;

    
    ////////////////////////////////////////////////////////////////
    // (040) Drop Tablespace Pending ���
    //
    // Transaction Commit�ÿ� ������ Pending Operation��� 
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                  aTrans,
                  aTBSNode->mHeader.mID,
                  ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                  SCT_POP_DROP_TBS,
                  & sPendingOp )
              != IDE_SUCCESS );

    // Commit�� sctTableSpaceMgr::executePendingOperation����
    // ������ Pending�Լ� ����
    sPendingOp->mPendingOpFunc = smmTBSDrop::dropTableSpacePending;
    sPendingOp->mTouchMode     = aTouchMode;
    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   Tablespace�� DROP�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
   
   [����] sctTableSpaceMgr::executePendingOperation ���� ȣ��ȴ�.
 */
IDE_RC smmTBSDrop::dropTableSpacePending( idvSQL*        /* aStatistics */,
                                          sctTableSpaceNode * aTBSNode,
                                          sctPendingOp      * aPendingOp )
{
    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aPendingOp != NULL );
    IDE_DASSERT( aPendingOp->mPendingOpParam == NULL );
    
    IDE_TEST( doDropTableSpace( aTBSNode, aPendingOp->mTouchMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ���� Drop Tablespace���� �ڵ� 
  
   PROJ-1548 User Memory Tablespace
 
   Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
   - ���� : Tablespace�� Lock������ �ٸ� Tx���� ����ϸ鼭
             ������ �� �ֱ� ������ �����ؼ��� �ȵȴ�.

   - ȣ��Ǵ� ���           
      - Drop Tablespace�� Commit Pending Operation
      - Create Tablespace�� NTA Undo
             
   [ �˰��� ] ======================================================
   
   (c-010) TBSNode.Status := Dropped (��3)
   (c-020) flush TBSNode  (��2)

   
   (c-030) latch TBSNode.SyncMutex // Checkpoint�� ����
   (c-040) unlatch TBSNode.SyncMutex
   
   (c-050) Memory Garbage Collector�� Block�Ѵ�. // Ager�� ����
   (c-060) Memory Garbage Collector�� Unblock�Ѵ�.
  
   (c-070) close all Checkpoint Image Files
   
   # (��1)
   # DROP TABLESPACE INCLUDING CONTENTS 
   # AND CHECKPOINT IMAGES
   if DropCheckpointImages then
   (c-080) delete Checkpoint Image Files
   fi
   
   (c-090) Lock����(STATE�ܰ�) ������ ��� ��ü �ı�, �޸� �ݳ�

             
   (c-100) Runtime���� ���� => Tablespace�� �� Counting 


-(��1) : Checkpoint Image�� �������� �ٽ� ������ �Ұ��ϹǷ�
         Commit�� Pendingó���Ѵ�.
         
-(��2) : TBSNode�� Status�� Dropped�� �Ͽ� Log Anchor�� ������.
         - Normal Processing�� checkpoint�� 
           Drop�� TBS�� Page�� Flush���� �ʴ´�.
         - Restart Recovery�� Drop�� TBS���� Page�� Redo/Undo�� �����Ѵ�.
         
-(��3) : Checkpoint Image File Node�� Checkpoint Path Node�� ���
         Log Anchor�� �״�� �����ְ� �ȴ�.
         Server�⵿�� Tablespace�� ���°� DROPPED�̸�
         �α׾�Ŀ�ȿ� �����ϴ� Checkpoint Image File Node��
         Checkpoint Path Node�� ��� �����Ѵ�.
         
*/

IDE_RC smmTBSDrop::doDropTableSpace( sctTableSpaceNode * aTBSNode,
                                     smiTouchMode        aTouchMode )
{
    // BUG-27609 CodeSonar::Null Pointer Dereference (8)
    IDE_ASSERT( aTBSNode   != NULL );

    idBool                      sPageCountLocked = ID_FALSE;
    

    // ���� ������ Tablespace�� �׻� Memory Tablespace���� �Ѵ�.
    IDE_ERROR( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE );
    

    // Checkpoint Thread�� �� Tablespace�� ���� Checkpoint���� �ʵ���
    // �ϱ� ���� FLAG�� �����Ѵ�.
    IDE_TEST( smmTBSDrop::flushTBSStatus(
                  aTBSNode,
                  aTBSNode->mState | SMI_TBS_DROP_PENDING )
              != IDE_SUCCESS );
    
    
    /////////////////////////////////////////////////////////////////////
    // - latch TBSNode.SyncMutex 
    // - unlatch TBSNode.SyncMutex
    //
    // ���� Checkpoint�� Dirty Page Flush�� �ϰ� �ִٸ� TBSNode.SyncMutex
    // �� ��� ���� ���̴�. 
    // ==> Mutex�� ��Ҵٰ� Ǯ� Dirty Page Flush�� ����Ǳ⸦ ��ٸ���.
    //
    // unlatch�� Checkpoint�� �߻��ϸ� TBSNode�� ���°�
    // EXECUTING_DROP_PENDING
    // Dirty Page Flush�� �ǽ����� �ʰ� Skip�ϰ� �ȴ�.
    //
    // ( ���� ������ Checkpoint Image Header�� �����ϴ�
    //   ���� �ڵ忡�� �Ȱ��� ����ȴ�.
    //   smmTBSMediaRecovery::doActUpdateAllDBFileHdr )
    IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aTBSNode )
              != IDE_SUCCESS );
    
    IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aTBSNode )
              != IDE_SUCCESS );

    /////////////////////////////////////////////////////////////////////
    // - Memory Garbage Collector�� Block�Ѵ�.
    // - Memory Garbage Collector�� Unblock�Ѵ�.
    //
    // ���� Ager�� �������̶�� blockMemGC���� Ager�� ������ �Ϸ�Ǳ⸦
    // ��ٸ� ���̴�.
    //
    // ==> Mutex�� ��Ҵٰ� Ǯ� Ager�� �ѹ� �� �������� ��ٸ���.
    //
    // unblock�� Ager�� ����Ǹ� �ϸ� TBSNode�� ���°� DROPPED����
    // Aging�� �ǽ����� �ʰ� Skip�ϰ� �ȴ�.

    // META���� �ܰ迡���� AGER�� �ʱ�ȭ���� �Ǿ����� ���� �����̴�
    // AGER�� �ʱ�ȭ�Ǿ����� Ȯ���� BLOCK/UNBLOCK�Ѵ�.
    
    // BUG-32237 Free lock node when dropping table.
    //
    // DropTablespacePending ���꿡���� Ager�� ��� block�ߴٰ� Unlock�� �մϴ�.
    // �̴� Ager�ʿ��� Tablespace�� Drop �ȵƴٰ� �����ϰ� �۾��� �ϴ� ����
    // �����ϱ� �����Դϴ�.
    //  
    // �׷��� �� ������ ���, ���� �̹� DropTable ����� Aging�� ���õ�
    // ���ü� ó�� ������ �ֽ��ϴ�.
    //   
    // �װ��� smaDeleteThread::waitForNoAccessAftDropTbl �Լ��Դϴ�.
    // �̰��� DropTablespacePending���꿡 �ִ� Ager Block ����� ������
    // �����Դϴ�. �� ������ Drop�ϱ� ���� ager���� ���ü��� �������ִ� ������
    // �մϴ�.
    //    
    // �� ������ ���� ����˴ϴ�.
    // a. Tx.commit
    // b. Tx.ProcessOIDList => TableHeader�� DeleteSCN ����
    // c. waitForNoAccessAftDropTbl
    // d. DropTablePending
    // e. Tx.End
    //     
    // b, c���� ���� ������ Ager�� Table�� Drop�Ǿ��ٴ� ���� Ȯ�� ���ϰ�
    // Row�� Aging�Ϸ��µ� DropTablePending�� ���� Page�� ���󰡴� ����
    // �����ϴ�.
    //  
    // 1) Table�� Drop�Ǿ��ٴ� ���� Ager�� ���� ��� => ���� �����ϴ�.
    // 2) Table�� Drop�Ǿ��ٴ� ���� Ager�� ������ ��� => Transaction�� c��
    // ������ ���� ����մϴ�. 
    //  
    // ���� e��, Tx.End�� �Ǿ�߸� Ager�� �ش� Transaction�� Aging�ϱ�
    // ������, DropTablePending�� �ļ��۾��� LockItem�� ����� �۾���
    // ������ ��ߵ� �����ϴ�.
    // ���� ���� �Ǳ� �����Դϴ�.
    
    

    /////////////////////////////////////////////////////////////////
    // close all Checkpoint Image Files
    // delete Checkpoint Image Files
    IDE_TEST( smmManager::closeAndRemoveChkptImages(
                    (smmTBSNode *) aTBSNode,
                    ( (aTouchMode == SMI_ALL_TOUCH ) ?
                      ID_TRUE /* REMOVE */ :
                      ID_FALSE /* �������� */ ) )
                != IDE_SUCCESS );



    // To Fix BUG-17267
    //    Tablespace Drop���� Dirty Page�� �����Ͽ� �������
    // => ���� : finiToStatePhase���� PCH Entry�� destroy�ϴµ�,
    //            Page�� Dirty Stat�� SMM_PCH_DIRTY_STAT_INIT��
    //            �ƴϾ ��� 
    // => �ذ� : Drop Tablespace�� ��� Page�� ���� PCH�� Dirty Stat�� 
    //            SMM_PCH_DIRTY_STAT_INIT���� �����Ѵ�.
    if ( sctTableSpaceMgr::isStateInSet( aTBSNode->mState,
                                         SCT_SS_NEED_PAGE_PHASE )
         == ID_TRUE )
    {
        IDE_TEST( smmManager::clearDirtyFlag4AllPages( (smmTBSNode*) aTBSNode )
                  != IDE_SUCCESS );
    }


    // ChunkȮ��� ��� Tablespace�� Total Page Count�� ���µ�
    // �� ���߿� Page�ܰ踦 �������� �ȵȴ�.
    // ���� : Total Page Count�� ���� smpTBSAlterOnOff::alterTBSonline��
    //         ����ִ� Alloc Page Count�� ������� Membase�� �����Ѵ�.
    //         �׷���, Membase�� Page�ܰ踦 �����鼭 NULL�� ���Ѵ�.
    IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex() != IDE_SUCCESS );
    sPageCountLocked = ID_TRUE;
    
    /////////////////////////////////////////////////////////////////
    // Lock����(STATE�ܰ�) ������ ��� ��ü �ı�, �޸� �ݳ�
    IDE_TEST( smmTBSMultiPhase::finiToStatePhase( (smmTBSNode*) aTBSNode  )
              != IDE_SUCCESS );


    sPageCountLocked = ID_FALSE;
    IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex() != IDE_SUCCESS );
    
    
    // To Fix BUG-17258     [PROJ-1548-test] Drop Tablespace���� ����� ���
    //                      Checkpoint Image�� �������� ���� �� ����
    //
    // => Checkpoint Image���� �Ŀ� Tablespace���¸� DROPPED�� �����Ѵ�.
    /////////////////////////////////////////////////////////////////////
    // - TBSNode.Status := Dropped
    // - flush TBSNode  (��2)
    IDE_TEST( flushTBSStatusDropped(aTBSNode) != IDE_SUCCESS );

    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sPageCountLocked == ID_TRUE )
    {
        IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex()
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}

/*
    Tablespace�� ���¸� DROPPED�� �����ϰ� Log Anchor�� Flush!
 */
IDE_RC smmTBSDrop::flushTBSStatusDropped( sctTableSpaceNode * aSpaceNode )
{
    IDE_TEST( smmTBSDrop::flushTBSStatus( aSpaceNode,
                                          SMI_TBS_DROPPED )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
    Tablespace�� ���¸� �����ϰ� Log Anchor�� Flush�Ѵ�.
    
    aSpaceNode     [IN] Tablespace�� Node
    aNewSpaceState [IN] Tablespace�� ���ο� ����
    
    [�˰���]
     (010) latch TableSpace Manager
     (020) TBSNode.Status := ���ο� ����
     (030) flush TBSNode 
     (040) unlatch TableSpace Manager

   [ ���� ]
     �� �Լ��� DROP TABLESPACE�� Commit Pending���� ȣ��ȴ�.
     Commit Pending�Լ��� �����ؼ��� �ȵǱ� ������
     ���� ���� Detect�� ���� IDE_ASSERT�� ����ó���� �ǽ��Ͽ���.
 */
IDE_RC smmTBSDrop::flushTBSStatus( sctTableSpaceNode * aSpaceNode,
                                   UInt                aNewSpaceState)
{
    /////////////////////////////////////////////////////////////////////
    // (010) latch TableSpace Manager
    //       �ٸ� Tablespace�� ����Ǵ� ���� Block�ϱ� ����
    //       �ѹ��� �ϳ��� Tablespace�� ����/Flush�Ѵ�.
    sctTableSpaceMgr::lockSpaceNode( NULL, aSpaceNode ) ;
    
    /////////////////////////////////////////////////////////////////////
    // (020) TBSNode.Status := ���ο� ����
    aSpaceNode->mState = aNewSpaceState;
    
    /////////////////////////////////////////////////////////////////////
    // (c-030) flush TBSNode  (��2)
    /* PROJ-2386 DR
     * standby ������ active�� ������ ������ loganchor�� �����. */
    IDE_ASSERT( smLayerCallback::updateTBSNodeAndFlush( aSpaceNode ) 
                == IDE_SUCCESS );


    /////////////////////////////////////////////////////////////////////
    // (040) unlatch TableSpace Manager
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    
    return IDE_SUCCESS;
}


