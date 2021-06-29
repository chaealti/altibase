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
#include <ulnError.h>
#include <ulnErrorDef.h>
#include <ulsdnFailover.h>
#include <aciErrorMgr.h>
#include <ulsdnTrans.h>
#include <idErrorCodeClient.h>
#include <smErrorCodeClient.h>
#include <mtErrorCodeClient.h>
#include <mmErrorCodeClient.h>
#include <cmErrorCodeClient.h>
#include <qcuErrorClient.h>
#include <ulsdDistTxInfo.h>

/*
 * Note: ODBC3 / ODBC2 ������ �Ź� �Ͼ�� ���� �ƴ϶�
 * ����ڰ� GetDiagRec() �� ȣ���Ҷ��� �ص� �ǰڳ�
 */

aci_client_error_factory_t gUlnErrorFactory[] =
{
#include "E_UL_US7ASCII.c"
};

static aci_client_error_factory_t *gClientFactory[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    gUlnErrorFactory,
    NULL,
    NULL,
    NULL,
    NULL // gUtErrorFactory,
};

/*
 * �Լ��̸� ���� ������ �ȵ��.
 * �׷��ٰ� ���� ������ �̸��� �������� �͵� �ƴϰ�... _-;
 */
acp_char_t *ulnErrorMgrGetSQLSTATE_Server(acp_uint32_t aServerErrorCode)
{
    acp_uint32_t i;

    for (i = 0; gUlnServerErrorSQLSTATEMap[i].mErrorCode != 0; i++)
    {
        if (ACI_E_ERROR_CODE(gUlnServerErrorSQLSTATEMap[i].mErrorCode) ==
            ACI_E_ERROR_CODE(aServerErrorCode))
        {
            return (acp_char_t *)(gUlnServerErrorSQLSTATEMap[i].mSQLSTATE);
        }
    }

    return (acp_char_t *)"HY000";
}

static void ulnErrorMgrSetSQLSTATE(ulnErrorMgr  *aManager, acp_char_t *aSQLSTATE)
{
    acpSnprintf(aManager->mErrorState, SQL_SQLSTATE_SIZE + 1, "%s", aSQLSTATE);
}

static void ulnErrorMgrSetErrorCode(ulnErrorMgr  *aManager, acp_uint32_t aErrorCode)
{
    aManager->mErrorCode = aErrorCode;
}

/*
 * �Լ� �̸� ���� ������ �ȵ�� -_-;;
 */
static ACI_RC ulnErrorMgrSetErrorMessage(ulnErrorMgr  *aManager, acp_uint8_t* aVariable, acp_uint32_t aLen)
{
    acp_uint32_t sErrorMsgSize;

    /*
     * NULL terminate
     */
    if (aLen >= ACI_SIZEOF(aManager->mErrorMessage))
    {
        sErrorMsgSize = ACI_SIZEOF(aManager->mErrorMessage) - 1;
    }
    else
    {
        sErrorMsgSize = aLen;
    }

    acpMemCpy(aManager->mErrorMessage, aVariable, sErrorMsgSize);

    aManager->mErrorMessage[sErrorMsgSize] = '\0';

    return ACI_SUCCESS;
}

/*
 * ���� : �Ʒ� �Լ� ulnErrorMgrSetUlErrorVA() (�Լ��̸��� ���� �ϳ� -_-);;
 *        ������ �����ڵ常 ������
 *        SQLSTATE, ErrorMessage ���� ������ ������.
 */
void ulnErrorMgrSetUlErrorVA( ulnErrorMgr  *aErrorMgr,
                              acp_uint32_t  aErrorCode,
                              va_list       aArgs )
{
    acp_uint32_t                sSection;
    aci_client_error_factory_t *sCurFactory;

    sSection = (aErrorCode & ACI_E_MODULE_MASK) >> 28;

    sCurFactory = gClientFactory[sSection];

    aciSetClientErrorCode(aErrorMgr,
                          sCurFactory,
                          aErrorCode,
                          aArgs);
}

