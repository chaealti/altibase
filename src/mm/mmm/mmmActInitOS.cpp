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
#include <mmErrorCode.h>
#include <mmuProperty.h>


/* =======================================================
 * Action Function
 * => Check Environment
 * =====================================================*/

static IDE_RC mmmSetupFD()
{
#if defined(ALTIBASE_USE_VALGRIND)
    return IDE_SUCCESS;
#else
    SInt CurMaxHandle, MaxHandle;

    /* ------------------------------------------------------------------
     *  Maximum File Descriptor ����
     * -----------------------------------------------------------------*/

    CurMaxHandle = idlVA::max_handles();
    idlVA::set_handle_limit();
    MaxHandle = idlVA::max_handles();

    if( MaxHandle > FD_SETSIZE )
    {
        ideLog::log(IDE_SERVER_0,
                    MM_TRC_MAX_HANDLE_EXCEED_FD_SETSIZE,
                    (SInt)MaxHandle,
                    (SInt)FD_SETSIZE);
        idlOS::exit(-1);
    }

    if( ID_SIZEOF(fd_set) < (SInt)(FD_SETSIZE / 8) )
    {
        ideLog::log(IDE_SERVER_0,
                    MM_TRC_SMALL_SIZE_OF_FD_SET,
                    (SInt)ID_SIZEOF(fd_set),
                    (SInt)(FD_SETSIZE / 8));
        idlOS::exit(-1);
    }

    IDE_TEST_RAISE(CurMaxHandle == -1, error_get_max_handle);
    IDE_TEST_RAISE(MaxHandle < 0, error_set_max_handle);

    // �ڵ��� ������ ������ ������ �� ���� (��������)
    if ((UInt)MaxHandle < mmuProperty::getMaxClient())
    {
        ideLog::log(IDE_SERVER_0, MM_TRC_MAX_HANDLE_EXCEED_MAX_CLIENT);
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION(error_get_max_handle);
    {
        ideLog::log(IDE_SERVER_0, MM_TRC_GET_MAX_HANDLE_FAILED);
        IDE_SET(ideSetErrorCode(mmERR_FATAL_MAXHANDLE_ERROR));
    }
    IDE_EXCEPTION(error_set_max_handle);
    {
        ideLog::log(IDE_SERVER_0, MM_TRC_GET_MAX_HANDLE_FAILED);
        IDE_SET(ideSetErrorCode(mmERR_FATAL_MAXHANDLE_ERROR));
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#endif
}

/************************************************************************ 
   BUG-16958
   file size limit ��å
   getrlimit(RLIMIT_FSIZE, ...)�� ���� 32-bit system������
   4G�� �ƴ� 2G�� �����ؾ� �Ѵ�. �ֳ��ϸ� 32-bit linux �ý��ۿ�����
   getrlimit�� 4G���� ����� �� �ִ��ص� �����δ� 2G�ۿ� ����� ���� ����.
   ��, �����Ͽɼǿ� _FILE_OFFSET_BITS=64�� �ָ� 32bit system�̶� �ϴ���
   2G �̻��� ������ ����� �� �ִ�.
   ���� system�� 32-bit�̳�, _FILE_OFFSET_BITS definition�� �ִ��Ŀ� ����
   ������ RLIMIT_FSIZE ���� �����Ѵ�.
**************************************************************************/
static IDE_RC mmmSetupOSFileSize()
{
#if (!defined(COMPILE_64BIT) && (_FILE_OFFSET_BITS != 64))
    struct rlimit limit;

    IDE_TEST_RAISE(idlOS::getrlimit(RLIMIT_FSIZE, &limit) != 0,
                   getrlimit_error);

    limit.rlim_cur = 0x7FFFFFFF;

    IDE_TEST_RAISE(idlOS::setrlimit(RLIMIT_FSIZE, &limit) != 0,
                   setrlimit_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(getrlimit_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_GETLIMIT_ERROR));
    }
    IDE_EXCEPTION(setrlimit_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_SETLIMIT_ERROR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#else
    return IDE_SUCCESS;
#endif
}

static IDE_RC mmmCheckFileSize()
{
    struct rlimit  limit;
    /* ------------------------------------------------------------------
     *  [4] Log(DB) File Size �˻�
     * -----------------------------------------------------------------*/
    IDE_TEST_RAISE(idlOS::getrlimit(RLIMIT_FSIZE, &limit) != 0,
                   getrlimit_error);

    // PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�
    //
    // �� �Լ��� pre process�ܰ迡 üũ�Ǵ� ��ɵ��� ��Ƴ��� ���̴�.
    // �����ͺ��̽� ���� ũ�⿡ ���� limit üũ�� smmManager���� ó���Ѵ�.
    // smmManager::initialize����

    IDE_TEST_RAISE( mmuProperty::getLogFileSize() - 1 > limit.rlim_cur ,
                    check_OSFileLimit);

    return IDE_SUCCESS;
    IDE_EXCEPTION(check_OSFileLimit);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_OSFileSizeLimit_ERROR));
    }
    IDE_EXCEPTION(getrlimit_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_GETLIMIT_ERROR));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

