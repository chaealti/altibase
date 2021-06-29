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
 * $Id: iSQLMain.cpp 88494 2020-09-04 04:29:31Z chkim $
 **********************************************************************/

#include <idp.h>
#include <uttMemory.h>
#include <utString.h>
#include <iSQLProperty.h>
#include <iSQLProgOption.h>
#include <iSQLCompiler.h>
#include <iSQLCommandQueue.h>
#include <iSQLExecuteCommand.h>
#include <iSQLHostVarMgr.h>
#ifdef USE_READLINE
#include <histedit.h>
#endif

extern SInt iSQLParserparse(void *);
extern SInt iSQLPreLexerlex();

/* BUG-41172 Passing Parameters through the START Command */
extern void lexSubstituteVars();


/* Caution:
 * Right 3 nibbles of gErrorMgr.mErrorCode must not be read,
 * because they can have dummy values. */
uteErrorMgr          gErrorMgr;

iSQLProgOption       gProgOption;
iSQLProperty         gProperty;
iSQLHostVarMgr       gHostVarMgr;
iSQLBufMgr         * gBufMgr;
iSQLCompiler       * gSQLCompiler;
iSQLCommand        * gCommand;
iSQLCommand        * gCommandTmp;
iSQLCommandQueue   * gCommandQueue;
iSQLExecuteCommand * gExecuteCommand;
utISPApi           * gISPApi;
iSQLSpool          * gSpool;
uttMemory          * g_memmgr;

/* bug 18731 */
SInt     gLastReturnValue;

SChar  * gTmpBuf; // for iSQLParserparse
idBool   g_glogin;      // $ALTIBASE_HOME/conf/glogin.sql
idBool   g_login;       // ./login.sql
idBool   g_inLoad;
idBool   g_inEdit;
idBool   g_isSIGINT; // fix BUG-19750
idBool   gSameUser;
SChar    QUERY_LOGFILE[256];

#ifdef USE_READLINE
EditLine *gEdo = NULL;
History  *gHist;
HistEvent gEvent;
#endif

void   gSetInputStr(SChar * s);
void   ShowCopyRight(void);

IDE_RC gExecuteGlogin();
IDE_RC gExecuteLogin();
IDE_RC SaveCommandToFile(SChar * szCommand, SChar * szFile);

SInt LoadFileData(const SChar *file, UChar **buf);
void QueryLogging( const SChar *aQuery );
void InitQueryLogFile();

void isqlFinalizeLineEditor( void );
void isqlExitHandler( void );
void Exit( int aStatus );

#ifdef USE_READLINE
SChar * isqlprompt(EditLine * /*__ el __*/)
{
    static SChar * sSqlPrompt;
    static SChar * sDefaultPrompt = ISQL_PROMPT_DEFAULT_STR;
    static SChar * sNoPrompt = ISQL_PROMPT_OFF_STR; /* BUG-29760 */

    sSqlPrompt = gProperty.GetSqlPrompt();

    if (gProgOption.IsNoPrompt() == ID_TRUE) /* BUG-29760 */
    {
        return sNoPrompt;
    }
    else 
    {
        if ((gProperty.IsSysDBA() == ID_TRUE) && 
            (gProgOption.IsATC() == ID_TRUE))
        {
            return sDefaultPrompt;
        }
        else
        {
            return sSqlPrompt;
        }
    }
}
#endif

/* BUG-46019 */
void ProcedurePrintCallback( acp_uint8_t *aMessage,
                             acp_uint32_t aLength,
                             void * /*aUserData*/ )
{
    idlOS::memcpy(gSpool->m_Buf, aMessage, aLength);
    gSpool->m_Buf[aLength] = '\0';
    gSpool->Print();
}

SQLMessageCallbackStruct gMessageCallback =
{
    ProcedurePrintCallback, NULL
};

IDL_EXTERN_C_BEGIN
static void sigfunc( SInt /* signo */ )
{
    if (gExecuteCommand->m_ISPApi->IsSQLExecuting() == ID_TRUE)
    {
        if (gExecuteCommand->m_ISPApi->Cancel() != IDE_SUCCESS)
        {
            /* BUG-43489 prevent isql hanging from SIGINT */
            uteSprintfErrorCode(gSpool->m_Buf,
                                gProperty.GetCommandLen(),
                                &gErrorMgr);
            gSpool->Print();
        }
    }

    /* BUG-32568 Support Cancel
     * ���� Fetch ������ Ȯ������ �ʴ´�. �� �����ϳ� Ȯ���ϳ� �װ� �װŶ�.. */
    gExecuteCommand->FetchCancel();

    // fix BUG-19750
    g_isSIGINT = ID_TRUE;

    idlOS::printf("\n");
    idlOS::signal(SIGINT, sigfunc);
}
IDL_EXTERN_C_END