void ulnErrorMgrSetUlError( ulnErrorMgr *aErrorMgr, acp_uint32_t aErrorCode, ...)
{
    va_list sArgs;

    va_start(sArgs, aErrorCode);
    ulnErrorMgrSetUlErrorVA(aErrorMgr, aErrorCode, sArgs);
    va_end(sArgs);
}

SQLRETURN ulnErrorDecideSqlReturnCode(acp_char_t *aSqlState)
{
    if (aSqlState[0] == '0')
    {
        /* PROJ-1789 Updatable Scrollable Cursor
         * SQL_NO_DATA�� Fetch ����� ���� ���� �������ش�.
         * ���⼭�� �ٸ� DB�� ó�� '02*'�� SUCCESS_WITH_INFO�� �����Ѵ�. */
        switch (aSqlState[1])
        {
            case '0':
                return SQL_SUCCESS;
                break;
            case '1':
            case '2':
                return SQL_SUCCESS_WITH_INFO;
                break;
            default :
                break;
        }
    }

    return SQL_ERROR;
}

static void ulnErrorBuildDiagRec(ulnDiagRec   *aDiagRec,
                                 ulnErrorMgr  *aErrorMgr,
                                 acp_sint32_t  aRowNumber,
                                 acp_sint32_t  aColumnNumber)
{
    /*
     * �޼��� �ؽ�Ʈ ����
     * BUGBUG : �޸� ������Ȳ ó��
     */
    ulnDiagRecSetMessageText(aDiagRec, ulnErrorMgrGetErrorMessage(aErrorMgr));

    /*
     * SQLSTATE ����
     */
    ulnDiagRecSetSqlState(aDiagRec, ulnErrorMgrGetSQLSTATE(aErrorMgr));

    /*
     * NativeErrorCode ����
     */
    ulnDiagRecSetNativeErrorCode(aDiagRec, ulnErrorMgrGetErrorCode(aErrorMgr));

    /*
     * RowNumber, ColumnNumber ����
     */
    ulnDiagRecSetRowNumber(aDiagRec, aRowNumber);
    ulnDiagRecSetColumnNumber(aDiagRec, aColumnNumber);
}

