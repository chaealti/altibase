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
* $Id: smmTBSStartupShutdown.cpp 19201 2006-11-30 00:54:40Z kmkim $
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

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSStartupShutdown::smmTBSStartupShutdown()
{

}



/*
    Server startup�� Log Anchor�� Tablespace Attribute�� ��������
    Tablespace Node�� �����Ѵ�.

    [IN] aTBSAttr      - Log Anchor�� ����� Tablespace�� Attribute
    [IN] aAnchorOffset - Log Anchor�� Tablespace Attribute�� ����� Offset
*/
IDE_RC smmTBSStartupShutdown::loadTableSpaceNode(
                                           smiTableSpaceAttr   * aTBSAttr,
                                           UInt                  aAnchorOffset )
{
    smmTBSNode    * sTBSNode;
    UInt            sState  = 0;

    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );

    /* smmTBSStartupShutdown_loadTableSpaceNode_calloc_TBSNode.tc */
    IDU_FIT_POINT("smmTBSStartupShutdown::loadTableSpaceNode::calloc::TBSNode");
    IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               1,
                               ID_SIZEOF(smmTBSNode),
                               (void**)&(sTBSNode))
                 != IDE_SUCCESS);
    sState  = 1;

    // Tablespace�� ���¿� ���� �ٴܰ� �ʱ�ȭ ����
    // �켱 STATE�ܰԱ����� �ʱ�ȭ�Ѵ�.
    //
    // MEDIA�ܰ� �ʱ�ȭ�� ���ؼ��� üũ����Ʈ ��θ� �ʿ�� �Ѵ�.
    // Log Anchor�κ��� ��� üũ����Ʈ �н� ��� �� ���� �ε�� �Ŀ�
    // MEDIA�ܰ� ������ �ʱ�ȭ�� �����Ѵ�.

    // ���⿡�� TBSNode.mState�� TBSAttr.mTBSStateOnLA�� ����
    IDE_TEST( smmTBSMultiPhase::initStatePhase( sTBSNode,
                                                aTBSAttr )
              != IDE_SUCCESS );

    // Log Anchor���� Offset�ʱ�ȭ
    sTBSNode->mAnchorOffset = aAnchorOffset;


    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ��� ���� �޽����� ��ȯ���� �ʵ��� ����
    IDE_ASSERT( sctTableSpaceMgr::checkExistSpaceNodeByName(
                    sTBSNode->mHeader.mName ) == ID_FALSE );

    // ������ Tablespace Node�� Tablespace�����ڿ� �߰�
    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode*)sTBSNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sTBSNode ) == IDE_SUCCESS );
            sTBSNode = NULL;
        default:
            break;
    }

    // ���������� Tablespace Load���� ���н�
    // ����ó������ �ʰ� FATAL�� ���δ�.

    IDE_CALLBACK_FATAL("Failed to initialize Tablespace reading from loganchor ");

    return IDE_FAILURE;
}

/*
  �α׾�Ŀ�� ���� �ǵ��� Checkpoint Image Attribute��
  ��Ÿ�� ����� �����Ѵ�. ������ ���������� �ʴ´�.

  [IN] aChkptImageAttr - �α׾�Ŀ�� �ǵ��� �޸� ����Ÿ���� �Ӽ�
  [IN] aMemRedoLSN  - �α׾�Ŀ�� ����� �޸� Redo LSN
  [IN] aAnchorOffset   - �Ӽ��� ����� LogAnchor Offset
 */
