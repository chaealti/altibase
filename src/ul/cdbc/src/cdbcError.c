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
#include <stdarg.h>

/* BUGBUG (2014-11-28) handle�� ��ȿ���� �������� ���� ������ �ֱ����� ������
 * ���� �޽����� �ٲ�� ���⵵ �ٲ��ִ°� ����. */
#define INVALID_HANDLE_ERRCODE  ACI_E_ERROR_CODE(ulERR_ABORT_INVALID_HANDLE)
#define INVALID_HANDLE_ERRMSG   "Invalid Handle."
#define INVALID_HANDLE_SQLSTATE "IH"



/** is same as gUlnErrorFactory */
static aci_client_error_factory_t gCdbcErrorFactory[] =
{
#include "../../uln/E_UL_US7ASCII.c"
};

static aci_client_error_factory_t *gCdbcClientFactory[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    gCdbcErrorFactory,
    NULL,
    NULL,
    NULL,
    NULL /* gUtErrorFactory, */
};

/**
 * ���� ������ �ʱ�ȭ�Ѵ�.
 *
 * @param[in,out] aDiagRec �ʱ�ȭ�� ���� ���� ����ü
 */
CDBC_INTERNAL
void altibase_init_errinfo (cdbcABDiagRec *aDiagRec)
{
    #define CDBC_FUNC_NAME "altibase_init_errinfo"

    CDBCLOG_IN();

    CDBC_DASSERT(aDiagRec != NULL);

    if ((aDiagRec->mErrorCode != 0)
     || (aDiagRec->mErrorMessage[0] != '\0')
     || (0 != acpCStrCmp((acp_char_t *)aDiagRec->mErrorState, "00000",
                         ALTIBASE_MAX_SQLSTATE_LEN)))
    {
        acpMemSet(aDiagRec, 0, ACI_SIZEOF(cdbcABDiagRec));
        (void) acpCStrCpy((acp_char_t *)aDiagRec->mErrorState,
                          ALTIBASE_MAX_SQLSTATE_LEN + 1,
                          "00000", ALTIBASE_MAX_SQLSTATE_LEN);
    }

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ������ �����Ѵ�.
 *
 * @param[in,out] aDiagRec �ʱ�ȭ�� ���� ���� ����ü
 * @param[in] aErrorCode ������ ���� ������ ���� �ڵ�
 * @param[in] aArgs ���� �޽����� �ϼ��ϴµ� ���Ǵ� ��
 */
CDBC_INTERNAL
void altibase_set_errinfo_v (cdbcABDiagRec *aDiagRec, acp_uint32_t aErrorCode, va_list aArgs)
{
    #define CDBC_FUNC_NAME "altibase_set_errinfo_v"

    acp_uint32_t                sSection;
    aci_client_error_factory_t *sCurFactory;

    CDBCLOG_IN();

    CDBC_DASSERT(aDiagRec != NULL);

    sSection = (aErrorCode & ACI_E_MODULE_MASK) >> 28;

    sCurFactory = gCdbcClientFactory[sSection];
    CDBC_DASSERT(sCurFactory != NULL);

    CDBCLOG_CALL("aciSetClientErrorCode");
    aciSetClientErrorCode(aDiagRec, sCurFactory, aErrorCode, aArgs);
    CDBCLOG_BACK("aciSetClientErrorCode");

    /* convert real error code */
    aDiagRec->mErrorCode = ACI_E_ERROR_CODE(aDiagRec->mErrorCode);

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ������ �����Ѵ�.
 *
 * @param[in,out] aDiagRec �ʱ�ȭ�� ���� ���� ����ü
 * @param[in] aErrorCode ������ ���� ������ ���� �ڵ�
 * @param[in] ... ���� �޽����� �ϼ��ϴµ� ���Ǵ� ��
 */
CDBC_INTERNAL
void altibase_set_errinfo (cdbcABDiagRec *aDiagRec, acp_uint32_t aErrorCode, ...)
{
    #define CDBC_FUNC_NAME "altibase_set_errinfo"

    va_list          sArgs;

    CDBCLOG_IN();

    CDBC_DASSERT(aDiagRec != NULL);

    va_start(sArgs, aErrorCode);
    altibase_set_errinfo_v(aDiagRec, aErrorCode, sArgs);
    va_end(sArgs);

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ������ ������ ���� ������ �����Ѵ�.
 *
 * @param[in,out] aDiagRec ���� ������ ���� ����ü
 * @param[in] aHandleType ���� �ڵ� ����
 * @param[in] aHandle ���� �ڵ�
 * @return �����ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
void altibase_set_errinfo_by_sqlhandle (cdbcABDiagRec *aDiagRec,
                                        SQLSMALLINT    aHandleType,
                                        SQLHANDLE      aHandle,
                                        acp_rc_t       aRC)
{
    #define CDBC_FUNC_NAME "altibase_set_errinfo_by_sqlhandle"

    acp_rc_t sRC;

    CDBCLOG_IN();

    CDBC_DASSERT(aDiagRec != NULL);

    CDBC_TEST_RAISE(aRC == ALTIBASE_INVALID_HANDLE, InvalidHandle);

    CDBC_DASSERT(aHandle != NULL);

    CDBCLOG_CALL("SQLGetDiagRec");
    sRC = SQLGetDiagRec(aHandleType, aHandle, 1,
                        (SQLCHAR *) aDiagRec->mErrorState,
                        (SQLINTEGER *) &(aDiagRec->mErrorCode),
                        (SQLCHAR *) aDiagRec->mErrorMessage,
                        ACI_SIZEOF(aDiagRec->mErrorMessage),
                        NULL);
    CDBCLOG_BACK_VAL("SQLGetDiagRec", "%d", sRC);
    CDBCLOG_PRINT_VAL("%s", aDiagRec->mErrorState);
    CDBCLOG_PRINT_VAL("0x%05X", (acp_uint32_t)(aDiagRec->mErrorCode));
    CDBCLOG_PRINT_VAL("%s", aDiagRec->mErrorMessage);
    CDBC_TEST_RAISE(sRC == SQL_INVALID_HANDLE, InvalidHandle);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), FailedToGetDiagRec);

    CDBC_EXCEPTION(InvalidHandle);
    {
        altibase_set_errinfo(aDiagRec, ulERR_ABORT_INVALID_HANDLE);
    }
    CDBC_EXCEPTION(FailedToGetDiagRec);
    {
        altibase_set_errinfo(aDiagRec, ulERR_ABORT_FAIL_OBTAIN_ERROR_INFORMATION);
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ��ȣ�� ��´�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return ���� ��ȣ. ������ ���ų� �����ϸ� 0.
 */
CDBC_EXPORT
acp_uint32_t altibase_errno (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_errno"

    cdbcABConn  *sABConn = (cdbcABConn *) aABConn;

    CDBCLOG_IN();

    CDBC_TEST(HCONN_NOT_VALID(sABConn));

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) (sABConn->mDiagRec).mErrorCode);

    return (sABConn->mDiagRec).mErrorCode;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) INVALID_HANDLE_ERRCODE);

    return INVALID_HANDLE_ERRCODE;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ��ȣ�� ��´�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���� ��ȣ. ������ ���ų� �����ϸ� 0.
 */
