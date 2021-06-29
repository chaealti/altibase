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
 * $Id: smmTBSPhase.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_MULTI_PHASE_H_
#define _O_SMM_TBS_MULTI_PHASE_H_ 1

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
    smmTBSMultiPhase ; Tablespace�� �ٴܰ� �ʱ�ȭ ����

    ------------------------------------------------------------------------
    Tablespace�� �ٴܰ� �ʱ�ȭ/�ı�
    ------------------------------------------------------------------------   
    
    Tablespace�� ���¿� ���� ������ ���� �ٴܰ��
    �ʱ�ȭ�� �����Ѵ�.

    ���� �ܰ���� �ʱ�ȭ�� ���ؼ��� ���� �ܰ��� �ʱ�ȭ�� �ʼ����̴�.
    (ex> 2. MEDIA�ܰ�� �ʱ�ȭ�� ���ؼ��� 1.STATE�ܰ��� �ʱ�ȭ�� �ʼ�)
    
    1. STATE  - State Phase
       ������ ���� :
         - Tablespace�� State�� ���� ����
         - ���ü� ��� ���� Lock�� Mutex�� ��밡��
         
       �ʱ�ȭ :
         - Tablespace�� Attribute�ʱ�ȭ
         - Tablespace�� Lock, Mutex�ʱ�ȭ
       
    2. MEDIA  - Media Phase
       ������ ���� :
         - Tablespace�� DB File ���Ͽ� �б�/���� ����
         
       �ʱ�ȭ :
         - DB File ��ü �ʱ�ȭ, Open
    
    3. PAGE   - Page Phase
       ������ ���� :
         - Prepare/Restore (DB File => Page Memory���� ������ �ε� )
         - Tablespace�� Page ����/�Ҵ�/�ݳ� ����
         
       �ʱ�ȭ :
         - PCH* Array�Ҵ�
         - Page Memory Pool�Ҵ�
         - Free Page List, Chunk ������ �ʱ�ȭ

    ------------------------------------------------------------------------
    Tablespace State�� �ʱ�ȭ �ܰ�
    ------------------------------------------------------------------------   
    A. ONLINE
       �ʱ�ȭ �ܰ� : PAGE
       ���� : Page�� �Ҵ�� �ݳ�, Page Data�� ������ �����ؾ���
       
    B. OFFLINE
       �ʱ�ȭ �ܰ� : MEDIA
       ���� : Checkpoint���� DB File�� Header�� RedoLSN�� �����ؾ���
       
    C. DISCARDED
       �ʱ�ȭ �ܰ� : MEDIA
       ���� : ���� Transaction�� ���ÿ� DISCARD�� Tablespace�� ���ٰ���
               Discard�� Tablespace�� Drop Tablespace�� ������
               ��� DML/DDL�� ������ �����ǹǷ�,
               Tablespace�� ���¸� ��ȸ�� �� ������ �ȴ�.
               
               Drop Tablespace�� �����ϱ� ������, DB File��ü�� ������
               �����ؾ� �ϸ�, �̸� ���� MEDIA�ܰ���� �ʱ�ȭ��
               �Ǿ��־�� �Ѵ�.
       
    D. DROPPED
       �ʱ�ȭ �ܰ� : STATE
       ���� : ���� Transaction�� ���ÿ� DROP�� Tablespace�� ���ٰ���
               Drop�� Tablespace�� ��� DML/DDL�� ������ �����ǹǷ�,
               Tablespace�� ���¸� ��ȸ�� �� ������ �ȴ�.
    

    ------------------------------------------------------------------------   
    Meta/Service���¿����� Tablespace Node�� ��������
    ------------------------------------------------------------------------   
     - ONLINE => OFFLINE
        [ PAGE => MEDIA ] finiPAGE
       
     - OFFLINE => ONLINE
        [ MEDIA => PAGE ] initPAGE
       
     - ONLINE => DROPPED
        [ PAGE => STATE ] finiPAGE, finiMEDIA

     - OFFLINE => DROPPED
        [ STATE => PAGE ] initMEDIA, initPAGE
    ------------------------------------------------------------------------   
    Control���¿����� Tablespace Node�� ��������
     - ONLINE => DISCARDED
        [ PAGE => STATE ] finiPAGE, finiMEDIA
       
     - OFFLINE => DISCARDED
        [ STATE => PAGE ] initMEDIA, initPAGE
               
 */



class smmTBSMultiPhase
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSMultiPhase();

    
    // STATE Phase �κ��� Tablespace�� ���¿� ����
    // �ʿ��� �ܰԱ��� �ٴܰ� �ʱ�ȭ
    static IDE_RC initTBS( smmTBSNode * aTBSNode,
                           smiTableSpaceAttr * aTBSAttr );
    
    // Server Shutdown�ÿ� Tablespace�� ���¿� ����
    // Tablespace�� �ʱ�ȭ�� �ܰ���� �ı��Ѵ�.
    static IDE_RC finiTBS( smmTBSNode * aTBSNode );
    
    // PAGE, MEDIA Phase�κ��� STATE Phase �� ����
    static IDE_RC finiToStatePhase( smmTBSNode * aTBSNode );

    // �ٴܰ� ������ ���� MEDIA�ܰ�� ����
    static IDE_RC finiToMediaPhase( UInt         aTBSState,
                                    smmTBSNode * aTBSNode );
    
    // Tablespace�� STATE�ܰ踦 �ʱ�ȭ
    static IDE_RC initStatePhase( smmTBSNode * aTBSNode,
                                  smiTableSpaceAttr * aTBSAttr );

    // STATE�ܰ���� �ʱ�ȭ�� Tablespace�� �ٴܰ��ʱ�ȭ
    static IDE_RC initFromStatePhase( smmTBSNode * aTBSNode );
    
    // Tablespace�� MEDIA�ܰ踦 �ʱ�ȭ
    static IDE_RC initMediaPhase( smmTBSNode * aTBSNode );

    // Tablespace�� PAGE�ܰ踦 �ʱ�ȭ
    static IDE_RC initPagePhase( smmTBSNode * aTBSNode );

    // Tablespace�� STATE�ܰ踦 �ı�
    static IDE_RC finiStatePhase( smmTBSNode * aTBSNode );

    // Tablespace�� MEDIA�ܰ踦 �ı�
    static IDE_RC finiMediaPhase( smmTBSNode * aTBSNode );

    // Tablespace�� PAGE�ܰ踦 �ı�
    static IDE_RC finiPagePhase( smmTBSNode * aTBSNode );
};

#endif /* _O_SMM_TBS_MULTI_PHASE_H_ */
