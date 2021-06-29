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
* $Id: smmTBSCreate.cpp 19201 2006-11-30 00:54:40Z kmkim $
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
#include <smmTBSCreate.h>
#include <smmTBSMultiPhase.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSCreate::smmTBSCreate()
{

}


/*
     Tablespace Attribute�� �ʱ�ȭ �Ѵ�.

     [IN] aTBSAttr        - �ʱ�ȭ �� Tablespace Attribute
     [IN] aSpaceID        - ���� ������ Tablespace�� ����� Space ID
     [IN] aType           - Tablespace ����
                            User||System, Memory, Data || Temp
     [IN] aName           - Tablespace�� �̸�
     [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
     [IN] aSplitFileSize  - Checkpoint Image File�� ũ��
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��

     [ �˰��� ]
       (010) �⺻ �ʵ� �ʱ�ȭ
       (020) mSplitFilePageCount �ʱ�ȭ
       (030) mInitPageCount �ʱ�ȭ

     [ ����ó�� ]
       (e-010) aSplitFileSize �� Expand Chunkũ���� ����� �ƴϸ� ����
       (e-020) aInitSize �� Expand Chunkũ���� ����� �ƴϸ� ����

*/
IDE_RC smmTBSCreate::initializeTBSAttr( smiTableSpaceAttr    * aTBSAttr,
                                         scSpaceID              aSpaceID,
                                         smiTableSpaceType      aType,
                                         SChar                * aName,
                                         UInt                   aAttrFlag,
                                         ULong                  aSplitFileSize,
                                         ULong                  aInitSize)
{
    ULong sChunkSize = smuProperty::getExpandChunkPageCount() * SM_PAGE_SIZE;

    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aName != NULL );

    // checkErrorOfTBSAttr ���� �̹� üũ�� ������̹Ƿ� ASSERT�� �˻�
    IDE_ASSERT ( ( aSplitFileSize % sChunkSize ) == 0 );
    IDE_ASSERT ( ( aInitSize % sChunkSize ) == 0 );

    IDE_ASSERT( sChunkSize > 0 );


    idlOS::memset( aTBSAttr, 0, ID_SIZEOF( *aTBSAttr ) );

    aTBSAttr->mMemAttr.mShmKey             = 0;
    aTBSAttr->mMemAttr.mCurrentDB          = 0;

    aTBSAttr->mAttrType                    = SMI_TBS_ATTR;
    aTBSAttr->mID                          = aSpaceID;
    aTBSAttr->mType                        = aType;
    aTBSAttr->mAttrFlag                    = aAttrFlag;
    aTBSAttr->mMemAttr.mSplitFilePageCount = aSplitFileSize / SM_PAGE_SIZE ;
    aTBSAttr->mMemAttr.mInitPageCount      = aInitSize / SM_PAGE_SIZE;
    // Membase�� ������ Page��ŭ �߰�
    aTBSAttr->mMemAttr.mInitPageCount     += SMM_DATABASE_META_PAGE_CNT ;

    idlOS::strncpy( aTBSAttr->mName,
                    aName,
                    SMI_MAX_TABLESPACE_NAME_LEN );
    aTBSAttr->mName[SMI_MAX_TABLESPACE_NAME_LEN] = '\0';

    // BUGBUG-1548 Refactoring mName�� '\0'���� ������ ���ڿ��̹Ƿ�
    //             mNameLength�� �ʿ䰡 ����. �����Ұ�.
    aTBSAttr->mNameLength                  = idlOS::strlen( aName );

    return IDE_SUCCESS;
}

/*
    ����ڰ� Tablespace������ ���� �ɼ��� �������� ���� ���
    �⺻���� �����ϴ� �Լ�

     [IN/OUT] aSplitFileSize  - Checkpoint Image File�� ũ��
     [IN/OUT] aInitSize       - Tablespace�� �ʱ�ũ��
 */
IDE_RC smmTBSCreate::makeDefaultArguments( ULong  * aSplitFileSize,
                                           ULong  * aInitSize)
{
    ULong sChunkSize = smuProperty::getExpandChunkPageCount() * SM_PAGE_SIZE;

    // ����ڰ� Checkpoint Image Splitũ��(DB Fileũ��)��
    // �������� ���� ���
    if ( *aSplitFileSize == 0 )
    {
        // ������Ƽ�� ������ �⺻ DB Fileũ��� ����
        *aSplitFileSize = smuProperty::getDefaultMemDBFileSize();
    }

    // ����ڰ� Tablespace�ʱ� ũ�⸦ �������� ���� ���
    if ( *aInitSize == 0 )
    {
        // �ּ� Ȯ������� Chunkũ�⸸ŭ ����
        *aInitSize = sChunkSize;
    }

    return IDE_SUCCESS;
}


/*
    Tablespace Attribute�� ���� ����üũ�� �ǽ��Ѵ�.

     [IN] aName           - Tablespace�� �̸�
     [IN] aSplitFileSize  - Checkpoint Image File�� ũ��
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��

 */
