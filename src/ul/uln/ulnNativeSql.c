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
#include <ulnEscape.h>
#include <ulnCharSet.h>

ACI_RC ulnNativeSqlCheckArgs(ulnFnContext *aFnContext,
                             acp_char_t   *aInputStatement,
                             acp_sint32_t  aInputLength,
                             acp_char_t   *aOutputStatement,
                             acp_sint32_t  aOutputBufferSize)
{
    ACI_TEST_RAISE(aInputStatement  == NULL, LABEL_INVALID_NULL);

    if (aInputLength < 0)
    {
        ACI_TEST_RAISE(aInputLength != SQL_NTS, LABEL_INVALID_BUFFER_LENGTH);
    }

    if (aOutputBufferSize < 0)
    {
        /*
         * Note : DB2 cli �� M$ odbc �� ������ �ٸ���.
         *        DB2 CLI �� aOutputStatement �� NULL �̸� Invalid use of null pointer ����.
         *        M$ odbc �� �Ʒ�ó�� �����Ѵ� :
         *          The argument BufferLength was less than 0 and the argument 
         *          OutStatementText was not a null pointer.
         *        �� ����, OutStatementText �� NULL �̾ �ȴٴ� ���� �����ϰ� �ִ�.
         *
         *        �Ƹ���, ���⿡ NULL �� �� �Ŀ� ���ϵǴ� aOutputLength �� ����
         *        malloc �� �� �Ŀ� �ٽ� ȣ���ϴ� �������� ����ϱ� ���ؼ�
         *        ���� ������ ������ ����� �� �� �ϴ�.
         *
         *        ���� ������ �ߴ� -_-;;;
         */
        ACI_TEST_RAISE(aOutputStatement != NULL, LABEL_INVALID_BUFFER_LENGTH);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_NULL)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }

    ACI_EXCEPTION(LABEL_INVALID_BUFFER_LENGTH)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_BUFFER_LEN, aInputLength);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

SQLRETURN ulnNativeSql(ulnDbc       *aDbc,
                       acp_char_t   *aInputStatement,
                       acp_sint32_t  aInputLength,
                       acp_char_t   *aOutputStatement,
                       acp_sint32_t  aOutputBufferSize,
                       acp_sint32_t *aOutputLength)
{
    ULN_FLAG(sNeedExit);

    ulnFnContext  sFnContext;
    ulnEscape     sEscape;
    acp_sint32_t  sInputLength;
    acp_sint32_t  sResultLength = 0;
    acp_char_t   *sResultString = NULL;
    ulnCharSet    sCharSetIn;
    ulnCharSet    sCharSetOut;
    acp_sint32_t  sState = 0;

    /* PROJ-1573 XA */
    ulnDbc       *sDbc;

    ulnEscapeInitialize(&sEscape);
    sState = 1;

    ulnCharSetInitialize(&sCharSetIn);
    ulnCharSetInitialize(&sCharSetOut);
    sState = 2;

    sDbc = ulnDbcSwitch(aDbc);

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_NATIVESQL, sDbc, ULN_OBJ_TYPE_DBC);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /*
     * ===========================================
     * Function BEGIN
     * ===========================================
     */

    ACI_TEST(ulnNativeSqlCheckArgs(&sFnContext,
                                   aInputStatement,
                                   aInputLength,
                                   aOutputStatement,
                                   aOutputBufferSize) != ACI_SUCCESS);

    if (aInputLength == SQL_NTS)
    {
        sInputLength = acpCStrLen(aInputStatement, ACP_SINT32_MAX);
    }
    else
    {
        sInputLength = aInputLength;
    }

    ACI_TEST(ulnCharSetConvert(&sCharSetIn,
                               &sFnContext,
                               NULL,
                               sDbc->mClientCharsetLangModule,    //BUG-22684
                               (const mtlModule*)sDbc->mCharsetLangModule,
                               (void*)aInputStatement,
                               sInputLength,
                               CONV_DATA_IN)
             != ACI_SUCCESS);

    ACI_TEST_RAISE(ulnEscapeUnescapeByLen(&sEscape,
                                          (acp_char_t*)ulnCharSetGetConvertedText(&sCharSetIn),
                                          ulnCharSetGetConvertedTextLen(&sCharSetIn))
                   != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    ACI_TEST(ulnCharSetConvert(&sCharSetOut,
                               &sFnContext,
                               NULL,
                               (const mtlModule*)sDbc->mCharsetLangModule,
                               sDbc->mClientCharsetLangModule,    //BUG-22684
                               (void*)ulnEscapeUnescapedSql(&sEscape),
                               ulnEscapeUnescapedLen(&sEscape),
                               CONV_DATA_OUT)
             != ACI_SUCCESS);

    sResultLength = ulnCharSetGetConvertedTextLen(&sCharSetOut);

    if (aOutputLength != NULL)
    {
        *aOutputLength = sResultLength;
    }

    if (aOutputStatement != NULL)
    {
        sResultString = (acp_char_t*)ulnCharSetGetConvertedText(&sCharSetOut);     //BUG-28623 [CodeSonar]Ignored Return Value

        acpMemCpy(aOutputStatement, sResultString, aOutputBufferSize);

        if (aOutputBufferSize <= sResultLength)
        {
            *(aOutputStatement + aOutputBufferSize - 1) = 0;
        }
        else
        {
            *(aOutputStatement + sResultLength) = 0;
        }
    }

    if (aOutputBufferSize <= sResultLength)
    {
        ulnError(&sFnContext, ulERR_IGNORE_RIGHT_TRUNCATED);
    }

    /*
     * ===========================================
     * Function END
     * ===========================================
     */

    sState = 1;
    ulnCharSetFinalize(&sCharSetIn);
    ulnCharSetFinalize(&sCharSetOut);

    sState = 0;
    ulnEscapeFinalize(&sEscape);

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnPrepDoText");
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    switch(sState)
    {
        case 2:
            ulnCharSetFinalize(&sCharSetIn);
            ulnCharSetFinalize(&sCharSetOut);
        case 1:
            ulnEscapeFinalize(&sEscape);
        default:
            break;
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

