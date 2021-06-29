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
 * $Id: iSQLProperty.cpp 86554 2020-01-21 05:05:40Z bethy $
 **********************************************************************/

#include <iSQL.h>
#include <utString.h>
#include <utISPApi.h>
#include <iSQLSpool.h>
#include <iSQLProperty.h>
#include <iSQLExecuteCommand.h>
#include <iSQLCommand.h>
#include <isqlFloat.h>

extern iSQLSpool          * gSpool;
extern iSQLCommand        * gCommand;
extern iSQLExecuteCommand * gExecuteCommand;
extern iSQLProperty         gProperty;

/* BUG-41163 SET SQLP{ROMPT} */
extern SInt lexSqlPrompt(const SChar *aSqlPrompt,
                         SChar       *aNewPromptBuf,
                         UInt        *aPromptRefreshFlag);

iSQLProperty::iSQLProperty()
{
    m_ColSize              = 0;
    m_LineSize             = 80;
    m_PageSize             = 0;
    m_Feedback             = 1;
    m_Term                 = ID_TRUE;
    m_Timing               = ID_FALSE;
    m_Vertical             = ID_FALSE; // BUG-22685
    m_Heading              = ID_TRUE;
    m_ShowCheckConstraints = ID_FALSE; /* PROJ-1107 Check Constraint ���� */
    m_ShowForeignKeys      = ID_FALSE;
    m_ShowPartitions       = ID_FALSE; /* BUG-43516 */
    m_PlanCommit           = ID_FALSE;
    m_QueryLogging         = ID_FALSE;
    mIsSysDBA              = ID_FALSE;
    mIsConnToRealInstance  = ID_FALSE;
    m_TimeScale            = iSQL_SEC;
    mExplainPlan           = EXPLAIN_PLAN_OFF;
    m_LobOffset            = 0;
    m_LobSize              = 80;
    m_NumWidth             = 11;       // BUG-39213 Need to support SET NUMWIDTH in isql
    mEcho                  = ID_TRUE;  /* BUG-45722 */
    mFullName              = ID_FALSE;
    m_Define               = ID_FALSE;
    m_Verify               = ID_TRUE;  /* BUG-43599 */
    m_MultiError           = ID_FALSE; /* BUG-47627 */

    /* BUG-44613 */
    m_PrefetchRows         = 0;
    m_AsyncPrefetch        = ASYNCPREFETCH_OFF;

    idlOS::memset(m_UserName, 0x00, ID_SIZEOF(m_UserName));
    idlOS::memset(m_UserCert, 0x00, ID_SIZEOF(m_UserCert));
    idlOS::memset(m_UserKey, 0x00, ID_SIZEOF(m_UserKey));
    idlOS::memset(m_UserAID, 0x00, ID_SIZEOF(m_UserAID));
    idlOS::memset(m_UnixdomainFilepath, 0x00, ID_SIZEOF(m_UnixdomainFilepath));
    idlOS::memset(m_IpcFilepath, 0x00, ID_SIZEOF(m_IpcFilepath));
    idlOS::memset(mPasswd, 0x00, ID_SIZEOF(mPasswd));
    idlOS::memset(mCaseSensitivePasswd, 0x00, ID_SIZEOF(mCaseSensitivePasswd));
    idlOS::memset(m_Editor, 0x00, ID_SIZEOF(m_Editor));
    idlOS::memset(mConnTypeStr,  0x00, ID_SIZEOF(mConnTypeStr));
#ifdef USE_READLINE
    idlOS::memset(mHistFile, 0x00, ID_SIZEOF(mHistFile)); /* BUG-45145 */
#endif

    /*BUG-41163 SET SQLP[ROMPT] */
    mSqlPrompt[0] = '\0';
    mPromptRefreshFlag = PROMPT_REFRESH_OFF;

    SetEnv();

    mColFmtList = NULL;

    mNumFormat[0] = '\0';
    mNumToken[0] = '\0';
}

iSQLProperty::~iSQLProperty()
{
    freeFmtList();
}

/* BUG-19097 */
void iSQLProperty::clearPlanProperty()
{
    mExplainPlan = EXPLAIN_PLAN_OFF;
}

/* ============================================
 * iSQL ���� ȯ�溯���� �о ����
 * ============================================ */
void
iSQLProperty::SetEnv()
{
    /* ============================================
     * ISQL_BUFFER_SIZE, default : 64K
     * ============================================ */
    if (idlOS::getenv(ENV_ISQL_BUFFER_SIZE))
    {
        m_CommandLen = idlOS::atoi(idlOS::getenv(ENV_ISQL_BUFFER_SIZE));
    }
    else
    {
        m_CommandLen = COMMAND_LEN;
    }

    /* ============================================
     * ISQL_EDITOR, default : /usr/bin/vi
     * ============================================ */
    if (idlOS::getenv(ENV_ISQL_EDITOR))
    {
        idlOS::strcpy(m_Editor, idlOS::getenv(ENV_ISQL_EDITOR));
    }
    else
    {
        idlOS::strcpy(m_Editor, ISQL_EDITOR);
    }

    /* ============================================
     * ISQL_CONNECTION, default : TCP
     * ============================================ */
    if (idlOS::getenv(ENV_ISQL_CONNECTION))
    {
        idlOS::strcpy(mConnTypeStr, idlOS::getenv(ENV_ISQL_CONNECTION));
    }
    else
    {
        idlOS::strcpy(mConnTypeStr, "TCP");
    }

#ifdef USE_READLINE
    /*
     * BUG-45145 Need to enhance history
     * ISQL_HIST_FILE, default : not used
     */
    if (idlOS::getenv(ENV_ISQL_HIST_FILE))
    {
        idlOS::strcpy(mHistFile, idlOS::getenv(ENV_ISQL_HIST_FILE));
    }
    else
    {
        /* BUG-46552 Do not use /dev/null */
        idlOS::strcpy(mHistFile, "");
    }
#endif
}

#ifdef USE_READLINE
/* BUG-46552 Do not use /dev/null */
/* BUG-45145 Need to enhance history */
SChar * iSQLProperty::GetHistFile()
{
    if (idlOS::strlen(mHistFile) == 0)
    {
        return NULL;
    }
    else
    {
        return mHistFile;
    }
}
#endif

/* ============================================
 * Set ColSize
 * Display �Ǵ� �� �÷�(char,varchar Ÿ�Ը� ����)�� ����
 * ============================================ */