IDE_RC checkUser()
{
#ifndef VC_WIN32
    uid_t       uid;
    PDL_stat    sBuf;
    SChar       sExFileName[WORD_LEN];
    SChar*      sHomePath = NULL;

    uid = idlOS::getuid();

    // bug-26749 windows isql filename parse error
    // windows���� bin/isql -sysdba �̷� ������ �����ϸ�
    // �ڿ� exe�� ������ �ʾ� error �߻�.
    // �������:
    // 1. windows�� ��� �ǹ� ���� �ڵ忴���Ƿ� �ƿ� �˻����� ����.
    // 2. isql ���� userid �˻縦 altibase_home dir userid �˻�� ����.
    // why? isql ������ �����Ͽ� �����ϸ� �˻縦 ���ذ� �� �����Ƿ�.
    // �׷���, altibase_home�� �ڱ� dir�� ������ �����ϸ�
    // ������ �˻縦 ���Ҽ��� �ִ�. ������, dir �˻簡 �� �� ���ٴ�
    // bug reporter ���� �ǰ��� �־� �׷��� ����� �Ѵ�.
    // ��¥�� �ּ����� �˻��̹Ƿ� ũ�� �ǹ̸� ���� �ʵ��� ����.
    // cf) ora�� ��� ���� error �� �߻������� log file create ������,
    // altió�� ���� �˻縦 ������ �ʴ� �� ����.
    sHomePath = idlOS::getenv(IDP_HOME_ENV);
    IDE_TEST_RAISE(((sHomePath == NULL) || (sHomePath[0] == '\0')), env_error);
    // bug-33948: codesonar: Buffer Overrun: strcpy
    idlOS::strncpy(sExFileName, sHomePath, WORD_LEN-1);
    sExFileName[WORD_LEN-1] = '\0';

    // cf) ���� ����ȭ testcase���� stat error 2 �� �߻��Ѵٸ� �̰���
    // ataf���� altibase_home ȯ�� ������ Ȯ����� �ѱ�� ������.
    // ex) server.conf: ALTIBASE_HOME=$ATC_HOME/TC/Server/repl4/db1
    // �ذ���: sql ���Ͼȿ��� ���� connect ���� ���� ���� isql ȣ�� ���.
    // ex) connect .. as sysdba => SYSTEM @DB1 isql .. -sysdba
    IDE_TEST_RAISE( idlOS::stat(sExFileName, &sBuf) != 0, stat_error );

    if ( uid == sBuf.st_uid )
    {
        gSameUser = ID_TRUE;
    }
    else
    {
        gSameUser = ID_FALSE;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(env_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_env_not_exist, IDP_HOME_ENV);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(stat_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_stat_error, (UInt)errno);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

// bug-26749 windows isql filename parse error
// windows�� ��� sysdba ���ӽ� ���� userid �˻縦 ���� �ʵ��� ��.
// Win32's stat return uid as 0
#else
    gSameUser = ID_TRUE;
    return IDE_SUCCESS;
#endif
}

int
main( int     argc,
      char ** argv )
{
    SChar  * empty; // for iSQLParserparse, but not use
    SInt     commandLen;
    SInt     ret;
#if !defined(VC_WIN32)
    SInt     sConnType;
#endif

    ret        = IDE_FAILURE;
    g_inLoad   = ID_FALSE;
    g_inEdit   = ID_FALSE;
    g_isSIGINT = ID_FALSE; // fix BUG-19750
    g_glogin   = ID_FALSE; // SetScriptFile���� �̰� üũ
    g_login    = ID_FALSE;
    empty      = NULL;
    gTmpBuf    = NULL;
    /* bug 18731 */
    /* ������ ����� return�� �����Ѵ�. */
    gLastReturnValue = IDE_SUCCESS;

    commandLen = gProperty.GetCommandLen();
    /* BUG-33003 :  Codesonar warnings at UL module */
    /* ============================================
     * Check ISQL_BUFFER_SIZE value
     * ============================================ */
    if ( commandLen < COMMAND_LEN)
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_command_buffer_Error,
                        (SChar*)ENV_ISQL_PREFIX"BUFFER_SIZE", (UInt)COMMAND_LEN);
        utePrintfErrorCode(stderr, &gErrorMgr);
        Exit(0);
    }

    gCommand        = new iSQLCommand();
    gCommandTmp     = new iSQLCommand();
    gSpool          = new iSQLSpool();
    gISPApi         = new utISPApi(commandLen, &gErrorMgr);
    gExecuteCommand = new iSQLExecuteCommand(commandLen, gSpool, gISPApi);
    gCommandQueue   = new iSQLCommandQueue(gSpool);
    gSQLCompiler    = new iSQLCompiler(gSpool);
    gBufMgr         = new iSQLBufMgr(commandLen, gSpool);
    g_memmgr        = new uttMemory;

    g_memmgr->init();

    /* BUG-31874 */
    IDE_TEST_RAISE(NULL == idlOS::getenv(IDP_HOME_ENV), envhome_error);

    /* ============================================
     * gTmpBuf initialization
     * ============================================ */
    if ( (gTmpBuf = (SChar*)idlOS::malloc(commandLen)) == NULL )
    {
        idlOS::fprintf(stderr,
                       "Memory allocation error!!! --- (%"ID_INT32_FMT", %s)\n",
                        __LINE__,
                        __FILE__);
        Exit(0);
    }
    idlOS::memset(gTmpBuf, 0x00, commandLen);

    /* ============================================
     * Signal Processing Setting
     * Ignore ^C, ^Z, ...
     * ============================================ */
    idlOS::signal(SIGINT, sigfunc);
    idlOS::signal(SIGPIPE, SIG_IGN);
