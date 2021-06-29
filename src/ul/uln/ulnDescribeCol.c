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

static ACI_RC ulnDescribeColCheckArgs(ulnFnContext *aFnContext,
                                      acp_uint16_t  aColumnNumber,
                                      acp_sint16_t  aBufferLength)
{
    ulnStmt      *sStmt = aFnContext->mHandle.mStmt;
    acp_sint16_t  sNumOfResultColumns;

    /*
     * HY090 : Invalid string or buffer length
     */
    ACI_TEST_RAISE(aBufferLength < 0, LABEL_INVALID_BUFF_LEN);

    sNumOfResultColumns = ulnStmtGetColumnCount(sStmt);
    ACI_TEST_RAISE(sNumOfResultColumns == 0, LABEL_NO_RESULT_SET);

    /*
     * 07009 : Invalid Descriptor index
     */
    ACI_TEST_RAISE((aColumnNumber == 0) &&
                   (ulnStmtGetAttrUseBookMarks(sStmt) == SQL_UB_OFF),
                   LABEL_INVALID_DESC_INDEX);

    ACI_TEST_RAISE(aColumnNumber > sNumOfResultColumns, LABEL_INVALID_DESC_INDEX);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_DESC_INDEX)
    {
        /*
         * 07009 : BOOKMARK �������ϴµ� column number �� 0 �� �شٰų�
         *         result column ���� ū index �� ���� �� �߻�
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aColumnNumber);
    }

    ACI_EXCEPTION(LABEL_NO_RESULT_SET)
    {
        /*
         * 07005 : result set �� ���� ���ϴ� statement �� ����Ǿ
         *         result set �� ���µ� SQLDescribeCol() �� ȣ���Ͽ���.
         */
        ulnError(aFnContext, ulERR_ABORT_STMT_HAVE_NO_RESULT_SET);
    }

    ACI_EXCEPTION(LABEL_INVALID_BUFF_LEN)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_BUFFER_LEN, aBufferLength);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static void ulnDescribeColDoColumnName(ulnFnContext *aFnContext,
                                       ulnDescRec   *aIrdRecord,
                                       acp_char_t   *aColumnName,
                                       acp_uint16_t  aBufferLength,
                                       acp_sint16_t *aNameLengthPtr)
{
    acp_char_t   *sName;
    acp_uint32_t  sNameLength;

    if (aColumnName != NULL)
    {
        /*
         * Name�� length�� ���Ѵ�.
         */
        /* BUGBUG (BUG-33625) */
        sName = ulnDescRecGetDisplayName(aIrdRecord);

        if (sName == NULL)
        {
            sNameLength = 0;
        }
        else
        {
            sNameLength = acpCStrLen(sName, ACP_SINT32_MAX);
        }

        ulnDataWriteStringToUserBuffer(aFnContext,
                                       sName,
                                       sNameLength,
                                       aColumnName,
                                       aBufferLength,
                                       aNameLengthPtr);
    }
}

static void ulnDescribeColDoDataTypePtr(ulnDescRec   *aDescRecIrd,
                                        acp_sint16_t *aDataTypePtr,
                                        acp_bool_t    aLongDataCompat)

{
    acp_sint16_t sDataType;

    if (aDataTypePtr != NULL)
    {
        /*
         * BUGBUG: ODBC 2.0 ���ø����̼��̸� ������ �� ��� �Ѵ�.
         */
        sDataType     = ulnTypeMap_MTYPE_SQL(ulnMetaGetMTYPE(&aDescRecIrd->mMeta));

        /*
         * Note : ��ó�� ulnTypes.cpp ���� �ٺ������� �������� �ʰ�,
         *        ����ڿ��� Ÿ���� �����ִ� �Լ����� �׶��׶� LOB type �� �����ϴ� �Լ���
         *        ȣ���ϴ� ���� ������ ������ �ְ� ������ ��������,
         *        ulnTypes.cpp �� function context, dbc, stmt ���� �������� �ٸ� �͵���
         *        �޴� �Լ��� ����� ���� �ʾƼ� ���� �̿Ͱ��� �ߴ�.
         */
        sDataType     = ulnTypeMap_LOB_SQLTYPE(sDataType, aLongDataCompat);

        *aDataTypePtr = sDataType;
    }
}

