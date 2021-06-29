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
 
/***********************************************************************
 * $Id$
 **********************************************************************/

/***********************************************************************
 * NAME
 *  ideLog.cpp
 *
 * DESCRIPTION
 *  This file defines methods for writing to trace log.
 *
 *   !!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!
 *   Don't Use IDE_ASSERT() in this file.
 **********************************************************************/

#include <idl.h>
#include <ideCallback.h>
#include <ideErrorMgr.h>
#include <ideLog.h>
#include <ideMsgLog.h>
#include <idp.h>
#include <iduMemMgr.h>
#include <iduProperty.h>
#include <iduShmProcType.h>
#include <iduStack.h>
#include <idtContainer.h>

extern UInt ideNoErrorYet(); /* BUG-47586 */

/* ----------------------------------------------------------------------
 *  BootLog Function (altibase_boot.log)
 * ----------------------------------------------------------------------*/

/*
 * BOOT ��⿡ ���� �޽��� �α��� ���� ideLogMsg ��ü�� �ʱ�ȭ �Ѵ�.
 * ������Ƽ �ε� ������ ȣ��Ǿ�� �Ѵ�.
 */

ideMsgLog       ideLog::mLogObj[IDE_LOG_MAX_MODULE];

/*
 * Process type string and process ID.
 */

SChar const * ideLog::mProcTypeStr = "";
ULong         ideLog::mPID;


/* Project 2514 */
const SChar *ideLog::mMsgModuleName[] =
{
    "ERROR",
#if defined(ALTIBASE_FIT_CHECK)
    /* PROJ-2581 */
    "FIT",
#endif
    "SERVER",
    "SM",
    "RP",
    "QP",
    "SD",   /* BUG-46138 */
    "DK",
    "XA",
    "MM",
    "RP_CONFLICT",
    "DUMP",
    "SNMP",
    "CM",
    /* 
     * Add additional module logs BEFORE miscellanous trc log
     * PROJ-2595
     */
    "MISC",
    "LB"     /* BUG-45274 */
};

/*
 * Substitute format specifiers according to the given template.  A
 * format specifier is a single letter following the letter %.  The
 * following is the list of accepted format specifiers and what they
 * are substituted to:
 *
 * %t - The XDB process type description.
 * %p - The process ID in decimal number.
 *
 * The function removes from output string the format specifiers that
 * it does not know about.
 */
void ideLog::formatTemplate( SChar       *aOutBuf,
                             UInt         aOutBufLen,
                             SChar const *aTemplate )
{
    SChar const *sProcTypeDesc = NULL;
    UInt         i;
    UInt         j;

#define IDE_TEMPL_EXIT_CONDITION ( ( j + 1 >= aOutBufLen ) || ( aTemplate[ i ] == '\0' ) )

    for ( i = 0, j = 0; IDE_TEMPL_EXIT_CONDITION == ID_FALSE; i++ )
    {
        if ( aTemplate[ i ] != '%' )
        {
            aOutBuf[ j++ ] = aTemplate[ i ];
        }
        else
        {
            ++i;
            IDE_TEST_CONT( IDE_TEMPL_EXIT_CONDITION == ID_TRUE, end_formatting );
            switch ( aTemplate[ i ] )
            {
            case 't':
                sProcTypeDesc = iduGetShmProcTypeDesc( iduGetShmProcType() );
                idlOS::strncpy( &aOutBuf[ j ], sProcTypeDesc, aOutBufLen - j );
                j += idlOS::strnlen( sProcTypeDesc, 8 );
                IDE_TEST_CONT( IDE_TEMPL_EXIT_CONDITION == ID_TRUE, end_formatting );
                break;

            case 'p':
                idlOS::snprintf( &aOutBuf[ j ], aOutBufLen - j, "%"ID_UINT64_FMT, mPID );
                j += idlOS::strnlen( &aOutBuf[ j ], 24 );
                IDE_TEST_CONT( IDE_TEMPL_EXIT_CONDITION == ID_TRUE, end_formatting );
                break;

            default:
                /* unknown format specifier */
                break;
            }
        }
    }

    IDE_EXCEPTION_CONT( end_formatting );
    aOutBuf[ j ] = '\0';
}

