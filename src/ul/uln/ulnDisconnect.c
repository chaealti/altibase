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
#include <ulnDisconnect.h>

/*
 * ULN_SFID_67
 * SQLDisconnect(), DBC �������� �Լ� : C3, C4, C5
 *
 * ��� �����ÿ� C2 �� ������.
 */
ACI_RC ulnSFID_67(ulnFnContext *aFnContext)
{
    acp_list_node_t *sIterator;
    ulnStmt         *sStmt;
    ulnDbc          *sDbc;

    sDbc = aFnContext->mHandle.mDbc;

    if(aFnContext->mWhere == ULN_STATE_ENTRY_POINT)
    {
        /*
         * �� �� : �ϴ� ������ ��� STMT �� ���µ� ���� �Ѵ�.
         *             �ϳ��� ��� ���ϸ� ��� ���ϴ� �Ŵ�.
         */

        ACP_LIST_ITERATE(&(sDbc->mStmtList), sIterator)
        {
            /*
             * Note : �� ���� ����� Ư���� ���ν�, ������ STMT ���� ���¿� �Ű��� ���
             *        �Ѵ�. �ֳ��ϸ� disconnect �ϰ� ���� stmt �ڵ���� ��� free �� ���̱�
             *        �����̴�. �׷���, ������ stmt �� ���ؼ� ���¸ӽ��� �θ� ���� ���� �븩�̴�.
             *        �ֳ��ϸ� ���� ������ context �� mHandle �� DBC �̱� �����̴�. context ��
             *        �ϳ� �� ������ ���� ���� �븩�̰�... �׷��ٰ�, SQLFreeHandle() �� ���ó��
             *        ������ ���� �ڵ��� ���¸� �����ϸ� �Ǵ� �͵� �ƴϴ� ���̴�.
             *        �ϴ� ������ ���� stmt �� ���¸� üũ�ؼ� �ϳ��� �ƴϸ� ���� ����
             *        �����ϵ���, ��, �� �̻� �Լ��� �������� ���ϵ��� �Ͽ���.
             */
            sStmt = (ulnStmt *)sIterator;
            ACI_TEST_RAISE(ULN_OBJ_GET_STATE(sStmt) >= ULN_S_S8, LABEL_HY010);
        }
    }
    else
    {
        /*
         * �������� �� : SQL_SUCCESS �� ������ ��� ���� ���̸� �Ѵ�.
         */
        if(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext)) != 0)
        {
            ULN_OBJ_SET_STATE(sDbc, ULN_S_C2);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_HY010)
    {
        /*
         * HY010
         */
        ulnError(aFnContext, ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnCallbackDisconnectResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext )
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

static ACI_RC ulnDisconnSendDisconnectReq(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    acp_uint16_t        sState = 0;
    cmiProtocol         sPacket;
    cmiProtocolContext *sCtx            = &(aPtContext->mCmiPtContext);
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);

    sPacket.mOpID = CMP_OP_DB_Disconnect;

    CMI_WRITE_CHECK(sCtx, 2);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_Disconnect);
    CMI_WR1(sCtx, CMP_DB_DISCONN_CLOSE_SESSION);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