static ACI_RC ulnErrorAddDiagRec(ulnFnContext *aFnContext,
                                 ulnErrorMgr  *aErrorMgr,
                                 acp_sint32_t  aRowNumber,
                                 acp_sint32_t  aColumnNumber)
{
    ulnDiagRec    *sDiagRec    = NULL;
    ulnDiagHeader *sDiagHeader;

    if (aFnContext->mHandle.mObj != NULL)
    {
        if (ulnErrorMgrGetErrorCode(aErrorMgr) == ulERR_ABORT_INVALID_HANDLE)
        {
            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
        }
        else
        {
            sDiagHeader = &aFnContext->mHandle.mObj->mDiagHeader;

            /*
             * Diagnostic Record ���� (�޸� �����ϸ� static record �����)
             */
            ulnDiagRecCreate(sDiagHeader, &sDiagRec);
            /* BUG-36729 Connection attribute will be added to unlock client mutex by force */
            ACI_TEST(sDiagRec == NULL);

            ulnErrorBuildDiagRec(sDiagRec,
                                 aErrorMgr,
                                 aRowNumber,
                                 aColumnNumber);

            /*
             * Diagnostic Record �Ŵޱ�
             */
            ulnDiagHeaderAddDiagRec(sDiagHeader, sDiagRec);

            /*
             * DiagHeader �� SQLRETURN ����.
             * BUGBUG : �Ʒ��� �Լ�(ulnDiagSetReturnCode) �̻��ϴ� -_-;; �ٽ� �ѹ� ����.
             */
            // ulnDiagSetReturnCode(sDiagHeader, ulnErrorDecideSqlReturnCode(sDiagRec->mSQLSTATE));
            ULN_FNCONTEXT_SET_RC(aFnContext, ulnErrorDecideSqlReturnCode(sDiagRec->mSQLSTATE));

            /* BUG-46092 */
            if ( ulnErrorMgrGetErrorCode( aErrorMgr )
                 == ACI_E_ERROR_CODE( ulERR_ABORT_FAILOVER_SUCCESS ) )
            {
                aFnContext->mIsFailoverSuccess = ACP_TRUE;
            }

#if COMPILE_SHARDCLI
            /* TASK-7218 Multi-Error Handling 2nd */
            ulsdMultiErrorAdd(aFnContext, sDiagRec);
#endif
        }

        ULN_TRACE_LOG(aFnContext, ULN_TRACELOG_LOW, NULL, 0,
                "%-18s| row: %"ACI_INT32_FMT" col: %"ACI_INT32_FMT
                " code: 0x%08x\n msg [%s]",
                "ulnErrorAddDiagRec", aRowNumber, aColumnNumber,
                ulnErrorMgrGetErrorCode(aErrorMgr),
                ulnErrorMgrGetErrorMessage(aErrorMgr));
    }

    ACI_TEST(!(SQL_SUCCEEDED(ULN_FNCONTEXT_GET_RC(aFnContext))));

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnErrorHandleFetchError(ulnFnContext *aFnContext,
                                       ulnErrorMgr  *aErrorMgr,
                                       acp_sint32_t  aColumnNumber)
{
    ulnDiagHeader *sDiagHeader;
    ulnDiagRec    *sDiagRec = NULL;

    ulnCache      *sCache;

    ACI_TEST(aFnContext->mHandle.mObj == NULL);

    sCache = aFnContext->mHandle.mStmt->mCache;
    ACI_TEST(sCache == NULL);

    sDiagHeader = &aFnContext->mHandle.mObj->mDiagHeader;

    /*
     * DiagHeader �� not enough memory �������� static diag rec �� �̿��Ѵ�.
     */
    sDiagRec = &sDiagHeader->mStaticDiagRec;

    ulnErrorBuildDiagRec(sDiagRec,
                         aErrorMgr,
                         ulnCacheGetRowCount(sCache) + 1,
                         aColumnNumber);

    sCache->mServerError = sDiagRec;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnError(ulnFnContext *aFnContext, acp_uint32_t aErrorCode, ...)
{
    va_list     sArgs;
    ulnErrorMgr sErrorMgr;

    /*
     * BUGBUG : ulnErrorMgr ������ ���� �޼����� ���� ������
     *          MAX_ERROR_MSG_LEN + 256 ����Ʈ��ŭ
     *          static ���� ������ �ִ�. ���� �޼����� �󸶳� Ŀ���� �𸣴� ���¿��� ��ó��
     *          ���� ����� ������ ���� ��ĩ ������ �߸� ������ �ִ�.
     *          �Ӹ� �ƴ϶�, ��κ��� ���� �޼����� ©��©���ѵ�, ���� 2K �� �Ѵ� ������
     *          �ٷ� �Ҵ��Ѵٴ� ���� �Ƿ� �޸� ���� �ƴ� �� ����. ��� ���ÿ���������.
     *
     *          �� �κ��� ������ malloc() �� �ϵ��� �����ؾ� �Ѵ�.
     *          ������ �ٻڰ� �Ӹ� ������ �׳� ���� -_-;;
     */
    va_start(sArgs, aErrorCode);
    ulnErrorMgrSetUlErrorVA(&sErrorMgr, aErrorCode, sArgs);
    va_end(sArgs);

    if (aErrorCode != ulERR_IGNORE_NO_ERROR)
    {
        if (aErrorCode == ulERR_ABORT_INVALID_HANDLE)
        {
            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
            ACI_RAISE(LABEL_INVALID_HANDLE);
        }
        else
        {
            ACI_TEST(ulnErrorAddDiagRec(aFnContext,
                                        &sErrorMgr,
                                        SQL_NO_ROW_NUMBER,
                                        SQL_NO_COLUMN_NUMBER) != ACI_SUCCESS);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_HANDLE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * BUGBUG : �ڿ��ٰ� extended �� �ִϱ� �� M$ �Լ����� -_-;;;
 *          �� �ø��� ���� �ڿ������� �Ǵ� ����� ������ ����. -_-;;
 */
ACI_RC ulnErrorExtended(ulnFnContext *aFnContext,
                        acp_sint32_t  aRowNumber,
                        acp_sint32_t  aColumnNumber,
                        acp_uint32_t  aErrorCode, ...)
{
    va_list     sArgs;
    ulnErrorMgr sErrorMgr;

    /*
     * BUGBUG : ulnErrorMgr ������ ���� �޼����� ���� ������
     *          MAX_ERROR_MSG_LEN + 256 ����Ʈ��ŭ
     *          static ���� ������ �ִ�. ���� �޼����� �󸶳� Ŀ���� �𸣴� ���¿��� ��ó��
     *          ���� ����� ������ ���� ��ĩ ������ �߸� ������ �ִ�.
     *          �Ӹ� �ƴ϶�, ��κ��� ���� �޼����� ©��©���ѵ�, ���� 2K �� �Ѵ� ������
     *          �ٷ� �Ҵ��Ѵٴ� ���� �Ƿ� �޸� ���� �ƴ� �� ����. ��� ���ÿ���������.
     *
     *          �� �κ��� ������ malloc() �� �ϵ��� �����ؾ� �Ѵ�.
     *          ������ �ٻڰ� �Ӹ� ������ �׳� ���� -_-;;
     */
    va_start(sArgs, aErrorCode);
    ulnErrorMgrSetUlErrorVA(&sErrorMgr, aErrorCode, sArgs);
    va_end(sArgs);

    if (aErrorCode != ulERR_IGNORE_NO_ERROR)
    {
        if (aErrorCode == ulERR_ABORT_INVALID_HANDLE)
        {
            ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
            ACI_RAISE(LABEL_INVALID_HANDLE);
        }
        else
        {
            ACI_TEST(ulnErrorAddDiagRec(aFnContext,
                                        &sErrorMgr,
                                        aRowNumber,
                                        aColumnNumber) != ACI_SUCCESS);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_HANDLE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnCallbackErrorResult(cmiProtocolContext *aPtContext,
                              cmiProtocol        *aProtocol,
                              void               *aServiceSession,
                              void               *aUserContext)
{
    ulnFnContext        *sFnContext = (ulnFnContext *)aUserContext;
    ulnStmt             *sStmt;
    ulnErrorMgr          sErrorMgr;

    acp_uint8_t          sOperationID;
    acp_uint32_t         sErrorIndex;
    acp_uint32_t         sErrorCode;
    acp_uint16_t         sErrorMessageLen;
    acp_uint8_t         *sErrorMessage;
    acp_uint64_t         sSCN = 0;
    acp_uint32_t         sNodeId = 0;

    /* BUG-36256 Improve property's communication */
    ulnDbc              *sDbc = NULL;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD1(aPtContext, sOperationID);
    CMI_RD4(aPtContext, &sErrorIndex);
    CMI_RD4(aPtContext, &sErrorCode);
    CMI_RD2(aPtContext, &sErrorMessageLen);

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MANAGE_ERR);

    if (sErrorMessageLen > 0)
    {
        sErrorMessage = aPtContext->mReadBlock->mData + aPtContext->mReadBlock->mCursor;
        aPtContext->mReadBlock->mCursor += sErrorMessageLen;
    }
    else
    {
        sErrorMessage = NULL;
    }

    /* PROJ-2733-Protocol ������ Handshake �������� �б� ���� Error�� �� �� �ֳ�???
                          Error�� �� �� �ִٸ� ������ ���� ó���� �� �� �־�� �Ѵ�. */
    switch (aProtocol->mOpID)
    {
        case CMP_OP_DB_ErrorV3Result:
            CMI_RD8(aPtContext, &sSCN);
            CMI_RD4(aPtContext, &sNodeId); // TASK-7218
            break;

        case CMP_OP_DB_ErrorResult:
        default:
            break;
    }

    /* PROJ-2733-DistTxInfo */
    if (sSCN > 0)
    {
        ulsdUpdateSCN(sDbc, &sSCN);
    }

    if (CMI_IS_EXECUTE_GROUP(sOperationID) == ACP_TRUE)
    {
        /* bug-18246 */
        sStmt = sFnContext->mHandle.mStmt;

        if (ulnStmtGetStatementType(sStmt) == ULN_STMT_UPDATE ||
            ulnStmtGetStatementType(sStmt) == ULN_STMT_DELETE)
        {
            ulnStmtSetAttrParamStatusValue(sStmt, sErrorIndex - 1, SQL_PARAM_ERROR);
            ULN_FNCONTEXT_SET_RC((sFnContext), SQL_ERROR);
        }
    }
    /* BUG-36256 Improve property's communication */
    else if ( (CMI_IS_PROPERTY_SET_GROUP(sOperationID) == ACP_TRUE) ||
              (CMI_IS_PROPERTY_GET_GROUP(sOperationID) == ACP_TRUE) )
    {
        if (ACI_E_ERROR_CODE(sErrorCode) ==
            ACI_E_ERROR_CODE(mmERR_IGNORE_UNSUPPORTED_PROPERTY))
        {
            /* �������� �������� �ʴ� ������Ƽ�� Off �Ѵ�. */
            ACI_TEST(ulnSetConnectAttrOff(sFnContext,
                                          sDbc,
                                          sErrorIndex)
                     != ACI_SUCCESS);
        }
        else
        {
            /* Nothing */
        }
    }
    else
    {
        /* Nothing */
    }

    if ((ACI_E_ACTION_MASK & sErrorCode) != ACI_E_ACTION_IGNORE)
    {
        /*
         * ulnErrorMgr �� �����ڵ� ����
         */
        ulnErrorMgrSetErrorCode(&sErrorMgr, sErrorCode);

        /*
         * ulnErrorMgr �� SQLSTATE ����
         */
        ulnErrorMgrSetSQLSTATE(&sErrorMgr,
                               ulnErrorMgrGetSQLSTATE_Server(sErrorCode));

        /*
         * ulnErrorMgr �� ���� �޼��� ����
         */
        ACI_TEST_RAISE(ulnErrorMgrSetErrorMessage(&sErrorMgr, sErrorMessage, sErrorMessageLen)
                       != ACI_SUCCESS,
                       LABEL_MEM_MANAGE_ERR);

        /*
         * ���� FETCH REQ �� ���� �������, �ϴ� ����� ��Ű��, �׷��� ������ �Ϲ����� ����.
         * �̷��� �ϴ� ������ �뷫 ������ ����.
         *
         * 1. ExecDirect�� Prepare, Execute, Fetch�� �ѹ��� ������ ����� �ѹ��� �޴´�.
         *    ������ ������ϸ�, SELECT�� �����Ѱ� �ƴ� ��쿡 Fetch ����� ������ ��������.
         * 2. Array Fetch�� ���� ó�� ����� ���� �ٸ���.
         *    ������ �ִٰ� �ٷ� ��ȯ�ϸ� �ȵȴ�.
         *
         * ��, Fetch out of sequence�� ��ȿȭ�� Cursor�� Fetch�� �ؼ� �� �����̹Ƿ�
         * �׳� ������ ó���Ѵ�. (for PROJ-1381 FAC)
         */
        if ( (CMI_IS_FETCH_GROUP(sOperationID) == ACP_TRUE) &&
             (sErrorCode != mmERR_ABORT_FETCH_OUT_OF_SEQ) )
        {
            ACI_TEST_RAISE(ulnErrorHandleFetchError(sFnContext,
                                                    &sErrorMgr,
                                                    SQL_COLUMN_NUMBER_UNKNOWN)
                           != ACI_SUCCESS,
                           LABEL_MEM_MAN_ERR_DIAG);
        }
        else
        {
            /*
             * DiagRec �� ���� object �� ���̱�
             * BUGBUG : �Լ� �̸�, ����, scope ���� �̻��ϴ�.
             */
            ulnErrorAddDiagRec(sFnContext,
                               &sErrorMgr,
                               (sErrorIndex == 0) ? SQL_NO_ROW_NUMBER
                                                             : (acp_sint32_t)sErrorIndex,
                               SQL_COLUMN_NUMBER_UNKNOWN);

            if (ULN_OBJ_GET_TYPE(sFnContext->mHandle.mObj) == ULN_OBJ_TYPE_STMT)
            {
                if (sFnContext->mFuncID == ULN_FID_EXECUTE ||
                    sFnContext->mFuncID == ULN_FID_EXECDIRECT ||
                    sFnContext->mFuncID == ULN_FID_PARAMDATA)
                {
                    // BUG-21255
                    if (sErrorIndex > 0)
                    {
                        ulnExecProcessErrorResult(sFnContext, sErrorIndex);
                    }
                }
            }

#ifdef COMPILE_SHARDCLI /* BUG-46092 */
            if ( ( sErrorCode & ACI_E_MODULE_MASK ) == ACI_E_MODULE_SD )
            {
                ulsdErrorHandleShardingError( sFnContext, sNodeId );
            }
#endif /* COMPILE_SHARDCLI */
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR_DIAG)
    {
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnCallbackErrorResult : while registering fetch error");
    }

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "CallbackErrorResult");
    }

    ACI_EXCEPTION_END;

    /*
     * BUG �ƴ�. callback �̹Ƿ� ACI_SUCCESS �����ؾ� ��.
     */
    return ACI_SUCCESS;
}

void ulnErrorMgrSetCmError( ulnDbc       *aDbc,
                            ulnErrorMgr  *aErrorMgr,
                            acp_uint32_t  aCmErrorCode )
{
    switch (aCmErrorCode)
    {
        case cmERR_ABORT_CONNECT_ERROR:
        case cmERR_ABORT_IB_RCONNECT_ERROR: /* PROJ-2681 */
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_FATAL_FAIL_TO_ESTABLISH_CONNECTION,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;

        /* TASK-5894 Permit sysdba via IPC */
        case cmERR_ABORT_CMN_ERR_FULL_IPC_CHANNEL:
            /*
             * SYSDBA�� ���� �����ϸ� �̹� SYSDBA�� �������� ���� �ְ�,
             * Non-SYSDBA�� �������ӵ鿡 ���� ������ ���� ����� ���� �ִ�.
             * ä���� �Ҵ�ް� Connect ���������� ���۹޾ƾ� SYSDBA���� �ƴ���
             * �� �� �ֱ� ������ ��Ȯ�ϰ� ��Ȳ�� �� �� ����.
             * �Ϲ����� ���� �����ϰ� ADMIN_ALREADY_RUNNING ������ �ش�.
             */
            if (ulnDbcGetPrivilege(aDbc) == ULN_PRIVILEGE_SYSDBA)
            {
                ulnErrorMgrSetUlError( aErrorMgr,
                                       ulERR_ABORT_ADMIN_ALREADY_RUNNING );
            }
            else
            {
                ulnErrorMgrSetUlError( aErrorMgr,
                                       ulERR_FATAL_FAIL_TO_ESTABLISH_CONNECTION,
                                       aciGetErrorMsg(aCmErrorCode) );
            }
            break;

        case cmERR_ABORT_TIMED_OUT:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_CONNECTION_TIME_OUT );
            break;

        case cmERR_ABORT_CONNECTION_CLOSED:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_COMMUNICATION_LINK_FAILURE,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;

        case cmERR_ABORT_CALLBACK_DOES_NOT_EXIST:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_CM_CALLBACK_NOT_EXIST );
            break;

        /* bug-30835: support link-local address with zone index. */
        case cmERR_ABORT_GETADDRINFO_ERROR:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_GETADDRINFO_ERROR );
            break;

        case cmERR_ABORT_CONNECT_INVALIDARG:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_CONNECT_INVALIDARG );
            break;

        /* PROJ-2681 */
        case cmERR_ABORT_IB_RCONNECT_INVALIDARG:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_IB_RCONNECT_INVALIDARG );
            break;

        /* BUG-41330 Returning a detailed SSL error to the client */
        /* SQLState: 08S01 - Communications link failure */
        case cmERR_ABORT_SSL_READ:
        case cmERR_ABORT_SSL_WRITE:
        case cmERR_ABORT_SSL_CONNECT:
        case cmERR_ABORT_SSL_HANDSHAKE:
        case cmERR_ABORT_SSL_SHUTDOWN:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_SSL_LINK_FAILURE,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;

        /* SQLState: HY000 - General error */
        case cmERR_ABORT_INVALID_CERTIFICATE:
        case cmERR_ABORT_INVALID_PRIVATE_KEY:
        case cmERR_ABORT_PRIVATE_KEY_VERIFICATION:
        case cmERR_ABORT_INVALID_VERIFY_LOCATION:
        case cmERR_ABORT_SSL_OPERATION:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_SSL_OPERATION_FAILURE,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;

        case cmERR_ABORT_DLOPEN:
        case cmERR_ABORT_DLSYM:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_SSL_LIBRARY_ERROR,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;

#ifdef COMPILE_SHARDCLI
        /* PROJ-2658 altibase sharding */
        case cmERR_ABORT_SHARD_VERSION_MISMATCH:
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_SHARD_VERSION_MISMATCH );
            break;
#endif /* COMPILE_SHARDCLI */

        default:
            /* BUGBUG : ������ 08S01 �� ������� ���� �ƴ϶� �� �� �����ϰ� �ؾ� �Ѵ�. */
            ulnErrorMgrSetUlError( aErrorMgr,
                                   ulERR_ABORT_CM_GENERAL_ERROR,
                                   aciGetErrorMsg(aCmErrorCode) );
            break;
    }
}

