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
* $Id: smmTBSAlterChkptPath.cpp 19201 2006-11-30 00:54:40Z kmkim $
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
#include <smmTBSAlterChkptPath.h>
#include <smmTBSMultiPhase.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSAlterChkptPath::smmTBSAlterChkptPath()
{

}


/*
    ���ο� Checkpoint Path�� �߰��Ѵ�.

    aTrans     [IN] Transaction
    aSpaceID   [IN] Tablespace�� ID
    aChkptPath [IN] Tablespace�� �߰��� Checkpoint Path

    [ �˰��� ]
      (010) CheckpointPathNode �Ҵ�
      (020) TBSNode �� CheckpointPathNode �߰�
      (030) Log Anchor Flush

    [ Checkpoint Path add���� ���� ����ϸ�? ]
      - ���� ���� Log Anchor�� �Ϻΰ� �κ� Flush�Ǿ�
        inconsistent�� ���°� �ȴ�.
      - ���� ��⵿�� consistent�� �ֽ� Log Anchor��
        �а� �ǹǷ� �������� ����.

    [ ����ó�� ]
      (e-010) CONTROL�ܰ谡 �ƴϸ� ����
      (e-020) �̹� Tablespace�� �����ϴ� Checkpoint Path�̸� ���� 
      (e-030) NewPath�� ���� ���丮�̸� ����
      (e-040) NewPath�� ���丮�� �ƴ� �����̸� ����
      (e-050) NewPath�� read/write/execute permission�� ������ ����

    [ ��Ÿ ]
      - ������ Checkpoint���� ���� �߰��� Checkpoint Path���� 
        Checkpoint Image File ��й踦 ���� �ʴ´�.
      - Checkpoint Image�� ���ڱ� �������� DBA�� ��Ȳ�� ���̴�.
      - Checkpoint Image File ��й迡 ���� Undoó���� �����ϴ�.
      - ��й谡 �ʿ��� ��� altibase ������ DBA�� ����ó��
    
    [ ���� ]
      ���⿡ ���ڷ� �Ѿ���� Checkpoint Path �޸𸮴�
      �Լ� ȣ�� ������ �����Ǳ� ������ �״�� ����ϸ� �ȵȴ�.
      
*/
IDE_RC smmTBSAlterChkptPath::alterTBSaddChkptPath( scSpaceID          aSpaceID,
                                            SChar            * aChkptPath )
{
    smmTBSNode       * sTBSNode;
    smmChkptPathNode * sCPathNode = NULL;

    IDE_DASSERT( aChkptPath != NULL );
        
    //////////////////////////////////////////////////////////////////
    // (e-010) CONTROL�ܰ谡 �ƴϸ� ����
    // => smiTableSpace���� �̹� ó��

    // Tablespace�� TBSNode�� ���´�.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                  aSpaceID,
                  (void**) & sTBSNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////
    // (e-020) �̹� Tablespace�� �����ϴ� Checkpoint Path�̸� ����
    IDE_TEST( smmTBSChkptPath::findChkptPathNode( sTBSNode,
                                                  aChkptPath,
                                                  & sCPathNode )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sCPathNode != NULL, err_chkpt_path_already_exists );
        
    //////////////////////////////////////////////////////////////////
    // (e-030) NewPath�� ���� ���丮�̸� ����
    // (e-040) NewPath�� ���丮�� �ƴ� �����̸� ����
    // (e-050) NewPath�� read/write/execute permission�� ������ ����
    IDE_TEST( smmTBSChkptPath::checkAccess2ChkptPath( aChkptPath )
              != IDE_SUCCESS );
    
    //////////////////////////////////////////////////////////////////
    // (010) CheckpointPathNode �Ҵ�
    IDE_TEST( smmTBSChkptPath::makeChkptPathNode( aSpaceID,
                                                  aChkptPath,
                                                  & sCPathNode )
              != IDE_SUCCESS );
    
    // (020) TBSNode �� CheckpointPathNode �߰�
    IDE_TEST( smmTBSChkptPath::addChkptPathNode( sTBSNode,
                                                 sCPathNode )
              != IDE_SUCCESS );
    
    //////////////////////////////////////////////////////////////////
    // (030) Log Anchor Flush
    IDE_TEST( smLayerCallback::addChkptPathNodeAndFlush( sCPathNode )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_chkpt_path_already_exists );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_ALREADY_EXISTS,
                                aChkptPath));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*
    ������ Checkpoint Path�� �����Ѵ�.
    
    aTrans         [IN] Transaction
    aSpaceID       [IN] Tablespace�� ID
    aOrgChkptPath  [IN] ���� Checkpoint Path
    aNewChkptPath  [IN] �� Checkpoint Path

    [ �˰��� ]
      (010) CheckpointPathNode �˻� 
      (020) CheckpointPath ���� 
      (030) Log Anchor Flush

    [ Checkpoint Path rename���� ���� ����ϸ�? ]
      - smmTBSAlterChkptPath::addChkptPath ����
    
    [ ����ó�� ]
      (e-010) CONTROL�ܰ谡 �ƴϸ� ����
      (e-020) sNewChkptPath�� Tablespace�� �̹� �����ϸ� ���� 
      (e-030) aOrgChkptPath �� aSpaceID�� �ش��ϴ� Tablespace�� ������ ����
      (e-040) NewPath�� ���� ���丮�̸� ����
      (e-050) NewPath�� ���丮�� �ƴ� �����̸� ����
      (e-060) NewPath�� read/write/execute permission�� ������ ����

    [ ��Ÿ ]
      - Checkpoint Image�� OldPath���� NewPath��
        �̵��ϴ� ó���� ���� �ʴ´�. ( DBA�� ���� ó���ؾ� �� )
      
    [ ���� ]
      ���⿡ ���ڷ� �Ѿ���� smiChkptPathAttr�� �޸𸮴�
      �Լ� ȣ�� ������ ������ �� �����Ƿ� �״�� ����ϸ� �ȵȴ�.
      smiChkptPathAttr�� ���� �Ҵ��Ͽ� �����ؾ� �Ѵ�.
