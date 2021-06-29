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
#include <ulnLob.h>

/*
 * ex) LOB_LOCATOR �� ���ε��Ѵ�.
 *     execute �� �ϸ� LOCATOR �� ���� �� �ִ�.
 *     ( get length �� �Ѵ�. )
 *     put lob �� �̿��ؼ� ������ �κп� �����δ�
 *                         Ȥ�� offset ���� �ʰ� ó������ ����� �� �� ������.
 */
SQLRETURN ulnPutLob(acp_sint16_t  aHandleType,
                    ulnObject    *aObject,
                    acp_sint16_t  aLocatorCType,
                    acp_uint64_t  aLocator,
                    acp_uint32_t  aFromPosition,
                    acp_uint32_t  aForLength,
                    acp_sint16_t  aSourceCType,
                    void         *aBuffer,
                    acp_uint32_t  aBufferSize)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);

    ulnFnContext  sFnContext;
    ulnPtContext *sPtContext = NULL;

    ulnDbc       *sDbc;
    ulnLob        sLob;
    ulnLobBuffer  sLobBuffer;
    ulnMTypeID    sMTYPE;
    ulnCTypeID    sCTYPE;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_PUTLOB, aObject, aHandleType);

    /*
     * Enter
     */

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);

    ULN_FNCONTEXT_GET_DBC(&sFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ULN_FLAG_UP(sNeedExit);

    sPtContext = &(sDbc->mPtContext);
    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    ACI_TEST_RAISE(cmiGetLinkImpl(&sPtContext->mCmiPtContext) == CMI_LINK_IMPL_IPCDA,
                   IPCDANotSupport);
    /*
     * BUGBUG : ����� LOB �������� ũ�⸦ �Ѿ offset �� �ԷµǸ� ������ ����
     *          aFromPosition + aLength > ID_UINT_MAX --> ERROR!!!
     *          ó���� ... �ٱ�, ����.. -_-;; �������� ó���ϰ� �� �ѱ�...
     */

    ACI_TEST_RAISE(aBuffer == NULL, LABEL_INVALID_NULL);

    switch (aLocatorCType)
    {
        case SQL_C_CLOB_LOCATOR:
            sMTYPE = ULN_MTYPE_CLOB;
            break;

        case SQL_C_BLOB_LOCATOR:
            sMTYPE = ULN_MTYPE_BLOB;
            break;

        default:
            ACI_RAISE(LABEL_INVALID_LOCATOR_TYPE);
    }

    switch (aSourceCType)
    {
        case SQL_C_CHAR:
            sCTYPE = ULN_CTYPE_CHAR;
            break;

        /* PROJ-2047 Strengthening LOB - Partial Converting */
        case SQL_C_WCHAR:
            sCTYPE = ULN_CTYPE_WCHAR;
            break;

        case SQL_C_BINARY:
            sCTYPE = ULN_CTYPE_BINARY;
            break;

        default:
            ACI_RAISE(LABEL_INVALID_BUFFER_TYPE);
    }

    /*
     * ulnLob ����ü �ʱ�ȭ
     */

    ulnLobInitialize(&sLob, sMTYPE);                        /* ULN_LOB_ST_INITIALIZED */
    sLob.mOp->mSetLocator(&sFnContext, &sLob, aLocator);    /* ULN_LOB_ST_LOCATOR */
    sLob.mState = ULN_LOB_ST_OPENED;                        /* ULN_LOB_ST_OPENED */

    /*
     * ulnLobBuffer �ʱ�ȭ �� �غ�
     */

    ulnLobBufferInitialize(&sLobBuffer,
                           sDbc,
                           sLob.mType,
                           sCTYPE,
                           (acp_uint8_t *)aBuffer,
                           aBufferSize);

    sLobBuffer.mOp->mPrepare(&sFnContext, &sLobBuffer);

    /*
     * LOB ������ ����
     */
    // fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(sDbc->mPtContext),
                                          &(sDbc->mSession)) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedFinPtContext);

    ACI_TEST(sLob.mOp->mUpdate(&sFnContext,
                               &(sDbc->mPtContext),
                               &sLob,
                               &sLobBuffer,
                               aFromPosition,
                               aForLength) != ACI_SUCCESS);

    /*
     * PROJ-2739 Client-side Sharding LOB
     *   for ulsdDoCallback
     */
    ACI_TEST( ulnFlushAndReadProtocol( &sFnContext,
                                  &(sDbc->mPtContext),
                                  sDbc->mConnTimeoutValue ) != ACI_SUCCESS );

    ULN_FLAG_DOWN(sNeedFinPtContext);
    // fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext,
                        &(sDbc->mPtContext)) != ACI_SUCCESS);

    /*
     * ulnLobBuffer ����
     */

    sLobBuffer.mOp->mFinalize(&sFnContext, &sLobBuffer);

    /*
     * Exit
     */

    ULN_FLAG_DOWN(sNeedExit);

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(&sFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_INVALID_NULL)
    {
        /* HY009 : invalid use of null pointer */
        ulnError(&sFnContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }

    ACI_EXCEPTION(LABEL_INVALID_BUFFER_TYPE)
    {
        ulnError(&sFnContext,
                 ulERR_ABORT_RESTRICTED_DATATYPE_VIOLATION,
                 aSourceCType,
                 (aLocatorCType == SQL_C_BLOB_LOCATOR) ? SQL_BLOB : SQL_CLOB);
    }
    ACI_EXCEPTION(LABEL_INVALID_LOCATOR_TYPE)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_APP_BUFFER_TYPE, aLocatorCType);
    }
    ACI_EXCEPTION(IPCDANotSupport)
    {
        ulnError(&sFnContext, ulERR_ABORT_IPCDA_UNSUPPORTED_FUNCTION);
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        // fix BUG-17722
        ulnFinalizeProtocolContext(&sFnContext,&(sDbc->mPtContext));
    }

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

