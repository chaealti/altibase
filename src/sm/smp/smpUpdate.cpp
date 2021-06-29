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
 * $Id: smpUpdate.cpp 86110 2019-09-02 04:52:04Z et16 $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smpDef.h>
#include <smpFixedPageList.h>
#include <smpVarPageList.h>
#include <smpFreePageList.h>
#include <smpUpdate.h>
#include <sctTableSpaceMgr.h>
#include <smmTBSMultiPhase.h>
#include <smpTBSAlterOnOff.h>
#include <smpReq.h>

/* Update type:  SMR_SMM_PERS_UPDATE_LINK  */
/* before image: Prev PageID | Next PageID */
/* after  image: Prev PageID | Next PageID */
IDE_RC smpUpdate::redo_undo_SMM_PERS_UPDATE_LINK(smTID       /*a_tid*/,
                                                 scSpaceID     aSpaceID,
                                                 scPageID      a_pid,
                                                 scOffset    /*a_offset*/,
                                                 vULong      /*a_data*/,
                                                 SChar        *a_pImage,
                                                 SInt        /*a_nSize*/,
                                                 UInt        /*aFlag*/ )
{

    smpPersPage *s_pPersPage;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            a_pid,
                                            (void**)&s_pPersPage )
                == IDE_SUCCESS );

    s_pPersPage->mHeader.mSelfPageID = a_pid;

    idlOS::memcpy(&(s_pPersPage->mHeader.mPrevPageID),
                  a_pImage,
                  ID_SIZEOF(scPageID));

    a_pImage += ID_SIZEOF(scPageID);

    idlOS::memcpy(&(s_pPersPage->mHeader.mNextPageID),
                  a_pImage,
                  ID_SIZEOF(scPageID));


    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, a_pid) != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/* Update type: SMR_SMC_PERS_INIT_FIXED_PAGE           */
/* Redo Only                                           */
/* After Image: SlotSize | SlotCount | AllocPageListID */
IDE_RC smpUpdate::redo_SMC_PERS_INIT_FIXED_PAGE(smTID        /*a_tid*/,
                                                scSpaceID      aSpaceID,
                                                scPageID       a_pid,
                                                scOffset     /*a_offset*/,
                                                vULong       /*a_data*/,
                                                SChar         *a_pImage,
                                                SInt         /*a_nSize*/,
                                                UInt        /*aFlag*/ )
{
    smpPersPage    *s_pPersPage;
    vULong          s_slotSize;
    vULong          s_cSlot;
    UInt            sPageListID;
    smOID           sTableOID;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            a_pid,
                                            (void**)&s_pPersPage )
                == IDE_SUCCESS );

    /* BUG-15710: Redo�ÿ� Ư�������� ����Ÿ�� �ùٸ��� ASSERT�� �ɰ� �ֽ��ϴ�.
       Redo�ÿ� �ش絥��Ÿ�� redo�ϱ���������  Valid�ϴٴ� ���� �������� ���մϴ�.
       IDE_ASSERT( s_pPersPage->mHeader.mSelfPageID == a_pid );
    */

    idlOS::memcpy(&s_slotSize, a_pImage, ID_SIZEOF(vULong));
    a_pImage  += ID_SIZEOF(vULong);
    idlOS::memcpy(&s_cSlot, a_pImage, ID_SIZEOF(vULong));
    a_pImage  += ID_SIZEOF(vULong);
    idlOS::memcpy(&sPageListID, a_pImage, ID_SIZEOF(UInt));
    a_pImage  += ID_SIZEOF(UInt);
    idlOS::memcpy(&sTableOID, a_pImage, ID_SIZEOF(smOID));

    smpFixedPageList::initializePage(s_slotSize,
                                     s_cSlot,
                                     sPageListID,
                                     sTableOID,
                                     s_pPersPage );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, a_pid) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Update type: SMR_SMC_PERS_INIT_VAR_PAGE                      */
