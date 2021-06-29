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
* $Id: smmTBSChkptPath.cpp 19201 2006-11-30 00:54:40Z kmkim $
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
#include <smmTBSChkptPath.h>
#include <smmTBSMultiPhase.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSChkptPath::smmTBSChkptPath()
{

}


/*
  PRJ-1548 User Memory TableSpace ���䵵�� 
  Checkpoint Path Attribute�� Checkpoint Path�� �����Ѵ�.
    
  [IN] aCPathAttr - Checkpoint Path Attribute
  [IN] aChkptPath - Checkpoint Path ���ڿ� 
*/
IDE_RC smmTBSChkptPath::setChkptPath( smiChkptPathAttr * aCPathAttr,
                                    SChar            * aChkptPath )
{
#if defined(VC_WIN32)
    sctTableSpaceMgr::adjustFileSeparator( aChkptPath );
#endif

    idlOS::strncpy( aCPathAttr->mChkptPath,
                    aChkptPath,
                    SMI_MAX_CHKPT_PATH_NAME_LEN );
    
    aCPathAttr->mChkptPath[SMI_MAX_CHKPT_PATH_NAME_LEN] = '\0';
    
    return IDE_SUCCESS ;
}


/*
    Checkpoint Path Attribute�� �ʱ�ȭ�Ѵ�.
    
    [IN] aCPathAttr - �ʱ�ȭ�� Checkpoint Path Attribute
    [IN] aSpaceID   - �ʱ�ȭ�� Checkpoint Path Attribute��
                      ���ϴ� Tablespace�� ID
    [IN] aChkptPath - �ʱ�ȭ�� Checkpoint Path Attribute�� ���� Path
 */
IDE_RC smmTBSChkptPath::initializeChkptPathAttr(
                          smiChkptPathAttr * aCPathAttr,
                          scSpaceID          aSpaceID,
                          SChar            * aChkptPath )
{

    aCPathAttr->mAttrType   = SMI_CHKPTPATH_ATTR;
    aCPathAttr->mSpaceID    = aSpaceID;
    
    IDE_TEST( setChkptPath( aCPathAttr, aChkptPath ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
    Checkpoint Path Node�� �ʱ�ȭ�Ѵ�.
    
    [IN] aCPathNode - �ʱ�ȭ�� Checkpoint Path Node
    [IN] aSpaceID   - �ʱ�ȭ�� Checkpoint Path Attribute�� ���ϴ�
                      Tablespace�� ID
    [IN] aChkptPath - �ʱ�ȭ�� Checkpoint Path Attribute�� ���� Path
    
 */
IDE_RC smmTBSChkptPath::initializeChkptPathNode(
                          smmChkptPathNode * aCPathNode,
                          scSpaceID          aSpaceID,
                          SChar            * aChkptPath )
{

    IDE_TEST( initializeChkptPathAttr( & aCPathNode->mChkptPathAttr,
                                       aSpaceID,
                                       aChkptPath )
              != IDE_SUCCESS );

    aCPathNode->mAnchorOffset = SCT_UNSAVED_ATTRIBUTE_OFFSET;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
     Checkpoint Path Node�� �ı��Ѵ�.
  
    [IN] aCPathNode - �ı��� Checkpoint Path Node
 */
IDE_RC smmTBSChkptPath::destroyChkptPathNode(smmChkptPathNode * aCPathNode )
{
    // �ƹ� �͵� �� ���� ����.
    // ���� Checkpoint Path�� �̸��� destroy�Ǿ��ٰ� �����صд�.

    IDE_TEST( setChkptPath( & aCPathNode->mChkptPathAttr,
                            (SChar*)"destroyedTBS" )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

    

/*
    Checkpoint Path Node�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.

    [IN]  aSpaceID   - Checkpoint Path�� ���ϰ� �� Tablespace�� ID
    [IN]  aChkptPath - Checkpoint Path ( ���丮 ��� ���ڿ� )
    [OUT] aCPathNode - ��
 */
IDE_RC smmTBSChkptPath::makeChkptPathNode( scSpaceID           aSpaceID,
                                           SChar             * aChkptPath,
                                           smmChkptPathNode ** aCPathNode )
{
    UInt                sState = 0;
    smmChkptPathNode  * sCPathNode;
    
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace( aSpaceID ) == ID_TRUE );
    IDE_DASSERT( aChkptPath != NULL );

    /* smmTBSChkptPath_makeChkptPathNode_calloc_CPathNode.tc */
    IDU_FIT_POINT("smmTBSChkptPath::makeChkptPathNode::calloc::CPathNode");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SMM,
                                1,
                                ID_SIZEOF(smmChkptPathNode),
                                (void**)&(sCPathNode))
              != IDE_SUCCESS);
    sState = 1;
    
    IDE_TEST( initializeChkptPathNode( sCPathNode,
                                       aSpaceID,
                                       aChkptPath )
              != IDE_SUCCESS );

    *aCPathNode = sCPathNode;
    sState = 0;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sState )
    {
        case 1 :
            IDE_ASSERT( iduMemMgr::free( sCPathNode ) == IDE_SUCCESS );
            sCPathNode = NULL;
            break;
            
        default:
            break;
    }
    
    IDE_POP();
    
    
    return IDE_FAILURE;
}




/*
 * Checkpoint Path Node�� Ư�� Tablespace�� �߰��Ѵ�. 
 *
 * aTBSNode       [IN] - Checkpoint Path Node�� �߰��� Tablespace
 * aChkptPathNode [IN] - �߰��� Checkpoint Path Node
 */
IDE_RC smmTBSChkptPath::addChkptPathNode( smmTBSNode       * aTBSNode,
                                        smmChkptPathNode * aChkptPathNode )
{
    UInt        sState  = 0;
    smuList   * sNewNode;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aChkptPathNode != NULL );
    
    
    IDE_DASSERT( aTBSNode->mHeader.mID ==
                 aChkptPathNode->mChkptPathAttr.mSpaceID );
    
    /* smmTBSChkptPath_addChkptPathNode_calloc_NewNode.tc */
    IDU_FIT_POINT("smmTBSChkptPath::addChkptPathNode::calloc::NewNode");
    IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               1,
                               ID_SIZEOF(smuList),
                               (void**)&(sNewNode))
             != IDE_SUCCESS);
    sState = 1;

    sNewNode->mData = aChkptPathNode;
    
    SMU_LIST_ADD_LAST( & aTBSNode->mChkptPathBase,
                       sNewNode );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
 
    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sNewNode ) == IDE_SUCCESS );
            sNewNode = NULL;
        default:
            break;
    }
   
    return IDE_FAILURE;
}

