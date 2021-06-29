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
 * $Id: utmSeqSym.cpp $
 **********************************************************************/

#include <smDef.h>
#include <utm.h>
#include <utmExtern.h>
#include <utmMisc.h>

#define GET_SYNONYM_USER_QUERY                                      \
    "SELECT a.synonym_name, a.object_owner_name, a.object_name"     \
    " FROM system_.sys_synonyms_ a, system_.sys_users_ b"           \
    " WHERE a.synonym_owner_id = b.user_id AND b.user_name = ?"     \
    " AND a.synonym_name != 'DBMS_METADATA'"

#define GET_SYNONYM_DETAIL_QUERY                                      \
    "SELECT/*+ NO_PLAN_CACHE */ b.user_name, "                        \
    " a.synonym_name, a.object_owner_name, "                          \
    " a.object_name"                                                  \
    " FROM system_.sys_synonyms_ a left outer join "                  \
    " system_.sys_users_ b"                                           \
    " on a.synonym_owner_id = b.user_id"                              \
    " WHERE a.synonym_name != 'DBMS_METADATA'"

#define GET_SEQUENCE_QUERY                                          \
    "SELECT /*+ USE_HASH(B, C) NO_PLAN_CACHE */"                    \
    "       a.user_name USER_NAME, "                                \
    "       b.table_name SEQUENCE_NAME "                            \
    "  from system_.sys_users_ a, "                                 \
    "       system_.sys_tables_ b "                                 \
    " where a.user_id=b.user_id  "                                  \
    "   and a.user_name<>'SYSTEM_' "                                \
    "   and b.table_type='S' "

#define GET_DIRECTORY_QUERY                                         \
    "SELECT/*+ NO_PLAN_CACHE */ directory_name, directory_path"     \
    " FROM SYSTEM_.SYS_DIRECTORIES_" 
    
/* BUG-36367 aexport must consider new objects, 'package' and 'library' */
#define GET_LIBRARY_QUERY                                            \
    "SELECT /*+ NO_PLAN_CACHE */ users.user_name, lib.library_name, "\
           "lib.file_spec "                                          \
    "FROM system_.sys_libraries_ lib, system_.sys_users_ users "     \
    "WHERE lib.user_id = users.user_id "                             \

/* PROJ-1438 Job Scheduler */
#define GET_JOB_QUERY                                                \
    "SELECT JOB_NAME "                                               \
    "FROM system_.sys_jobs_ "                                        \
    "ORDER BY 1"