static ACI_RC ulnDisconnReceiveDisconnectRes(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc *sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * Ÿ�Ӿƿ� ����
     */
    if (cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST(ulnReadProtocolIPCDA(aFnContext,
                                      aPtContext,
                                      sDbc->mConnTimeoutValue) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(ulnReadProtocol(aFnContext,
                                 aPtContext,
                                 sDbc->mConnTimeoutValue) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDisconnDoLogicalDisconnect(ulnFnContext *aFnContext)
{
    acp_bool_t  sNeedPtFin = ACP_FALSE;
    ulnDbc     *sDbc;

    sDbc = aFnContext->mHandle.mDbc;

    if(ulnDbcIsConnected(sDbc) == ACP_TRUE)
    {
        /*
         * protocol context �� �ʱ�ȭ�Ѵ�.
         */
        //fix BUG-17722
        ACI_TEST(ulnInitializeProtocolContext(aFnContext,
                                              &(sDbc->mPtContext),
                                              &(sDbc->mSession))
                 != ACI_SUCCESS);
        sNeedPtFin = ACP_TRUE;

        /*
         * disconnect req ����
         *
         * BUG-16724 Disconnect �ÿ� ������ �׾� �־ �����ؾ� �Ѵ�.
         *           --> ACI_TEST() �� ���ּ� ������ ���� ��� ����.
         */
        //fix BUG-17722
        if (ulnDisconnSendDisconnectReq(aFnContext,&(sDbc->mPtContext)) == ACI_SUCCESS)
        {
            /*
             * BUGBUG : ulnDisconnSendDisconnectReq() �Լ��� ������ �ݵ��
             *          communication link failure ��� ���� ����.
             *          �ϴ�, ����� �ٻڹǷ� �̿Ͱ��� ó���ϰ� �Ѿ��.
             */

            /*
             * disconnect result ����
             */
            if (ulnDisconnReceiveDisconnectRes(aFnContext,&(sDbc->mPtContext))
                     != ACI_SUCCESS)
            {
                ACI_TEST_RAISE(ulnClearDiagnosticInfoFromObject(aFnContext->mHandle.mObj)
                               != ACI_SUCCESS,
                               LABEL_MEM_MAN_ERR);
                ULN_FNCONTEXT_SET_RC(aFnContext, SQL_SUCCESS);
            }
        }
        else
        {
            ACI_TEST_RAISE(ulnClearDiagnosticInfoFromObject(aFnContext->mHandle.mObj)
                           != ACI_SUCCESS,
                           LABEL_MEM_MAN_ERR);

            // BUG-24817
            // windows ���� ulnDisconnSendDisconnectReq �Լ��� �����մϴ�.
            // �����°��� �����߱� ������ �����͵� ����.
            sDbc->mPtContext.mNeedReadProtocol = 0;

            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_SUCCESS);
        }

        /*
         * protocol context ����
         */
        sNeedPtFin = ACP_FALSE;
        //fix BUG-17722
        ACI_TEST(ulnFinalizeProtocolContext(aFnContext,
                                           &(sDbc->mPtContext)) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnDisconnDoLogicalDisconnect");
    }

    ACI_EXCEPTION_END;

    if(sNeedPtFin == ACP_TRUE)
    {
        //fix BUG-17722
        ulnFinalizeProtocolContext(aFnContext, &(sDbc->mPtContext));
    }

    return ACI_FAILURE;
}

static ACI_RC ulnDisconnDoPhysicalDisconnect(ulnFnContext *aFnContext)
{
    ulnDbc *sDbc;

    sDbc = aFnContext->mHandle.mDbc;


    if(sDbc->mLink != NULL)
    {
        ACI_TEST_RAISE(ulnDbcFreeLink(sDbc) != ACI_SUCCESS, LABEL_CM_ERR);
    }

    ulnDbcSetIsConnected(sDbc, ACP_FALSE);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrHandleCmError(aFnContext, NULL);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * BUGBUG : �Ʒ� �� �Լ��� �־�� �� ������ ������� �� �� ������ ����.
 */
static ACI_RC ulnDisconnFreeAllDesc(ulnFnContext *aFnContext)
{
    ulnDbc          *sDbc;
    ulnDesc         *sDesc;
    acp_list_node_t *sIterator1;
    acp_list_node_t *sIterator2;

    sDbc = aFnContext->mHandle.mDbc;

    ACP_LIST_ITERATE_SAFE(&(sDbc->mDescList), sIterator1, sIterator2)
    {
        sDesc = (ulnDesc *)sIterator1;

        ACI_TEST(ulnFreeHandleDescBody(aFnContext, sDbc, sDesc) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDisconnFreeAllStmt(ulnFnContext *aFnContext)
{
    ulnDbc          *sDbc;
    ulnStmt         *sStmt;
    acp_list_node_t *sIterator1;
    acp_list_node_t *sIterator2;

    sDbc = aFnContext->mHandle.mDbc;

    ACP_LIST_ITERATE_SAFE(&(sDbc->mStmtList), sIterator1, sIterator2)
    {
        sStmt = (ulnStmt *)sIterator1;

        /*
         * Note : disconnect �ÿ� ������ stmt free request �� ������ �ʴ´�.
         *        disconnect �� �ϸ� ���������� �ڵ����� �ش� ���ῡ ���� stmt ����
         *        �����ϱ� ������ ���� ������ �ȴ�.
         *        ū ������ ������ ������ �������� �´�.
         */
        ACI_TEST(ulnFreeHandleStmtBody(aFnContext, sDbc, sStmt, ACP_FALSE) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

SQLRETURN ulnDisconnect(ulnDbc *aDbc)
{
    acp_bool_t   sNeedExit = ACP_FALSE;
    ulnFnContext sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_DISCONNECT, aDbc, ULN_OBJ_TYPE_DBC);

    /*
     * �Լ� ����
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;

    ACI_TEST_RAISE(aDbc->mXaEnlist == ACP_TRUE, LABEL_SUCCESS_RETURN);

    /*
     * ���� üũ : ulnDbc �� ulnEnter() �Լ����� üũ��. ���� ������ �ʿ� ����.
     */

    /*
     * �����ִ� STMT �ڵ� ����
     * Note : ���Խ� ���¸ӽſ��� stmt ���� üũ �� �����Ƿ� �������� �����ص� �ȴ�.
     *        ��� ���� stmt �� SQLFreeHandle() �� ȣ�� �� �� �ִ� ���¿� �ִ�.
     */
    ACI_TEST(ulnDisconnFreeAllStmt(&sFnContext) != ACI_SUCCESS);

    /*
     * �����ִ� DESC ����
     */
    ACI_TEST(ulnDisconnFreeAllDesc(&sFnContext) != ACI_SUCCESS);

    /*
     * Note : disconnect �� stmt �������� �ʰ� �ϴ� ������ stmt �����ϸ鼭 ������ stmt free ��
     *        ������ �����̴�.
     *
     * ���� ���� (DB Session)
     */
    ACI_TEST(ulnDisconnDoLogicalDisconnect(&sFnContext) != ACI_SUCCESS);

    /*
     * ��ũ ����
     */
    ACI_TEST(ulnDisconnDoPhysicalDisconnect(&sFnContext) != ACI_SUCCESS);

    /*
     * �Լ� Ż��
     */
    sNeedExit = ACP_FALSE;

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s|", "ulnDisconnect");

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_SUCCESS_RETURN);
    {
        ulnExit(&sFnContext);
        return ACI_SUCCESS;
    }

    ACI_EXCEPTION_END;

    if(sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| fail", "ulnDisconnect");

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

}

/* ulnDisconnectLocal
 *
 * ����:
 *  REQ_SendDisconnect�� �����ϰ� �̰Ϳ� ���� ���� ����� ������� �ʴ´�.
 *  ���忡�� �ڵ�����Ϳ� �ϳ��� Ŭ���̾�Ʈ���� ���ǿ� ���� ���� ������ ������
 *  ������ ������ �� �ֽ��ϴ�. �� ������ ��� ���� �߿��� �������� �Ͱ� ����������
 *  ���� ������ �� �ֽ��ϴ�. �̰Ϳ� ���ؼ� ��Ÿ������ �����ͳ�忡���� ���信
 *  ������� ���� ������ ������ �����ϴ�.
 *
 *  Return : SQL_SUCCESS or Error
 */
SQLRETURN ulnDisconnectLocal(ulnDbc *aDbc)
{
    acp_bool_t   sNeedExit = ACP_FALSE;
    ulnFnContext sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_DISCONNECT, aDbc, ULN_OBJ_TYPE_DBC);

    /*
     * �Լ� ����
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;

    ACI_TEST_RAISE(aDbc->mXaEnlist == ACP_TRUE, LABEL_SUCCESS_RETURN);

    /*
     * ���� üũ : ulnDbc �� ulnEnter() �Լ����� üũ��. ���� ������ �ʿ� ����.
     */

    /*
     * �����ִ� STMT �ڵ� ����
     * Note : ���Խ� ���¸ӽſ��� stmt ���� üũ �� �����Ƿ� �������� �����ص� �ȴ�.
     *        ��� ���� stmt �� SQLFreeHandle() �� ȣ�� �� �� �ִ� ���¿� �ִ�.
     */
    ACI_TEST(ulnDisconnFreeAllStmt(&sFnContext) != ACI_SUCCESS);

    /*
     * �����ִ� DESC ����
     */
    ACI_TEST(ulnDisconnFreeAllDesc(&sFnContext) != ACI_SUCCESS);

    /*
     * Note : disconnect �� stmt �������� �ʰ� �ϴ� ������ stmt �����ϸ鼭 ������ stmt free ��
     *        ������ �����̴�.
     *
     * ���� ���� (DB Session)
     */
    (void)ulnDisconnDoLogicalDisconnect(&sFnContext);

    /*
     * ��ũ ����
     */
    ACI_TEST(ulnDisconnDoPhysicalDisconnect(&sFnContext) != ACI_SUCCESS);

    /*
     * �Լ� Ż��
     */
    sNeedExit = ACP_FALSE;

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s|", "ulnDisconnect");

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_SUCCESS_RETURN);
    {
        (void)ulnExit(&sFnContext);
        return ACI_SUCCESS;
    }

    ACI_EXCEPTION_END;

    if ( sNeedExit == ACP_TRUE )
    {
        (void)ulnExit(&sFnContext);
    }
    else
    {
        /* Nothing to do */
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| fail", "ulnDisconnect");

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

}