IDE_RC smmTBSStartupShutdown::initializeChkptImageAttr(
                               smmChkptImageAttr * aChkptImageAttr,
                               smLSN             * aMemRedoLSN,
                               UInt                aAnchorOffset )
{
    UInt              sLoop;
    UInt              sSmVersion;
    smmTBSNode      * sSpaceNode;
    smmDatabaseFile * sDatabaseFile;

    IDE_DASSERT( aMemRedoLSN   != NULL );
    IDE_DASSERT( aMemRedoLSN != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace(
                 aChkptImageAttr->mSpaceID ) == ID_TRUE );

    // ���̺����̽� ��� �˻�
    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aChkptImageAttr->mSpaceID );

    if ( sSpaceNode != NULL )
    {
        if ( SMI_TBS_IS_DROPPED(sSpaceNode->mHeader.mState) )
        {
            // Drop�� Tablespace�� ��� ����
            // ���� ! DROP_PENDING�� ���
            //        ���� ó������ ���� DROP�۾��� �Ϸ��ؾ� �ϱ� ������
            //        �Ʒ� else�� Ÿ�� ��.
        }
        else
        {
            // ���̺����̽� ��尡 �����ϴ� ���

            // ����Ÿ���� ��ü�� �˻��Ѵ�.
            // ���� ������ �����ϴ����� identifyDatabase����
            // ó���Ѵ�.

            // �α׾�Ŀ�κ��� ����� ���̳ʸ� ������ ��´�.
            sSmVersion = smLayerCallback::getSmVersionIDFromLogAnchor();

            for ( sLoop = 0; sLoop < SMM_PINGPONG_COUNT; sLoop ++ )
            {
                IDE_TEST( smmManager::getDBFile( sSpaceNode,
                                                 sLoop,
                                                 aChkptImageAttr->mFileNum,
                                                 SMM_GETDBFILEOP_NONE,
                                                 &sDatabaseFile )
                          != IDE_SUCCESS );

                sDatabaseFile->setChkptImageHdr(
                                        aMemRedoLSN,
                                        &aChkptImageAttr->mMemCreateLSN,
                                        &sSpaceNode->mHeader.mID,
                                        &sSmVersion,
                                        &aChkptImageAttr->mDataFileDescSlotID );
            }

            // �α׾�Ŀ�� ����� ���ϵ��� ��� ������ �����̹Ƿ�
            // crtdbfile �÷��׸� true�� �����Ͽ� �α׾�Ŀ��
            // �ߺ� �߰����� �ʵ��� �Ѵ�.
            for ( sLoop = 0; sLoop < SMM_PINGPONG_COUNT; sLoop++ )
            {
                IDE_ASSERT ( smmManager::getCreateDBFileOnDisk(
                                                 sSpaceNode,
                                                 sLoop,      // pingpong no
                                                 aChkptImageAttr->mFileNum )
                             == ID_FALSE );
            }

            // ����Ÿ���̽� ���������� �̵�����������
            // �α׾�Ŀ�� ����� ����Ÿ������ ������� �����ϰ�
            // ������ �ϱ� ���� LstCreatedDBFile �� �����Ѵ�.
            if ( aChkptImageAttr->mFileNum > 0 )
            {
                IDE_ASSERT( aChkptImageAttr->mFileNum >
                            sSpaceNode->mLstCreatedDBFile );
            }

            sSpaceNode->mLstCreatedDBFile = aChkptImageAttr->mFileNum;

            for ( sLoop = 0; sLoop < SMM_PINGPONG_COUNT; sLoop++ )
            {
                smmManager::setCreateDBFileOnDisk(
                                sSpaceNode,
                                sLoop, // pingpong no.
                                aChkptImageAttr->mFileNum,
                                aChkptImageAttr->mCreateDBFileOnDisk[ sLoop ] );
            }

            smmManager::setAnchorOffsetToCrtDBFileInfo( sSpaceNode,
                                                        aChkptImageAttr->mFileNum,
                                                        aAnchorOffset );
        }

    }
    else
    {
        // DROPPED�� �Ǿ��ų� �������� �ʴ� ��쿡��
        // NOTHING TO DO...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Loganchor�κ��� �о���� Checkpoint Path Attribute�� Node�� �����Ѵ�.
 *
 * aChkptPathAttr [IN] - �߰��� smiChkptPathAttr* Ÿ���� Attribute
 * aAnchorOffset  [IN] - Loganchor�� �޸𸮹��ۻ��� ChkptPath Attribute ������
 */
IDE_RC smmTBSStartupShutdown::createChkptPathNode(
                                 smiChkptPathAttr *  aChkptPathAttr,
                                 UInt                aAnchorOffset )
{
    smmTBSNode        * sTBSNode;
    smmChkptPathNode  * sCPathNode;
    UInt                sState  = 0;

    IDE_DASSERT( aChkptPathAttr != NULL );
    IDE_DASSERT( aChkptPathAttr->mAttrType == SMI_CHKPTPATH_ATTR );
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace(
                                   aChkptPathAttr->mSpaceID ) == ID_TRUE );
    IDE_DASSERT( aAnchorOffset != 0 );

    /* smmTBSStartupShutdown_createChkptPathNode_calloc_CPathNode.tc */
    IDU_FIT_POINT("smmTBSStartupShutdown::createChkptPathNode::calloc::CPathNode");
    IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               1,
                               ID_SIZEOF(smmChkptPathNode),
                               (void**)&(sCPathNode))
             != IDE_SUCCESS);
    sState = 1;

    idlOS::memcpy(& sCPathNode->mChkptPathAttr,
                  aChkptPathAttr,
                  ID_SIZEOF(smiChkptPathAttr));

    // set attibute loganchor offset
    sCPathNode->mAnchorOffset = aAnchorOffset;

    // ���̺����̽� ��� �˻�
    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aChkptPathAttr->mSpaceID );

    if ( sTBSNode != NULL )
    {
        sctTableSpaceMgr::lockSpaceNode( NULL /* idvSQL * */,
                                         sTBSNode );
        sState = 2;

        if ( SMI_TBS_IS_DROPPED(sTBSNode->mHeader.mState) )
        {
            // Drop�� Tablespace�� ��� ����
            // ���� ! DROP_PENDING�� ���
            //        ���� ó������ ���� DROP�۾��� �Ϸ��ؾ� �ϱ� ������
            //        �Ʒ� else�� Ÿ�� ��.
        }
        else
        {
            IDE_TEST( smmTBSChkptPath::addChkptPathNode( sTBSNode,
                                                         sCPathNode )
                      != IDE_SUCCESS );
        }
        sState = 1;
        sctTableSpaceMgr::unlockSpaceNode( sTBSNode );
    }
    else
    {
        // Drop�� Tablespace�� ��� ����
    }


    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 2:
            sctTableSpaceMgr::unlockSpaceNode( sTBSNode );
        case 1:
            IDE_ASSERT( iduMemMgr::free( sCPathNode ) == IDE_SUCCESS );
            sCPathNode = NULL;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}