SQLRETURN getSynonymUser( SChar *aUserName,
                          FILE  *aSynFp)
{
#define IDE_FN "getSynonymUser()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SQLHSTMT  sSynonymStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;

    SChar     sSynonymName[UTM_NAME_LEN+1];
    SChar     sSchemaName[UTM_NAME_LEN+1];
    SChar     sObjectName[UTM_NAME_LEN+1];
    SChar     s_query[QUERY_LEN / 2];
    SQLLEN    sSynonymNameInd;
    SQLLEN    sSchemaNameInd;
    SQLLEN    sObjectNameInd;
    SChar     sDdl[QUERY_LEN] = { '\0', };

    idlOS::fprintf(stdout, "\n##### SYNONYM #####\n");

    IDE_TEST_RAISE(SQLAllocStmt(m_hdbc, &sSynonymStmt) != SQL_SUCCESS,
                   alloc_error);

    idlOS::snprintf(s_query, ID_SIZEOF(s_query),
                    GET_SYNONYM_USER_QUERY);
    
    IDE_TEST(Prepare(s_query, sSynonymStmt) != SQL_SUCCESS);

    IDE_TEST_RAISE(
    SQLBindParameter(sSynonymStmt, 1, SQL_PARAM_INPUT,
                     SQL_C_CHAR, SQL_VARCHAR, UTM_NAME_LEN, 0,
                     aUserName, UTM_NAME_LEN+1, NULL)
                     != SQL_SUCCESS, user_error);

    IDE_TEST_RAISE(
    SQLBindCol(sSynonymStmt, 1, SQL_C_CHAR,
               (SQLPOINTER)sSynonymName, (SQLLEN)ID_SIZEOF(sSynonymName),
               &sSynonymNameInd)
               != SQL_SUCCESS, user_error );
 
    IDE_TEST_RAISE(    
    SQLBindCol(sSynonymStmt, 2, SQL_C_CHAR,
               (SQLPOINTER)sSchemaName, (SQLLEN)ID_SIZEOF(sSchemaName),
               &sSchemaNameInd)
               != SQL_SUCCESS, user_error );
    
    IDE_TEST_RAISE(    
    SQLBindCol(sSynonymStmt, 3, SQL_C_CHAR,
               (SQLPOINTER)sObjectName, (SQLLEN)ID_SIZEOF(sObjectName),
               &sObjectNameInd)
               != SQL_SUCCESS, user_error );    

    IDE_TEST(Execute(sSynonymStmt) != SQL_SUCCESS);

    while ((sRet = SQLFetch(sSynonymStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, user_error );
        idlOS::snprintf( sDdl,
                         ID_SIZEOF(sDdl),
                         "create synonym \"%s\".\"%s\" for \"%s\".\"%s\";\n",
                         aUserName,
                         sSynonymName,
                         sSchemaName,
                         sObjectName );

        idlOS::fprintf( aSynFp, "--############################\n");
        idlOS::fprintf( aSynFp, "-- SYNONYM \"%s\"\n", sSynonymName);
        idlOS::fprintf( aSynFp, "--############################\n");

        if ( gProgOption.mbExistDrop == ID_TRUE )
        {
            // BUG-20943 drop �������� user �� ��õ��� �ʾ� drop �� �����մϴ�.
            idlOS::fprintf( aSynFp, "drop Synonym \"%s\".\"%s\";\n", aUserName, sSynonymName);
        }
#ifdef DEBUG
        idlOS::fprintf( stderr, "%s\n", sDdl );
#endif
        idlOS::fprintf( aSynFp, "%s\n\n", sDdl );
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &sSynonymStmt );

    idlOS::fflush( aSynFp );

    return SQL_SUCCESS;

    IDE_EXCEPTION(alloc_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(user_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sSynonymStmt);
    }
    IDE_EXCEPTION_END;

    idlOS::fflush( aSynFp );

    if ( sSynonymStmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &sSynonymStmt );        
    }

    return SQL_ERROR;
#undef IDE_FN
}