IDE_RC smmTBSCreate::checkErrorOfTBSAttr( SChar * aTBSName,
                                          ULong   aSplitFileSize,
                                          ULong   aInitSize)
{
    ULong sChunkSize = smuProperty::getExpandChunkPageCount() * SM_PAGE_SIZE;

    IDE_TEST_RAISE(idlOS::strlen(aTBSName) > (SM_MAX_DB_NAME -1),
                   too_long_tbsname);

    IDE_TEST_RAISE ( ( aSplitFileSize % sChunkSize ) != 0,
                     error_db_file_split_size_not_aligned_to_chunk_size );

    IDE_TEST( smuProperty::checkFileSizeLimit( "the split size of checkpoint image",
                                               aSplitFileSize )
              != IDE_SUCCESS );

    IDE_TEST_RAISE ( ( aInitSize % sChunkSize ) != 0,
                     error_tbs_init_size_not_aligned_to_chunk_size );

    return IDE_SUCCESS;

    IDE_EXCEPTION(too_long_tbsname);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_TooLongTBSName,
                                  (UInt)SM_MAX_DB_NAME ) );
    }
    IDE_EXCEPTION( error_tbs_init_size_not_aligned_to_chunk_size );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TBSInitSizeNotAlignedToChunkSize,
                                // KB ������ Expand Chunk Page ũ��
                                ( sChunkSize / 1024 )));
    }
    IDE_EXCEPTION( error_db_file_split_size_not_aligned_to_chunk_size );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SplitSizeNotAlignedToChunkSize,
                                // KB ������ Expand Chunk Page ũ��
                                ( sChunkSize / 1024 )));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ���ο�  Tablespace�� ����� Tablespace ID�� �Ҵ�޴´�.

    [OUT] aSpaceID - �Ҵ���� Tablespace�� ID
 */
IDE_RC smmTBSCreate::allocNewTBSID( scSpaceID * aSpaceID )
{
    UInt  sStage = 0;

    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( sctTableSpaceMgr::allocNewTableSpaceID( aSpaceID )
              != IDE_SUCCESS );

    sStage = 0;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1 :
            IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
            break;
        default:
            break;
    }

    IDE_POP();


    return IDE_FAILURE;
}


/*
 * ����ڰ� ������, Ȥ�� �ý����� �⺻ Checkpoint Path��
 * Tablespace�� �߰��Ѵ�.
 *
 * aTBSNode           [IN] - Checkpoint Path�� �߰��� Tabelspace�� Node
 * aChkptPathAttrList [IN] - �߰��� Checkpoint Path�� List
 */