#ifndef VC_WINCE
#ifdef _MSC_VER //BUGBUG I don't know what is needed instead of SIGTSTP
    idlOS::signal(SIGTERM, SIG_IGN);
#else
    idlOS::signal(SIGTSTP, SIG_IGN);
#endif
#endif

    /* ============================================
     * Get option with command line
     * ============================================ */
    IDE_TEST(gProgOption.ParsingCommandLine(argc, argv) != IDE_SUCCESS);

    /* ============================================
     * If not silent option, print copyright
     * ============================================ */
    if (gProgOption.IsSilent() == ID_FALSE)
    {
        ShowCopyRight();
    }

    /* BUG-31387: ConnType�� �����ϰ� ��쿡 ���� ��� ���.
       ConnType�� ReadEnviroment(), ReadServerProperties()����
       �����ϹǷ� �� �Լ����� ȣ���ϱ� ���� ȣ��. */
    gProgOption.AdjustConnType();

    /* ============================================
     * Get option with enviroment
     * ============================================ */
    IDE_TEST(gProgOption.ReadEnvironment() != IDE_SUCCESS);

    // BUG-26287: ������ altibase.properties�� ���� (for server)
    gProgOption.ReadServerProperties();

    /* ============================================
     * Get option with interactive (-s, -u, -p)
     * ============================================ */
    IDE_TEST(gProgOption.ReadProgOptionInteractive() != IDE_SUCCESS);

    if ( gProgOption.IsSilent() == ID_FALSE )
    {
#if !defined(VC_WIN32)
        sConnType = gProperty.GetConnType(gProperty.IsSysDBA(), gProgOption.GetServerName());
        if ( (sConnType == ISQL_CONNTYPE_IPC)
             || (sConnType == ISQL_CONNTYPE_IPCDA)
             || (sConnType == ISQL_CONNTYPE_UNIX) )
        {
            idlOS::fprintf(gProgOption.m_OutFile,
                           ENV_ISQL_CONNECTION" = %s, SERVER = %s\n",
                           gProperty.GetConnTypeStr(),
                           gProgOption.GetServerName());
        }
        else
#endif
        {
            idlOS::fprintf(gProgOption.m_OutFile,
                           ENV_ISQL_CONNECTION" = %s, SERVER = %s,"
                           " PORT_NO = %"ID_INT32_FMT"\n",
                           gProperty.GetConnTypeStr(),
                           gProgOption.GetServerName(),
                           gProgOption.GetPortNo());
        }
    }

    gSameUser = ID_FALSE;
    /* ============================================
     * Connect to Altibase Server
     * ============================================ */
    if (gProgOption.IsATC() == ID_FALSE)
    {
        // bypass user check if atc mode is true
        
        // bug-26749 windows isql filename parse error
        // sysdba�� ������ ��츸 exefile userid �˻�
        if (gProperty.IsSysDBA() == ID_TRUE)
        {
            IDE_TEST(checkUser() != IDE_SUCCESS);
            IDE_TEST_RAISE(gSameUser != ID_TRUE, sysdba_error);
        }
    }
    
    /* BUG-41163 SET SQLP[ROMPT] */
    gProperty.InitSqlPrompt();

    /* BUG-41476 /NOLOG option */
    IDE_TEST_CONT(gProgOption.IsNOLOG() == ID_TRUE, skip_connect);

    IDE_TEST_RAISE(gExecuteCommand->ConnectDB() != IDE_SUCCESS, connect_error);

    if ( (gProperty.IsConnToRealInstance() == ID_FALSE) &&
         (gProgOption.IsATC() == ID_FALSE) &&
         (gProgOption.IsATAF() == ID_FALSE) )
    {
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }

    IDE_EXCEPTION_CONT(skip_connect);

    InitQueryLogFile();

    if ( gProgOption.IsInFile() == ID_TRUE )
    {
        /* ============================================
         * Case of -f option
         * SetFileRead   : ���� ������ ������ isql �����ϱ� ����
         * SetScriptFile : input file setting
         * ============================================ */
        gSQLCompiler->SetFileRead(ID_TRUE);
        IDE_TEST_RAISE( gSQLCompiler->SetScriptFile(gProgOption.GetInFileName(),
                                                   ISQL_PATH_CWD,
                                                   gCommand->GetPassingParams())
                        != IDE_SUCCESS, exit_pos );
    }
    else
    {
        /* ============================================
         * -f option�� �ƴ� ���
         * RegStdin : input file setting with stdin
         * ============================================ */
        gSQLCompiler->RegStdin();
    }

    g_glogin = ID_TRUE;
    g_login  = ID_TRUE;

    if ( gExecuteLogin() == IDE_SUCCESS )
    {
        g_login = ID_TRUE;
    }
    else
    {
        g_login = ID_FALSE;
    }
    if ( gExecuteGlogin() == IDE_SUCCESS )
    {
        g_glogin = ID_TRUE;
    }
    else
    {
        g_glogin = ID_FALSE;
    }

