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

#include <cdbc.h>



/**
 * �÷� ����� ��´�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @param[in] aRestrictions ���� ����
 * @return �÷� ����� ���� ��� �ڵ�
 */
CDBC_EXPORT
ALTIBASE_RES altibase_list_fields (ALTIBASE aABConn, const acp_char_t *aRestrictions[])
{
    #define CDBC_FUNC_NAME "altibase_list_fields"

    cdbcABConn   *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RES  sRes;
    acp_rc_t      sRC;

    CDBCLOG_IN();

    CDBC_TEST(HCONN_NOT_VALID(sABConn));

    altibase_init_errinfo(&(sABConn->mDiagRec));

    CDBC_TEST_RAISE(aRestrictions == NULL, InvalidNullPtr);

    sRC = altibase_ensure_hstmt(sABConn);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    CDBCLOG_CALL("SQLColumns");
    sRC = SQLColumns(sABConn->mHstmt,
                     /* db     name */ NULL, 0,
                     /* user   name */ (SQLCHAR *) aRestrictions[0], SQL_NTS,
                     /* table  name */ (SQLCHAR *) aRestrictions[1], SQL_NTS,
                     /* column name */ (SQLCHAR *) aRestrictions[2], SQL_NTS);
    CDBCLOG_BACK_VAL("SQLColumns", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CONN_SET_EXECUTED(sABConn);
    sABConn->mQueryType = ALTIBASE_QUERY_SELECT;

    sRes = altibase_use_result(aABConn);
    CDBC_TEST(sRes == NULL);

    CDBCLOG_OUT_VAL("%p", sRes);

    return sRes;

    CDBC_EXCEPTION(InvalidNullPtr);
    {
        altibase_set_errinfo(&(sABConn->mDiagRec),
                             ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_connstmt(sABConn, sRC);
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "{null}");

    return NULL;

    #undef CDBC_FUNC_NAME
}

/**
 * ���̺� ����� ��´�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @param[in] aRestrictions ���� ����
 * @return ���̺� ����� ���� ��� �ڵ�
 */
CDBC_EXPORT
ALTIBASE_RES altibase_list_tables (ALTIBASE aABConn, const acp_char_t *aRestrictions[])
{
    #define CDBC_FUNC_NAME "altibase_list_tables"

    cdbcABConn   *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RES  sRes;
    acp_rc_t      sRC;

    CDBCLOG_IN();

    CDBC_TEST(HCONN_NOT_VALID(sABConn));

    altibase_init_errinfo(&(sABConn->mDiagRec));

    CDBC_TEST_RAISE(aRestrictions == NULL, InvalidNullPtr);

    sRC = altibase_ensure_hstmt(sABConn);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    CDBCLOG_CALL("SQLTables");
    sRC = SQLTables(sABConn->mHstmt,
                    /* db    name */ NULL, 0,
                    /* user  name */ (SQLCHAR *) aRestrictions[0], SQL_NTS,
                    /* table name */ (SQLCHAR *) aRestrictions[1], SQL_NTS,
                    /* table type */ (SQLCHAR *) aRestrictions[2], SQL_NTS);
    CDBCLOG_BACK_VAL("SQLTables", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CONN_SET_EXECUTED(sABConn);
    sABConn->mQueryType = ALTIBASE_QUERY_SELECT;

    sRes = altibase_use_result(aABConn);
    CDBCLOG_PRINT_VAL("%p", sRes);
    CDBC_TEST(sRes == NULL);

    CDBCLOG_OUT_VAL("%p", sRes);

    return sRes;

    CDBC_EXCEPTION(InvalidNullPtr);
    {
        altibase_set_errinfo(&(sABConn->mDiagRec),
                             ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_connstmt(sABConn, sRC);
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "{null}");

    return NULL;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ��������� Ŀ���� �̵��Ѵ�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return ALTIBASE_SUCCESS: �����߰� ���� ������� �����Դ�.
 *         ALTIBASE_NO_DATA: ���������� �� ������ ������� ����.
 *         ALTIBASE_ERROR  : ����
 */
CDBC_EXPORT
ALTIBASE_RC altibase_next_result (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_stmt_next_result"

    cdbcABConn *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HCONN_NOT_VALID(sABConn), InvalidHandle);

    altibase_init_errinfo(&(sABConn->mDiagRec));

    /* free result�� �ϸ� executed ���¸� unset�ϹǷ�
       ������� �������϶��� ���� ������ �߾����� ���߾����� Ȯ���� �� ����.
       free result �� executed ���¸� unset�ϴ� ������
       �׷��� ���ϸ� �ѹ� execute �� �Ŀ��� ��� execute ���·� �����ְ� �Ǳ� ���� */
    /*CDBC_TEST_RAISE(CONN_NOT_EXECUTED(sABConn), FuncSeqError);*/
    /* �� �Լ��� ȣ���ϱ� ���� �ݵ�� free result�� �ؾ��Ѵ�. */
    CDBC_TEST_RAISE(CONN_IS_FETCHED(sABConn), FuncSeqError);

    CDBCLOG_CALL("SQLMoreResults");
    sRC = SQLMoreResults(sABConn->mHstmt);
    CDBCLOG_BACK_VAL("SQLMoreResults", "%d", sRC);
    CDBC_TEST_RAISE((sRC != SQL_NO_DATA) && CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CONN_SET_EXECUTED(sABConn);
    CONN_UNSET_FETCHED(sABConn);

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(FuncSeqError);
    {
        altibase_set_errinfo(&(sABConn->mDiagRec),
                             ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_connstmt(sABConn, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ��������� Ŀ���� �̵��Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ALTIBASE_SUCCESS: �����߰� ���� ������� �����Դ�.
 *         ALTIBASE_NO_DATA: ���������� �� ������ ������� ����.
 *         ALTIBASE_ERROR  : ����
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_next_result (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_next_result"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    /*CDBC_TEST_RAISE(STMT_NOT_EXECUTED(sABStmt), FuncSeqError);*/
    CDBC_TEST_RAISE(STMT_IS_FETCHED(sABStmt), FuncSeqError);

    CDBCLOG_CALL("SQLMoreResults");
    sRC = SQLMoreResults(sABStmt->mHstmt);
    CDBCLOG_BACK_VAL("SQLMoreResults", "%d", sRC);
    CDBC_TEST_RAISE((sRC != SQL_NO_DATA) && CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    STMT_SET_EXECUTED(sABStmt);
    STMT_UNSET_FETCHED(sABStmt);

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(FuncSeqError);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(sABStmt, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * ������ �����Ѵ�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @param[in] aQstr ������
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_query (ALTIBASE aABConn, const acp_char_t *aQstr)
{
    #define CDBC_FUNC_NAME "altibase_query"

    cdbcABConn *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HCONN_NOT_VALID(sABConn), InvalidHandle);

    altibase_init_errinfo(&(sABConn->mDiagRec));

    CDBC_TEST_RAISE(aQstr == NULL, InvalidNullPtr);

    sRC = altibase_ensure_hstmt(sABConn);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    CDBCLOG_CALL("SQLExecDirect");
    sRC = SQLExecDirect(sABConn->mHstmt, (SQLCHAR *) aQstr, SQL_NTS);
    CDBCLOG_BACK_VAL("SQLExecDirect", "%d", sRC);
    CDBC_TEST_RAISE((sRC != SQL_NO_DATA) && CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    sABConn->mQueryType = altibase_query_type(aQstr);
    CDBCLOG_PRINT_VAL("%d", sABConn->mQueryType);

    CONN_SET_EXECUTED(sABConn);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(InvalidNullPtr);
    {
        altibase_set_errinfo(&(sABConn->mDiagRec),
                             ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_connstmt(sABConn, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * SQL���� prepare �Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aQstr prepare�� SQL��
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_prepare (ALTIBASE_STMT aABStmt, const acp_char_t *aQstr)
{
    #define CDBC_FUNC_NAME "altibase_stmt_prepare"

    cdbcABStmt     *sABStmt = (cdbcABStmt *) aABStmt;
    cdbcABRes      *sRes = NULL;
    acp_sint32_t    sQstrLen;
    acp_bool_t      sIsNeedQstrCopy = ACP_FALSE;
    ALTIBASE_RC     sRC;
    ALTIBASE_RC     sTmpRC;

    SAFE_FAILOVER_RETRY_INIT();

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    CDBC_TEST_RAISE(aQstr == NULL, InvalidNullPtr);

    sRC = altibase_stmt_reset(aABStmt);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    Retry:

    sQstrLen = acpCStrLen(aQstr, ACP_SINT32_MAX);
    CDBCLOG_PRINT_VAL("%d", sQstrLen);

    CDBCLOG_CALL("SQLPrepare");
    sRC = SQLPrepare(sABStmt->mHstmt, (SQLCHAR *) aQstr, sQstrLen);
    CDBCLOG_BACK_VAL("SQLPrepare", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    /* if ���� ���� ������ ���� ��, ������� Ȯ���Ѵٴ� ������ �����Ƿ� �̷��� ó�� */
    if (sABStmt->mQstr == NULL)
    {
        sIsNeedQstrCopy = ACP_TRUE;
    }
    else if (acpCStrCmp(sABStmt->mQstr, aQstr, sQstrLen) != 0)
    {
        sIsNeedQstrCopy = ACP_TRUE;
    }
    if (sIsNeedQstrCopy == ACP_TRUE)
    {
        if (sABStmt->mQstrMaxLen < (sQstrLen + CDBC_NULLTERM_SIZE))
        {
            SAFE_FREE_AND_CLEAN(sABStmt->mQstr);

            CDBCLOG_CALL("acpMemAlloc");
            sRC = acpMemAlloc((void **)&(sABStmt->mQstr),
                              sQstrLen + CDBC_NULLTERM_SIZE);
            CDBCLOG_BACK_VAL("acpMemAlloc", "%d", sRC);
            CDBC_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC), MAllocError);

            sABStmt->mQstrMaxLen = sQstrLen + CDBC_NULLTERM_SIZE;
        }

        sRC = acpCStrCpy(sABStmt->mQstr, sABStmt->mQstrMaxLen, aQstr, sQstrLen);
        CDBC_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC), MAllocError);
    }

    /* BUGBUG: prepare �� �������� ��ȯ�� ���� ������ Ȯ���ؼ� �����ϸ� ����. */
    sABStmt->mQueryType = altibase_query_type(aQstr);

    CDBCLOG_CALL("SQLNumParams");
    sRC = SQLNumParams(sABStmt->mHstmt, &(sABStmt->mParamCount));
    CDBCLOG_BACK_VAL("SQLNumParams", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
    CDBCLOG_PRINT_VAL("%d", sABStmt->mParamCount);

    STMT_SET_PREPARED(sABStmt);

    if (sABStmt->mRes == NULL)
    {
        sRes = altibase_stmt_result_init(sABStmt, ALTIBASE_HANDLE_RES);
        CDBC_TEST(sRes == NULL);
        sABStmt->mRes = sRes;
    }

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(InvalidNullPtr);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(sABStmt, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(MAllocError);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_FATAL_MEMORY_ALLOC_ERROR,
                             CDBC_FUNC_NAME);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        if (sRes != NULL)
        {
            sTmpRC = altibase_free_result(sRes);
            CDBC_ASSERT( ALTIBASE_SUCCEEDED(sTmpRC) );
            sABStmt->mRes = NULL;
        }

        SAFE_FAILOVER_POST_PROC_AND_RETRY(sABStmt, Retry);

        STMT_UNSET_PREPARED(sABStmt);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * prepare�� �� ���·� �ǵ�����.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_reset (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_reset"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    CDBC_TEST_RAISE(STMT_IS_RESRETURNED(sABStmt), FuncSeqError);

    CDBCLOG_CALL("SQLFreeStmt : SQL_UNBIND");
    sRC = SQLFreeStmt(sABStmt->mHstmt, SQL_UNBIND);
    CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CDBCLOG_CALL("SQLFreeStmt : SQL_RESET_PARAMS");
    sRC = SQLFreeStmt(sABStmt->mHstmt, SQL_RESET_PARAMS);
    CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    /* ���� ���ο� ������� ���ε� ������ �ʱ�ȭ �Ѵ�.
       failover�� �߻��ϴ��� �ٽ� ���ε� ���� �ʵ��� �ϱ� ����. */
    sABStmt->mBindParam = NULL;
    sABStmt->mBindResult = NULL;
    altibase_stmt_parambind_free(sABStmt);

    /* prepare �� ���� ���� ��� �Ѿ */
    if (sABStmt->mRes != NULL)
    {
        altibase_result_bind_free(sABStmt->mRes);

        sRC = altibase_stmt_set_array_bind(aABStmt, 1);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

        sRC = altibase_stmt_set_array_fetch(aABStmt, 1);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    STMT_UNSET_FETCHED(sABStmt);
    STMT_UNSET_EXECUTED(sABStmt);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(FuncSeqError);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(sABStmt, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        SAFE_FAILOVER_POST_PROC(sABStmt);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * �غ�� ������ �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_execute (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_execute"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;
    ALTIBASE_RC sRC;

    SAFE_FAILOVER_RETRY_INIT();

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    /* ��������� free result�� ������ �ʾƵ�, �ٽ� execute �� ���� �˾Ƽ� ���� */
    if (STMT_IS_EXECUTED(sABStmt) && QUERY_IS_SELECTABLE(sABStmt->mQueryType))
    {
        sRC = altibase_stmt_free_result(sABStmt);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    sRC = altibase_stmt_parambind_core(sABStmt, sABStmt->mBindParam);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    Retry:

    CDBCLOG_CALL("SQLExecute");
    sRC = SQLExecute(sABStmt->mHstmt);
    CDBCLOG_BACK_VAL("SQLExecute", "%d", sRC);

    /* BUGBUG (2014-11-28) altibase_stmt_send_long_data()�� �����Ѵٸ� �����Ѵ�. */
    /* BUG-38527 [ux-cdbc] cdbc is not output error message when the SQL_NEED_DATA error */
    CDBC_TEST_RAISE(sRC == SQL_NEED_DATA, INDICATORError);

    CDBC_TEST_RAISE((sRC != SQL_NO_DATA) && CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
    CDBCLOG_PRINT_VAL("%d", *(sABStmt->mRes->mState));

    STMT_SET_EXECUTED(sABStmt);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    /* BUG-38527 [ux-cdbc] cdbc is not output error message when the SQL_NEED_DATA error */
    CDBC_EXCEPTION(INDICATORError);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec), ulERR_ABORT_INVALID_INDICATOR);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(sABStmt, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        SAFE_FAILOVER_POST_PROC_AND_RETRY(sABStmt, Retry);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * ��뷮 ����Ÿ�� �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aParamNum �Ķ���� ���� (0���� ����)
 * @param[in] aValue ��
 * @param[in] aValueLength �� ����
 * @return ���������� ��Ÿ ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_send_long_data (ALTIBASE_STMT  aABStmt,
                                          acp_sint32_t   aParamNum,
                                          void          *aValue,
                                          ALTIBASE_LONG  aValueLength)
{
    #define CDBC_FUNC_NAME "altibase_stmt_send_long_data"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    /* BUGBUG: (CLI) Ư�� �Ķ���� ������ �����ؼ� ����Ÿ�� ���� �� ���� */
    ACP_UNUSED(aParamNum);
    ACP_UNUSED(aValue);
    ACP_UNUSED(aValueLength);
    CDBC_RAISE(NotSupported);

    /* TODO */
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(NotSupported);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_ABORT_UNSUPPORTED_FUNCTION);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(sABStmt, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        SAFE_FAILOVER_POST_PROC(sABStmt);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