IDE_RC smmTBSCreate::createDefaultOrUserChkptPaths(
                         smmTBSNode           * aTBSNode,
                         smiChkptPathAttrList * aChkptPathAttrList )
{
    UInt                  i;
    UInt                  sChkptPathCount;
    UInt                  sChkptPathSize;
    smiChkptPathAttrList  sCPAttrLists[ SM_DB_DIR_MAX_COUNT + 1];
    smiChkptPathAttrList *sCPAttrList;

    IDE_DASSERT( aTBSNode != NULL );
    // ����ڰ� Checkpoint Path�� �������� ���� ���
    // aChkptPathAttrList�� NULL�̴�.

    // ����ڰ� Checkpoint Path�� �������� ���� ���
    if ( aChkptPathAttrList == NULL )
    {
        // ����ڰ� �����ص� �ϳ� �̻��� DB_DIR ������Ƽ�� �о
        // Checkpoint Path Attribute���� List�� �����Ѵ�.
        sChkptPathCount = smuProperty::getDBDirCount();

        for(i = 0; i < sChkptPathCount; i++)
        {
            sCPAttrLists[i].mCPathAttr.mAttrType = SMI_CHKPTPATH_ATTR;

            IDE_TEST( smmTBSChkptPath::setChkptPath(
                          & sCPAttrLists[i].mCPathAttr,
                          (SChar*)smuProperty::getDBDir(i) )
                      != IDE_SUCCESS );

            sCPAttrLists[i].mNext = & sCPAttrLists[i+1] ;
        }

        sCPAttrLists[ sChkptPathCount - 1 ].mNext = NULL ;
        aChkptPathAttrList = & sCPAttrLists[0];
    }
    // BUG-29812
    // ����ڰ� Checkpoint Path�� ������ ���
    // �������̸� �״�� ����ϰ�,
    // ������̸� �����η� �����Ͽ� ����Ѵ�.
    else
    {
        sCPAttrList = aChkptPathAttrList;

        while( sCPAttrList != NULL )
        {
            // BUG-29812
            // Memory TBS�� Checkpoint Path�� �����η� ��ȯ�Ѵ�.
            sChkptPathSize = idlOS::strlen(sCPAttrList->mCPathAttr.mChkptPath);

            IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                        ID_FALSE,
                                        sCPAttrList->mCPathAttr.mChkptPath,
                                        &sChkptPathSize,
                                        SMI_TBS_MEMORY )
                      != IDE_SUCCESS);

            sCPAttrList = sCPAttrList->mNext;
        }
    }

    IDE_TEST( createChkptPathNodes( aTBSNode, aChkptPathAttrList )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * �ϳ� Ȥ�� �� �̻��� Checkpoint Path�� TBSNode�� Tail�� �߰��Ѵ�.
 *
 * aTBSNode       [IN] - Checkpoint Path�� �߰��� Tabelspace�� Node
 * aChkptPathList [IN] - �߰��� Checkpoint Path�� List
 */
IDE_RC smmTBSCreate::createChkptPathNodes( smmTBSNode           * aTBSNode,
                                           smiChkptPathAttrList * aChkptPathList )
{

    SChar                * sChkptPath;
    smiChkptPathAttrList * sCPAttrList;
    smmChkptPathNode     * sCPathNode = NULL;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aChkptPathList != NULL );


    for ( sCPAttrList = aChkptPathList;
          sCPAttrList != NULL;
          sCPAttrList = sCPAttrList->mNext )
    {
        sChkptPath = sCPAttrList->mCPathAttr.mChkptPath;

        //////////////////////////////////////////////////////////////////
        // (e-010) NewPath�� ���� ���丮�̸� ����
        // (e-020) NewPath�� ���丮�� �ƴ� �����̸� ����
        // (e-030) NewPath�� read/write/execute permission�� ������ ����
        IDE_TEST( smmTBSChkptPath::checkAccess2ChkptPath( sChkptPath )
                  != IDE_SUCCESS );

        //////////////////////////////////////////////////////////////////
        // (e-040) �̹� Tablespace�� �����ϴ� Checkpoint Path�� ������ ����
        IDE_TEST( smmTBSChkptPath::findChkptPathNode( aTBSNode,
                                                      sChkptPath,
                                                      & sCPathNode )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sCPathNode != NULL, err_chkpt_path_already_exists );

        //////////////////////////////////////////////////////////////////
        // (010) CheckpointPathNode �Ҵ�
        IDE_TEST( smmTBSChkptPath::makeChkptPathNode(
                      aTBSNode->mTBSAttr.mID,
                      sChkptPath,
                      & sCPathNode ) != IDE_SUCCESS );

        // (020) TBSNode �� CheckpointPathNode �߰�
        IDE_TEST( smmTBSChkptPath::addChkptPathNode( aTBSNode,
                                                     sCPathNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_chkpt_path_already_exists );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_ALREADY_EXISTS,
                                sChkptPath));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    // aChkptPathList�� �����ϴ� ��� Checkpoint Path Node�� �����Ѵ�.
    IDE_ASSERT( smmTBSChkptPath::removeChkptPathNodesIfExist(
                    aTBSNode, aChkptPathList )
                == IDE_SUCCESS );

    IDE_POP();

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : tablespace ���� �� �ʱ�ȭ
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 *****************************************************************************/
IDE_RC smmTBSCreate::createTBS4Redo( void                 * aTrans,
                                     smiTableSpaceAttr    * aTBSAttr,
                                     smiChkptPathAttrList * aChkptPathList )
{
    scSpaceID       sSpaceID;
    sctPendingOp  * sPendingOp;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aTBSAttr   != NULL );
    // aChkptPathList�� ����ڰ� �������� ���� ��� NULL�̴�.
    // aSplitFileSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aInitSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aNextSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aMaxSize�� ����ڰ� �������� ���� ��� 0�̴�.

    /*******************************************************************/
    // Tablespace ���� �ǽ�

    // Tablespace�� ID�Ҵ� =======================================
    sSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

    IDE_TEST( sSpaceID != aTBSAttr->mID );

    IDE_TEST( allocNewTBSID( & sSpaceID ) != IDE_SUCCESS );

    // ���� Tablespace���� ���� ==================================
    // Tablespace�� �� ���� Checkpoint Path�� �����Ѵ�.
    IDE_TEST( createTBSWithNTA4Redo( aTrans,
                                     aTBSAttr,
                                     aChkptPathList )
              != IDE_SUCCESS );

    // Commit�� ������ Pending Operation���  ======================
    // Commit�� ���� : Tablespace���¿��� SMI_TBS_CREATING�� ����
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
            aTrans,
            aTBSAttr->mID,
            ID_TRUE,
            SCT_POP_CREATE_TBS,
            & sPendingOp)
        != IDE_SUCCESS );
    sPendingOp->mPendingOpFunc = smmTBSCreate::createTableSpacePending;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : 
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 *****************************************************************************/
IDE_RC smmTBSCreate::createTBSWithNTA4Redo( void                  * aTrans,
                                            smiTableSpaceAttr     * aTBSAttr,
                                            smiChkptPathAttrList  * aChkptPathList )
{
    IDE_DASSERT( aTrans                 != NULL );
    IDE_DASSERT( aTBSAttr->mName[0]     != '\0' );
    IDE_DASSERT( aTBSAttr->mAttrType    == SMI_TBS_ATTR );
    // NO ASSERT : aChkptPathList�� ��� NULL�� �� �ִ�.

    idBool          sChkptBlocked   = ID_FALSE;
    idBool          sNodeCreated    = ID_FALSE;
    idBool          sNodeAdded      = ID_FALSE;

    smmTBSNode    * sTBSNode;
    sctLockHier     sLockHier;

    SCT_INIT_LOCKHIER(&sLockHier);

    // Tablespace Node�� �Ҵ�, �ٴܰ� �ʱ�ȭ�� �����Ѵ�.
    // - ó������
    //   - Tablespace Node�� �Ҵ�
    //   - Tablespace�� PAGE�ܰ���� �ʱ�ȭ�� ����
    IDE_TEST( allocAndInitTBSNode( aTrans,
                                   aTBSAttr,
                                   & sTBSNode )
              != IDE_SUCCESS );
    sNodeCreated = ID_TRUE;

    // TBSNode�� ���� Lock�� X�� ��´�. ( �Ϲ� DML,DDL�� ���� )
    //   - Lockȹ��� Lock Slot�� sLockHier->mTBSNodeSlot�� �Ѱ��ش�.
    //   - �� �Լ� ȣ������ �����ִ� Latch�� �־�� �ȵȴ�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                  aTrans,
                  & sTBSNode->mHeader,
                  ID_FALSE,   /* intent */
                  ID_TRUE,    /* exclusive */
                  sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aTrans),
                  SCT_VAL_DDL_DML,/* validation */
                  NULL,       /* is locked */
                  & sLockHier )      /* lockslot */
              != IDE_SUCCESS );

    // - Checkpoint�� Block��Ų��.
    //   - Checkpoint�� Dirty Page�� flush�ϴ� �۾��� ����
    //   - Checkpoint�� DB File Header�� flush�ϴ� �۾��� ����
    IDE_TEST( smLayerCallback::blockCheckpoint() != IDE_SUCCESS );
    sChkptBlocked=ID_TRUE;

    ///////////////////////////////////////////////////////////////
    // �α�ǽ� => SCT_UPDATE_MRDB_CREATE_TBS
    //  - redo�� : Tablespace���¿��� SMI_TBS_CREATING ���¸� ���ش�
    //  - undo�� : Tablespace���¸� SMI_TBS_DROPPED�� ����

    // Tablespace Node�� TBS Node List�� �߰��Ѵ�.
    IDE_TEST( registerTBS( sTBSNode ) != IDE_SUCCESS );
    sNodeAdded = ID_TRUE;

    IDE_TEST( createTBSInternal4Redo( aTrans,
                                      sTBSNode,
                                      aChkptPathList ) != IDE_SUCCESS );

    sChkptBlocked = ID_FALSE;
    IDE_TEST( smLayerCallback::unblockCheckpoint() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sChkptBlocked == ID_TRUE )
    {
        IDE_ASSERT( smLayerCallback::unblockCheckpoint() == IDE_SUCCESS );
    }

    if ( sNodeCreated == ID_TRUE )
    {
        if ( sNodeAdded == ID_TRUE )
        {
            // Tablespace Node�� �ý��ۿ� �߰��� ���
            // SCT_UPDATE_MRDB_CREATE_TBS �� Undo�����
            // Tablespace�� Phase�� STATE�ܰ���� �����ش�.
            // ���� �������� �ʴ´�.
        }
        else
        {
            // �����ִ� Lock�� �ִٸ� ����
            if ( sLockHier.mTBSNodeSlot != NULL )
            {
                IDE_ASSERT( smLayerCallback::unlockItem(
                                aTrans,
                                sLockHier.mTBSNodeSlot )
                            == IDE_SUCCESS );
            }

            // ��带 ����
            IDE_ASSERT( finiAndFreeTBSNode( sTBSNode ) == IDE_SUCCESS );
        }
    }

    IDE_POP();

    return IDE_FAILURE;
}

