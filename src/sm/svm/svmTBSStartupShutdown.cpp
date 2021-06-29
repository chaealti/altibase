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
#include <svmTBSStartupShutdown.h>

/*
  ������ (�ƹ��͵� ����)
*/
svmTBSStartupShutdown::svmTBSStartupShutdown()
{

}


/*
    Server startup�� Log Anchor�� Tablespace Attribute�� ��������
    Tablespace Node�� �����Ѵ�.

    [IN] aTBSAttr      - Log Anchor�� ����� Tablespace�� Attribute
    [IN] aAnchorOffset - Log Anchor�� Tablespace Attribute�� ����� Offset
*/
IDE_RC svmTBSStartupShutdown::loadTableSpaceNode(
           smiTableSpaceAttr   * aTBSAttr,
           UInt                  aAnchorOffset )
{
    UInt         sStage = 0;
    svmTBSNode * sTBSNode;

    IDE_DASSERT( aTBSAttr != NULL );
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );

    // sTBSNode�� �ʱ�ȭ �Ѵ�.
    IDE_TEST(svmManager::allocTBSNode(&sTBSNode, aTBSAttr)
             != IDE_SUCCESS);

    sStage = 1;

    // Volatile Tablespace�� �ʱ�ȭ�Ѵ�.
    IDE_TEST(svmManager::initTBS(sTBSNode) != IDE_SUCCESS);

    sStage = 2;

    // Log Anchor���� Offset�ʱ�ȭ
    sTBSNode->mAnchorOffset = aAnchorOffset;

    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ��� ���� �޽����� ��ȯ���� �ʵ��� ����
    IDE_ASSERT( sctTableSpaceMgr::checkExistSpaceNodeByName(
                    sTBSNode->mHeader.mName ) == ID_FALSE );

    // ������ Tablespace Node�� Tablespace�����ڿ� �߰�
    sctTableSpaceMgr::addTableSpaceNode((sctTableSpaceNode*)sTBSNode);
    sStage = 3;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 3:
            sctTableSpaceMgr::removeTableSpaceNode(
                                  (sctTableSpaceNode*) sTBSNode );
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
            IDE_ASSERT(svmManager::destroyTBSNode(sTBSNode)
                       == IDE_SUCCESS);
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
 * svmTBSStartupShutdown::prepareAllTBS�� ���� Action�Լ�
 * �ϳ��� Volatile TBS�� ���� TBS �ʱ�ȭ ������ �����Ѵ�.
 * �̹� TBSNode�� �ʱ�ȭ�� �Ǿ� �־�� �Ѵ�.
 * Volatile TBS �ʱ�ȭ ������ ������ ����.
 *  1. TBS page ����
 */
IDE_RC svmTBSStartupShutdown::prepareTBSAction(idvSQL*            /*aStatistics*/,
                                               sctTableSpaceNode *aTBSNode,
                                               void              */* aArg */)
{
    svmTBSNode *sTBSNode = (svmTBSNode*)aTBSNode;

    IDE_DASSERT(sTBSNode != NULL);

    if ( sctTableSpaceMgr::isVolatileTableSpace(aTBSNode->mID) == ID_TRUE )
    {
        IDE_TEST(svmManager::createTBSPages(
                                 sTBSNode,
                                 sTBSNode->mTBSAttr.mName,
                                 sTBSNode->mTBSAttr.mVolAttr.mInitPageCount )
              != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * ��� TBSNode�� ��ȸ�ϸ鼭 Volatile TBS�� ����
 * �ʱ�ȭ �۾��� �Ѵ�.
 * �� �Լ��� �ҷ����� ���� smrLogAnchorMgr��
 * ��� TBSNode���� �ʱ�ȭ ������ ��ģ �����̾�� �Ѵ�.
 */
IDE_RC svmTBSStartupShutdown::prepareAllTBS()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  prepareTBSAction,
                                                  NULL,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ��� Volatile Tablespace�� destroy�Ѵ�.
 */
IDE_RC svmTBSStartupShutdown::destroyAllTBSNode()
{

    sctTableSpaceNode * sNextSpaceNode;
    sctTableSpaceNode * sCurrSpaceNode;
    svmTBSNode        * sTBSNode;

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNodeIncludingDropped( sCurrSpaceNode->mID );

        if ( sctTableSpaceMgr::isVolatileTableSpace(sCurrSpaceNode->mID) == ID_TRUE )
        {

            sctTableSpaceMgr::removeTableSpaceNode( sCurrSpaceNode );

            /* BUG-39806 Valgrind Warning
             * - svmTBSDrop::dropTableSpacePending() �� ó���� ���ؼ�, ���� �˻�
             *   �ϰ� svmManager::finiTBS()�� ȣ���մϴ�.
             */
            sTBSNode = (svmTBSNode*)sCurrSpaceNode;

            if ( ( sTBSNode->mHeader.mState & SMI_TBS_DROPPED )
                 != SMI_TBS_DROPPED )
            {
                IDE_TEST( svmManager::finiTBS( sTBSNode ) != IDE_SUCCESS);
            }
            else
            {
                // Drop�� TBS��  �̹� �ڿ��� �����Ǿ� �ִ�.
            }

            // �� �ȿ��� sCurrSpaceNode�� �޸𸮱��� �����Ѵ�.
            IDE_TEST(svmManager::destroyTBSNode((svmTBSNode*)sCurrSpaceNode)
                     != IDE_SUCCESS );
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Volatile Tablespace �������� �ʱ�ȭ
 */
IDE_RC svmTBSStartupShutdown::initializeStatic()
{
    // �ƹ��͵� ���� �ʴ´�.

    return IDE_SUCCESS;
}



/*
    Volatile Tablespace�������� ����
 */
IDE_RC svmTBSStartupShutdown::destroyStatic()
{
    // ��� Memory Tablespace�� destroy�Ѵ�.
    IDE_TEST( destroyAllTBSNode() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
