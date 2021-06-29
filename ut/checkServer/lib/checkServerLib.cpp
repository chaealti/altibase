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

#include <idp.h>
#include <idu.h>
#include <iduLatch.h>
#include <checkServerPid.h>
#include <checkServerLib.h>

#define MAX_TIMESTRING_LEN      20      /* YYYY/MM/DD HH:mm:ss */

#define CHKSVR_DEF_SLEEPTIME    0       /* need not to sleep */

CHKSVR_INTERNAL CHKSVR_RC checkServerRunByPortBinding (CheckServerHandle *aHandle, idBool *aServerRunning);

static pthread_once_t     gCheckServerInitOnce  = PTHREAD_ONCE_INIT;
static PDL_thread_mutex_t gCheckServerMutex;
static SInt               gCheckServerCount;



/**
 * ���̺귯������ �������� ���� ���� ������ �ʱ�ȭ�Ѵ�.
 *
 * ���̺귯���� ����ϱ� ���� ȣ��Ǿ�� �ϸ�,
 * �ݵ�� ���μ����� 1���� ȣ��Ǿ�� �Ѵ�.
 */
CHKSVR_INTERNAL_FOR_CALLBACK
void checkServerInitializeOnce (void)
{
    IDE_ASSERT(idlOS::thread_mutex_init(&gCheckServerMutex) == 0);

    gCheckServerCount = 0;
}

/**
 * ���̺귯���� �ʱ�ȭ�Ѵ�.
 *
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerLibraryInit (void)
{
    IDE_RC sReturn;

    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    idlOS::pthread_once(&gCheckServerInitOnce, checkServerInitializeOnce);

    IDE_ASSERT(idlOS::thread_mutex_lock(&gCheckServerMutex) == 0);

    /* BUGBUG: ���ο��� ������ idp ���� ���� ������ ���� ����� �ȵȴ�.
       �ڵ� ����� �������̽����� ���μ��� �� �ϳ��� �� �� �ִ�.
       idp ���� ����ϴ� �ٸ� ���̺귯���͵� ���ÿ� ����� �� ����. */
    IDE_TEST_RAISE(gCheckServerCount != 0, DupInitError);

    /* Global Memory Manager initialize */
    sReturn = iduMemMgr::initializeStatic(IDU_CLIENT_TYPE);
    IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibInitError);
    sReturn = iduMutexMgr::initializeStatic(IDU_CLIENT_TYPE);
    IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibInitError);

    //fix TASK-3870
    iduLatch::initializeStatic(IDU_CLIENT_TYPE);

    sReturn = iduCond::initializeStatic();
    IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibInitError);

    gCheckServerCount++;

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCheckServerMutex) == 0);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(DupInitError);
    {
        sRC = ALTIBASE_CS_DUP_INIT_ERROR;
    }
    IDE_EXCEPTION(LibInitError);
    {
        gCheckServerCount = -1;
        sRC = ALTIBASE_CS_LIB_INIT_ERROR;
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCheckServerMutex) == 0);

    return sRC;
}

/**
 * ���̺귯�� �ʱ�ȭ�� �����Ѵ�.
 *
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerLibraryDestroy (void)
{
    IDE_RC sReturn;
    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC sRC;

    IDE_ASSERT(idlOS::thread_mutex_lock(&gCheckServerMutex) == 0);

    IDE_TEST_RAISE(gCheckServerCount < 0, CheckServerCountError);

    gCheckServerCount--;
    if (gCheckServerCount == 0)
    {
        sReturn = iduCond::destroyStatic();
        IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibDestroyError);

        iduLatch::destroyStatic();

        sReturn = iduMutexMgr::destroyStatic();
        IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibDestroyError);
        sReturn = iduMemMgr::destroyStatic();
        IDE_TEST_RAISE(sReturn != IDE_SUCCESS, LibDestroyError);
    }

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCheckServerMutex) == 0);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(CheckServerCountError);
    {
        sRC = ALTIBASE_CS_CHECK_SERVER_COUNT_ERROR;
    }
    IDE_EXCEPTION(LibDestroyError);
    {
        gCheckServerCount = -1;
        sRC = ALTIBASE_CS_LIB_DESTROY_ERROR;
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCheckServerMutex) == 0);

    return sRC;
}

/**
 * ���� �ð����� ����Ѵ�.
 *
 * @param[IN] aMSec ����� �ð�(�и�(1/1000)�� ����)
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerSleep (SInt aMSec)
{
    PDL_Time_Value  sTimeVal;

    /* BUG-32377 Returned error codes of checkServer library
       should be varied to distinguish errors */
    CHKSVR_RC   sRC;
    IDE_TEST_RAISE(aMSec < 0, SleepTimeError);

    sTimeVal.msec(aMSec);
    (void) idlOS::select(0, NULL, NULL, NULL, sTimeVal);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(SleepTimeError);
    {
        sRC = ALTIBASE_CS_SLEEP_TIME_ERROR;
    }

    IDE_EXCEPTION_END;
    return sRC;
}