CDBC_EXPORT
acp_uint32_t altibase_stmt_errno (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_errno"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;

    CDBCLOG_IN();

    CDBC_TEST(HSTMT_NOT_VALID(sABStmt));

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) (sABStmt->mDiagRec).mErrorCode);

    return (sABStmt->mDiagRec).mErrorCode;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) INVALID_HANDLE_ERRCODE);

    return INVALID_HANDLE_ERRCODE;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� ��ȣ�� ��´�.
 *
 * @param[in] aABRes ��� �ڵ�
 * @return ���� ��ȣ. ������ ���ų� �����ϸ� 0.
 */
CDBC_INTERNAL
acp_uint32_t altibase_result_errno (ALTIBASE_RES aABRes)
{
    #define CDBC_FUNC_NAME "altibase_result_errno"

    cdbcABRes *sABRes = (cdbcABRes *) aABRes;

    CDBCLOG_IN();

    CDBC_TEST(HROM_NOT_VALID(sABRes));

    CDBC_DASSERT(sABRes->mDiagRec != NULL);

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) sABRes->mDiagRec->mErrorCode);

    return (sABRes->mDiagRec)->mErrorCode;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("0x%05X", (acp_uint32_t) INVALID_HANDLE_ERRCODE);

    return INVALID_HANDLE_ERRCODE;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� �޽����� ��´�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return ���� �޽���. ������ ������ �� ���ڿ�, �����ϸ� NULL
 */
