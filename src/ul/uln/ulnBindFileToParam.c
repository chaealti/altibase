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

/*
 * 07006 ���ѵ� ������ Ÿ�� �Ӽ� ���� | sqlType ���ڿ� ��õ� ���� ��ȿ�� ���� �ƴ�
 * 07009 ��ȿ���� ���� ��ȣ           | ��õ� par�� ���� 1���� ����
 * 08S01 ��� ȸ�� ���               | ODBC�� �����ͺ��̽� ���� �Լ� ó���� �Ϸ�Ǳ�
 *                                    | ���� ��� ȸ�� ����
 * HY000 �Ϲ� ����                    |
 * HY001 �޸� �Ҵ� ����             | �� �Լ��� �����ϱ� ���� �ʿ��� �޸� �Ҵ� ����
 * HY009 ��ȿ���� ���� ���� ���      | fileName, fileOptions, ind ���ڰ� null pointer
 *       (null pointer)
 * HY090 ��ȿ���� ���� ���ڿ� ����    | maxFileNameLength ���ڰ� 0���� ����
 */

static ACI_RC ulnBindFileToParamCheckArgs(ulnFnContext  *aContext,
                                          acp_uint16_t   aParamNumber,
                                          acp_sint16_t   aSqlType,
                                          acp_char_t   **aFileNameArray,
                                          acp_uint32_t  *aFileOptionPtr,
                                          ulvSLen        aMaxFileNameLength,
                                          ulvSLen       *aIndicator)
{
    ACI_TEST_RAISE( aParamNumber < 1, LABEL_INVALID_PARAMNUM );

    ACI_TEST_RAISE( aSqlType != SQL_BINARY && aSqlType != SQL_BLOB && aSqlType != SQL_CLOB, LABEL_INVALID_CONVERSION );

    ACI_TEST_RAISE( aFileNameArray == NULL || aFileOptionPtr == NULL || aIndicator == NULL, LABEL_INVALID_USE_OF_NULL );

    ACI_TEST_RAISE( aMaxFileNameLength < ULN_vLEN(0), LABEL_INVALID_BUFFER_LEN );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_PARAMNUM)
    {
        /*
         * Invalid descriptor index
         * 07009
         */
        ulnError(aContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aParamNumber);
    }
    ACI_EXCEPTION(LABEL_INVALID_CONVERSION)
    {
        /*
         * Restricted data type attribute violation
         * 07006
         */
        ulnError(aContext,
                 ulERR_ABORT_RESTRICTED_DATATYPE_VIOLATION,
                 SQL_C_FILE,
                 aSqlType);
    }
    ACI_EXCEPTION(LABEL_INVALID_USE_OF_NULL)
    {
        /*
         * HY009
         */
        ulnError(aContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    ACI_EXCEPTION(LABEL_INVALID_BUFFER_LEN)
    {
        /*
         * Invalid string or buffer length
         */
        ulnError(aContext, ulERR_ABORT_INVALID_BUFFER_LEN, aMaxFileNameLength);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * filename �� type, � ���� �͵��� ��δ� APD record �� �����Ѵ�.
 *
 * ���� execute �ÿ� �ش� ���ϵ��� open �ؼ� read �Ͽ� �����͸� �����Ѵ�.
 *
 * BUGBUG : aIndicator �� �ǹ̸� �ٽ��ѹ� ������ ������ ����.
 */
SQLRETURN ulnBindFileToParam(ulnStmt       *aStmt,
                             acp_uint16_t   aParamNumber,
                             acp_sint16_t   aSqlType,
                             acp_char_t   **aFileNameArray,
                             ulvSLen       *aFileNameLengthArray,
                             acp_uint32_t  *aFileOptionPtr,
                             ulvSLen        aMaxFileNameLength,
                             ulvSLen       *aIndicator)
{
    ulnFnContext  sFnContext;
    ulnPtContext *sPtContext = NULL;
    ulnDescRec   *sDescRecApd;

    acp_bool_t    sNeedExit = ACP_FALSE;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_BINDFILETOPARAM, aStmt, ULN_OBJ_TYPE_STMT);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;

    sPtContext = &(aStmt->mParentDbc->mPtContext);
    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    ACI_TEST_RAISE(cmiGetLinkImpl(&sPtContext->mCmiPtContext) == CMI_LINK_IMPL_IPCDA,
                   IPCDANotSupport);
    /*
     * arguments checking
     */
    ACI_TEST(ulnBindFileToParamCheckArgs(&sFnContext,
                                         aParamNumber,
                                         aSqlType,
                                         aFileNameArray,
                                         aFileOptionPtr,
                                         aMaxFileNameLength,
                                         aIndicator) != ACI_SUCCESS);

    /*
     * ulnBindParameter() �� ȣ���Ͽ� �⺻���� APD, IPD ������ �Ѵ�.
     *
     * Note : aStrLenOrIndPtr �� aIndicator �� �ƴ϶�
     *        aFileNameLengthArray �� ���������ν�
     *        ulnDescRec �� mOctetLengthPtr �� aFileNameLengthArray �� ������ �Ѵ�.
     */
    ACI_TEST(ulnBindParamBody(&sFnContext,
                              aParamNumber,
                              NULL,
                              SQL_PARAM_INPUT,
                              SQL_C_FILE,
                              aSqlType,
                              0, // column size : BUGBUG
                              0, // decimal digits : BUGBUG
                              (void *)aFileNameArray,
                              aMaxFileNameLength,
                              aFileNameLengthArray) != ACI_SUCCESS);

    sDescRecApd = ulnStmtGetApdRec(aStmt, aParamNumber);

    ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_MEM_MAN_ERR);

    /*
     * file option �迭�� indicator �迭�� �Ҵ��Ѵ�.
     *
     * BUGBUG : �� �� ����ϰ� �����ϴ� ����� �����ؾ� �Ѵ�. ����� ����.
     */
    sDescRecApd->mFileOptionsPtr = aFileOptionPtr;
    ulnDescRecSetIndicatorPtr(sDescRecApd, aIndicator);

    /*
     * Exit
     */
    sNeedExit = ACP_FALSE;
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "BindFileToParam");
    }
    ACI_EXCEPTION(IPCDANotSupport)
    {
        ulnError(&sFnContext, ulERR_ABORT_IPCDA_UNSUPPORTED_FUNCTION);
    }
    ACI_EXCEPTION_END;

    if (sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