/**
 * ���� �Ӽ��� �о�´�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN] aHomePath ��Ƽ���̽� ������ ��ġ�� ���丮.
 *                      ȯ�溯���� ������ ���� ����Ϸ��� NULL.
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerLoadProperties (CheckServerHandle *aHandle, SChar *aHomePath)
{
    SChar       sMsgFilePath[MAX_PATH_LEN] = {'\0', };
    CHKSVR_RC   sRC;
    SChar *     sMsgLogDir = NULL;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    IDE_TEST_RAISE(STATE_IS_PROP_LOADED(aHandle), AlreadyLoaded);

    if ( aHomePath == NULL )
    {
        IDE_TEST_RAISE(idlOS::getenv(IDP_HOME_ENV) == NULL, NullHomepathError);
    }

    sRC = idp::initialize(aHomePath, NULL);
    IDE_TEST_RAISE(sRC != IDE_SUCCESS, LoadPropertiesError);

    IDE_ASSERT( idp::read( "PORT_NO", &aHandle->mPortNo ) == IDE_SUCCESS );

    IDE_ASSERT( idp::readPtr( "SERVER_MSGLOG_DIR", (void **)&sMsgLogDir )
                             == IDE_SUCCESS );

    idlOS::snprintf(aHandle->mPidFilePath, ID_SIZEOF(aHandle->mPidFilePath),
                    "%s%c%s",
                    sMsgLogDir,
                    IDL_FILE_SEPARATOR,
                    CHKSVR_PID_FILE);

    idlOS::snprintf(sMsgFilePath, ID_SIZEOF(sMsgFilePath),
                    "%s%c%s",
                    sMsgLogDir,
                    IDL_FILE_SEPARATOR,
                    CHKSVR_MSG_FILE);
    sRC = aHandle->mMsgLog->initialize(sMsgFilePath,
                                       CHKSVR_MSGLOG_FILESIZE,
                                       CHKSVR_MSGLOG_MAX_FILENUM);
    IDE_TEST_RAISE(sRC != IDE_SUCCESS, msglog_init_error);

    STATE_SET_PROP_LOADED(aHandle);

    IDE_EXCEPTION_CONT(AlreadyLoaded);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(NullHomepathError);
    {
        sRC = ALTIBASE_CS_NULL_HOMEPATH_ERROR;
    }
    IDE_EXCEPTION(LoadPropertiesError);
    {
        sRC = ALTIBASE_CS_LOAD_PROPERTIES_ERROR;
    }
    IDE_EXCEPTION(msglog_init_error);
    {
        sRC = ALTIBASE_CS_MSGLOG_INIT_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * �о�� ���� �Ӽ��� ���õ� �ڿ��� �����Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerUnloadProperties (CheckServerHandle *aHandle)
{
    IDE_RC sReturn;

    IDE_TEST_RAISE(STATE_NOT_PROP_LOADED(aHandle), AlreadyUnloaded);

    sReturn = idp::destroy();
    IDE_ASSERT(sReturn == IDE_SUCCESS);

    STATE_UNSET_PROP_LOADED(aHandle);

    IDE_EXCEPTION_CONT(AlreadyUnloaded);

    return ALTIBASE_CS_SUCCESS;
}

/**
 * ���� �ð��� "YYYY/MM/DD HH:mm:ss" ������ ���ڿ��� ��´�.
 *
 * @param[IN,OUT] aBuf ���� �ð� ���ڿ��� ���� ����
 * @param[IN] aBufSize ������ ũ��
 * @return ���ڿ��� �� ��ȯ������ ALTIBASE_CS_SUCCESS,
 *         ���۰� ������� ������ ALTIBASE_CS_SUCCESS_WITH_INFO
 */
