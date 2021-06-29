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

#include <idl.h>
#include <ide.h>
#include <checkServerLib.h>



#define COMMAND_FORM "sh %s"



/**
 * ���� �ð����� ����Ѵ�.
 *
 * @param[In] aSec ����� �ð�(�� ����)
 * @return �����ϸ� 0, �ƴϸ� 0�� �ƴ� ��
 */
SInt mySleep(SInt aSec)
{
    SInt rc;
    PDL_Time_Value sPDL_Time_Value;
    sPDL_Time_Value.initialize(aSec);
    rc = idlOS::select(0, NULL, NULL, NULL, sPDL_Time_Value);
    return rc;
}

/**
 * ��� ��� ���
 */
void printUsage()
{
    idlOS::fprintf(stderr, "USAGE : checkServer [-n] {-f server_restart_script_file}\n");
    idlOS::fprintf(stderr, "Ex> checkServer -f restart.sh \n");
    idlOS::fflush(stderr);
}

/**
 * checkServer�� �ԷµǴ� ��� �ñ׳ο� ����
 * Blocking ��Ű��, �ش� �ñ׳��� ���.
 *
 * @param[IN] aHandle �ڵ�
 * @return �����ϸ� IDE_SUCCESS, �ƴϸ� IDE_FAILURE
 */
IDE_RC setupSignalHandler(ALTIBASE_CHECK_SERVER_HANDLE aHandle)
{
    sigset_t  ispSigSet;
    IDE_TEST_RAISE(idlOS::sigemptyset(&ispSigSet) != 0, sigemptyset_error);
#ifdef DEC_TRU64
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGHUP) != 0, sigaddset_error);
    //IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGINT) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGQUIT) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGABRT) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGEMT) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGSYS) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGPIPE) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGALRM) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGTERM) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGURG) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGTSTP) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGCONT) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGCHLD) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGTTIN) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGTTOU) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGPOLL) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGXCPU) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGXFSZ) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGVTALRM) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGPROF) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGWINCH) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGINFO) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGUSR1) != 0, sigaddset_error);
    IDE_TEST_RAISE(idlOS::sigaddset(&ispSigSet, SIGRESV) != 0, sigaddset_error);
#else
    IDE_TEST_RAISE(idlOS::sigfillset(&ispSigSet)  != 0, sigfillset_error);
#endif
    IDE_TEST_RAISE(idlOS::sigprocmask(SIG_BLOCK, &ispSigSet, NULL) != 0,
                   sigprocmask_error);

    return IDE_SUCCESS;

#ifdef DEC_TRU64
    IDE_EXCEPTION(sigaddset_error);
    {
        altibase_check_server_log(aHandle, "error in sigaddset() : errno=%d", errno);
    }