#ifdef USE_READLINE
    if ( gProgOption.UseLineEditing() == ID_TRUE )
    {
        SChar * sEditor;

        gHist = history_init();
        history(gHist, &gEvent, H_SETSIZE, 100);

        /* BUG-45145 Need to enhance history */
        history(gHist, &gEvent, H_SETUNIQUE, 1);
        if (gProperty.GetHistFile() != NULL) // BUG-46552
        {
            history(gHist, &gEvent, H_LOAD, gProperty.GetHistFile());
        }

        sEditor = idlOS::getenv(ENV_ISQL_EDITOR);

        gEdo = el_init(*argv, stdin, stdout, stderr);
        
        el_set(gEdo, EL_PROMPT, isqlprompt);
        el_set(gEdo, EL_HIST, history, gHist);
        el_set(gEdo, EL_SIGNAL, 1);
        el_set(gEdo, EL_TERMINAL, NULL);
        
        if (sEditor != NULL)
        {
            el_set(gEdo, EL_EDITOR, sEditor);
        }
        else
        {
            el_set(gEdo, EL_EDITOR, (SChar *)"emacs");
        }
    }
#endif

    while(1)
    {
        g_memmgr->freeAll();
        gBufMgr->Reset();
        gCommand->reset();
        // ISQL_COMMENT2�̸� iSQL>���� �ּ����� �����Ͽ� �ּ��� �������� ���κ������� �ʰ� ��ɾ �̾ ����� ���.
        // �� ��� ������Ʈ�� ������� �ʰ� ���γѹ��� ����ؾ� ��.
        if ( ret != ISQL_COMMENT2 &&
             g_inEdit != ID_TRUE &&
             g_glogin == ID_FALSE &&
             g_login == ID_FALSE )
        {
            gSQLCompiler->PrintPrompt();
        }
        ret = iSQLPreLexerlex();
        if ( ret == ISQL_EMPTY )
        {
            continue;
        }
        else if ( ret == ISQL_COMMENT || ret == ISQL_COMMENT2 )
        {
            gSQLCompiler->PrintCommand();
            continue;
        }
        else if ( ret == IDE_FAILURE )
        {
            // only space
            idlOS::strcpy(gTmpBuf, gBufMgr->GetBuf());
            utString::eraseWhiteSpace(gTmpBuf);
            if ( gTmpBuf[0] == '\n' )
            {
                continue;
            }
        }
        else
        {
            /* do nothing */
        }

        gSQLCompiler->PrintCommand();

        // BUG-41173 Passing Parameters through the START command
        if (gProperty.GetDefine() == ID_TRUE)
        {
            lexSubstituteVars();
        }

        gSetInputStr(gBufMgr->GetBuf());

        gCommand->mExecutor = NULL; // BUG-42811

        ret = iSQLParserparse(empty);
        if ( ret == ISQL_UNTERMINATED )
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_Unterminated_error);
            uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
            gSpool->Print();
            if( gProgOption.IsATAF() == ID_TRUE )
            {
                (void)idlOS::printf( "\n$$EOF$$\n" );
                (void)idlOS::fflush( stdout );
            }
            if (gProgOption.IsInFile() ) /* ��ɾ ������ �ʰ� ������ ������ ��� with @ */
            {
                IDE_RAISE(exit_pos);
            }
            else /* ��ɾ ������ �ʰ� ������ ������ ��� with @ */
            {
                continue;
            }
        }
        else if ( ret == 1 )
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_Syntax_Error);
            uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
            gSpool->Print();
            if( gProgOption.IsATAF() == ID_TRUE )
            {
                (void)idlOS::printf( "\n$$EOF$$\n" );
                (void)idlOS::fflush( stdout );
            }
            continue;
        }
        else if ( ret == ISQL_COMMAND_SEMANTIC_ERROR )
        {
            uteSprintfErrorCode(gSpool->m_Buf,
                                gProperty.GetCommandLen(),
                                &gErrorMgr);
            gSpool->Print();
            if( gProgOption.IsATAF() == ID_TRUE )
            {
                (void)idlOS::printf( "\n$$EOF$$\n" );
                (void)idlOS::fflush( stdout );
            }
            continue;
        }
        if (g_inLoad == ID_TRUE)
        {
            g_inLoad = ID_FALSE;
            gCommandQueue->AddCommand(gCommand);
            gSQLCompiler->ResetInput();
            idlOS::sprintf(gSpool->m_Buf, "Load completed.\n");
            gSpool->Print();
            continue;
        }

        if (g_inEdit == ID_TRUE)
        {
            g_inEdit = ID_FALSE;
            gCommandQueue->AddCommand(gCommand);
            gSQLCompiler->ResetInput();
            continue;
        }