static IDE_RC mmmCheckOSenv()
{
    /* ------------------------------------------------------------------
     *  [5] AIX������ ȯ�溯�� �˻� �� Messasge �����
     *      ���ù��� : PR-3900
     *       => ȯ�溯�� ����Ʈ
     *          AIXTHREAD_MNRATIO=1:1
     *          AIXTHREAD_SCOPE=S
     *          AIXTHREAD_MUTEX_DEBUG=OFF
     *          AIXTHREAD_RWLOCK_DEBUG=OFF
     *          AIXTHREAD_COND_DEBUG=OFF
     *          SPINLOOPTIME=1000
     *          YIELDLOOPTIME=50
     *          MALLOCMULTIHEAP=1
     *
     *   [5] HP������ ȯ�溯�� �˻�
     *       => _M_ARENA_OPTS : 8 Ȥ�� 16���� ���� ���. (BUG-13294, TASK-1733)
     *
     * -----------------------------------------------------------------*/
    {
        static const SChar *sEnvList[] =
        {
#if defined(IBM_AIX)
            "AIXTHREAD_MNRATIO",
            "AIXTHREAD_SCOPE",
            "AIXTHREAD_MUTEX_DEBUG",
            "AIXTHREAD_RWLOCK_DEBUG",
            "AIXTHREAD_COND_DEBUG",
            "SPINLOOPTIME",
            "YIELDLOOPTIME",
            "MALLOCMULTIHEAP",
#elif defined(HP_HPUX)
            "_M_ARENA_OPTS",
#endif
            NULL
        };
        SChar *sEnvVal;
        UInt   i;
        idBool sCheckSuccess;

        IDE_CALLBACK_SEND_SYM("[PREPARE] Check O/S Environment  Variables..");

        sCheckSuccess = ID_TRUE;
        for (i = 0; sEnvList[i] != NULL; i++)
        {
            UInt sLen = 0;

            sEnvVal = idlOS::getenv(sEnvList[i]);
            if (sEnvVal != NULL)
            {
                sLen = idlOS::strlen(sEnvVal);
            }

            if(sLen == 0)
            {
                ideLog::log(IDE_SERVER_0, MM_TRC_ENV_NOT_DEFINED, sEnvList[i]);
                sCheckSuccess = ID_FALSE;
            }
            else
            {
                ideLog::log(IDE_SERVER_0, MM_TRC_ENV_DEFINED, sEnvList[i], sEnvVal);
            }
        }

        if (sCheckSuccess != ID_TRUE)
        {
            IDE_CALLBACK_SEND_MSG("[WARNING!!!] check $ALTIBASE_HOME"IDL_FILE_SEPARATORS"trc"IDL_FILE_SEPARATORS"altibase_boot.log");
        }
        else
        {
            IDE_CALLBACK_SEND_MSG("[SUCCESS]");
        }
   }
    return IDE_SUCCESS;
}

static IDE_RC mmmPhaseActionInitOS(mmmPhase         /*aPhase*/,
                                   UInt             /*aOptionflag*/,
                                   mmmPhaseAction * /*aAction*/)
{
    IDE_TEST( mmmSetupFD() != IDE_SUCCESS);

    IDE_TEST( mmmSetupOSFileSize() != IDE_SUCCESS);

#if !defined(WRS_VXWORKS) && !defined(SYMBIAN)
    IDE_TEST( mmmCheckFileSize() != IDE_SUCCESS);
#endif

    IDE_TEST( mmmCheckOSenv()   != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* =======================================================
 *  Action Func Object for Action Descriptor
 * =====================================================*/

mmmPhaseAction gMmmActInitOS =
{
    (SChar *)"Initialize Operating System Parameters",
    0,
    mmmPhaseActionInitOS
};

