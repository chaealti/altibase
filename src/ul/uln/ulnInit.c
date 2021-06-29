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

// fix Bug-17858
#include <uln.h>
#include <ulnConfigFile.h>
#include <ulnPrivate.h>

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#include <ulnSemiAsyncPrefetch.h>

/* BUG-45411 client-side global transaction */
#include <ulsdnTrans.h>

static acp_thr_once_t     gUlnInitOnce  = ACP_THR_ONCE_INIT;
static acp_thr_mutex_t    gUlnInitMutex = ACP_THR_MUTEX_INITIALIZER;
static acp_sint32_t       gUlnInitCount;

ACP_EXTERN_C_BEGIN

// bug-26661: nls_use not applied to nls module for ut
// UT���� ����� ���氡���� nls module ���� ������ ����

mtlModule* gNlsModuleForUT = NULL;

static void ulnInitializeOnce( void )
{
    ACE_ASSERT(acpThrMutexCreate(&gUlnInitMutex, ACP_THR_MUTEX_DEFAULT) == ACP_RC_SUCCESS);

    gUlnInitCount = 0;
}

ACP_EXTERN_C_END

/*
 * ulnInitializeDBProtocolCallbackFunctions.
 *
 * ��Ÿ���� �ݹ� �Լ����� �����ϴ� �Լ��̴�.
 * ENV �� �Ҵ��� �� �ѹ��� �� �ָ� �ȴ�.
 * cmiInitialize() �� ȣ���ϸ鼭 �Բ� ȣ���ؼ� �����ϸ� �̻����̰ڴ�.
 */

