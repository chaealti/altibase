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
 * ����Ÿ ũ�Ⱑ �������ִ� ���ε� Ÿ���� ���� ũ�⸦ ��´�.
 *
 * @param[in] aBind ���� ũ�⸦ ���ϴµ� ����� ���ε� ����
 * @return ���� ũ�� (>= 0),
 *         ����Ÿ ũ�Ⱑ ���������� ���� Ÿ���̸� CDBC_EXPNO_VARSIZE_TYPE,
 *         ��ȿ���� ���� Ÿ���̸� CDBC_EXPNO_INVALID_BIND_TYPE
 */
CDBC_INTERNAL
acp_sint32_t altibase_bind_max_typesize (ALTIBASE_BIND_TYPE aBindType)
{
    #define CDBC_FUNC_NAME "altibase_bind_max_typesize"

    acp_sint32_t sBufSize;

    CDBCLOG_IN();

    switch (aBindType)
    {
        case ALTIBASE_BIND_BINARY:
        case ALTIBASE_BIND_STRING:
        case ALTIBASE_BIND_WSTRING:
            sBufSize = CDBC_EXPNO_VARSIZE_TYPE;
            break;

        case ALTIBASE_BIND_SMALLINT:
            sBufSize = ACI_SIZEOF(acp_sint8_t);
            break;
        case ALTIBASE_BIND_INTEGER:
            sBufSize = ACI_SIZEOF(acp_sint32_t);
            break;
        case ALTIBASE_BIND_BIGINT:
            sBufSize = ACI_SIZEOF(acp_sint64_t);
            break;
        case ALTIBASE_BIND_REAL:
            sBufSize = ACI_SIZEOF(acp_float_t);
            break;
        case ALTIBASE_BIND_DOUBLE:
            sBufSize = ACI_SIZEOF(acp_double_t);
            break;
        case ALTIBASE_BIND_NUMERIC:
            sBufSize = ACI_SIZEOF(ALTIBASE_NUMERIC);
            break;
        case ALTIBASE_BIND_DATE:
            sBufSize = ACI_SIZEOF(ALTIBASE_TIMESTAMP);
            break;

        default:
            CDBC_RAISE(InvalidType);
            break;
    }

    CDBCLOG_OUT_VAL("%d", sBufSize);

    return sBufSize;

    CDBC_EXCEPTION(InvalidType);
    {
        /* ���� �������̽����� ó�� */
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ERROR");

    return CDBC_EXPNO_INVALID_BIND_TYPE;

    #undef CDBC_FUNC_NAME
}

/**
 * �ʵ� ������ ����, ���̳ʸ� �Ǵ� ���ڿ��� ����� ���� �ִ� ���� ũ�⸦ ��´�.
 *
 * �ʵ� Ÿ���� IS_BIN_TYPE()�̸� ���̳ʸ�,
 * �׷��� ������ ���ڿ��� ����� ���� �ִ� ���� ũ�⸦ ��ȯ�Ѵ�.
 *
 * LOB�� LOCATOR�� �̿��ϹǷ� LOCATOR ũ�⸦ ��´�.
 *
 * printf() ��� ������ �Ͼ ���� �����Ƿ� alignment�� �Ѵ�.
 *
 * @param[in] aFieldInfo ���� ũ�⸦ ���ϴµ� ����� �ʵ� ����
 * @return ���� ũ��. ��ȿ���� ���� �ʵ� Ÿ���̸� CDBC_EXPNO_INVALID_FIELD_TYPE
 */
CDBC_INTERNAL
acp_sint32_t altibase_bind_max_sbinsize (ALTIBASE_FIELD *aFieldInfo)
{
    #define CDBC_FUNC_NAME "altibase_bind_max_sbinsize"

    acp_sint32_t sBufSize;

    CDBCLOG_IN();

    CDBC_DASSERT(aFieldInfo != NULL);

    switch (aFieldInfo->type)
    {
        case ALTIBASE_TYPE_CHAR:
        case ALTIBASE_TYPE_VARCHAR:
            sBufSize = aFieldInfo->size * 2 + CDBC_NULLTERM_SIZE;
            break;

        case ALTIBASE_TYPE_NCHAR:
        case ALTIBASE_TYPE_NVARCHAR:
            sBufSize = aFieldInfo->size * 4 + CDBC_NULLTERM_SIZE;
            break;

        case ALTIBASE_TYPE_NUMERIC:
            /* ��ȣ(-) 1����, �Ҽ��� 1����,
               ������ ����(E, e) 1����, ������ ��ȣ(+, -) 1����, ������ ���� �ִ� 2����,
               ��� ���ϸ� 168���ں��� 6���ڱ��� �þ �� ����.
               176����Ʈ�� �ǳ� �˳��ϰ� �Ҵ���. */
            sBufSize = 191 + CDBC_NULLTERM_SIZE;
            break;
        case ALTIBASE_TYPE_FLOAT:
            /* �Ҽ��� 1����,
               ������ ����(E, e) 1����, ������ ��ȣ(+, -) 1����, ������ ���� �ִ� 3����,
               ��� ���ϸ� 168���ں��� 6���ڱ��� �þ �� ����.
               176����Ʈ�� �ǳ� �˳��ϰ� �Ҵ���. */
            sBufSize = 191 + CDBC_NULLTERM_SIZE;
            break;

        /* �ִ� �ڸ��� + ��ȣ + NULL-Term */
        case ALTIBASE_TYPE_DOUBLE:
        case ALTIBASE_TYPE_REAL:
            sBufSize = 383 + CDBC_NULLTERM_SIZE;
            break;
        case ALTIBASE_TYPE_BIGINT:
            sBufSize = 20 + CDBC_NULLTERM_SIZE;
            break;
        case ALTIBASE_TYPE_INTEGER:
            sBufSize = 11 + CDBC_NULLTERM_SIZE;
            break;
        case ALTIBASE_TYPE_SMALLINT:
            sBufSize = 6 + CDBC_NULLTERM_SIZE;
            break;

        case ALTIBASE_TYPE_DATE:
            /* DATE_FORMAT�� �ִ� 64�����̰�
               FF �����������ڸ� ����ϸ� ���̰� 3����� �þ �� ���� */
            sBufSize = (64 * 3) + CDBC_NULLTERM_SIZE;
            break;

        /* binary */
        case ALTIBASE_TYPE_BYTE:
            sBufSize = 32000 + 2;
            break;
        case ALTIBASE_TYPE_NIBBLE:
            sBufSize = (254 / 2) + 1;
            break;
        case ALTIBASE_TYPE_BIT:
            sBufSize = (60576 / 8) + 4;
            break;
        case ALTIBASE_TYPE_VARBIT:
            sBufSize = (131068 / 8) + 4;
            break;
        case ALTIBASE_TYPE_GEOMETRY:
            sBufSize = aFieldInfo->size + 56;
            break;

        /* lob locator */
        case ALTIBASE_TYPE_BLOB:
        case ALTIBASE_TYPE_CLOB:
            sBufSize = ACI_SIZEOF(ALTIBASE_LOBLOCATOR);
            break;

        default:
            CDBC_RAISE(InvalidType);
            break;
    }

    CDBC_ADJUST_ALIGN(sBufSize);

    CDBCLOG_OUT_VAL("%d", sBufSize);

    return sBufSize;

    CDBC_EXCEPTION(InvalidType);
    {
        /* ���� �������̽����� ó�� */
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ERROR");

    return CDBC_EXPNO_INVALID_FIELD_TYPE;

    #undef CDBC_FUNC_NAME
}

/**
 * �ʵ� ������ ���ε� ������ ���� �ִ� ���� ũ�⸦ ��´�.
 *
 * LOB�� LOCATOR�� �̿��ϹǷ� LOCATOR ũ�⸦ ��´�.
 * printf() ��� ������ �Ͼ ���� �����Ƿ� alignment�� �Ѵ�.
 *
 * @param[in] aFieldInfo ���� ũ�⸦ ���ϴµ� ����� �ʵ� ����
 * @param[in] aBaseBindInfo ���� ũ�⸦ ���ϴµ� ����� ���ε� ����.
 * @return ���� ũ��.
 *         ��ȿ���� ���� �ʵ� Ÿ���̸� CDBC_EXPNO_INVALID_FIELD_TYPE
 *         ��ȿ���� ���� ���ε� Ÿ���̸� CDBC_EXPNO_INVALID_BIND_TYPE
 */
CDBC_INTERNAL
acp_sint32_t altibase_bind_max_bufsize (ALTIBASE_FIELD *aFieldInfo, ALTIBASE_BIND *aBaseBindInfo)
{
    #define CDBC_FUNC_NAME "altibase_bind_max_bufsize"

    acp_sint32_t sBufSize;

    CDBCLOG_IN();

    CDBC_DASSERT(aFieldInfo != NULL);

    if (aBaseBindInfo == NULL)
    {
        sBufSize = altibase_bind_max_sbinsize(aFieldInfo);
        CDBC_TEST_RAISE(sBufSize == CDBC_EXPNO_INVALID_FIELD_TYPE, InvalidType);
    }
    else
    {
        if (IS_VAR_BIND(aBaseBindInfo->buffer_type))
        {
            sBufSize = altibase_bind_max_sbinsize(aFieldInfo);
            CDBC_TEST_RAISE(sBufSize == CDBC_EXPNO_INVALID_FIELD_TYPE, InvalidType);
            CDBC_ADJUST_ALIGN(sBufSize);
        }
        else
        {
            sBufSize = altibase_bind_max_typesize(aBaseBindInfo->buffer_type);
            CDBC_DASSERT(sBufSize != CDBC_EXPNO_VARSIZE_TYPE);
            CDBC_TEST_RAISE(sBufSize == CDBC_EXPNO_INVALID_BIND_TYPE, InvalidType);
        }
    }

    CDBC_EXCEPTION_CONT(InvalidType);

    CDBCLOG_OUT_VAL("%d", sBufSize);

    return sBufSize;

    #undef CDBC_FUNC_NAME
}

/**
 * ���ε带 ���� ������ �Ҵ��Ѵ�.
 *
 * altibase_query(), altibase_fetch_row() �Լ��� ���ؼ� ���ȴ�.
 *
 * @param[in] aABRes ��� �ڵ�
 * @param[in] aBaseBindInfo ���� ũ�⸦ ���ϴµ� ����� ���ε� ����.
 * @param[in] aBufAlloc ����Ÿ�� ���� ���� ������ �Ҵ����� ����.
 *                      CDBC_ALLOC_BUF_ON : ���� ������ �Ҵ�
 *                      CDBC_ALLOC_BUF_OFF: ���� ������ �����ϰ� length, is_null�� �Ҵ�
 *                                          ����ڰ� ������ ���ε� ������ �״�� �̿��ϹǷ�
 *                                          aBaseBindInfo�� �ݵ�� NULL�� �ƴϾ�� �Ѵ�.
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_result_bind_init (cdbcABRes *aABRes, ALTIBASE_BIND *aBaseBindInfo, CDBC_ALLOC_BUF aBufAlloc)
{
    #define CDBC_FUNC_NAME "altibase_result_bind_init"

    cdbcBufferItm  *sBufItm;
    acp_char_t     *sBufPtr     = NULL;
    ALTIBASE_BIND  *sBindResult = NULL;
    ALTIBASE_BIND  *sBaseBind;
    acp_sint32_t    sBufSize;
    acp_sint32_t    sLen;
    acp_rc_t        sRC;
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_DASSERT(HRES_IS_VALID(aABRes));
    CDBC_DASSERT(aABRes->mBindResult == NULL);
    CDBC_ASSERT((aBufAlloc == CDBC_ALLOC_BUF_ON) || (aBaseBindInfo != NULL));
    CDBCLOG_PRINT_VAL("%d", aABRes->mArrayFetchSize);
    CDBC_DASSERT(aABRes->mArrayFetchSize >= 1);

    sRC = altibase_ensure_basic_fieldinfos(aABRes);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    /* ��ü ũ�� ���ϱ� */
    sLen = aABRes->mFieldCount * ACI_SIZEOF(ALTIBASE_BIND);
    for (i = 0; i < aABRes->mFieldCount; i++)
    {
        /* for buffer  */
        if (aBufAlloc == CDBC_ALLOC_BUF_ON)
        {
            if (aBaseBindInfo != NULL)
            {
                sBaseBind = &(aBaseBindInfo[i]);
                /* ALTIBASE_BIND_NULL�� �Ķ���� ���� Ÿ�� */
                CDBC_TEST_RAISE(sBaseBind->buffer_type == ALTIBASE_BIND_NULL,
                                InvalidBindType);
            }
            else
            {
                sBaseBind = NULL;
            }

            sBufSize = altibase_bind_max_bufsize(&(aABRes->mFieldInfos[i]),
                                                 sBaseBind);
            CDBC_TEST_RAISE(sBufSize == CDBC_EXPNO_INVALID_BIND_TYPE,
                            InvalidBindType);
            CDBC_TEST_RAISE(sBufSize == CDBC_EXPNO_INVALID_FIELD_TYPE,
                            InvalidFieldType);
        }
        else
        {
            sBufSize = 0;
        }

        /* for length  */
        sBufSize += ACI_SIZEOF(ALTIBASE_LONG);

        /* is_null�� fetch�� �� length ���� �̿��� �����ϹǷ� alloc ���� �ʴ´�. */

        sBufSize *= aABRes->mArrayFetchSize;

        sLen += sBufSize;
    }
    CDBCLOG_PRINT_VAL("%d", sLen);

    /* �޸� �Ҵ� */
    sBufItm = altibase_new_buffer(&(aABRes->mBindBuffer), sLen,
                                  CDBC_BUFFER_TAIL);
    CDBC_TEST_RAISE(sBufItm == NULL, MAllocError);
    sBufPtr = sBufItm->mBuffer;

    sBindResult = (ALTIBASE_BIND *) sBufPtr;
    sBufPtr += aABRes->mFieldCount * ACI_SIZEOF(ALTIBASE_BIND);

    /* buffer, length ������ ����. */
    for (i = 0; i < aABRes->mFieldCount; i++)
    {
        CDBC_DASSERT(sBufPtr < (sBufItm->mBuffer + sBufItm->mBufferLength));

        if (aBufAlloc == CDBC_ALLOC_BUF_ON)
        {
            sBaseBind = (aBaseBindInfo == NULL) ? NULL : &(aBaseBindInfo[i]);
            sBufSize = altibase_bind_max_bufsize(&(aABRes->mFieldInfos[i]),
                                                 sBaseBind);
            CDBC_DASSERT(sBufSize != CDBC_EXPNO_INVALID_BIND_TYPE);
            CDBC_DASSERT(sBufSize != CDBC_EXPNO_INVALID_FIELD_TYPE);
        }
        else
        {
            sBufSize = 0;
        }

        if (aBaseBindInfo != NULL)
        {
            sBindResult[i].buffer_type = aBaseBindInfo[i].buffer_type;
        }
        else if (IS_BIN_TYPE(aABRes->mFieldInfos[i].type))
        {
            sBindResult[i].buffer_type = ALTIBASE_BIND_BINARY;
        }
        else /* if (not bin type) */
        {
            /* altibase_fetch_row()���� �������̽��ϰ� �Բ� ���̹Ƿ�
               ����Ÿ�� ������ ���ڿ��� �޴´�. */
            sBindResult[i].buffer_type = ALTIBASE_BIND_STRING;
        }

        if (aBufAlloc == CDBC_ALLOC_BUF_ON)
        {
            sBindResult[i].buffer        = sBufPtr;
            sBindResult[i].buffer_length = sBufSize;
        }
        else
        {
            /* ����ڰ� ������ ���ε� ���� ����. */
            sBindResult[i].buffer = aBaseBindInfo[i].buffer;
            if (aBaseBindInfo[i].buffer_length != 0)
            {
                sBindResult[i].buffer_length = aBaseBindInfo[i].buffer_length;
            }
            else /* if (aBaseBindInfo[i].buffer_length == 0) */
            {
                /* ũ�Ⱑ �������ִ� ���ε� Ÿ���� buffer_size�� 0�̸�
                   �ڵ����� ������ ���� �����Ѵ�.
                   ũ�Ⱑ ���������� ���� ���ε� Ÿ���� buffer_size��
                   0�̶� ��ȿ�� ���� ������ ������ �����Ѵ�. */
                sLen = altibase_bind_max_typesize(sBindResult[i].buffer_type);
                CDBCLOG_PRINT_VAL("%d", sLen);
                if (sLen > 0)
                {
                    sBindResult[i].buffer_length = sLen;
                }
            }
        }

        sBindResult[i].length = (ALTIBASE_LONG *)(sBufPtr
                              + (sBufSize * aABRes->mArrayFetchSize));

        CDBCLOG_PRINT_VAL("%d", i);
        CDBCLOG_PRINT_VAL("%d", sBufSize);
        CDBCLOG_PRINT_VAL("%p", sBindResult[i].buffer);
        CDBCLOG_PRINT_VAL("%d", (acp_sint32_t)(sBindResult[i].buffer_length));
        CDBCLOG_PRINT_VAL("%p", sBindResult[i].length);

        sBufPtr += (sBufSize + ACI_SIZEOF(ALTIBASE_LONG))
                   * aABRes->mArrayFetchSize;
    }

    aABRes->mBindResult = sBindResult;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidBindType);
    {
        altibase_set_errinfo(aABRes->mDiagRec,
                             ulERR_ABORT_INVALID_USE_OF_BIND_TYPE,
                             sBaseBind->buffer_type);
    }
    CDBC_EXCEPTION(InvalidFieldType);
    {
        /* must unreachable */
        altibase_set_errinfo(aABRes->mDiagRec,
                             ulERR_ABORT_INVALID_USE_OF_FIELD_TYPE,
                             aABRes->mFieldInfos[i].type);
    }
    CDBC_EXCEPTION(MAllocError);
    {
        altibase_set_errinfo(aABRes->mDiagRec,
                             ulERR_FATAL_MEMORY_ALLOC_ERROR,
                             CDBC_FUNC_NAME);
    }
    CDBC_EXCEPTION_END;

    SAFE_FREE(sBindResult);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

/**
 * ���ε带 ���� �Ҵ��� ������ �����Ѵ�.
 *
 * altibase_query(), altibase_fetch_row() �Լ��� ���ؼ� ���ȴ�.
 *
 * @param[in] aABRes ��� �ڵ�
 */
CDBC_INTERNAL
void altibase_result_bind_free (cdbcABRes *aABRes)
{
    #define CDBC_FUNC_NAME "altibase_result_bind_free"

    CDBCLOG_IN();

    CDBC_DASSERT(HRES_IS_VALID(aABRes));

    if (aABRes->mBindResult != NULL)
    {
        /* ���� �޸𸮴� altibase_new_buffer()�� ������Ƿ� ������ �ʱ�ȭ */
        aABRes->mBindResult = NULL;

        altibase_clean_buffer(&(aABRes->mBindBuffer));
    }

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * ���ε带 ���� ������ �����ϰ� ���ε� �Ѵ�.
 *
 * @param[in] aABRes ��� �ڵ�
 * @param[in] aUseLocator LOB�� LOCATOR�� ���ε� ���� ����.
 *                        CDBC_USE_LOCATOR_ON : LOB�� LOCATOR�� ���ε�.
 *                        CDBC_USE_LOCATOR_OFF: ������ ���ε� Ÿ������ ���ε�.
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_result_bind_proc (cdbcABRes *aABRes, CDBC_USE_LOCATOR aUseLocator)
{
    #define CDBC_FUNC_NAME "altibase_result_bind_proc"

    SQLSMALLINT     sTargetCType;
    acp_rc_t        sRC;
    acp_bool_t      sErrorExist;
#if defined(USE_CDBCLOG)
    SQLLEN          sArraySize4CLI;
#endif
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_DASSERT(HRES_IS_VALID(aABRes));
    CDBC_DASSERT(aABRes->mFieldInfos != NULL);

    CDBCLOG_CALL("SQLFreeStmt : SQL_UNBIND");
    sRC = SQLFreeStmt(aABRes->mHstmt, SQL_UNBIND);
    CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

#if defined(USE_CDBCLOG)
    CDBCLOG_CALL("SQLGetStmtAttr");
    sRC = SQLGetStmtAttr(aABRes->mHstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                         (SQLPOINTER) &sArraySize4CLI, ACI_SIZEOF(SQLLEN),
                         NULL);
    CDBCLOG_BACK_VAL("SQLGetStmtAttr", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
    CDBCLOG_PRINT_VAL("%ld", (acp_ulong_t) sArraySize4CLI);
#endif

    sErrorExist = ACP_FALSE;
    for (i = 0; i < aABRes->mFieldCount; i++)
    {
        CDBCLOG_PRINT_VAL("%d", i);
        CDBCLOG_PRINT_VAL("%p", aABRes->mBindResult);
        CDBCLOG_PRINT_VAL("%p", &aABRes->mBindResult[i]);
        if (aUseLocator == CDBC_USE_LOCATOR_ON)
        {
            /* LOB�� LOCATOR�� �̿��� ���;� �ϹǷ� sTargetCType�� LOCATOR�� ���� */
            if (aABRes->mFieldInfos[i].type == ALTIBASE_TYPE_BLOB)
            {
                sTargetCType = SQL_BLOB_LOCATOR;

                /* ���ο����� ȣ��ǹǷ� assert�� Ȯ�� */
                CDBC_DASSERT((acp_uint32_t)aABRes->mBindResult[i].buffer_length
                            >= ACI_SIZEOF(ALTIBASE_LOBLOCATOR));
            }
            else if (aABRes->mFieldInfos[i].type == ALTIBASE_TYPE_CLOB)
            {
                sTargetCType = SQL_CLOB_LOCATOR;

                /* ���ο����� ȣ��ǹǷ� assert�� Ȯ�� */
                CDBC_DASSERT((acp_uint32_t)aABRes->mBindResult[i].buffer_length
                            >= ACI_SIZEOF(ALTIBASE_LOBLOCATOR));
            }
            else
            {
                sTargetCType = aABRes->mBindResult[i].buffer_type;
            }
        }
        else
        {
            sTargetCType = aABRes->mBindResult[i].buffer_type;
        }

        CDBCLOG_PRINT_VAL("%d", sTargetCType);
        CDBCLOG_PRINT_VAL("%d", aABRes->mBindResult[i].buffer_type);
        CDBCLOG_PRINT_VAL("%p", aABRes->mBindResult[i].buffer);
        CDBCLOG_PRINT_VAL("%d", (acp_sint32_t)(aABRes->mBindResult[i].buffer_length));
        CDBCLOG_PRINT_VAL("%p", aABRes->mBindResult[i].length);
        CDBCLOG_CALL("SQLBindCol");
        sRC = SQLBindCol(aABRes->mHstmt,
                         i+1,
                         sTargetCType,
                         aABRes->mBindResult[i].buffer,
                         aABRes->mBindResult[i].buffer_length,
                         aABRes->mBindResult[i].length);
        CDBCLOG_BACK_VAL("SQLBindCol", "%d", sRC);
        if (CDBC_CLI_NOT_SUCCEEDED(sRC))
        {
            altibase_set_errinfo_by_res(aABRes, sRC);
            aABRes->mBindResult[i].error = (acp_sint32_t)(aABRes->mDiagRec->mErrorCode);
            sErrorExist = ACP_TRUE;
        }
        else
        {
            aABRes->mBindResult[i].error = 0;
        }
    }
    CDBC_TEST(sErrorExist == ACP_TRUE);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(STMTError)
    {
        altibase_set_errinfo_by_res(aABRes, sRC);
    }
    CDBC_EXCEPTION_END;

    if (HRES_IS_VALID(aABRes))
    {
        CDBCLOG_CALL("SQLFreeStmt : SQL_UNBIND");
        sRC = SQLFreeStmt(aABRes->mHstmt, SQL_UNBIND);
        CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sRC);
        CDBC_ASSERT( CDBC_CLI_SUCCEEDED(sRC) );
    }

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

/**
 * LOB�� LOCATOR�� �ٽ� ���ε��Ѵ�.
 *
 * @param[in] aABRes ��� �ڵ�
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_result_rebind_for_lob (cdbcABRes *aABRes)
{
    #define CDBC_FUNC_NAME "altibase_result_rebind_for_lob"

    cdbcBufferItm  *sBufItm;
    SQLSMALLINT     sTargetCType;
    acp_char_t     *sLocatorBufPtr;
    acp_sint32_t    sAdjustSize;
    acp_sint32_t    sTotBufSize;
    acp_sint32_t    sExpBufSize;
    ALTIBASE_RC     sRC;
    SQLRETURN       sCliRC;
    acp_bool_t      sErrorExist;
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HRES_NOT_VALID(aABRes), InvalidHandle);
    CDBC_TEST_RAISE(aABRes->mFieldInfos == NULL, InvalidHandle);

    sErrorExist = ACP_FALSE;
    for (i = 0; i < aABRes->mFieldCount; i++)
    {
        /* LOB�� LOCATOR�� �̿��� ���;� �ϹǷ� sTargetCType�� LOCATOR�� ���� */
        if (aABRes->mFieldInfos[i].type == ALTIBASE_TYPE_BLOB)
        {
            sTargetCType = SQL_BLOB_LOCATOR;
        }
        else if (aABRes->mFieldInfos[i].type == ALTIBASE_TYPE_CLOB)
        {
            sTargetCType = SQL_CLOB_LOCATOR;
        }
        else
        {
            continue;
        }

        /* LOB LOCATOR�� ���� ������ �ȵǸ� buffer �Ҵ� */
        sTotBufSize = aABRes->mBindResult[i].buffer_length * aABRes->mArrayFetchSize;
        sExpBufSize = ACI_SIZEOF(ALTIBASE_LOBLOCATOR) * aABRes->mArrayFetchSize;
        sLocatorBufPtr = aABRes->mBindResult[i].buffer;
        sAdjustSize = ((acp_ulong_t)sLocatorBufPtr) % ACI_SIZEOF(ALTIBASE_LOBLOCATOR);
        if ((sTotBufSize - sAdjustSize) < sExpBufSize)
        {
            sBufItm = altibase_new_buffer(&(aABRes->mBindBuffer),
                                          sExpBufSize,
                                          CDBC_BUFFER_TAIL);
            CDBC_TEST_RAISE(sBufItm == NULL, MAllocError);

            aABRes->mBindResult[i].buffer        = sBufItm->mBuffer;
            aABRes->mBindResult[i].buffer_length = sBufItm->mBufferLength;
        }
        else if (sAdjustSize > 0)
        {
            aABRes->mBindResult[i].buffer = sLocatorBufPtr + sAdjustSize;
        }

        CDBCLOG_PRINT_VAL("%d", i);
        CDBCLOG_PRINT_VAL("%d", aABRes->mBindResult[i].buffer_type);
        CDBCLOG_PRINT_VAL("%p", aABRes->mBindResult[i].buffer);
        CDBCLOG_PRINT_VAL("%d", (acp_sint32_t)(aABRes->mBindResult[i].buffer_length));
        CDBCLOG_PRINT_VAL("%p", aABRes->mBindResult[i].length);
        CDBCLOG_CALL("SQLBindCol");
        sCliRC = SQLBindCol(aABRes->mHstmt,
                            i+1,
                            sTargetCType,
                            aABRes->mBindResult[i].buffer,
                            aABRes->mBindResult[i].buffer_length,
                            aABRes->mBindResult[i].length);
        CDBCLOG_BACK_VAL("SQLBindCol", "%d", sCliRC);
        if (CDBC_CLI_NOT_SUCCEEDED(sCliRC))
        {
            altibase_set_errinfo_by_res(aABRes, sCliRC);
            aABRes->mBindResult[i].error = (acp_sint32_t)(aABRes->mDiagRec->mErrorCode);
            sErrorExist = ACP_TRUE;
        }
        else
        {
            aABRes->mBindResult[i].error = 0;
        }
    }
    CDBC_TEST_RAISE(sErrorExist == ACP_TRUE, ErrorSet);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(InvalidHandle)
    {
        sRC = ALTIBASE_INVALID_HANDLE;
    }
    CDBC_EXCEPTION(MAllocError);
    {
        altibase_set_errinfo(aABRes->mDiagRec,
                             ulERR_FATAL_MEMORY_ALLOC_ERROR,
                             CDBC_FUNC_NAME);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION(ErrorSet)
    {
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        CDBCLOG_CALL("SQLFreeStmt : SQL_UNBIND");
        sCliRC = SQLFreeStmt(aABRes->mHstmt, SQL_UNBIND);
        CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sCliRC);
        CDBC_ASSERT( CDBC_CLI_SUCCEEDED(sCliRC) );
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * ���ε� ���� �ٲ��� ���ε带 �ٽ� �ؾ��ϴ��� Ȯ��
 *
 * @return ���ε带 �ٽ� �ؾ��ϸ� ALTIBASE_TRUE, �׷��� ������ ALTIBASE_FALSE
 */
CDBC_INTERNAL
ALTIBASE_BOOL altibase_stmt_parambind_changed (cdbcABStmt *aABStmt, ALTIBASE_BIND *aBind)
{
    #define CDBC_FUNC_NAME "altibase_stmt_parambind_changed"

    ALTIBASE_BIND  *sBakBind;
    acp_sint32_t    sRC;
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    CDBC_TEST_RAISE((aABStmt->mBindParam == NULL) && (aBind == NULL), NoBind);
    CDBC_TEST_RAISE(aABStmt->mBindParam != aBind, IsChanged);
    CDBC_DASSERT(aABStmt->mBakBindParam != NULL);

    sBakBind = aABStmt->mBakBindParam;

    CDBCLOG_PRINT_VAL("%p", aABStmt->mBindParam);
    CDBCLOG_PRINT_VAL("%p", aBind);
    CDBCLOG_PRINT_VAL("%p", aABStmt->mBakBindParam);
    CDBCLOG_PRINT_VAL("%d", aABStmt->mParamCount);
    CDBCLOG_PRINT_VAL("%d", aABStmt->mArrayBindSize);
    CDBCLOG_CALL("acpMemCmp");
    sRC = acpMemCmp(sBakBind, aBind,
                    aABStmt->mParamCount * ACI_SIZEOF(ALTIBASE_BIND));
    CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
    CDBC_TEST_RAISE(sRC != 0, IsChanged);

    for (i = 0; i < aABStmt->mParamCount; i++)
    {
        if (aBind[i].length != NULL)
        {
            CDBC_TEST_RAISE(sBakBind[i].length == NULL, IsChanged);
            CDBCLOG_CALL("acpMemCmp");
            sRC = acpMemCmp(aABStmt->mBakBindParamLength
                            + (i * aABStmt->mArrayBindSize),
                            aBind[i].length,
                            aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_LONG));
            CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
            CDBC_TEST_RAISE(sRC != 0, IsChanged);
        }
        else
        {
            CDBC_TEST_RAISE(sBakBind[i].length != NULL, IsChanged);
        }

        if (aBind[i].is_null != NULL)
        {
            CDBC_TEST_RAISE(sBakBind[i].is_null == NULL, IsChanged);
            CDBCLOG_CALL("acpMemCmp");
            sRC = acpMemCmp(aABStmt->mBakBindParamIsNull
                            + (i * aABStmt->mArrayBindSize),
                            aBind[i].is_null,
                            aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_BOOL));
            CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
            CDBC_TEST_RAISE(sRC != 0, IsChanged);
        }
        else
        {
            CDBC_TEST_RAISE(sBakBind[i].is_null != NULL, IsChanged);
        }
    }

    CDBC_EXCEPTION_CONT(NoBind);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_FALSE");

    return ALTIBASE_FALSE;

    CDBC_EXCEPTION(IsChanged)
    {
        /* ������ �ƴϹǷ�, ���������� ������ �ʿ� ���� */
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_TRUE");

    return ALTIBASE_TRUE;

    #undef CDBC_FUNC_NAME
}

/**
 * �Ķ���� ���ε� ������ ����ϴµ� ����� �޸𸮸� �Ҵ��Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_stmt_parambind_alloc (cdbcABStmt *aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_parambind_alloc"

    acp_sint32_t    sBufSize;
    acp_char_t     *sBufPtr;
    cdbcBufferItm  *sBufItm;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));
    CDBC_DASSERT(aABStmt->mBakBindParam == NULL);
    CDBC_DASSERT(aABStmt->mParamCount > 0);
    CDBC_DASSERT(aABStmt->mArrayBindSize > 0);

    sBufSize = aABStmt->mParamCount
               * (ACI_SIZEOF(ALTIBASE_BIND) + aABStmt->mArrayBindSize
                  * (ACI_SIZEOF(ALTIBASE_LONG) * 2 + ACI_SIZEOF(ALTIBASE_BOOL)) );

    sBufItm = altibase_new_buffer(&(aABStmt->mBindBuffer), sBufSize,
                                  CDBC_BUFFER_TAIL);
    CDBC_TEST_RAISE(sBufItm == NULL, MAllocError);

    sBufPtr = sBufItm->mBuffer;

    aABStmt->mBakBindParam = (ALTIBASE_BIND *) sBufPtr;
    sBufPtr += aABStmt->mParamCount * ACI_SIZEOF(ALTIBASE_BIND);

    aABStmt->mBakBindParamLength = (ALTIBASE_LONG *) sBufPtr;
    sBufPtr += aABStmt->mParamCount
            * (aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_LONG));

    aABStmt->mRealBindParamLength = (ALTIBASE_LONG *) sBufPtr;
    sBufPtr += aABStmt->mParamCount
            * (aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_LONG));

    aABStmt->mBakBindParamIsNull = (ALTIBASE_BOOL *) sBufPtr;
    sBufPtr += aABStmt->mParamCount
            * (aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_BOOL));

    CDBC_ASSERT(sBufPtr == (sBufItm->mBuffer + sBufItm->mBufferLength));

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(MAllocError);
    {
        altibase_set_errinfo(&(aABStmt->mDiagRec),
                             ulERR_FATAL_MEMORY_ALLOC_ERROR,
                             CDBC_FUNC_NAME);
    }
    CDBC_EXCEPTION_END;

    aABStmt->mBakBindParam = NULL;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

/**
 * �Ķ���� ���ε� ������ ����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aBind ����� ���ε� ����
 */
CDBC_INTERNAL
void altibase_stmt_parambind_backup (cdbcABStmt *aABStmt, ALTIBASE_BIND *aBind)
{
    #define CDBC_FUNC_NAME "altibase_stmt_parambind_backup"

    ALTIBASE_BIND  *sBakBind;
    acp_sint32_t    sIndLenSize;
    acp_sint32_t    sIsNullSize;
    acp_sint32_t    sBaseIdx;
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    CDBC_DASSERT(aABStmt->mBakBindParam != NULL);
    CDBC_DASSERT(aABStmt->mBakBindParamLength != NULL);
    CDBC_DASSERT(aABStmt->mBakBindParamIsNull != NULL);

    sBakBind = aABStmt->mBakBindParam;

    /* ����� ���� ���� */
    CDBCLOG_PRINT_VAL("%p", aBind);
    CDBCLOG_PRINT_VAL("%p", aABStmt->mBakBindParam);
    CDBCLOG_PRINT_VAL("%d", aABStmt->mParamCount);
    CDBCLOG_PRINT_VAL("%d", aABStmt->mArrayBindSize);
    acpMemCpy(sBakBind, aBind, aABStmt->mParamCount * ACI_SIZEOF(ALTIBASE_BIND));

    sIndLenSize = aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_LONG);
    sIsNullSize = aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_BOOL);
    for (i = 0; i < aABStmt->mParamCount; i++)
    {
        sBaseIdx = i * aABStmt->mArrayBindSize;

        if (sBakBind[i].length != NULL)
        {
            acpMemCpy(aABStmt->mBakBindParamLength + sBaseIdx, sBakBind[i].length, sIndLenSize);
        }

        if (sBakBind[i].is_null != NULL)
        {
            acpMemCpy(aABStmt->mBakBindParamIsNull + sBaseIdx, sBakBind[i].is_null, sIsNullSize);
        }
    }

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

/**
 * �Ķ���� ���ε� ������ ����ϱ� ���� �Ҵ��� �޸𸮸� �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 */
CDBC_INTERNAL
void altibase_stmt_parambind_free (cdbcABStmt *aABStmt)
{
    #define CDBC_FUNC_NAME "altibase_stmt_parambind_free"

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    if (aABStmt->mBakBindParam != NULL)
    {
        altibase_clean_buffer(&(aABStmt->mBindBuffer));

        aABStmt->mBakBindParam        = NULL;
        aABStmt->mBakBindParamLength  = NULL;
        aABStmt->mBakBindParamIsNull  = NULL;
        aABStmt->mRealBindParamLength = NULL;
    }

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

typedef enum
{
    CDBC_BIND_CHECK_NONE    = 0,
    CDBC_BIND_CHECK_ALLOCED = 1,
    CDBC_BIND_CHECK_CHANGED = 2
} CDBC_BIND_CHECK;

CDBC_INLINE
acp_char_t * cdbc_bind_check_string (CDBC_BIND_CHECK aCheck)
{
    #define CASE_RETURN_STR(ID) case ID : return # ID

    switch (aCheck)
    {
        CASE_RETURN_STR( CDBC_BIND_CHECK_NONE );
        CASE_RETURN_STR( CDBC_BIND_CHECK_ALLOCED );
        CASE_RETURN_STR( CDBC_BIND_CHECK_CHANGED );

        default: return "CDBC_BIND_CHECK_UNKNOWN";
    }

    #undef CASE_RETURN_STR
}

/**
 * �Ķ���� ���ε带 �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aBind ���ε� ����
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_stmt_parambind_core (cdbcABStmt *aABStmt, ALTIBASE_BIND *aBind)
{
    #define CDBC_FUNC_NAME "altibase_stmt_parambind_core"

    SQLHDESC        sIPD        = SQL_NULL_HDESC;
    SQLSMALLINT     sParamType  = SQL_PARAM_TYPE_UNKNOWN;
    SQLSMALLINT     sSqlType;
    SQLSMALLINT     sBufType;
    ALTIBASE_LONG  *sIndLen;
    acp_sint32_t    sIndLenSize;
    acp_sint32_t    sBaseIdx;
    CDBC_BIND_CHECK sCheckBind  = CDBC_BIND_CHECK_NONE;
    acp_rc_t        sRC;
    acp_bool_t      sErrorExist;
    acp_sint32_t    i;
    acp_sint32_t    j;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    CDBC_TEST_RAISE(aABStmt->mParamCount == 0, NoBind);

    CDBCLOG_PRINT_VAL("%p", aABStmt->mBakBindParam);
    if (aABStmt->mBakBindParam == NULL)
    {
        sRC = altibase_stmt_parambind_alloc(aABStmt);
        CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));
        sCheckBind = CDBC_BIND_CHECK_ALLOCED;
    }
    else
    {
        CDBCLOG_CALL("acpMemCmp");
        sRC = acpMemCmp(aABStmt->mBakBindParam, aBind,
                        aABStmt->mParamCount * ACI_SIZEOF(ALTIBASE_BIND));
        CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
        sCheckBind = (sRC == 0) ? CDBC_BIND_CHECK_NONE : CDBC_BIND_CHECK_CHANGED;
    }
    CDBCLOG_PRINTF_ARG1("sCheckBind = %s", cdbc_bind_check_string(sCheckBind));

    CDBCLOG_CALL("SQLGetStmtAttr");
    sRC = SQLGetStmtAttr(aABStmt->mHstmt, SQL_ATTR_IMP_PARAM_DESC, &sIPD, SQL_IS_POINTER, NULL);
    CDBCLOG_BACK_VAL("SQLGetStmtAttr", "%d", sRC);
    CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);

    CDBCLOG_PRINT_VAL("%d", aABStmt->mParamCount);
    CDBCLOG_PRINT_VAL("%d", aABStmt->mArrayBindSize);
    CDBCLOG_PRINT_VAL("%p", aABStmt->mRealBindParamLength);
    CDBC_DASSERT(aABStmt->mRealBindParamLength != NULL);
    sIndLenSize = aABStmt->mArrayBindSize * ACI_SIZEOF(ALTIBASE_LONG);
    sErrorExist = ACP_FALSE;
    for (i = 0; i < aABStmt->mParamCount; i++)
    {
        CDBCLOG_PRINT_VAL("%d", i);
        aBind[i].error = 0; /* init */

        /* IndLen�� ������ �ʿ䰡 ���ٸ� length�� ���� �� �������ϸ� ���� */
        if ( (aBind[i].buffer_type != ALTIBASE_BIND_NULL) &&
             (aBind[i].is_null == NULL) )
        {
            CDBCLOG_PRINT(">> use length-direct <<");
            sIndLen = aBind[i].length;
        }
        else
        {
            sBaseIdx = (i * aABStmt->mArrayBindSize);
            CDBCLOG_PRINT_VAL("%d", sBaseIdx);
            sIndLen = aABStmt->mRealBindParamLength + sBaseIdx;
            CDBCLOG_PRINT_VAL("%p", sIndLen);
            CDBCLOG_PRINT_VAL("%d", aBind[i].buffer_type);
            CDBCLOG_PRINT_VAL("%p", aBind[i].length);
            CDBCLOG_PRINT_VAL("%p", aBind[i].is_null);

            CDBCLOG_PRINT(">> use length-temp <<");
            if (aBind[i].length != NULL)
            {
                CDBCLOG_CALL("acpMemCmp");
                sRC = acpMemCmp(aABStmt->mBakBindParamLength + sBaseIdx,
                                aBind[i].length,
                                sIndLenSize);
                CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
                if (sRC != 0)
                {
                    acpMemCpy(sIndLen, aBind[i].length, sIndLenSize);
                }
            }
            else
            {
                acpMemSet(sIndLen, 0, sIndLenSize);
            }

            if (aBind[i].is_null != NULL)
            {
                for (j = 0; j < aABStmt->mArrayBindSize; j++)
                {
                    switch (aBind[i].is_null[j])
                    {
                        case ALTIBASE_TRUE:
                            CDBCLOG_PRINTF_ARG1("is_null[ %d ] = TRUE", i);
                            sIndLen[j] = ALTIBASE_NULL_DATA;
                            break;
                        case ALTIBASE_FALSE:
                            CDBCLOG_PRINTF_ARG1("is_null[ %d ] = FALSE", i);
                            break;
                        default:
                            altibase_set_errinfo(&(aABStmt->mDiagRec),
                                                 ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
                            aBind[i].error = (acp_sint32_t)(aABStmt->mDiagRec.mErrorCode);
                            sErrorExist = ACP_TRUE;
                            CDBC_RAISE(ContinueToNextParam);
                            break;
                    }
                }
            } /* END :: if ( aBind[i].is_null != NULL ) */
        } /* END :: if (use length-direct) */

        if (sCheckBind != CDBC_BIND_CHECK_NONE)
        {
            CDBCLOG_PRINTF_ARG1("sCheckBind = %s", cdbc_bind_check_string(sCheckBind));

            /* ���ε� ������ �ȹٲ����� �Ѿ�� �ȴ� */
            if (sCheckBind == CDBC_BIND_CHECK_CHANGED)
            {
                CDBCLOG_CALL("acpMemCmp");
                sRC = acpMemCmp(&aABStmt->mBakBindParam[i], &aBind[i],
                                ACI_SIZEOF(ALTIBASE_BIND));
                CDBCLOG_BACK_VAL("acpMemCmp", "%d", sRC);
                CDBC_TEST_RAISE(sRC == 0, NoRebind);
                CDBCLOG_PRINT(">> Rebind <<");
            }
#if defined(USE_CDBCLOG)
            else
            {
                CDBCLOG_PRINT(">> First Bind <<");
            }
#endif

            CDBCLOG_CALL("SQLDescribeParam");
            sRC = SQLDescribeParam(aABStmt->mHstmt,
                                   i+1,
                                   &sSqlType,
                                   NULL,
                                   NULL,
                                   NULL);
            CDBCLOG_BACK_VAL("SQLDescribeParam", "%d", sRC);
            CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
            CDBCLOG_PRINTF_ARG2("sSqlType = %s (%d)", cli_sql_type_string(sSqlType), sSqlType);

            CDBCLOG_CALL("SQLGetDescField");
            sRC = SQLGetDescField(sIPD, i+1, SQL_DESC_PARAMETER_TYPE, &sParamType, SQL_IS_SMALLINT, NULL);
            CDBCLOG_BACK_VAL("SQLGetDescField", "%d", sRC);
            CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
            CDBCLOG_PRINTF_ARG2("sParamType = %s (%d)", cli_parameter_type_string(sParamType), sParamType);

            sBufType = aBind[i].buffer_type;
            CDBCLOG_PRINTF_ARG1("sBufType = %s", altibase_bind_type_string(sBufType));
            switch (sBufType)
            {
                case ALTIBASE_BIND_NULL:
                    CDBCLOG_PRINT("sBufType = ALTIBASE_BIND_NULL");
                    /* ���ε� Ÿ���� �̿��� NULL�� ���ε� �� �� �����Ƿ� Ÿ�� ���� */
                    sBufType = ALTIBASE_BIND_STRING;
                    sSqlType = SQL_CHAR;

                    for (j = 0; j < aABStmt->mArrayBindSize; j++)
                    {
                        sIndLen[j] = ALTIBASE_NULL_DATA;
                    }
                    break;
                case ALTIBASE_BIND_BINARY:
                    /* do nothing */
                    break;
                case ALTIBASE_BIND_STRING:
                    sSqlType = SQL_CHAR;
                    break;
                case ALTIBASE_BIND_WSTRING:
                    sSqlType = SQL_WCHAR;
                    break;
                case ALTIBASE_BIND_SMALLINT:
                    sSqlType = SQL_SMALLINT;
                    break;
                case ALTIBASE_BIND_INTEGER:
                    sSqlType = SQL_INTEGER;
                    break;
                case ALTIBASE_BIND_BIGINT:
                    sSqlType = SQL_BIGINT;
                    break;
                case ALTIBASE_BIND_REAL:
                    sSqlType = SQL_REAL;
                    break;
                case ALTIBASE_BIND_DOUBLE:
                    sSqlType = SQL_DOUBLE;
                    break;
                case ALTIBASE_BIND_NUMERIC:
                    sSqlType = SQL_NUMERIC;
                    break;
                case ALTIBASE_BIND_DATE:
                    sSqlType = SQL_DATE;
                    break;
                default:
                    altibase_set_errinfo(&(aABStmt->mDiagRec),
                                         ulERR_ABORT_INVALID_APP_BUFFER_TYPE, sBufType);
                    aBind[i].error = (acp_sint32_t)(aABStmt->mDiagRec.mErrorCode);
                    sErrorExist = ACP_TRUE;
                    CDBC_RAISE(ContinueToNextParam);
                    break;
            }

            CDBCLOG_PRINT_VAL("%d", sSqlType);
            CDBCLOG_PRINT_VAL("%p", aBind[i].buffer);
            CDBCLOG_PRINT_VAL("%d", (acp_sint32_t)(aBind[i].buffer_length));
            CDBCLOG_PRINT_VAL("%p", aBind[i].length);
            CDBCLOG_PRINT_VAL("%p", sIndLen);
            CDBCLOG_CALL("SQLBindParameter");
            sRC = SQLBindParameter(aABStmt->mHstmt,
                                   i+1,
                                   sParamType,
                                   sBufType,
                                   sSqlType,
                                   0,
                                   0,
                                   aBind[i].buffer,
                                   aBind[i].buffer_length,
                                   sIndLen);
            CDBCLOG_BACK_VAL("SQLBindParameter", "%d", sRC);
            if (CDBC_CLI_NOT_SUCCEEDED(sRC))
            {
                altibase_set_errinfo_by_stmt(aABStmt, sRC);
                aBind[i].error = (acp_sint32_t)(aABStmt->mDiagRec.mErrorCode);
                sErrorExist = ACP_TRUE;
                CDBC_RAISE(ContinueToNextParam);
            }
        } /* END :: if (sCheckBind != CDBC_BIND_CHECK_NONE) */
        else
        {
            CDBCLOG_PRINT(">> NoRebind <<");
            CDBC_EXCEPTION_CONT(NoRebind);
        }

        CDBC_EXCEPTION_CONT(ContinueToNextParam);
    }
    CDBC_TEST(sErrorExist == ACP_TRUE);

    if (sCheckBind != CDBC_BIND_CHECK_NONE)
    {
        altibase_stmt_parambind_backup(aABStmt, aBind);

        aABStmt->mBindParam = aBind;
    }

    CDBC_EXCEPTION_CONT(NoBind);

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION(STMTError);
    {
        altibase_set_errinfo_by_stmt(aABStmt, sRC);
    }
    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

/**
 * �Ķ���� ���ε带 �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aBind ���ε� ����
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_bind_param (ALTIBASE_STMT aABStmt, ALTIBASE_BIND *aBind)
{
    #define CDBC_FUNC_NAME "altibase_stmt_bind_param"

    cdbcABStmt     *sABStmt = (cdbcABStmt *) aABStmt;
    SQLRETURN       sCliRC;
    ALTIBASE_RC     sRC;

    SAFE_FAILOVER_RETRY_INIT();

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    CDBC_TEST_RAISE(STMT_NOT_PREPARED(sABStmt), FuncSeqError);
    CDBC_TEST_RAISE(aBind == NULL, InvalidNullPtr);

    Retry:

    if (sABStmt->mBindParam != aBind)
    {
        CDBCLOG_PRINT_VAL("%p", sABStmt->mBindParam);
        CDBCLOG_CALL("SQLFreeStmt : SQL_RESET_PARAMS");
        sRC = SQLFreeStmt(sABStmt->mHstmt, SQL_RESET_PARAMS);
        CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sRC);
        CDBC_TEST_RAISE(CDBC_CLI_NOT_SUCCEEDED(sRC), STMTError);
        sABStmt->mBindParam = NULL;
    }

    sRC = altibase_stmt_parambind_core(sABStmt, aBind);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

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
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        /* failover ��ó���� �����ϸ� retry �ϹǷ� reset ���ش�. */
        CDBCLOG_CALL("SQLFreeStmt : SQL_RESET_PARAMS");
        sCliRC = SQLFreeStmt(sABStmt->mHstmt, SQL_RESET_PARAMS);
        CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sCliRC);
        CDBC_ASSERT( CDBC_CLI_SUCCEEDED(sCliRC) );
        sABStmt->mBindParam = NULL;

        SAFE_FAILOVER_POST_PROC_AND_RETRY(sABStmt, Retry);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

/**
 * ��� ���ε带 �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aBind ���ε� ����
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
ALTIBASE_RC altibase_stmt_resultbind_proc (cdbcABStmt *aABStmt, ALTIBASE_BIND *aBind, ALTIBASE_BOOL aUseAllocLoc)
{
    #define CDBC_FUNC_NAME "altibase_stmt_resultbind_proc"

    acp_sint32_t    sFieldCount;
    acp_rc_t        sRC;
    acp_sint32_t    i;

    CDBCLOG_IN();

    CDBC_DASSERT(HSTMT_IS_VALID(aABStmt));

    sFieldCount = altibase_stmt_field_count(aABStmt);
    CDBC_TEST(sFieldCount == ALTIBASE_INVALID_FIELDCOUNT);

    /* buffer type, array size�� ���� �Ҵ��ؾ��� ������ �޶����Ƿ� �ٽ� �Ҵ� */
    altibase_result_bind_free(aABStmt->mRes);

    sRC = altibase_result_bind_init(aABStmt->mRes, aBind, (CDBC_ALLOC_BUF) aUseAllocLoc);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    sRC = altibase_result_bind_proc(aABStmt->mRes, (CDBC_USE_LOCATOR) aUseAllocLoc);
    /* ���ε� ���� ���� ���� */
    for (i = 0; i < sFieldCount; i++)
    {
        aBind[i].error = (aABStmt->mRes->mBindResult[i]).error;
    }
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_SUCCESS");

    return ALTIBASE_SUCCESS;

    CDBC_EXCEPTION_END;

    CDBCLOG_OUT_VAL("%s", "ALTIBASE_ERROR");

    return ALTIBASE_ERROR;

    #undef CDBC_FUNC_NAME
}

