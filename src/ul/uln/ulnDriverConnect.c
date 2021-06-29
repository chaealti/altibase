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
#include <ulnConfigFile.h>
#include <ulnPrivate.h>
#include <ulnConnect.h>
#include <ulnDriverConnect.h>
#include <ulnConnectCore.h>
#include <ulnConnString.h>
#include <mmErrorCodeClient.h>
#include <ulsdDistTxInfo.h>

/*
 * ULN_SFID_68
 * SQLDriverConnect(), DBC �������� : C2 ����
 *      C4 [s]
 *      -- [nf]
 *  where
 *      [nf] Not found.
 *           The function returned SQL_NO_DATA.
 *           This does not apply when SQLExecDirect, SQLExecute, or SQLParamData
 *           returns SQL_NO_DATA after executing a searched update or delete statement.
 *
 *  Note : SQLDriverConnect() ���� ���̾�α� �ڽ��� ����µ�, ����ڰ� Cancel ��ư�� ������
 *         SQL_NO_DATA �� ���ϵȴ�.
 */

ACI_RC ulnSFID_68(ulnFnContext *aFnContext)
{
    if (aFnContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        if (ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS ||
           ULN_FNCONTEXT_GET_RC(aFnContext) == SQL_SUCCESS_WITH_INFO)
        {
            /* [s] */
            ULN_OBJ_SET_STATE(aFnContext->mHandle.mDbc, ULN_S_C4);
        }
    }

    return ACI_SUCCESS;
}

/* PROJ-2177 User Interface - Cancel */
/* BUG-38496 */
ACI_RC ulnCallbackDBConnectResult(cmiProtocolContext *aProtocolContext,
                                  cmiProtocol        *aProtocol,
                                  void               *aServiceSession,
                                  void               *aUserContext)
{
    ulnFnContext           *sFnContext  = (ulnFnContext *)aUserContext;
    ulnDbc                 *sDbc        = sFnContext->mHandle.mDbc;
    acp_uint32_t            sSessionID;
    acp_uint32_t            sResultCode;
    acp_uint32_t            sResultInfo;
    acp_uint64_t            sSCN;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD4(aProtocolContext, &sSessionID);
    CMI_RD4(aProtocolContext, &sResultCode);
    CMI_RD4(aProtocolContext, &sResultInfo);
    CMI_RD8(aProtocolContext, &sSCN);  /* PROJ-2733-Protocol */

    ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sDbc) != ULN_OBJ_TYPE_DBC, LABEL_MEM_MANAGE_ERR);

    /* ���� CID�� ��������°� ���� ���ؼ� SessionID(server side)�� �޾� �����Ѵ�. */
    sDbc->mSessionID  = sSessionID;
    sDbc->mNextCIDSeq = 0;
    ulnDbcInitUsingCIDSeq(sDbc);

    if (sSCN > 0)
    {
        ulsdUpdateSCN(sDbc, &sSCN);  /* PROJ-2733-DistTxInfo */
    }

    /*
     * proj_2160 cm_type removal
     * server sets sessionID at the packet
     * in mmtServiceThread::connectProtocol()
     */
    aProtocolContext->mWriteHeader.mA7.mSessionID =
        aProtocolContext->mReadHeader.mA7.mSessionID;

    /* BUG-38496 Notify users when their password expiry date is approaching */
    if(ACI_E_ERROR_CODE(sResultCode) ==
       ACI_E_ERROR_CODE(mmERR_IGNORE_PASSWORD_GRACE_PERIOD))
    {
        ulnError(sFnContext,
                 ulERR_IGNORE_PASSWORD_GRACE_PERIOD, 
                 sResultInfo);
        ULN_FNCONTEXT_SET_RC(sFnContext, SQL_SUCCESS_WITH_INFO);
    }
    else
    {
        /* do nothing */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "CallbackConnectExResult, Object is not a DBC handle.");
    }
    ACI_EXCEPTION_END;

    /* CM �ݹ� �Լ��� communication error�� �ƴ� �� ACI_SUCCESS�� ��ȯ�ؾ� �Ѵ�.
     * �ݹ� ������ function context�� ������ ������ �Ǵ��Ѵ�. */
    return ACI_SUCCESS;
}

