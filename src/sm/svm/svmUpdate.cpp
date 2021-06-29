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
#include <ideErrorMgr.h>
#include <svmReq.h>
#include <svmUpdate.h>
#include <svmExpandChunk.h>
#include <sctTableSpaceMgr.h>

/*
 * Update type : SCT_UPDATE_VRDB_CREATE_TBS
 *
 * Volatile tablespace ������ ���� redo ����
 *
 * Commit���� ������ Log Anchor�� Tablespace�� Flush�Ͽ��� ������
 * Tablespace�� Attribute�� Checkpoint Path�� ���� �����鿡 ����
 * ������ Redo�� ������ �ʿ䰡 ����.
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::redo_SCT_UPDATE_DRDB_CREATE_TBS �� �ּ� ����
 *    ( ���⿡ �ּ��� �˰��� ��,��,��,...�� ���� ��ȣ�� ����Ǿ� ����)
 */
IDE_RC svmUpdate::redo_SCT_UPDATE_VRDB_CREATE_TBS(
                    idvSQL        * /*aStatistics*/,
                    void          * aTrans,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /* aFileID */,
                    UInt            aValueSize,
                    SChar         * /* aValuePtr */,
                    idBool          /* aIsRestart */ )
{
    svmTBSNode     *  sSpaceNode;
    sctPendingOp   *  sPendingOp;

    IDE_ERROR( aTrans != NULL );

    // After Image�� ����� �Ѵ�.
    IDE_ERROR( aValueSize == 0 );

    // Loganchor�κ��� �ʱ�ȭ�� TBS List�� �˻��Ѵ�. 
    sSpaceNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sSpaceNode != NULL )
    {
        if ( SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) )
        {
            // �˰��� (��)�� �ش��ϴ� CREATINIG ������ ��쿡�� �����Ƿ� 
            // ���¸� ONLINE���� ������ �� �ְ� Commit Pending ������ ����Ѵ�. 

            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                    aTrans,
                                                    aSpaceID,
                                                    ID_TRUE, /* commit�ÿ� ���� */
                                                    SCT_POP_CREATE_TBS,
                                                    & sPendingOp )
                      != IDE_SUCCESS );

            sPendingOp->mPendingOpFunc = svmTBSCreate::createTableSpacePending;
            
            // �˰��� (��)�� �ش��ϴ� ���� Rollback Pending �����̱� ������
            // undo_SCT_UPDATE_DRDB_CREATE_TBS()���� POP_DROP_TBS ���� ����Ѵ�.
        }
        else
        {
            // �˰��� (��) �� �ش��ϹǷ� ��������� �ʴ´�. 
        }
    }
    else
    {
        // �˰��� (��) �� �ش��ϹǷ� ��������� �ʴ´�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Update type : SCT_UPDATE_VRDB_CREATE_TBS
 *
 * Volatile tablespace ������ ���� undo ����.
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::undo_SCT_UPDATE_DRDB_CREATE_TBS �� �ּ� ����
 *    ( ���⿡ �ּ��� �˰��� ��,��,��,...�� ���� ��ȣ�� ����Ǿ� ����)
 * 
 * after image : tablespace attribute
 */
IDE_RC svmUpdate::undo_SCT_UPDATE_VRDB_CREATE_TBS(
                    idvSQL        * aStatistics,
                    void          * aTrans,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /* aFileID */,
                    UInt            /* aValueSize */,
                    SChar         * /* aValuePtr */,
                    idBool          /* aIsRestart */  )
{
    UInt             sState = 0;
    svmTBSNode     * sSpaceNode;
    sctPendingOp   * sPendingOp;

    // TBS List�� �˻��Ѵ�. 
    sSpaceNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sSpaceNode != NULL )
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED );

        if ( SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) )
        {
            // CREATE TBS ���꿡���� 
            // ��� �������� �����ϴ��� Loganchor�� DROPPING���°� ����� �� �����Ƿ� 
            // RESTART�ÿ��� DROPPING ���°� ���� �� ����.
            IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPING) 
                       != SMI_TBS_DROPPING );

            // RESTART �˰��� (��),(��)�� �ش��Ѵ�.
            // RUNTIME �˰��� (��)�� �ش��Ѵ�.

            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                  aTrans,
                                                  sSpaceNode->mHeader.mID,
                                                  ID_FALSE, /* abort �� ���� */
                                                  SCT_POP_DROP_TBS,
                                                  &sPendingOp) != IDE_SUCCESS );

            // Pending �Լ� ����.
            // ó�� : Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
            sPendingOp->mPendingOpFunc = svmTBSDrop::dropTableSpacePending;

            // �������̴� Checkpoint Image��� ���� 
            sPendingOp->mTouchMode = SMI_ALL_TOUCH ;
            
            sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;
        }
        else
        {
            // �˰��� RESTART (��)�� ����ȴ�.
            // nothing to do ..
            IDE_DASSERT( 0 );
        }

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // RESTART �˰��� (��) �ش�
        // RUNTIME �˰��� (��) �ش�
        // nothing to do ...
    }

    // RUNTIME�ÿ� ������ �߻��ߴٸ� Rollback Pending�� ��ϵǾ��� ���̰�
    // Rollback Pending�� Loganchor�� �����Ѵ�. 
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    
    return IDE_FAILURE;    
}

