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
 
#ifndef _O_SVM_TBS_STARTUP_SHUTDOWN_H_
#define _O_SVM_TBS_STARTUP_SHUTDOWN_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <svmDef.h>
#include <smu.h>

/*

  [Volatile]  Startup, Shutdown���� Tablespace���� ó���� ����
  
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
class svmTBSStartupShutdown
{
public :
    // ������ (�ƹ��͵� ����)
    svmTBSStartupShutdown();

    // Volatile Tablespace �������� �ʱ�ȭ
    static IDE_RC initializeStatic();
    
    // Volatile Tablespace�������� ����
    static IDE_RC destroyStatic();
    

    // Log anchor�� ���� TBSAttr�κ��� TBSNode�� �����Ѵ�.
    static IDE_RC loadTableSpaceNode(smiTableSpaceAttr *aTBSAttr,
                                     UInt               aAnchorOffset);

    // ��� Volatile TBS�� �ʱ�ȭ�Ѵ�.
    // TBSNode���� �̹� smrLogAchorMgr���� �ʱ�ȭ�� �����̾�� �Ѵ�.
    static IDE_RC prepareAllTBS();

    // ��� Volatile TBSNode���� �����Ѵ�.
    static IDE_RC destroyAllTBSNode();

private :

    // �ϳ��� Volatile TBS�� �ʱ�ȭ�Ѵ�.
    static IDE_RC prepareTBSAction(idvSQL*            aStatistics,
                                   sctTableSpaceNode *aTBSNode,
                                   void              */* aArg */);
};

#endif /* _O_SVM_TBS_STARTUP_SHUTDOWN_H_ */