void
iSQLProperty::SetColSize( SInt    a_ColSize )
{
    if ( (a_ColSize < 0) || (a_ColSize > 32767) )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Column_Size_Error, (UInt)0, (UInt)32767);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    else
    {
        m_ColSize = a_ColSize;
    }
}

void
iSQLProperty::SetFeedback( SInt    a_Feedback )
{
    m_Feedback = a_Feedback;
}

/* ============================================
 * Set LineSize
 * Display �Ǵ� �� ������ ����
 * ============================================ */
void
iSQLProperty::SetLineSize( SInt    a_LineSize )
{
    if ( (a_LineSize < 10) || (a_LineSize > 32767) )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Line_Size_Error, (UInt)10, (UInt)32767);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    else
    {
        m_LineSize = a_LineSize;
    }
}

// BUG-39213 Need to support SET NUMWIDTH in isql
/* ============================================
 * Set NumWidth
 * Display �Ǵ� �� �÷�(numeric, decimal, float Ÿ�Ը� ����)�� ����
 * ============================================ */
void
iSQLProperty::SetNumWidth( SInt    a_NumWidth )
{
    if ( (a_NumWidth < 11) || (a_NumWidth > 50) )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Num_Width_Error, (UInt)11, (UInt)50);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    else
    {
        m_NumWidth = a_NumWidth;
    }
}

/* ============================================
 * Set PageSize
 * ���ڵ带 �� �� ������ ������ ���ΰ�
 * ============================================ */
void
iSQLProperty::SetPageSize( SInt    a_PageSize )
{
    if ( a_PageSize < 0 || a_PageSize > 50000 )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Page_Size_Error, (UInt)0, (UInt)50000);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);

        gSpool->Print();
    }
    else
    {
        m_PageSize = a_PageSize;
    }
}

/* ============================================
 * Set Term
 * �ܼ� ȭ�������� ����� �� ���ΰ� �� ���ΰ�
 * ============================================ */
void
iSQLProperty::SetTerm( idBool  a_IsTerm )
{
    m_Term = a_IsTerm;
    mEcho = m_Term; /* BUG-45722 term ������ echo �� �ڵ����� ���� */
}

/* ============================================
 * Set Timing
 * ���� ���� �ð��� ������ ���ΰ� �� ���ΰ�
 * ============================================ */
void
iSQLProperty::SetTiming( idBool  a_IsTiming )
{
    m_Timing = a_IsTiming;
}

// BUG-22685
/* ============================================
 * Set Vertical
 * ���� ������� ���η� ������ ���ΰ�
 * ============================================ */
void
iSQLProperty::SetVertical( idBool  a_IsVertical )
{
    m_Vertical = a_IsVertical;
}

/* ============================================
 * Set Heading
 * ���(Column Name)�� ������ ���ΰ� �� ���ΰ�
 * ============================================ */
void
iSQLProperty::SetHeading( idBool  a_IsHeading )
{
    m_Heading = a_IsHeading;
}

/* ============================================
 * Set TimeScale
 * ���� ���� �ð��� ����
 * ============================================ */
void
iSQLProperty::SetTimeScale( iSQLTimeScale   a_Timescale )
{
    m_TimeScale = a_Timescale;
}

/* ============================================
 * Set User
 * Connect �� ������ ������ ������ �����Ѵ�
 * ============================================ */
void
iSQLProperty::SetUserName( SChar * a_UserName )
{
    // To Fix BUG-17430
    utString::makeNameInCLI(m_UserName,
                            ID_SIZEOF(m_UserName),
                            a_UserName,
                            idlOS::strlen(a_UserName));
}

/* ============================================
 * Set User
 * 
 * ============================================ */
void
iSQLProperty::SetUserCert( SChar * a_UserCert )
{
    idlOS::strcpy(m_UserCert, a_UserCert);
}

/* ============================================
 * Set User
 * Connect?????????????????????????????????
 * ============================================ */
void
iSQLProperty::SetUserKey( SChar * a_UserKey )
{
    idlOS::strcpy(m_UserKey, a_UserKey);
}

/* ============================================
 * Set User
 * Connect?????????????????????????????????
 * ============================================ */
void
iSQLProperty::SetUserAID( SChar * a_UserAID )
{
    idlOS::strcpy(m_UserAID, a_UserAID);
}

/* ============================================
 * Set IpcFilepath
 * Connect?????????????????????????????????
 * ============================================ */
void
iSQLProperty::SetIpcFilepath( SChar * a_IpcFilepath )
{
    idlOS::strcpy(m_IpcFilepath, a_IpcFilepath);
}

/* ============================================
 * Set UnixdomainFilepath
 * Connect?????????????????????????????????
 * ============================================ */
void
iSQLProperty::SetUnixdomainFilepath( SChar * a_UnixdomainFilepath )
{
    idlOS::strcpy(m_UnixdomainFilepath, a_UnixdomainFilepath);
}

/* ============================================
 * Set Password.
 * ============================================ */
void iSQLProperty::SetPasswd(SChar * aPasswd)
{
    idlOS::snprintf(mCaseSensitivePasswd, ID_SIZEOF(mCaseSensitivePasswd), "%s", aPasswd);
    idlOS::snprintf(mPasswd, ID_SIZEOF(mPasswd), "%s", aPasswd);
}

// ============================================
// bug-19279 remote sysdba enable
// conntype string(tcp/unix...)�� �� �����Ѵ�.(ȭ�� ��¿�)
// why? sysdba�� ��� �ʱⰪ�� �ٸ� �� �ִ�.
void iSQLProperty::AdjustConnTypeStr(idBool aIsSysDBA, SChar* aServerName)
{
    SInt sConnType = GetConnType(aIsSysDBA, aServerName);

    switch (sConnType)
    {
        default:
        case ISQL_CONNTYPE_TCP:
            idlOS::strcpy(mConnTypeStr, "TCP");
            break;
        case ISQL_CONNTYPE_UNIX:
            idlOS::strcpy(mConnTypeStr, "UNIX");
            break;
        case ISQL_CONNTYPE_IPC:
            idlOS::strcpy(mConnTypeStr, "IPC");
            break;
        case ISQL_CONNTYPE_IPCDA:
            idlOS::strcpy(mConnTypeStr, "IPCDA");
            break;
        case ISQL_CONNTYPE_SSL:
            idlOS::strcpy(mConnTypeStr, "SSL");
            break;
        case ISQL_CONNTYPE_IB: /* PROJ-2681 */
            idlOS::strcpy(mConnTypeStr, "IB");
            break;
    }
}

/* ============================================
 * Get connnection type number
 * from connection type string.
 * ============================================ */