/*
 * Update type : SCT_UPDATE_VRDB_DROP_TBS
 *
 * Volatile tablespace Drop�� ���� redo ����.
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::redo_SCT_UPDATE_DRDB_DROP_TBS �� �ּ� ����
 *    ( ���⿡ �ּ��� �˰��� ��,��,��,...�� ���� ��ȣ�� ����Ǿ� ����)
 * 
 *   [�α� ����]
 *   After Image ��� ----------------------------------------
 *      smiTouchMode  aTouchMode
 */
IDE_RC svmUpdate::redo_SCT_UPDATE_VRDB_DROP_TBS(
                    idvSQL        * /*aStatistics*/,
                    void          * aTrans,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /* aFileID */,
                    UInt            aValueSize,
                    SChar         * /*aValuePtr*/,
                    idBool          /* aIsRestart */  )
{
    svmTBSNode        * sSpaceNode;
    sctPendingOp      * sPendingOp;
    
    IDE_ERROR( aTrans != NULL );
    IDE_ERROR( aValueSize == 0 );

    // TBS List���� �˻��Ѵ�. 
    sSpaceNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sSpaceNode != NULL )
    {
        // DROPPED������ TBS�� findSpaceNodeWithoutException���� �ǳʶڴ�
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
                   != SMI_TBS_DROPPED );
        
        // PRJ-1548 User Memory Tablespace
        // DROP TBS ������ commit�� �ƴϱ� ������ DROPPED�� 
        // �����ϸ� ���õ� �α׷��ڵ带 ������� �� ����. 
        // RESTART RECOVERY�� Commit Pending Operation�� �����Ͽ�
        // �� ���׸� �����Ѵ�. 
        // SCT_UPDATE_DRDB_DROP_TBS ������� �� ��쿡�� DROPPING
        // ���·� �����ϰ�, �ش� Ʈ������� COMMIT �α׸� ������� ��
        // Commit Pending Operation���� DROPPED ���·� �����Ѵ�. 

        if ( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPING)
             != SMI_TBS_DROPPING )
        {
            sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;
            
            // �˰��� (��), (��)�� �ش��ϴ� ��� Commit Pending ���� ���
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                            aTrans,
                                            sSpaceNode->mHeader.mID,
                                            ID_TRUE, /* commit�ÿ� ���� */
                                            SCT_POP_DROP_TBS,
                                            &sPendingOp)
                      != IDE_SUCCESS );

            // Pending �Լ� ����.
            // ó�� : Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
            sPendingOp->mPendingOpFunc = svmTBSDrop::dropTableSpacePending;

            /* Volatile TBS drop������ touch mode�� ������� �ʴ´�. */
            sPendingOp->mTouchMode     = SMI_ALL_NOTOUCH;
        }
        else
        {
            // nothing to do ..
        }
    }
    else
    {
        // �˰��� (��)�� �ش��ϴ� ��� TBS List���� �˻��� ���� ������
        // ��������� �ʴ´�. 
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Update type : SCT_UPDATE_VRDB_DROP_TBS
 *
 * Volatile tablespace ���ſ� ���� undo ����
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::undo_SCT_UPDATE_DRDB_DROP_TBS �� �ּ� ����
 * 
 * before image : tablespace attribute
 */
IDE_RC svmUpdate::undo_SCT_UPDATE_VRDB_DROP_TBS(
                    idvSQL        * aStatistics,
                    void          * /* aTrans */,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /* aFileID */,
                    UInt            aValueSize ,
                    SChar         * /* aValuePtr */,
                    idBool          /* aIsRestart */  )
{
    UInt          sState = 0;
    svmTBSNode *  sSpaceNode;

    IDE_ERROR( aValueSize == 0 );

    sSpaceNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sSpaceNode != NULL )
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        if ( SMI_TBS_IS_DROPPING( sSpaceNode->mHeader.mState ) )
        {
            // �˰��� RESTART (��), RUNTIME (��) �� �ش��ϴ� ����̴�. 
            // DROPPING�� ����, ONLINE ���·� �����Ѵ�. 
            sSpaceNode->mHeader.mState &= ~SMI_TBS_DROPPING;
        }
        
        IDE_ERROR( SMI_TBS_IS_ONLINE(sSpaceNode->mHeader.mState) );
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_CREATING) 
                   != SMI_TBS_CREATING );

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // TBS List���� �˻��� ���� ������ �̹� Drop�� Tablespace�̴�.
        // �ƹ��͵� ���� �ʴ´�.
        // nothing to do...
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}
/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� Log Image�� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ�� 
    [IN]  aValuePtr      - Log Image
    [OUT] aAutoExtMode   - Auto extent mode
    [OUT] aNextPageCount - Next page count
    [OUT] aMaxPageCount  - Max page count
 */