/*
        if (inChange == ID_TRUE)
        {
            inChange = ID_FALSE;
            gCommandQueue->AddCommand(gCommand);
            idlOS::sprintf(gSpool->m_Buf, "%s\n", gCommand->GetCommandStr());
            gSpool->Print();
            idlOS::sprintf(gSpool->m_Buf, "Change success.\n");
            gSpool->Print();
            continue;
        }
*/
        if (gCommand->GetCommandKind() == HISRUN_COM)
        {
            if (gCommand->mExecutor() == IDE_FAILURE)
            {
                continue;
            }
        }

        if (gSQLCompiler->IsFileRead() == ID_FALSE &&
            gCommand->GetCommandKind() != SAVE_COM &&
            gCommand->GetCommandKind() != LOAD_COM &&
            gCommand->GetCommandKind() != HISRUN_COM &&
            gCommand->GetCommandKind() != CHANGE_COM &&
            gCommand->GetCommandKind() != HELP_COM &&
            gCommand->GetCommandKind() != EDIT_COM &&
            gCommand->GetCommandKind() != HISEDIT_COM &&
            gCommand->GetCommandKind() != SHELL_COM &&
            gCommand->GetCommandKind() != HISTORY_COM ) // if -f option, not save in history
        {
            gCommandQueue->AddCommand(gCommand);
        }

        /* BUG-42811 code refactoring using fuction pointers */
        if (gCommand->mExecutor != NULL)
        {
            gLastReturnValue = gCommand->mExecutor();
        }

        if( ( gProgOption.IsATAF() == ID_TRUE ) &&
            ( g_glogin == ID_FALSE ) ) // BUG-47644
        {
            (void)idlOS::printf( "\n$$EOF$$\n" );
            (void)idlOS::fflush( stdout );
        }
    }

    gExecuteCommand->DisconnectDB();

    delete(g_memmgr);
    
    isqlFinalizeLineEditor();

    return IDE_SUCCESS;

    IDE_EXCEPTION(envhome_error)
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_env_not_exist, IDP_HOME_ENV);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(sysdba_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_account_privilege_error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(exit_pos);
    {
        gExecuteCommand->DisconnectDB(IDE_SUCCESS);
        delete(g_memmgr);
        isqlFinalizeLineEditor();
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(connect_error);
    {
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    gExecuteCommand->DisconnectDB(IDE_FAILURE);

    delete(g_memmgr);
    isqlFinalizeLineEditor();

    return IDE_FAILURE;
}

void
ShowCopyRight()
{
    SChar         sBuf[1024 + 1];
    SChar         sBannerFile[1024];
    SInt          sCount;
    FILE        * sFP;
    SChar       * sAltiHome;
    const SChar * sBanner = BAN_FILENAME;

    sAltiHome = idlOS::getenv(IDP_HOME_ENV);
    IDE_TEST_RAISE( sAltiHome == NULL, err_altibase_home );

    // make full path banner file name
    idlOS::memset(sBannerFile, 0, ID_SIZEOF(sBannerFile));
    idlOS::snprintf(sBannerFile,
                    ID_SIZEOF(sBannerFile),
                    "%s%c%s%c%s",
                    sAltiHome,
                    IDL_FILE_SEPARATOR,
                    "msg",
                    IDL_FILE_SEPARATOR,
                    sBanner);

    sFP = idlOS::fopen(sBannerFile, "r");
    IDE_TEST_RAISE( sFP == NULL, err_file_open );

    sCount = idlOS::fread( (void*) sBuf, 1, 1024, sFP );
    IDE_TEST_RAISE( sCount <= 0, err_file_read );

    sBuf[sCount] = '\0';
    idlOS::fprintf(gProgOption.m_OutFile, "%s", sBuf);
    idlOS::fflush(gProgOption.m_OutFile);

    idlOS::fclose(sFP);

    IDE_EXCEPTION( err_altibase_home );
    {
        // nothing to do
        // ignore error in ShowCopyright
    }
    IDE_EXCEPTION( err_file_open );
    {
        // nothing to do
        // ignore error in ShowCopyright
    }
    IDE_EXCEPTION( err_file_read );
    {
        idlOS::fclose(sFP);
    }
    IDE_EXCEPTION_END;
}

IDE_RC
gExecuteGlogin()
{
    SChar tmp[WORD_LEN];

    if ( idlOS::getenv(IDP_HOME_ENV) != NULL )
    {
        idlOS::sprintf(tmp, "%s%cconf%cglogin.sql", idlOS::getenv(IDP_HOME_ENV),
                       IDL_FILE_SEPARATOR, IDL_FILE_SEPARATOR);
        IDE_TEST(gSQLCompiler->SetScriptFile(tmp, ISQL_PATH_CWD) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
gExecuteLogin()
{
    IDE_TEST(gSQLCompiler->SetScriptFile((SChar*)"login.sql",
                ISQL_PATH_CWD) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SInt
LoadFileData( const SChar *  file,
                    UChar ** buf )
{
    SInt len;
    PDL_stat st;
    FILE *fp;

    if ( 0 != idlOS::stat(file, &st) )
    {
        return 0;
    }
    len = st.st_size;

    /* BUG-47652 Set file permission */
    if ( (fp = isql_fopen( file, "r", gProgOption.isExistFilePerm() )) == NULL )
    {
        return 0;
    }

    *buf = (UChar *)idlOS::malloc(len + 2);
    memset(*buf, 0, len + 2);
    len = idlOS::fread(*buf, 1, len, fp);
    
    if ( (*buf)[len - 1] == '\n')
    {
        (*buf)[len - 1] = '\0';
    }    
    if ( (*buf)[len - 2] != ';')
    {
        (*buf)[len - 1] = ';';
    }
    (*buf)[len] = '\n';

    if (len <= 0)
    {
        idlOS::fclose(fp);
        return 0;
    }
    idlOS::fclose(fp);

    return len;
}

SInt
SaveFileData( const SChar * file,
                    UChar * buf )
{
    SInt len;
    FILE *fp;

    /* BUG-47652 Set file permission */
    if ( (fp = isql_fopen( file, "w", gProgOption.isExistFilePerm()) ) == NULL )
    {
        return 0;
    }

    len = idlOS::fwrite(buf, 1, idlOS::strlen((SChar *)buf), fp);
    idlOS::fprintf(fp, "\n");

    idlOS::fclose(fp);

    return len;
}

void
QueryLogging( const SChar *aQuery )
{
    FILE *fp;
    time_t timet;
    struct tm  now;
    // BUG-35099: [ux] Codesonar warning UX part - 158503.2579701
    SInt       sState = 0;

    IDE_TEST( gProperty.GetQueryLogging() != ID_TRUE );

    /* BUG-47652 Set file permission */
    fp = isql_fopen( QUERY_LOGFILE, "a", gProgOption.isExistFilePerm() );
    IDE_TEST( fp == NULL );
    sState = 1;

    IDE_TEST( (SInt)(idlOS::time( &timet ) ) < 0 );
    
    idlOS::localtime_r(&timet, &now);
    idlOS::fprintf(fp,
            "[%4"ID_UINT32_FMT
            "/%02"ID_UINT32_FMT
            "/%02"ID_UINT32_FMT
            " %02"ID_UINT32_FMT
            ":%02"ID_UINT32_FMT
            ":%02"ID_UINT32_FMT"] ",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday,
            now.tm_hour,
            now.tm_min,
            now.tm_sec);

    idlOS::fprintf( fp,
                    "[%s:%"ID_INT32_FMT" %s]",
                    gProgOption.GetServerName(),
                    gProgOption.GetPortNo(),
                    gProperty.GetUserName() );

    idlOS::fprintf(fp, " %s\n", aQuery);

    idlOS::fflush(fp);

    sState = 0;
    idlOS::fclose(fp);
    fp = NULL;
    
    return ;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void)idlOS::fclose( fp );
        fp = NULL;
    }

    return;
}

void InitQueryLogFile()
{
    SChar *tmp = NULL;
    tmp = getenv(IDP_HOME_ENV);

    if (tmp != NULL)
    {
        idlOS::sprintf(QUERY_LOGFILE,
                       "%s%ctrc%cisql_query.log",
                       tmp,
                       IDL_FILE_SEPARATOR,
                       IDL_FILE_SEPARATOR);
    }
    else
    {
        QUERY_LOGFILE[0] = 0;
    }
}

void Finalize()
{
    if (gTmpBuf != NULL)
    {
        idlOS::free(gTmpBuf);
        gTmpBuf = NULL;
    }
    if (g_memmgr != NULL)
    {
        delete g_memmgr;
        g_memmgr = NULL;
    }
    if (gBufMgr != NULL)
    {
        delete gBufMgr;
        gBufMgr = NULL;
    }
    if (gSQLCompiler != NULL)
    {
        delete gSQLCompiler;
        gSQLCompiler = NULL;
    }
    if (gCommandQueue != NULL)
    {
        delete gCommandQueue;
        gCommandQueue = NULL;
    }
    if (gISPApi != NULL)
    {
        delete gISPApi;
        gISPApi = NULL;
    }
    if (gExecuteCommand != NULL)
    {
        delete gExecuteCommand;
        gExecuteCommand = NULL;
    }
    if (gSpool != NULL)
    {
        delete gSpool;
        gSpool = NULL;
    }
    if (gCommandTmp != NULL)
    {
        delete gCommandTmp;
        gCommandTmp = NULL;
    }
    if (gCommand != NULL)
    {
        delete gCommand;
        gCommand = NULL;
    }
}

void isqlFinalizeLineEditor( void )
{
#ifdef USE_READLINE
    if( ( gProgOption.UseLineEditing() == ID_TRUE ) && ( gEdo != NULL ) )
    {
        /* BUG-45145 Need to enhance history */
        if (gProperty.GetHistFile() != NULL) // BUG-46552
        {
            history(gHist, &gEvent, H_SAVE, gProperty.GetHistFile());
        }

        el_end( gEdo );
        gEdo = NULL;
    }
#endif
}

void isqlExitHandler( void )
{
    /* BUG-41863 */
    Exit( gLastReturnValue );
}

void Exit(int aStatus)
{
    isqlFinalizeLineEditor();

    idlOS::exit(aStatus);
}