CHKSVR_INTERNAL
CHKSVR_RC getTimeString (SChar *aBuf, SInt aBufSize)
{
    PDL_Time_Value  sTimevalue;
    struct tm       sLocalTime;
    time_t          sTime;
    SInt            sOutLen = 0;
    CHKSVR_RC sRC;

    if ((aBuf != NULL) && (aBufSize > 0))
    {
        sTimevalue = idlOS::gettimeofday();
        sTime      = (time_t) sTimevalue.sec();
        idlOS::localtime_r(&sTime, &sLocalTime);

        sOutLen = idlOS::snprintf(aBuf, aBufSize,
                                  "%4"ID_UINT32_FMT
                                  "/%02"ID_UINT32_FMT
                                  "/%02"ID_UINT32_FMT
                                  " %02"ID_UINT32_FMT
                                  ":%02"ID_UINT32_FMT
                                  ":%02"ID_UINT32_FMT"",
                                  sLocalTime.tm_year + 1900,
                                  sLocalTime.tm_mon + 1,
                                  sLocalTime.tm_mday,
                                  sLocalTime.tm_hour,
                                  sLocalTime.tm_min,
                                  sLocalTime.tm_sec);

        if (sOutLen >= aBufSize)
        {
            sRC = ALTIBASE_CS_SUCCESS_WITH_INFO;
        }
        else
        {
            sRC = ALTIBASE_CS_SUCCESS;
        }
    }
    else
    {
        sRC = ALTIBASE_CS_SUCCESS_WITH_INFO;
    }

    return sRC;
}