/**
 * ��� ���ε带 �����Ѵ�.
 *
 * @param[in] aABStmt ��ɹ� �ڵ�
 * @param[in] aBind ���ε� ����
 * @return ���������� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_EXPORT
ALTIBASE_RC altibase_stmt_bind_result (ALTIBASE_STMT aABStmt, ALTIBASE_BIND *aBind)
{
    #define CDBC_FUNC_NAME "altibase_stmt_bind_result"

    cdbcABStmt     *sABStmt = (cdbcABStmt *) aABStmt;
    SQLRETURN       sCliRC;
    ALTIBASE_RC     sRC;

    SAFE_FAILOVER_RETRY_INIT();

    CDBCLOG_IN();

    CDBC_TEST_RAISE(HSTMT_NOT_VALID(sABStmt), InvalidHandle);

    altibase_init_errinfo(&(sABStmt->mDiagRec));

    CDBC_TEST_RAISE(STMT_NOT_PREPARED(sABStmt), FuncSeqError);
    CDBC_TEST_RAISE(aBind == NULL, InvalidNullPtr);
    CDBC_DASSERT(sABStmt->mRes != NULL);

    Retry:

    sRC = altibase_stmt_resultbind_proc(sABStmt, aBind, ALTIBASE_FALSE);
    CDBC_TEST(ALTIBASE_NOT_SUCCEEDED(sRC));

    sABStmt->mBindResult = aBind;

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
    CDBC_EXCEPTION(InvalidNullPtr);
    {
        altibase_set_errinfo(&(sABStmt->mDiagRec),
                             ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
        sRC = ALTIBASE_ERROR;
    }
    CDBC_EXCEPTION_END;

    if (sRC != ALTIBASE_INVALID_HANDLE)
    {
        CDBCLOG_CALL("SQLFreeStmt : SQL_UNBIND");
        sCliRC = SQLFreeStmt(sABStmt->mHstmt, SQL_UNBIND);
        CDBCLOG_BACK_VAL("SQLFreeStmt", "%d", sCliRC);
        CDBC_ASSERT( CDBC_CLI_SUCCEEDED(sCliRC) );
        sABStmt->mBindResult = NULL;

        SAFE_FAILOVER_POST_PROC_AND_RETRY(sABStmt, Retry);
    }

    CDBCLOG_OUT_VAL("%s", altibase_rc_string(sRC));

    return sRC;

    #undef CDBC_FUNC_NAME
}