/* smmTBSStartupShutdown::initFromStatePhase4AllTBS �� �׼� �Լ�
 */
IDE_RC smmTBSStartupShutdown::initFromStatePhaseAction(
           idvSQL            * /*aStatistics */,
           sctTableSpaceNode * aTBSNode,
           void * /* aActionArg */ )
{
    IDE_DASSERT( aTBSNode != NULL );

    if ( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE )
    {
        IDE_TEST( smmTBSMultiPhase::initFromStatePhase (
                                          (smmTBSNode *) aTBSNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ��� Tablespace�� �ٴܰ� �ʱ�ȭ�� �����Ѵ�.
 *
 * �޸� Tablespace�� STATE => MEDIA => PAGE �� ������
 * �ܰ躰�� �ʱ�ȭ �ȴ�.
 *
 * STATE�ܰ�� Log Anchor���� Tablespace Node�� �о���϶� �ʱ�ȭ�ǰ�,
 * MEDIA, PAGE�ܰ�� Log Anchor�κ��� ��� Tablespace Node��
 * ���� ������ �� �о�����Ŀ� �ʱ�ȭ �ȴ�.
 *
 * MEDIA Phase�� �ʱ�ȭ�ϱ� ���ؼ��� Tablespace Node��
 * üũ����Ʈ ��ΰ� ���õǾ� �־�� �Ѵ�.
 * �׷���, üũ����Ʈ ��� Attribute�� ����� ������ Log Anchor��
 * �߰� �߰��� ��Ÿ�� �� �ֱ� ������,
 * Log Anchor�� �ε尡 ��� �Ϸ�� �Ŀ� MEDIA�ܰ踦 �ʱ�ȭ�Ѵ�.
 *
 */
IDE_RC smmTBSStartupShutdown::initFromStatePhase4AllTBS()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                  NULL,  /* idvSQL* */
                                  initFromStatePhaseAction,
                                  NULL, /* Action Argument*/
                                  SCT_ACT_MODE_NONE)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
 * ��� Tablespace�� destroy�Ѵ�.
 */
IDE_RC smmTBSStartupShutdown::destroyAllTBSNode()
{
    sctTableSpaceNode *sNextSpaceNode;
    sctTableSpaceNode *sCurrSpaceNode;

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNodeIncludingDropped( sCurrSpaceNode->mID );

        if ( sctTableSpaceMgr::isMemTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {

            sctTableSpaceMgr::removeTableSpaceNode( sCurrSpaceNode );

            /* BUG-40451 Valgrind Warning
             * - smmTBSDrop::dropTableSpacePending()�� ó���� ���ؼ�, ���� �˻�
             *   �ϰ� svmManager::finiTBS()�� ȣ���մϴ�.
             */ 
            if ( ( sCurrSpaceNode->mState & SMI_TBS_DROPPED )
                 != SMI_TBS_DROPPED )
            {
                IDE_TEST( smmTBSMultiPhase::finiTBS (
                              (smmTBSNode *) sCurrSpaceNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // Drop�� TBS��  �̹� �ڿ��� �����Ǿ� �ִ�.
            }

            // To Fix BUG-18178
            //    Shutdown�� Tablespace Node Memory�� �������� ����
            IDE_TEST( iduMemMgr::free( sCurrSpaceNode ) != IDE_SUCCESS );
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Memory Tablespace �������� �ʱ�ȭ
 */
IDE_RC smmTBSStartupShutdown::initializeStatic()
{
    // �ƹ��͵� ���� �ʴ´�.

    return IDE_SUCCESS;
}



/*
    Memory Tablespace�������� ����
 */
IDE_RC smmTBSStartupShutdown::destroyStatic()
{
    // ��� Memory Tablespace�� destroy�Ѵ�.
    IDE_TEST( destroyAllTBSNode() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