*/
IDE_RC smmTBSAlterChkptPath::alterTBSrenameChkptPath( scSpaceID      aSpaceID,
                                               SChar        * aOrgChkptPath,
                                               SChar        * aNewChkptPath )
{
    smmTBSNode       * sTBSNode;
    smmChkptPathNode * sCPathNode = NULL;
    
    IDE_DASSERT( aOrgChkptPath != NULL );
    IDE_DASSERT( aNewChkptPath != NULL );

    //////////////////////////////////////////////////////////////////
    // (e-010) CONTROL�ܰ谡 �ƴϸ� ����
    // => smiTableSpace���� �̹� ó��

    // Memory Dictionary Tablespace�� TBSNode�� ���´�.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                  aSpaceID,
                  (void**) & sTBSNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////
    // (e-020) sNewChkptPath�� Tablespace�� �̹� �����ϸ� ���� 
    IDE_TEST( smmTBSChkptPath::findChkptPathNode( sTBSNode,
                                                  aNewChkptPath,
                                                  & sCPathNode )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sCPathNode != NULL, err_chkpt_path_node_already_exist );
    
    //////////////////////////////////////////////////////////////////////
    // (010) CheckpointPathNode �˻�
    IDE_TEST( smmTBSChkptPath::findChkptPathNode( sTBSNode,
                                                  aOrgChkptPath,
                                                  & sCPathNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////
    // (e-030) aOrgChkptPath �� aSpaceID�� �ش��ϴ� Tablespace�� ������ ����
    IDE_TEST_RAISE( sCPathNode == NULL, err_chkpt_path_node_not_found );

    //////////////////////////////////////////////////////////////////////
    // (e-040) NewPath�� ���� ���丮�̸� ����
    // (e-050) NewPath�� ���丮�� �ƴ� �����̸� ����
    // (e-060) NewPath�� read/write/execute permission�� ������ ����
    IDE_TEST( smmTBSChkptPath::checkAccess2ChkptPath( aNewChkptPath )
              != IDE_SUCCESS );
    
    //////////////////////////////////////////////////////////////////////
    // (020) CheckpointPath ����
    IDE_TEST( smmTBSChkptPath::renameChkptPathNode( sCPathNode, aNewChkptPath )
              != IDE_SUCCESS );
    
    //////////////////////////////////////////////////////////////////////
    // (030) Log Anchor Flush
    IDE_TEST( smLayerCallback::updateChkptPathAndFlush( sCPathNode )
              != IDE_SUCCESS );
          
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_chkpt_path_node_already_exist );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_ALREADY_EXISTS,
                                aNewChkptPath));
    }
    IDE_EXCEPTION( err_chkpt_path_node_not_found );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NODE_NOT_EXIST,
                                aOrgChkptPath));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
    ������ Checkpoint Path�� �����Ѵ�.

    aTrans     [IN] Transaction
    aSpaceID   [IN] Tablespace�� ID
    aChkptPath [IN] Tablespace���� ������ Checkpoint Path

    [ �˰��� ]
      (010) CheckpointPathNode �˻� 
      (020) CheckpointPath ���� 
      (030) Log Anchor Flush
    

    [ Checkpoint Path drop���� ���� ����ϸ�? ]
      - smmTBSAlterChkptPath::addChkptPath ����

    [ ����ó�� ]
      (e-010) CONTROL�ܰ谡 �ƴϸ� ���� 
      (e-020) OldPath�� �ش� TBSNode�� CheckpointPath�� ������ ����
      (e-030) �ش� TBS�� ���� �ϳ����� Checkpoint Path��
              DROP�õ� �ϸ� ����
      (e-040) DROP�Ϸ��� ����� Checkpoint Image�� �ٸ� ��ȿ�� ��η� �Ű���� Ȯ��.
              DROP�Ϸ��� ��ο� ���� ������ ����

    [ ��Ÿ ]
      - Checkpoint Image�� Drop�� Path����
        Drop���� ���� Path�� �̵��ϴ� ó���� ���� �ʴ´�. 
       ( DBA�� ���� ó���ؾ� �� )
    
    ���� --
      ���⿡ ���ڷ� �Ѿ���� smiChkptPathAttr�� �޸𸮴�
      �Լ� ȣ�� ������ ������ �� �����Ƿ� �״�� ����ϸ� �ȵȴ�.
      smiChkptPathAttr�� ���� �Ҵ��Ͽ� �����ؾ� �Ѵ�.
