/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <checkServerPid.h>



/**
 * PID ������ �����.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerPidFileCreate (CheckServerHandle *aHandle)
{
    pid_t       sPid;
    FILE        *sFP;
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    sFP = idlOS::fopen(aHandle->mPidFilePath, "w");
    IDE_TEST_RAISE(sFP == NULL, fopen_error);

    sPid = idlOS::getpid();
    sRC = fwrite(&sPid, sizeof(pid_t), 1, sFP);
    IDE_TEST_RAISE(sRC != 1, fwrite_error);

    sRC = idlOS::fclose(sFP);
    IDE_TEST_RAISE(sRC != 0, fclose_error);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(fopen_error);
    {
        sRC = ALTIBASE_CS_FOPEN_ERROR;
    }
    IDE_EXCEPTION(fwrite_error);
    {
        sRC = ALTIBASE_CS_FWRITE_ERROR;
    }
    IDE_EXCEPTION(fclose_error);
    {
        sRC = ALTIBASE_CS_FCLOSE_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * PID ������ �ִ��� Ȯ���Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN,OUT] aExist PID ������ ������ ID_TRUE, �ƴϸ� ID_FALSE
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerPidFileExist (CheckServerHandle *aHandle, idBool *aExist)
{
    FILE        *sFP;
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);
    IDE_ASSERT(aExist != NULL);

    sFP = idlOS::fopen(aHandle->mPidFilePath, "r");
    if (sFP == NULL)
    {
        /* path�� �߸��ưų� ������ ��� ������ ��(ENOENT)�� �ƴϸ� ���� */
        IDE_TEST_RAISE(errno != ENOENT, fopen_error);

        *aExist = ID_FALSE;
    }
    else
    {
        *aExist = ID_TRUE;

        sRC = idlOS::fclose(sFP);
        IDE_TEST_RAISE(sRC != 0, fclose_error);
    }

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(fopen_error);
    {
        sRC = ALTIBASE_CS_FOPEN_ERROR;
    }
    IDE_EXCEPTION(fclose_error);
    {
        sRC = ALTIBASE_CS_FCLOSE_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * PID ���Ϸκ��� pid ���� ��´�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN,OUT] aPid �о���� pid ���� ���� ������
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerPidFileLoad (CheckServerHandle *aHandle, pid_t *aPid)
{
    FILE     *sFP;
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);
    IDE_ASSERT(aPid != NULL);

    sFP = idlOS::fopen(aHandle->mPidFilePath, "r");
    IDE_TEST_RAISE(sFP == NULL, fopen_error);

    if ((sRC = fread(aPid, sizeof(pid_t), 1, sFP)) != 1)
    {
        IDE_RAISE(fread_error);
    }

    sRC = idlOS::fclose(sFP);
    IDE_TEST_RAISE(sRC != 0, fclose_error);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(fopen_error);
    {
        sRC = ALTIBASE_CS_FOPEN_ERROR;
    }
    IDE_EXCEPTION(fread_error);
    {
        sRC = ALTIBASE_CS_FREAD_ERROR;
    }
    IDE_EXCEPTION(fclose_error);
    {
        sRC = ALTIBASE_CS_FCLOSE_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * PID ������ �����Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerPidFileRemove (CheckServerHandle *aHandle)
{
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    sRC = idlOS::unlink(aHandle->mPidFilePath);
    IDE_TEST_RAISE(sRC != 0, pid_remove_error);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(pid_remove_error)
    {
        sRC = ALTIBASE_CS_PID_REMOVE_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * pid�� �ش��ϴ� ���μ����� ���δ�.
 *
 * @param[IN] aPid ������ ���μ����� pid
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerKillProcess (pid_t aPid)
{
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    sRC = idlOS::kill(aPid, SIGKILL);
    IDE_TEST_RAISE(sRC != 0, process_kill_error);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(process_kill_error);
    {
        sRC = ALTIBASE_CS_PROCESS_KILL_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}