static ACI_RC ulnInitializeDBProtocolCallbackFunctions(void)
{
    /*
     * MESSAGE
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, Message),
                            ulnCallbackMessage) != ACI_SUCCESS);

    /*
     * ERROR
     */

    /* PROJ-2733-Protocol Handshake ������ ������ �߻��� �� �����Ƿ� ErrorResult�� �����Ѵ�. */
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ErrorResult),
                            ulnCallbackErrorResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ErrorV3Result),
                            ulnCallbackErrorResult) != ACI_SUCCESS);

    /*
     * PREPARE
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, PrepareV3Result),  /* BUG-48775 */
                            ulnCallbackPrepareResult) != ACI_SUCCESS);

    /*
     * BINDINFO GET / SET
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ColumnInfoGetResult),
                            ulnCallbackColumnInfoGetResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ColumnInfoGetListResult),
                            ulnCallbackColumnInfoGetListResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ParamInfoSetListResult),
                            ulnCallbackParamInfoSetListResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ParamInfoGetResult),
                            ulnCallbackParamInfoGetResult) != ACI_SUCCESS);

    /*
     * BINDDATA IN / OUT
     */

    // bug-28259: ipc needs paramDataInResult
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ParamDataInResult),
                            ulnCallbackParamDataInResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ParamDataOutList),
                            ulnCallbackParamDataOutList) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ParamDataInListV3Result),  /* PROJ-2733-Protocol */
                            ulnCallbackParamDataInListResult) != ACI_SUCCESS);

    /*
     * CONNECT / DISCONNECT
     */

    /* BUG-38496 Notify users when their password expiry date is approaching */
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ConnectV3Result),  /* PROJ-2733-Protocol */
                            ulnCallbackDBConnectResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, DisconnectResult),
                            ulnCallbackDisconnectResult) != ACI_SUCCESS);

    /*
     * PROPERTY SET
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, PropertySetV3Result),  /* PROJ-2733-Protocol */
                            ulnCallbackDBPropertySetResult) != ACI_SUCCESS);

    /*
     * PROPERTY GET
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, PropertyGetResult),
                            ulnCallbackDBPropertyGetResult) != ACI_SUCCESS);

    /*
     * FETCH / FETCHMOVE
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, FetchMoveResult),
                            ulnCallbackFetchMoveResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, FetchBeginResult),
                            ulnCallbackFetchBeginResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, FetchResult),
                            ulnCallbackFetchResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, FetchEndResult),
                            ulnCallbackFetchEndResult) != ACI_SUCCESS);

    /*
     * EXECUTE
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ExecuteV3Result),  /* PROJ-2733-Protocol */
                            ulnCallbackExecuteResult) != ACI_SUCCESS);

    /*
     * TRANSACTION
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, TransactionResult),
                            ulnCallbackTransactionResult) != ACI_SUCCESS);

    /*
     * FREE
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, FreeResult),
                            ulnCallbackFreeResult) != ACI_SUCCESS);

    /*
     * LOB
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobGetSizeV3Result),
                            ulnCallbackLobGetSizeResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobPutBeginResult),
                            ulnCallbackDBLobPutBeginResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobPutEndResult),
                            ulnCallbackDBLobPutEndResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobGetResult),
                            ulnCallbackLobGetResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobFreeResult),
                            ulnCallbackLobFreeResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobFreeAllResult),
                            ulnCallbackLobFreeAllResult) != ACI_SUCCESS);

    /*
    * PROJ-2047 Strengthening LOB - Added Interfaces
    */
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, LobTrimResult),
                            ulnCallbackLobTrimResult) != ACI_SUCCESS);

    /*
     * PLAN GET
     */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, PlanGetResult),
                            ulnCallbackPlanGetResult) != ACI_SUCCESS);

    /* PROJ-1573 XA */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, XaResult),
                            ulxCallbackXaGetResult) != ACI_SUCCESS);
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, XaXid),
                            ulxCallbackXaXid) != ACI_SUCCESS);

    /* PROJ-2177 User Interface - Cancel */

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, CancelResult),
                            ulnCallbackCancelResult) != ACI_SUCCESS);

    /* BUG-45411 client-side global transaction */
    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ShardPrepareV3Result),
                            ulsdCallbackShardPrepareResult) != ACI_SUCCESS);

    ACI_TEST(cmiSetCallback(CMI_PROTOCOL(DB, ShardEndPendingTxV3Result),
                            ulsdCallbackShardEndPendingTxResult) != ACI_SUCCESS);

    /* BUG-46785 Shard statement partial rollback */
    ACI_TEST( cmiSetCallback( CMI_PROTOCOL( DB, SetSavepointResult ),
                              ulnCallbackSetSavepointResult ) != ACI_SUCCESS );

    ACI_TEST( cmiSetCallback( CMI_PROTOCOL( DB, RollbackToSavepointResult ),
                              ulnCallbackRollbackToSavepointResult ) != ACI_SUCCESS );

    ACI_TEST( cmiSetCallback( CMI_PROTOCOL( DB, ShardStmtPartialRollbackResult ),
                              ulsdnStmtShardStmtPartialRollbackResult ) != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnInitialize()
{
    // bug-26661: nls_use not applied to nls module for ut
    acp_char_t sDefaultNls[128] = "US7ASCII";

    ACE_ASSERT(acpInitialize() == ACP_RC_SUCCESS);

    acpThrOnce(&gUlnInitOnce, ulnInitializeOnce);

    ACE_ASSERT(acpThrMutexLock(&gUlnInitMutex) == ACP_RC_SUCCESS);

    ACI_TEST(gUlnInitCount < 0);

    if (gUlnInitCount == 0)
    {
        ACI_TEST(cmiInitialize(ACP_UINT32_MAX) != ACI_SUCCESS);

        // BUG-20859 �߸��� NLS_USE �� ���� Ŭ���̾�Ʈ�� �׽��ϴ�.
        // ulnDbcInitialize ���� ȯ�溯���� �о �ٽ� �����ϱ� ������ ������ ����.
        ACI_TEST(mtlInitialize(sDefaultNls, ACP_TRUE ) != ACI_SUCCESS);

        // bug-26661: nls_use not applied to ul nls module
        // UT������ ���� mtl::defModule�� �����ϰ� ���
        // gNlsModuleForUT�� ����� ���̴�.
        // ���ʿ� gNlsModuleForUT�� NULL �̹Ƿ� �ϴ�
        // US7ASCII ����̶� ������ �ؾ� �Ѵ�.
        ACI_TEST(mtlModuleByName((const mtlModule **)&gNlsModuleForUT,
                                 sDefaultNls,
                                 strlen(sDefaultNls))
                 != ACI_SUCCESS);

        //PROJ-1645 UL-FailOver.
        ulnConfigFileLoad();
        ACI_TEST(ulnInitializeDBProtocolCallbackFunctions() != ACI_SUCCESS);

        /* bug-35142 cli trace log
           trace level, file ������ latch ����.
           ���ɻ��� ������ level�� ���ʿ� �ѹ��� ���Ѵ� */
        ulnInitTraceLog();

        /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
        ulnInitTraceLogPrefetchAsyncStat();
    }

    gUlnInitCount++;

    ACE_ASSERT(acpThrMutexUnlock(&gUlnInitMutex) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    {
        gUlnInitCount = -1;

        ACE_ASSERT(acpThrMutexUnlock(&gUlnInitMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

ACI_RC ulnFinalize()
{
    ACE_ASSERT(acpThrMutexLock(&gUlnInitMutex) == ACP_RC_SUCCESS);

    ACI_TEST(gUlnInitCount < 0);

    gUlnInitCount--;

    if (gUlnInitCount == 0)
    {
        /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
        ulnFinalizeTraceLogPrefetchAsyncStat();

        /* bug-35142 cli trace log */
        ulnFinalTraceLog();
        //PROJ-1645 UL-FailOver.
        ulnConfigFileUnLoad();

        // BUG-20250
        ACI_TEST(mtlFinalize() != ACI_SUCCESS);

        ACI_TEST(cmiFinalize() != ACI_SUCCESS);
    }

    ACE_ASSERT(acpThrMutexUnlock(&gUlnInitMutex) == ACP_RC_SUCCESS);

    ACE_ASSERT(acpFinalize() == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    {
        gUlnInitCount = -1;

        ACE_ASSERT(acpThrMutexUnlock(&gUlnInitMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

// BUG-39147
void ulnDestroy()
{
    mtcDestroyForClient();
    cmiDestroy();
}

/*fix BUG-25597 APRE���� AIX�÷��� �νõ� ���������� �ذ��ؾ� �մϴ�.
APRE�� ulpConnMgr�ʱ�ȭ���� �̹� Xa Open�� Connection��
APRE�� Loading�ϴ� �Լ�.
*/

void ulnLoadOpenedXaConnections2APRE()
{
    ulxXaRegisterOpenedConnections2APRE();
}