SQLRETURN getSynonymAll( FILE *aSynFp )
{
#define IDE_FN "getSynonymAll()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));
    SQLHSTMT  sSynonymStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;
    SChar     sUserName[UTM_NAME_LEN+1];
    SChar     sSynonymName[UTM_NAME_LEN+1];
    SChar     sSchemaName[UTM_NAME_LEN+1];
    SChar     sObjectName[UTM_NAME_LEN+1];
    SChar     s_query[QUERY_LEN / 2];
    SQLLEN    sUserNameInd      = 0;
    SQLLEN    sSynonymNameInd   = 0;
    SQLLEN    sSchemaNameInd    = 0;
    SQLLEN    sObjectNameInd    = 0;
    SChar     sDdl[QUERY_LEN]   = { '\0', };

    idlOS::fprintf( stdout, "\n##### SYNONYM #####\n" );

    IDE_TEST_RAISE( SQLAllocStmt(m_hdbc, &sSynonymStmt ) != SQL_SUCCESS,
                    alloc_error);

    idlOS::snprintf( s_query, ID_SIZEOF(s_query), GET_SYNONYM_DETAIL_QUERY);
    
    IDE_TEST_RAISE( SQLExecDirect(sSynonymStmt, (SQLCHAR *)s_query, 
                    SQL_NTS) != SQL_SUCCESS, user_error );
    IDE_TEST_RAISE(
        SQLBindCol( sSynonymStmt, 1, SQL_C_CHAR,
                    (SQLPOINTER)sUserName, (SQLLEN)ID_SIZEOF(sUserName),
                    &sUserNameInd) != SQL_SUCCESS, user_error );
    IDE_TEST_RAISE(    
        SQLBindCol(sSynonymStmt, 2, SQL_C_CHAR,
                   (SQLPOINTER)sSynonymName, (SQLLEN)ID_SIZEOF(sSynonymName),
                   &sSynonymNameInd) != SQL_SUCCESS, user_error );
    IDE_TEST_RAISE(    
        SQLBindCol(sSynonymStmt, 3, SQL_C_CHAR,
                   (SQLPOINTER)sSchemaName, (SQLLEN)ID_SIZEOF(sSchemaName),
                   &sSchemaNameInd) != SQL_SUCCESS, user_error );
    IDE_TEST_RAISE(    
        SQLBindCol(sSynonymStmt, 4, SQL_C_CHAR,
                   (SQLPOINTER)sObjectName, (SQLLEN)ID_SIZEOF(sObjectName),
                   &sObjectNameInd) != SQL_SUCCESS, user_error );
    
    while ((sRet = SQLFetch(sSynonymStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, user_error );

        // BUG-25194 DROP �ɼ��� OFF �϶� synonym �� ������� ����
        if (sUserNameInd != SQL_NULL_DATA)
        {
            idlOS::snprintf( sDdl,
                             ID_SIZEOF(sDdl),
                             "create synonym \"%s\".\"%s\" for \"%s\".\"%s\";\n",
                             sUserName,
                             sSynonymName,
                             sSchemaName,
                             sObjectName );

            idlOS::fprintf( aSynFp, "--############################\n" );
            idlOS::fprintf( aSynFp, "-- SYNONYM \"%s\"\n", sSynonymName );
            idlOS::fprintf( aSynFp, "--############################\n" );

            if ( gProgOption.mbExistDrop == ID_TRUE )
            {
                // BUG-20943 drop �������� user �� ��õ��� �ʾ� drop �� �����մϴ�.
                idlOS::fprintf( aSynFp, "drop Synonym \"%s\".\"%s\";\n", 
                                sSchemaName, sSynonymName );
            }

#ifdef DEBUG
            idlOS::fprintf( stderr, "%s\n", sDdl );
#endif
            idlOS::fprintf( aSynFp, "%s\n\n", sDdl );
        }
        else
        { // to skip public synonym for system_ user.
            if ( idlOS::strncmp( sSchemaName, "SYSTEM_", 7 ) != 0 )
            {
                idlOS::snprintf( sDdl,
                                 ID_SIZEOF(sDdl),
                                 "create public synonym \"%s\" for \"%s\".\"%s\";\n",
                                 sSynonymName,
                                 sSchemaName,
                                 sObjectName );

                idlOS::fprintf( aSynFp, "--############################\n" );
                idlOS::fprintf( aSynFp, "-- SYNONYM \"%s\"\n", sSynonymName );
                idlOS::fprintf( aSynFp, "--############################\n" );

                if ( gProgOption.mbExistDrop == ID_TRUE )
                {
                    idlOS::fprintf( aSynFp, "drop public Synonym \"%s\";\n",
                                    sSynonymName );
                }

#ifdef DEBUG
                idlOS::fprintf( stderr, "%s\n", sDdl );
#endif
                idlOS::fprintf( aSynFp, "%s\n\n", sDdl );
            }
        }
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &sSynonymStmt );

    idlOS::fflush( aSynFp );

    return SQL_SUCCESS;

    IDE_EXCEPTION(alloc_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(user_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sSynonymStmt);
    }
    IDE_EXCEPTION_END;

    idlOS::fflush( aSynFp );

    if ( sSynonymStmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &sSynonymStmt );
    }

    return SQL_ERROR;
#undef IDE_FN
}