/*
    Checkpoint Path Node�� Checkpoint Path ���ڿ��� ���Ͽ�
    ���� Checkpoint Path���� üũ�Ѵ�.

    [IN] aCPathNode - ���� Checkpoint Path Node
    [IN] aChkptPath - ���� Checkpoint Path ���ڿ�
 */
idBool smmTBSChkptPath::isSameChkptPath(smmChkptPathNode * aCPathNode,
                                      SChar            * aChkptPath)
{
    idBool sIsSame;

#if defined(VC_WIN32)
    sctTableSpaceMgr::adjustFileSeparator( aCPathNode->mChkptPathAttr.mChkptPath );
    sctTableSpaceMgr::adjustFileSeparator( aChkptPath );
#endif

    if ( idlOS::strcmp( aCPathNode->mChkptPathAttr.mChkptPath,
                        aChkptPath ) == 0 )
    {
        sIsSame = ID_TRUE;
    }
    else
    {
        sIsSame = ID_FALSE;
    }

    return sIsSame;
    
}


/*
 * Ư�� Tablespace���� Ư�� Checkpoint Path Node�� ã�´�.
 *
 * aTBSNode       [IN] - Checkpoint Path Node�� ã�� Tablespace
 * aChkptPath     [IN] - ã���� �ϴ� Checkpoint Path ���ڿ�
 * aChkptPathNode [IN] - ã�Ƴ� Checkpoint Path Node
 */  
IDE_RC smmTBSChkptPath::findChkptPathNode( smmTBSNode        * aTBSNode,
                                         SChar             * aChkptPath,
                                         smmChkptPathNode ** aChkptPathNode )
{
    smmChkptPathNode * sCPathNode;
    smuList          * sListNode;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aChkptPath != NULL );
    IDE_DASSERT( aChkptPathNode != NULL );
    
    * aChkptPathNode = NULL ;
    
    sListNode = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );

    while ( sListNode != & aTBSNode->mChkptPathBase )
    {
        sCPathNode = (smmChkptPathNode*)  sListNode->mData;
        
        if ( isSameChkptPath( sCPathNode,
                              aChkptPath ) == ID_TRUE )
        {
            * aChkptPathNode = sCPathNode;
            break;
        }
            
        sListNode = SMU_LIST_GET_NEXT( sListNode );
    }

    return IDE_SUCCESS;

}


