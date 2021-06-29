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
* $Id: smpTBSAlterOnOff.cpp 19201 2006-11-30 00:54:40Z kmkim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <smm.h>
#include <smpReq.h>
#include <smrUpdate.h>
#include <sctTableSpaceMgr.h>
#include <smpTBSAlterOnOff.h>
#include <smmTBSMultiPhase.h>

/*
    smpTBSAlterOnOff.cpp

    smpTBSAlterOnOff Ŭ������ �Լ��� 
    ���� ����� ������ ����ִ� �����̴�.

    Alter TableSpace Offline
    Alter TableSpace Online
 */


/*
  ������ (�ƹ��͵� ����)
*/
smpTBSAlterOnOff::smpTBSAlterOnOff()
{
    
}


/*
   Memory Tablespace�� ���� Alter Tablespace Online/Offline�� ����
    
   [IN] aTrans        - ���¸� �����Ϸ��� Transaction
   [IN] aTableSpaceID - ���¸� �����Ϸ��� Tablespace�� ID
   [IN] aState        - ���� ������ ���� ( Online or Offline )
 */
IDE_RC smpTBSAlterOnOff::alterTBSStatus( void       * aTrans,
                                         smmTBSNode * aTBSNode,
                                         UInt         aState )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );
    
    switch ( aState )
    {
        case SMI_TBS_ONLINE :
            IDE_TEST( smpTBSAlterOnOff::alterTBSonline( aTrans,
                                                        aTBSNode )
                      != IDE_SUCCESS );
            break;

        case SMI_TBS_OFFLINE :
            IDE_TEST( smpTBSAlterOnOff::alterTBSoffline( aTrans,
                                                         aTBSNode )
                      != IDE_SUCCESS );
            break;
    }

    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Alter Tablespace Online Offline �α׸� ��� 

    [IN] aTrans          - �α��Ϸ��� Transaction ��ü
    [IN] aTBSNode        - ALTER�� Tablespace�� Node
    [IN] aUpdateType     - Alter Tablespace Online���� Offline���� ���� 
    [IN] aStateToRemove  - ������ ���� (���� &= ~����)
    [IN] aStateToAdd     - �߰��� ���� (���� |=  ����)
    [OUT] aNewTBSState   - ���� ������ Tablespace�� ����

 */
IDE_RC smpTBSAlterOnOff::writeAlterTBSStateLog(
                             void                 * aTrans,
                             smmTBSNode           * aTBSNode,
                             sctUpdateType          aUpdateType,
                             smiTableSpaceState     aStateToRemove,
                             smiTableSpaceState     aStateToAdd,
                             UInt                 * aNewTBSState )
{

    IDE_DASSERT( aTrans != 0 );
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aNewTBSState != NULL );
    
    UInt sBeforeState;
    UInt sAfterState;

    sBeforeState = aTBSNode->mHeader.mState ;

    // �α��ϱ� ���� Backup�����ڿ��� ���ü� ��� ���� �ӽ� Flag�� ����
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_OFFLINE;
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_ONLINE;

    sAfterState  = sBeforeState;
    sAfterState &= ~aStateToRemove;
    sAfterState |=  aStateToAdd;
        
    IDE_TEST( smrUpdate::writeTBSAlterOnOff(
                             NULL, /* idvSQL* */
                             aTrans,
                             aTBSNode->mHeader.mID,
                             aUpdateType,
                             // Before Image
                             sBeforeState,
                             // After Image
                             sAfterState, 
                             NULL )
              != IDE_SUCCESS );

    *aNewTBSState = sAfterState ;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}





