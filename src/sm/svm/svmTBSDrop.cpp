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
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <svmReq.h>
#include <sctTableSpaceMgr.h>
#include <svmDatabase.h>
#include <svmManager.h>
#include <svmTBSDrop.h>

/*
  ������ (�ƹ��͵� ����)
*/
svmTBSDrop::svmTBSDrop()
{

}


/*
    ����ڰ� ������ �޸� Tablespace�� drop�Ѵ�.
 */
IDE_RC svmTBSDrop::dropTBS(void       * aTrans,
                              svmTBSNode * aTBSNode)
                              
{
    // aStatistics�� NULL�� ���´�.
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );
    
    IDE_TEST( dropTableSpace( aTrans, aTBSNode ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  Volatile ���̺� �����̽��� DROP�Ѵ�.
  
[ PROJ-1548 User Memory Tablespace ]

Drop Tablespace ��ü �˰��� ================================================

* Restart Recovery�� Disk Tablespace�� �⺻������ �����ϰ� ó���Ѵ�.

(010) lock TBSNode in X

(020) DROP-TBS-LOG �α�ǽ�

(030) TBSNode.Status |= Dropping

(040) Drop Tablespace Pending ���

- commit : ( pending ó�� )
           �ڼ��� �˰����� svmTBSDrop::dropTableSpacePending�� ����

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
IDE_RC svmTBSDrop::dropTableSpace(void       * aTrans,
                                     svmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );

    sctPendingOp * sPendingOp;

    ////////////////////////////////////////////////////////////////
    // (010) lock TBSNode in X
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                                   aTrans,
                                   &aTBSNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   ID_ULONG_MAX, /* lock wait micro sec */
                                   SCT_VAL_DROP_TBS,
                                   NULL,       /* is locked */
                                   NULL )      /* lockslot */
              != IDE_SUCCESS );


    ////////////////////////////////////////////////////////////////
    // (020) DROP-TBS-LOG �α�ǽ�
    IDE_TEST( smLayerCallback::writeVolatileTBSDrop( NULL, /* idvSQL* */
                                                     aTrans,
                                                     aTBSNode->mHeader.mID )
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
    sPendingOp->mPendingOpFunc = svmTBSDrop::dropTableSpacePending;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   Tablespace�� DROP�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
  
   PROJ-1548 User Memory Tablespace
 
   Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
   - ���� : Tablespace�� Lock������ �ٸ� Tx���� ����ϸ鼭
             ������ �� �ֱ� ������ �����ؼ��� �ȵȴ�.

   [����] sctTableSpaceMgr::executePendingOperation ���� ȣ��ȴ�.

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
   
   (c-090) Lock���� ������ ��� ��ü �ı�, �޸� �ݳ�

   (c-100) Runtime���� ���� => Tablespace�� �� Counting 

   [ ���� ]
     Commit Pending�Լ��� �����ؼ��� �ȵǱ� ������
     ���� ���� Detect�� ���� IDE_ASSERT�� ����ó���� �ǽ��Ͽ���.
             
   

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
IDE_RC svmTBSDrop::dropTableSpacePending( idvSQL*             /*aStatistics*/,
                                          sctTableSpaceNode * aTBSNode,
                                          sctPendingOp      * /*aPendingOp*/ )
{
    IDE_DASSERT( aTBSNode   != NULL );

    // ���� ������ Tablespace�� �׻� Volatile Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isVolatileTableSpace( aTBSNode->mID ) == ID_TRUE );
    // Tablespace�� ���¸� DROPPED�� ���º���
    //
    // To Fix BUG-17323 �������� �ʴ� Checkpoint Path�����Ͽ�
    //                  Tablespace���� ������ Log Anchor��
    //                  Log File Group Count�� 0�� �Ǿ����
    //
    // => Tablespace Node�� ���� Log Anchor�� ��ϵ��� ���� ���
    //    Node���� ���¸� �������ش�.
    //    Log Anchor�� �ѹ��̶� ����� �Ǹ�
    //    sSpaceNode->mAnchorOffset �� 0���� ū ���� ������ �ȴ�.
    if ( ((svmTBSNode*)aTBSNode)->mAnchorOffset > 0 )  
    {
        /////////////////////////////////////////////////////////////////////
        // (c-010) TBSNode.Status := Dropped
        // (c-020) flush TBSNode  (��2)
        IDE_ASSERT( flushTBSStatusDropped(aTBSNode) == IDE_SUCCESS );
    }
    else
    {
        // Log Anchor�� �ѹ��� ��ϵ��� �ʴ� ���
        aTBSNode->mState = SMI_TBS_DROPPED;
    }

    /////////////////////////////////////////////////////////////////////
    // (c-030) latch TBSNode.SyncMutex 
    // (c-040) unlatch TBSNode.SyncMutex
    //
    // ���� Checkpoint�� Dirty Page Flush�� �ϰ� �ִٸ� TBSNode.SyncMutex
    // �� ��� ���� ���̴�. 
    // ==> Mutex�� ��Ҵٰ� Ǯ� Dirty Page Flush�� ����Ǳ⸦ ��ٸ���.
    //
    // unlatch�� Checkpoint�� �߻��ϸ� TBSNode�� ���°� DROPPED����
    // Dirty Page Flush�� �ǽ����� �ʰ� Skip�ϰ� �ȴ�.
    IDE_ASSERT( sctTableSpaceMgr::latchSyncMutex( aTBSNode )
                == IDE_SUCCESS );
    
    IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex( aTBSNode )
                == IDE_SUCCESS );


    /////////////////////////////////////////////////////////////////////
    // (c-050) Memory Garbage Collector�� Block�Ѵ�.
    // (c-060) Memory Garbage Collector�� Unblock�Ѵ�.
    //
    // ���� Ager�� �������̶�� blockMemGC���� Ager�� ������ �Ϸ�Ǳ⸦
    // ��ٸ� ���̴�.
    //
    // ==> Mutex�� ��Ҵٰ� Ǯ� Ager�� �ѹ� �� �������� ��ٸ���.
    //
    // unblock�� Ager�� ����Ǹ� �ϸ� TBSNode�� ���°� DROPPED����
    // Aging�� �ǽ����� �ʰ� Skip�ϰ� �ȴ�.
    
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

    /* BUG-39806 Valgrind Warning
     * - DROPPED ���·� ���� �����Ͽ��� ������, �˻����� �ʰ� svmManager::finiTB
     *   S()�� ȣ���մϴ�.
     */
    /////////////////////////////////////////////////////////////////
    // (c-090) Lock���� ������ ��� ��ü �ı�, �޸� �ݳ� (��4)
    //
    // ������ �޸� �ݳ�
    IDE_ASSERT( svmManager::finiTBS((svmTBSNode*)aTBSNode)
                == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/*
    Tablespace�� ���¸� DROPPED�� �����ϰ� Log Anchor�� Flush!

    [�˰���]
     (010) latch TableSpace Manager
     (020) TBSNode.Status := Dropped
     (030) flush TBSNode 
     (040) unlatch TableSpace Manager

   [ ���� ]
     �� �Լ��� DROP TABLESPACE�� Commit Pending���� ȣ��ȴ�.
     Commit Pending�Լ��� �����ؼ��� �ȵǱ� ������
     ���� ���� Detect�� ���� IDE_ASSERT�� ����ó���� �ǽ��Ͽ���.
 */
IDE_RC svmTBSDrop::flushTBSStatusDropped( sctTableSpaceNode * aSpaceNode )
{
    /////////////////////////////////////////////////////////////////////
    // (010) latch TableSpace Manager
    //       �ٸ� Tablespace�� ����Ǵ� ���� Block�ϱ� ����
    //       �ѹ��� �ϳ��� Tablespace�� ����/Flush�Ѵ�.
    
    sctTableSpaceMgr::lockSpaceNode( NULL, aSpaceNode );
    
    /////////////////////////////////////////////////////////////////////
    // (020) TBSNode.Status := Dropped
    IDE_ASSERT( SMI_TBS_IS_ONLINE(aSpaceNode->mState) );
    IDE_ASSERT( SMI_TBS_IS_DROPPING(aSpaceNode->mState) );
    
    aSpaceNode->mState = SMI_TBS_DROPPED;
    
    /////////////////////////////////////////////////////////////////////
    // (c-030) flush TBSNode  (��2)
    if ( smLayerCallback::isRestart() != ID_TRUE )
    {
        IDE_ASSERT( smLayerCallback::updateTBSNodeAndFlush( aSpaceNode ) 
                    == IDE_SUCCESS );
    }
    else
    {
        // ���������ÿ��� RECOVERY ���Ŀ� �ѹ��� Loganchor �����Ѵ�.
    }

    /////////////////////////////////////////////////////////////////////
    // (040) unlatch TableSpace Manager
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    
    return IDE_SUCCESS;
}