SInt iSQLProperty::GetConnType(idBool aIsSysDBA, SChar* aServerName)
{
    SInt sConnType;

    if (aIsSysDBA == ID_FALSE)
    {
        if (idlOS::strcasecmp(mConnTypeStr, "TCP") == 0)
        {
            sConnType = ISQL_CONNTYPE_TCP;
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "UNIX") == 0)
        {
#if !defined(VC_WIN32) && !defined(NTO_QNX)
            sConnType = ISQL_CONNTYPE_UNIX;
#else
            sConnType = ISQL_CONNTYPE_TCP;
#endif
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "IPC") == 0)
        {
            sConnType = ISQL_CONNTYPE_IPC;
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "IPCDA") == 0)
        {
            sConnType = ISQL_CONNTYPE_IPCDA;
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "SSL") == 0)
        {
            sConnType = ISQL_CONNTYPE_SSL;
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "IB") == 0) /* PROJ-2681 */
        {
            sConnType = ISQL_CONNTYPE_IB;
        }
        else
        {
            sConnType = ISQL_CONNTYPE_TCP;
        }
    }
    // =============================================================
    // bug-19279 remote sysdba enable
    // ��� ���(tcp/ unix domain)�� ������ ���� ����
    // windows               : tcp
    // localhost: unix domain socket
    // �׿�                   : tcp socket (ipc�� �����)
    // why? local�� ��� unix domain�� �� �������ϰ� ���Ƽ�
    else
    {
#if defined(VC_WIN32) || defined(NTO_QNX)

        /* TASK-5894 Permit sysdba via IPC */
        if (idlOS::strcasecmp(mConnTypeStr, "IPC") == 0)
        {
            sConnType = ISQL_CONNTYPE_IPC;
        }
        else if (idlOS::strcasecmp(mConnTypeStr, "SSL") == 0)
        {
            sConnType = ISQL_CONNTYPE_SSL;
        }
        else
        {
            sConnType = ISQL_CONNTYPE_TCP;
        }

#else
        if ((idlOS::strcmp(aServerName, "127.0.0.1" ) == 0) ||
            (idlOS::strcmp(aServerName, "::1") == 0) ||
            (idlOS::strcmp(aServerName, "0:0:0:0:0:0:0:1") == 0) ||
            (idlOS::strcmp(aServerName, "localhost" ) == 0))
        {
            /* TASK-5894 Permit sysdba via IPC */
            if (idlOS::strcasecmp(mConnTypeStr, "IPC") == 0)
            {
                sConnType = ISQL_CONNTYPE_IPC;
            }
            else
            {
                sConnType = ISQL_CONNTYPE_UNIX;
            }
        }
        else
        {
            if (idlOS::strcasecmp(mConnTypeStr, "SSL") == 0)
            {
                sConnType = ISQL_CONNTYPE_SSL;
            }
            else if (idlOS::strcasecmp(mConnTypeStr, "IB") == 0) /* PROJ-2681 */
            {
                sConnType = ISQL_CONNTYPE_IB;
            }
            else
            {
                sConnType = ISQL_CONNTYPE_TCP;
            }
        }
#endif
    }

    return sConnType;
}

/* ============================================
 * Set CheckConstraints
 * desc ����� Check Constraint ������ ������ ������
 * ============================================ */
void
iSQLProperty::SetCheckConstraints( idBool   a_ShowCheckConstraints )
{
    m_ShowCheckConstraints = a_ShowCheckConstraints;
}

/* ============================================
 * Set ForeignKeys
 * desc ����� foreign key ������ ������ ������
 * ============================================ */
void
iSQLProperty::SetForeignKeys( idBool  a_ShowForeignKeys )
{
    m_ShowForeignKeys = a_ShowForeignKeys;
}

/* ============================================
 * BUG-43516 Set Partitions
 * desc ����� partition ������ ������ ������
 * ============================================ */
void
iSQLProperty::SetPartitions( idBool  a_ShowPartitions )
{
    m_ShowPartitions = a_ShowPartitions;
}

/* ============================================
 * Set PlanCommit
 * autocommit mode false �� ���ǿ��� explain plan ��
 * on �Ǵ� only �� ���� ��, desc, select * From tab;
 * ���� ��ɾ ����ϰ� �Ǹ�, ������ ��������
 * Ʈ������� ������ ��쿡 ������ �߻��ϰ� �ȴ�.
 * error -> The transaction is already active.
 * �̸� �����ϱ� ���ؼ� �������� commit �� �ڵ�����
 * �����ϵ��� �ϴ� �ɼ��� �� �� �ִ�.
 * ============================================ */
void
iSQLProperty::SetPlanCommit( idBool  a_Commit )
{
    m_PlanCommit = a_Commit;
}

void
iSQLProperty::SetQueryLogging( idBool  a_Logging )
{
    m_QueryLogging = a_Logging;
}

void
iSQLProperty::SetExplainPlan(SChar           * aCmdStr,
                             iSQLSessionKind   aExplainPlan)
{
    mExplainPlan = aExplainPlan;

    if (idlOS::strncasecmp(aCmdStr, "ALTER", 5) == 0)
    {
        idlOS::snprintf(gSpool->m_Buf, GetCommandLen(), "Alter success.\n");
        gSpool->Print();

        if (GetTiming() == ID_TRUE)
        {
            idlOS::snprintf(gSpool->m_Buf, GetCommandLen(),
                            "elapsed time : 0.00\n");
            gSpool->Print();
        }
    }
}
/* ============================================
 * Set LobSize
 * Display �Ǵ� �� �÷�(clob Ÿ�Ը� ����)�� ����
 * ============================================ */
void
iSQLProperty::SetLobOffset( SInt    a_LobOffset )
{
    if ( (a_LobOffset < 0) || (a_LobOffset > 32767) )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Column_Size_Error, (UInt)0, (UInt)32767);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    else
    {
        m_LobOffset = a_LobOffset;
    }
}

void
iSQLProperty::SetLobSize( SInt    a_LobSize )
{
    if ( (a_LobSize < 0) || (a_LobSize > 32767) )
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Column_Size_Error, (UInt)0, (UInt)32767);
        uteSprintfErrorCode(gSpool->m_Buf, gProperty.GetCommandLen(), &gErrorMgr);
        gSpool->Print();
    }
    else
    {
        m_LobSize = a_LobSize;
    }
}

/* ============================================
 * ������ iSQL Option�� �����ش�.
 * ============================================ */
