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
 * $Id: smmTBSDrop.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_DROP_H_
#define _O_SMM_TBS_DROP_H_ 1

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
  Drop Memory Tablespace ����
 */
class smmTBSDrop
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSDrop();

         
    // ����� ���̺� �����̽��� DROP�Ѵ�.
    static IDE_RC dropTBS(void            * aTrans,
                          smmTBSNode      * aTBSNode,
                          smiTouchMode      aTouchMode);

public :
    // Tablespace�� DROP�� Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC dropTableSpacePending( idvSQL            * aStatistics, 
                                         sctTableSpaceNode * aTBSNode,
                                         sctPendingOp      * aPendingOp );
    // ���� Drop Tablespace���� �ڵ� 
    static IDE_RC doDropTableSpace( sctTableSpaceNode * aTBSNode,
                                    smiTouchMode        aTouchMode );

    // Tablespace�� ���¸� DROPPED�� �����ϰ� Log Anchor�� Flush!
    static IDE_RC flushTBSStatusDropped( sctTableSpaceNode * aTBSNode );
    
private :

    //Tablespace�� ���¸� �����ϰ� Log Anchor�� Flush�Ѵ�.
    static IDE_RC flushTBSStatus( sctTableSpaceNode * aSpaceNode,
                                  UInt                aNewSpaceState);
    
    // �޸� ���̺� �����̽��� DROP�Ѵ�.
    static IDE_RC dropTableSpace(void       * aTrans,
                                 smmTBSNode * aTBSNode,
                                 smiTouchMode   aTouchMode);

};

#endif /* _O_SMM_TBS_DROP_H_ */