/*
    META, SERVICE�ܰ迡�� Tablespace�� Offline���·� �����Ѵ�.

    [IN] aTrans   - ���¸� �����Ϸ��� Transaction
    [IN] aTBSNode - ���¸� ������ Tablespace�� Node

    [ �˰��� ]
      (010) TBSNode�� X�� ȹ��
      (020) Tablespace�� Backup���̶�� Backup����ñ��� ��� 
      (030) "TBSNode.Status := Offline" �� ���� �α� 
      (040) TBSNode.OfflineSCN := Current System SCN
      (050) Instant Memory Aging �ǽ� - Aging �����߿��� ��� Ager ��ġȹ��
      (060) Checkpoint Latch ȹ�� 
      (070)    Dirty Page Flush �ǽ�
               ( Unstable DB ����, 0�� 1�� DB ��� �ǽ� )
      (080) Checkpoint Latch ����
      (090) TBSNode�� Dirty Page�� ������ ASSERT�� Ȯ��
      (100) Commit Pending���
      
    [ Commit�� : (Pending) ]
      (c-010) TBSNode.Status := Offline
      (c-020) latch TBSNode.SyncMutex        // Checkpoint�� ����
      (c-030) unlatch TBSNode.SyncMutex
      (c-040) Free All Page Memory of TBS
      (c-050) Free All Index Memory of TBS
      (c-060) Free Runtime Info At Table Header
      (c-070) Free Runtime Info At TBSNode ( Expcet Lock )
      (c-080) flush TBSNode to loganchor

    [ Abort�� ]
      [ UNDO ] ���� 

    [ REDO ]
      (u-010) (020)�� ���� REDO�� TBSNode.Status := After Image(OFFLINE)
      (note-1) TBSNode�� loganchor�� flush���� ���� 
               -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
      (note-2) Commit Pending�� �������� ����
               -> Restart Recovery�Ϸ��� OFFLINE TBS�� ���� Resource������ �Ѵ�
      
    [ UNDO ]
      (u-010) (020)�� ���� UNDO�� TBSNode.Status := Before Image(ONLINE)
              TBSNode.Status�� Commit Pending���� ����Ǳ� ������
              ���� undo�߿� Before Image�� ����ĥ �ʿ�� ����.
              �׷��� �ϰ����� �����ϱ� ���� TBSNode.Status��
              Before Image�� �����ϵ��� �Ѵ�.      
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS OFFLINE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����

*/
IDE_RC smpTBSAlterOnOff::alterTBSoffline( void       * aTrans,
                                          smmTBSNode * aTBSNode )
{
    idvSQL       * sStatistics ;
    sctPendingOp * sPendingOp;
    UInt           sNewTBSState = 0;
    UInt           sState = 0;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );

    sStatistics = smxTrans::getStatistics( aTrans );

    ///////////////////////////////////////////////////////////////////////////
    //  (010) TBSNode�� X�� ȹ��
    //
    // Tablespace�� Offline���¿��� ������ ���� �ʴ´�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode( 
                                   aTrans,
                                   & aTBSNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   SCT_VAL_ALTER_TBS_ONOFF) /* validation */
              != IDE_SUCCESS );
    
    ///////////////////////////////////////////////////////////////////////////
    //  (e-010) Tablespace���¿� ���� ����ó��
    IDE_TEST( sctTableSpaceMgr::checkError4AlterStatus( 
                                     (sctTableSpaceNode*)aTBSNode,
                                     SMI_TBS_OFFLINE  /* New State */ )
              != IDE_SUCCESS );
    
    ///////////////////////////////////////////////////////////////////////////
    //  (020) Tablespace�� Backup���̶�� Backup����ñ��� ��� 
    IDE_TEST( sctTableSpaceMgr::wait4BackupAndBlockBackup( sStatistics,
                                                           (sctTableSpaceNode*)aTBSNode,
                                                           SMI_TBS_SWITCHING_TO_OFFLINE )
              != IDE_SUCCESS );
    sState = 1;

    ///////////////////////////////////////////////////////////////////////////
    //  (030) "TBSNode.Status := Offline" �� ���� �α� 
    //
    IDE_TEST( writeAlterTBSStateLog( aTrans,
                                     aTBSNode,
                                     SCT_UPDATE_MRDB_ALTER_TBS_OFFLINE,
                                     SMI_TBS_ONLINE,  /* State To Remove */
                                     SMI_TBS_OFFLINE, /* State To Add */
                                     & sNewTBSState )
              != IDE_SUCCESS );
    
    ///////////////////////////////////////////////////////////////////////////
    //  (050) Instant Memory Aging �ǽ�
    //        - Aging �����߿��� ��� Ager ��ġȹ��
    //
    IDE_TEST( smLayerCallback::doInstantAgingWithMemTBS(
                  aTBSNode->mHeader.mID ) != IDE_SUCCESS );
    
    ///////////////////////////////////////////////////////////////////////////
    //  (060) Checkpoint Latch ȹ�� 
    //
    //  (070)    Dirty Page Flush �ǽ�
    //           ( Unstable DB ����, 0�� 1�� DB ��� �ǽ� )
    //  (080) Checkpoint Latch ����
    //
    IDE_TEST( smrRecoveryMgr::flushDirtyPages4AllTBS()
              != IDE_SUCCESS );


    //////////////////////////////////////////////////////////////////////
    //  (090)   TBSNode�� Dirty Page�� ������ ASSERT�� Ȯ��
    IDE_TEST( smrRecoveryMgr::assertNoDirtyPagesInTBS( aTBSNode )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (100) Commit Pending���
    //
    // Transaction Commit�ÿ� ������ Pending Operation��� 
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                  aTrans,
                  aTBSNode->mHeader.mID,
                  ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                  SCT_POP_ALTER_TBS_OFFLINE,
                  & sPendingOp )
              != IDE_SUCCESS );

    // Commit�� sctTableSpaceMgr::executePendingOperation����
    // ������ Pending�Լ� ����
    sPendingOp->mPendingOpFunc = smpTBSAlterOnOff::alterOfflineCommitPending;
    sPendingOp->mNewTBSState   = sNewTBSState;
    sState = 0;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    switch( sState)
    {
    case 1:
        // Tablespace�� ���¿��� SMI_TBS_SWITCHING_TO_OFFLINE�� ����
        aTBSNode->mHeader.mState &= ~SMI_TBS_SWITCHING_TO_OFFLINE;
        break;

    default:
        break;
    }
    IDE_POP();

    return IDE_FAILURE;
    
}