/******************************************************************************
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 *
 ******************************************************************************/
IDE_RC smmTBSCreate::createTBSInternal4Redo(
                          void                 * aTrans,
                          smmTBSNode           * aTBSNode,
                          smiChkptPathAttrList * aChkptPathAttrList )
{
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aTBSNode   != NULL );

    // NO ASSERT : aChkptPathAttrList�� ��� NULL�� �� �ִ�.

    ///////////////////////////////////////////////////////////////
    // (070) Checkpoint Path Node���� ����
    IDE_TEST( createDefaultOrUserChkptPaths( aTBSNode,
                                             aChkptPathAttrList )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (080) ���̺� �����̽��� ������ �޸𸮷� ����
    //       Page Pool�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( smmManager::initializePagePool( aTBSNode )
              != IDE_SUCCESS );

    // (100) initialize Membase on Page 0
    // (110) alloc ExpandChunks
    IDE_TEST( smmManager::createTBSPages4Redo(
                  aTBSNode,
                  aTrans,
                  smmDatabase::getDBName(), // aDBName,
                  aTBSNode->mTBSAttr.mMemAttr.mSplitFilePageCount,
                  aTBSNode->mTBSAttr.mMemAttr.mInitPageCount, // aInitPageCount,
                  smmDatabase::getDBCharSet(), // aDBCharSet,
                  smmDatabase::getNationalCharSet() // aNationalCharSet
                  )
              != IDE_SUCCESS );

    IDE_TEST( smmTBSCreate::flushTBSAndCPaths(aTBSNode) != IDE_SUCCESS );

    /* PROJ-2386 DR 
     * - ���⼭�� checkpoint file �� create LSN �� �˼������Ƿ� checkpoint file�� ������ �ʵ��� �Ѵ�.
     * - checkpoint file�� SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK �α� redo�� �����Ѵ�. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * �ý���/����� �޸� Tablespace�� �����Ѵ�.
 *
 * [IN] aTrans          - Tablespace�� �����Ϸ��� Transaction
 * [IN] aDBName         - Tablespace�� ������ Database�� �̸�
 * [IN] aType           - Tablespace ����
 *                  User||System, Memory, Data || Temp
 * [IN] aName           - Tablespace�� �̸�
 * [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
 * [IN] aChkptPathList  - Tablespace�� Checkpoint Image���� ������ Path��
 * [IN] aSplitFileSize  - Checkpoint Image File�� ũ��
 * [IN] aInitSize       - Tablespace�� �ʱ�ũ��
 *                  Meta Page(0�� Page)�� �������� �ʴ� ũ���̴�.
 * [IN] aIsAutoExtend   - Tablespace�� �ڵ�Ȯ�� ����
 * [IN] aNextSize       - Tablespace�� �ڵ�Ȯ�� ũ��
 * [IN] aMaxSize        - Tablespace�� �ִ�ũ��
 * [IN] aIsOnline       - Tablespace�� �ʱ� ���� (ONLINE�̸� ID_TRUE)
 * [IN] aDBCharSet      - �����ͺ��̽� ĳ���� ��(PROJ-1579 NCHAR)
 * [IN] aNationalCharSet- ���ų� ĳ���� ��(PROJ-1579 NCHAR)
 * [OUT] aTBSID         - ������ Tablespace�� ID
 *
 * - Latch�� Deadlockȸ�Ǹ� ���� Latch��� ����
 *   1. sctTableSpaceMgr::lock()           // TBS LIST
 *   2. sctTableSpaceMgr::latchSyncMutex() // TBS NODE
 *   3. smmPCH.mPageMemMutex.lock()        // PAGE
 *
 * - Lock�� Latch���� Deadlockȸ�Ǹ� ���� Lock/Latch��� ����
 *   1. Lock��  ���� ��´�.
 *   2. Latch�� ���߿� ��´�.
 *****************************************************************************/