/*
 * Checkpoint Path Node�� Ư�� Tablespace���� �����Ѵ�.
 *
 * aTBSNode       [IN] - Checkpoint Path Node�� ���ŵ� Tablespace
 * aChkptPathNode [IN] - ������ Checkpoint Path Node
 */  
IDE_RC smmTBSChkptPath::removeChkptPathNode(
                          smmTBSNode       * aTBSNode,
                          smmChkptPathNode * aChkptPathNode )
{
    smmChkptPathNode * sCPathNode;
    smuList          * sListNode;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aChkptPathNode != NULL );
    
    sListNode = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );

    // �ּ��� �ϳ��� Checkpoint Path�� �־�� �Ѵ�.
    IDE_DASSERT( sListNode != & aTBSNode->mChkptPathBase );

    while ( sListNode !=  & aTBSNode->mChkptPathBase )
    {
        sCPathNode = (smmChkptPathNode*)  sListNode->mData;
        
        if (  sCPathNode == aChkptPathNode )
        {

            IDE_TEST( destroyChkptPathNode( sCPathNode ) != IDE_SUCCESS );

            // Chkpt Path List ���� Node�� ���ŵ���
            // Checkpoint Path Count �����ϱ� ���� ������ �߻��ؼ��� �ȵȴ�
            IDE_ASSERT( iduMemMgr::free( sCPathNode ) == IDE_SUCCESS );
            
            SMU_LIST_DELETE( sListNode );

            IDE_ASSERT( iduMemMgr::free( sListNode ) == IDE_SUCCESS );

            break;
        }
            
        sListNode = SMU_LIST_GET_NEXT( sListNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*
 * Checkpoint Path Node�� Checkpoint Path�� �����Ѵ�.
 *
 * aChkptPathNode [IN] - ������ Checkpoint Path Node
 * aChkptPath     [IN] - ���� ������ Checkpoint Path ���ڿ� 
 */  
IDE_RC smmTBSChkptPath::renameChkptPathNode(
                          smmChkptPathNode * aChkptPathNode,
                          SChar            * aChkptPath )
{
    IDE_DASSERT( aChkptPathNode != NULL );
    IDE_DASSERT( aChkptPath != NULL );

   IDE_TEST(  setChkptPath( & aChkptPathNode->mChkptPathAttr,
                            aChkptPath )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*
 * Checkpoint Path Node�� ���� �����Ѵ�.
 *
 * [IN]  aTBSNode        - Checkpoint Path�� ���� Count�� Tablespace
 * [OUT] aChkptPathCount - Checkpoint Path�� �� 
 */  
IDE_RC smmTBSChkptPath::getChkptPathNodeCount( smmTBSNode * aTBSNode,
                                             UInt       * aChkptPathCount )
{
    smuList  * sListNode;
    UInt       sNodeCount = 0;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aChkptPathCount != NULL );
    
    sListNode = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );

    while ( sListNode != & aTBSNode->mChkptPathBase )
    {
        sNodeCount ++;
        sListNode = SMU_LIST_GET_NEXT( sListNode );
    }

    * aChkptPathCount = sNodeCount;

    return IDE_SUCCESS;

}

/*
   N��° Checkpoint Path Node�� �����Ѵ�.
   
   [IN]  aTBSNode   - Checkpoint Path�� �˻��� Tablespace Node
   [IN]  aIndex     - ���° Checkpoint Path�� �������� (0-based index)
   [OUT] aCPathNode - �˻��� Checkpoint Path Node
 */
IDE_RC smmTBSChkptPath::getChkptPathNodeByIndex(
                          smmTBSNode        * aTBSNode,
                          UInt                aIndex,
                          smmChkptPathNode ** aCPathNode )
{
    smuList  * sListNode;
    UInt       sNodeIndex = 0;

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aCPathNode != NULL );
    
    *aCPathNode = NULL;
    
    sListNode = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );

    while ( sListNode != & aTBSNode->mChkptPathBase )
    {
        if ( sNodeIndex == aIndex )
        {
            *aCPathNode = (smmChkptPathNode*) sListNode->mData;
            break;
        }
        sListNode = SMU_LIST_GET_NEXT( sListNode );
        sNodeIndex ++;
    }
    
    return IDE_SUCCESS;
}



/*
 * �ϳ� Ȥ�� �� �̻��� Checkpoint Path�� TBSNode���� �����Ѵ�.
 *
 * Checkpoint Path�� Tablespace�� �����ϴ� ��쿡�� �����Ѵ�.
 *
 * aTBSNode       [IN] - Checkpoint Path�� ���ŵ� Tabelspace�� Node
 * aChkptPathList [IN] - ������ Checkpoint Path�� List
 */
