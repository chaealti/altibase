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



CDBC_INTERNAL SQLUINTEGER altibase_cli_failover_callback (
    SQLHDBC      aDBC,
    void        *aAppContext,
    SQLUINTEGER  aFailOverEvent
);



/**
 * STF�� ���� �ݹ� �Լ��� ����Ѵ�.
 *
 * @param[in] aABConn       ���� �ڵ�
 * @param[in] aCallbackFunc �ݹ� �Լ�
 * @param[in] aAppContext   �ݹ� �Լ����� ����� ����Ÿ�� ������
 * @return �����ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_set_failover_callback (
    ALTIBASE                         aABConn,
    altibase_failover_callback_func  aCallbackFunc,
    void                            *aAppContext)
{
    #define CDBC_FUNC_NAME "altibase_set_failover_callback"

    cdbcABConn                 *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RC                 sRC;
    SQLFailOverCallbackContext *sFOCC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HCONN_NOT_VALID(sABConn), InvalidHandle);

    altibase_init_errinfo(&(sABConn->mDiagRec));

    if (aCallbackFunc == NULL)
    {
        sFOCC = NULL;
    }
    else
    {
        sABConn->mFOAppContext   = aAppContext;
        sABConn->mFOCallbackFunc = aCallbackFunc;

        sFOCC = &(sABConn->mFOCallbackContext);
        sFOCC->mDBC                  = sABConn->mHdbc;
        sFOCC->mAppContext           = sABConn;
        sFOCC->mFailOverCallbackFunc = altibase_cli_failover_callback;
    }
    CDBCLOG_PRINT_VAL("%p", sFOCC);

    CDBCLOG_CALL("SQLSetConnectAttr");
    sRC = SQLSetConnectAttr(sABConn->mHdbc, ALTIBASE_FAILOVER_CALLBACK, sFOCC, 0);
    CDBCLOG_BACK_VAL("SQLSetConnectAttr", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), DBCError);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(DBCError);
    {
        altibase_set_errinfo_by_conndbc(sABConn, sRC);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * CLI�� ����ϱ� ���� �ݹ� �Լ�.
 * �ܼ��� C/DBC�� ��ϵ� �ݹ� �Լ��� ȣ�����ִ� ���Ҹ� ����Ѵ�.
 */
CDBC_INTERNAL
SQLUINTEGER altibase_cli_failover_callback (
    SQLHDBC      aDBC,
    void        *aAppContext,
    SQLUINTEGER  aFailOverEvent)
{
    #define CDBC_FUNC_NAME "altibase_cli_failover_callback"

    cdbcABConn              *sABConn = (cdbcABConn *) aAppContext;
    ALTIBASE_FAILOVER_EVENT  sRE;

    CDBCLOG_IN();

    CDBC_DASSERT(aDBC == sABConn->mHdbc);
    CDBC_DASSERT(sABConn->mFOCallbackFunc);

    sRE = sABConn->mFOCallbackFunc(sABConn,
                                   sABConn->mFOAppContext,
                                   (ALTIBASE_FAILOVER_EVENT) aFailOverEvent);

    CDBCLOG_OUT_VAL("%d", (acp_sint32_t) sRE);

    return (SQLUINTEGER) sRE;

    #undef CDBC_FUNC_NAME
}

/**
 * failover�� �������� �� prepare, bind�� �ٽ� ���ش�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_stmt_failover_postproc (cdbcABStmt *aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_failover_postproc"

    cdbcABDiagRec  sDiagRec;
    ALTIBASE_BIND *sBindParam;
    ALTIBASE_BIND *sBindResult;
    int            sArrayBindSize;
    int            sArrayFetchSize;
    acp_rc_t       sRC;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    /* prepare �� �� reset�ϹǷ� ��� �صξ�� �Ѵ�. */
    sBindParam = aABStmt->mBindParam;
    sBindResult = aABStmt->mBindResult;
    sArrayBindSize = aABStmt->mArrayBindSize;
    sArrayFetchSize = aABStmt->mArrayFetchSize;

    /* �������� ���� ���� ��� */
    acpMemCpy(&sDiagRec, &(aABStmt->mDiagRec), ACI_SIZEOF(cdbcABDiagRec));

    (void) altibase_stmt_free_result(aABStmt);

    if (STMT_IS_PREPARED(aABStmt))
    {
        sRC = altibase_stmt_prepare(aABStmt, aABStmt->mQstr);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    if (sArrayBindSize > 1)
    {
        sRC = altibase_stmt_set_array_bind(aABStmt, sArrayBindSize);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    if (sBindParam != NULL)
    {
        sRC = altibase_stmt_bind_param(aABStmt, sBindParam);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    if (sArrayFetchSize > 1)
    {
        sRC = altibase_stmt_set_array_fetch(aABStmt, sArrayFetchSize);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    if (sBindResult != NULL)
    {
        sRC = altibase_stmt_bind_result(aABStmt, sBindResult);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
    }

    /* ���� ���� �޽���(failover success) ���� */
    acpMemCpy(&(aABStmt->mDiagRec), &sDiagRec, ACI_SIZEOF(cdbcABDiagRec));

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