*/
IDE_RC smmTBSAlterChkptPath::alterTBSdropChkptPath( scSpaceID     aSpaceID,
                                                    SChar       * aChkptPath )
{
    smmTBSNode       * sTBSNode;
    smmChkptPathNode * sCPathNode = NULL;
    UInt               sCPathCount = 0;

    IDE_DASSERT( aChkptPath != NULL );

    //////////////////////////////////////////////////////////////////
    // (e-010) CONTROL�ܰ谡 �ƴϸ� ����
    // => smiTableSpace���� �̹� ó��

    // Memory Dictionary Tablespace�� TBSNode�� ���´�.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                  aSpaceID,
                  (void**) & sTBSNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////////
    // (010) CheckpointPathNode �˻� 
    IDE_TEST( smmTBSChkptPath::findChkptPathNode( sTBSNode,
                                                  aChkptPath,
                                                  & sCPathNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////////
    // (e-020) OldPath�� �ش� TBSNode�� CheckpointPath�� ������ ����
    IDE_TEST_RAISE( sCPathNode == NULL, err_chkpt_path_node_not_found );

    //////////////////////////////////////////////////////////////////////
    // (e-030) �ش� TBS�� ���� �ϳ����� Checkpoint Path��
    //         DROP�õ� �ϸ� ����
    IDE_TEST( smmTBSChkptPath::getChkptPathNodeCount( sTBSNode,
                                                      & sCPathCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sCPathCount == 1, err_trial_to_drop_the_last_chkpt_path );

    //////////////////////////////////////////////////////////////////////
    // (e-040)  BUG-28523 DROP�Ϸ��� ����� Checkpoint Image File��
    //          �ٸ� ��ȿ�� ��η� �ű��� �ʰ� DROP�õ� �ϸ� ����
    IDE_TEST( smmDatabaseFile::checkChkptImgInDropCPath( sTBSNode,
                                                         sCPathNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////////
    // (020) CheckpointPath ����
    IDE_TEST( smmTBSChkptPath::removeChkptPathNode( sTBSNode, sCPathNode )
              != IDE_SUCCESS );

    //////////////////////////////////////////////////////////////////////
    // (030) Log Anchor Flush
    //
    // ��� Tablespace�� Log Anchor������ ���� �����Ѵ�.
    // ( ���ŵ� Checkpoint Path Node�� ������
    //   ������ Checkpoint Path Node�� Log Anchor�� ���ܵα� ���� )
    IDE_TEST( smLayerCallback::updateAnchorOfTBS() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_chkpt_path_node_not_found );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NODE_NOT_EXIST,
                                aChkptPath));
    }
    IDE_EXCEPTION( err_trial_to_drop_the_last_chkpt_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_DROP_LAST_CPATH,
                                aChkptPath));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

