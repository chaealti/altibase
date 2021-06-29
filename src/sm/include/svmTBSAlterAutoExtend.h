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
 
#ifndef _O_SVM_TBS_ALTER_AUTO_EXTEND_H_
#define _O_SVM_TBS_ALTER_AUTO_EXTEND_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <svmDef.h>
#include <smu.h>


/*
  [Volatile] Alter Tablespace Auto Extend ����

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
class svmTBSAlterAutoExtend
{
public :
    // ������ (�ƹ��͵� ����)
    svmTBSAlterAutoExtend();

    // ALTER TABLESPACE AUTOEXTEND ... �� ���� �ǽ� 
    static IDE_RC alterTBSsetAutoExtend(void      * aTrans,
                                        scSpaceID   aTableSpaceID,
                                        idBool      aAutoExtendMode,
                                        ULong       aNextSize,
                                        ULong       aMaxSize );
    
private :
    // ALTER TABLESPACE AUTOEXTEND ... �� ���� ����ó��
    static IDE_RC checkErrorOnAutoExtendAttrs( svmTBSNode * aTBSNode,
                                               idBool       aAutoExtendMode,
                                               ULong        aNextSize,
                                               ULong        aMaxSize );
    
    //  Next Page Count �� Max Page Count�� ����Ѵ�.
    static IDE_RC calcAutoExtendAttrs(
                      svmTBSNode * aTBSNode,
                      idBool       aAutoExtendMode,
                      ULong        aNextSize,
                      ULong        aMaxSize,
                      scPageID   * aNextPageCount,
                      scPageID   * aMaxPageCount );
};

#endif /* _O_SVM_TBS_ALTER_AUTO_EXTEND_H_ */