/*
   Tablespace�� OFFLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
  
   PROJ-1548 User Memory Tablespace
 
   Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
   - ���� : Tablespace�� Lock������ �ٸ� Tx���� ����ϸ鼭
             ������ �� �ֱ� ������ �����ؼ��� �ȵȴ�.
  
   [����] sctTableSpaceMgr::executePendingOperation ���� ȣ��ȴ�.

   [ �˰��� ] ======================================================
      (c-010) TBSNode.Status := OFFLINE
      (c-020) latch TBSNode.SyncMutex        // Checkpoint�� ����
      (c-030) unlatch TBSNode.SyncMutex
      (c-040) Free All Index Memory of TBS
      (c-050) Destroy/Free Runtime Info At Table Header
      (c-060) Free All Page Memory of TBS ( �����޸��� ��� ��� ���� )
      (c-070) Free Runtime Info At TBSNode ( Expcet Lock, DB File Objects )
              - Offline�� Tablespace�� Checkpoint��
                redo LSN�� DB File Header�� ����ؾ� �ϱ� ������
                DB File ��ü���� �ı����� �ʴ´�.
      (c-080) flush TBSNode to loganchor

*/
IDE_RC smpTBSAlterOnOff::alterOfflineCommitPending(
                          idvSQL*             aStatistics, 
                          sctTableSpaceNode * aTBSNode,
                          sctPendingOp      * aPendingOp )
{
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aPendingOp != NULL );

    idBool sPageCountLocked = ID_FALSE;
    
    // ���� ������ Tablespace�� �׻� Memory Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE );

    // Commit Pending���� �������� OFFLINE���� ���� ���̶��
    // Flag�� ���õǾ� �־�� �Ѵ�.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_SWITCHING_TO_OFFLINE )
                == SMI_TBS_SWITCHING_TO_OFFLINE );
    
    /////////////////////////////////////////////////////////////////////
    // (c-010) TBSNode.Status := OFFLINE
    aTBSNode->mState = aPendingOp->mNewTBSState;

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� ������ �ȵȴ�.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_SWITCHING_TO_OFFLINE )
                != SMI_TBS_SWITCHING_TO_OFFLINE );
    
    /////////////////////////////////////////////////////////////////////
    // (c-020) latch TBSNode.SyncMutex
    // (c-030) unlatch TBSNode.SyncMutex
    //
    // ���� �� Thread���� ���ü� ����
    // - Alter TBS Offline Thread
    //    - �� �Լ��� finiPagePhase���� smmDirtyPageMgr�� ����.
    // - Checkpoint Thread
    //    - �� TBS�� smmDirtyPageMgr�� Open�ϰ� Read�õ�
    //      - �� Tablespace�� OFFLINE�Ǿ����Ƿ� Dirty Page�� ������
    //         Checkpoint���߿� smmDirtyPageMgr�� ����
    //         Open/Read/Close�۾��� ����ȴ�
    //
    // ���� Checkpoint�� smmDirtyPageMgr�� �����ϰ� �ϰ� �ִٸ�
    // TBSNode.SyncMutex �� ��� ���� ���̴�. 
    // ==> Mutex�� ��Ҵٰ� Ǯ� smmDirtyPageMgr���� ������ ������
    //     Ȯ���Ѵ�.
    //
    if ( smrRecoveryMgr::isRestart() == ID_FALSE )
    {
        // ��߿��� 
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aTBSNode )   
                  != IDE_SUCCESS );
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aTBSNode ) 
                  != IDE_SUCCESS );

        /////////////////////////////////////////////////////////////////////
        //  (c-040) Free All Index Memory of TBS
        //  (c-050) Destroy/Free Runtime Info At Table Header
        IDE_TEST( smLayerCallback::alterTBSOffline4Tables( aStatistics,
                                                           aTBSNode->mID )
                  != IDE_SUCCESS );

        // ChunkȮ��� ��� Tablespace�� Total Page Count�� ���µ�
        // �� ���߿� Page�ܰ踦 �������� �ȵȴ�.
        // ���� : Total Page Count�� ����
        //         smmFPLManager::getTotalPageCount4AllTBS��
        //         ����ִ� Alloc Page Count�� ������� Membase�� �����Ѵ�.
        //         �׷���, Membase�� Page�ܰ踦 �����鼭 NULL�� ���Ѵ�.
        //
        // ���� Tablespace�� DROP_PENDING����������,
        // DROP_PENDING���� ���� ������ Total Page Counting�� ���۵Ǿ�
        // �� Tablespace�� ���� Membase�� �����ϰ� ���� ���� �ִ�.
        //
        // => Page�ܰ�� ������ ���� GlobalPageCountCheck Mutex�� ȹ��.
        
        IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex() != IDE_SUCCESS );
        sPageCountLocked = ID_TRUE;

        
        /////////////////////////////////////////////////////////////////////
        //  (c-060) Free All Page Memory of TBS( �����޸��� ��� ��� ���� )
        //  (c-070) Free Runtime Info At TBSNode ( Expcet Lock, DB File Objects )
        IDE_TEST( smmTBSMultiPhase::finiPagePhase( (smmTBSNode*) aTBSNode )
                != IDE_SUCCESS );


        sPageCountLocked = ID_FALSE;
        IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex() != IDE_SUCCESS );

        /////////////////////////////////////////////////////////////////////
        //  (c-080) flush TBSNode to loganchor
        IDE_TEST( smrRecoveryMgr::updateTBSNodeToAnchor( aTBSNode )
                != IDE_SUCCESS );
    }
    else
    {
       // restart recovery�ÿ��� ���¸� �����Ѵ�. 
    }

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
    META/SERVICE�ܰ迡�� Tablespace�� Online���·� �����Ѵ�.

    [IN] aTrans   - ���¸� �����Ϸ��� Transaction
    [IN] aTBSNode - ���¸� ������ Tablespace�� Node

    [ �˰��� ]
      (010) TBSNode�� X�� ȹ��
      (020) Tablespace�� Backup���̶�� Backup����ñ��� ��� 
      (030) "TBSNode.Status := ONLINE"�� ���� �α�
      (040)  LogAnchor�� TBSNode.stableDB�� �ش��ϴ�
             Checkpoint Image�� �ε��Ѵ�.
      (050) Table�� Runtime���� �ʱ�ȭ
            - Mutex ��ü, Free Page���� ���� �ʱ�ȭ �ǽ�
            - ��� Page�� ���� Free Slot�� ã�Ƽ� �� Page�� �޾��ش�.
            - ���̺� ����� Runtime������ Free Page���� �������ش�.
      (060) �ش� TBS�� ���� ��� Table�� ���� Index Rebuilding �ǽ�
      (070) Commit Pending���
      (note-1) Log anchor�� TBSNode�� flush���� �ʴ´�. (commit pending���� ó��)

    [ Commit�� ] (pending)
      (c-010) TBSNode.Status := ONLINE    (��1)
      (c-020) Flush TBSNode To LogAnchor


    [ Abort�� ]
      [ UNDO ] ����
      
    [ REDO ]
      (r-010) (030)�� ���� REDO�� TBSNode.Status := After Image(ONLINE)
      (note-1) TBSNode�� loganchor�� flush���� ���� 
               -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
      (note-2) Page Redo�� PAGE�� NULL�̸� �ش� TBS���� Restore�� �ǽ��ϹǷ�
               (050)�� �۾��� �ʿ����� �ʴ�.
      (note-3) Restart Recovery�Ϸ��� (060), (070)�� �۾��� ����ǹǷ�
               Redo�߿� �̸� ó������ �ʴ´�.
               
    [ UNDO ]
      if RestartRecovery
         // do nothing
      else
         (u-010) (070)���� Index Rebuilding�� �޸� ����
         (u-020) (060)���� �ʱ�ȭ�� Table�� Runtime ���� ����
         (u-030) (050)���� Restore�� Page�� �޸� ����
         (u-040) (010)���� �Ҵ��� TBS�� Runtime���� ����
      fi
      (u-050) (020)�� ���� UNDO�� TBSNode.Status := Before Image(OFFLINE)
              -> TBSNode.Status�� Commit Pending���� ����Ǳ� ������
                 ���� undo�߿� Before Image�� ����ĥ �ʿ�� ����.
                 �׷��� �ϰ����� �����ϱ� ���� TBSNode.Status��
                 Before Image�� �����ϵ��� �Ѵ�.
                 
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS ONLINEE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
      
    [ �������� ] 
       �� �Լ��� META, SERVICE�ܰ迡�� ONLINE���� �ø� ��쿡�� ȣ��ȴ�.


    (��1) TBSNode.Status�� ONLINE�� �Ǹ� Checkpoint�� Dirty Page Flush��
          �� �� �ְ� �ȴ�. �׷��Ƿ� TBSNode.Status�� tablespace��
          ��� resource�� �غ��س��� commit pending���� ONLINE���� �����Ѵ�
       
 */
