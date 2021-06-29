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
 * $Id: utmUser.cpp $
 **********************************************************************/

#include <smDef.h>
#include <utm.h>
#include <utmExtern.h>

#define GET_USER_PRIV_QUERY                                             \
    "select A.PRIV_NAME "                                               \
    "from system_.sys_privileges_ a,system_.sys_grant_system_ b,"       \
    "system_.sys_users_ c "                                             \
    "where a.priv_id=b.priv_id and "                                    \
    "b.grantee_id=c.user_id and a.priv_type=2 "                         \
    "and c.user_name=? "                                                \
    "order by 1"

#define GET_USER_COUNT_QUERY                    \
    "select/*+ NO_PLAN_CACHE */ count(*) from system_.sys_users_"


#define GET_USER_DETAIL_QUERY                                        \
    " select /*+ USE_HASH( A, B ) USE_HASH( A, C ) NO_PLAN_CACHE */" \
    " a.USER_ID, a.USER_NAME,"                                       \
    " a.user_type "                                                  \
    " from system_.sys_users_ a, x$tablespaces_header b, "           \
    " x$tablespaces_header c"                                        \
    " where b.id = a.default_tbs_id"                                 \
    " and c.id = a.temp_tbs_id "                                     \
    " and user_name not in ('SYS', 'SYSTEM_', 'PUBLIC') "            \
    " order by a.user_type desc,1 "

/* BUG-31699 */
#define GET_NO_EXIST_TABLESPACE_USER_QUERY               \
    " select A.USER_NAME from SYSTEM_.SYS_USERS_ A "     \
    " where A.DEFAULT_TBS_ID "                           \
    " not in ( select B.ID from V$TABLESPACES B) "

/* PROJ-1812 ROLE */  
#define GET_ROLE_TO_USER_QUERY                                                  \
    " select b.user_name from system_.sys_user_roles_ a,system_.sys_users_ b, " \
    " (select user_id from system_.sys_users_  "                                \
    " where user_name = ?) c "                                                  \
    " where a.role_id = c.user_id "                                             \
    " and a.grantee_id = b.user_id "                                            \
    
#define MAX_OUTPUT_CNT 100

/* tablespace�� ���� �ǰų� ���� ���� �ʴ� USER NAME ���Ͽ� ���� ��� */
void setErrNoExistTBSUser()
{
#define IDE_FN "setErrNoExistTBSUser()"
    SQLHSTMT    sStmt = SQL_NULL_HSTMT;
    SQLRETURN   sRet;
    SChar       sUserName[UTM_NAME_LEN+1] = {'\0',};
    SChar       sQuery[QUERY_LEN / 2];  
    SQLLEN      sUserNameInd;
    SChar       sTotalUserName[UTM_NAME_LEN * MAX_OUTPUT_CNT + 6] = {'\0', };
    SInt        i;

    IDE_TEST_RAISE( SQLAllocStmt(m_hdbc, &sStmt) != SQL_SUCCESS, DBCError );

    idlOS::sprintf( sQuery, GET_NO_EXIST_TABLESPACE_USER_QUERY );

    IDE_TEST_RAISE( SQLExecDirect( sStmt,
                                   (SQLCHAR *)sQuery,
                                   SQL_NTS) != SQL_SUCCESS, StmtError );

    IDE_TEST_RAISE( SQLBindCol( sStmt,
                                1,
                                SQL_C_CHAR,
                                (SQLPOINTER)sUserName,
                                (SQLLEN)ID_SIZEOF( sUserName ),
                                &sUserNameInd ) != SQL_SUCCESS, StmtError );

    /* user�� ���̳� ������ �� �� �����Ƿ�, MAX_OUTPUT_CNT ������
     * ������ ����Ѵ�. �� ������ ����ǥ(...)�� �̿��� �� ������ ǥ��. */
    for ( i = 0; i < MAX_OUTPUT_CNT; i++ )
    {
        sRet = SQLFetch( sStmt );
        if ( sRet == SQL_NO_DATA )
        {
            break;
        }
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, StmtError );

        if ( i > 0 )
        {
            idlOS::strcat( sTotalUserName, "," );
        }
        idlOS::strcat( sTotalUserName, sUserName );
    }
    if ( i == MAX_OUTPUT_CNT )
    {
        idlOS::strcat( sTotalUserName, ", ..." );
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &sStmt );

    if ( i > 0 )
    {
        uteSetErrorCode( &gErrorMgr,
                         utERR_ABORT_NOT_EXIST_TABLESPACE_ERR,
                         sTotalUserName );
    }

    return;

    IDE_EXCEPTION( DBCError );
    {
        utmSetErrorMsgWithHandle( SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc );
    }
    IDE_EXCEPTION( StmtError );
    {
        utmSetErrorMsgWithHandle( SQL_HANDLE_STMT, (SQLHANDLE)sStmt );
    }
    IDE_EXCEPTION_END;

    // BUG-33995 aexport have wrong free handle code
    if ( sStmt != SQL_NULL_HSTMT )
    {
        FreeStmt( &sStmt );
    }

    // BUG-35396 Free Non-Heap Variable at utmUser.cpp: recall BUG-35093

   return;