CDBC_EXPORT
const acp_char_t * altibase_error (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_error"

    cdbcABConn *sABConn = (cdbcABConn *) aABConn;

    CDBCLOG_IN();

    CDBC_TEST(HCONN_NOT_VALID(sABConn));

    CDBCLOG_OUT_VAL("%s", (sABConn->mDiagRec).mErrorMessage);

    return (const acp_char_t *) (sABConn->mDiagRec).mErrorMessage;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", INVALID_HANDLE_ERRMSG);

    return INVALID_HANDLE_ERRMSG;

    #undef CDBC_FUNC_NAME
}

/**
 * ���� �޽����� ��´�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���� �޽���. ������ ������ �� ���ڿ�, �����ϸ� NULL
 */
CDBC_EXPORT
const acp_char_t * altibase_stmt_error (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_error"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;

    CDBCLOG_IN();

    CDBC_TEST(HSTMT_NOT_VALID(sABStmt));

    CDBCLOG_OUT_VAL("%s", (sABStmt->mDiagRec).mErrorMessage);

    return (const acp_char_t *)((sABStmt->mDiagRec).mErrorMessage);

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", INVALID_HANDLE_ERRMSG);

    return INVALID_HANDLE_ERRMSG;

    #undef CDBC_FUNC_NAME
}

/**
 * SQLSTATE�� ��´�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return SQLSTATE ��. ������ ������ "00000", �����ϸ� NULL.
 */
CDBC_EXPORT
const acp_char_t * altibase_sqlstate (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_sqlstate"

    cdbcABConn *sABConn = (cdbcABConn *) aABConn;

    CDBCLOG_IN();

    CDBC_TEST(HCONN_NOT_VALID(sABConn));

    CDBCLOG_OUT_VAL("%s", (sABConn->mDiagRec).mErrorState);

    return (const acp_char_t *) (sABConn->mDiagRec).mErrorState;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", INVALID_HANDLE_SQLSTATE);

    return INVALID_HANDLE_SQLSTATE;

    #undef CDBC_FUNC_NAME
}

/**
 * SQLSTATE�� ��´�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return SQLSTATE ��. ������ ������ "00000", �����ϸ� NULL.
 */
CDBC_EXPORT
const acp_char_t * altibase_stmt_sqlstate (ALTIBASE_STMT aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_sqlstate"

    cdbcABStmt *sABStmt = (cdbcABStmt *) aABStmt;

    CDBCLOG_IN();

    CDBC_TEST(HSTMT_NOT_VALID(sABStmt));

    CDBCLOG_OUT_VAL("%s", (sABStmt->mDiagRec).mErrorState);

    return (const acp_char_t *)((sABStmt->mDiagRec).mErrorState);

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", INVALID_HANDLE_SQLSTATE);

    return INVALID_HANDLE_SQLSTATE;

    #undef CDBC_FUNC_NAME
}

