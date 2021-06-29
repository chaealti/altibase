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
 
#include <ide.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <svrLogMgr.h>
#include <svrRecoveryMgr.h>

/********************************************************************************
 * Description : �� Ʈ������� volatile TBS�� ���� ������ ���� �������
 *               ��� undo�Ѵ�. �׸��� �α� ����Ʈ�� savepoint���� ����.
 *               savepointLSN�� SVR_LSN_BEFORE_FIRST�̸� total rollback�Ѵ�.
 ********************************************************************************/
IDE_RC svrRecoveryMgr::undoTrans(svrLogEnv *aEnv,
                                 svrLSN     aSavepointLSN)
{
    svrLSN sCurLSN;

    sCurLSN = svrLogMgr::getLastLSN(aEnv);

    while (sCurLSN != aSavepointLSN)
    {
        IDE_ASSERT(sCurLSN != SVR_LSN_BEFORE_FIRST);
        IDE_TEST(undo(aEnv, sCurLSN, &sCurLSN) != IDE_SUCCESS);
    }

    /* undo �� �α׵��� ��� ������. */
    IDE_TEST(svrLogMgr::removeLogHereafter(aEnv, sCurLSN) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/********************************************************************************
 * Description : volatile tbs�� update ���꿡 ���� undo�� �����Ѵ�.
 *               �� �α׿� ���ѵȴ�. �׸��� private �Լ��̴�.
 ********************************************************************************/
IDE_RC svrRecoveryMgr::undo(svrLogEnv *aEnv,
                            svrLSN     aLSN,
                            svrLSN    *aUndoNextLSN)
{
    svrLog  *sLog;
    svrLSN   sSubLSN;

    IDE_TEST(svrLogMgr::readLog(aEnv,
                                aLSN,
                                (svrLog**)&sLog,
                                aUndoNextLSN,
                                &sSubLSN)
             != IDE_SUCCESS);

    IDE_ASSERT(sLog->mUndo != NULL);

    IDE_TEST(sLog->mUndo(aEnv, sLog, sSubLSN) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