SQLRETURN getSeqQuery( SChar *a_user,
                       SChar *aObjName,
                       FILE  *aSeqFp )
{
#define IDE_FN "getSeqQuery()"
    SQLHSTMT s_seq_stmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;

    SInt     i = 0;
    SInt     s_pos = 0;
    SChar    s_query[QUERY_LEN];

    SChar    s_user_name[UTM_NAME_LEN+1];
    SChar    s_puser_name[UTM_NAME_LEN+1];
    SChar    s_seq_name[UTM_NAME_LEN+1];
    SChar    s_passwd[STR_LEN];

    s_puser_name[0] = '\0';
    idlOS::fprintf( stdout, "\n##### SEQUENCE #####\n" );

    IDE_TEST_RAISE( SQLAllocStmt( m_hdbc, &s_seq_stmt ) 
                    != SQL_SUCCESS, alloc_error);

    s_pos = idlOS::sprintf(s_query, GET_SEQUENCE_QUERY );

    if ( gProgOption.m_bExist_OBJECT != ID_TRUE )
    {
        if ( idlOS::strcasecmp( a_user, "SYS" ) != 0 )
        {
            s_pos += idlOS::sprintf( s_query + s_pos,
                                     "and a.user_name='%s' ",
                                     a_user );
        }
    }
    else
    {
        s_pos += idlOS::sprintf( s_query + s_pos,
                                 "and a.user_name='%s' "
                                 "and b.table_name='%s' ",
                                 a_user,aObjName );
    }

    idlOS::sprintf( s_query + s_pos, "order by 1,2" );

    IDE_TEST_RAISE( SQLExecDirect( s_seq_stmt, (SQLCHAR *)s_query, SQL_NTS )
                   != SQL_SUCCESS, seq_error );
    IDE_TEST_RAISE(
        SQLBindCol( s_seq_stmt, 1, SQL_C_CHAR, (SQLPOINTER)s_user_name,
                    (SQLLEN)ID_SIZEOF(s_user_name), NULL )
        != SQL_SUCCESS, seq_error );
    IDE_TEST_RAISE(
        SQLBindCol( s_seq_stmt, 2, SQL_C_CHAR, (SQLPOINTER)s_seq_name,
                    (SQLLEN)ID_SIZEOF(s_seq_name), NULL )
        != SQL_SUCCESS, seq_error );

    while ( ( sRet = SQLFetch(s_seq_stmt)) != SQL_NO_DATA )
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, seq_error );

        if ( gProgOption.m_bExist_OBJECT != ID_TRUE )
        {
            if (i == 0 || idlOS::strcmp(s_user_name, s_puser_name) != 0)
            {
                IDE_TEST(getPasswd(s_user_name, s_passwd) != SQL_SUCCESS);

                idlOS::fprintf(aSeqFp, "connect \"%s\" / \"%s\"\n",s_user_name, s_passwd);
            }
        }

        i++;

        if ( gProgOption.mbExistDrop == ID_TRUE )
        {
            // BUG-20943 drop �������� user �� ��õ��� �ʾ� drop �� �����մϴ�.
            idlOS::fprintf( aSeqFp, "drop sequence \"%s\".\"%s\";\n", 
                            s_user_name, s_seq_name);
        }

        /* BUG-47159 Using DBMS_METADATA package in aexport */
        IDE_TEST(gMeta->getDdl(DDL, UTM_OBJ_TYPE_SEQUENCE, s_seq_name, s_user_name)
                 != IDE_SUCCESS);
#ifdef DEBUG
        idlOS::fprintf( stderr, "%s\n", gMeta->getDdlStr() );
