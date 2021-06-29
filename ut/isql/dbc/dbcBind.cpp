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
 * $Id$
 **********************************************************************/

#include <utString.h>
#include <utISPApi.h>
#if !defined(PDL_HAS_WINCE)
#include <errno.h>
#endif

IDE_RC utISPApi::BuildBindInfo(idBool aPrepare, idBool aExecute)
{
    SShort ulCols;
    SShort Col;
    SShort Type;
    SShort Scale;
    SShort Null;
    SQLULEN Precision;
    SInt   i;
    SQLHSTMT  sStmt;
    SQLRETURN sSQLRC = SQL_SUCCESS;
    SChar  sColName[UT_MAX_NAME_BUFFER_SIZE];

    if ( aPrepare == ID_TRUE )
    {
        sStmt = m_TmpStmt3;
    }
    else
    {
        sStmt = m_IStmt;
    }

    IDE_TEST_RAISE(SQLNumResultCols(sStmt, (SQLSMALLINT *)&ulCols)
                   != SQL_SUCCESS, NumResultColError);
    IDE_TEST_RAISE(ulCols == 0, SKIP_SET_COLUMN);

    IDE_TEST_RAISE(m_Result.SetSize(ulCols) != IDE_SUCCESS, MAllocError);

    for (i=0; i<m_Result.GetSize(); i++)
    {
        sSQLRC = SQLDescribeCol(sStmt,
#if (SIZEOF_LONG == 8) && defined(BUILD_REAL_64_BIT_MODE)
                                (SQLUSMALLINT)(i + 1),
#else
                                (SQLSMALLINT)(i + 1),
#endif
                                (SQLCHAR *)sColName,
                                UT_MAX_NAME_BUFFER_SIZE, (SQLSMALLINT *)&Col,
                                (SQLSMALLINT *)&Type,
                                &Precision,
                                (SQLSMALLINT *)&Scale, (SQLSMALLINT *)&Null);
        IDE_TEST_RAISE(sSQLRC != SQL_SUCCESS &&
                       sSQLRC != SQL_SUCCESS_WITH_INFO, BindError);

        IDE_TEST_RAISE( m_Result.AddColumn(i,
                               sColName,
                               Type,
                               Precision,
                               mErrorMgr,
                               aExecute)
                        != IDE_SUCCESS, SetError );

        if (aExecute == ID_TRUE)
        {
            sSQLRC = SQLBindCol(sStmt,
                                (SQLUSMALLINT)(i + 1),
                                m_Result.GetCType(i),
                                (SQLPOINTER)m_Result.GetBuffer(i),
                                (SQLLEN)m_Result.GetBufferSize(i),
                                m_Result.GetInd(i));
            IDE_TEST_RAISE(sSQLRC != SQL_SUCCESS &&
                           sSQLRC != SQL_SUCCESS_WITH_INFO, BindError);
        }
        else
        {
            /* Do nothing */
        }
    }

    IDE_EXCEPTION_CONT(SKIP_SET_COLUMN);

    return IDE_SUCCESS;

    IDE_EXCEPTION(NumResultColError);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sStmt);
    }
    IDE_EXCEPTION(MAllocError);
    {
        uteSetErrorCode(mErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, __LINE__);
    }
    IDE_EXCEPTION(BindError);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)sStmt);
        m_Result.freeMem();
    }
    IDE_EXCEPTION(SetError);
    {
        m_Result.freeMem();
        (void)StmtClose(sStmt);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC utISPApi::GetParamDescriptor()
{
    IDE_TEST_RAISE(SQLGetStmtAttr(m_TmpStmt3,
                                  SQL_ATTR_IMP_PARAM_DESC,
                                  (SQLPOINTER) &mIRD,
                                  0,
                                  NULL)
                   != SQL_SUCCESS, TmpStmt3Error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(TmpStmt3Error);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)m_TmpStmt3);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC utISPApi::GetDescParam(SShort  a_Order,
                              SShort *a_InOutType)
{
    SQLSMALLINT sDataType;
    SQLSMALLINT sDecimalDigits;
    SQLSMALLINT sNullable;
    SQLULEN     sParamSize;

    /* SQLDescribeParam must be called before SQLGetDescField. */
    IDE_TEST_RAISE(SQLDescribeParam(m_TmpStmt3,
                                    a_Order,
                                    &sDataType,
                                    &sParamSize,
                                    &sDecimalDigits,
                                    &sNullable)
                   != SQL_SUCCESS, TmpStmt3Error);

    IDE_TEST_RAISE(SQLGetDescField(mIRD,
                                   (SQLUSMALLINT)a_Order,
                                   SQL_DESC_PARAMETER_TYPE,
                                   (SQLPOINTER) a_InOutType,
                                   SQL_IS_SMALLINT,
                                   0)
                   != SQL_SUCCESS, TmpStmt3Error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(TmpStmt3Error);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)m_TmpStmt3);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC utISPApi::ProcBindPara(SShort a_Order, SShort a_InOutType,
                              SShort a_CType, SShort a_SqlType,
                              SInt a_Precision, void *a_HostVar,
                              SInt a_MaxValue, SQLLEN *a_Len)
{
/*
  printf("a_InOutType=%"ID_INT32_FMT"\n", a_InOutType);
  printf("a_CType=%"ID_INT32_FMT"\n",     a_CType);
  printf("a_SqlType=%"ID_INT32_FMT"\n",   a_SqlType);
  printf("a_Precision=%"ID_INT32_FMT"\n", a_Precision);
  printf("a_MaxValue=%"ID_INT32_FMT"\n",  a_MaxValue);
  printf("a_Len=%"ID_INT32_FMT"\n",       (SInt)(*a_Len));
  printf("a_Order=%"ID_INT32_FMT"\n",     a_Order);
*/
    IDE_TEST_RAISE(SQLBindParameter(m_TmpStmt3, (SQLUSMALLINT)a_Order,
                                    (SQLSMALLINT)a_InOutType,
                                    (SQLSMALLINT)a_CType,
                                    (SQLSMALLINT)a_SqlType,
                                    (SQLULEN)a_Precision, 0,
                                    (SQLPOINTER)a_HostVar,
                                    (SQLLEN)a_MaxValue,
                                    a_Len)
                   != SQL_SUCCESS, TmpStmt3Error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(TmpStmt3Error);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_STMT, (SQLHANDLE)m_TmpStmt3);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * GetAltiDateFmtLen.
 *
 * DBC�� ALTIBASE_DATE_FORMAT ���� ���ڿ��� ����
 * DATE �÷����� ���ڿ��� ��ȯ�Ǿ��� ��,
 * ���ڿ��� ���� �� �ִ� �ִ� ���̸� ���� �����Ѵ�.
 *
 * @param[out] aLen
 *  DATE �÷����� ��ȯ�� ���ڿ��� �ִ� ���̸� �����ϱ� ���� ����.
 */
IDE_RC utISPApi::GetAltiDateFmtLen(SQLULEN *aLen)
{
    SChar sAltiDateFmt[65];

    sAltiDateFmt[0] = '\0';

    /* DBC�� ALTIBASE_DATE_FORMAT�� ��´�. */
    IDE_TEST_RAISE(SQLGetConnectAttr(m_ICon, ALTIBASE_DATE_FORMAT,
                                     (SQLPOINTER)sAltiDateFmt,
                                     (SQLINTEGER)ID_SIZEOF(sAltiDateFmt), NULL)
                   != SQL_SUCCESS, DBCError);

    /* BUGBUG:
     * ODBCCLI���� ALTIBASE_DATE_FORMAT�� ����� ������ �� �Ǿ��־
     * �ӽ÷� ���� ���� �˻�. */
    if (sAltiDateFmt[0] != '\0' &&
        idlOS::strcasecmp(sAltiDateFmt, "(null)") != 0 &&
        idlOS::strcasecmp(sAltiDateFmt, "null") != 0)
    {
        /* ALTIBASE_DATE_FORMAT���κ���
         * ���ڿ��� ��ȯ�� DATE �÷����� �ִ� ���̸� ���Ѵ�. */
        *aLen = (SQLULEN)GetDateFmtLenFromDateFmt(sAltiDateFmt);
    }
    else
    {
        /* BUGBUG:
         * ODBCCLI���� ALTIBASE_DATE_FORMAT�� ����� ������ �� �Ǿ��־
         * �ӽ÷� ���� �ڵ�. */
        *aLen = 19;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(DBCError);
    {
        SetErrorMsgWithHandle(SQL_HANDLE_DBC, (SQLHANDLE)m_ICon);
    }
    IDE_EXCEPTION_END;

    /* DATE �÷��� column size �⺻�� = 19 */
    *aLen = 19;

    return IDE_FAILURE;
}

/**
 * GetDateFmtLenFromDateFmt.
 *
 * DATE �÷����� ���ڿ��� ��ȯ �� ����ϴ� ���� ���ڿ��� ���ڷ� �޾�,
 * DATE �÷����� ��ȯ�� ���ڿ��� ���� �� �ִ� �ִ� ���̸� ���� �����Ѵ�.
 *
 * @param[in] aDateFmt
 *  DATE �÷����� ���ڿ��� ��ȯ �� ����ϴ� ���� ���ڿ�.
 */
UInt utISPApi::GetDateFmtLenFromDateFmt(SChar *aDateFmt)
{
    SChar  sDateFmt[65];
    SChar *sTk;
    UInt   sDateFmtLen;

    (void)idlOS::snprintf(sDateFmt, ID_SIZEOF(sDateFmt), "%s", aDateFmt);

    /* sDateFmtLen�� ���� ���ڿ��� ���̷� �ʱ�ȭ�Ѵ�. */
    sDateFmtLen = (UInt)idlOS::strlen(sDateFmt);

    /* ���� �������� ���̿�
     * DATE �÷����� ���� �����ڿ� ���� ��µ� ���ڿ��� �ִ� ���̰�
     * ���� �ٸ� �� �ִ� ���� �����ڸ� ã��,
     * sDateFmtLen�� �����Ѵ�. */
    sTk = idlOS::strtok(sDateFmt,
                        " \t\r\n\v\f-/,.:'`~!@#$%^&*()_=+\\|[{]};\"<>?");
    while (sTk != NULL)
    {
        if (idlOS::strcasecmp(sTk, "DAY") == 0)
        {
            /* ���ڿ� ���� �ִ� = 9(WEDNESDAY) */
            sDateFmtLen += 6;
        }
        else if (idlOS::strcasecmp(sTk, "DY") == 0)
        {
            /* ���ڿ� ���� = 3 */
            sDateFmtLen += 1;
        }
        else if (idlOS::strncasecmp(sTk, "FF", 2) == 0)
        {
            if (sTk[2] == '\0')
            {
                /* ���ڿ� ���� = 6 */
                sDateFmtLen += 4;
            }
            else if ('1' <= sTk[2] && sTk[2] <= '6' &&
                     sTk[3] == '\0')
            {
                /* ���ڿ� ���� = [1, 6] */
                sDateFmtLen = sDateFmtLen - 3 + (sTk[2] - '0');
            }
        }
        else if (idlOS::strncasecmp(sTk, "HH", 2) == 0)
        {
            if (sTk[2] == '1' && sTk[3] == '2' && sTk[4] == '\0')
            {
                /* ���ڿ� ���� = 2 */
                sDateFmtLen -= 2;
            }
            else if (sTk[2] == '2' && sTk[3] == '4' && sTk[4] == '\0')
            {
                /* ���ڿ� ���� = 2 */
                sDateFmtLen -= 2;
            }
        }
        else if (idlOS::strcasecmp(sTk, "MONTH") == 0)
        {
            /* ���ڿ� ���� �ִ� = 9(SEPTEMBER) */
            sDateFmtLen += 4;
        }
        else if (idlOS::strcasecmp(sTk, "RM") == 0)
        {
            /* ���ڿ� ���� �ִ� = 4(VIII) */
            sDateFmtLen += 2;
        }

        sTk = idlOS::strtok(NULL,
                            " \t\r\n\v\f-/,.:'i`~!@#$%^&*()_=+\\|[{]};\"<>?");
    }

    return sDateFmtLen;
}