void
iSQLProperty::ShowStmt( iSQLOptionKind   a_iSQLOptionKind )
{
    SChar tmp[20];

    switch(a_iSQLOptionKind)
    {
    case iSQL_SHOW_ALL :
        idlOS::sprintf(gSpool->m_Buf, "User      : %s\n", m_UserName);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "ColSize   : %d\n", m_ColSize);
        gSpool->Print(); 
        idlOS::sprintf(gSpool->m_Buf, "LobOffset : %d\n", m_LobOffset);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "LineSize  : %d\n", m_LineSize);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "LobSize   : %d\n", m_LobSize);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "NumWidth  : %d\n", m_NumWidth);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "NumFormat : \"%s\"\n", mNumFormat);
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "PageSize  : %d\n", m_PageSize);
        gSpool->Print();

        if (m_TimeScale == iSQL_SEC)
            idlOS::strcpy(tmp, "Second");
        else if (m_TimeScale == iSQL_MILSEC)
            idlOS::strcpy(tmp, "MilliSecond");
        else if (m_TimeScale == iSQL_MICSEC)
            idlOS::strcpy(tmp, "MicroSecond");
        else if (m_TimeScale == iSQL_NANSEC)
            idlOS::strcpy(tmp, "NanoSecond");
        idlOS::sprintf(gSpool->m_Buf, "TimeScale : %s\n", tmp);
        gSpool->Print();

        if (m_Heading == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Heading   : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Heading   : Off\n");
        gSpool->Print();

        if (m_Timing == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Timing    : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Timing    : Off\n");
        gSpool->Print();

        if (m_Vertical == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Vertical  : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Vertical  : Off\n");
        gSpool->Print();

        /* PROJ-1107 Check Constraint ���� */
        if ( m_ShowCheckConstraints == ID_TRUE )
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"ChkConstraints : On\n" );
        }
        else
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"ChkConstraints : Off\n" );
        }
        gSpool->Print();

        if (m_ShowForeignKeys == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"ForeignKeys : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"ForeignKeys : Off\n");
        }
        gSpool->Print();

        /* BUG-43516 */
        if (m_ShowPartitions == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Partitions : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Partitions : Off\n");
        }
        gSpool->Print();

        if (m_PlanCommit == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"PlanCommit : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"PlanCommit : Off\n");
        }
        gSpool->Print();

        if (m_QueryLogging == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"QueryLogging : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"QueryLogging : Off\n");
        }
        gSpool->Print();
        if (m_Term == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Term : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Term : Off\n");
        }
        gSpool->Print();
        if ( GetEcho() == ID_TRUE )
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"Echo : On\n" );
        }
        else
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"Echo : Off\n" );
        }
        gSpool->Print();
        idlOS::sprintf(gSpool->m_Buf, "Feedback : %d\n", m_Feedback );
        gSpool->Print();
        if ( mFullName == ID_TRUE )
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"Fullname : On\n" );
        }
        else
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"Fullname : Off\n" );
        }
        gSpool->Print();

        /*BUG-41163 SET SQLP[ROMPT] */
        if (mCustPrompt[0] != '\0')
        {
            idlOS::sprintf(gSpool->m_Buf, "Sqlprompt : \"%s\"\n", mCustPrompt);
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "Sqlprompt : \"%s\"\n", mSqlPrompt);
        }
        gSpool->Print();

        /* BUG-43537 */
        if ( GetDefine() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Define : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Define : Off\n");
        }
        gSpool->Print();

        /* BUG-43599 SET VERIFY ON|OFF */
        if ( GetVerify() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Verify : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Verify : Off\n");
        }
        gSpool->Print();

        /* BUG-44613 Set PrefetchRows */
        idlOS::sprintf(gSpool->m_Buf, "PrefetchRows : %d\n", m_PrefetchRows);
        gSpool->Print();

        /* BUG-44613 Set AsyncPrefetch On|Auto|Off */
        if ( GetAsyncPrefetch() == ASYNCPREFETCH_ON )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : On\n");
        }
        else if ( GetAsyncPrefetch() == ASYNCPREFETCH_OFF )
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : Off\n");
        }
        else if ( GetAsyncPrefetch() == ASYNCPREFETCH_AUTO_TUNING )
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : Auto\n");
        }
        else
        {
            /* Do nothing */
        }
        gSpool->Print();

        /* BUG-47627 SET MULTIERROR ON|OFF */
        if ( GetMultiError() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"MultiError : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"MultiError : Off\n");
        }
        gSpool->Print();

        break;

    case iSQL_HEADING :
        if (m_Heading == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Heading : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Heading : Off\n");
        gSpool->Print();
        break;
    case iSQL_COLSIZE :
        idlOS::sprintf(gSpool->m_Buf, "ColSize  : %d\n", m_ColSize);
        gSpool->Print();
        break;
    case iSQL_LINESIZE :
        idlOS::sprintf(gSpool->m_Buf, "LineSize : %d\n", m_LineSize);
        gSpool->Print();
        break;
    case iSQL_LOBOFFSET :
        idlOS::sprintf(gSpool->m_Buf, "LobOffset: %d\n", m_LobOffset);
        gSpool->Print();
        break;
    case iSQL_LOBSIZE :
        idlOS::sprintf(gSpool->m_Buf, "LobSize  : %d\n", m_LobSize);
        gSpool->Print();
        break;
    case iSQL_NUMWIDTH :
        idlOS::sprintf(gSpool->m_Buf, "NumWidth  : %d\n", m_NumWidth);
        gSpool->Print();
        break;
    case iSQL_NUMFORMAT :
        idlOS::sprintf(gSpool->m_Buf, "NumFormat : \"%s\"\n", mNumFormat);
        gSpool->Print();
        break;
    case iSQL_PAGESIZE :
        idlOS::sprintf(gSpool->m_Buf, "PageSize : %d\n", m_PageSize);
        gSpool->Print();
        break;
    case iSQL_SQLPROMPT :
        if (mCustPrompt[0] != '\0')
        {
            idlOS::sprintf(gSpool->m_Buf, "Sqlprompt : \"%s\"\n", mCustPrompt);
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "Sqlprompt : \"%s\"\n", mSqlPrompt);
        }
        gSpool->Print();
        break;
    case iSQL_TERM :
        if (m_Term == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Term : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Term : Off\n");
        gSpool->Print();
        break;
    case iSQL_TIMING :
        if (m_Timing == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Timing : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Timing : Off\n");
        gSpool->Print();
        break;
    case iSQL_VERTICAL :
        if (m_Vertical == ID_TRUE)
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Vertical  : On\n");
        else
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Vertical  : Off\n");
        gSpool->Print();
        break;
    case iSQL_CHECKCONSTRAINTS : /* PROJ-1107 Check Constraint ���� */
        if ( m_ShowCheckConstraints == ID_TRUE )
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"ChkConstraints : On\n" );
        }
        else
        {
            idlOS::sprintf( gSpool->m_Buf, "%s",
                            (SChar*)"ChkConstraints : Off\n" );
        }
        gSpool->Print();
        break;
    case iSQL_FOREIGNKEYS :
        if (m_ShowForeignKeys == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"ForeignKeys : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"ForeignKeys : Off\n");
        }
        gSpool->Print();
        break;
    case iSQL_PARTITIONS : /* BUG-43516 */
        if (m_ShowPartitions == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Partitions : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"Partitions : Off\n");
        }
        gSpool->Print();
        break;
    case iSQL_PLANCOMMIT :
        if (m_PlanCommit == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"PlanCommit : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"PlanCommit : Off\n");
        }
        gSpool->Print();
        break;
    case iSQL_QUERYLOGGING :
        if (m_QueryLogging  == ID_TRUE)
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"QueryLogging : On\n");
        }
        else
        {
            idlOS::sprintf(gSpool->m_Buf, "%s",
                           (SChar*)"QueryLogging : Off\n");
        }
        gSpool->Print();
        break;
    case iSQL_USER :
        idlOS::sprintf(gSpool->m_Buf, "User : %s\n", m_UserName);
        gSpool->Print();
        break;
    case iSQL_TIMESCALE :
        if (m_TimeScale == iSQL_SEC)
            idlOS::strcpy(tmp, "Second");
        else if (m_TimeScale == iSQL_MILSEC)
            idlOS::strcpy(tmp, "MilliSecond");
        else if (m_TimeScale == iSQL_MICSEC)
            idlOS::strcpy(tmp, "MicroSecond");
        else if (m_TimeScale == iSQL_NANSEC)
            idlOS::strcpy(tmp, "NanoSecond");
        idlOS::sprintf(gSpool->m_Buf, "TimeScale : %s\n", tmp);
        gSpool->Print();
        break;
    case iSQL_FEEDBACK:
        idlOS::sprintf(gSpool->m_Buf, "Feedback : %d\n",  m_Feedback );
        gSpool->Print();
        break;
    /* BUG-37772 */
    case iSQL_ECHO:
        if ( GetEcho() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Echo : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Echo : Off\n");
        }
        gSpool->Print();
        break;
    /* BUG-39620 */
    case iSQL_FULLNAME:
        if ( GetFullName() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"FullName : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"FullName : Off\n");
        }
        gSpool->Print();
        break;
    /* BUG-43537 */
    case iSQL_DEFINE:
        if ( GetDefine() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Define : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Define : Off\n");
        }
        gSpool->Print();
        break;

    /* BUG-43599 SET VERIFY ON|OFF */
    case iSQL_VERIFY:
        if ( GetVerify() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Verify : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"Verify : Off\n");
        }
        gSpool->Print();
        break;

    /* BUG-44613 Set PrefetchRows */
    case iSQL_PREFETCHROWS:
        idlOS::sprintf(gSpool->m_Buf, "PrefetchRows : %d\n", m_PrefetchRows);
        gSpool->Print();
        break;

    /* BUG-44613 Set AsyncPrefetch On|Auto|Off */
    case iSQL_ASYNCPREFETCH:
        if ( GetAsyncPrefetch() == ASYNCPREFETCH_ON )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : On\n");
        }
        else if ( GetAsyncPrefetch() == ASYNCPREFETCH_OFF )
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : Off\n");
        }
        else if ( GetAsyncPrefetch() == ASYNCPREFETCH_AUTO_TUNING )
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"AsyncPrefetch : Auto\n");
        }
        else
        {
            /* Do nothing */
        }
        gSpool->Print();
        break;

    /* BUG-47627 SET MULTIERROR ON|OFF */
    case iSQL_MULTIERROR:
        if ( GetMultiError() == ID_TRUE )
        {
            idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"MultiError : On\n");
        }
        else
        {
          idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"MultiError : Off\n");
        }
        gSpool->Print();
        break;

    default :
	// BUG-32613 The English grammar of the iSQL message "Have no saved command." needs to be corrected.
        idlOS::sprintf(gSpool->m_Buf, "%s", (SChar*)"No information to show.\n");
        gSpool->Print();
        break;
    }
}