static ACI_RC ulnDrvConnCheckArgs(ulnFnContext *aFnContext, acp_char_t *aText, acp_sint16_t aLength)
{

    ACI_TEST_RAISE( aText == NULL, LABEL_INVALID_USE_OF_NULL );
     
    ACI_TEST_RAISE( ulnCheckStringLength(aLength, 0) != ACI_SUCCESS, LABEL_INVALID_BUFFER_LEN );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_USE_OF_NULL)
    {
        /*
         * HY009 : Invalide Use of Null Pointer
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    ACI_EXCEPTION(LABEL_INVALID_BUFFER_LEN)
    {
        /*
         * HY090 : Invalid string of buffer length
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_BUFFER_LEN, aLength);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

SQLRETURN ulnDriverConnect(ulnDbc       *aDbc,
                           acp_char_t   *aConnString,
                           acp_sint16_t  aConnStringLength,
                           acp_char_t   *aOutConnStr,
                           acp_sint16_t  aOutBufferLength,
                           acp_sint16_t *aOutConnectionStringLength)
{
    ULN_FLAG(sNeedExit);

    ulnFnContext   sFnContext;
    //PROJ-1645 UL Fail-Over.
    ulnDataSource *sDataSource;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_DRIVERCONNECT, aDbc, ULN_OBJ_TYPE_DBC);

    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedExit);

    /* PROJ-1573 XA */
    ACI_TEST_RAISE(aDbc->mXaEnlist == ACP_TRUE, LABEL_SUCCESS_RETURN);

    /*
     * =====================
     * Function BEGIN
     * =====================
     */

    /* BUG-36256 Improve property's communication. */
    ulnConnAttrArrReInit(&aDbc->mUnsupportedProperties);

    ACI_TEST(ulnDrvConnCheckArgs(&sFnContext, aConnString, aConnStringLength)
             != ACI_SUCCESS);

    if (aConnStringLength == SQL_NTS)
    {
        aConnStringLength = acpCStrLen(aConnString, ACP_SINT32_MAX);
    }
    else
    {
        aConnStringLength = acpCStrLen(aConnString, aConnStringLength);
    }

    //PROJ-1645 UL-FailOver DSN�� �����Ѵ�.
    ACI_TEST(ulnSetDSNByConnString(&sFnContext,
                                   aConnString,
                                   aConnStringLength) != ACI_SUCCESS);

    /* ALTIBASE_CLI.ini,  ODBC.ini, Connection String�� �켱����
       �� ������ ����.
       Connection String > ODBC.ini > ALTIBASE_CLI.ini    */

    //PROJ-1645 UL Fail-Over.
    sDataSource  = ulnConfigFileGetDataSource(ulnDbcGetDsnString(aDbc));

    if(sDataSource != NULL)
    {
        ACI_TEST(ulnDataSourceSetConnAttr(&sFnContext,sDataSource) != ACI_SUCCESS);
    }
    /*
     * User Profile (odbc.ini, registry ��) �� �̿��� DBC Attribute ����
     *
     * cli �� ���� �� ��� �ƹ��͵� ���ϴ� ���� �Լ� ȣ��
     */

    ACI_TEST(ulnSetConnAttrByProfileFunc(&sFnContext,
                                         ulnDbcGetDsnString(aDbc),
                                         (acp_char_t *)"ODBC.INI") != ACI_SUCCESS);

    /*
     * connection string �� �̿��� DBC Attribute ����
     */

    ACI_TEST(ulnSetConnAttrByConnString(&sFnContext,
                                        aConnString,
                                        aConnStringLength) != ACI_SUCCESS);

#ifdef COMPILE_SHARDCLI
    /* BUG-45707 */
    ulsdDbcSetShardCli( aDbc, ULSD_SHARD_CLIENT_TRUE );

    aDbc->mShardDbcCxt.mTargetShardMetaNumber = 0UL;

    ulnDbcSetShardMetaNumber( aDbc, 0 );

    /* BUG-47272 */
    if ( aDbc->mShardDbcCxt.mShardSessionType == ULSD_SESSION_TYPE_LIB )
    {
        ACI_TEST( ulsdSetConnAttrForLibConn( &sFnContext ) != ACI_SUCCESS );
    }
#endif

    ulsdInitDistTxInfo(aDbc);  /* PROJ-2733-DistTxInfo */

    /* TASK-7219 Non-shard DML */
    ulnDbcInitStmtExecSeqForShardTx(aDbc);

    /*
     * ����ڿ��� full connection string �� �������ؼ� ����
     */
    ACI_TEST(ulnConnStrBuildOutConnString(&sFnContext,
                                          aOutConnStr,
                                          aOutBufferLength,
                                          aOutConnectionStringLength) != ACI_SUCCESS);

    ACI_TEST( ulnFailoverBuildServerList(&sFnContext) != ACI_SUCCESS );

    ACI_TEST( ulnConnectCore(aDbc, &sFnContext) != ACI_SUCCESS );

#ifdef COMPILE_SHARDCLI
    // shard connection ���н�
    // sFnContext �� SQL_SUCCESS_WITH_INFO �� �����ϰ� SQL_ERROR �� ��ȯ
    ACI_TEST( !SQL_SUCCEEDED( ulsdModuleNodeDriverConnect( aDbc,
                                                           &sFnContext,
                                                           aConnString,
                                                           aConnStringLength ) ) );
#endif /* COMPILE_SHARDCLI */

    /*
     * =====================
     * Function END
     * =====================
     */

    ULN_FLAG_DOWN(sNeedExit);

    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s|\n [%s]", "ulnDriverConnect", aConnString);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_SUCCESS_RETURN);
    {
        (void)ulnExit(&sFnContext);
        return ACI_SUCCESS;
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| fail\n [%s]", "ulnDriverConnect", aConnString);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

