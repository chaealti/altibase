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
 * $Id: smmTableSpace.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_CHKPT_PATH_H_
#define _O_SMM_TBS_CHKPT_PATH_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <smmDef.h>
#include <smu.h>

/*
  [����] SMM�ȿ����� File���� Layer�� ������ ������ ����.
         ���� Layer�� �ڵ忡���� ���� Layer�� �ڵ带 ����� �� ����.

  ----------------------------------------------------------------------------
  smmTBSCreate          ; Create Tablespace ����
  smmTBSDrop            ; Drop Tablespace ����
  smmTBSAlterAutoExtend ; Alter Tablespace Auto Extend ����
  smmTBSAlterChkptPath  ; Alter Tablespace Add/Rename/Drop Checkpoint Path����
  smmTBSAlterDiscard    ; Alter Tablespace Discard ����
  smmTBSStartupShutdown ; Startup, Shutdown���� Tablespace���� ó���� ����
  ----------------------------------------------------------------------------
  smmTBSChkptPath  ; Tablespace�� Checkpoint Path ����
  smmTBSMultiPhase ; Tablespace�� �ٴܰ� �ʱ�ȭ
  ----------------------------------------------------------------------------
  smmManager       ; Tablespace�� ���� ���� 
  smmFPLManager    ; Tablespace Free Page List�� ���� ����
  smmExpandChunk   ; Chunk�� ���α��� ����
  ----------------------------------------------------------------------------
  
  c.f> Memory Tablespace�� Alter Online/Offline�� smp layer�� �����Ǿ� �ִ�.
*/

/*
   Memory Tablespace�� Checkpoint Path�� �߰�,����,������ �����Ѵ�.

   smmTBSAlterChkptPath���� �� Class�� ����Ͽ� Alter Checkpoint Path����� �����Ѵ�.
   
 */
class smmTBSChkptPath
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSChkptPath();
    
    // Loganchor�κ��� �о���� Checkpoint Path Attribute�� Node�� �����Ѵ�.
    static IDE_RC createChkptPathNode( smiChkptPathAttr  * aChkptPathAttr,
                                       UInt                aAnchorOffset );

    // Checkpoint Path Attribute�� Checkpoint Path�� �����Ѵ�.
    static IDE_RC setChkptPath( smiChkptPathAttr * aCPathAttr,
                                SChar            * aChkptPath );
    

    // Checkpoint Path Node�� ���� �����Ѵ�.
    static IDE_RC getChkptPathNodeCount( smmTBSNode * aTBSNode,
                                         UInt       * aChkptPathCount );

    // N��° Checkpoint Path Node�� �����Ѵ�.
    static IDE_RC getChkptPathNodeByIndex(
                      smmTBSNode        * aTBSNode,
                      UInt                aIndex,
                      smmChkptPathNode ** aCPathNode );


    // Checkpoint Path Node�� Ư�� Tablespace�� �߰��Ѵ�. 
    static IDE_RC addChkptPathNode(  smmTBSNode      * aTBSNode,
                                    smmChkptPathNode * aChkptPathNode );

    // Ư�� Tablespace���� Ư�� Checkpoint Path Node�� ã�´�.
    static IDE_RC findChkptPathNode(
                      smmTBSNode        * aTBSNode,
                      SChar             * aChkptPath,
                      smmChkptPathNode ** aChkptPathNode );

    // Checkpoint Path�� ���� ���ɿ��� üũ 
    static IDE_RC checkAccess2ChkptPath( SChar * aChkptPath );

    // Checkpoint Path Node�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
    static IDE_RC makeChkptPathNode( scSpaceID           aSpaceID,
                                     SChar             * aChkptPath,
                                     smmChkptPathNode ** aCPathNode );


    // Checkpoint Path Node�� Checkpoint Path�� �����Ѵ�.
    static IDE_RC renameChkptPathNode(
                      smmChkptPathNode * aChkptPathNode,
                      SChar            * aChkptPath );


    // Checkpoint Path Node�� Ư�� Tablespace���� �����Ѵ�.
    static IDE_RC removeChkptPathNode(
                      smmTBSNode       * aTBSNode,
                      smmChkptPathNode * aChkptPathNode );
    

    // �ϳ� Ȥ�� �� �̻��� Checkpoint Path�� TBSNode���� �����Ѵ�.
    static IDE_RC removeChkptPathNodesIfExist(
                      smmTBSNode           * aTBSNode,
                      smiChkptPathAttrList * aChkptPathList );


    // Tablespace�� �ٴܰ� ������ Media Phase�� ������ ȣ��ȴ�
    static IDE_RC freeAllChkptPathNode( smmTBSNode * aTBSNode );
    
private :


    // Checkpoint Path Attribute�� �ʱ�ȭ�Ѵ�.
    static IDE_RC initializeChkptPathAttr(
                      smiChkptPathAttr * aCPathAttr,
                      scSpaceID          aSpaceID,
                      SChar            * aChkptPath );

    // Checkpoint Path Attribute�� �ʱ�ȭ�Ѵ�.
    static IDE_RC initializeChkptPathNode(
                      smmChkptPathNode * aCPathNode,
                      scSpaceID          aSpaceID,
                      SChar            * aChkptPath );

    // Checkpoint Path Node�� �ı��Ѵ�.
    static IDE_RC destroyChkptPathNode(smmChkptPathNode * aCPathNode );
    
    // Checkpoint Path Node�� Checkpoint Path ���ڿ��� ���Ͽ�
    // ���� Checkpoint Path���� üũ�Ѵ�.
    static idBool isSameChkptPath(smmChkptPathNode * aCPathNode,
                                  SChar            * aChkptPath);
    

    
};

#endif /* _O_SMM_TBS_CHKPT_PATH_H_ */