/* Redo Only                                                    */
/* After Image: VarIdx | SlotSize | SlotCount | AllocPageListID */
IDE_RC smpUpdate::redo_SMC_PERS_INIT_VAR_PAGE(smTID       /*a_tid*/,
                                              scSpaceID     aSpaceID,
                                              scPageID      a_pid,
                                              scOffset    /*a_offset*/,
                                              vULong      /*a_data*/,
                                              SChar       * a_pAfterImage,
                                              SInt        /*a_nSize*/,
                                              UInt        /*aFlag*/ )
{

    smpPersPage    *s_pPersPage;
    vULong          s_slotSize;
    vULong          s_cSlot;
    vULong          s_idx;
    UInt            sPageListID;
    smOID           sTableOID;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            a_pid,
                                            (void**)&s_pPersPage )
                == IDE_SUCCESS );

    /* BUG-15710: Redo�ÿ� Ư�������� ����Ÿ�� �ùٸ��� ASSERT�� �ɰ� �ֽ��ϴ�.
       Redo�ÿ� �ش絥��Ÿ�� redo�ϱ���������  Valid�ϴٴ� ���� �������� ���մϴ�.
       IDE_ASSERT( s_pPersPage->mHeader.mSelfPageID == a_pid );
    */

    idlOS::memcpy(&s_idx, a_pAfterImage, ID_SIZEOF(vULong));
    a_pAfterImage  += ID_SIZEOF(vULong);
    idlOS::memcpy(&s_slotSize, a_pAfterImage, ID_SIZEOF(vULong));
    a_pAfterImage  += ID_SIZEOF(vULong);
    idlOS::memcpy(&s_cSlot, a_pAfterImage, ID_SIZEOF(vULong));
    a_pAfterImage  += ID_SIZEOF(vULong);
    idlOS::memcpy(&sPageListID, a_pAfterImage, ID_SIZEOF(UInt));
    a_pAfterImage  += ID_SIZEOF(UInt);
    idlOS::memcpy(&sTableOID, a_pAfterImage, ID_SIZEOF(smOID));

    smpVarPageList::initializePage(s_idx,
                                   sPageListID,
                                   s_slotSize,
                                   s_cSlot,
                                   sTableOID,
                                   s_pPersPage );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, a_pid) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smpUpdate::redo_SMP_NTA_ALLOC_FIXED_ROW( scSpaceID    aSpaceID,
                                                scPageID     aPageID,
                                                scOffset     aOffset,
                                                idBool       aIsSetDeleteBit )
{
    smpSlotHeader     *sSlotHeader;
    smSCN              sSCN;
    smOID              sOID;

    sOID = SM_MAKE_OID(aPageID, aOffset);
    IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                       sOID,
                                       (void**)&sSlotHeader )
                == IDE_SUCCESS );

    SM_INIT_SCN(&sSCN);

    if(aIsSetDeleteBit == ID_TRUE)
    {
        SM_SET_SCN_DELETE_BIT( &sSCN );
    }

    SMP_SLOT_SET_OFFSET( sSlotHeader, aOffset );
    sSlotHeader->mCreateSCN = sSCN;
    SM_SET_SCN_FREE_ROW( &(sSlotHeader->mLimitSCN) );
    SMP_SLOT_INIT_POSITION( sSlotHeader );
    SMP_SLOT_SET_USED( sSlotHeader );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID, aPageID) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    ALTER TABLESPACE TBS1 ONLINE/OFFLINE .... �� ���� Log Image�� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ��
    [IN]  aValuePtr      - Log Image
    [OUT] aState         - Tablespace�� ����
 */
IDE_RC smpUpdate::getAlterTBSOnOffImage( UInt       aValueSize,
                                         SChar    * aValuePtr,
                                         UInt     * aState )
{
    IDE_DASSERT( aValuePtr != NULL );

    IDE_ASSERT( aValueSize == (UInt)( ID_SIZEOF(*aState)));

    IDE_DASSERT( aState   != NULL );

    idlOS::memcpy(aState, aValuePtr, ID_SIZEOF(*aState));
    aValuePtr += ID_SIZEOF(*aState);

    return IDE_SUCCESS;
}