IDE_RC svmUpdate::getAlterAutoExtendImage( UInt       aValueSize,
                                           SChar    * aValuePtr,
                                           idBool   * aAutoExtMode,
                                           scPageID * aNextPageCount,
                                           scPageID * aMaxPageCount )
{
    IDE_ERROR( aValuePtr != NULL );
    IDE_ERROR( aValueSize == (UInt)( ID_SIZEOF(*aAutoExtMode) +
                                      ID_SIZEOF(*aNextPageCount) +
                                      ID_SIZEOF(*aMaxPageCount) ) );
    IDE_ERROR( aAutoExtMode   != NULL );
    IDE_ERROR( aNextPageCount != NULL );
    IDE_ERROR( aMaxPageCount  != NULL );
    
    idlOS::memcpy(aAutoExtMode, aValuePtr, ID_SIZEOF(*aAutoExtMode));
    aValuePtr += ID_SIZEOF(*aAutoExtMode);

    idlOS::memcpy(aNextPageCount, aValuePtr, ID_SIZEOF(*aNextPageCount));
    aValuePtr += ID_SIZEOF(*aNextPageCount);

    idlOS::memcpy(aMaxPageCount, aValuePtr, ID_SIZEOF(*aMaxPageCount));
    aValuePtr += ID_SIZEOF(*aMaxPageCount);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� REDO ����

    [ �α� ���� ]
    After Image   --------------------------------------------
      idBool              aAIsAutoExtend
      scPageID            aANextPageCount
      scPageID            aAMaxPageCount 
    
    [ ALTER_TBS_AUTO_EXTEND �� REDO ó�� ]
      (r-010) TBSNode.AutoExtend := AfterImage.AutoExtend
      (r-020) TBSNode.NextSize   := AfterImage.NextSize
      (r-030) TBSNode.MaxSize    := AfterImage.MaxSize
*/
IDE_RC svmUpdate::redo_SCT_UPDATE_VRDB_ALTER_AUTOEXTEND(
                    idvSQL        * /*aStatistics*/,
                    void          * /*aTrans*/,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          /* aIsRestart */ )
{
    svmTBSNode       * sTBSNode;
    idBool             sAutoExtMode;
    scPageID           sNextPageCount;
    scPageID           sMaxPageCount;

    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAutoExtendImage ���� �ǽ�.
    IDE_TEST( getAlterAutoExtendImage( aValueSize,
                                       aValuePtr,
                                       & sAutoExtMode,
                                       & sNextPageCount,
                                       & sMaxPageCount ) != IDE_SUCCESS );

    sTBSNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sTBSNode != NULL )
    {
        sTBSNode->mTBSAttr.mVolAttr.mIsAutoExtend  = sAutoExtMode;
        sTBSNode->mTBSAttr.mVolAttr.mNextPageCount = sNextPageCount;
        sTBSNode->mTBSAttr.mVolAttr.mMaxPageCount  = sMaxPageCount;
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� undo ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      idBool              aBIsAutoExtend
      scPageID            aBNextPageCount
      scPageID            aBMaxPageCount
      
    [ ALTER_TBS_AUTO_EXTEND �� UNDO ó�� ]
      (u-010) �α�ǽ� -> CLR ( ALTER_TBS_AUTO_EXTEND )
      (u-020) TBSNode.AutoExtend := BeforeImage.AutoExtend
      (u-030) TBSNode.NextSize   := BeforeImage.NextSize
      (u-040) TBSNode.MaxSize    := BeforeImage.MaxSize
*/
IDE_RC svmUpdate::undo_SCT_UPDATE_VRDB_ALTER_AUTOEXTEND(
                    idvSQL        * /*aStatistics*/,
                    void          * aTrans,
                    smLSN           /* sCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          aIsRestart )
{
    svmTBSNode       * sTBSNode;
    idBool             sAutoExtMode;
    scPageID           sNextPageCount;
    scPageID           sMaxPageCount;
    

    // BUGBUG-1548 �� Restart���߿��� aTrans == NULL�� �� �ִ���?
    IDE_ERROR( (aTrans != NULL) || (aIsRestart == ID_TRUE) );


    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAutoExtendImage ���� �ǽ�.
    IDE_TEST( getAlterAutoExtendImage( aValueSize,
                                       aValuePtr,
                                       & sAutoExtMode,
                                       & sNextPageCount,
                                       & sMaxPageCount ) != IDE_SUCCESS );

    sTBSNode = (svmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        sTBSNode->mTBSAttr.mVolAttr.mIsAutoExtend  = sAutoExtMode;
        sTBSNode->mTBSAttr.mVolAttr.mNextPageCount = sNextPageCount;
        sTBSNode->mTBSAttr.mVolAttr.mMaxPageCount  = sMaxPageCount;
        
        if ( aIsRestart == ID_FALSE )
        {
            // Log Anchor�� flush.
            IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( (sctTableSpaceNode*)sTBSNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // RESTART�ÿ��� Loganchor�� flush���� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