IDE_RC smpTBSAlterOnOff::alterTBSonline( void       * aTrans,
                                         smmTBSNode * aTBSNode )
{
    idvSQL       *   sStatistics;
    UInt             sStage = 0;
    sctPendingOp *   sPendingOp;
    UInt             sNewTBSState = 0;
    idBool           sPageCountLocked = ID_FALSE;
    scPageID         sTotalPageCount;
    ULong            sTBSCurrentSize;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSNode != NULL );

    sStatistics = smxTrans::getStatistics( aTrans );

    ///////////////////////////////////////////////////////////////////////////
    //  (010) TBSNode�� X�� ȹ��
    //
    // Tablespace�� Offline���¿��� ������ ���� �ʴ´�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode( 
                                   aTrans,
                                   & aTBSNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   SCT_VAL_ALTER_TBS_ONOFF) /* validation */
              != IDE_SUCCESS );

    // To Fix BUG ; ALTER TBS ONLINE�� MEM_MAX_DB_SIZE üũ�� ����
    //
    // MEM_MAX_DB_SIZEüũ�� ���� GlobalPageCountCheck Mutex�� ��´�.
    // GlobalPageCountCheck�� ��� ������ ������ ����.
    //
    //   - ��� : Tablespace���¸� SMI_TBS_SWITCHING_TO_ONLINE ���� ��������
    //   - Ǯ�� : Membase�� ���� Tablespace�� �Ҵ��  Page Count ������
    //            �����ϸ�, Page�ܰ谡 ������ ���ɼ��� ����,
    //            Membase�� �����Ͱ� �������� ������ ��������
    //            �����Ҽ� �ִ� ����
    //
    //   - ���� : ChunkȮ��ø���
    //             smmFPLManager::aggregateTotalPageCountAction ����
    //            Tablespace�� ���°� SMI_TBS_SWITCHING_TO_ONLINE�� ��� 
    //            Tablespace�� Membase�κ��� �о Page����
    //            DB�� Total Page���� ���Խ�Ű�� ����
    //            
    IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex() != IDE_SUCCESS );
    sPageCountLocked = ID_TRUE;

    ///////////////////////////////////////////////////////////////////////////
    //  (e-010) Tablespace���¿� ���� ����ó��
    IDE_TEST( sctTableSpaceMgr::checkError4AlterStatus( 
                                      (sctTableSpaceNode*)aTBSNode,
                                      SMI_TBS_ONLINE /* New State */ ) 
              != IDE_SUCCESS );
    

    ///////////////////////////////////////////////////////////////////////////
    //  (020) Tablespace�� Backup���̶�� Backup����ñ��� ���
    //
    //  ���¿� SMI_TBS_SWITCHING_TO_ONLINE �� �߰��Ѵ�.
    IDE_TEST( sctTableSpaceMgr::wait4BackupAndBlockBackup( sStatistics,
                                                           (sctTableSpaceNode*)aTBSNode,
                                                           SMI_TBS_SWITCHING_TO_ONLINE )
              != IDE_SUCCESS );
    sStage = 1;
    
    ///////////////////////////////////////////////////////////////////////////
    //  (030) "TBSNode.Status := ONLINE"�� ���� �α�
    //
    IDE_TEST( writeAlterTBSStateLog( aTrans,
                                     aTBSNode,
                                     SCT_UPDATE_MRDB_ALTER_TBS_ONLINE,
                                     SMI_TBS_OFFLINE,  /* State To Remove */
                                     SMI_TBS_ONLINE,   /* State To Add */
                                     & sNewTBSState )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (040)  Tablespace �ʱ�ȭ
    //       - Offline������ TBS�� STATE, MEDIA Phase���� �ʱ�ȭ�� �����̴�
    IDE_TEST( smmTBSMultiPhase::initPagePhase( aTBSNode )
              != IDE_SUCCESS );
    sStage = 2;

    ///////////////////////////////////////////////////////////////////////////
    //  (040)  LogAnchor�� TBSNode.stableDB�� �ش��ϴ�
    //         Checkpoint Image�� �ε��Ѵ�.
    //
    IDE_TEST( smmManager::prepareAndRestore( aTBSNode ) != IDE_SUCCESS );

    // ���� ONLINE��Ű���� Tablespace�� ������ ��� Tablespace����
    // ũ�⸦ ���Ѵ�.
    //
    // BUGBUG-1548 prepareAndRestore�� �ϱ� ���� �� üũ�� �����ؾ���
    //             ���� : prepareAndRestore���� Page Memory�� �����
    //             Membase�� ���ŵǰ� �Ҵ�� Page���� Tablespace Node����
    //             ������ �� �ְ� �Ǹ� �̿Ͱ��� ó������
    /*
     * BUG- 35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE
     */
    if( smuProperty::getSeparateDicTBSSizeEnable() == ID_TRUE )
    {
        /* 
         * SYS_TBS_MEM_DIC�� TBS online/offline�� �����Ҽ� ���� ������
         * SYS_TBS_MEM_DIC�� ������ TBS�� ũ�⸸ ���Ͽ� MEM_MAX_DB_SIZE �˻縦
         * �����Ѵ�. 
         * smiTableSpace::alterStatus()���� SYS_TBS_MEM_DIC ���� �˻��
         */
        IDE_TEST( smmFPLManager::getTotalPageCountExceptDicTBS( 
                                                            &sTotalPageCount )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smmFPLManager::getTotalPageCount4AllTBS( 
                                                        &sTotalPageCount ) 
                  != IDE_SUCCESS );
    }

    // ���� Tablespace�� �Ҵ�� Page Memoryũ�� : �����߻��� ����� ��
    sTBSCurrentSize = 
        smmDatabase::getAllocPersPageCount( aTBSNode->mMemBase ) *
        SM_PAGE_SIZE;
    
    IDE_TEST_RAISE( sTotalPageCount >
                    ( smuProperty::getMaxDBSize() / SM_PAGE_SIZE ),
                    error_unable_to_alter_online_cuz_mem_max_db_size );
    
    
    ///////////////////////////////////////////////////////////////////////////
    //  (050) Table�� Runtime���� �ʱ�ȭ
    ///////////////////////////////////////////////////////////////////////////
    //  (060) �ش� TBS�� ���� ��� Table�� ���� Index Rebuilding �ǽ�
    //
    IDE_TEST( smLayerCallback::alterTBSOnline4Tables( sStatistics,
                                                      aTrans,
                                                      aTBSNode->mHeader.mID )
              != IDE_SUCCESS );
    sStage = 3;
    
    ///////////////////////////////////////////////////////////////////////////
    //  (070) Commit Pending���
    //
    // Transaction Commit�ÿ� ������ Pending Operation��� 
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                              aTrans,
                              aTBSNode->mHeader.mID,
                              ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                              SCT_POP_ALTER_TBS_ONLINE,
                              & sPendingOp )
              != IDE_SUCCESS );
    // Commit�� sctTableSpaceMgr::executePendingOperation����
    // ������ Pending�Լ� ����
    sPendingOp->mPendingOpFunc = smpTBSAlterOnOff::alterOnlineCommitPending;
    sPendingOp->mNewTBSState   = sNewTBSState;

    sStage = 0;

    sPageCountLocked = ID_FALSE;
    IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex()
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_unable_to_alter_online_cuz_mem_max_db_size );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_ALTER_ONLINE_CUZ_MEM_MAX_DB_SIZE,
                     aTBSNode->mHeader.mName,
                     (ULong) (sTBSCurrentSize / 1024),
                     (ULong) (smuProperty::getMaxDBSize()/1024),
                     (ULong) ( ( (sTotalPageCount * SM_PAGE_SIZE ) -
                                 sTBSCurrentSize ) / 1024 )
                ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 3:
                //////////////////////////////////////////////////////////
                //  Free All Index Memory of TBS
                //  Destroy/Free Runtime Info At Table Header
                IDE_ASSERT( smLayerCallback::alterTBSOffline4Tables( sStatistics,
                                                                     aTBSNode->mHeader.mID )
                            == IDE_SUCCESS );
                
                
            case 2:
                //////////////////////////////////////////////////////////
                // initPagePhase�� prepareAndRestore����
                // �Ҵ�, �ʱ�ȭ�� ���� ����
                IDE_ASSERT( smmTBSMultiPhase::finiPagePhase( aTBSNode )
                            == IDE_SUCCESS );

            case 1:
                // GlobalPageCountCheck Lock�� Ǯ�� ���� 
                // Tablespace�� ���¿��� SMI_TBS_SWITCHING_TO_ONLINE�� ����
                // ����1 : ���� Page�ܰ谡 ������ ���·� Membase�� NULL�̰�                //         ���̻� Total Page Count�� ���µ� �� Tablespace��
                //         ���� ��Ű�� �ȵǱ� ����
                
                // ����2 : ALTER ONLINE�� UNDO�� �� FLAG�� �ȵ�� ������
                //         ����ó�� ��ƾ���� Tablespace�� ONLINE��Ű����
                //         �Ҵ��� ���ҽ��� ��� �����Ѱ����� �����ϰ�
                //         ���ҽ� UNDO�� ���� ����
                aTBSNode->mHeader.mState &= ~SMI_TBS_SWITCHING_TO_ONLINE;
                break;
        }

        
        if ( sPageCountLocked == ID_TRUE )
        {
        
            IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex()
                        == IDE_SUCCESS );
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}



