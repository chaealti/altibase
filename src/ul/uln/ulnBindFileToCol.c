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

static ACI_RC ulnBindFileToColCheckArgs(ulnFnContext  *aContext,
                                        acp_sint16_t   aColumnNumber,
                                        acp_char_t   **aFileNameArray,
                                        acp_uint32_t  *aFileOptionPtr,
                                        ulvSLen        aMaxFileNameLength,
                                        ulvSLen       *aValueLength)
{
    ACI_TEST_RAISE( aColumnNumber < 1, LABEL_INVALID_PARAMNUM );
    
    ACI_TEST_RAISE( (aFileNameArray == NULL || aFileOptionPtr == NULL || aValueLength == NULL), LABEL_INVALID_USE_OF_NULL );

    ACI_TEST_RAISE( aMaxFileNameLength < 0, LABEL_INVALID_BUFFER_LEN );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_PARAMNUM)
    {
        /*
         * Invalid descriptor index
         * 07009
         */
        ulnError(aContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aColumnNumber);
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
         * HY090
         */
        ulnError(aContext, ulERR_ABORT_INVALID_BUFFER_LEN, aMaxFileNameLength);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ����� ���Ͽ� ���� ������ ��� ARD record �� �����Ѵ�.
 *
 * fetch �� �ش� ������ open �Ͽ� ������ �����͸� ����.
 *
 * BUGBUG : descriptor record �� octet length ptr �� indicator ptr �� �ǹ̸� Ȯ���� �ٽ��ѹ�
 *          �����ؾ� �Ѵ�.
 */
SQLRETURN ulnBindFileToCol(ulnStmt      *aStmt,
                           acp_sint16_t  aColumnNumber,
                           acp_char_t  **aFileNameArray,
                           ulvSLen      *aFileNameLengthArray,
                           acp_uint32_t *aFileOptionPtr,
                           ulvSLen       aMaxFileNameLength,
                           ulvSLen *     aValueLength)
{
    acp_bool_t    sNeedExit    = ACP_FALSE;
    ulnFnContext  sFnContext;
    ulnPtContext *sPtContext   = NULL;
    ulnDescRec   *sDescRecArd;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_BINDFILETOCOL, aStmt, ULN_OBJ_TYPE_STMT);

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
    ACI_TEST(ulnBindFileToColCheckArgs(&sFnContext,
                                       aColumnNumber,
                                       aFileNameArray,
                                       aFileOptionPtr,
                                       aMaxFileNameLength,
                                       aValueLength) != ACI_SUCCESS);

    /*
     * ARD record �غ�
     */
    ACI_TEST_RAISE(ulnBindArrangeNewDescRec(aStmt->mAttrArd, aColumnNumber, &sDescRecArd)
                   != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    /*
     * Note : aStrLenOrIndPtr �� aFileNameLengthArray �� �����Ѵٴµ��� �����Ѵ�.
     */
    ACI_TEST(ulnBindColBody(&sFnContext,
                            sDescRecArd,
                            SQL_C_FILE,
                            (void *)aFileNameArray,
                            aMaxFileNameLength,
                            aFileNameLengthArray) != ACI_SUCCESS);

    /*
     * file option �迭�� indicator �迭�� ����.
     */
    sDescRecArd->mFileOptionsPtr = aFileOptionPtr;
    ulnDescRecSetOctetLengthPtr(sDescRecArd, aValueLength);
    ulnDescRecSetIndicatorPtr(sDescRecArd, aValueLength);

    /*
     * Exit
     */
    sNeedExit = ACP_FALSE;
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnBindFileToCol");
    }
    ACI_EXCEPTION(IPCDANotSupport)
    {
        ulnError(&sFnContext, ulERR_ABORT_IPCDA_UNSUPPORTED_FUNCTION);
    }
    ACI_EXCEPTION_END;

    if(sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

