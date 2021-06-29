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

#ifndef _O_SMM_TBS_STARTUP_SHUTDOWN_H_
#define _O_SMM_TBS_STARTUP_SHUTDOWN_H_ 1

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
  Memory Tablespace�� �����Ͽ� Startup, Shutdown���� ó�������� �����Ѵ�.
 */
class smmTBSStartupShutdown
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSStartupShutdown();

    // Memory Tablespace �������� �ʱ�ȭ
    static IDE_RC initializeStatic();

    // Memory Tablespace�������� ����
    static IDE_RC destroyStatic();
    ////////////////////////////////////////////////////////////////////
    // �������̽� �Լ� ( SM�� �ٸ� ��⿡�� ȣ�� )
    ////////////////////////////////////////////////////////////////////
    // ��� Tablespace�� destroy�Ѵ�. ( Shutdown�� ȣ�� )
    static IDE_RC destroyAllTBSNode();
    

    ////////////////////////////////////////////////////////////////////
    // Server Startup�� Tablespace�ʱ�ȭ ���� �Լ�
    ////////////////////////////////////////////////////////////////////
    // Server startup�� Log Anchor�� Tablespace Attribute�� ��������
    // Tablespace Node�� �����Ѵ�.
    static IDE_RC loadTableSpaceNode(
                      smiTableSpaceAttr   * aTBSAttr,
                      UInt                  aAnchorOffset );


    // Checkpoint Image Attribute�� �ʱ�ȭ�Ѵ�.
    static IDE_RC initializeChkptImageAttr(
                      smmChkptImageAttr * aChkptImageAttr,
                      smLSN             * aMemRedoLSN,
                      UInt                aAnchorOffset );


    // Loganchor�κ��� �о���� Checkpoint Path Attribute�� Node�� �����Ѵ�.
    static IDE_RC createChkptPathNode( smiChkptPathAttr  * aChkptPathAttr,
                                       UInt                aAnchorOffset );


    // ��� Tablespace�� �ٴܰ� �ʱ�ȭ�� �����Ѵ�.
    static IDE_RC initFromStatePhase4AllTBS();
    
private :
    // smmTBSStartupShutdown::initFromStatePhase4AllTBS �� �׼� �Լ�
    static IDE_RC initFromStatePhaseAction( idvSQL            * aStatistics, 
                                            sctTableSpaceNode * aTBSNode,
                                            void * /* aActionArg */ );
    
};

#endif /* _O_SMM_TBS_STARTUP_SHUTDOWN_H_ */