IDE_RC smmTBSCreate::createTBS( void                 * aTrans,
                                SChar                * aDBName,
                                SChar                * aTBSName,
                                UInt                   aAttrFlag,
                                smiTableSpaceType      aType,
                                smiChkptPathAttrList * aChkptPathList,
                                ULong                  aSplitFileSize,
                                ULong                  aInitSize,
                                idBool                 aIsAutoExtend,
                                ULong                  aNextSize,
                                ULong                  aMaxSize,
                                idBool                 aIsOnline,
                                SChar                * aDBCharSet,
                                SChar                * aNationalCharSet,
                                scSpaceID            * aTBSID )
{
    scSpaceID            sSpaceID;
    smiTableSpaceAttr    sTBSAttr;
    smmTBSNode         * sCreatedTBSNode;
    sctPendingOp       * sPendingOp;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aDBName    != NULL );
    IDE_DASSERT( aTBSName   != NULL );
    // aChkptPathList�� ����ڰ� �������� ���� ��� NULL�̴�.
    // aSplitFileSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aInitSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aNextSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aMaxSize�� ����ڰ� �������� ���� ��� 0�̴�.

    /*******************************************************************/
    // Tablespace ������ ���� �غ�ܰ�
    {
        // ����ڰ� Tablespace������ ���� �ɼ��� �������� ���� ���
        // �⺻���� ����
        IDE_TEST( makeDefaultArguments( & aSplitFileSize,
                                        & aInitSize ) != IDE_SUCCESS );


        // ����ڰ� ������ Tablespace������ ���� ���뿡 ����
        // ����üũ �ǽ�
        IDE_TEST( checkErrorOfTBSAttr( aTBSName,
                                       aSplitFileSize,
                                       aInitSize) != IDE_SUCCESS );
    }

    /*******************************************************************/
    // Tablespace ���� �ǽ�
    {
        // Tablespace�� ID�Ҵ� =======================================
        IDE_TEST( allocNewTBSID( & sSpaceID ) != IDE_SUCCESS );

        // Tablespace������ ���� Attribute�ʱ�ȭ ====================
        IDE_TEST( initializeTBSAttr( & sTBSAttr,
                                     sSpaceID,
                                     aType,
                                     aTBSName,
                                     aAttrFlag,
                                     aSplitFileSize,
                                     aInitSize) != IDE_SUCCESS );


        // ���� Tablespace���� ���� ==================================
        // Tablespace�� �� ���� Checkpoint Path�� �����Ѵ�.
        IDE_TEST( createTBSWithNTA(aTrans,
                                   aDBName,
                                   & sTBSAttr,
                                   aChkptPathList,
                                   sTBSAttr.mMemAttr.mInitPageCount,
                                   aDBCharSet,
                                   aNationalCharSet,
                                   & sCreatedTBSNode)
                  != IDE_SUCCESS );


        IDE_TEST( smmTBSAlterAutoExtend::alterTBSsetAutoExtend(
                                                  aTrans,
                                                  sTBSAttr.mID,
                                                  aIsAutoExtend,
                                                  aNextSize,
                                                  aMaxSize ) != IDE_SUCCESS );

        IDU_FIT_POINT( "3.PROJ-1548@smmTBSCreate::createTBS" );

        // Offline���� =================================================
        // ������ ������ Tablespace�� �⺻������ Online���¸� ���Ѵ�.
        // ����ڰ� Tablespace�� Offline���� �����Ϸ��� ���
        // ���¸� Offline���� �ٲپ��ش�.
        if ( aIsOnline == ID_FALSE )
        {
            IDE_TEST( smLayerCallback::alterTBSStatus( aTrans,
                                                       sCreatedTBSNode,
                                                       SMI_TBS_OFFLINE )
                      != IDE_SUCCESS );
        }

        // Commit�� ������ Pending Operation���  ======================
        // Commit�� ���� : Tablespace���¿��� SMI_TBS_CREATING�� ����
        IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                  aTrans,
                                                  sCreatedTBSNode->mHeader.mID,
                                                  ID_TRUE, /* commit�ÿ� ���� */
                                                  SCT_POP_CREATE_TBS,
                                                  & sPendingOp)
                  != IDE_SUCCESS );
        sPendingOp->mPendingOpFunc = smmTBSCreate::createTableSpacePending;
    }

    if ( aTBSID != NULL )
    {
        *aTBSID = sSpaceID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Tablespace�� �����ϰ� ������ �Ϸ�Ǹ� NTA�� ���´�.
 * ������ ������ ��� NTA�� ���۱��� Physical Undo�Ѵ�.
 *
 * [IN] aTrans          - Tablespace�� �����Ϸ��� Transaction
 * [IN] aDBName         - Tablespace�� ������ Database�� �̸�
 * [IN] aTBSAttr        - Tablespace�� Attribute
 *                  ( ����ڰ� ������ �������� �ʱ�ȭ�Ǿ� ����)
 * [IN] aChkptPathList  - Tablespace�� Checkpoint Image���� ������ Path��
 * [IN] aInitPageCount  - Tablespace�� �ʱ�ũ�� ( Page�� )
 * [IN] aDBCharSet      - �����ͺ��̽� ĳ���� ��(PROJ-1579 NCHAR)
 * [IN] aNationalCharSet- ���ų� ĳ���� ��(PROJ-1579 NCHAR)
 * [OUT] aCreatedTBSNode - ������ Tablespace�� Node
 *****************************************************************************/
IDE_RC smmTBSCreate::createTBSWithNTA(
           void                  * aTrans,
           SChar                 * aDBName,
           smiTableSpaceAttr     * aTBSAttr,
           smiChkptPathAttrList  * aChkptPathList,
           scPageID                aInitPageCount,
           SChar                 * aDBCharSet,
           SChar                 * aNationalCharSet,
           smmTBSNode           ** aCreatedTBSNode )
{
    IDE_DASSERT( aTrans                 != NULL );
    IDE_DASSERT( aDBName                != NULL );
    IDE_DASSERT( aCreatedTBSNode        != NULL );
    IDE_DASSERT( aInitPageCount          > 0 );
    IDE_DASSERT( aTBSAttr->mName[0]     != '\0' );
    IDE_DASSERT( aTBSAttr->mAttrType    == SMI_TBS_ATTR );
    // NO ASSERT : aChkptPathAttrList�� ��� NULL�� �� �ִ�.

    idBool         sNTAMarked    = ID_TRUE;
    idBool         sChkptBlocked = ID_FALSE;
    idBool         sNodeCreated  = ID_FALSE;
    idBool         sNodeAdded    = ID_FALSE;

    smLSN          sNTALSN;
    smmTBSNode   * sTBSNode;
    sctLockHier    sLockHier;

    SCT_INIT_LOCKHIER(&sLockHier);

    sNTALSN = smLayerCallback::getLstUndoNxtLSN( aTrans );
    sNTAMarked = ID_TRUE;

    // Tablespace Node�� �Ҵ�, �ٴܰ� �ʱ�ȭ�� �����Ѵ�.
    // - ó������
    //   - Tablespace Node�� �Ҵ�
    //   - Tablespace�� PAGE�ܰ���� �ʱ�ȭ�� ����
    IDE_TEST( allocAndInitTBSNode( aTrans,
                                   aTBSAttr,
                                   & sTBSNode )
              != IDE_SUCCESS );
    sNodeCreated = ID_TRUE;

    // TBSNode�� ���� Lock�� X�� ��´�. ( �Ϲ� DML,DDL�� ���� )
    //   - Lockȹ��� Lock Slot�� sLockHier->mTBSNodeSlot�� �Ѱ��ش�.
    //   - �� �Լ� ȣ������ �����ִ� Latch�� �־�� �ȵȴ�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                  aTrans,
                  & sTBSNode->mHeader,
                  ID_FALSE,   /* intent */
                  ID_TRUE,    /* exclusive */
                  sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aTrans),
                  SCT_VAL_DDL_DML,/* validation */
                  NULL,       /* is locked */
                  & sLockHier )      /* lockslot */
              != IDE_SUCCESS );

    // - Checkpoint�� Block��Ų��.
    //   - Checkpoint�� Dirty Page�� flush�ϴ� �۾��� ����
    //   - Checkpoint�� DB File Header�� flush�ϴ� �۾��� ����
    IDE_TEST( smLayerCallback::blockCheckpoint() != IDE_SUCCESS );
    sChkptBlocked=ID_TRUE;

    ///////////////////////////////////////////////////////////////
    // �α�ǽ� => SCT_UPDATE_MRDB_CREATE_TBS
    //  - redo�� : Tablespace���¿��� SMI_TBS_CREATING ���¸� ���ش�
    //  - undo�� : Tablespace���¸� SMI_TBS_DROPPED�� ����
    IDE_TEST( smLayerCallback::writeMemoryTBSCreate(
                                    NULL, /* idvSQL* */
                                    aTrans,
                                    aTBSAttr,
                                    aChkptPathList )
              != IDE_SUCCESS );

    // Tablespace Node�� TBS Node List�� �߰��Ѵ�.
    IDE_TEST( registerTBS( sTBSNode ) != IDE_SUCCESS );
    sNodeAdded = ID_TRUE;

    IDE_TEST( createTBSInternal(aTrans,
                                sTBSNode,
                                aDBName,
                                aChkptPathList,
                                aInitPageCount,
                                aDBCharSet,
                                aNationalCharSet) != IDE_SUCCESS );

    // ====== write NTA ======
    // - Redo : do nothing
    // - Undo : drop Tablespace����
    IDE_TEST( smLayerCallback::writeCreateTbsNTALogRec(
                                   NULL, /* idvSQL* */
                                   aTrans,
                                   &sNTALSN,
                                   sTBSNode->mHeader.mID )
              != IDE_SUCCESS );

    sChkptBlocked = ID_FALSE;
    IDE_TEST( smLayerCallback::unblockCheckpoint() != IDE_SUCCESS );

    * aCreatedTBSNode = sTBSNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sNTAMarked == ID_TRUE )
    {
        // Fatal ������ �ƴ϶��, NTA���� �ѹ�
        IDE_ASSERT( smLayerCallback::undoTrans( NULL, /* idvSQL* */
                                                aTrans,
                                                &sNTALSN )
                    == IDE_SUCCESS );
    }

    if ( sChkptBlocked == ID_TRUE )
    {
        IDE_ASSERT( smLayerCallback::unblockCheckpoint() == IDE_SUCCESS );
    }

    if ( sNodeCreated == ID_TRUE )
    {
        if ( sNodeAdded == ID_TRUE )
        {
            // Tablespace Node�� �ý��ۿ� �߰��� ���
            // SCT_UPDATE_MRDB_CREATE_TBS �� Undo�����
            // Tablespace�� Phase�� STATE�ܰ���� �����ش�.
            // ���� �������� �ʴ´�.
        }
        else
        {
            // �����ִ� Lock�� �ִٸ� ����
            if ( sLockHier.mTBSNodeSlot != NULL )
            {
                IDE_ASSERT( smLayerCallback::unlockItem( aTrans,
                                                         sLockHier.mTBSNodeSlot )
                            == IDE_SUCCESS );
            }

            // ��带 ����
            IDE_ASSERT( finiAndFreeTBSNode( sTBSNode ) == IDE_SUCCESS );
        }
    }

    IDE_POP();

    return IDE_FAILURE;
}

