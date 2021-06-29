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
 * $Id: smmTBSCreate.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_CREATE_H_
#define _O_SMM_TBS_CREATE_H_ 1

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
   Create Memory Tablespace ����
 */
class smmTBSCreate
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSCreate();

    ////////////////////////////////////////////////////////////////////
    // �������̽� �Լ� ( smiTableSpace���� �ٷ� ȣ�� )
    ////////////////////////////////////////////////////////////////////
    // PROJ-1923 ALTIBASE HDB Disaster Recovery
    // ����� ���̺� �����̽��� �����Ѵ�.
    static IDE_RC createTBS4Redo( void                 * aTrans,
                                  smiTableSpaceAttr    * aTBSAttr,
                                  smiChkptPathAttrList * aChkptPathList );
 
    // ����� ���̺� �����̽��� �����Ѵ�.
    static IDE_RC createTBS( void                 * aTrans,
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
                             scSpaceID            * aTBSID );
    
public :
    // Tablespace�� Create�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC createTableSpacePending( idvSQL            * aStatistics, 
                                           sctTableSpaceNode * aTBSNode,
                                           sctPendingOp      * aPendingOp );
    
private :
    // ����ڰ� Tablespace������ ���� �ɼ��� �������� ���� ���
    // �⺻���� �����ϴ� �Լ�
    static IDE_RC makeDefaultArguments( ULong  * aSplitFileSize,
                                        ULong  * aInitSize);
    
    //  Tablespace Attribute�� �ʱ�ȭ �Ѵ�.
    static IDE_RC initializeTBSAttr( smiTableSpaceAttr    * aTBSAttr,
                                     scSpaceID              aSpaceID,
                                     smiTableSpaceType      aType,
                                     SChar                * aName,
                                     UInt                   aAttrFlag,
                                     ULong                  aSplitFileSize,
                                     ULong                  aInitSize);
    

    // Tablespace Attribute�� ���� ����üũ�� �ǽ��Ѵ�.
    static IDE_RC checkErrorOfTBSAttr( SChar * aTBSName, 
                                       ULong   aSplitFileSize,
                                       ULong   aInitSize);

    // ����ڰ� ������, Ȥ�� �ý����� �⺻ Checkpoint Path��
    // Tablespace�� �߰��Ѵ�.
    static IDE_RC createDefaultOrUserChkptPaths(
                      smmTBSNode           * aTBSNode,
                      smiChkptPathAttrList * aChkptPathAttrList );
    

    // �ϳ� Ȥ�� �� �̻��� Checkpoint Path�� TBSNode�� Tail�� �߰��Ѵ�.
    static IDE_RC createChkptPathNodes( smmTBSNode           * aTBSNode,
                                        smiChkptPathAttrList * aChkptPathList );
    
    
    // ���ο�  Tablespace�� ����� Tablespace ID�� �Ҵ�޴´�.
    static IDE_RC allocNewTBSID( scSpaceID * aSpaceID );

    // Tablespace�� �����ϰ� ������ �Ϸ�Ǹ� NTA�� ���´�.
    static IDE_RC createTBSWithNTA4Redo(
                    void                  * aTrans,
                    smiTableSpaceAttr     * aTBSAttr,
                    smiChkptPathAttrList  * aChkptPathList );
    
    // Tablespace�� �����ϰ� ������ �Ϸ�Ǹ� NTA�� ���´�.
    static IDE_RC createTBSWithNTA(
                      void                  * aTrans,
                      SChar                 * aDBName,
                      smiTableSpaceAttr     * aTBSAttr,
                      smiChkptPathAttrList  * aChkptPathList,
                      scPageID                aInitPageCount,
                      SChar                 * aDBCharSet,
                      SChar                 * aNationalCharSet,
                      smmTBSNode           ** aCreatedTBSNode);

    // Tablespace�� �ý��ۿ� ����ϰ� ���� �ڷᱸ���� �����Ѵ�.
    static IDE_RC createTBSInternal4Redo(
                      void                 * aTrans,
                      smmTBSNode           * aTBSNode,
                      smiChkptPathAttrList * aChkptPathAttrList );
   
    // Tablespace�� �ý��ۿ� ����ϰ� ���� �ڷᱸ���� �����Ѵ�.
    static IDE_RC createTBSInternal(
                      void                 * aTrans,
                      smmTBSNode           * aTBSNode,
                      SChar                * aDBName,
                      smiChkptPathAttrList * aChkptPathAttrList,
                      scPageID               aInitPageCount,
                      SChar                * aDBCharSet,
                      SChar                * aNationalCharSet);

    // smmTBSNode�� �����ϰ� X Lock�� ȹ���� ��
    // sctTableSpaceMgr�� ����Ѵ�.
    static IDE_RC allocAndInitTBSNode(
                      void                * aTrans,
                      smiTableSpaceAttr   * aTBSAttr,
                      smmTBSNode         ** aCreatedTBSNode );

    // Tablespace Node�� �ٴܰ� ���������� free�Ѵ�.
    static IDE_RC finiAndFreeTBSNode( smmTBSNode * aTBSNode );
    
    // �ٸ� Transaction���� Tablespace�̸����� Tablespace�� ã�� �� �ֵ���
    // �ý��ۿ� ����Ѵ�.
    static IDE_RC registerTBS( smmTBSNode * aTBSNode );
    

    // Tablespace Node�� �׿� ���� Checkpoint Path Node���� ���
    // Log Anchor�� Flush�Ѵ�.
    static IDE_RC flushTBSAndCPaths(smmTBSNode * aTBSNode);
    

};

#endif /* _O_SMM_TBS_CREATE_H_ */
 