/*
    ALTER TABLESPACE TBS1 OFFLINE .... �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ ALTER TABLESPACE OFFLINE �� REDO ó�� ]
      (u-010) (020)�� ���� REDO�� TBSNode.Status := After Image(OFFLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
      (note-2) Commit Pending�� �������� ����
               -> Restart Recovery�Ϸ��� OFFLINE TBS�� ���� Resource������ �Ѵ�
*/
IDE_RC smpUpdate::redo_SCT_UPDATE_MRDB_ALTER_TBS_OFFLINE(
                    idvSQL        * /* aStatistics */,
                    void          * aTrans,
                    smLSN           /* aCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          /*aIsRestart*/ )
{
    UInt           sTBSState;
    smmTBSNode   * sTBSNode;
    sctPendingOp * sPendingOp;

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        // Tablespace�� ���¸� ��� ������ ���,
        // Tablespace�� OFFLINE���� ����� ���߿�
        // �߻��� Log���� Redo���� ���ϰ� �ȴ�.
        // ( Offline Tablespace�� ���ؼ��� Redo�� ���� ���� )
        //
        // => Transaction Commit�ÿ� ������ Pending Operation���
        IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                      aTrans,
                      aSpaceID,
                      ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                      SCT_POP_ALTER_TBS_OFFLINE,
                      & sPendingOp )
                  != IDE_SUCCESS );

        // Commit�� sctTableSpaceMgr::executePendingOperation����
        // ������ Pending�Լ� ����
        sPendingOp->mPendingOpFunc = smpTBSAlterOnOff::alterOfflineCommitPending;
        sPendingOp->mNewTBSState   = sTBSState;

        // Pending�Լ����� ����üũ�� ASSERT�����ʰ� �ϱ� ����
        sTBSNode->mHeader.mState |= SMI_TBS_SWITCHING_TO_OFFLINE ;

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
    ALTER TABLESPACE TBS1 OFFLINE .... �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE OFFLINE �� UNDO ó�� ]
      (u-010) (020)�� ���� UNDO�� TBSNode.Status := Before Image(ONLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS OFFLINE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
*/
IDE_RC smpUpdate::undo_SCT_UPDATE_MRDB_ALTER_TBS_OFFLINE(
                    idvSQL        * /*aStatistics*/,
                    void          * /*aTrans*/,
                    smLSN           /* aCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          /*aIsRestart*/ )
{
    UInt          sTBSState;
    smmTBSNode  * sTBSNode;

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        sTBSNode->mHeader.mState = sTBSState;
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
    ALTER TABLESPACE TBS1 ONLINE .... �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ ALTER TABLESPACE ONLINE �� REDO ó�� ]
      (r-010) TBSNode.Status := After Image(ONLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����

*/
IDE_RC smpUpdate::redo_SCT_UPDATE_MRDB_ALTER_TBS_ONLINE(
                    idvSQL        * /*aStatistics*/,
                    void          * /*aTrans*/,
                    smLSN           /* aCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          /*aIsRestart*/ )
{
    UInt          sTBSState;
    smmTBSNode  * sTBSNode;

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        if ( SMI_TBS_IS_ONLINE(sTBSNode->mHeader.mState) )
        {
            // �̹� Online���¶��, Page�ܰ���� �ʱ�ȭ�ǰ�
            // Restore�� �Ϸ�� �����̴�.
            //
            // �ƹ��� ó���� �ʿ����� �ʴ�.
        }
        else // Online���°� �ƴ϶��
        {
            if ( sTBSNode->mRestoreType ==
                 SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
            {
                // PAGE �ܰ���� �ʱ�ȭ�� ��Ű��
                // Restore�� �ǽ��Ѵ�.
                IDE_TEST( smmTBSMultiPhase::initPagePhase( sTBSNode )
                          != IDE_SUCCESS );
                IDE_TEST( smmManager::prepareAndRestore( sTBSNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // 1. ONLINE�� ä�� ���� �⵿
                //      => Page�ܰ���� �ʱ�ȭ, Restore�ǽ�
                // 2. REDO���� ALTER OFFLINE�α׸� ������ OFFLINE���� ����
                //      => ���¸� OFFLINE���� �ٲٰ�,
                //         Tablespace�� �״�� ��
                // 3. REDO���� ALTER ONLINE�α� ����
                //      => 1���� Restore���� ��� ��ģ ����
                //         ���⿡���� �ƹ��� ó���� �������� ����
            }

        }

        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        // Online���·� ����
        sTBSNode->mHeader.mState = sTBSState;

        IDE_ERROR( SMI_TBS_IS_ONLINE(sTBSState) );
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
    ALTER TABLESPACE TBS1 ONLINE .... �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE ONLINE �� UNDO ó�� ]

      (u-050)  TBSNode.Status := Before Image(OFFLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS ONLINE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����


*/
IDE_RC smpUpdate::undo_SCT_UPDATE_MRDB_ALTER_TBS_ONLINE(
                    idvSQL        * aStatistics,
                    void          * /*aTrans*/,
                    smLSN           /* aCurLSN */,
                    scSpaceID       aSpaceID,
                    UInt            /*aFileID*/,
                    UInt            aValueSize,
                    SChar         * aValuePtr,
                    idBool          aIsRestart )
{
    UInt          sTBSState;
    smmTBSNode  * sTBSNode;

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        if ( aIsRestart == ID_TRUE )
        {
            // Restart Recovery�߿���
            // Tablespace�� ���¸� �����ϰ�
            // Resource�� �״�� �д�.
            //
            // Restart Recovery�Ϸ��Ŀ� �ϰ�ó���Ѵ�.
        }
        else
        {
            // Alter Tablespace Online ���൵�� �����߻� ���� üũ
            if ( ( sTBSNode->mHeader.mState & SMI_TBS_SWITCHING_TO_ONLINE )
                 == SMI_TBS_SWITCHING_TO_ONLINE )
            {
                // Alter Tablespace Online ������ �Ϸ��Ͽ�����,
                // ���Ŀ� Transaction�� ABORT�� ���

                //////////////////////////////////////////////////////////
                //  Free All Index Memory of TBS
                //  Destroy/Free Runtime Info At Table Header
                IDE_TEST( smLayerCallback::alterTBSOffline4Tables(
                              aStatistics,
                              aSpaceID )
                          != IDE_SUCCESS );


                //////////////////////////////////////////////////////////
                // Page�ܰ踦 �����Ѵ�.
                IDE_TEST( smmTBSMultiPhase::finiPagePhase( sTBSNode )
                          != IDE_SUCCESS );

            }
            else
            {
                // Alter Tablespace Online ������ ���� �߻��Ͽ�
                // ABORT�� ���

                // Do Nothing
                // => Alter Tablespace Online ����� ����ó�� ��ƾ����
                //    ���� �۾��� �̹� �� ó���� �����̴�.
            }
        }


        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        sTBSNode->mHeader.mState = sTBSState;
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

/* Update type: SMR_SMC_PERS_SET_INCONSISTENCY         */
IDE_RC smpUpdate::redo_undo_SMC_PERS_SET_INCONSISTENCY(
    smTID        /*aTid*/,
    scSpaceID      aSpaceID,
    scPageID       aPid,
    scOffset     /*aOffset*/,
    vULong       /*aData*/,
    SChar         *aImage,
    SInt         /*aSize*/,
    UInt         /*aFlag*/ )
{
    smpPersPage    * sPersPage;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            aPid,
                                            (void**)&sPersPage )
                == IDE_SUCCESS );
    idlOS::memcpy( &sPersPage->mHeader.mType,
                   aImage,
                   ID_SIZEOF( smpPageType ) );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, aPid) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


