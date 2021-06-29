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
 
#ifndef _O_SVM_TBS_CREATE_H_
#define _O_SVM_TBS_CREATE_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <svmDef.h>
#include <smu.h>


#define SVM_CNO_NONE             (0)
#define SVM_CNO_X_LOCK_TBS_NODE  (0x01)


/*
  [Volatile] Create Tablespace�� ����
  
  ����> svm ��� ���� �ҽ��� ������ ���� Layering�Ǿ� �ִ�.
  ----------------------------------------------------------------------------
  svmTBSCreate          ; Create Tablespace ����
  svmTBSDrop            ; Drop Tablespace ����
  svmTBSAlterAutoExtend ; Alter Tablespace Auto Extend ����
  svmTBSStartupShutdown ; Startup, Shutdown���� Tablespace���� ó���� ����
  ----------------------------------------------------------------------------
  svmManager       ; Tablespace�� ���� ���� 
  svmFPLManager    ; Tablespace Free Page List�� ���� ����
  svmExpandChunk   ; Chunk�� ���α��� ����
  ----------------------------------------------------------------------------
 */
class svmTBSCreate
{
public :
    // ������ (�ƹ��͵� ����)
    svmTBSCreate();

    // ����� ���̺� �����̽��� �����Ѵ�.
    static IDE_RC createTBS( void                 * aTrans,
                             SChar                * aDBName,
                             SChar                * aTBSName,
                             smiTableSpaceType      aType,
                             UInt                   aAttrFlag,
                             ULong                  aInitSize,
                             idBool                 aIsAutoExtend,
                             ULong                  aNextSize,
                             ULong                  aMaxSize,
                             UInt                   aState,
                             scSpaceID            * aSpaceID );

    // Tablespace�� Create�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC createTableSpacePending( idvSQL*             aStatistics,
                                           sctTableSpaceNode * aSpaceNode,
                                           sctPendingOp      * /*aPendingOp*/ );
    
private :
    
    // �����ͺ��̽��� �����Ѵ�.
    static IDE_RC createTableSpaceInternal(
                      void                 * aTrans,
                      SChar                * aDBName,
                      smiTableSpaceAttr    * aTBSAttr,
                      scPageID               aCreatePageCount,
                      svmTBSNode          ** aCreatedTBSNode );
    
    
    // TBSNode�� �����Ͽ� sctTableSpaceMgr�� ����Ѵ�.
    static IDE_RC createAndLockTBSNode(void              *aTrans,
                                       smiTableSpaceAttr *aTBSAttr);

    // Tablespace Node�� Log Anchor�� flush�Ѵ�.
    static IDE_RC flushTBSNode(svmTBSNode * aTBSNode);

    //  Tablespace Attribute�� �ʱ�ȭ �Ѵ�.
    static IDE_RC initializeTBSAttr( smiTableSpaceAttr    * aTBSAttr,
                                     smiTableSpaceType      aType,
                                     SChar                * aName,
                                     UInt                   aAttrFlag,
                                     ULong                  aInitSize,
                                     UInt                   aState);
};

#endif /* _O_SVM_TBS_CREATE_H_ */
