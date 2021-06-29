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
#include <svmTBSCreate.h>
#include <svmTBSAlterAutoExtend.h>

/*
  ������ (�ƹ��͵� ����)
*/
svmTBSCreate::svmTBSCreate()
{

}

/*
     Tablespace Attribute�� �ʱ�ȭ �Ѵ�.

     [IN] aTBSAttr        - �ʱ�ȭ �� Tablespace Attribute
     [IN] aName           - Tablespace�� ������ Database�� �̸�
     [IN] aType           - Tablespace ����
                            User||System, Memory, Data || Temp
     [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��
     [IN] aState          - Tablespace�� ����

     [ �˰��� ]
       (010) �⺻ �ʵ� �ʱ�ȭ
       (020) mSplitFilePageCount �ʱ�ȭ
       (030) mInitPageCount �ʱ�ȭ

     [ ����ó�� ]
       (e-010) aSplitFileSize �� Expand Chunkũ���� ����� �ƴϸ� ����
       (e-020) aInitSize �� Expand Chunkũ���� ����� �ƴϸ� ����

*/
IDE_RC svmTBSCreate::initializeTBSAttr( smiTableSpaceAttr    * aTBSAttr,
                                         smiTableSpaceType      aType,
                                         SChar                * aName,
                                         UInt                   aAttrFlag,
                                         ULong                  aInitSize,
                                         UInt                   aState)
{
    ULong sChunkSize = smuProperty::getExpandChunkPageCount() * SM_PAGE_SIZE;

    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aName != NULL );

    IDE_ASSERT( sChunkSize > 0 );

    // Volatile Tablespace�� ��� Log Compression����ϰڴٰ� �ϸ�
    // ������ �߻�
    IDE_TEST_RAISE( (aAttrFlag & SMI_TBS_ATTR_LOG_COMPRESS_MASK)
                    == SMI_TBS_ATTR_LOG_COMPRESS_TRUE,
                    err_volatile_log_compress );

    idlOS::memset( aTBSAttr, 0, ID_SIZEOF( *aTBSAttr ) );

    aTBSAttr->mAttrType   = SMI_TBS_ATTR;
    aTBSAttr->mType       = aType;
    aTBSAttr->mAttrFlag   = aAttrFlag;

    idlOS::strncpy( aTBSAttr->mName, aName, SMI_MAX_TABLESPACE_NAME_LEN );
    aTBSAttr->mName[SMI_MAX_TABLESPACE_NAME_LEN] = '\0';

    // BUGBUG-1548 Refactoring mName�� '\0'���� ������ ���ڿ��̹Ƿ�
    //             mNameLength�� �ʿ䰡 ����. �����Ұ�.
    aTBSAttr->mNameLength = idlOS::strlen( aName );
    aTBSAttr->mTBSStateOnLA      = aState;

    // ����ڰ� Tablespace�ʱ� ũ�⸦ �������� ���� ���
    if ( aInitSize == 0 )
    {
        // �ּ� Ȯ������� Chunkũ�⸸ŭ ����
        aInitSize = sChunkSize;
    }

    IDE_TEST_RAISE ( ( aInitSize % sChunkSize ) != 0,
                     error_tbs_init_size_not_aligned_to_chunk_size );

    aTBSAttr->mVolAttr.mInitPageCount = aInitSize / SM_PAGE_SIZE;

    return IDE_SUCCESS;


    IDE_EXCEPTION( err_volatile_log_compress );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION( error_tbs_init_size_not_aligned_to_chunk_size );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TBSInitSizeNotAlignedToChunkSize,
                                // KB ������ Expand Chunk Page ũ��
                                ( sChunkSize / 1024 )));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
     ����� �޸� Tablespace�� �����Ѵ�.
     [IN] aTrans          - Tablespace�� �����Ϸ��� Transaction
     [IN] aDBName         - Tablespace�� ������ Database�� �̸�
     [IN] aType           - Tablespace ����
                            User||System, Memory, Data || Temp
     [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
     [IN] aName           - Tablespace�� �̸�
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��
                            Meta Page(0�� Page)�� �������� �ʴ� ũ���̴�.
     [IN] aIsAutoExtend   - Tablespace�� �ڵ�Ȯ�� ����
     [IN] aNextSize       - Tablespace�� �ڵ�Ȯ�� ũ��
     [IN] aMaxSize        - Tablespace�� �ִ�ũ��
     [IN] aState          - Tablespace�� ����
     [OUT] aTBSID         - ������ Tablespace�� ID
 */
IDE_RC svmTBSCreate::createTBS( void                 * aTrans,
                                 SChar                * aDBName,
                                 SChar                * aTBSName,
                                 smiTableSpaceType      aType,
                                 UInt                   aAttrFlag,
                                 ULong                  aInitSize,
                                 idBool                 aIsAutoExtend,
                                 ULong                  aNextSize,
                                 ULong                  aMaxSize,
                                 UInt                   aState,
                                 scSpaceID            * aTBSID )
{
    smiTableSpaceAttr    sTBSAttr;
    svmTBSNode         * sCreatedTBSNode;


    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aDBName != NULL );
    IDE_DASSERT( aTBSName != NULL );
    // aChkptPathList�� ����ڰ� �������� ���� ��� NULL�̴�.
    // aSplitFileSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aInitSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aNextSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aMaxSize�� ����ڰ� �������� ���� ��� 0�̴�.

    IDE_TEST( initializeTBSAttr( &sTBSAttr,
                                 aType,
                                 aTBSName,
                                 aAttrFlag,
                                 aInitSize,
                                 aState) != IDE_SUCCESS );

    // BUGBUG-1548-M3 mInitPageCount��ŭ TBS������ MEM_MAX_DB_SIZE�ȳѾ���� üũ
    // BUGBUG-1548-M3 mNextPageCount�� EXPAND_CHUNK_PAGE_COUNT�� ������� üũ
    // BUGBUG-1548-M3 mIsAutoExtend�� ���� ûũ �ڵ�Ȯ�忩�� �����ϱ�

    IDE_TEST( createTableSpaceInternal(aTrans,
                                       aDBName,
                                       &sTBSAttr,
                                       sTBSAttr.mVolAttr.mInitPageCount,
                                       &sCreatedTBSNode )
             != IDE_SUCCESS );

    IDE_TEST( svmTBSAlterAutoExtend::alterTBSsetAutoExtend(
                  aTrans,
                  sTBSAttr.mID,
                  aIsAutoExtend,
                  aNextSize,
                  aMaxSize ) != IDE_SUCCESS );

    if ( aTBSID != NULL )
    {
        *aTBSID = sTBSAttr.mID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   �޸� Tablespace�� �����Ѵ�.

   aTrans              [IN] Tablespace�� �����Ϸ��� Transaction
   aDBName             [IN] Tablespace�� ���ϴ� Database�� �̸�
   aTBSAttr            [IN] �����Ϸ��� Tablespace�� �̸�
   aChkptPathAttrList  [IN] Checkpoint Path Attribute�� List
   aCreatedTBSNode     [OUT] ������ Tablespace�� Node

[ PROJ-1548 User Memory Tablespace ]

Create Tablespace ��ü �˰��� =============================================

* Tablespace Attribute, Checkpoint Path�� ����
  Restart Recovery�� Disk Tablespace�� �⺻������ �����ϰ� ó���Ѵ�.
  ( Commit���� Log Anchor�� Flush�صΰ�
    Redo�ÿ� ���� ������ �����ϴ� Pending���길 �޾��ش�. )

(010) Tablespace ID�Ҵ�

(015) �α�ǽ� => CREATE_TBS

(020) allocate TBSNode
(030) Lock TBSNode                 // Create���� TBS�� DML/DDL�� ����
(050) TBS List�� TBSNode�� �߰�

(060) Checkpoint Path Node���� ����
(070) TBSNode�� ���õ� ��� ���ҽ� �ʱ�ȭ
(080) create Checkpoint Image Files (init size��ŭ)

BUGBUG-1548 ���� �̸��� Checkpoint Image���� ����� ����ó��
=> Forward Processing��, Restart Redo�÷� �����Ͽ� ���

(090) initialize PCHArray
(100) initialize Membase on Page 0
(110) alloc ExpandChunks

(120) flush Page 0 to Checkpoint Image (��5)

(130) Log Anchor Flush   // Commit �α׸� ��ϵǰ� Pendingó�� �ȵ� ���
                         // ����Ͽ� Log Anchor��
                         // CREATE���� Tablespace�� Flush

(140) Commit Pending���


- commit : (pending)
           (c-010) Status := Online(��2)
           (c-020) flush TBSNode (��3)
           <commit �Ϸ�>

- Restart Redo�� Commit Pending => Forward Processing�ÿ� ���������� ó��

- CREATE_TBS �� ���� REDO :
  - �ƹ��� ó���� ���� ����
  - ���� : Create Tablespace�� Commit Pending���� Tablespace���� ������
           ��� Log Anchor�� Flush�߱� ����

- CREATE_TBS �� ���� UNDO :
  - Forward Processing�� Abort�߻��� ȣ��
  - Restart Undo�� ȣ��
  - �˰���
           (a-010) (�α�ǽ�-CLR-CREATETBS-ABORT)
           (a-010) TBSID �� �ش��ϴ� TBSNode�� ã��
           if ( TBSNode == NULL ) // TBSNode�� ����
             // Do Nothing ( Create �� ���� Undo�� �ƹ��͵� �� �ʿ� ���� )
           else // TBSNode�� ����
             (a-020) latch TBSNode.SyncMutex
             (a-030)   Status := DROPPED       // TBSNode���¸� DROPPED�� ����
             (a-040) unlatch TBSNode.SyncMutex  // ���ķδ� Checkpoint�� ����
             (a-050) close all Checkpoint Image Files
             (a-060) delete Checkpoint Image Files
             (a-070) Lock���� ������ ��� ��ü �ı�, �޸� �ݳ� (��4)
             (a-080) flush TBSNode
           fi
           <abort �Ϸ�>

- CLR-CREATETBS-ABORT�� ���� REDO :
  - �޸𸮳� ������ �����ϴ� ��쿡�� �޸� �ݳ�, ���ϻ��� �ǽ�
    - ���� : �̹� CLR�� �ش��ϴ� ������ REDO�Ǿ��� �� �ֱ� ����.
           if ( TBSNode ���� )
              (clr-010) Status := DROPPED
              (clr-020) Lock���� ������ ��� ��ü �ı�, �޸� �ݳ� (��4)
              (clr-030) TBS�� ���õ� ������ ������ ����
           else
              // Tablespace Node�� List���� ���ŵ� ����̸�,
              // �ƹ��͵� �� �ʿ� ����
           fi

- checkpoint :
  - Creating���� TBS�� ���ؼ� Checkpoint�� ����Ѵ�.
      => (080)���� Checkpoint Image���� ���� �����ϰ�
         (100), (110)���� Dirty Page �߰��ϱ� ������
         Checkpoint�� �߻��Ͽ� Dirty Page Flush�Ǿ ������ ����.
         ��, UNDO���� Checkpoint Image�� ����� ����
         Sync Mutex�� ��Ƽ� Checkpoint���� ������ ���´�.

  - Drop��, Ȥ�� Create Tablespace�� UNDO�Ǿ� DROP���·� ��
    Tablespace�� ���� Checkpoint�� �������� �ʴ´�.
      => Checkpoint���� TBSNode�� SyncMutex�� ��� ���°˻� �ǽ�
           latch TBSNode.SyncMutex
              if ( TBSNode.Status & DROPPED )
              {
                 // Do Nothing
              }
              else
              {
                 Checkpoint(Dirty Page Flush) �ǽ�
              }
           unlatch TBSNode.SyncMutex


- (��1) : TBS�� X���� ��Ƽ� TBS�����ϴ� TX�� COMMIT�ϱ� ����
         �ƹ��� TBS�� �������� ���ϵ��� �����ش�.

- (��2) : TBSNode�� Status�� Creating���� Online���� �������ش�.

- (��3) : Commit Pending���� Log Anchor�� Flush�ǰ� ����
          Transction�� Commit�� �Ϸ�ȴ�.
          ����
          TBSNode�� commit������ Log Anchor�� �ٷ� Flush�ص� �����ϴ�.
          ��, �� �� abort�� TBSNode�� Log Anchor���� �����ִ� �߰��۾��� �ʿ��ϴ�.
- (��4) : �ش� TBSNode�� Lock�� ����ϴ� �ٸ� Tx�� ���� �� �ֱ� ������,
          TBSNode��ü�� TBS List���� ������ �� ����.
          ((��)Create Tablespace TBS1�� Commit�Ǳ� ����
           (��)Create Table In TBS1�� ���� ���,
           (��)�� TBS1�� Lock�� ����Ѵ�. )

- (��5) : Membase�� ����ִ� 0�� Page�� Restart Recovery������,
          Prepare/Restore DB�ÿ� �о�鿩�� �� ���� File��
          Restore�� ���� �����ϰ� �ȴ�.
          Restrart Redo�� ���� ���� ���¿����� �� ����
          Create Tablespace����� ������ �����ϱ� ����
          0�� Page�� Flush�Ѵ�.

          BUGBUG-�ּ������

- Latch�� Deadlockȸ�Ǹ� ���� Latch��� ����

    1. sctTableSpaceMgr::lock()           // TBS LIST
    2. sctTableSpaceMgr::latchSyncMutex() // TBS NODE
    3. svmPCH.mPageMemMutex.lock()        // PAGE

- Lock�� Latch���� Deadlockȸ�Ǹ� ���� Lock/Latch��� ����
    1. Lock��  ���� ��´�.
    2. Latch�� ���߿� ��´�.

*/
IDE_RC svmTBSCreate::createTableSpaceInternal(
                          void                 * aTrans,
                          SChar                * aDBName,
                          smiTableSpaceAttr    * aTBSAttr,
                          scPageID               aCreatePageCount,
                          svmTBSNode          ** aCreatedTBSNode )
{
    svmTBSNode   * sNewTBSNode = NULL;
    idBool         sIsCreateLatched = ID_FALSE;
    sctPendingOp * sPendingOp;

    IDE_DASSERT( aTBSAttr->mName[0] != '\0' );
    IDE_DASSERT( aCreatePageCount > 0 );
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );

    IDE_TEST_RAISE(idlOS::strlen(aTBSAttr->mName) > (SM_MAX_DB_NAME -1),
                   too_long_tbsname);

    ///////////////////////////////////////////////////////////////
    // (010) Tablespace ID�Ҵ�
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sIsCreateLatched = ID_TRUE;

    IDE_TEST( sctTableSpaceMgr::allocNewTableSpaceID(&aTBSAttr->mID)
              != IDE_SUCCESS );

    sIsCreateLatched = ID_FALSE;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );


    ///////////////////////////////////////////////////////////////
    // (015) �α�ǽ� => CREATE_TBS
    IDE_TEST( smLayerCallback::writeVolatileTBSCreate( NULL, /* idvSQL */
                                                       aTrans,
                                                       aTBSAttr->mID )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (020) allocate TBSNode
    // (030) Lock TBSNode             // DML/DDL�� Create���� TBS���� ����
    // (040) Latch TBSNode.SyncMutex // Checkpoint�� Create���� TBS���� ����
    // (050) TBS List�� TBSNode�� �߰�

    // createAndLockTBSNode����
    // TBSNode->mTBSAttr.mState �� CREATING | ONLINE �� �ǵ��� �Ѵ�.
    // Commit�� Pending���� CREATING�� ���ְ�,
    // Abort�� writeVolatileTBSLog�� ���� UNDO�� ���¸� DROPPED�� �����Ѵ�.

    // createAndLockTBSNode���� TBSNode.mState�� TBSAttr.mTBSStateOnLA�� ����
    //
    //
    // - Volatile Tablespace�� ���
    //   SMI_TBS_INCONSISTENT���°� �ʿ����� �ʴ�.
    //   - ���� : Membase, Checkpoint Image���� ��� ����
    //   -         Tablespace�� �����Ǵ� ���߿�
    //             �Ͻ������� INCONSISTENT�� ���°� �Ǵ� ���� ����
    aTBSAttr->mTBSStateOnLA = SMI_TBS_CREATING | SMI_TBS_ONLINE;

    // Tablespace Node�� �����Ͽ� TBS Node List�� �߰��Ѵ�.
    // - TBSNode�� ���� Lock�� X�� ��´�. ( �Ϲ� DML,DDL�� ���� )
    // - Sync Mutex�� Latch�� ȹ���Ѵ�. ( Checkpoint�� ���� )
    IDE_TEST( createAndLockTBSNode(aTrans,
                                   aTBSAttr)
              != IDE_SUCCESS );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTBSAttr->mID,
                                                        (void**)&sNewTBSNode )
              != IDE_SUCCESS );

    // (080) initialize PCHArray
    // (100) alloc ExpandChunks
    IDE_TEST( svmManager::createTBSPages(
                  sNewTBSNode,
                  aDBName,
                  aCreatePageCount )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (110) Log Anchor Flush
    //
    // Commit �α׸� ��ϵǰ� Pendingó�� �ȵ� ��쿡 ����Ͽ�
    // Log Anchor�� CREATE���� Tablespace�� Flush
    IDE_TEST( svmTBSCreate::flushTBSNode(sNewTBSNode) != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (120) Commit Pending���
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                  aTrans,
                  sNewTBSNode->mHeader.mID,
                  ID_TRUE, /* commit�ÿ� ���� */
                  SCT_POP_CREATE_TBS,
                  & sPendingOp)
              != IDE_SUCCESS );

    sPendingOp->mPendingOpFunc = svmTBSCreate::createTableSpacePending;

    *aCreatedTBSNode = sNewTBSNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION(too_long_tbsname);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_TooLongTBSName,
                                  (UInt)SM_MAX_DB_NAME ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsCreateLatched == ID_TRUE )
    {
        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
    }

    // (030)���� ȹ���� Tablespace X Lock�� UNDO�Ϸ��� �ڵ����� Ǯ�Եȴ�
    // ���⼭ ���� ó���� �ʿ� ����

    IDE_POP();

    return IDE_FAILURE;
}
/*
   Tablespace�� Create�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�

   [ �˰��� ]
     (010) latch TableSpace Manager
     (020) TBSNode.State ���� CREATING���� ����
     (030) TBSNode�� Log Anchor�� Flush
     (040) unlatch TableSpace Manager
   [ ���� ]
     Commit Pending�Լ��� �����ؼ��� �ȵǱ� ������
     ���� ���� Detect�� ���� IDE_ASSERT�� ����ó���� �ǽ��Ͽ���.
 */

