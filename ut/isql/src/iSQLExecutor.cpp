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
 * $Id: iSQLExecutor.cpp 86554 2020-01-21 05:05:40Z bethy $
 **********************************************************************/

/* ====================================================================
   NAME
    iSQLExecutor.cpp

   DESCRIPTION
    BUG-42811 code refactoring using fuction pointer.

    main �Լ� ������ Ŀ�ǵ� ������ ���� switch case ������
    �б��ϴ� �κ��� �Լ� �����͸� ���ؼ� ȣ���ϴ� ������� ����.

    ����, Ŀ�ǵ� �߰��� iSQLCommand Ŭ������ static �Լ��� �߰��ϰ�
    parser���� �Լ� �����Ϳ� �ش� �Լ��� address�� �Ҵ��ؾ� ��.
    ex) gCommand->mExecutor = iSQLCommand::executeDDL

    cf) BUG-42811���� �ϰ� ������ ���̸� ���� iSQLCommand Ŭ������ Ŀ�ǵ�
        ������ ���� static �Լ��� �ϰ� �߰��Ͽ�����, ���� �߰��ÿ��� 
        �ٸ� ������ Ŭ������ static �Լ��� �߰��Ͽ��� ������.

 ==================================================================== */

#include <ideErrorMgr.h>
#include <uttMemory.h>
#include <iSQLProperty.h>
#include <iSQLProgOption.h>
#include <iSQLCompiler.h>
#include <iSQLCommandQueue.h>
#include <iSQLCommand.h>
#include <iSQLExecuteCommand.h>
#include <iSQLHostVarMgr.h>

extern iSQLProgOption       gProgOption;
extern iSQLProperty         gProperty;
extern iSQLHostVarMgr       gHostVarMgr;
extern iSQLCompiler       * gSQLCompiler;
extern iSQLCommand        * gCommand;
extern iSQLCommand        * gCommandTmp;
extern iSQLCommandQueue   * gCommandQueue;
extern iSQLExecuteCommand * gExecuteCommand;
extern iSQLSpool          * gSpool;
extern uttMemory          * g_memmgr;

extern idBool   g_glogin;      // $ALTIBASE_HOME/conf/glogin.sql
extern idBool   g_login;       // ./login.sql
extern idBool   g_inLoad;
extern idBool   g_inEdit;
extern idBool   gSameUser;
extern SChar    QUERY_LOGFILE[256];

extern SInt   iSQLParserparse(void *);
extern void   gSetInputStr(SChar * s);
extern void   isqlFinalizeLineEditor( void );
extern void   QueryLogging( const SChar *aQuery );
extern IDE_RC checkUser();
extern IDE_RC gExecuteGlogin();
extern IDE_RC gExecuteLogin();

