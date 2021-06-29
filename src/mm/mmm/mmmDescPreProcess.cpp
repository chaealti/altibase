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

#include <mmm.h>
#include <mmmActList.h>


/* =======================================================
 *  Action Descriptor
 * =====================================================*/
static mmmPhaseAction *gPreProcessActions[] =
{
    &gMmmActBeginPreProcess,

    &gMmmActCheckShellEnv,

    &gMmmActInitBootMsgLog,

    &gMmmActLogRLimit,

    &gMmmActProperty,

    &gMmmActCheckLicense,

    &gMmmActSignal,

    /* 
     * BUG-32751
     * Memory Manager���� �� �̻� Mutex�� ������� ������
     * iduMemPool���� server_malloc�� ����ϱ� ������ ��ġ�� �����մϴ�.
     */
    &gMmmActInitMemoryMgr,

    &gMmmActCheckIdeTSD, // Ide Layer �ʱ�ȭ �˻�, mutex, latch �ʱ�ȭ

    &gMmmActInitMemPoolMgr,

    &gMmmActLoadMsb,

    &gMmmActInitModuleMsgLog,


    &gMmmActCallback,

    &gMmmActVersionInfo,
#if !defined(ANDROID) && !defined(SYMBIAN)
    &gMmmActSystemUpTime,
#endif
    &gMmmActDaemon,
    &gMmmActThread,
    &gMmmActPIDInfo,
    &gMmmActInitLockFile,    // Daemonize ���Ŀ� ����ϵ��� ��.

    /* TASK-6780 */
    &gMmmActInitRBHash,

//     &gMmmActInitGPKI,
    &gMmmActInitSM, // todo
    &gMmmActInitQP, // todo
    &gMmmActInitMT,
    &gMmmActFixedTable,
    &gMmmActTimer,
    &gMmmActInitCM,       // Initialize CM
    &gMmmActInitDK,
    &gMmmActInitThreadManager,
    &gMmmActInitOS,
    // &gMmmActInitNLS,
    &gMmmActInitQueryProfile,
    &gMmmActInitSD,
    &gMmmActInitREPL,
    &gMmmActInitService,

    &gMmmActEndPreProcess,
    NULL /* should exist!! */
};

/* =======================================================
 *  Process Phase Descriptor
 * =====================================================*/
mmmPhaseDesc gMmmGoToPreProcess =
{
    MMM_STARTUP_PRE_PROCESS,
    (SChar *)"PRE-PROCESSING",
    gPreProcessActions
};
