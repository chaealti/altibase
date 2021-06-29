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
 
#ifndef _O_SVM_TBS_DROP_H_
#define _O_SVM_TBS_DROP_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <svmDef.h>
#include <smu.h>



/*
  [Volatile] Drop Tablespace�� ����
  
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
class svmTBSDrop
{
public :
    // ������ (�ƹ��͵� ����)
    svmTBSDrop();

        
    // ����� ���̺� �����̽��� DROP�Ѵ�.
    static IDE_RC dropTBS(void       * aTrans,
                          svmTBSNode * aTBSNode);
    
    // �޸� ���̺� �����̽��� DROP�Ѵ�.
    static IDE_RC dropTableSpace(void       * aTrans,
                                 svmTBSNode * aTBSNode );

    // Tablespace�� DROP�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC dropTableSpacePending( idvSQL*             aStatistics,
                                         sctTableSpaceNode * aTBSNode,
                                         sctPendingOp      * aPendingOp );


private :
    // Tablespace�� ���¸� DROPPED�� �����ϰ� Log Anchor�� Flush!
    static IDE_RC flushTBSStatusDropped( sctTableSpaceNode * aSpaceNode );

};

#endif /* _O_SVM_TBS_DROP_H_ */