IDE_RC iSQLCommand::executeAlter()
{
    IDE_TEST( gExecuteCommand->ExecuteDDLStmt(
                  gCommand->GetQuery(),
                  gCommand->GetCommandKind()) != IDE_SUCCESS);

    if ( gCommand->GetiSQLOptionKind() == iSQL_CURRENCY )
    {
        /* BUG-34447 SET NUMF[ORMAT]
         * alter session set NLS_TERRITORY ���� ���ؼ� ������ currency�� 
         * �����ϴ� ��� V$SESSION���� currency�� �ٽ� �����;� ��
         */ 
        gExecuteCommand->SetNlsCurrency();
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeDDL()
{
    return gExecuteCommand->ExecuteDDLStmt(
                  gCommand->GetQuery(),
                  gCommand->GetCommandKind());
}

IDE_RC iSQLCommand::executeAutoCommit()
{
    return gExecuteCommand->ExecuteAutoCommitStmt(
                                gCommand->GetCommandStr(),
                                gCommand->GetOnOff());
}

IDE_RC iSQLCommand::executeConnect()
{
    if (gProperty.IsConnToRealInstance() == ID_FALSE)
    {
        (void)gExecuteCommand->ExecuteDisconnectStmt(
                                                     (SChar *)"DISCONNECT",
                                                     ID_FALSE);
    }

    if (gProgOption.IsATC() == ID_FALSE)
    { // bypass user check if atc mode is true

        // bug-26749 windows isql filename parse error
        // sysdba�� ������ ��츸 exefile userid �˻�
        if (gCommand->IsSysdba() == ID_TRUE)
        {
            IDE_TEST(checkUser() != IDE_SUCCESS);
            IDE_TEST_RAISE(gSameUser != ID_TRUE, sysdba_error);
        }
    }

    IDE_TEST_RAISE(gExecuteCommand->ExecuteConnectStmt(
                                        gCommand->GetQuery(),
                                        gCommand->GetUserName(),
                                        gCommand->GetPasswd(),
                                        gCommand->GetNlsUse(),
                                        gCommand->IsSysdba())
                   != IDE_SUCCESS, connect_error);

    if ( (gProperty.IsConnToRealInstance() == ID_FALSE) &&
         (gProgOption.IsATC() == ID_FALSE) &&
         (gProgOption.IsATAF() == ID_FALSE) )
    {
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }

    /* BUG-41163 SET SQLPROMPT */
    gProperty.ResetSqlPrompt();

    if ( ( gCommand->IsSysdba() == ID_FALSE ) &&
         ( gProgOption.IsATAF() == ID_FALSE ) )  // BUG-47644
    {
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
    }

    /* BUG-19097 */
    gProperty.SetPromptRefreshFlag(PROMPT_RECONNECT_ON);
    gProperty.clearPlanProperty();

    return IDE_SUCCESS;

    IDE_EXCEPTION(sysdba_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_account_privilege_error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(connect_error);
    {
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeDisconnect()
{
    return gExecuteCommand->ExecuteDisconnectStmt(
                                                  gCommand->GetQuery(),
                                                  ID_TRUE);
}

IDE_RC iSQLCommand::executeDML()
{
    QueryLogging(gCommand->GetQuery());

    return iSQLCommand::executeSelect();
}

IDE_RC iSQLCommand::executeSelect()
{
    return gExecuteCommand->ExecuteSelectOrDMLStmt(
                                gCommand->GetQuery(),
                                gCommand->GetCommandKind());
}

IDE_RC iSQLCommand::executeDescTable()
{
    return gExecuteCommand->DisplayAttributeList(
                                gCommand->GetUserName(),
                                gCommand->GetTableName());
}

IDE_RC iSQLCommand::executeDescDollarTable()
{
    return gExecuteCommand->DisplayAttributeList4FTnPV(
                                gCommand->GetUserName(),
                                gCommand->GetTableName());
}

IDE_RC iSQLCommand::executeProc()
{
    idBool sIsFunc = ID_FALSE;
    
    if (gCommand->GetCommandKind() == EXEC_FUNC_COM)
    {
        sIsFunc = ID_TRUE;
    }
    else
    {
        sIsFunc = ID_FALSE;
    }

    /* bug 18731 */
    IDE_TEST( gSQLCompiler->ParsingExecProc( gCommand->GetQuery(),
                gProperty.GetCommandLen() ) != IDE_SUCCESS );

    /* BUG-37002 isql cannot parse package as a assigned variable */
    IDE_TEST( gExecuteCommand->ExecutePSMStmt(
                  gCommand->GetQuery(),
                  gCommand->GetUserName(),
                  gCommand->GetPkgName(),
                  gCommand->GetProcName(),
                  sIsFunc ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-46733 Need to support Anomymous Block */
IDE_RC iSQLCommand::executeAnonymBlock()
{
    gCommand->setUserName((SChar*)"");
    gCommand->SetPkgName((SChar*)"");
    gCommand->setProcName((SChar*)"");

    return executeProc();
}

IDE_RC iSQLCommand::executeHelp()
{
    gExecuteCommand->PrintHelpString(gCommand->GetHelpKind());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeEdit()
{
    SChar    tmpFile[WORD_LEN];

    IDE_TEST_RAISE( gSQLCompiler->IsFileRead() == ID_TRUE, not_allowed_command );

    if ( idlOS::strlen(gCommand->GetFileName()) == 0 )
    {
        IDE_TEST_CONT( gCommandQueue->GetCommand(0, gCommandTmp)
                       != IDE_SUCCESS, ret_pos );

        gSQLCompiler->SaveCommandToFile2(gCommandTmp->GetCommandStr());
    }

    gExecuteCommand->ExecuteEditStmt(gCommand->GetFileName(),
                                     gCommand->GetPathType(),
                                     tmpFile);

    if ( gSQLCompiler->SetScriptFile(tmpFile,
                                     gCommand->GetPathType())
         == IDE_SUCCESS )
    {
        g_inEdit = ID_TRUE;
    }

    IDE_EXCEPTION_CONT(ret_pos);

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_allowed_command );
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Invalid_Command_Error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC iSQLCommand::executeHisEdit()
{
    SChar    tmpFile[WORD_LEN];

    IDE_TEST( gCommandQueue->GetCommand(gCommand->GetHistoryNo(),
                                        gCommandTmp)
              != IDE_SUCCESS );

    gSQLCompiler->SaveCommandToFile2(gCommandTmp->GetCommandStr());

    gExecuteCommand->ExecuteEditStmt(gCommand->GetFileName(),
                                     gCommand->GetPathType(),
                                     tmpFile);

    if ( gSQLCompiler->SetScriptFile(tmpFile,
                                     gCommand->GetPathType())
         == IDE_SUCCESS )
    {
        g_inEdit = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeHistory()
{
    IDE_TEST_RAISE( gSQLCompiler->IsFileRead() == ID_TRUE, not_allowed_command );

    gCommandQueue->DisplayHistory();

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_allowed_command );
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Invalid_Command_Error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeRunHistory()
{
    SChar *empty = NULL;

    IDE_TEST_RAISE( gSQLCompiler->IsFileRead() == ID_TRUE, not_allowed_command );

    IDE_TEST( gCommandQueue->GetCommand(gCommand->GetHistoryNo(), gCommand)
              == IDE_FAILURE );

    gSetInputStr(gCommand->GetCommandStr());
    gCommand->mExecutor = NULL;
    iSQLParserparse(empty);

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_allowed_command );
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Invalid_Command_Error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeLoad()
{
    IDE_TEST_RAISE( gSQLCompiler->IsFileRead() == ID_TRUE, not_allowed_command );

    if ( gSQLCompiler->SetScriptFile(gCommand->GetFileName(),
                                     gCommand->GetPathType())
         == IDE_SUCCESS )
    {
        g_inLoad = ID_TRUE;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( not_allowed_command );
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Invalid_Command_Error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeSave()
{
    IDE_TEST_RAISE( gSQLCompiler->IsFileRead() == ID_TRUE, not_allowed_command );

    if ( gCommandQueue->GetCommand(0, gCommandTmp) == IDE_SUCCESS )
    {
        gSQLCompiler->SaveCommandToFile(gCommandTmp->GetCommandStr(),
                                        gCommand->GetFileName(),
                                        gCommand->GetPathType());
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( not_allowed_command );
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Invalid_Command_Error);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
IDE_RC iSQLCommand::executeDCL()
{
    QueryLogging(gCommand->GetQuery());

    return gExecuteCommand->ExecuteOtherCommandStmt(
                  gCommand->GetQuery());
}

IDE_RC iSQLCommand::executePrintVar()
{
    gExecuteCommand->ShowHostVar(gCommand->GetHostVarName());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executePrintVars()
{
    gExecuteCommand->ShowHostVar();
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeRunScript()
{
    gSQLCompiler->SetScriptFile(gCommand->GetFileName(),
                                gCommand->GetPathType(),
                                gCommand->GetPassingParams());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeDMLWithPrepare()
{
    QueryLogging(gCommand->GetQuery());

    return iSQLCommand::executeSelectWithPrepare();
}

IDE_RC iSQLCommand::executeSelectWithPrepare()
{
    IDE_TEST( gSQLCompiler->ParsingPrepareSQL(gCommand->GetQuery(),
                gProperty.GetCommandLen()) != IDE_SUCCESS );

    IDE_TEST( gExecuteCommand->PrepareSelectOrDMLStmt(
                  gCommand->GetQuery(),
                  gCommand->GetCommandKind())
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeShowTablesWithPrepare()
{
    return gExecuteCommand->DisplayTableListOrPrepare(
                                gCommand->GetQuery(),
                                gProperty.GetCommandLen());
}

IDE_RC iSQLCommand::executeColumns()
{
    return gProperty.ListAllColumns();
}

IDE_RC iSQLCommand::executeColumn()
{
    return gProperty.ListColumn();
}

IDE_RC iSQLCommand::executeColumnClear()
{
    return gProperty.ClearFormat();
}

IDE_RC iSQLCommand::executeColumnFormat()
{
    if ( gCommand->GetCommandKind() == COLUMN_FMT_CHR_COM )
    {
        gProperty.SetFormat4Char();
    }
    else // if COLUMN_FMT_NUM_COM
    {
        gProperty.SetFormat4Num();
    }
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeColumnOnOff()
{
    return gProperty.TurnColumnOnOff();
}

IDE_RC iSQLCommand::executeClearColumns()
{
    return gProperty.ClearAllFormats();
}

IDE_RC iSQLCommand::executeSet()
{
    switch (gCommand->GetiSQLOptionKind())
    {
    case iSQL_NON       :
        gExecuteCommand->ExecuteOtherCommandStmt(
                gCommand->GetQuery());
        break;
    case iSQL_HEADING   :
        gProperty.SetHeading(gCommand->GetOnOff());
        break;
    case iSQL_CHECKCONSTRAINTS : /* PROJ-1107 Check Constraint ���� */
        gProperty.SetCheckConstraints( gCommand->GetOnOff() );
        break;
    case iSQL_FOREIGNKEYS   :
        gProperty.SetForeignKeys(gCommand->GetOnOff());
        break;
    case iSQL_PLANCOMMIT   :
        gProperty.SetPlanCommit(gCommand->GetOnOff());
        break;
    case iSQL_QUERYLOGGING   :
        gProperty.SetQueryLogging(gCommand->GetOnOff());
        break;
    case iSQL_COLSIZE  :
        gProperty.SetColSize(gCommand->GetColsize());
        break;
    case iSQL_LINESIZE  :
        gProperty.SetLineSize(gCommand->GetLinesize());
        break;
    case iSQL_LOBOFFSET  :
        gProperty.SetLobOffset(gCommand->GetLoboffset());
        break;
    case iSQL_LOBSIZE  :
        gProperty.SetLobSize(gCommand->GetLobsize());
        break;
    case iSQL_NUMWIDTH  :   /* BUG-39213 Need to support SET NUMWIDTH in isql */
        gProperty.SetNumWidth(gCommand->GetNumWidth());
        break;
    case iSQL_PAGESIZE  :
        gProperty.SetPageSize(gCommand->GetPagesize());
        break;
    case iSQL_TERM    :
        gProperty.SetTerm(gCommand->GetOnOff());
        break;
    case iSQL_TIMING    :
        gProperty.SetTiming(gCommand->GetOnOff());
        break;
    // BUG-22685
    case iSQL_VERTICAL  :
        gProperty.SetVertical(gCommand->GetOnOff());
        break;
    case iSQL_TIMESCALE :
        gProperty.SetTimeScale(gCommand->GetTimescale());
        break;
    case iSQL_FEEDBACK   :
        gProperty.SetFeedback(gCommand->GetFeedback());
        break;
    case iSQL_EXPLAINPLAN :
        gProperty.SetExplainPlan(gCommand->GetCommandStr(),
                gCommand->GetExplainPlan());
        gExecuteCommand->m_ISPApi->SetPlanMode(
                gCommand->GetExplainPlan());
        break;
    /* BUG-37772 */
    case iSQL_ECHO:
        gProperty.SetEcho( gCommand->GetOnOff() );
        break;
    /* BUG-39620 */
    case iSQL_FULLNAME:
        gProperty.SetFullName( gCommand->GetOnOff() );
        break;
    /* BUG-41163 SET SQLP[ROMPT] */
    case iSQL_SQLPROMPT:
        gProperty.SetSqlPrompt( gCommand->GetSqlPrompt() );
        break;
    case iSQL_DEFINE:
        gProperty.SetDefine(gCommand->GetOnOff());
        break;
    /* BUG-34447 SET NUMMF[ORMAT] */
    case iSQL_NUMFORMAT:
        gProperty.SetNumFormat();
        break;
    case iSQL_PARTITIONS: /* BUG-43516 */
        gProperty.SetPartitions(gCommand->GetOnOff());
        break;
    case iSQL_VERIFY: /* BUG-43599 */
        gProperty.SetVerify(gCommand->GetOnOff());
        break;
    case iSQL_PREFETCHROWS: /* BUG-44613 */
        gProperty.SetPrefetchRows(gCommand->GetPrefetchRows());
        break;
    case iSQL_ASYNCPREFETCH: /* BUG-44613 */
        gProperty.SetAsyncPrefetch(gCommand->GetAsyncPrefetch());
        break;
    case iSQL_MULTIERROR: /* BUG-47627 */
        gProperty.SetMultiError(gCommand->GetOnOff());
        break;
    default :
        break;
    }
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeShell()
{
    gExecuteCommand->ExecuteShellStmt(gCommand->GetShellCommand());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeShow()
{
    gProperty.ShowStmt(gCommand->GetiSQLOptionKind());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeSpool()
{
    gExecuteCommand->ExecuteSpoolStmt(gCommand->GetFileName(),
                                      gCommand->GetPathType());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeSpoolOff()
{
    gExecuteCommand->ExecuteSpoolOffStmt();
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeShowTables()
{
    return gExecuteCommand->DisplayTableListOrSelect(
                                gCommand->GetQuery());
}

IDE_RC iSQLCommand::executeShowFixedTables()
{
    return gExecuteCommand->DisplayFixedTableList(
                                "X$",
                                "FIXED TABLE");
}

IDE_RC iSQLCommand::executeShowDumpTables()
{
    return gExecuteCommand->DisplayFixedTableList(
                                "D$", 	 
                                "DUMP TABLE"); 	 
}

IDE_RC iSQLCommand::executeShowPerfViews()
{
    return gExecuteCommand->DisplayFixedTableList(
                                "V$", 	 
                                "PERFORMANCE VIEW");
}

/* BUG-45646 */
IDE_RC iSQLCommand::executeShowShardPerfViews()
{
    return gExecuteCommand->DisplayFixedTableList(
                                "S$", 	 
                                "SHARD PERFORMANCE VIEW");
}

IDE_RC iSQLCommand::executeShowSequences()
{
    return gExecuteCommand->DisplaySequenceList();
}

IDE_RC iSQLCommand::executeDeclareVar()
{
    /* BUG-41817 Declare Host Variable */
    gHostVarMgr.add(gCommand->GetHostVarName(),
                    gCommand->GetHostVarType(),
                    gCommand->GetHostInOutType(),
                    gCommand->GetHostVarPrecision(),
                    gCommand->GetHostVarScale());
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeAssignVar()
{
    /* BUG-41817 Assign Value to Host Variable */
    gHostVarMgr.setValue(gCommand->GetHostVarName(),
                         gCommand->GetHostVarValue());
    gCommand->FreeHostVarValue();
    return IDE_SUCCESS;
}

IDE_RC iSQLCommand::executeStartup()
{
    IDE_TEST( gProperty.IsSysDBA() != ID_TRUE );

    return gExecuteCommand->Startup(gCommand->GetCommandKind(),
                                    FORKONLYDAEMON);

    IDE_EXCEPTION_END;

    uteSetErrorCode(&gErrorMgr, utERR_ABORT_sysdba_privilege_Error);
    uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
    gSpool->Print();

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeShutdown()
{
    IDE_TEST( gProperty.IsSysDBA() != ID_TRUE );

    return gExecuteCommand->Shutdown(gCommand->GetCommandKind());

    IDE_EXCEPTION_END;

    uteSetErrorCode(&gErrorMgr, utERR_ABORT_sysdba_privilege_Error);
    uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
    gSpool->Print();

    return IDE_FAILURE;
}

IDE_RC iSQLCommand::executeExit()
{
    idlOS::unlink(ISQL_BUF);
    gExecuteCommand->DisconnectDB(IDE_SUCCESS);
    delete(g_memmgr);
    isqlFinalizeLineEditor();

    return IDE_SUCCESS;
}