#undef IDE_FN
}

/* BUG-44595 */
SInt getPrivNo( const SChar *aPrivName )
{
#define IDE_FN "getPrivNo()"
    SInt sPrivNo = UTM_NONE;

    /* BUG-22769
     * ����� ���� ������ �⺻������ �����Ǵ� ������ �Ʒ� 8�� �ִ�.
     * 0. CREATE_PROCEDURE
     * 1. CREATE_SEQUENCE
     * 2. CREATE_SESSION
     * 3. CREATE_SYNONYM
     * 4. CREATE_TABLE
     * 5. CREATE_TRIGGER
     * 6. CREATE_VIEW
     * 7. CREATE_MATERIALIZED_VIEW
     * 8. CREATE_DATABASE_LINK
     * 9. CREATE_LIBRARY
     * sDefaultPriv[] �� array ��ȣ�� ���� ���ڸ� ����Ѵ�.
     */
    if ( idlOS::strcmp( aPrivName, "CREATE PROCEDURE" ) == 0 )
    {
        sPrivNo = UTM_CREATE_PROCEDURE;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE SEQUENCE" ) == 0 )
    {
        sPrivNo = UTM_CREATE_SEQUENCE;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE SESSION" ) == 0 )
    {
        sPrivNo = UTM_CREATE_SESSION;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE SYNONYM" ) == 0 )
    {
        sPrivNo = UTM_CREATE_SYNONYM;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE TABLE" ) == 0 )
    {
        sPrivNo = UTM_CREATE_TABLE;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE TRIGGER" ) == 0 )
    {
        sPrivNo = UTM_CREATE_TRIGGER;
    }
    else if ( idlOS::strcmp( aPrivName, "CREATE VIEW" ) == 0 )
    {
        sPrivNo = UTM_CREATE_VIEW;
    }
    /* PROJ-2211 Materialized View */
    else if ( idlOS::strcmp( aPrivName, "CREATE MATERIALIZED VIEW" ) == 0 )
    {
        sPrivNo = UTM_CREATE_MATERIALIZED_VIEW;
    }
    /* BUG-36830 aexport must handle new privileges */
    else if( idlOS::strcmp( aPrivName, "CREATE DATABASE LINK" ) == 0 )
    {
        sPrivNo = UTM_CREATE_DATABASE_LINK;
    }
    else if( idlOS::strcmp( aPrivName, "CREATE LIBRARY" ) == 0 )
    {
        sPrivNo = UTM_CREATE_LIBRARY;
    }
    else
    {
        sPrivNo = UTM_NONE;
    }

    return sPrivNo;
#undef IDE_FN
}

SQLRETURN getUserPrivileges( FILE  *aFp,
                             SChar *aUserName,
                             idBool aIsRole )
{
#define IDE_FN "getUserPrivileges()"
    idBool         sAllFlag = ID_FALSE;
    SQLLEN         sPrivNameInd;
    SChar          sQuery[QUERY_LEN];
    SChar          sPrivName[256];
    SQLHSTMT       sPrivStmt = SQL_NULL_HSTMT;
    SQLRETURN      sRet;
    //fix BUG-22905.
    UInt           i;
    //BUG-22769
    idBool         sDefaultPriv[UTM_PRIV_COUNT];   //������ revoke �Ǿ������� �Ǵ��ϱ� ���� ���ȴ�.
    SChar          sPrivList[QUERY_LEN] = { '\0', };
    SInt           sPrivNo; /* BUG-44595 */

    //FALSE�� ����...
    idlOS::memset(sDefaultPriv, 0x00, ID_SIZEOF(idBool) * UTM_PRIV_COUNT);

    IDE_TEST_RAISE(SQLAllocStmt(m_hdbc, &sPrivStmt) != SQL_SUCCESS,
                   DBCError);

    idlOS::sprintf( sQuery, GET_USER_PRIV_QUERY );
      
    IDE_TEST(Prepare(sQuery, sPrivStmt) != SQL_SUCCESS);

    IDE_TEST_RAISE( SQLBindParameter(sPrivStmt,
                                     1,
                                     SQL_PARAM_INPUT,
                                     SQL_C_CHAR,
                                     SQL_VARCHAR,
                                     UTM_NAME_LEN,
                                     0,
                                     aUserName,
                                     UTM_NAME_LEN+1,
                                     NULL) != SQL_SUCCESS, StmtError);

    IDE_TEST_RAISE( SQLBindCol(sPrivStmt,
                               1,
                               SQL_C_CHAR,
                               (SQLPOINTER)sPrivName,
                               (SQLLEN)ID_SIZEOF(sPrivName),
                               &sPrivNameInd) != SQL_SUCCESS, StmtError);        

    IDE_TEST(Execute(sPrivStmt) != SQL_SUCCESS);
    
    //BUG-22769 revoke ��ų ���� �����ʱ�ȭ
    idlOS::strcpy( m_revokeStr, "" );
    
    while ((sRet = SQLFetch(sPrivStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE(sRet != SQL_SUCCESS, StmtError);

        replace( sPrivName );
        if ( idlOS::strcmp( sPrivName, "ALL" ) == 0 )
        {
            idlOS::fprintf( aFp, "GRANT ALL PRIVILEGES TO \"%s\";\n", aUserName );
            sAllFlag = ID_TRUE;
            break;
        }
        /* BUG-44594 */
        else if ( aIsRole == ID_TRUE )
        {
            idlOS::strcat( sPrivList, sPrivName );
            idlOS::strcat( sPrivList, ",\n" );
        }
        else
        {
            sPrivNo = getPrivNo( sPrivName );
            /* Check if sPrivName is default privilege */
            if ( sPrivNo >= 0 && sPrivNo < UTM_PRIV_COUNT)
            {
                sDefaultPriv[sPrivNo] = ID_TRUE;
            }
            else
            {
                idlOS::strcat( sPrivList, sPrivName );
                idlOS::strcat( sPrivList, ",\n" );
            }
        }
    }

    /* PROJ-1812 ROLE */
    if ( aIsRole != ID_TRUE )
    {
        // BUG-22769
        // �⺻������ ������ ������ revoke �Ǿ��� ����� ó��
        for ( i = 0; i < UTM_PRIV_COUNT; i++)
        {
            if ( sDefaultPriv[i] == ID_FALSE )
            {
                switch ( i )
                {
                case 0:
                    idlOS::strcat( m_revokeStr, "CREATE PROCEDURE" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 1:
                    idlOS::strcat( m_revokeStr, "CREATE SEQUENCE" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 2:
                    idlOS::strcat( m_revokeStr, "CREATE SESSION" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 3:
                    idlOS::strcat( m_revokeStr, "CREATE SYNONYM" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 4:
                    idlOS::strcat( m_revokeStr, "CREATE TABLE" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 5:
                    idlOS::strcat( m_revokeStr, "CREATE TRIGGER" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 6:
                    idlOS::strcat( m_revokeStr, "CREATE VIEW" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 7 : /* PROJ-2211 Materialized View */
                    idlOS::strcat( m_revokeStr, "CREATE MATERIALIZED VIEW" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                    /* BUG-36830 aexport must handle new privileges */
                case 8 :
                    idlOS::strcat( m_revokeStr, "CREATE DATABASE LINK" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                case 9 :
                    idlOS::strcat( m_revokeStr, "CREATE LIBRARY" );
                    idlOS::strcat( m_revokeStr, ",\n");
                    break;
                default:
                    continue;
                }
            }
        }
    }
    else
    {
        /* Nothing To Do */
    }
    
    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &sPrivStmt );

    if ( sAllFlag != ID_TRUE && idlOS::strlen( sPrivList ) > 0 )
    {
        sPrivList[ idlOS::strlen( sPrivList ) - 2 ] = ' ';
        idlOS::fprintf( aFp, "GRANT %s TO \"%s\";\n", sPrivList, aUserName );
    }
    
    // BUG-22769
    // ���� Revoke
    if ( sAllFlag != ID_TRUE && idlOS::strlen( m_revokeStr ) > 0 )
    {
        m_revokeStr[ idlOS::strlen(m_revokeStr) - 2 ] = ' ';
        idlOS::fprintf( aFp, "REVOKE %s FROM \"%s\";\n", m_revokeStr, aUserName );
    }
    return SQL_SUCCESS;

    IDE_EXCEPTION(DBCError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(StmtError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sPrivStmt);
    }
    IDE_EXCEPTION_END;

    if ( sPrivStmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &sPrivStmt );
    }

    return SQL_ERROR;
#undef IDE_FN
}

/* 
 * getUserQuery_user: 
 *   user mode���� user ���� ������ export�ϴ� �Լ�.
 *
 * BUG-40469:
 *   user mode���� TABLESPACE ������Ƽ�� ON�� ���,
 *   tablespace ���� ������ export �ϴ� ����� �߰���.
 *   ***
 *   export �Ǵ� ���̺����̽��� ����� ���� - ��,
 *   ������� default, temporary, access tablespace - �̴�.
 */
SQLRETURN getUserQuery_user( FILE  *aUserFp,
                             SChar *a_user_name,
                             SChar *a_passwd )
{
#define IDE_FN "getUserQuery_user()"

    idlOS::fprintf(stdout, "\n##### USER #####\n");

    idlOS::fprintf( aUserFp, "--############################\n");
    idlOS::fprintf( aUserFp, "--  USER \n");
    idlOS::fprintf( aUserFp, "--############################\n");

    if ( gProgOption.mbExistDrop == ID_TRUE )
    {
        idlOS::fprintf( aUserFp, "drop user \"%s\" cascade;\n", a_user_name );
    }

    /* BUG-47159 Using DBMS_METADATA package in aexport */
    IDE_TEST(gMeta->getUserDdl(a_user_name, a_passwd)
             != IDE_SUCCESS);

#ifdef DEBUG
    idlOS::fprintf( stderr, "%s\n", gMeta->getDdlStr() );
#endif
    idlOS::fprintf( aUserFp, "%s\n\n", gMeta->getDdlStr() );

    /* BUG-40469 output tablespaces info. in user mode */
    if ( gProgOption.mbCrtTbs4UserMode == ID_TRUE )
    {
        IDE_TEST(getTBSInfo4UserMode(gFileStream.mTbsFp,
                                     a_user_name) != SQL_SUCCESS);
    }

    IDE_TEST( getUserPrivileges( aUserFp, a_user_name, ID_FALSE ) != SQL_SUCCESS );

    idlOS::fflush( aUserFp );

    return SQL_SUCCESS;

    IDE_EXCEPTION_END;

    idlOS::fflush( aUserFp );

    return SQL_ERROR;
#undef IDE_FN
}

SQLRETURN getUserQuery( FILE  *aUserFp,
                        SInt  *a_user_cnt,
                        SChar *aUserName,
                        SChar *aPasswd )
{
#define IDE_FN "getUserQuery()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    SQLHSTMT s_userStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;

    SInt        i = 1;
    SInt        sUserCnt = 0;
    SInt        s_userID;
    SChar       s_user_name[UTM_NAME_LEN+1];
    SChar       s_query[QUERY_LEN / 2];
    SChar       s_tmp[UTM_NAME_LEN+1];
    SQLLEN      s_userID_ind;
    SQLLEN      s_user_ind;
    SQLLEN      s_UserType_ind = 0;
    SChar       s_UserType[2] = {0,};

    idlOS::fprintf(stdout, "\n##### USER  #####\n");

    IDE_TEST_RAISE(SQLAllocStmt(m_hdbc, &s_userStmt) != SQL_SUCCESS, DBCError);

    idlOS::sprintf(s_query, GET_USER_COUNT_QUERY);

    IDE_TEST_RAISE(SQLExecDirect(s_userStmt, (SQLCHAR *)s_query, SQL_NTS)
                   != SQL_SUCCESS, UserStmtError);

    IDE_TEST_RAISE(
        SQLBindCol(s_userStmt, 1, SQL_C_SLONG, (SQLPOINTER)&sUserCnt, 0, NULL)
        != SQL_SUCCESS, UserStmtError);        

    sRet = SQLFetch(s_userStmt);
    IDE_TEST(sRet == SQL_NO_DATA);
    IDE_TEST_RAISE(sRet != SQL_SUCCESS, UserStmtError);

    IDE_TEST_RAISE(SQLFreeStmt(s_userStmt, SQL_CLOSE)
                   != SQL_SUCCESS, UserStmtError);                   
                   
    IDE_TEST_RAISE(SQLFreeStmt(s_userStmt, SQL_UNBIND)
                   != SQL_SUCCESS, UserStmtError);                

    sUserCnt = sUserCnt - 2; // 'PUBLIC', 'SYSTEM_'

    if ( sUserCnt > 0 )
    {
        // BUG-35099: [ux] Codesonar warning UX part - 228797.2579802
        IDE_ASSERT( ( ID_SIZEOF(UserInfo) * sUserCnt ) < ID_UINT_MAX );
        m_UserInfo = (UserInfo *) idlOS::malloc(ID_SIZEOF(UserInfo) * sUserCnt);
        IDE_TEST(m_UserInfo == NULL);
    }
    idlOS::strcpy(m_UserInfo[0].m_user, aUserName);
    idlOS::strcpy(m_UserInfo[0].m_passwd, aPasswd);

    /* PROJ-1349
     * 1. TEMPORARY TABLESPACE tbs_name
     * 2. DEFAULT   TABLESPACE tbs_name
     * 3. ACCESS tbs_name ON/OFF
     * */
    idlOS::sprintf(s_query, GET_USER_DETAIL_QUERY);

    IDE_TEST_RAISE(SQLExecDirect(s_userStmt, (SQLCHAR *)s_query, SQL_NTS)
                   != SQL_SUCCESS, UserStmtError);

    IDE_TEST_RAISE(
        SQLBindCol(s_userStmt, 1, SQL_C_SLONG, (SQLPOINTER)&s_userID, 0,
                   &s_userID_ind) != SQL_SUCCESS, UserStmtError);
    IDE_TEST_RAISE(
        SQLBindCol(s_userStmt, 2, SQL_C_CHAR, (SQLPOINTER)s_user_name,
                   (SQLLEN)ID_SIZEOF(s_user_name), &s_user_ind)
        != SQL_SUCCESS, UserStmtError);
    /* PROJ-1812 ROLE */
    IDE_TEST_RAISE(
        SQLBindCol(s_userStmt, 3, SQL_C_CHAR, (SQLPOINTER)s_UserType,
                   (SQLLEN)ID_SIZEOF(s_UserType), &s_UserType_ind)
        != SQL_SUCCESS, UserStmtError);

    while ((sRet = SQLFetch(s_userStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, UserStmtError );

        /* PROJ-1812 ROLE */
        if ( idlOS::strcmp(s_UserType, "U") == 0 )
        {
            if ( gProgOption.mbExistUserPasswd == ID_TRUE )
            {
                idlOS::strcpy(s_tmp, gProgOption.mUserPasswd);
            }
            else
            {
                /* BUG-19012 */
                // password �Է� ���� enter �� �Է��� ���
                // default password �μ� user name �� ��.
                idlOS::fprintf(stdout, "** input user %s's password"
                               "(default - same with USER_NAME): ",
                               s_user_name);
                idlOS::gets(s_tmp, sizeof(s_tmp));
                eraseWhiteSpace(s_tmp);
                if ( idlOS::strcmp(s_tmp, "") == 0 )
                {
                    /* BUG-39622
                     * If the length of a user name is longer than the maximum length of password,
                     * the user name is truncated. */
                    if (idlOS::strlen(s_user_name) > 40)
                    {
                        idlOS::strncpy(s_tmp, s_user_name, 40);
                        s_tmp[40]= '\0';
                    }
                    else
                    {
                        idlOS::strcpy(s_tmp, s_user_name);
                    }
                }
            }
            idlOS::strcpy(m_UserInfo[i].m_user, s_user_name);
            idlOS::strcpy(m_UserInfo[i].m_passwd, s_tmp);

            idlOS::fprintf(aUserFp, "--############################\n");
            idlOS::fprintf(aUserFp, "--  USER \"%s\"\n", s_user_name);
            idlOS::fprintf(aUserFp, "--############################\n");

            if ( gProgOption.mbExistDrop == ID_TRUE )
            {
                idlOS::fprintf(aUserFp, "drop user \"%s\" cascade;\n", s_user_name);
            }

            /* BUG-47159 Using DBMS_METADATA package in aexport */
            IDE_TEST(gMeta->getUserDdl(s_user_name, s_tmp)
                     != IDE_SUCCESS);
#ifdef DEBUG
            idlOS::fprintf( stderr, "%s\n", gMeta->getDdlStr() );
#endif
            idlOS::fprintf( aUserFp, "%s\n\n", gMeta->getDdlStr() );

            IDE_TEST( getUserPrivileges( aUserFp, s_user_name, ID_FALSE ) != SQL_SUCCESS );
        }
        else
        {
            idlOS::strcpy(m_UserInfo[i].m_user, s_user_name);
            
            idlOS::fprintf(aUserFp, "--############################\n");
            idlOS::fprintf(aUserFp, "--  ROLE \"%s\"\n", s_user_name);
            idlOS::fprintf(aUserFp, "--############################\n");
            
            if ( gProgOption.mbExistDrop == ID_TRUE )
            {
                idlOS::fprintf(aUserFp, "drop role \"%s\";\n", s_user_name);
            }

            /* BUG-47159 Using DBMS_METADATA package in aexport */
            IDE_TEST(gMeta->getDdl(DDL, UTM_OBJ_TYPE_ROLE, s_user_name, (SChar*)NULL)
                     != IDE_SUCCESS);
#ifdef DEBUG
            idlOS::fprintf( stderr, "%s\n", gMeta->getDdlStr() );
#endif
            idlOS::fprintf( aUserFp, "%s\n\n", gMeta->getDdlStr() );

            /* role */
            IDE_TEST( getRoleToUser( aUserFp, s_user_name ) != SQL_SUCCESS );

            /* privilege */
            IDE_TEST( getUserPrivileges( aUserFp, s_user_name, ID_TRUE ) != SQL_SUCCESS ) ;
        }
        i++;   
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &s_userStmt );
    
    idlOS::fflush(aUserFp);

    IDE_TEST_RAISE( sUserCnt != i, NoExistTablespaceError );

    *a_user_cnt = i;

    return SQL_SUCCESS;

    IDE_EXCEPTION(DBCError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(UserStmtError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)s_userStmt);
    }
    IDE_EXCEPTION(NoExistTablespaceError);
    {
        setErrNoExistTBSUser();
    }
    IDE_EXCEPTION_END;

    idlOS::fflush(aUserFp);

    if ( s_userStmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &s_userStmt );
    }

    return SQL_ERROR;
#undef IDE_FN
}

/* PROJ-1812 ROLE */
SQLRETURN getRoleToUser( FILE  *aFp,
                         SChar *aRoleName )
{
    SQLLEN         sUserNameInd;
    SChar          sQuery[QUERY_LEN];
    SChar          sUserName[256];
    SQLHSTMT       sRoleStmt = SQL_NULL_HSTMT;
    SQLRETURN      sRet;

    IDE_TEST_RAISE(SQLAllocStmt(m_hdbc, &sRoleStmt) != SQL_SUCCESS,
                   DBCError);

    idlOS::sprintf( sQuery, GET_ROLE_TO_USER_QUERY );
      
    IDE_TEST(Prepare(sQuery, sRoleStmt) != SQL_SUCCESS);

    IDE_TEST_RAISE(
    SQLBindParameter(sRoleStmt, 1, SQL_PARAM_INPUT,
                     SQL_C_CHAR, SQL_VARCHAR, UTM_NAME_LEN, 0,
                     aRoleName, UTM_NAME_LEN+1, NULL)
                     != SQL_SUCCESS, StmtError);

    IDE_TEST_RAISE(
    SQLBindCol(sRoleStmt, 1, SQL_C_CHAR, (SQLPOINTER)sUserName,
               (SQLLEN)ID_SIZEOF(sUserName), &sUserNameInd)
               != SQL_SUCCESS, StmtError);        

    IDE_TEST(Execute(sRoleStmt) != SQL_SUCCESS);
    
    while ((sRet = SQLFetch(sRoleStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE(sRet != SQL_SUCCESS, StmtError);
        
        idlOS::fprintf( aFp, "GRANT %s TO \"%s\";\n", aRoleName, sUserName );
    }
    
    FreeStmt( &sRoleStmt );

    return SQL_SUCCESS;

    IDE_EXCEPTION(DBCError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(StmtError);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sRoleStmt);
    }
    IDE_EXCEPTION_END;

    if ( sRoleStmt != SQL_NULL_HSTMT )
    {
        FreeStmt( &sRoleStmt );
    }

    return SQL_ERROR;
}
