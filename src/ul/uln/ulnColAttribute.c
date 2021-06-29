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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnColAttribute.h>

/*
 * ULN_SFID_06
 * SQLColAttribute(), STMT, S2
 *      -- [1]
 *      07005 [2]
 *  where
 *      [1] FieldIdentifier was SQL_DESC_COUNT.
 *      [2] FieldIdentifier was not SQL_DESC_COUNT.
 */
ACI_RC ulnSFID_06(ulnFnContext *aFnContext)
{
    if(aFnContext->mWhere == ULN_STATE_ENTRY_POINT)
    {
        ACI_TEST_RAISE(*(acp_uint16_t *)(aFnContext->mArgs) != SQL_DESC_COUNT, LABEL_07005);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_07005)
    {
        /*
         * 07005 : Prepared statement is not a cursor specification.
         * Cause : Since the statement associated with the statement handle did not return
         *         a result set, there was no column to describe.
         */
        ulnError(aFnContext, ulERR_ABORT_STMT_HAVE_NO_RESULT_SET);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/*----------------------------------------------------------------*
 *
 * Description:
 *
 *
 * Implementation:
 *
 * To Fix BUG-17521
 *    : win64 ������ ���� ������ 64-bit value�� �����Ѵ�.
 *
 * When the FieldIdentifier parameter has one of the following values,
 * a 64-bit value is returned in *NumericAttribute:
 *
 *      SQL_DESC_DISPLAY_SIZE
 *      SQL_DESC_LENGTH
 *      SQL_DESC_OCTET_LENGTH
 *      SQL_DESC_COUNT
 *
 *---------------------------------------------------------------*/

SQLRETURN ulnColAttribute(ulnStmt      *aStmt,
                          acp_uint16_t  aColumnNumber,
                          acp_uint16_t  aFieldIdentifier,
                          void         *aCharacterAttributePtr,
                          acp_sint16_t  aBufferLength,
                          acp_sint16_t *aStringLengthPtr,
                          void         *aNumericAttributePtr)
{
    ULN_FLAG(sNeedExit);
    ulnFnContext  sFnContext;
    ulnDesc      *sDescIrd    = NULL;
    ulnDescRec   *sDescRecIrd = NULL;

    acp_uint32_t  sLength;
    acp_char_t   *sSourceBuffer;
    acp_sint16_t  sSQLTYPE;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_COLATTRIBUTE, aStmt, ULN_OBJ_TYPE_STMT);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /* PROJ-1891, BUG-46011 If deferred prepare is exists, process it first */
    if (ulnStmtIsSetDeferredQstr(aStmt) == ACP_TRUE)
    {
        ACI_TEST( ulnPrepareDeferComplete(&sFnContext, ACP_TRUE) );
    }

    ACI_TEST_RAISE((aColumnNumber == 0) &&
                   (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_OFF),
                   LABEL_INVALID_INDEX);

    ACI_TEST_RAISE(aColumnNumber > ulnStmtGetColumnCount(aStmt),
                   LABEL_INVALID_INDEX);

    sDescIrd = ulnStmtGetIrd(aStmt);
    ACI_TEST_RAISE(sDescIrd == NULL, LABEL_MEM_MAN_ERR);

    /* PROJ-1789 Updatable Scrollable Cursor */
    if(aColumnNumber == 0)
    {
        /* Ird�� BOOKMARK�� ���� ������ ����.
         * ���� �����Ǿ������Ƿ� �׳� hard coding �Ѵ�. */

        switch(aFieldIdentifier)
        {
            case SQL_DESC_TYPE:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr =
                        (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_VARIABLE)
                        ? SQL_BIGINT : SQL_INTEGER;
                }
                break;

            case SQL_DESC_OCTET_LENGTH:
                if(aNumericAttributePtr != NULL)
                {
                    *(ulvSLen *)aNumericAttributePtr =
                        (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_VARIABLE)
                        ? ACI_SIZEOF(acp_sint64_t) : ACI_SIZEOF(acp_sint32_t);
                }
                break;

            default:
                /* do nothing */;
                break;
        }
    }
    else /* if (aColumnNumver > 0) */
    {
        sDescRecIrd = ulnStmtGetIrdRec(aStmt, aColumnNumber);
        ACI_TEST_RAISE(sDescRecIrd == NULL, LABEL_MEM_MAN_ERR2);

        /* BUGBUG : ulnGetDescField �� �ߺ��� �ڵ尡 ���� �����Ѵ�.
         *          ���� SQLColAttribute() �Լ��� ��쿡�� ��� ����
         *          32bit signed-int�� ĳ�����ؼ� ����ڿ��� �����־�� �Ѵٴ�
         *          ���̰� ������, ��� ����� ��ƾ�� ���ؼ� ���� ������ ������
         *          �� ���� ĳ�����ϵ��� �ؾ� �� ���̴�.
         */

        switch(aFieldIdentifier)
        {
            case SQL_DESC_CONCISE_TYPE:
                if(aNumericAttributePtr != NULL)
                {
                    /* Note : ��ó�� ulnTypes.cpp ���� �ٺ������� �������� �ʰ�,
                     * ����ڿ��� Ÿ���� �����ִ� �Լ����� �׶� �׶� long type��
                     * �����ϴ� �Լ��� ȣ���ϴ� ���� ������ ������ �ְ� ������
                     * ��������, ulnTypes.cpp�� function context, dbc, stmt ����
                     * �������� �ٸ� �͵��� �޴� �Լ��� ����� ���� �ʾƼ�
                     * ���� �̿Ͱ��� �ߴ�.
                     */

                    sSQLTYPE = ulnMetaGetOdbcConciseType(&sDescRecIrd->mMeta);
                    sSQLTYPE = ulnTypeMap_LOB_SQLTYPE(sSQLTYPE,
                                                      ulnDbcGetLongDataCompat(aStmt->mParentDbc));
                    *(acp_sint32_t *)aNumericAttributePtr = (acp_sint32_t)sSQLTYPE;
                }
                break;

            case SQL_DESC_DISPLAY_SIZE:
                if(aNumericAttributePtr != NULL)
                {
                    // To Fix BUG-22936
                    *(ulvSLen *)aNumericAttributePtr =
                        (ulvSLen)ulnDescRecGetDisplaySize(sDescRecIrd);
                }
                break;

            /* PROJ-1789 Updatable Scrollable Cursor */
            case SQL_DESC_LABEL:
            case SQL_DESC_NAME:
                //BUG-28184 [CodeSonar] Ignored Return Value
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                /* BUGBUG (BUG-33625) */
                sSourceBuffer = ulnDescRecGetDisplayName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_BASE_COLUMN_NAME:
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetBaseColumnName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_NULLABLE:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = ulnMetaGetNullable(&sDescRecIrd->mMeta);
                }
                break;

            case SQL_DESC_OCTET_LENGTH:
                if(aNumericAttributePtr != NULL)
                {
                    // BUG-22301
                    // To Fix BUG-22936
                    // bug-34925 SQL_DESC_OCTET_LENGTH should not include the nullbyte
                    *(ulvSLen *)aNumericAttributePtr =
                        (ulvSLen)ulnMetaGetOctetLength(&sDescRecIrd->mMeta);
                }
                break;

            case SQL_DESC_LENGTH:
                if(aNumericAttributePtr != NULL)
                {
                    /* BUGBUG: IRD �� ��Ÿ�� �����ϸ鼭 length�� octet length��
                     * �׻� ���� ������ �����ϴµ�, �̰��� �´��� �� �𸣰ڴ�.
                     *
                     * ��·��, length�� �о �״�� �Ѱ��ش�.
                     * null �������� ���̴� ������ ������. */

                    // To Fix BUG-22936
                    *(ulvSLen *)aNumericAttributePtr =
                        (ulvSLen)ulnMetaGetOdbcLength(&sDescRecIrd->mMeta);
                }
                break;

            case SQL_DESC_PRECISION:
                if(aNumericAttributePtr != NULL)
                {
                    // fix BUG-17229
                    *(acp_sint32_t *)aNumericAttributePtr =
                        ulnTypeGetColumnSizeOfType(ulnMetaGetMTYPE(&sDescRecIrd->mMeta),
                                                   &sDescRecIrd->mMeta);
                }
                break;

            case SQL_DESC_SCALE:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = ulnMetaGetScale(&sDescRecIrd->mMeta);
                }
                break;

            case SQL_DESC_TYPE:
                if(aNumericAttributePtr != NULL)
                {
                    /*
                     * verbose data type �� �����ؾ� �Ѵ�.
                     * ulnMeta �� odbc type ���� verbose type �� ����.
                     *
                     * Note : BUG-17018 ���� comment
                     *
                     *      SQLDescribeCol() �Լ����� �����ϴ� Ÿ�� ����
                     *         ��ũ������ SQL_DESC_CONCISE_TYPE ������
                     *
                     *      SQLColAttribute() �Լ��� SQL_DESC_TYPE ���� ȣ������
                     *         ������ ��ũ������ SQL_DESC_TYPE �� ���� �����Ѵ�.
                     *
                     *      SQL_DESC_CONCISE_TYPE ���� concise type �� ����ǰ�,
                     *      SQL_DESC_TYPE ���� verbose type �� ����ȴ�.
                     *
                     *      datetime Ÿ�Ե鿡 ���ؼ� ���� ������ ���� �Ʒ��� ����.
                     *
                     *      concise type           verbose type
                     *      ------------------------------------------------
                     *      SQL_TYPE_DATE          SQL_DATETIME
                     *      SQL_TYPE_TIME          SQL_DATETIME
                     *      SQL_TYPE_TIMESTAMP     SQL_DATETIME
                     *      ------------------------------------------------
                     *
                     *      ��, �������� �˾Ҵµ�, ����� ������ �ξ��� ��.
                     */

                    sSQLTYPE = ulnMetaGetOdbcType(&sDescRecIrd->mMeta);

                    /* PROJ-2638 shard native linker */
                    if ( sSQLTYPE >= ULSD_INPUT_RAW_MTYPE_NULL )
                    {
                        sSQLTYPE = ulnTypeMap_MTYPE_SQL( sSQLTYPE - ULSD_INPUT_RAW_MTYPE_NULL );
                    }
                    else
                    {
                        /* Do Nothing. */
                    }

                    sSQLTYPE = ulnTypeMap_LOB_SQLTYPE(sSQLTYPE,
                                                      ulnDbcGetLongDataCompat(aStmt->mParentDbc));

                    *(acp_sint32_t *)aNumericAttributePtr = (acp_sint32_t)sSQLTYPE;
                }
                break;

            case SQL_DESC_COUNT:
                if(aNumericAttributePtr != NULL)
                {
                    // To Fix BUG-22936
                    *(ulvSLen *)aNumericAttributePtr = (ulvSLen)ulnStmtGetColumnCount(aStmt);
                }
                break;

            case SQL_DESC_SEARCHABLE:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = ulnDescRecGetSearchable(sDescRecIrd);
                }
                break;

            case SQL_DESC_UNSIGNED:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = ulnDescRecGetUnsigned(sDescRecIrd);
                }
                break;

            case SQL_DESC_UNNAMED:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = ulnDescRecGetUnnamed(sDescRecIrd);
                }
                break;

            case SQL_DESC_LITERAL_PREFIX:
                /* BUG-28980 [CodeSonar]Ignored Return Value */
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);
                sSourceBuffer = ulnDescRecGetLiteralPrefix(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_LITERAL_SUFFIX:
                /* BUG-28623 [CodeSonar]Ignored Return Value */
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetLiteralSuffix(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_LOCAL_TYPE_NAME:
            case SQL_DESC_TYPE_NAME:
                /* BUG-28980 [CodeSonar]Ignored Return Value */
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);
                sSourceBuffer = ulnDescRecGetTypeName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_UPDATABLE:
                /* PROJ-1789 Updatable Scrollable Cursor : DB2�� ���� �׻� UNKNOWN ��ȯ. */
                if(aNumericAttributePtr != NULL)
                {
                    // fix BUG-23997
                    // ������ ������ ���� �÷� �Ӽ��� SQL_ATTR_READONLY����
                    // SQL_ATTR_READWRITE_UNKNOWN���� ����
                    // ����Ŭ�� DB2�� SQL_ATTR_READWRITE_UNKNOWN��
                    *(acp_sint32_t *)aNumericAttributePtr = SQL_ATTR_READWRITE_UNKNOWN;
                }
                break;

            /* PROJ-1789 Updatable Scrollable Cursor */

            case SQL_DESC_TABLE_NAME:
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetTableName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_BASE_TABLE_NAME:
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetBaseTableName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_SCHEMA_NAME:
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetSchemaName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            case SQL_DESC_CATALOG_NAME:
                ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFFERSIZE);

                sSourceBuffer = ulnDescRecGetCatalogName(sDescRecIrd);
                if(sSourceBuffer == NULL)
                {
                    sLength = 0;
                }
                else
                {
                    sLength = acpCStrLen(sSourceBuffer, ACP_SINT32_MAX);
                }

                ulnDataWriteStringToUserBuffer(&sFnContext,
                                               sSourceBuffer,
                                               sLength,
                                               (acp_char_t *)aCharacterAttributePtr,
                                               aBufferLength,
                                               aStringLengthPtr);
                break;

            //Fix Bug-18607
            case SQL_DESC_FIXED_PREC_SCALE:
                if(aNumericAttributePtr != NULL)
                {
                    // To Fix BUG-22936
                    *(ulvSLen *)aNumericAttributePtr =
                        (ulvSLen) ulnMetaGetFixedPrecScale(&(sDescRecIrd->mMeta));
                }
                break;

            // fix BUG-24695 altibase4.3.9�� �����ϵ��� ��� �߰�
            case SQL_DESC_AUTO_UNIQUE_VALUE:
                if(aNumericAttributePtr != NULL)
                {
                    *(acp_sint32_t *)aNumericAttributePtr = SQL_FALSE;
                }
                break;

            // fix BUG-24695 altibase4.3.9�� �����ϵ��� ��� �߰�
            case SQL_DESC_CASE_SENSITIVE:
                if(aNumericAttributePtr != NULL)
                {
                    switch(ulnMetaGetMTYPE(&sDescRecIrd->mMeta))
                    {
                        case ULN_MTYPE_CHAR :
                        case ULN_MTYPE_VARCHAR :
                        case ULN_MTYPE_NCHAR :
                        case ULN_MTYPE_NVARCHAR :
                        case ULN_MTYPE_CLOB :
                            *(acp_sint32_t *)aNumericAttributePtr = SQL_TRUE;
                            break;
                        default:
                            *(acp_sint32_t *)aNumericAttributePtr = SQL_FALSE;
                    }
                }
                break;

            case SQL_DESC_NUM_PREC_RADIX:
                ACI_RAISE(LABEL_NOT_IMPLEMENTED);
                break;

            /*
             * BUGBUG : SQL_DESC_PARAMETER_TYPE ��?
             */

            default:
                ACI_RAISE(LABEL_INVALID_DESC_FIELD_ID);
                break;
        }
    }

    /*
     * Exit
     */
    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_HIGH, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" id: %4"ACI_UINT32_FMT"]",
            "ulnColAttribute", aColumnNumber, aFieldIdentifier);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_INVALID_BUFFERSIZE)
    {
        /*
         * HY090 : ����� �ϴ� ������ ���� Ÿ���ε�, aBufferLength �� ������ �־��� ���
         */
        ulnError(&sFnContext, ulERR_ABORT_INVALID_BUFFER_LEN, aBufferLength);
    }

    ACI_EXCEPTION(LABEL_NOT_IMPLEMENTED)
    {
        ulnError(&sFnContext, ulERR_ABORT_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
    }

    ACI_EXCEPTION(LABEL_INVALID_DESC_FIELD_ID)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_DESC_FIELD_IDENTIFIER, aFieldIdentifier);
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnColAttribute");
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR2)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnColAttribute2");
    }

    ACI_EXCEPTION(LABEL_INVALID_INDEX)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aColumnNumber);
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" id: %4"ACI_UINT32_FMT"] fail",
            "ulnColAttribute", aColumnNumber, aFieldIdentifier);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