#endif /* DEC_TRU64 */

    IDE_EXCEPTION(sigemptyset_error);
    {
        altibase_check_server_log(aHandle, "error in sigemptyset() : errno=%d", errno);
    }
    IDE_EXCEPTION(sigfillset_error);
    {
        altibase_check_server_log(aHandle, "error in sigfillset() : errno=%d", errno);
    }
    IDE_EXCEPTION(sigprocmask_error);
    {
        altibase_check_server_log(aHandle, "error in sigprocmask() : errno=%d", errno);
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

void mainCallbackFuncForFatal(
    SChar  *  file ,
    SInt      linenum ,
    SChar  *  msg  )
{
    idlOS::printf("%s:%d(%s)\n", file, linenum, msg);
    idlOS::exit(-1);
}

IDE_RC mainNullCallbackFuncForMessage(
    const SChar * /*_ msg  _*/,
    SInt          /*_ flag _*/,
    idBool        /*_ aLogMsg _*/)
{
    return IDE_SUCCESS;
}

void mainNullCallbackFuncForPanic(
    SChar  * /*_ file _*/,
    SInt     /*_ linenum _*/,
    SChar  * /*_ msg _*/ )
{
}

void mainNullCallbackFuncForErrlog(
    SChar  * /*_ file _*/,
    SInt     /*_ linenum _*/)
{
}

int main (SInt argc, SChar *argv[])
{
    ALTIBASE_CHECK_SERVER_HANDLE sHandle = ALTIBASE_CHECK_SERVER_NULL_HANDLE;
    SChar sPidFileName[1024];
    SChar sServerStartCommand[1024];
    idBool sDaemonMode = ID_TRUE;
    idBool sFileOptExist = ID_FALSE;
    idBool sDaemonOptExist = ID_FALSE;
    SInt sIntOpt;
    SInt sCmdOpt;
    SInt sRC;

    (void) ideSetCallbackFunctions
        (
            mainNullCallbackFuncForMessage,
            mainCallbackFuncForFatal,
            mainNullCallbackFuncForPanic,
            mainNullCallbackFuncForErrlog
            );

    while ( (sCmdOpt = idlOS::getopt(argc, argv, "nf:")) != EOF)
    {
        switch(sCmdOpt)
        {
            case 'f':  // startup script file
                sFileOptExist = ID_TRUE;
                idlOS::snprintf(sServerStartCommand,
                                ID_SIZEOF(sServerStartCommand),
                                COMMAND_FORM,
                                optarg);
                break;
            case 'n':
                sDaemonOptExist = ID_TRUE;
                sDaemonMode = ID_FALSE;
                break;
        }
    }

    if (sFileOptExist != ID_TRUE && sDaemonOptExist != ID_TRUE)
    {
        printUsage();
        return -1;
    }

    sRC = altibase_check_server_init(&sHandle, NULL);
    if (sRC != ALTIBASE_CS_SUCCESS)
    {
        idlOS::fprintf(stderr, "Unable to initialize ALTIBASE_CHECK_SERVER_HANDLE\n");
        return -1;
    }

    sIntOpt = ALTIBASE_CHECK_SERVER_ATTR_LOG_ON;
    sRC = altibase_check_server_set_attr(sHandle, ALTIBASE_CHECK_SERVER_ATTR_LOG, &sIntOpt);
    IDE_TEST_RAISE(sRC != ALTIBASE_CS_SUCCESS, CheckServerLibError);

    /* BUG-45135 default sleep time ���� 5 -> 500 (����:millisecond) */
    sIntOpt = 500;
    sRC = altibase_check_server_set_attr(sHandle, ALTIBASE_CHECK_SERVER_ATTR_SLEEP, &sIntOpt);
    IDE_TEST_RAISE(sRC != ALTIBASE_CS_SUCCESS, CheckServerLibError);

    sIntOpt = ALTIBASE_CHECK_SERVER_ATTR_CANCEL_OFF;
    sRC = altibase_check_server_set_attr(sHandle, ALTIBASE_CHECK_SERVER_ATTR_CANCEL, &sIntOpt);
    IDE_TEST_RAISE(sRC != ALTIBASE_CS_SUCCESS, CheckServerLibError);

    altibase_check_server_log(sHandle, "[START] =========== CheckServer Starting ============== ");

    /* ------------------------------------------------
     *   daemonize
     * ----------------------------------------------*/
    if( sDaemonMode == ID_TRUE )
    {
        sRC = idlVA::daemonize(".", 0);
        if (sRC != 0)
        {
            altibase_check_server_log(sHandle, "[FAILURE] Daemonized. errno=%d ", errno);
        }
        else
        {
            altibase_check_server_log(sHandle, "[SUCCESS] Daemonized ");
        }
    }

    /* �ʿ�� ������, ��µǴ� ������ ������ �����ſ� ����ϰ� �ϱ� ���� */
    idlOS::fprintf(stderr, "Initializing File Lock..\n");
    idlOS::fprintf(stderr, "Check Lock File Existence..\n");
    idlOS::fprintf(stderr, "[SUCCESS] ok. Lock File Existence..\n");
    idlOS::fprintf(stderr, "Open File Lock..\n");
    idlOS::fprintf(stderr, "[SUCCESS] ok. File Lock Opened..\n");

    sRC = altibase_check_server_get_attr(sHandle,
                                         ALTIBASE_CHECK_SERVER_ATTR_PIDFILE,
                                         sPidFileName, ID_SIZEOF(sPidFileName),
                                         NULL);
    IDE_ASSERT(sRC == ALTIBASE_CS_SUCCESS);
    idlOS::fprintf(stderr, "pidFilename : %s\n", sPidFileName);

    if (setupSignalHandler(sHandle) != IDE_SUCCESS)
    {
        altibase_check_server_log(sHandle, "[FAILURE] Signal Block. errno=%d", errno);
    }
    else
    {
        altibase_check_server_log(sHandle, "[SUCCESS] Signal Blocked. ");
    }

    idlOS::fprintf(stderr, "[SUCCESS] ok. Waiting Altibase-Process To Lock..\n");
    idlOS::fflush(stderr);
    while (ID_TRUE)
    {
        sRC = altibase_check_server(sHandle);
        if (sRC != ALTIBASE_CS_SERVER_STOPPED)
        {

            /* pid file�� �̹� ������ ��õ����� �ʴ´�. */
            IDE_TEST_RAISE(sRC == ALTIBASE_CS_PID_FILE_ALREADY_EXIST,
                           CheckServerLibError);

            /* �ʿ�� ������, �α� ������ ������ ������ ���߱� ����. */
            if ((sRC == ALTIBASE_CS_SOCKET_OPEN_ERROR) || (sRC == ALTIBASE_CS_SOCKET_REUSE_ERROR) || (sRC == ALTIBASE_CS_SOCKET_CLOSE_ERROR))
            {
                altibase_check_server_log(sHandle,
                                           "[ERROR] Can't Check Server Status"
                                           "so, wait 5 second. ");
            }

            /* ������ �߻��߾ �����ϰ� �ٽ� �õ� (���� checkServer ����) */
            continue;
        }

        altibase_check_server_log(sHandle, "==> Altibase is not running. so, Exec [%s] to start server.",
                                   sServerStartCommand);

        if (sFileOptExist == ID_TRUE)
        {
            restart_system:;
            if (idlOS::system(sServerStartCommand) != 0)
            {
                switch(errno)
                {
                    case EINTR:
                        altibase_check_server_log(sHandle,
                            "   [ERROR] system() interrupted by EINTR."
                            "    so retry sytem().");
                        break;
                    case EAGAIN:
                        altibase_check_server_log(sHandle,
                            "   [ERROR] system() error by EAGAIN."
                            "    so wait 5 second & retry sytem().");
                        mySleep(5);
                        break;
                    default:
                        altibase_check_server_log(sHandle, "   [FATAL ERROR] system() error = %d.", errno);
                        altibase_check_server_log(sHandle, "   so, wait 5 second &  retry sytem().");
                        mySleep(5);
                        break;
                }
                goto restart_system;
            }
            else
            {
                altibase_check_server_log(sHandle, "[SUCCESS] Running script to start server.");
            }
        }
        mySleep(5);
    }

    (void) altibase_check_server_final(&sHandle);

    return 0;

    IDE_EXCEPTION(CheckServerLibError);
    {
        idlOS::fprintf(stderr, "ERROR CODE : %d\n", sRC);
        idlOS::fflush(stderr);
    }
    IDE_EXCEPTION_END;

    if (sHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE)
    {
        (void) altibase_check_server_final(&sHandle);
    }

    return -1;
}