/**
 * �α� �޽����� �����.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN] aForm �α� �޽��� ��
 * @param[IN] aVaList �α� �޽����� ����� ��
 * @return �α� �޽����� �� �������� ALTIBASE_CS_SUCCESS,
 *         �α� �ɼ��� ���� ������ ALTIBASE_CS_SUCCESS_WITH_INFO,
 *         �ڵ��� ��ȿ���� ������ ALTIBASE_CS_INVALID_HANDLE
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerLogV (CheckServerHandle *aHandle, const SChar *aForm, va_list aVaList)
{
    SChar     sTimeBuf[MAX_TIMESTRING_LEN];
    CHKSVR_RC sRC;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);

    if (aHandle->mOptUseLog != ID_TRUE)
    {
        sRC = ALTIBASE_CS_SUCCESS_WITH_INFO;
    }
    else
    {
        sRC = getTimeString(sTimeBuf, ID_SIZEOF(sTimeBuf));
        IDE_ASSERT(sRC == ALTIBASE_CS_SUCCESS);

        (void) aHandle->mMsgLog->open();

        ideLogEntry sLog(aHandle->mMsgLog, ACP_TRUE, 0);
        (void) sLog.appendFormat("[%s] ", sTimeBuf);
        (void) sLog.appendArgs(aForm, aVaList);
        (void) sLog.append("\n");
        sLog.write();

        (void) aHandle->mMsgLog->close();
    }

    return sRC;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * �α� �޽����� �����.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN] aForm �α� �޽��� ��
 * @param[IN] args �α� �޽����� ����� ��
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerLog (CheckServerHandle *aHandle, const SChar *aForm, ...)
{
    va_list   sVaList;
    CHKSVR_RC sRC;

    va_start (sVaList, aForm);
    sRC = checkServerLogV(aHandle, aForm, sVaList);
    va_end (sVaList);

    return sRC;
}

/**
 * �Ӽ� ���� ��´�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN] aAttrType �Ӽ� ����
 * @param[IN,OUT] aValuePtr �Ӽ� ���� ���� ����
 * @param[IN] aBufSize ���� ũ��
 * @param[IN,OUT] aStrLenPtr �Ӽ� ���� ���ڿ� �� ��, ���ڿ� ���̸� ��ȯ���� ������
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerGetAttr (CheckServerHandle          *aHandle,
                              ALTIBASE_CHECK_SERVER_ATTR  aAttrType,
                              void                       *aValuePtr,
                              SInt                        aBufSize,
                              SInt                       *aStrLenPtr)
{
    SInt        sValLen;
    SInt        sOutLen;
    CHKSVR_RC   sRC = ALTIBASE_CS_SUCCESS;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);

    switch (aAttrType)
    {
        case ALTIBASE_CHECK_SERVER_ATTR_LOG:
            IDE_TEST_RAISE(aValuePtr == NULL, InvalidNullPtr);
            *((SInt *) aValuePtr) = (aHandle->mOptUseLog == ID_TRUE)
                                  ? ALTIBASE_CHECK_SERVER_ATTR_LOG_ON
                                  : ALTIBASE_CHECK_SERVER_ATTR_LOG_OFF;
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_SLEEP:
            IDE_TEST_RAISE(aValuePtr == NULL, InvalidNullPtr);
            *((SInt *) aValuePtr) = aHandle->mOptSleep;
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_CANCEL:
            IDE_TEST_RAISE(aValuePtr == NULL, InvalidNullPtr);
            *((SInt *) aValuePtr) = (aHandle->mOptUseCancel == ID_TRUE)
                                  ? ALTIBASE_CHECK_SERVER_ATTR_CANCEL_ON
                                  : ALTIBASE_CHECK_SERVER_ATTR_CANCEL_OFF;
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_PIDFILE:
            sValLen = idlOS::strlen(aHandle->mPidFilePath);
            IDE_ASSERT(sValLen >= 0);
            if ((aValuePtr != NULL) && (aBufSize > 0))
            {
                sOutLen = idlOS::snprintf((SChar *)aValuePtr, aBufSize,
                                          aHandle->mPidFilePath);
                if (sOutLen >= aBufSize)
                {
                    sRC = ALTIBASE_CS_SUCCESS_WITH_INFO;
                }
            }
            else
            {
                sRC = ALTIBASE_CS_SUCCESS_WITH_INFO;
            }

            if (aStrLenPtr != NULL)
            {
                *aStrLenPtr = sValLen;
            }
            break;

        default:
            IDE_RAISE(InvalidAttrType);
            break;
    }

    return sRC;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION(InvalidNullPtr);
    {
        sRC = ALTIBASE_CS_INVALID_NULL_PTR;
    }
    IDE_EXCEPTION(InvalidAttrType);
    {
        sRC = ALTIBASE_CS_INVALID_ATTR_TYPE;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * �Ӽ��� �����Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN] aAttrType �Ӽ� ����
 * @param[IN] aValuePtr �Ӽ� ���� ����Ű�� ������
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerSetAttr (CheckServerHandle          *aHandle,
                              ALTIBASE_CHECK_SERVER_ATTR  aAttrType,
                              void                       *aValuePtr)
{
    SInt     *sIntValuePtr;
    CHKSVR_RC sRC;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);
    IDE_TEST_RAISE(aValuePtr == NULL, InvalidNullPtr);

    switch (aAttrType)
    {
        case ALTIBASE_CHECK_SERVER_ATTR_LOG:
            sIntValuePtr = (SInt *) aValuePtr;
            if (*sIntValuePtr == ALTIBASE_CHECK_SERVER_ATTR_LOG_ON)
            {
                aHandle->mOptUseLog = ID_TRUE;
            }
            else
            {
                aHandle->mOptUseLog = ID_FALSE;
            }
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_SLEEP:
            sIntValuePtr = (SInt *) aValuePtr;
            aHandle->mOptSleep = *sIntValuePtr;
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_CANCEL:
            sIntValuePtr = (SInt *) aValuePtr;
            if (*sIntValuePtr == ALTIBASE_CHECK_SERVER_ATTR_CANCEL_ON)
            {
                aHandle->mOptUseCancel = ID_TRUE;
            }
            else
            {
                aHandle->mOptUseCancel = ID_FALSE;
            }
            break;

        case ALTIBASE_CHECK_SERVER_ATTR_PIDFILE:
            IDE_RAISE(PidFileSetUnsupported);
            break;

        default:
            IDE_RAISE(InvalidAttrType);
            break;
    }

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION(InvalidNullPtr);
    {
        sRC = ALTIBASE_CS_INVALID_NULL_PTR;
    }
    IDE_EXCEPTION(PidFileSetUnsupported);
    {
        sRC = ALTIBASE_CS_PID_FILE_SET_UNSUPPORTED;
    }
    IDE_EXCEPTION(InvalidAttrType);
    {
        sRC = ALTIBASE_CS_INVALID_ATTR_TYPE;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * ��Ƽ���̽� ������ �׾����� Ȯ���ϱ� ���� �ʿ��� �ʱ�ȭ�� �����Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerOpen (CheckServerHandle *aHandle)
{
    idBool    sPidFileExist;
    CHKSVR_RC sRC;

    IDE_DASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    IDE_TEST_RAISE(STATE_IS_OPENED(aHandle), AlreadyOpened);

    /* init ServerStat */

    sRC = aHandle->mServerStat->initialize();
    IDE_TEST_RAISE(sRC != IDE_SUCCESS, FileLockInitError);

    sRC = aHandle->mServerStat->isFileExist();
    IDE_TEST_RAISE(sRC != ID_TRUE, FileLockNotExist);

    sRC = aHandle->mServerStat->initFileLock();
    IDE_TEST_RAISE(sRC != IDE_SUCCESS, FileLockInitError);

    /* create pid file */

    sRC = checkServerPidFileExist(aHandle, &sPidFileExist);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);
    IDE_TEST_RAISE(sPidFileExist == ID_TRUE, PidFileAlreadyExist);

    sRC = checkServerPidFileCreate(aHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    STATE_SET_OPENED(aHandle);

    IDE_EXCEPTION_CONT(AlreadyOpened);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(FileLockInitError);
    {
        sRC = ALTIBASE_CS_FILE_LOCK_INIT_ERROR;
    }
    IDE_EXCEPTION(FileLockNotExist);
    {
        sRC = ALTIBASE_CS_FILE_LOCK_NOT_EXIST;
    }
    IDE_EXCEPTION(PidFileAlreadyExist);
    {
        sRC = ALTIBASE_CS_PID_FILE_ALREADY_EXIST;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * ��Ƽ���̽� ������ �׾����� Ȯ���ϱ� ���� ������ �ʱ�ȭ�� �����Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerClose (CheckServerHandle *aHandle)
{
    CHKSVR_RC sRC;

    IDE_DASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    IDE_TEST_RAISE(STATE_NOT_OPENED(aHandle), AlreadyClosed);

    /* dest ServerStat */
    sRC = aHandle->mServerStat->destFileLock();
    IDE_TEST_RAISE(sRC != IDE_SUCCESS, FileLockDestroyError);

    sRC = aHandle->mServerStat->destroy();
    IDE_ASSERT(sRC == IDE_SUCCESS); /* always success */

    /* remove pid file */

    sRC = checkServerPidFileRemove(aHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    STATE_UNSET_OPENED(aHandle);

    IDE_EXCEPTION_CONT(AlreadyClosed);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(FileLockDestroyError);
    {
        sRC = ALTIBASE_CS_FILE_LOCK_DESTROY_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * �ڵ��� �����Ѵ�.
 *
 * @param[IN,OUT] aHandlePtr ������ �ڵ��� ���� ������
 * @param[IN] aHomePath ��Ƽ���̽� ������ ��ġ�� ���丮.
 *                      ȯ�溯���� ������ ���� ����Ϸ��� NULL.
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerInit (CheckServerHandle **aHandlePtr, SChar *aHomePath)
{
    CheckServerHandle   *sHandle = ALTIBASE_CHECK_SERVER_NULL_HANDLE;
    CheckServerStat     *sServerStat = NULL;
    ideMsgLog           *sMsgLog = NULL;
    CHKSVR_RC           sRC;

    IDE_TEST_RAISE(aHandlePtr == NULL, InvalidNullPtr);

    sRC = checkServerLibraryInit();
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    sHandle = (CheckServerHandle *) idlOS::calloc(1, ID_SIZEOF(CheckServerHandle));
    IDE_TEST_RAISE(sHandle == NULL, MAllocError);

    sServerStat = new CheckServerStat();
    IDE_TEST_RAISE(sServerStat == NULL, MAllocError);

    sMsgLog = new ideMsgLog();
    IDE_TEST_RAISE(sMsgLog == NULL, MAllocError);

    IDE_ASSERT(idlOS::thread_mutex_init(&(sHandle->mMutex)) == 0);

    sHandle->mServerStat    = sServerStat;
    sHandle->mMsgLog        = sMsgLog;
    sHandle->mOptUseLog     = ID_FALSE;
    sHandle->mOptSleep      = CHKSVR_DEF_SLEEPTIME;
    sHandle->mOptUseCancel  = ID_TRUE;

    sRC = checkServerLoadProperties(sHandle, aHomePath);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    *aHandlePtr = sHandle;

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(InvalidNullPtr);
    {
        sRC = ALTIBASE_CS_INVALID_NULL_PTR;
    }
    IDE_EXCEPTION(MAllocError);
    {
        sRC = ALTIBASE_CS_MALLOC_ERROR;
    }
    IDE_EXCEPTION_END;

    SAFE_DELETE(sMsgLog);
    SAFE_DELETE(sServerStat);
    SAFE_FREE(sHandle);

    return sRC;
}

/**
 * ��Ƽ���̽� ������ ����Ǿ����� Ȯ���ϱ� ���� �Ҵ��� �ڵ��� �����Ѵ�.
 *
 * @param[IN,OUT] aHandlePtr ���� �� NULL�� �ʱ�ȭ �� �ڵ� ������
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerDestroy (CheckServerHandle **aHandlePtr)
{
    CheckServerHandle *sHandle;
    CHKSVR_RC sRC;

    IDE_TEST_RAISE(aHandlePtr == NULL, InvalidHandle);
    sHandle = *aHandlePtr;
    IDE_TEST_RAISE(sHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE,
                   InvalidHandle);

    sRC = checkServerUnloadProperties(sHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    sRC = checkServerClose(sHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    IDE_ASSERT(idlOS::thread_mutex_destroy(&(sHandle->mMutex)) == 0);

    delete sHandle->mMsgLog;
    delete sHandle->mServerStat;
    idlOS::free(sHandle);
    *aHandlePtr = ALTIBASE_CHECK_SERVER_NULL_HANDLE;

    sRC = checkServerLibraryDestroy();
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * port�� �̿��ؼ� ������ ���������� Ȯ���Ѵ�.
 *
 * @param[IN] aHandle �ڵ�
 * @param[IN,OUT] aServerRunning ������ ���� ������ ���θ� ���� ������.
 *                               ������ ���� ���̸� ID_TRUE, �ƴϸ� ID_FALSE
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL
CHKSVR_RC checkServerRunByPortBinding (CheckServerHandle *aHandle, idBool *aServerRunning)
{
    PDL_SOCKET          sSockFd;
    struct sockaddr_in  sServAddr;

    CHKSVR_RC sRC;

    IDE_ASSERT(aHandle != ALTIBASE_CHECK_SERVER_NULL_HANDLE);

    sSockFd = idlOS::socket(AF_INET, SOCK_STREAM, 0);
    IDE_TEST_RAISE(sSockFd == PDL_INVALID_SOCKET, socket_open_error);

    idlOS::memset(&sServAddr, 0, ID_SIZEOF(sServAddr));
    sServAddr.sin_family      = AF_INET;
    sServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sServAddr.sin_port        = htons(aHandle->mPortNo);

    IDE_TEST_RAISE(idlVA::setSockReuseraddr(sSockFd) != 0, socket_reuse_error);

    checkServerLog(aHandle, "<<< Check Server Running>>>");

    if (idlOS::bind(sSockFd, (struct sockaddr *)(&sServAddr),
                    ID_SIZEOF(sServAddr)) < 0)
    {
        *aServerRunning = ID_TRUE; // Server is Running.
        checkServerLog(aHandle, "    => Server is Running (port=%d) : errno=%d",
                       aHandle->mPortNo, errno);
    }
    else // ���ε� ����
    {
        *aServerRunning = ID_FALSE;
        checkServerLog(aHandle, "    => Server is Not Running (port=%d)",
                       aHandle->mPortNo);
    }

    IDE_TEST_RAISE(idlOS::closesocket(sSockFd) != 0, socket_close_error);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(socket_open_error);
    {
        checkServerLog(aHandle, "<<<socket error>>> (port=%d) errno=%d",
                       aHandle->mPortNo, errno);
        sRC = ALTIBASE_CS_SOCKET_OPEN_ERROR;
    }
    IDE_EXCEPTION(socket_reuse_error);
    {
        checkServerLog(aHandle, "<<<reuse error>>> (port=%d) errno=%d",
                       aHandle->mPortNo, errno);
        sRC = ALTIBASE_CS_SOCKET_REUSE_ERROR;
    }
    IDE_EXCEPTION(socket_close_error);
    {
        checkServerLog(aHandle, "<<<socket close error>>> (port=%d) errno=%d",
                       aHandle->mPortNo, errno);
        sRC = ALTIBASE_CS_SOCKET_CLOSE_ERROR;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * checkServerCancel()�� ���� �ñ׳ο� ���� �ڵ鷯
 */
CHKSVR_INTERNAL_FOR_CALLBACK
void checkServerSigDummyHandler (SInt aSigNo)
{
    PDL_UNUSED_ARG(aSigNo);

    /* do nothing */
}

/**
 * ��Ƽ���̽� ������ ����Ǿ����� Ȯ���Ѵ�.
 *
 * �Լ��� ȣ���ϸ� �ﰢ ���Ǵµ�,
 * ��Ƽ���� ������ ����ǰų� ���ܰ� �߻��ϸ� ���ϵȴ�.
 *
 * ��Ƽ���̽� ������ ����ִ��� Ȯ���ϱ� ���ؼ�
 * ���� ���� ������ �̿��ϹǷ�
 * �� �Ǵ� ������ ó���ϴٰ� ���� �� �� �ִµ�,
 * ��� �ڵ� ���� �̿��ϸ� �̸� Ȯ���� �� �ִ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerRun (CheckServerHandle *aHandle)
{
    idBool              sServerRunning;
    CHKSVR_RC           sRC;
    /* BUG-45135 */
    idBool              sIsReTryhold = ID_FALSE;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);

    IDE_ASSERT(idlOS::thread_mutex_lock(&(aHandle->mMutex)) == 0);

    sRC = checkServerOpen(aHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    /* block until server stoped or altibase_check_server_cancel() called */
    do
    {
        /* BUG-45135 */
        if (aHandle->mOptUseCancel == ID_TRUE)
        {
            IDE_TEST_RAISE(STATE_IS_CANCELED(aHandle), CancelOccurred);
            /* tryhold() ù�õ� ���� �α׸� ��´�. */
            if (sIsReTryhold == ID_FALSE)
            {
                checkServerLog(aHandle, "Waiting For Altibase Event...  ");
            }

            sRC = aHandle->mServerStat->tryhold();
            if (sRC != IDE_SUCCESS)
            {
                /* tryhold�� ������ �ֱ� (500 millisecond) */
                checkServerSleep(500);
                sServerRunning = ID_TRUE;
                sIsReTryhold = ID_TRUE;
                continue;
            }
            else
            {
                sIsReTryhold = ID_FALSE;
            }
        }
        else /* if (aHandle->mOptUseCancel == ID_FALSE) */
        {
            checkServerLog(aHandle, "Waiting For Altibase Event...  ");
            sRC = aHandle->mServerStat->hold();
            IDE_TEST_RAISE(sRC != IDE_SUCCESS, FileLockHoldError);
        }

        checkServerLog(aHandle, "[SUCCESS] Hold Lock-File.");

        sRC = checkServerRunByPortBinding(aHandle, &sServerRunning);
        IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

        sRC = aHandle->mServerStat->release();
        IDE_TEST_RAISE(sRC != IDE_SUCCESS, FileLockReleaseError);
        checkServerLog(aHandle, "[SUCCESS] Release Lock-File.");

        if (sServerRunning == ID_TRUE)
        {
            checkServerLog(aHandle, "The Server is already running.. retry to hold lock.");
            checkServerSleep(aHandle->mOptSleep);
        }
    }
    while (sServerRunning == ID_TRUE);

    IDE_ASSERT(idlOS::thread_mutex_unlock(&(aHandle->mMutex)) == 0);

    return ALTIBASE_CS_SERVER_STOPPED;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION(CancelOccurred);
    {
        sRC = ALTIBASE_CS_ABORTED_BY_USER;

        STATE_UNSET_CANCELED(aHandle);
    }
    IDE_EXCEPTION(FileLockHoldError);
    {
        checkServerLog(aHandle, "=> <<error>> FLOCK_WRLOCK()  errno=%d", errno);
        checkServerLog(aHandle, "[ERROR] Can't Hold Lock-File. errno=%d", errno);
        sRC = ALTIBASE_CS_FILE_LOCK_HOLD_ERROR;
    }
    IDE_EXCEPTION(FileLockReleaseError);
    {
        checkServerLog(aHandle, "=> << fatal >> File Lock Released.... errno=%d", errno);
        checkServerLog(aHandle, "[ERROR] Can't Release Lock-File. errno=%d", errno);
        sRC = ALTIBASE_CS_FILE_LOCK_RELEASE_ERROR;
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT(idlOS::thread_mutex_unlock(&(aHandle->mMutex)) == 0);

    return sRC;
}

/**
 * check server ������ �����.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerCancel (CheckServerHandle *aHandle)
{
    CHKSVR_RC sRC;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);
    IDE_TEST_RAISE(aHandle->mOptUseCancel == ID_FALSE, InvalidState);
    IDE_TEST_RAISE(STATE_IS_CANCELED(aHandle), InvalidState);

    STATE_SET_CANCELED(aHandle);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION(InvalidState);
    {
        sRC = ALTIBASE_CS_INVALID_STATE;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * check server ���μ����� ���δ�.
 *
 * @param[IN] aHandle �ڵ�
 * @return ��� �ڵ� ��
 */
CHKSVR_INTERNAL_FOR_EXTERNAL
CHKSVR_RC checkServerKill (CheckServerHandle *aHandle)
{
    pid_t     sPid;
    CHKSVR_RC sRC;

    IDE_TEST_RAISE(aHandle == ALTIBASE_CHECK_SERVER_NULL_HANDLE, InvalidHandle);

    IDE_ASSERT(idlOS::thread_mutex_lock(&(aHandle->mMutex)) == 0);

    sRC = checkServerPidFileLoad(aHandle, &sPid);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    sRC = checkServerKillProcess(sPid);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);

    sRC = checkServerPidFileRemove(aHandle);
    IDE_TEST(sRC != ALTIBASE_CS_SUCCESS);


    IDE_ASSERT(idlOS::thread_mutex_unlock(&(aHandle->mMutex)) == 0);

    return ALTIBASE_CS_SUCCESS;

    IDE_EXCEPTION(InvalidHandle);
    {
        sRC = ALTIBASE_CS_INVALID_HANDLE;
    }
    IDE_EXCEPTION_END;

    /* BUG-32370 */
    if (aHandle != NULL)
    {
        IDE_ASSERT(idlOS::thread_mutex_unlock(&(aHandle->mMutex)) == 0);
    }

    return sRC;
}

