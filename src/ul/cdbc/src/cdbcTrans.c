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
 * Ʈ������� �����Ѵ�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @param[in] aCompletionType Ʈ����� ���� ����. SQL_COMMIT �Ǵ� SQL_ROLLBACK
 * @return �����ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INLINE
ALTIBASE_RC altibase_end_trans (ALTIBASE aABConn, SQLSMALLINT aCompletionType)
{
    #define CDBC_FUNC_NAME "altibase_end_trans"

    cdbcABConn *sABConn = (cdbcABConn *) aABConn;
    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HCONN_NOT_VALID(sABConn), InvalidHandle);

    altibase_init_errinfo(&(sABConn->mDiagRec));

    CDBCLOG_CALL("SQLEndTran");
    sRC = SQLEndTran(SQL_HANDLE_DBC, sABConn->mHdbc, aCompletionType);
    CDBCLOG_BACK_VAL("SQLEndTran", "%d", sRC);
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
 * Ʈ������� commit�Ѵ�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return �����ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_commit (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_commit"

    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    sRC = altibase_end_trans(aABConn, SQL_COMMIT);

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * Ʈ������� rollback�Ѵ�.
 *
 * @param[in] aABConn ���� �ڵ�
 * @return �����ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_rollback (ALTIBASE aABConn)
{
    #define CDBC_FUNC_NAME "altibase_rollback"

    ALTIBASE_RC sRC;

    CDBCLOG_IN();

    sRC = altibase_end_trans(aABConn, SQL_ROLLBACK);

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