/******************************************************************************
 * Tablespace�� �ý��ۿ� ����ϰ� ���� �ڷᱸ���� �����Ѵ�.
 *
 * aTrans              [IN] Tablespace�� �����Ϸ��� Transaction
 * aTBSNode            [IN] Tablespace Node
 * aDBName             [IN] Tablespace�� ���ϴ� Database�� �̸�
 * aChkptPathAttrList  [IN] Checkpoint Path Attribute�� List
 * aCreatedTBSNode     [OUT] ������ Tablespace�� Node
 *
 * [ PROJ-1548 User Memory Tablespace ]
 ******************************************************************************/
IDE_RC smmTBSCreate::createTBSInternal(
                          void                 * aTrans,
                          smmTBSNode           * aTBSNode,
                          SChar                * aDBName,
                          smiChkptPathAttrList * aChkptPathAttrList,
                          scPageID               aInitPageCount,
                          SChar                * aDBCharSet,
                          SChar                * aNationalCharSet)
{
    IDE_DASSERT( aTrans         != NULL );
    IDE_DASSERT( aDBName        != NULL );
    IDE_DASSERT( aTBSNode       != NULL );
    IDE_DASSERT( aInitPageCount > 0 );

    UInt sWhichDB   = 0;
    // NO ASSERT : aChkptPathAttrList�� ��� NULL�� �� �ִ�.

    ///////////////////////////////////////////////////////////////
    // (070) Checkpoint Path Node���� ����
    IDE_TEST( createDefaultOrUserChkptPaths( aTBSNode,
                                             aChkptPathAttrList )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (080) ���̺� �����̽��� ������ �޸𸮷� ����
    //       Page Pool�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( smmManager::initializePagePool( aTBSNode )
              != IDE_SUCCESS );

    // (100) initialize Membase on Page 0
    // (110) alloc ExpandChunks
    IDE_TEST( smmManager::createTBSPages(
                  aTBSNode,
                  aTrans,
                  aDBName,
                  aTBSNode->mTBSAttr.mMemAttr.mSplitFilePageCount,
                  aInitPageCount,
                  aDBCharSet,
                  aNationalCharSet)
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (120) Log Anchor Flush
    //
    // Commit �α׸� ��ϵǰ� Pendingó�� �ȵ� ��쿡 ����Ͽ�
    // Log Anchor�� CREATE���� Tablespace�� Flush
    // SpaceNode Attr �� ���� �α׾�Ŀ�� ������ ��
    // Chkpt Image Attr �� �������� �Ѵ�.  -> (130)
    //
    // - SMI_TBS_INCONSISTENT ���·� Log Anchor�� ������.
    //   - Tablespace�� ������ �������� ���� ����
    //   - Membase�� DB File���� Consistency�� ������ �� ���� ����
    //   - Restart�� Prepare/Restore�� Skip�Ͽ��� �Ѵ�.

    aTBSNode->mHeader.mState |= SMI_TBS_INCONSISTENT ;
    IDE_TEST( smmTBSCreate::flushTBSAndCPaths(aTBSNode) != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////
    // (130) create Checkpoint Image Files (init size��ŭ)
    // ���̺� �����̽��� ��� ������ �����Ѵ�.
    // ���ÿ� �α׾�Ŀ�� Chkpt Image Attr �� ����ȴ�.
    // - File���� ���� Logging�� �ǽ��Ѵ�.
    //  - redo�� : do nothing
    //  - undo�� : file����
    IDE_TEST( smmManager::createDBFile( aTrans,
                                        aTBSNode,
                                        aTBSNode->mHeader.mName,
                                        aInitPageCount,
                                        ID_TRUE /* aIsNeedLogging */ )

              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////
    // (140) flush Page 0 to Checkpoint Image
    //
    // To Fix BUG-17227 create tablespace�� server kill; server start�ȵ�
    //  - Restart Redo/Undo������ 0�� Page�� �о sm version check��
    //    �ϱ� ������, 0�� Page�� Flush�ؾ���.
    for ( sWhichDB = 0; sWhichDB <= 1; sWhichDB++ )
    {
        IDE_TEST( smmManager::flushTBSMetaPage( aTBSNode, sWhichDB )
                  != IDE_SUCCESS );
    }

    // Membase�� Tablespace�� ��� Resource�� consistent�ϰ�
    // ���� �Ǿ����Ƿ�, Tablespace�� ���¿���
    // SMI_TBS_INCONSISTENT ���¸� �����Ѵ�.
    aTBSNode->mHeader.mState &= ~SMI_TBS_INCONSISTENT;
    IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( & aTBSNode->mHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace Node�� �׿� ���� Checkpoint Path Node���� ���
    Log Anchor�� Flush�Ѵ�.

    [IN] aTBSNode - Flush�Ϸ��� Tablespace Node
 */
IDE_RC smmTBSCreate::flushTBSAndCPaths(smmTBSNode * aTBSNode)
{
    smuList    * sNode;
    smuList    * sBaseNode;
    smmChkptPathNode  * sCPathNode;

    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smLayerCallback::addTBSNodeAndFlush( (sctTableSpaceNode*)aTBSNode )
              != IDE_SUCCESS );

    sBaseNode = &aTBSNode->mChkptPathBase;

    for (sNode = SMU_LIST_GET_FIRST(sBaseNode);
         sNode != sBaseNode;
         sNode = SMU_LIST_GET_NEXT(sNode))
    {
        sCPathNode = (smmChkptPathNode*)(sNode->mData);

        IDE_ASSERT( aTBSNode->mHeader.mID ==
                    sCPathNode->mChkptPathAttr.mSpaceID );

        IDE_TEST( smLayerCallback::addChkptPathNodeAndFlush( sCPathNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   Tablespace�� Create�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�

   [ �˰��� ]
     (010) latch TableSpace Manager
     (020) TBSNode.State ���� CREATING���� ����
     (030) TBSNode�� Log Anchor�� Flush
     (040) Runtime���� ���� => Tablespace�� �� Counting
     (050) unlatch TableSpace Manager
   [ ���� ]
     Commit Pending�Լ��� �����ؼ��� �ȵǱ� ������
     ���� ���� Detect�� ���� IDE_ASSERT�� ����ó���� �ǽ��Ͽ���.
 */

IDE_RC smmTBSCreate::createTableSpacePending(  idvSQL            * aStatistics,
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

    // Tablespace�� SMI_TBS_CREATING�� ��� ��Ʈ�� ���� �ִ��� ASSERT��
    // �ɾ�� �ȵȴ�.
    //
    // ���� :
    //   CREATE TABLESPACE .. OFFLINE �ÿ�
    //   Alter Tablespace Offline�� COMMIT PENDING�� ����Ǿ�
    //   SMI_TBS_SWITCHING_TO_OFFLINE�� ���ŵ� ä�� ���� �� �ִ�.
    //
    //   �׷��� SMI_TBS_CREATING �� SMI_TBS_SWITCHING_TO_OFFLINE��
    //   �Ϻ� ��Ʈ�� �����ϱ� ������, Tablespace�� SMI_TBS_CREATING��
    //   ��� ��Ʈ�� ���� ���� �ʰ� �ȴ�.

    aSpaceNode->mState &= ~SMI_TBS_CREATING;


    ///////////////////////////////////////////////////////////////
    // (030) TBSNode�� Log Anchor�� Flush
    /* PROJ-2386 DR
     * standby recovery�� ������ ������ loganchor update �Ѵ�. */
    IDE_ASSERT( smLayerCallback::updateTBSNodeAndFlush( aSpaceNode )
                == IDE_SUCCESS );



    /////////////////////////////////////////////////////////////////////
    // (050) unlatch TableSpace Manager
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );


    return IDE_SUCCESS;
}


/*
    smmTBSNode�� �����ϰ� X Lock�� ȹ���� ��  sctTableSpaceMgr�� ����Ѵ�.

    [ Lock / Latch���� deadlock ȸ�� ]
    - Lock�� ���� ��� Latch�� ���߿� ��Ƽ� Deadlock�� ȸ���Ѵ�.
    - �� �Լ��� �Ҹ��� ���� latch�� ���� ���¿����� �ȵȴ�.


    [IN] aTrans      - Tablespace �� ���� Lock�� ����� �ϴ� ���
                       Lock�� ���� Transaction
    [IN] aTBSAttr    - ������ Tablespace Node�� �Ӽ�
    [IN] aCreatedTBSNode - ������ Tablespace Node
*/
IDE_RC smmTBSCreate::allocAndInitTBSNode(
                                   void                * aTrans,
                                   smiTableSpaceAttr   * aTBSAttr,
                                   smmTBSNode         ** aCreatedTBSNode )

{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );
    IDE_DASSERT( aCreatedTBSNode != NULL );

    ACP_UNUSED( aTrans );

    UInt                 sStage = 0;
    smmTBSNode         * sTBSNode = NULL;

    /* TC/FIT/Limit/sm/smm/smmTBSCreate_allocAndInitTBSNode_calloc.sql */
    IDU_FIT_POINT_RAISE( "smmTBSCreate::allocAndInitTBSNode::calloc",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               1,
                               ID_SIZEOF(smmTBSNode),
                               (void**)&(sTBSNode)) != IDE_SUCCESS,
                   insufficient_memory );
    sStage = 1;


    ///////////////////////////////////////////////////////////////
    // (080) Tablespace�� �ٴܰ� �ʱ�ȭ
    // ���⿡�� TBSNode.mState�� TBSAttr.mTBSStateOnLA�� ����
    // - ó������ ONLINE���·� Tablespace�� �ٴܰ� �ʱ�ȭ
    // - ����ڰ� OFFLINE���� ������ Tablespace�� ���
    //   Create Tablespace�� ������ �ܰ迡�� OFFLINE���� ��ȯ
    //
    // - Create���� Tablespace�� ����
    //   - SMI_TBS_CREATING
    //     - BACKUP���� ���ü� ��� ���ظ� �־���
    //     - Commit Pending�� CREATING���°� ���ŵ�
    aTBSAttr->mTBSStateOnLA = SMI_TBS_CREATING | SMI_TBS_ONLINE;

    IDE_TEST( smmTBSMultiPhase::initTBS( sTBSNode,
                                         aTBSAttr )
              != IDE_SUCCESS );
    sStage = 2;


    * aCreatedTBSNode = sTBSNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 2:
        {
            IDE_ASSERT( smmTBSMultiPhase::finiTBS( sTBSNode )
                        == IDE_SUCCESS );
        }

        case 1:
        {
            IDE_ASSERT( iduMemMgr::free(sTBSNode) == IDE_SUCCESS );
        }

        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
    Tablespace Node�� �ٴܰ� ���������� free�Ѵ�.
 */
IDE_RC smmTBSCreate::finiAndFreeTBSNode( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmTBSMultiPhase::finiTBS( aTBSNode ) != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free(aTBSNode) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    �ٸ� Transaction���� Tablespace�̸����� Tablespace�� ã�� �� �ֵ���
    �ý��ۿ� ����Ѵ�.

    - ���� �̸��� Tablespace�� �̹� �����ϴ��� ���⿡�� üũ�Ѵ�.

    [IN] aTBSNode - ����Ϸ��� Tablespace�� Node
 */
IDE_RC smmTBSCreate::registerTBS( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    UInt    sStage = 0;
    idBool  sIsExistNode;

    // ������ Tablespace�� ���ÿ� �����Ǵ� ���� �����ϰ� ����
    // global tablespace latch�� ��´�.
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sStage = 1;

    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ��� ���� �޽����� ��ȯ���� �ʵ��� ����
    sIsExistNode = sctTableSpaceMgr::checkExistSpaceNodeByName( aTBSNode->mHeader.mName );

    IDE_TEST_RAISE( sIsExistNode == ID_TRUE, err_already_exist_tablespace_name );

    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode*)aTBSNode );

    sStage = 0;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_already_exist_tablespace_name )
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistTableSpaceName,
                                aTBSNode->mHeader.mName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 1:
        {
            IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
        }
        break;
        default :
            break;

    }

    IDE_POP();

    return IDE_FAILURE;
}