#endif
        idlOS::fprintf( aSeqFp, "%s\n\n", gMeta->getDdlStr() );

        IDE_TEST( getObjPrivQuery( aSeqFp, UTM_SEQUENCE, 
                                   s_user_name, s_seq_name )
                                   != SQL_SUCCESS );

        idlOS::fflush( aSeqFp );
        
        idlOS::strcpy( s_puser_name, s_user_name );
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &s_seq_stmt );

    return SQL_SUCCESS;

    IDE_EXCEPTION(alloc_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(seq_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)s_seq_stmt);
    }
    IDE_EXCEPTION_END;

    if ( s_seq_stmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &s_seq_stmt );
    }

    return SQL_ERROR;
#undef IDE_FN
}

/* ���� ���� �ϳ�? */
/* BUG-22708
 *
 * Directory ��ü�� ������..
 */
SQLRETURN getDirectoryAll( FILE *aDirFp )
{
    SQLHSTMT  sDirectoryStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;
    SChar     s_query[QUERY_LEN / 2];
    SChar     sDirectoryName[UTM_NAME_LEN+1];
    SChar     sDirectoryPath[4000+1];
    SQLLEN    sDirectoryNameInd;
    SQLLEN    sDirectoryPathInd;
    SChar     *sUserName = NULL;
    SChar     sDdl[QUERY_LEN] = { '\0', };
   
    sUserName = (SChar *) UTM_STR_SYS;

    idlOS::fprintf(stdout, "\n##### DIRECTORY #####\n");

    IDE_TEST_RAISE( SQLAllocStmt( m_hdbc, &sDirectoryStmt )
                                  != SQL_SUCCESS, alloc_error);

    idlOS::snprintf( s_query, ID_SIZEOF(s_query),
                     GET_DIRECTORY_QUERY);

    IDE_TEST_RAISE(SQLExecDirect(sDirectoryStmt, (SQLCHAR *)s_query, SQL_NTS)
                   != SQL_SUCCESS, user_error);

    IDE_TEST_RAISE(
        SQLBindCol(sDirectoryStmt, 1, SQL_C_CHAR,
                   (SQLPOINTER)sDirectoryName,
                   (SQLLEN)ID_SIZEOF(sDirectoryName), &sDirectoryNameInd)
        != SQL_SUCCESS, user_error); 
    IDE_TEST_RAISE(
        SQLBindCol(sDirectoryStmt, 2, SQL_C_CHAR,
                   (SQLPOINTER)sDirectoryPath,
                   (SQLLEN)ID_SIZEOF(sDirectoryPath), &sDirectoryPathInd)
        != SQL_SUCCESS, user_error);    

    while ((sRet = SQLFetch(sDirectoryStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, user_error );
        idlOS::snprintf( sDdl,
                         ID_SIZEOF(sDdl),
                         "CREATE DIRECTORY \"%s\" as '%s';\n",
                         sDirectoryName,
                         sDirectoryPath );

        if ( gProgOption.mbExistDrop == ID_TRUE )
        {
            idlOS::fprintf( aDirFp, "drop directory \"%s\";\n",
                            sDirectoryName );
        }

#ifdef DEBUG
        idlOS::fprintf( stderr, "%s\n", sDdl );
#endif
        idlOS::fprintf( aDirFp, "%s\n", sDdl );

        IDE_TEST( getObjPrivQuery( aDirFp, UTM_DIRECTORY, 
                                   sUserName, sDirectoryName )
                                   != SQL_SUCCESS );
    }

    // BUG-33995 aexport have wrong free handle code
    FreeStmt( &sDirectoryStmt );

    idlOS::fflush( aDirFp );

    return SQL_SUCCESS;

    IDE_EXCEPTION(alloc_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION(user_error);
    {
        utmSetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sDirectoryStmt);
    }
    IDE_EXCEPTION_END;

    idlOS::fflush( aDirFp );

    if ( sDirectoryStmt != SQL_NULL_HSTMT )
    {
        // BUG-33995 aexport have wrong free handle code
        FreeStmt( &sDirectoryStmt );
    }

    return SQL_ERROR;
}

/* BUG-36367 aexport must consider new objects, 'package' and 'library' */
SQLRETURN getLibQuery( FILE *aLibFp,
                       SChar *aUserName,
                       SChar *aObjName)
#define IDE_FN "getLibQuery()"
{
    SQLHSTMT sLibStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;

    SInt i    = 0;
    SInt sPos = 0;

    SChar    sQuery[QUERY_LEN];
    SChar    sPassWord[STR_LEN];

    SChar    sUserName[UTM_NAME_LEN+1];
    SChar    sLibName[UTM_NAME_LEN+1];
    SChar    sFilePath[256];

    SChar    sPostUserName[UTM_NAME_LEN+1];

    sFilePath[0] = '\0';
    sPostUserName[0] = '\0';

    idlOS::fprintf( stdout, "\n##### LIBRARY #####\n");

    IDE_TEST_RAISE( SQLAllocStmt( m_hdbc, &sLibStmt) != SQL_SUCCESS,
                    alloc_error);

    sPos = idlOS::snprintf( sQuery, ID_SIZEOF(sQuery), GET_LIBRARY_QUERY);
    
    if ( gProgOption.m_bExist_OBJECT != ID_TRUE)
    {
        if ( idlOS::strcasecmp( aUserName, "SYS") != 0)
        {
            sPos += idlOS::sprintf( sQuery + sPos,
                                     "AND users.user_name = '%s' ",
                                     aUserName );
        }
    }
    else
    {
        sPos += idlOS::sprintf( sQuery + sPos,
                                 "AND users.user_name = '%s' "
                                 "AND lib.library_name = '%s' ",
                                 aUserName, aObjName);
    }
    
    idlOS::sprintf( sQuery + sPos, "ORDER BY 1, 2");

    IDE_TEST( Prepare( sQuery, sLibStmt) != SQL_SUCCESS);

    IDE_TEST_RAISE(
        SQLBindCol( sLibStmt,
                    1,
                    SQL_C_CHAR,
                    (SQLPOINTER) sUserName,
                    (SQLLEN) ID_SIZEOF(sUserName), 
                    NULL) != SQL_SUCCESS, lib_error);
    
    IDE_TEST_RAISE(
        SQLBindCol( sLibStmt,
                    2,
                    SQL_C_CHAR,
                    (SQLPOINTER) sLibName,
                    (SQLLEN) ID_SIZEOF(sLibName),
                    NULL) != SQL_SUCCESS, lib_error);

    IDE_TEST_RAISE(
        SQLBindCol( sLibStmt,
                    3,
                    SQL_C_CHAR,
                    (SQLPOINTER) sFilePath,
                    (SQLLEN) ID_SIZEOF(sFilePath),
                    NULL) != SQL_SUCCESS, lib_error);
    
    IDE_TEST(Execute(sLibStmt) != SQL_SUCCESS );

    while ( ( sRet = SQLFetch(sLibStmt)) != SQL_NO_DATA)
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, lib_error);

        if ( gProgOption.m_bExist_OBJECT != ID_TRUE)
        {
            if (i == 0 || idlOS::strcmp(sUserName, sPostUserName) != 0)
            {
                IDE_TEST(getPasswd(sUserName, sPassWord) != SQL_SUCCESS);

                idlOS::fprintf(aLibFp, "connect \"%s\" / \"%s\"\n",sUserName, sPassWord);
            }
        }
        
        if ( gProgOption.mbExistDrop == ID_TRUE)
        {
            idlOS::fprintf( aLibFp, "drop library \"%s\".\"%s\";\n", sUserName, sLibName);
        }

        idlOS::fprintf( aLibFp, "create library \"%s\".\"%s\" as '%s';\n\n", sUserName, sLibName, sFilePath);
   

        IDE_TEST( getObjPrivQuery( aLibFp, UTM_LIBRARY, sUserName, sLibName)
              != SQL_SUCCESS);

        i++;
        idlOS::strcpy( sPostUserName, sUserName);
    }

    idlOS::fflush( aLibFp);
    FreeStmt( &sLibStmt);

    return SQL_SUCCESS;

    IDE_EXCEPTION( alloc_error);
    {
        utmSetErrorMsgWithHandle( SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc);
    }
    IDE_EXCEPTION( lib_error);
    {
        utmSetErrorMsgWithHandle( SQL_HANDLE_STMT, (SQLHANDLE)sLibStmt);
    }
    IDE_EXCEPTION_END;
    
    idlOS::fflush( aLibFp);

    if ( sLibStmt != SQL_NULL_HSTMT)
    {
        FreeStmt( &sLibStmt);
    }

    return SQL_ERROR;