ACI_RC ulnErrHandleCmError(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnErrorMgr   sErrorMgr;
    ulnDbc       *sDbc = NULL;
    acp_uint32_t  sCmErrorCode;

    sCmErrorCode = aciGetErrorCode();

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, LABEL_MEM_MAN_ERR);

    ulnErrorMgrSetCmError( sDbc, &sErrorMgr, sCmErrorCode );

    switch (sCmErrorCode)
    {
        case cmERR_ABORT_CONNECTION_CLOSED:
            if (aPtContext != NULL)
            {
                if (cmiGetLinkImpl(&(aPtContext->mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
                {
                    (void) cmiRecvIPCDA(&aPtContext->mCmiPtContext,
                                        aFnContext,
                                        ACP_TIME_INFINITE,
                                        sDbc->mIPCDAMicroSleepTime);
                }
                else
                {
                    (void) cmiRecv(&aPtContext->mCmiPtContext, aFnContext, ACP_TIME_INFINITE);
                }
            }
            else
            {
                /* do nothing */
            }

        case cmERR_ABORT_CONNECT_ERROR:
        case cmERR_ABORT_IB_RCONNECT_ERROR:          /* PROJ-2681*/
        case cmERR_ABORT_CMN_ERR_FULL_IPC_CHANNEL:
        case cmERR_ABORT_TIMED_OUT:
        case cmERR_ABORT_GETADDRINFO_ERROR:
        case cmERR_ABORT_CONNECT_INVALIDARG:
        case cmERR_ABORT_IB_RCONNECT_INVALIDARG:     /* PROJ-2681 */
        case cmERR_ABORT_SSL_READ:
        case cmERR_ABORT_SSL_WRITE:
        case cmERR_ABORT_SSL_CONNECT:
        case cmERR_ABORT_SSL_HANDSHAKE:
        case cmERR_ABORT_SSL_SHUTDOWN:
        case cmERR_ABORT_DLSYM:
        case cmERR_ABORT_DLOPEN:
        case cmERR_ABORT_SELECT_ERROR:               /* BUG-47714 */
            ulnDbcSetIsConnected(sDbc, ACP_FALSE);
            break;

        default:
            /* do nothing */
            break;
    }

    ACI_TEST( ulnErrorAddDiagRec(aFnContext,
                                 &sErrorMgr,
                                 SQL_NO_ROW_NUMBER,
                                 SQL_NO_COLUMN_NUMBER)
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnErrHandleCmError : object type is neither DBC nor STMT.");
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