/* BUG-37772 */
void iSQLProperty::SetEcho( idBool  aIsEcho )
{
    mEcho = aIsEcho;
}

idBool iSQLProperty::GetEcho( void )
{
    return mEcho;
}

/* ============================================
 * Set FULLNAME
 * 40 bytes �̻� ������ ��ü �̸��� �߶� �Ǵ�
 * ��� ���÷����� ������ ����
 * ============================================ */
void
iSQLProperty::SetFullName( idBool  aIsFullName )
{
    mFullName = aIsFullName;
    if (aIsFullName == ID_TRUE)
    {
        gExecuteCommand->SetObjectDispLen(QP_MAX_NAME_LEN);
    }
    else
    {
        /* BUG-39620: 40 is old display length for meta */
        gExecuteCommand->SetObjectDispLen(40);
    }
}

idBool iSQLProperty::GetFullName( void )
{
    return mFullName;
}

/* ============================================
 * BUG-41163: SET SQLP[ROMPT]
 * iSQL command prompt �ʱ�ȭ
 * ============================================ */
void iSQLProperty::InitSqlPrompt()
{
    if (mIsSysDBA == ID_TRUE)
    {
        idlOS::strcpy(mSqlPrompt, ISQL_PROMPT_SYSDBA_STR);
    }
    else
    {
        idlOS::strcpy(mSqlPrompt, ISQL_PROMPT_DEFAULT_STR);
    }
    mCustPrompt[0] = '\0';
}

void
iSQLProperty::ResetSqlPrompt()
{
    if (mCustPrompt[0] == '\0')
    {
        InitSqlPrompt();
    }
}

/* ============================================
 * BUG-41163: SET SQLP[ROMPT]
 * iSQL command prompt ����
 * ============================================ */
void
iSQLProperty::SetSqlPrompt( SChar * aSqlPrompt )
{
    parseSqlPrompt(aSqlPrompt);
}