#undef IDE_FN
}

/* PROJ-1438 Job Scheduler */
SQLRETURN getJobQuery( FILE * aJobFp )
{
    SQLHSTMT sJobStmt = SQL_NULL_HSTMT;
    SQLRETURN sRet;

    SInt i    = 0;

    SChar    sQuery[QUERY_LEN];
    SChar    sJobName[UTM_NAME_LEN+1];
    SChar    sPasswd[STR_LEN];
    SQLLEN   sJobNameInd = 0;

    idlOS::fprintf( stdout, "\n##### JOB #####\n" );

    IDE_TEST_RAISE( SQLAllocStmt( m_hdbc, &sJobStmt ) != SQL_SUCCESS,
                    alloc_error);

    idlOS::snprintf( sQuery, ID_SIZEOF(sQuery), GET_JOB_QUERY );

    IDE_TEST( Prepare( sQuery, sJobStmt) != SQL_SUCCESS );

    IDE_TEST_RAISE(
        SQLBindCol( sJobStmt,
                    1,
                    SQL_C_CHAR,
                    (SQLPOINTER) sJobName,
                    (SQLLEN) ID_SIZEOF(sJobName),
                    &sJobNameInd) != SQL_SUCCESS, job_error );

    IDE_TEST( Execute(sJobStmt) != SQL_SUCCESS );

    while ( ( sRet = SQLFetch(sJobStmt)) != SQL_NO_DATA )
    {
        IDE_TEST_RAISE( sRet != SQL_SUCCESS, job_error );

        if ( ( gProgOption.m_bExist_OBJECT != ID_TRUE ) && ( i == 0 ) )
        {
            IDE_TEST(getPasswd((SChar *)"SYS", sPasswd) != SQL_SUCCESS);

            idlOS::fprintf( aJobFp, "connect \"%s\" / \"%s\"\n", "SYS", sPasswd );
        }
        else
        {
            /* Nothing to do */
        }

        if ( gProgOption.mbExistDrop == ID_TRUE )
        {
            idlOS::fprintf( aJobFp, "drop job \"%s\";\n", sJobName );
        }
        else
        {
            /* Nothing to do */
        }

        /* BUG-47159 Using DBMS_METADATA package in aexport */
        IDE_TEST(gMeta->getDdl(DDL, UTM_OBJ_TYPE_JOB, sJobName, NULL)
                 != IDE_SUCCESS);

        idlOS::fprintf( aJobFp, "%s\n", gMeta->getDdlStr() );

        i++;
    }

    idlOS::fflush( aJobFp );
    FreeStmt( &sJobStmt );

    return SQL_SUCCESS;

    IDE_EXCEPTION( alloc_error );
    {
        ( void )utmSetErrorMsgWithHandle( SQL_HANDLE_DBC, (SQLHANDLE)m_hdbc );
    }
    IDE_EXCEPTION( job_error);
    {
        ( void )utmSetErrorMsgWithHandle( SQL_HANDLE_STMT, (SQLHANDLE)sJobStmt );
    }
    IDE_EXCEPTION_END;

    idlOS::fflush( aJobFp );

    if ( sJobStmt != SQL_NULL_HSTMT )
    {
        FreeStmt( &sJobStmt );
    }
    else
    {
        /* Nothing to do */
    }

    return SQL_ERROR;
}


