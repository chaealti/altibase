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

#include <idl.h>
#include <smi.h>
#include <mmm.h>
#include <sti.h>


static IDE_RC mmmPhaseActionInitSM(mmmPhase        aPhase,
                                   UInt            aOptionflag,
                                   mmmPhaseAction * /*aAction*/)
{
    smiStartupPhase sStartupPhase = SMI_STARTUP_MAX;

    switch(aPhase)
    {
        case MMM_STARTUP_PRE_PROCESS:
            sStartupPhase = SMI_STARTUP_PRE_PROCESS;
            break;
        case MMM_STARTUP_PROCESS:
            sStartupPhase = SMI_STARTUP_PROCESS;
            break;
        case MMM_STARTUP_CONTROL:
            sStartupPhase = SMI_STARTUP_CONTROL;
            break;
        case MMM_STARTUP_META:
            sStartupPhase = SMI_STARTUP_META;
              break;
        case MMM_STARTUP_DOWNGRADE: /* PROJ-2689 */
        case MMM_STARTUP_SERVICE:
            sStartupPhase = SMI_STARTUP_SERVICE;
            break;
        case MMM_STARTUP_SHUTDOWN:
        default:
            IDE_CALLBACK_FATAL("invalid startup phase");
            break;
    }

    IDE_TEST(smiStartup(sStartupPhase,aOptionflag, &mmm::mSmiGlobalCallBackList) != IDE_SUCCESS);

    /* PROJ-1488 Altibase Spatio-Temporal DBMS */
    /* R-Tree �� 3DR-Tree Index �߰� */
    if ( sStartupPhase == SMI_STARTUP_PRE_PROCESS )
    {
        IDE_TEST( sti::addExtSM_Index() != IDE_SUCCESS );
    }
    else
    {
        // ���� �ѹ��� ����Ѵ�.
    }

    /* BUG-25279 Btree for spatial�� Disk Btree�� �ڷᱸ�� �� �α� �и� 
     *  �� ������Ʈ�� ���� Disk Index�� ���屸���� ����Ǳ� ������
     * Btree for spatial, �� Disk Rtree Index�� Disk Btree index��
     * ���� ���� �� �α��� �����Ǿ�� �Ѵ�.
     *
     *  �̿� ���� Disk Rtree�� ���� Recovery �Լ����� st ���� �̵�
     * �Ǹ� sm������ st����� Recovery �Լ��� ����ϱ� ���� �ܺθ��
     * �� �����Ѵ�.
     *
     *  ���� �ܺθ��� Recovery�Լ��� �����ϴ� ���� Control�ܰ迡��
     * �����Ѵ�. �ֳ��ϸ� Control ���Ժο��� Redo - Undo Map�� �ʱ�ȭ
     * �ϱ� �����̴�.          */
    if ( sStartupPhase == SMI_STARTUP_CONTROL )
    {
        IDE_TEST( sti::addExtSM_Recovery() != IDE_SUCCESS );
    }
    else
    {
        // ���� �ѹ��� ����Ѵ�.
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* =======================================================
 *  Action Func Object for Action Descriptor
 * =====================================================*/

mmmPhaseAction gMmmActInitSM =
{
    (SChar *)"Initialize Storage Manager",
    0,
    mmmPhaseActionInitSM
};