IDE_RC smmTBSChkptPath::removeChkptPathNodesIfExist(
                          smmTBSNode           * aTBSNode,
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
        // (e-020) �̹� Tablespace�� �����ϴ� Checkpoint Path�� ������ ����
        IDE_TEST( findChkptPathNode( aTBSNode,
                                     sChkptPath,
                                     & sCPathNode )
                  != IDE_SUCCESS );

        // Checkpoint Path�� �����ϴ� ��쿡�� ���� 
        if ( sCPathNode != NULL )
        {
            IDE_TEST( removeChkptPathNode( aTBSNode, sCPathNode )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
        
    return IDE_FAILURE;
}



/*
    Checkpoint Path�� ���� ���ɿ��� üũ 

    [IN] aChkptPath - ���� ���ɿ��θ� üũ�� Checkpoint Path
    
    [ �˰��� ]
      (010) NewPath�� ���� ���丮�̸� ����
      (020) NewPath�� ���丮�� �ƴ� �����̸� ����
      (030) NewPath�� read/write/execute permission�� ������ ����
 */
IDE_RC smmTBSChkptPath::checkAccess2ChkptPath( SChar * aChkptPath )
{
    DIR * sDir;
    IDE_DASSERT( aChkptPath != NULL );

    ////////////////////////////////////////////////////////////////////
    // (010) NewPath�� ���� ���丮�̸� ����
    IDE_TEST_RAISE( idf::access( aChkptPath, F_OK ) != 0,
                    err_path_not_found );

    ////////////////////////////////////////////////////////////////////
    // (020) NewPath�� ���丮�� �ƴ� �����̸� ����
    sDir = idf::opendir( aChkptPath );
    IDE_TEST_RAISE( sDir == NULL, err_path_is_not_directory );
    (void)idf::closedir(sDir);
    
    ////////////////////////////////////////////////////////////////////
    // (030) NewPath�� read/write/execute permission�� ������ ����
    IDE_TEST_RAISE( idf::access( aChkptPath, R_OK ) != 0,
                    err_path_no_read_perm );

    IDE_TEST_RAISE( idf::access( aChkptPath, W_OK ) != 0,
                    err_path_no_write_perm );
    
    IDE_TEST_RAISE( idf::access( aChkptPath, X_OK ) != 0,
                    err_path_no_exec_perm );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_path_not_found);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NOT_EXIST,
                                aChkptPath));
    }
    IDE_EXCEPTION(err_path_is_not_directory);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NOT_A_DIRECTORY,
                                aChkptPath));
    }
    IDE_EXCEPTION(err_path_no_read_perm);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NO_READ_PERMISSION,
                                aChkptPath));
    }
    IDE_EXCEPTION(err_path_no_write_perm);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NO_WRITE_PERMISSION,
                                aChkptPath));
    }
    IDE_EXCEPTION(err_path_no_exec_perm);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CPATH_NO_EXEC_PERMISSION,
                                aChkptPath));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
 * Tablespace�� �ٴܰ� ������ Media Phase�� ������ ȣ��ȴ�
 *
 * aTBSNode [IN] - ��� Checkpoint Path Node�� Free�� Tablespace
 */  
IDE_RC smmTBSChkptPath::freeAllChkptPathNode( smmTBSNode       * aTBSNode )
{
    smmChkptPathNode * sCPathNode;
    smuList          * sListNode;
    smuList          * sNextListNode;

    IDE_DASSERT( aTBSNode != NULL );
    
    sListNode = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );

    // Create Tablespace�� Undo�ÿ��� Checkpoint Path�� ���� ���� �ִ�
    // �Ʒ� ASSERT�� �ɾ�� �ȵȴ�.
    // IDE_DASSERT( sListNode != & aTBSNode->mChkptPathBase );

    while ( sListNode !=  & aTBSNode->mChkptPathBase )
    {
        sNextListNode = SMU_LIST_GET_NEXT( sListNode );
        
        sCPathNode = (smmChkptPathNode*)  sListNode->mData;
        
        IDE_TEST( destroyChkptPathNode( sCPathNode ) != IDE_SUCCESS );
        
        // Chkpt Path List ���� Node�� ���ŵ���
        // Checkpoint Path Count �����ϱ� ���� ������ �߻��ؼ��� �ȵȴ�
        IDE_ASSERT( iduMemMgr::free( sCPathNode ) == IDE_SUCCESS );
        
        SMU_LIST_DELETE( sListNode );
        
        IDE_ASSERT( iduMemMgr::free( sListNode ) == IDE_SUCCESS );
        
        sListNode = sNextListNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