IDE_RC ideLog::initializeStaticBoot( iduShmProcType aProcType, idBool aDebug )
{
    SChar       sFileName[ID_MAX_FILE_NAME];
    SChar       sPath[ID_MAX_FILE_NAME];
    SChar const*sTemplate;
    UInt        sLogReserveSize = 0;

    /*
     * BOOT ���� ������ ���� �α�� : $ALTIBASE_HOME/trc/altibase_error.log �� ����������.
     */

    idlOS::umask(0);

    switch( aProcType )
    {
    case IDU_PROC_TYPE_DAEMON:
        mProcTypeStr = IDE_MSGLOG_SUBBEGIN_BLOCK"DAEMON"IDE_MSGLOG_SUBEND_BLOCK" ";
        break;
    case IDU_PROC_TYPE_WSERVER:
        mProcTypeStr = IDE_MSGLOG_SUBBEGIN_BLOCK"WSERVER"IDE_MSGLOG_SUBEND_BLOCK" ";
        break;
    case IDU_PROC_TYPE_RSERVER:
        mProcTypeStr = IDE_MSGLOG_SUBBEGIN_BLOCK"RSERVER"IDE_MSGLOG_SUBEND_BLOCK" ";
        break;
    case IDU_PROC_TYPE_USER:
        mProcTypeStr = IDE_MSGLOG_SUBBEGIN_BLOCK"USER"IDE_MSGLOG_SUBEND_BLOCK" ";
        break;
    default:
        mProcTypeStr = "";
        break;
    }

    mPID = (ULong)idlOS::getpid();

    // altibase_error.log
    idlOS::memset(sFileName, 0, ID_SIZEOF(sFileName));
    idlOS::memset(sPath, 0, ID_SIZEOF(sPath));

    formatTemplate( sFileName, ID_MAX_FILE_NAME, IDE_ERROR_LOG_FILE );
    idlOS::snprintf(sPath, ID_SIZEOF(sPath), "%s%strc%s%s",
                    idlOS::getenv(IDP_HOME_ENV),
                    IDL_FILE_SEPARATORS,
                    IDL_FILE_SEPARATORS,
                    sFileName);

    IDE_ASSERT(mLogObj[IDE_ERR].initialize(IDE_ERR, sPath, 0, 0) == IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_ERR].open(aDebug) != IDE_SUCCESS);

    // altibase_boot.log
    idlOS::memset(sFileName, 0, ID_SIZEOF(sFileName));
    idlOS::memset(sPath, 0, ID_SIZEOF(sPath));

    if ( aProcType == IDU_PROC_TYPE_USER )
    {
        sTemplate = IDE_USER_LOG_FILE;
        sLogReserveSize = 64 * 1024;
    }
    else
    {
        sTemplate = IDE_BOOT_LOG_FILE;
    }

    formatTemplate( sFileName, ID_MAX_FILE_NAME, sTemplate );
    idlOS::snprintf(sPath, ID_SIZEOF(sPath), "%s%strc%s%s",
                    idlOS::getenv(IDP_HOME_ENV),
                    IDL_FILE_SEPARATORS,
                    IDL_FILE_SEPARATORS,
                    sFileName);

    IDE_TEST(mLogObj[IDE_SERVER].initialize(IDE_SERVER, sPath, sLogReserveSize, 0) != IDE_SUCCESS);

    IDE_TEST(mLogObj[IDE_SERVER].open() != IDE_SUCCESS);

    // altibase_dump.log
    idlOS::memset(sFileName, 0, ID_SIZEOF(sFileName));
    idlOS::memset(sPath, 0, ID_SIZEOF(sPath));

    formatTemplate( sFileName, ID_MAX_FILE_NAME, IDE_DUMP_LOG_FILE );
    idlOS::snprintf(sPath, ID_SIZEOF(sPath), "%s%strc%s%s",
                    idlOS::getenv(IDP_HOME_ENV),
                    IDL_FILE_SEPARATORS,
                    IDL_FILE_SEPARATORS,
                    sFileName);

    IDE_ASSERT(mLogObj[IDE_DUMP].initialize(IDE_DUMP, sPath, 0, 0) == IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_DUMP].open() != IDE_SUCCESS);

#if defined(ALTIBASE_FIT_CHECK)

    // altibase_fit.log
    idlOS::memset(sFileName, 0, ID_SIZEOF(sFileName));
    idlOS::memset(sPath, 0, ID_SIZEOF(sPath));

    formatTemplate( sFileName, ACP_PATH_MAX_LENGTH, IDE_FIT_LOG_FILE );
    idlOS::snprintf( sPath, ID_SIZEOF(sPath), "%s%strc%s%s",
                     idlOS::getenv(IDP_HOME_ENV),
                     IDL_FILE_SEPARATORS,
                     IDL_FILE_SEPARATORS,
                     sFileName );

    IDE_ASSERT( mLogObj[IDE_FIT].initialize( IDE_SERVER, sPath, 0, 0 ) == IDE_SUCCESS );
    IDE_TEST( mLogObj[IDE_FIT].open() != IDE_SUCCESS );

#endif

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC ideLog::destroyStaticBoot()
{
    // IDE_ERR�� ���ؼ� �����ϰ�, SERVER�� ����Ѵ�.
    IDE_TEST(mLogObj[IDE_ERR].close() != IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_ERR].destroy() != IDE_SUCCESS);

    // IDE_BOOT�� ���ؼ� �����ϰ�, SERVER�� ����Ѵ�.
    IDE_TEST(mLogObj[IDE_SERVER].close() != IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_SERVER].destroy() != IDE_SUCCESS);

    // IDE_DUMP�� ���ؼ� �����ϰ�, SERVER�� ����Ѵ�.
    IDE_TEST(mLogObj[IDE_DUMP].close() != IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_DUMP].destroy() != IDE_SUCCESS);