IDE_RC svmTBSCreate::createTableSpacePending( idvSQL*             aStatistics,
                                              sctTableSpaceNode * aSpaceNode,
                                              sctPendingOp      * /*aPendingOp*/ )
{
    IDE_DASSERT( aSpaceNode != NULL );

    /////////////////////////////////////////////////////////////////////
    // (010) latch TableSpace Manager
    //       �ٸ� Tablespace�� ����Ǵ� ���� Block�ϱ� ����
    //       �ѹ��� �ϳ��� Tablespace�� ����/Flush�Ѵ�.

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    ///////////////////////////////////////////////////////////////
    // (020) TBSNode.State ���� CREATING���� ����

    IDE_ASSERT( SMI_TBS_IS_ONLINE(aSpaceNode->mState) );
    IDE_ASSERT( SMI_TBS_IS_CREATING(aSpaceNode->mState) );
    aSpaceNode->mState &= ~SMI_TBS_CREATING;

    ///////////////////////////////////////////////////////////////
    // (030) TBSNode�� Log Anchor�� Flush
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


/*
    svmTBSNode�� �����Ͽ� sctTableSpaceMgr�� ����Ѵ�.

    [ Lock / Latch���� deadlock ȸ�� ]
    - Lock�� ���� ��� Latch�� ���߿� ��Ƽ� Deadlock�� ȸ���Ѵ�.
    - �� �Լ��� �Ҹ��� ���� latch�� ���� ���¿����� �ȵȴ�.


    [IN] aTBSAttr    - ������ Tablespace Node�� �Ӽ�
    [IN] aOp         - Tablespace Node�������� Option
    [IN] aIsOnCreate - create tablespace ������ ��� ID_TRUE
    [IN] aTrans      - Tablespace �� ���� Lock�� ����� �ϴ� ���
                       Lock�� ���� Transaction
*/
IDE_RC svmTBSCreate::createAndLockTBSNode(void              *aTrans,
                                           smiTableSpaceAttr *aTBSAttr)
{
    UInt                 sStage = 0;
    svmTBSNode         * sTBSNode = NULL;
    sctLockHier          sLockHier = { NULL, NULL };
    idBool               sIsLocked = ID_FALSE;


    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );

    // sTBSNode�� �ʱ�ȭ �Ѵ�.
    IDE_TEST(svmManager::allocTBSNode(&sTBSNode, aTBSAttr)
             != IDE_SUCCESS);

    sStage = 1;

    // Volatile Tablespace�� �ʱ�ȭ�Ѵ�.
    IDE_TEST(svmManager::initTBS(sTBSNode) != IDE_SUCCESS);

    sStage = 2;

    // Lockȹ��� Lock Slot�� sLockHier->mTBSNodeSlot�� �Ѱ��ش�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                  aTrans,
                  & sTBSNode->mHeader,
                  ID_FALSE,   /* intent */
                  ID_TRUE,    /* exclusive */
                  ID_ULONG_MAX, /* lock wait micro sec */
                  SCT_VAL_DDL_DML, /* validation */
                  NULL,       /* is locked */
                  & sLockHier )      /* lockslot */
              != IDE_SUCCESS );

    // register it.
    /* 1. �ٸ� TBS�� ��� ���� Ȯ���ϹǷ� SpaceNode Lock���� ��ȣ�� �ȵȴ�.
     * 2. ���� ���� ���� ���� �ʾ����Ƿ� SpaceNode Lock�� ���� �ʾƵ� �ȴ�.
     * 3. checkExistSpaceNodeByName() �� SpaceNodeArray ���� Ȯ���Ѵ�.
     *    ���� ������ �̸��� ������ Ȯ���ϰ� SpaceNodeArray�� �߰� �� �� ����
     *    Lock���� ��ȣ �Ǿ�� �Ѵ�.
     */
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ��� ���� �޽����� ��ȯ���� �ʵ��� ����
    IDE_TEST_RAISE( sctTableSpaceMgr::checkExistSpaceNodeByName( sTBSNode->mHeader.mName )
                    == ID_TRUE, err_already_exist_tablespace_name );

    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode*)sTBSNode );
    sStage = 3;

    sIsLocked = ID_FALSE;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_already_exist_tablespace_name )
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistTableSpaceName,
                                sTBSNode->mHeader.mName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sLockHier.mTBSNodeSlot != NULL )
    {
        IDE_ASSERT( smLayerCallback::unlockItem( aTrans,
                                                 sLockHier.mTBSNodeSlot )
                    == IDE_SUCCESS );
    }

    switch(sStage)
    {
        case 3:
            sctTableSpaceMgr::removeTableSpaceNode(
                                  (sctTableSpaceNode*)sTBSNode );

        case 2: 
            /* BUG-39806 Valgrind Warning
             * - svmTBSDrop::dropTableSpacePending() �� ó���� ���ؼ�, ���� �˻�
             *   �ϰ� svmManager::finiTBS()�� ȣ���մϴ�.
             */
            if ( ( sTBSNode->mHeader.mState & SMI_TBS_DROPPED )
                 != SMI_TBS_DROPPED )
            {
                IDE_ASSERT( svmManager::finiTBS( sTBSNode ) == IDE_SUCCESS );
            }
            else
            {
                // Drop�� TBS��  �̹� �ڿ��� �����Ǿ� �ִ�.
            }
        case 1:
            IDE_ASSERT(svmManager::destroyTBSNode(sTBSNode) == IDE_SUCCESS);
        default:
           break;
    }

    if ( sIsLocked == ID_TRUE )
    {
        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
    Tablespace Node�� Log Anchor�� flush�Ѵ�.

    [IN] aTBSNode - Flush�Ϸ��� Tablespace Node
 */
IDE_RC svmTBSCreate::flushTBSNode(svmTBSNode * aTBSNode)
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smLayerCallback::addTBSNodeAndFlush( (sctTableSpaceNode*)aTBSNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