/*
 * _PRIVILEGE �Ǵ� _USER ������ ���Ե� prompt�� ��쿡��
 * �Ź� �Ľ����� �ʰ� CONNECT ��ɾ ����� ���� �Ľ��Ѵ�.
 * �� PROMPT_VARIABLE_ON, PROMPT_RECONNECT_ON �� ��� ������ ��쿡��
 */
SChar * iSQLProperty::GetSqlPrompt( void )
{
    if ( mPromptRefreshFlag == PROMPT_REFRESH_ON )
    {
        parseSqlPrompt(mCustPrompt);
    }
    else
    {
        /* do nothing. */
    }
    return mSqlPrompt;
}

/*
 * CONNECT ��ɾ ����� ��� PROMPT_RECONNECT_ON bit�� �����ϱ� ����,
 * �� �Լ��� ȣ��ȴ�.
 */
void iSQLProperty::SetPromptRefreshFlag(UInt aFlag)
{
    mPromptRefreshFlag = mPromptRefreshFlag | aFlag;
}

void
iSQLProperty::parseSqlPrompt( SChar * aSqlPrompt )
{
    SInt   sRet;
    UInt   sPromptRefreshFlag = PROMPT_REFRESH_OFF;
    SChar  sNewPrompt[WORD_LEN];

    sNewPrompt[0] ='\0';
    sRet = lexSqlPrompt(aSqlPrompt, sNewPrompt, &sPromptRefreshFlag);

    if (sRet == IDE_SUCCESS)
    {
        idlOS::strcpy(mSqlPrompt, sNewPrompt);
        idlOS::strcpy(mCustPrompt, aSqlPrompt);
        mPromptRefreshFlag = sPromptRefreshFlag;
    }
    else
    {
        uteSprintfErrorCode(gSpool->m_Buf,
                            gProperty.GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
}

/* ============================================
 * BUG-41173
 * Set Define
 * turns substitution on/off
 * ============================================ */
void
iSQLProperty::SetDefine( idBool  a_IsDefine )
{
    m_Define = a_IsDefine;
}

/* ============================================
 * BUG-40246
 * COLUMN column CLEAR
 * ============================================ */
IDE_RC iSQLProperty::ClearFormat()
{
    SChar *sColumnName = NULL;

    sColumnName = gCommand->GetColumnName();

    IDE_TEST(deleteNode(sColumnName) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    uteSetErrorCode(&gErrorMgr, utERR_ABORT_Column_Not_Defined,
                    sColumnName);
    uteSprintfErrorCode(gSpool->m_Buf,
                        gProperty.GetCommandLen(),
                        &gErrorMgr);
    gSpool->Print();

    return IDE_FAILURE;
}

/* ============================================
 * BUG-40246
 * COLUMN column ON|OFF
 * ============================================ */
IDE_RC iSQLProperty::TurnColumnOnOff()
{
    idBool       sOnOff;
    SChar       *sKey = NULL;
    SChar       *sColumnName = NULL;
    isqlFmtNode *sNode = NULL;

    sColumnName = gCommand->GetColumnName();
    sOnOff = gCommand->GetOnOff();

    sNode = getNode(sColumnName);

    if (sNode == NULL)
    {
        sKey = (SChar*)idlOS::malloc(idlOS::strlen(sColumnName)+1);
        IDE_TEST_RAISE(sKey == NULL, malloc_error);

        idlOS::strcpy(sKey, sColumnName);

        IDE_TEST(insertNode(sKey,        // Column Name
                            FMT_UNKNOWN, // Type
                            NULL,        // Format
                            0,           // Column Size
                            NULL,        // Token for Number format
                            sOnOff) != IDE_SUCCESS);
    }
    else
    {
        sNode->mOnOff = sOnOff;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(malloc_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        Exit(0);
    }
    IDE_EXCEPTION_END;

    if (sKey != NULL)
    {
        idlOS::free(sKey);
    }
    return IDE_FAILURE;
}

/* ============================================
 * BUG-40246
 * COLUMN 
 * ============================================ */
IDE_RC
iSQLProperty::ListAllColumns()
{
    UInt         sCnt = 0;
    isqlFmtNode *sNode = NULL;

    sNode = mColFmtList;
    while (sNode != NULL)
    {
        if (sCnt > 0)
        {
            idlOS::sprintf(gSpool->m_Buf, "\n");
            gSpool->Print();
        }
        printNode(sNode);
        sNode = sNode->mNext;
        sCnt++;
    }
    return IDE_SUCCESS;
}

/* ============================================
 * BUG-40246
 * COLUMN column
 * ============================================ */
IDE_RC
iSQLProperty::ListColumn()
{
    SChar       *sColumnName = NULL;
    isqlFmtNode *sNode = NULL;

    sColumnName = gCommand->GetColumnName();

    sNode = getNode(sColumnName);

    IDE_TEST(sNode == NULL);

    printNode(sNode);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    uteSetErrorCode(&gErrorMgr, utERR_ABORT_Column_Not_Defined,
                    sColumnName);
    uteSprintfErrorCode(gSpool->m_Buf,
                        gProperty.GetCommandLen(),
                        &gErrorMgr);
    gSpool->Print();

    return IDE_FAILURE;
}

/* ============================================
 * BUG-40246
 * COLUMN char_column FORMAT fmt
 * ============================================ */
void
iSQLProperty::SetFormat4Char()
{
    SInt   sSize;
    SChar *sFmt = NULL;
    SChar *sKey = NULL;
    SChar *sValue = NULL;
    SChar *sColumnName = NULL;
    isqlFmtNode *sNode = NULL;

    sFmt = gCommand->GetFormatStr();
    sSize = idlOS::atoi(sFmt + 1);

    IDE_TEST_RAISE(sSize == 0 || sSize > 32767, illegal_format);

    sColumnName = gCommand->GetColumnName();

    sKey = (SChar*)idlOS::malloc(idlOS::strlen(sColumnName)+1);
    IDE_TEST_RAISE(sKey == NULL, malloc_error);

    idlOS::strcpy(sKey, sColumnName);

    sValue = (SChar*)idlOS::malloc(idlOS::strlen(sFmt)+1);
    IDE_TEST_RAISE(sValue == NULL, malloc_error);

    idlOS::strcpy(sValue, sFmt);

    sNode = getNode(sColumnName);

    if (sNode == NULL)
    {
        IDE_TEST(insertNode(sKey,        // Column Name
                            FMT_CHR,     // Type
                            sValue,      // Format
                            sSize,       // Column Size
                            NULL,        // Token for only Number format
                            ID_TRUE) != IDE_SUCCESS);
    }
    else
    {
        changeNode(sNode,
                   sKey,
                   FMT_CHR,
                   sValue,
                   sSize,
                   NULL);
    }

    return;

    IDE_EXCEPTION(illegal_format);
    {
        uteSetErrorCode(&gErrorMgr,
                        utERR_ABORT_Illegal_Format_String,
                        sFmt);
        uteSprintfErrorCode(gSpool->m_Buf,
                            gProperty.GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(malloc_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        Exit(0);
    }
    IDE_EXCEPTION_END;

    if (sKey != NULL)
    {
        idlOS::free(sKey);
    }
    if (sValue != NULL)
    {
        idlOS::free(sValue);
    }
    return;
}

SInt iSQLProperty::GetCharSize(SChar *aColumnName)
{
    isqlFmtNode *sNode = NULL;

    sNode = getNode(aColumnName);

    IDE_TEST(sNode == NULL);

    IDE_TEST(sNode->mOnOff != ID_TRUE);

    return sNode->mColSize;

    IDE_EXCEPTION_END;

    return 0;
}

/* ============================================
 * BUG-34447
 * COLUMN numeric_column FORMAT fmt
 * ============================================ */
void
iSQLProperty::SetFormat4Num()
{
    SChar *sFmt = NULL;
    SChar *sKey = NULL;
    SChar *sValue = NULL;
    UChar *sToken = NULL;
    SChar *sColumnName = NULL;
    UInt   sFmtLen;
    UChar  sTokenBuf[MTD_NUMBER_MAX];
    isqlFmtNode *sNode = NULL;

    IDE_TEST_RAISE( gExecuteCommand->SetNlsCurrency() != IDE_SUCCESS,
                    nls_error );

    sFmt = gCommand->GetFormatStr();
    sFmtLen = idlOS::strlen(sFmt);

    IDE_TEST_RAISE( sFmtLen > MTC_TO_CHAR_MAX_PRECISION,
                    exceed_max_length );

    IDE_TEST_RAISE( isqlFloat::CheckFormat( (UChar*)sFmt,
                                            sFmtLen,
                                            sTokenBuf )
                    != IDE_SUCCESS, illegal_format );

    sColumnName = gCommand->GetColumnName();

    sKey = (SChar*)idlOS::malloc(idlOS::strlen(sColumnName)+1);
    IDE_TEST_RAISE(sKey == NULL, malloc_error);

    idlOS::strcpy(sKey, sColumnName);

    sValue = (SChar*)idlOS::malloc(idlOS::strlen(sFmt)+1);
    IDE_TEST_RAISE(sValue == NULL, malloc_error);

    sToken = (UChar*)idlOS::malloc(MTD_NUMBER_MAX);
    IDE_TEST_RAISE(sToken == NULL, malloc_error);

    idlOS::strcpy(sValue, sFmt);
    idlOS::memcpy(sToken, sTokenBuf, MTD_NUMBER_MAX);

    sNode = getNode(sColumnName);

    if (sNode == NULL)
    {
        IDE_TEST(insertNode(sKey,        // Column Name
                            FMT_NUM,     // Type
                            sValue,      // Format
                            0,           // Column Size
                            sToken,      // Token for Number format
                            ID_TRUE) != IDE_SUCCESS);
    }
    else
    {
        changeNode( sNode,
                    sKey,
                    FMT_NUM,
                    sValue,
                    0,
                    sToken );
    }

    return;

    IDE_EXCEPTION(exceed_max_length);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Exceed_Max_String,
                        MTC_TO_CHAR_MAX_PRECISION);
        uteSprintfErrorCode(gSpool->m_Buf,
                            gProperty.GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(illegal_format);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Illegal_Format_String,
                        sFmt);
        uteSprintfErrorCode(gSpool->m_Buf,
                            gProperty.GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(malloc_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);

        utePrintfErrorCode(stderr, &gErrorMgr);
        Exit(0);
    }
    IDE_EXCEPTION(nls_error);
    {
        uteSprintfErrorCode(gSpool->m_Buf, GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    if (sKey != NULL)
    {
        idlOS::free(sKey);
    }
    if (sValue != NULL)
    {
        idlOS::free(sValue);
    }
    if (sToken != NULL)
    {
        idlOS::free(sToken);
    }
    return;
}

SChar* iSQLProperty::GetColumnFormat(SChar          *aColumnName,
                                     isqlFmtType     aType, /* BUG-43351 */
                                     mtlCurrency    *aCurrency,
                                     SInt           *aColSize,
                                     UChar          **aToken)
{
    isqlFmtNode *sNode = NULL;

    *aColSize = 0; /* BUG-43351 Uninitialized Variable */
    *aToken = NULL;
    sNode = getNode(aColumnName);

    IDE_TEST(sNode == NULL);

    /* BUG-43351 CodeSonar warning: Uninitialized Variable */
    IDE_TEST(sNode->mType != aType);

    IDE_TEST(sNode->mOnOff != ID_TRUE);

    IDE_TEST_CONT(sNode->mToken == NULL, ret_code);

    *aColSize = isqlFloat::GetColSize( aCurrency, sNode->mFmt, sNode->mToken );

    *aToken = sNode->mToken;

    IDE_EXCEPTION_CONT( ret_code );

    return sNode->mFmt;

    IDE_EXCEPTION_END;

    return NULL;
}

/* ============================================
 * BUG-40246
 * Operations For FmtNodeList
 *  - getNode
 *  - insertNode
 *  - deleteNode
 * ============================================ */
isqlFmtNode* iSQLProperty::getNode(SChar *aColumnName)
{
    isqlFmtNode *sTmpNode  = NULL;
    isqlFmtNode *sNode  = NULL;

    sTmpNode = mColFmtList;
    while (sTmpNode != NULL)
    {
        if ( idlOS::strcasecmp(sTmpNode->mKey, aColumnName) == 0 )
        {
            sNode = sTmpNode;
            break;
        }
        else
        {
            sTmpNode = sTmpNode->mNext;
        }
    }
    return sNode;
}

IDE_RC iSQLProperty::insertNode(SChar       *aColumnName,
                                isqlFmtType  aType,
                                SChar       *aFmt,
                                SInt         aColSize,
                                UChar       *aToken,
                                idBool       aOnOff /* = ID_TRUE */)
{
    isqlFmtNode *sNode = NULL;

    sNode = (isqlFmtNode *)idlOS::malloc(ID_SIZEOF(isqlFmtNode));
    IDE_TEST_RAISE(sNode == NULL, malloc_error);

    sNode->mType = aType;
    sNode->mOnOff = aOnOff;
    sNode->mKey = aColumnName;
    sNode->mFmt = aFmt;
    sNode->mColSize = aColSize;
    sNode->mToken = aToken;
    sNode->mNext = mColFmtList;
    mColFmtList = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION(malloc_error);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        Exit(0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iSQLProperty::deleteNode(SChar *aColumnName)
{
    isqlFmtNode *sNode = NULL;
    isqlFmtNode *sPrevNode = NULL;

    sNode = mColFmtList;
    while (sNode != NULL)
    {
        if (idlOS::strcasecmp(aColumnName, sNode->mKey) == 0)
        {
            if (sNode == mColFmtList)
            {
                mColFmtList = sNode->mNext;
                IDE_CONT(node_found);
            }
            else
            {
                sPrevNode->mNext = sNode->mNext;
                IDE_CONT(node_found);
            }
        }
        else
        {
            sPrevNode = sNode;
            sNode = sNode->mNext;
        }
    }

    IDE_RAISE(node_not_found);

    IDE_EXCEPTION_CONT(node_found);

    if (sNode->mFmt != NULL)
    {
        idlOS::free(sNode->mFmt);
    }
    if (sNode->mToken != NULL)
    {
        idlOS::free(sNode->mToken);
    }
    idlOS::free(sNode->mKey);
    idlOS::free(sNode);

    return IDE_SUCCESS;

    IDE_EXCEPTION(node_not_found);
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void iSQLProperty::changeNode(isqlFmtNode *aNode,
                              SChar       *aColumnName,
                              isqlFmtType  aType,
                              SChar       *aFmt,
                              SInt         aColSize,
                              UChar       *aToken)
{
    if (aNode->mFmt != NULL)
    {
        idlOS::free(aNode->mFmt);
    }
    if (aNode->mToken != NULL)
    {
        idlOS::free(aNode->mToken);
    }
    idlOS::free(aNode->mKey);

    aNode->mKey = aColumnName;
    aNode->mType = aType;
    aNode->mFmt = aFmt;
    aNode->mColSize = aColSize;
    aNode->mToken = aToken;
}

void iSQLProperty::freeFmtList()
{
    isqlFmtNode *sNode = NULL;
    isqlFmtNode *sTmpNode = NULL;

    sNode = mColFmtList;
    while (sNode != NULL)
    {
        sTmpNode = sNode->mNext;
        if (sNode->mFmt != NULL)
        {
            idlOS::free(sNode->mFmt);
        }
        if (sNode->mToken != NULL)
        {
            idlOS::free(sNode->mToken);
        }
        idlOS::free(sNode->mKey);
        idlOS::free(sNode);
        sNode = sTmpNode;
    }
    mColFmtList = NULL;
}

void iSQLProperty::printNode(isqlFmtNode *aNode)
{
    idlOS::sprintf(gSpool->m_Buf, "COLUMN   %s %s\n",
                   aNode->mKey,
                   (aNode->mOnOff == ID_TRUE)? "ON":"OFF");
    gSpool->Print();

    if (aNode->mFmt != NULL)
    {
        idlOS::sprintf(gSpool->m_Buf, "FORMAT   %s\n", aNode->mFmt);
        gSpool->Print();
    }
}

/* BUG-34447 SET NUMFORMAT */
IDE_RC iSQLProperty::SetNumFormat()
{
    SChar *sFmt = NULL;
    UInt   sFmtLen;
    UChar  sToken[MTD_NUMBER_MAX];

    IDE_TEST_RAISE( gExecuteCommand->SetNlsCurrency() != IDE_SUCCESS,
                    nls_error );

    sFmt = gCommand->GetFormatStr();
    sFmtLen = idlOS::strlen(sFmt);

    IDE_TEST_RAISE( isqlFloat::CheckFormat( (UChar*)sFmt,
                                            sFmtLen,
                                            sToken )
                    != IDE_SUCCESS, illegal_format );

    idlOS::strcpy(mNumFormat, sFmt);
    idlOS::memcpy(mNumToken, sToken, MTD_NUMBER_MAX);

    return IDE_SUCCESS;

    IDE_EXCEPTION(illegal_format);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_Illegal_Format_String,
                        sFmt);
        uteSprintfErrorCode(gSpool->m_Buf,
                            gProperty.GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION(nls_error);
    {
        uteSprintfErrorCode(gSpool->m_Buf, GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SChar * iSQLProperty::GetNumFormat(mtlCurrency  *aCurrency,
                                   SInt         *aColSize,
                                   UChar       **aToken)
{
    *aToken = NULL;

    IDE_TEST_CONT( mNumFormat == NULL, ret_code );

    *aColSize = isqlFloat::GetColSize( aCurrency, mNumFormat, mNumToken );

    *aToken = mNumToken;

    IDE_EXCEPTION_CONT( ret_code );

    return mNumFormat;
}

/* ============================================
 * BUG-40246
 * CLEAR
 * ============================================ */
IDE_RC iSQLProperty::ClearAllFormats()
{
    freeFmtList();

    return IDE_SUCCESS;
}

/* BUG-43599 SET VERIFY ON|OFF */
void
iSQLProperty::SetVerify( idBool  a_IsVerify )
{
    m_Verify = a_IsVerify;
}

/* BUG-44613 Set PrefetchRows */
void
iSQLProperty::SetPrefetchRows( SInt    a_PrefetchRows )
{
    IDE_TEST_RAISE(
                gExecuteCommand->SetPrefetchRows(a_PrefetchRows)
                != IDE_SUCCESS, set_error );

    m_PrefetchRows = a_PrefetchRows;

    return;

    IDE_EXCEPTION(set_error);
    {
        uteSprintfErrorCode(gSpool->m_Buf,
                            GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return;
}

/* BUG-44613 Set AsyncPrefetch On|Auto|Off */
void
iSQLProperty::SetAsyncPrefetch( AsyncPrefetchType  a_Type )
{
    IDE_TEST_RAISE( gExecuteCommand->SetAsyncPrefetch(a_Type)
                    != IDE_SUCCESS, set_error );

    m_AsyncPrefetch = a_Type;

    return;

    IDE_EXCEPTION(set_error);
    {
        uteSprintfErrorCode(gSpool->m_Buf, GetCommandLen(),
                            &gErrorMgr);
        gSpool->Print();
    }
    IDE_EXCEPTION_END;

    return;
}

/* BUG-47627 SET MULTIERROR ON|OFF */
void
iSQLProperty::SetMultiError( idBool  a_IsMultiError )
{
    m_MultiError = a_IsMultiError;
}

