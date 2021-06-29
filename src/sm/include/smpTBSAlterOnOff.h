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
 * $Id: smpTBSAlterOnOff.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMP_TBS_ALTER_ONOFF_H_
#define _O_SMP_TBS_ALTER_ONOFF_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <smmDef.h>

/*
   Memory DB�� Tablespace���� ������� ������ class
   
   - Alter Tablespace Offline
   - Alter Tablespace Online
 */
class smpTBSAlterOnOff
{
public :
    // ������ (�ƹ��͵� ����)
    smpTBSAlterOnOff();

    ////////////////////////////////////////////////////////////////////
    // �������̽� �Լ� ( smiTableSpace���� �ٷ� ȣ�� )
    ////////////////////////////////////////////////////////////////////

    //Memory Tablespace�� ���� Alter Tablespace Online/Offline�� ����
    static IDE_RC alterTBSStatus( void       * aTrans,
                                  smmTBSNode * aTBSNode,
                                  UInt         aState );

    ////////////////////////////////////////////////////////////////////
    // Restart Recovery�� ȣ��Ǵ� �Լ�
    ////////////////////////////////////////////////////////////////////
    // Restart REDO, UNDO���� �Ŀ� Offline ������
    // Tablespace�� �޸𸮸� ����
    static IDE_RC finiOfflineTBS();


    ////////////////////////////////////////////////////////////////////
    // Alter Tablespace Online/Offline �����Լ�
    ////////////////////////////////////////////////////////////////////
    // Tablespace�� OFFLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC alterOfflineCommitPending(
                      idvSQL*             aStatistics, 
                      sctTableSpaceNode * aTBSNode,
                      sctPendingOp      * aPendingOp );

    // Tablespace�� ONLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC alterOnlineCommitPending(
                      idvSQL*             aStatistics, 
                      sctTableSpaceNode * aTBSNode,
                      sctPendingOp      * aPendingOp );

    
    
private:
    ////////////////////////////////////////////////////////////////////
    // Alter Tablespace Online/Offline �����Լ�
    ////////////////////////////////////////////////////////////////////
    
    // META, SERVICE�ܰ迡�� Tablespace�� Offline���·� �����Ѵ�.
    static IDE_RC alterTBSoffline(void       * aTrans,
                                  smmTBSNode * aTBSNode );


    // META/SERVICE�ܰ迡�� Tablespace�� Online���·� �����Ѵ�.
    static IDE_RC alterTBSonline(void       * aTrans,
                                 smmTBSNode * aTBSNode );


    ////////////////////////////////////////////////////////////////////
    // Restart Recovery�� ȣ��Ǵ� �Լ�
    ////////////////////////////////////////////////////////////////////
    // finiOfflineTBS�� ���� Action�Լ�
    static IDE_RC finiOfflineTBSAction( idvSQL            * aStatistics,
                                        sctTableSpaceNode * aTBSNode,
                                        void * /* aActionArg */ );
    
    
    ////////////////////////////////////////////////////////////////////
    // Alter Tablespace Online/Offline �� ���������� ����ϴ� �Լ�
    ////////////////////////////////////////////////////////////////////

    // Alter Tablespace Online Offline �α׸� ��� 
    static IDE_RC writeAlterTBSStateLog(
                      void                 * aTrans,
                      smmTBSNode           * aTBSNode,
                      sctUpdateType          aUpdateType,
                      smiTableSpaceState     aStateToRemove,
                      smiTableSpaceState     aStateToAdd,
                      UInt                 * aNewTBSState );
};

#endif /* _O_SMP_TBS_ALTER_ONOFF_H_ */