#if defined(ALTIBASE_FIT_CHECK)

    // IDE_FIT�� ���ؼ� �����ϰ�, SERVER�� ����Ѵ�.
    IDE_TEST(mLogObj[IDE_FIT].close() != IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_FIT].destroy() != IDE_SUCCESS);

#endif

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ��� ��⿡ ���� �޽��� �α� ��ü�� ������Ƽ�� �ٰ��Ͽ� �ʱ�ȭ�Ѵ�.
 */

IDE_RC ideLog::initializeStaticModule(idBool aDebug)
{
    UInt  i = 0;
    SChar *sDir;
    UInt   sFileSize;
    UInt   sFileCount;
    UInt   sPerm;

    SChar  sFileNamePropertyName[512];
    SChar  sFileSizePropertyName[512];
    SChar  sFileCountPropertyName[512];

    SChar       sFileName[ID_MAX_FILE_NAME];
    SChar       sPath[ID_MAX_FILE_NAME];
    SChar      *sTemplate = sPath;

    UInt        sSuccessLogMgrCount = 0;
    UInt        sStage = 0;

    mPID = (ULong)idlOS::getpid();

    /* PROJ-2118 BUG Reporting
     * �� trace file ���� ���������� ��� ���� �����ߴ� ����
     * �ϳ��� ��� SERVER_MSGLOG_DIR�� �������� ����ϵ��� ����
     * */
    IDE_TEST_RAISE(idp::readPtr( "SERVER_MSGLOG_DIR",
                                 (void **)&sDir)
                   != IDE_SUCCESS, property_error );

    /*
     * PROJ-2595
     * Set permisison
     */
    IDE_TEST_RAISE(idp::read( "TRC_ACCESS_PERMISSION", &sPerm)
                   != IDE_SUCCESS, property_error );
    IDE_TEST_RAISE(ideMsgLog::setPermission(sPerm) != IDE_SUCCESS,
                   property_error);

    /*
     * altibase_error.log�� ���� �ʱ�ȭ�ϰ�
     * stdout, stderr�� ���� ���� �Ѵ�.
     */
    idlOS::memset(sPath, 0, ID_SIZEOF(sPath));
    idlOS::memset(sFileName, 0, ID_SIZEOF(sFileName));

    /*
     * ���� ���� �α��
     */
    for (i = IDE_ERR; i < IDE_LOG_MAX_MODULE; i++)
    {
        sStage = 0;

        idlOS::memset(sFileNamePropertyName,  0, ID_SIZEOF(sFileNamePropertyName));
        idlOS::memset(sFileSizePropertyName,  0, ID_SIZEOF(sFileSizePropertyName));
        idlOS::memset(sFileCountPropertyName, 0, ID_SIZEOF(sFileCountPropertyName));
        idlOS::memset(sPath,                  0, ID_SIZEOF(sPath));
        idlOS::memset(sFileName,              0, ID_SIZEOF(sFileName));

        idlOS::snprintf(sFileNamePropertyName,    ID_SIZEOF(sFileNamePropertyName),    "%s_MSGLOG_FILE",  mMsgModuleName[i]);
        idlOS::snprintf(sFileSizePropertyName,    ID_SIZEOF(sFileSizePropertyName),    "%s_MSGLOG_SIZE",  mMsgModuleName[i]);
        idlOS::snprintf(sFileCountPropertyName,   ID_SIZEOF(sFileCountPropertyName),   "%s_MSGLOG_COUNT", mMsgModuleName[i]);

        IDE_TEST_RAISE(idp::readPtr(sFileNamePropertyName,   (void **)&sTemplate)  != IDE_SUCCESS,
                       property_error);
        IDE_TEST_RAISE(idp::read   (sFileSizePropertyName,   (void  *)&sFileSize)  != IDE_SUCCESS,
                       property_error);
        IDE_TEST_RAISE(idp::read   (sFileCountPropertyName,  (void  *)&sFileCount) != IDE_SUCCESS,
                       property_error);

        formatTemplate( sFileName, ID_MAX_FILE_NAME, sTemplate );
        idlOS::snprintf(sPath, ID_SIZEOF(sPath), "%s%s%s",
                        sDir,
                        IDL_FILE_SEPARATORS,
                        sFileName);
        if ( i == IDE_RP_CONFLICT )
        {
            IDE_TEST(mLogObj[i].initialize((ideLogModule)i, sPath,
                                           sFileSize, sFileCount,
                                           iduProperty::getRpConflictTrcEnable() == 1?
                                           ID_TRUE:ID_FALSE)
                     != IDE_SUCCESS);
        }
        else if ( i == IDE_SD )
        {
            /* BUG-46138 */
            IDE_TEST(mLogObj[i].initialize((ideLogModule)i, sPath,
                                           sFileSize, sFileCount,
                                           iduProperty::getSdTrcEnable() == 1?
                                           ID_TRUE:ID_FALSE)
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(mLogObj[i].initialize((ideLogModule)i, sPath, sFileSize, sFileCount)
                     != IDE_SUCCESS);
        }
        sStage = 1;

        IDE_TEST(mLogObj[i].open(aDebug) != IDE_SUCCESS);
        sStage = 2;

        sSuccessLogMgrCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(property_error);
    {
        ideLog::log(IDE_SERVER_0, ID_TRC_PROPERTY_STATIC_INIT_ERROR, i);
        idlOS::exit(0);
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 2:
            (void)mLogObj[i].close();
        case 1:
            (void)mLogObj[i].destroy();
        default:
            break;
    }

    for ( i = IDE_ERR; 
          i < ( IDE_ERR + sSuccessLogMgrCount ); 
          i++ )
    {
        (void)mLogObj[i].close();
        (void)mLogObj[i].destroy();
    }

    IDE_POP();

    return IDE_FAILURE;
}

// BUG-40916
// extprocAgent���� msg log�� �ʱ�ȭ �� �� ����Ѵ�.
IDE_RC ideLog::initializeStaticForUtil()
{
    mPID = (ULong)idlOS::getpid();
    mProcTypeStr = "";

    return IDE_SUCCESS;
}

// BUG-40916
IDE_RC ideLog::destroyStaticForUtil()
{
    // Nothing to do.
    return IDE_SUCCESS;
}

void  ideLog::flushAllModuleLogs()
{
    /* Not needed anymore */
    UInt i;

    for (i = IDE_SERVER; i < IDE_LOG_MAX_MODULE; i++)
    {
        //  for preventing cm-detector from hanging.
        (void)mLogObj[i].flush();
    }
}


IDE_RC ideLog::destroyStaticModule()
{
    UInt  i;

    /*
     * ���� ���� �α��
     */
    for (i = IDE_SERVER; i < IDE_LOG_MAX_MODULE; i++)
    {
        IDE_TEST(mLogObj[i].close() != IDE_SUCCESS);
        IDE_TEST(mLogObj[i].destroy() != IDE_SUCCESS);
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC ideLog::destroyStaticError()
{
    /*
     * BUG-34491
     * altibase_error.log�� ������ ��찡 ������ ���� ó��
     */
    IDE_TEST(mLogObj[IDE_ERR].close() != IDE_SUCCESS);
    IDE_TEST(mLogObj[IDE_ERR].destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  External API for multiple message
 * ----------------------------------------------*/

void ideLog::logMessage(UInt aChkFlag, ideLogModule aModule, UInt aLevel, const SChar *aFormat, ... )
{
    ideMsgLog  *sLogObj;
    va_list     ap;

    PDL_UNUSED_ARG(aLevel);
    if ( aChkFlag != 0 )
    {
        va_start (ap, aFormat);

        sLogObj = &mLogObj[aModule];

        ideLogEntry sLog(sLogObj, ACP_TRUE, 0);
        sLog.appendArgs(aFormat, ap);
        sLog.write();

        va_end (ap);
    }
}

void ideLog::itoa(SLong aValue, SChar * aString, SInt aMaxStringLength, SInt aRadix, SInt aFormatLength )
{
    UInt sElement = 0;
    UInt sDegree  = 1;

    if ( aRadix == 0 )
    {
        aRadix = 10;
    }

    /* when the caller doesn't know the value of 'aFormatLength'
     * or there is no need to print formatively,
     * then the value of 'aFormatLength' can be '0'. */
    if (aFormatLength <= 0)
    {
        while(1)
        {
            if ((aValue/sDegree) > 0)
            {
                aFormatLength++;
            }
            else
            {
                break;
            }

            sDegree = sDegree * aRadix;
        }
    }

    /* validation check */
    if ( ( aString == NULL ) || ( aMaxStringLength <= aFormatLength ) )
    {
        IDE_RAISE(E_INVAL);
    }


    aString[aFormatLength] = 0;
    while( aValue > 0 )
    {
        sElement = aValue % aRadix;
        aValue /= aRadix;

        if ( sElement >= 10 )
        {
            aString[ --aFormatLength ] = 'a' + sElement - 10;
        }
        else
        {
            aString[ --aFormatLength ] = '0' + sElement;
        }
    }
    while( aFormatLength > 0 )
    {
        aString[ --aFormatLength ] = '0';
    }

    return;

    /* Exception Handling */
    IDE_EXCEPTION(E_INVAL)
    {
    }
    IDE_EXCEPTION_END;

    return;
}


/* ----------------------------------------------
 * External API for messages.
 * This is to void using anti-asynchronous-safe functions like fprintf, vfprintf
 * in a signal handler.
 *
 * @param  [in] aFD      : the file descriptor of a opened log file for logging.
 * @param  [in] aMessage : a message that you want to write
 *                         into the log file reffered to by the file descriptor.
 *
 * @return void
 * --------------------------------------------- */
void ideLog::logWrite(SInt aFD, const SChar *aMessage)
{
    idlOS::write((PDL_HANDLE)aFD, aMessage, idlOS::strnlen(aMessage, IDE_MESSAGE_SIZE));
}


/* ------------------------------------------------
 *  Single Log :
 * ----------------------------------------------*/

IDE_RC ideLog::logInternal(ideLogModule aModule, UInt aLevel, const SChar *aFormat, va_list aList)
{
    ideLogEntry sLog(1, aModule, aLevel);
    sLog.appendArgs(aFormat, aList);
    sLog.write();

    return IDE_SUCCESS;

}

// bug-24840 divide xa log
// XA log�� ���ٷ� ����� ���� �߰��� �Լ�
// logOpen -> logOpenLine �� ȣ���Լ� ����
IDE_RC ideLog::logInternalLine(ideLogModule aModule, UInt aLevel, const SChar *aFormat, va_list aList)
{
    ideLogEntry sLog(1, aModule, aLevel, ACP_FALSE);
    sLog.appendArgs(aFormat, aList);
    sLog.write();

    return IDE_SUCCESS;

}

void  ideLog::logErrorMsgInternal(ideLogEntry &aLog)
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    UInt errorCode = iderr->Stack.LastError;
    SChar* errMsg  = iderr->Stack.LastErrorMsg;

    if ( (iduProperty::getSourceInfo() & IDE_SOURCE_INFO_SERVER) == 0)
    {
        aLog.appendFormat("ERR-%05x(errno=%u) %s\n",
                          E_ERROR_CODE(errorCode),
                          errno,
                          errMsg);
    }
    else
    {
        /* ------------------------------------------------
         *  �ҽ��ڵ� ������ �Բ� ���.
         * ----------------------------------------------*/
        aLog.appendFormat(   "ERR-%05x"
                             " (%s:%x"
                             ")[errno=%x"
                             "] : %s\n",
                             E_ERROR_CODE(errorCode),
                             iderr->Stack.LastErrorFile,
                             iderr->Stack.LastErrorLine,
                             errno,
                             errMsg);
    }
}


void  ideLog::logWriteErrorMsgInternal(SInt aFD, UInt /*aLevel*/)
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    UInt         errorCode = iderr->Stack.LastError;
    SChar*       errMsg  = iderr->Stack.LastErrorMsg;
    SChar        sBuffer[IDE_MESSAGE_SIZE];

    if ( (iduProperty::getSourceInfo() & IDE_SOURCE_INFO_SERVER) == 0)
    {
        ideLog::logWrite(aFD, "ERR-");
        ideLog::itoa(E_ERROR_CODE(errorCode), sBuffer, IDE_MESSAGE_SIZE, 10, 5);
        ideLog::logWrite(aFD, sBuffer);
        ideLog::logWrite(aFD, "(errno=");
        ideLog::itoa(errno, sBuffer, IDE_MESSAGE_SIZE, 10, 0);
        ideLog::logWrite(aFD, sBuffer);
        ideLog::logWrite(aFD, ") ");
        ideLog::logWrite(aFD, errMsg);
        ideLog::logWrite(aFD, "\n");
    }
    else
    {
        /* ------------------------------------------------
         *  �ҽ��ڵ� ������ �Բ� ���.
         * ----------------------------------------------*/
        ideLog::logWrite(aFD, "ERR-");
        ideLog::itoa(E_ERROR_CODE(errorCode), sBuffer, IDE_MESSAGE_SIZE, 10, 5);
        ideLog::logWrite(aFD, sBuffer);
        ideLog::logWrite(aFD, " (");
        ideLog::logWrite(aFD, iderr->Stack.LastErrorFile);
        ideLog::logWrite(aFD, ":");
        ideLog::itoa(iderr->Stack.LastErrorLine, sBuffer, IDE_MESSAGE_SIZE, 10, 0);
        ideLog::logWrite(aFD, sBuffer);
        ideLog::logWrite(aFD, ")[errno=");
        ideLog::itoa(errno, sBuffer, IDE_MESSAGE_SIZE, 10, 0);
        ideLog::logWrite(aFD, sBuffer);
        ideLog::logWrite(aFD, "] : ");
        ideLog::logWrite(aFD, errMsg);
        ideLog::logWrite(aFD, "\n");
    }
}

void  ideLog::logCallStackInternal()
{
    //fix PROJ-1749
    if (gCallbackForDumpStack() == ID_TRUE)
    {
        iduStack::dumpStack();
    }
}

void  ideLog::logErrorMgrStackInternalForDebug(UInt aChkFlag, ideLogModule aModule, UInt aLevel)
{
#ifdef DEBUG
    UInt                 sErrorCode;
    ideErrorMgr         *sErrorMgr  = ideGetErrorMgr();
    SChar                sMessage[8192];
    UInt i;

    sErrorCode = ideGetErrorCode();
    idlOS::snprintf(sMessage,
                    ID_SIZEOF(sMessage),
                    "===================== ERROR STACK FOR ERR-%05X ======================\n",
                    E_ERROR_CODE(sErrorCode));

    (void)ideLog::logLine(aChkFlag, aModule, aLevel, sMessage );
    for (i = 0; i < sErrorMgr->ErrorIndex; i++)
    {
        idlOS::snprintf(sMessage,
                        ID_SIZEOF(sMessage),
                        " %" ID_INT32_FMT ": %s:%" ID_INT32_FMT,
                        i,
                        sErrorMgr->ErrorFile[i],
                        sErrorMgr->ErrorLine[i]);

        idlVA::appendFormat(sMessage, ID_SIZEOF(sMessage), "                                        ");

        idlVA::appendFormat(sMessage,
                            ID_SIZEOF(sMessage),
                            " %s\n",
                            sErrorMgr->ErrorTestLine[i]);
        (void)ideLog::logLine(aChkFlag, aModule, aLevel, sMessage );
    }

    idlOS::snprintf(sMessage,
                    ID_SIZEOF(sMessage),
                    "======================================================================\n");
    (void)ideLog::logLine(aChkFlag, aModule, aLevel, sMessage );
#else
    PDL_UNUSED_ARG(aChkFlag);
    PDL_UNUSED_ARG(aModule);
    PDL_UNUSED_ARG(aLevel);
#endif

}

/* ---------------------------------------------
 * TASK-4007 [SM]PBT�� ���� ��� �߰�
 *
 * �������� ���� Hex������ String���� ��ȯ
 * aSrcPtr      [in]    ����� ���� �޸� ��ġ
 * aSrcSize     [in]    Dump�� ���� �޸� ũ�� (Byte)
 * aFormatFlag  [in]    ��¹��� ���� Format
 * aDstPtr      [out]   ������� ����� ����
 * aDstSize     [out]   ������ ũ�� (Byte)
 *
 * ��¿�)
 *     Abso  Rela            Body                        Character
 *
 * ff0000a4: 0000| 8f044547 00000000 00000000 6c080000 ; ..EG........l...
 * ff0000b4: 0010| 801f801f 20000000 7000f01f 15000000 ; .... ...p.......
 * ff0000c4: 0020| 00000000 00000000 00000000 00000000 ; ................
 *
 * --------------------------------------------- */
IDE_RC ideLog::ideMemToHexStr( const UChar  * aSrcPtr,
                               UInt           aSrcSize,
                               UInt           aFormatFlag,
                               SChar        * aDstPtr,
                               UInt           aDstSize )
{
    UInt   i;
    UInt   j;
    UInt   k;
    UInt   sLineSize;
    UInt   sBlockSize;

    /* ==========================================
     * Parameter Validation
     * ========================================== */

    IDE_TEST( aSrcPtr             == NULL );
    IDE_TEST( aDstPtr             == NULL );
    IDE_TEST( IDE_DUMP_DEST_LIMIT <  aDstSize );
     /* aDstSize�� ����ġ�� Ŭ ���, Buffer�� write�ص� �Ǵ��� �ǽɽ�����
      * ������ Buffer�� write���� ���� failure�� �ٷ� �����Ѵ�. */

    if ( IDE_DUMP_SRC_LIMIT < aSrcSize )
    {
        // aSrcSize ���ڰ� ���Ѱ��� �ѱ�� ���� ���� �������� ���
        // (memory scratch)�� Ȯ���� ����. ���� ������ �����Ű��
        // ����, ũ�⸦ ���� ���� ����Ѵ�.
        aSrcSize = IDE_DUMP_SRC_LIMIT;
    }


    /* ==========================================
     * Piece Format
     * ========================================== */

    switch( aFormatFlag & IDE_DUMP_FORMAT_PIECE_MASK )
    {
    case IDE_DUMP_FORMAT_PIECE_4BYTE:
        sLineSize  = 32;
        sBlockSize  = 4;
        break;
    case IDE_DUMP_FORMAT_PIECE_SINGLE:
    default:
        sLineSize  = aSrcSize;
        sBlockSize = aSrcSize;
        break;
    }

    aDstPtr[ 0 ] = '\0';


    /* ==========================================
     * Converting
     * ========================================== */
    // Loop�� ���� ���پ� ������ش�. ���ٴ� LineSize��ŭ ����Ѵ�.
    for( i = 0 ; i < aSrcSize; i += sLineSize )
    {
        /* ==========================================
         * Address Format
         * ========================================== */
        switch( aFormatFlag & IDE_DUMP_FORMAT_ADDR_MASK )
        {
        case IDE_DUMP_FORMAT_ADDR_NONE:
            break;
        case IDE_DUMP_FORMAT_ADDR_ABSOLUTE:
            idlVA::appendFormat( aDstPtr,
                                 aDstSize,
                                 "%016"ID_xPOINTER_FMT": ",
                                 aSrcPtr + i);
            break;
        case IDE_DUMP_FORMAT_ADDR_RELATIVE:
            idlVA::appendFormat( aDstPtr,
                                 aDstSize,
                                 "%08"ID_xINT32_FMT"| ",
                                 i);
            break;
        case IDE_DUMP_FORMAT_ADDR_BOTH:
            idlVA::appendFormat( aDstPtr,
                                 aDstSize,
                                 "%016"ID_xPOINTER_FMT": "
                                 "%08"ID_xINT32_FMT"| ",
                                 aSrcPtr + i,
                                 i);
            break;
        default:
            break;
        }


        /* ==========================================
         * Body Format
         * ========================================== */
        switch( aFormatFlag & IDE_DUMP_FORMAT_BODY_MASK )
        {
        case IDE_DUMP_FORMAT_BODY_HEX:
            for( j = 0 ; ( j < sLineSize ) && ( i + j < aSrcSize ) ; j += sBlockSize )
            {
                //Block�� ������ �ϳ��ϳ� ����Ѵ�. ���޹��� �޸� �ּҰ�
                //Align �´´ٴ� ������ ���� ������ �ѹ���Ʈ�� ����ش�.
                for( k = 0 ;
                     ( k < sBlockSize ) &&
                     ( j < sLineSize )  &&
                     ( i + j + k < aSrcSize ) ;
                     k ++)
                {
                    idlVA::appendFormat( aDstPtr,
                                         aDstSize,
                                         "%02"ID_xINT32_FMT,
                                         aSrcPtr[ i + j + k] );
                }

                // BlockSize��ŭ HexStr�� ���������,
                // Line�� �������� �ƴ� ���, ���� ���
                if ( ( k == sBlockSize ) && ( j != sLineSize ) )
                {
                    idlVA::appendFormat( aDstPtr, aDstSize, " " );
                }
            }
            break;
        case IDE_DUMP_FORMAT_BODY_NONE:
            break;
        default:
            break;
        }


        /* ==========================================
         * Character Format
         * ========================================== */
        switch( aFormatFlag & IDE_DUMP_FORMAT_CHAR_MASK )
        {
        case IDE_DUMP_FORMAT_CHAR_ASCII:
            idlVA::appendFormat( aDstPtr,
                                 aDstSize,
                                 "; " );
            for( j = 0 ; ( j < sLineSize ) && ( i + j < aSrcSize ) ; j ++ )
            {
                //����(32)�� ~(126)������ ������ Ascii�� ��°����� �͵�
                //�̴�. �׷� �͵鸸 ����ش�.
                idlVA::appendFormat( aDstPtr,
                                     aDstSize,
                                     "%c",
                                     ( isprint( aSrcPtr[ i + j] ) != 0 )
                                     ? aSrcPtr[ i + j] : '.' );
            }
            break;
        case IDE_DUMP_FORMAT_CHAR_NONE:
            break;
        default:
            break;
        }

        /*
         * [BUG-29740] [SM] dumplf���� �α� ����� 0�� ���
         *             ��� ���˿� ������ �ֽ��ϴ�
         */

        // aSrc�� �������� �ƴ� ���, ������
        if ( (i + sLineSize) < aSrcSize )
        {
            idlVA::appendFormat( aDstPtr,
                                 aDstSize,
                                 "\n" );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC ideLog::logMem( UInt           aChkFlag,
                       ideLogModule   aModule,
                       UInt           aLevel,
                       const UChar  * aPtr,
                       UInt           aSize )
{
    if (aChkFlag != 0)
    {
        ideLogEntry sLog(aChkFlag, aModule, aLevel);
        IDE_TEST( sLog.dumpHex( aPtr, aSize, IDE_DUMP_FORMAT_FULL )
                   != IDE_SUCCESS );
        sLog.write();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ---------------------------------------------
 * PROJ-2118 BUG Reporting
 *
 * �����͸� �Ѿư� �޸� ������ Binary��
 * Hex�� ��ȯ�Ͽ� TRC Log�� ������ش�.
 *
 * ����Ϸ��� Hexa Data�� �������� �� �� �ֵ���
 * �߰� ���ڿ��� ����ϴ� ��� �߰�
 *
 * aChkFlag     [in]    ChkFlag
 * aModule      [in]    Log�� ���� Module
 * aLevel       [in]    LogLevel
 * aPtr         [in]    ����� �޸� ��ġ
 * aSize        [in]    Dump�� �޸� ũ�� (Byte)
 * aFormat      [in]    �߰� ����� ���ڿ��� ���� Format
 * ...
 * --------------------------------------------- */
IDE_RC ideLog::logMem( UInt           aChkFlag,
                       ideLogModule   aModule,
                       UInt           aLevel,
                       const UChar  * aPtr,
                       UInt           aSize,
                       const SChar  * aFormat,
                       ... )
{
    va_list   ap;

    if (aChkFlag != 0)
    {
        ideLogEntry sLog(aChkFlag, aModule, aLevel);

        va_start( ap, aFormat );
        sLog.appendArgs( aFormat, ap );
        va_end( ap );

        sLog.append( "\n" );

        IDE_TEST( sLog.dumpHex( aPtr, aSize, IDE_DUMP_FORMAT_FULL )
                   != IDE_SUCCESS );

        sLog.write();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * PROJ-2118 BUG Reporting
 *
 * ������ Property�� ���� ����ó�� Ȥ�� Assert�� ó���Ѵ�.
 * Assert�� ��� Dump��, ���� ó�� �� ��� Error.log�� ����Ѵ�.
 *
 * __ERROR_VALIDATION_LEVEL = 0 : ���ܷ� ó��, Error Message ��� ������
 *                                __WRITE_ERROR_TRACE Property�� ������.
 * __ERROR_VALIDATION_LEVEL = 1 : Message�� ����ϰ� Assert�� ó�� (default)
 * __WRITE_ERROR_TRACE = 0 : Error Message�� ���� �ʴ´�.
 * __WRITE_ERROR_TRACE = 1 : Error Message�� ����Ѵ�. (default)
 *
 *    aErrInfo     [in] ������ ã�Ƴ� ����
 *    aFileName    [in] ���� �߻� ����
 *    aLineNum     [in] ���� ���� ��ġ
 *    aFormat      [in] �߰� ��� �� Message�� Format
 *    ...          [in] �߰� ��� �� Message�� ����
 ****************************************************************************/
void ideLog::writeErrorTrace( const SChar * aErrInfo,
                              const idBool  aAcceptFaultTolerance, /* PROJ-2617 */
                              const SChar * aFileName,
                              const UInt    aLineNum,
                              const SChar * aFormat,
                              va_list       ap )
{

    if ( iduProperty::getErrorValidationLevel() == IDE_ERROR_VALIDATION_LEVEL_FATAL )
    {

        writeErrorTraceInternal( IDE_ERR_0,
                                 aErrInfo,
                                 aFileName,
                                 aLineNum,
                                 aFormat,
                                 ap );

        if (gCallbackForAssert( (SChar *)idlVA::basename( aFileName ),
                                aLineNum, aAcceptFaultTolerance ) == ID_TRUE)
        {
            os_assert(0);
            idlOS::exit(-1);
        }
        else
        {
            /* PROJ-2617 */
            ideNonLocalJumpForFaultTolerance();
        }
    }
    else
    {
        if ( iduProperty::getWriteErrorTrace() == ID_TRUE )
        {

            writeErrorTraceInternal( IDE_ERR_0,
                                     aErrInfo,
                                     aFileName,
                                     aLineNum,
                                     aFormat,
                                     ap );
        }

        /* BUG-47586 */
        /* it means nobody set the error code yet.*/
        if( ideNoErrorYet() == ID_TRUE )
        {
            IDE_SET( ideSetErrorCode( idERR_ABORT_InternalServerError )); 
        }
        
    }
}

/***************************************************************************
 * PROJ-2118 BUG Reporting
 *
 * Error Message�� ����ϴ� ideLog::writeErrorTrace �� ���� �Լ�.
 *
 ****************************************************************************/
void ideLog::writeErrorTraceInternal( UInt          aChkFlag,
                                      ideLogModule  aModule,
                                      UInt          aLevel,
                                      const SChar * aErrInfo,
                                      const SChar * aFileName,
                                      const UInt    aLineNum,
                                      const SChar * aFormat,
                                      va_list       ap )
{
    ideLogEntry sLog(  aChkFlag,
                       aModule,
                       aLevel );

    sLog.appendFormat( "%s[%s:%u], errno=[%u]\n",
                       aErrInfo,
                       (SChar *)idlVA::basename( aFileName ),
                       aLineNum,
                       errno );

    if ( aFormat != NULL )
    {
        /* va_list ���� logMessage�Լ�, Ceck Flag �� ������� �ʴ´�. */
        sLog.appendArgs( aFormat,
                         ap );

        sLog.append( "\n" );
    }

    logCallStackInternal();

    sLog.write();
}