static void ulnDescribeColDoColumnSizePtr(ulnDescRec   *aDescRecIrd,
                                          ulvULen      *aColumnSizePtr,
                                          acp_sint16_t *aDecimalDigitsPtr)
{
    ulvULen    sColumnSize;
    ulnMTypeID sMTYPE;

    sMTYPE = ulnMetaGetMTYPE(&aDescRecIrd->mMeta);


    if (aColumnSizePtr != NULL)
    {
        sColumnSize     = ulnTypeGetColumnSizeOfType(sMTYPE, &(aDescRecIrd->mMeta));
        *aColumnSizePtr = sColumnSize;
    }

    if (aDecimalDigitsPtr != NULL)
    {
        *aDecimalDigitsPtr = ulnTypeGetDecimalDigitsOfType(sMTYPE, &(aDescRecIrd->mMeta));
    }
}

static void ulnDescribeColDoNullablePtr(ulnDescRec *aDescRecIrd, acp_sint16_t *aNullablePtr)
{
    if (aNullablePtr != NULL)
    {
        *aNullablePtr = aDescRecIrd->mMeta.mNullable;
    }
}

SQLRETURN ulnDescribeCol(ulnStmt      *aStmt,
                         acp_uint16_t  aColumnNumber,
                         acp_char_t   *aColumnName,
                         acp_sint16_t  aBufferLength,
                         acp_sint16_t *aNameLengthPtr,
                         acp_sint16_t *aDataTypePtr,
                         ulvULen      *aColumnSizePtr,
                         acp_sint16_t *aDecimalDigitsPtr,
                         acp_sint16_t *aNullablePtr)
{
    ULN_FLAG(sNeedExit);

    ulnDescRec   *sIrdRecord;
    ulnFnContext  sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_DESCRIBECOL, aStmt, ULN_OBJ_TYPE_STMT);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /* PROJ-1891, BUG-46011 If deferred prepare is exists, process it first */
    if (ulnStmtIsSetDeferredQstr(aStmt) == ACP_TRUE)
    {
        ACI_TEST( ulnPrepareDeferComplete(&sFnContext, ACP_TRUE) );
    }

    /*
     * ===========================================
     * Function BEGIN
     * ===========================================
     */

    ACI_TEST(ulnDescribeColCheckArgs(&sFnContext,
                                     aColumnNumber,
                                     aBufferLength) != ACI_SUCCESS);

    /* PROJ-1789 Updatable Scrollable Cursor */
    if(aColumnNumber == 0)
    {
        /* Ird���� BOOKMARK�� ���� ������ ����.
         * ���� �����Ǿ������Ƿ� hard coding �Ѵ�. */

        if (aColumnName != NULL)
        {
            ulnDataWriteStringToUserBuffer(&sFnContext,
                                           "0",
                                           acpCStrLen("0", ACP_SINT32_MAX),
                                           aColumnName,
                                           aBufferLength,
                                           aNameLengthPtr);
        }

        if (aDataTypePtr != NULL)
        {
            *aDataTypePtr =
                (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_VARIABLE)
                ? SQL_BIGINT : SQL_INTEGER;
        }

        if (aColumnSizePtr != NULL)
        {
            *aColumnSizePtr =
                (ulnStmtGetAttrUseBookMarks(aStmt) == SQL_UB_VARIABLE)
                ? 20 : 10;
        }

        if (aDecimalDigitsPtr != NULL)
        {
            *aDecimalDigitsPtr = 0;
        }

        if (aNullablePtr != NULL)
        {
            *aNullablePtr = SQL_NO_NULLS;
        }
    }
    else /* if (aColumnNumver > 0) */
    {
        sIrdRecord = ulnStmtGetIrdRec(aStmt, aColumnNumber);
        ACI_TEST_RAISE(sIrdRecord == NULL, LABEL_MEM_MAN_ERR);

        /*
        * Note : aBufferLength �� ���� üũ �ϸ鼭 0 �̻����� Ȯ���ߴ�.
        */
        ulnDescribeColDoColumnName(&sFnContext,
                                   sIrdRecord,
                                   aColumnName,
                                   aBufferLength,
                                   aNameLengthPtr);

        ulnDescribeColDoDataTypePtr(sIrdRecord,
                                    aDataTypePtr,
                                    ulnDbcGetLongDataCompat(aStmt->mParentDbc));

        ulnDescribeColDoColumnSizePtr(sIrdRecord, aColumnSizePtr, aDecimalDigitsPtr);

        ulnDescribeColDoNullablePtr(sIrdRecord, aNullablePtr);
    }

    /*
     * ===========================================
     * Function END
     * ===========================================
     */

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_MID, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" name: %-20s"
            " stypePtr: %p]", "ulnDescribeCol",
            aColumnNumber, aColumnName, aDataTypePtr);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(&sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnDescribeCol : IRD not found");
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" name: %-20s"
            " stypePtr: %p] fail", "ulnDescribeCol",
            aColumnNumber, aColumnName, aDataTypePtr);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

