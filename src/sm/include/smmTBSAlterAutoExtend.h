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
 * $Id: smmTBSAlterAutoExtend.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_ALTER_AUTO_EXTEND_H_
#define _O_SMM_TBS_ALTER_AUTO_EXTEND_H_ 1

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
   Memory Tablespace�� Alter Tablespace Auto Extend ����
 */
class smmTBSAlterAutoExtend
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSAlterAutoExtend();

    // ALTER TABLESPACE AUTOEXTEND ... �� ���� �ǽ� 
    static IDE_RC alterTBSsetAutoExtend(void      * aTrans,
                                        scSpaceID   aTableSpaceID,
                                        idBool      aAutoExtendMode,
                                        ULong       aNextSize,
                                        ULong       aMaxSize );

private :
    
    // ALTER TABLESPACE AUTOEXTEND ... �� ���� ����ó��
    static IDE_RC checkErrorOnAutoExtendAttrs( smmTBSNode * aTBSNode,
                                               idBool       aAutoExtendMode,
                                               ULong        aNextSize,
                                               ULong        aMaxSize );
    
    //  Next Page Count �� Max Page Count�� ����Ѵ�.
    static IDE_RC calcAutoExtendAttrs(
                      smmTBSNode * aTBSNode,
                      idBool       aAutoExtendMode,
                      ULong        aNextSize,
                      ULong        aMaxSize,
                      scPageID   * aNextPageCount,
                      scPageID   * aMaxPageCount );
};

#endif /* _O_SMM_TBS_ALTER_AUTO_EXTEND_H_ */