/*
   Tablespace�� ONLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
  
   PROJ-1548 User Memory Tablespace
 
  
   [����] sctTableSpaceMgr::executePendingOperation ���� ȣ��ȴ�.

   [ �˰��� ] ======================================================
      (c-010) TBSNode.Status := ONLINE
      (c-020) Flush TBSNode To LogAnchor

*/
IDE_RC smpTBSAlterOnOff::alterOnlineCommitPending(
                          idvSQL*             /* aStatistics */, 
                          sctTableSpaceNode * aTBSNode,
                          sctPendingOp      * aPendingOp )
{
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aPendingOp != NULL );

    // ���� ������ Tablespace�� �׻� Memory Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE );
    
    // Commit Pending���� �������� ONLINE���� ���� ���̶��
    // Flag�� ���õǾ� �־�� �Ѵ�.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_SWITCHING_TO_ONLINE )
                == SMI_TBS_SWITCHING_TO_ONLINE );
    
    /////////////////////////////////////////////////////////////////////
    // (c-010) TBSNode.Status := ONLINE    
    aTBSNode->mState = aPendingOp->mNewTBSState;

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� ������ �ȵȴ�.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_SWITCHING_TO_ONLINE )
                != SMI_TBS_SWITCHING_TO_ONLINE );
    
    /////////////////////////////////////////////////////////////////////
    //  (c-020) flush TBSNode to loganchor
    if ( smrRecoveryMgr::isRestart() == ID_FALSE )
    {
        IDE_TEST( smrRecoveryMgr::updateTBSNodeToAnchor( aTBSNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // restart recovery�ÿ��� ���¸� �����Ѵ�. 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Restart REDO, UNDO���� �Ŀ� Offline ������ Tablespace�� �޸𸮸� ����
 *
 * Restart REDO, UNDO�߿��� Alter TBS Offline�α׸� ������
 * ���¸� OFFLINE���� �ٲٰ� TBS�� Memory�� �������� �ʴ´�.
 *  => ���� : OFFLINE, ONLINE ���̰� ����ϰ� �߻��� ���
 *            �Ź� restore�ϴ��� Restart Recovery������ ���ϵ� �� ����
 *      
 */
IDE_RC smpTBSAlterOnOff::finiOfflineTBS()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                  NULL, /* idvSQL* */
                                  finiOfflineTBSAction,
                                  NULL, /* Action Argument*/
                                  SCT_ACT_MODE_NONE ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * smpTBSAlterOnOff::destroyAllTBSNode�� ���� Action�Լ�
 */
IDE_RC smpTBSAlterOnOff::finiOfflineTBSAction( idvSQL*          /* aStatistics */,
                                               sctTableSpaceNode * aTBSNode,
                                               void * /* aActionArg */ )
{ 
    IDE_DASSERT( aTBSNode != NULL );

    if ( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE )
    {
        // Tablespace�� ���°� Offline�ε�
        if ( SMI_TBS_IS_OFFLINE(aTBSNode->mState) )
        {
            // Tablespace Page�� Load�Ǿ� �ִ� ��� 
            if ( ((smmTBSNode*)aTBSNode)->mRestoreType !=
                 SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
            {

                // Alter Tablespace Offline�� ����
                //   - ��� Dirty Page�� 0��, 1�� DB FIle�� Flush
                //   - on commit pending
                //     - flush to loganchor(Tablespace.state := OFFLINE)
                //
                // ��, Alter Tablespace Offline�� commit�Ǿ����� �ǹ��ϸ�
                // �̴� ��, Normal Processing�� ��� Dirty Page��
                // �̹� Disk�� ������ ������ �ǹ��Ѵ�.
                //
                // => Redo/Undo�� OFFLINE�� Tablespace�� ����
                //    Dirty Page�� ��� �����ص� �����ϴ�.
                IDE_TEST( smmManager::clearDirtyFlag4AllPages(
                                         (smmTBSNode*) aTBSNode )
                          != IDE_SUCCESS );
                
                //  Free All Page Memory of TBS
                //      ( �����޸��� ��� ��� ���� )
                //  Free Runtime Info At TBSNode
                //      ( Expcet Lock, DB File Objects )
                IDE_TEST( smmTBSMultiPhase::finiPagePhase(
                              (smmTBSNode*) aTBSNode ) != IDE_SUCCESS );
                
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
