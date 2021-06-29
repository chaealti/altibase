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

/**
 * �� ������ ulnDbc ����ü �� �ٷ�� ����Ǵ� ��ƾ���� ��� �ִ�.
 *
 *  - ulnDbc �� ����
 *  - ulnDbc �� �ı�
 *  - ulnDbc �� �ʱ�ȭ
 *  - ulnDbc �� ������ STMT, DESC ���� �߰� / ����
 */

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnFreeStmt.h>
#include <uluLock.h>
#include <ulnDbc.h>
#include <ulnFailOver.h>
#include <ulnConnectCore.h>
#include <ulnString.h> /* ulnStrChr */
#include <ulnSemiAsyncPrefetch.h> /* PROJ-2625 */
#include <sqlcli.h>
#include <mtcc.h>

ACI_RC ulnDbcSetConnectionTimeout(ulnDbc *aDbc, acp_uint32_t aConnectionTimeout);

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
static ACI_RC ulnDbcReadEnvSockRcvBufBlockRatio(ulnDbc *aDbc);

ACI_RC ulnDbcAllocNewLink(ulnDbc *aDbc)
{
    cmnLinkPeerIPCDA *sLinkIPCDA = NULL;

    if (aDbc->mLink != NULL)
    {
        ACI_TEST(ulnDbcFreeLink(aDbc) != ACI_SUCCESS);
    }

    aDbc->mLink = NULL;

    ACE_ASSERT(aDbc->mCmiLinkImpl != CMI_LINK_IMPL_INVALID);

    /*
     * cmiLink �� �뵵�� ���� Type �� ������ ���� Impl �� ������.
     *
     *  1. Link Type
     *      - Listen Link (CMI_LINK_TYPE_LISTEN)
     *        ������ �޾Ƶ��̱� ���� ������ ��ũ
     *      - Peer Link (CMI_LINK_TYPE_PEER)
     *        ������ ����� ���� ���ӵ� �� Peer �� ��ũ.
     *
     *  2. Link Impl
     *      - TCP (CMI_LINK_IMPL_TCP)
     *        TCP socket
     *      - UNIX (CMI_LINK_IMPL_UNIX)
     *        Unix domain socket
     *      - IPC (CMI_LINK_IMPL_IPC)
     *        SysV shared memory IPC
     */
    ACI_TEST(cmiAllocLink(&(aDbc->mLink),
                          CMI_LINK_TYPE_PEER_CLIENT,
                          aDbc->mCmiLinkImpl) != ACI_SUCCESS);

    // proj_2160 cm_type removal: allocate cmbBlock
    ACI_TEST(cmiAllocCmBlock(&(aDbc->mPtContext.mCmiPtContext),
                             CMP_MODULE_DB,
                             aDbc->mLink,
                             aDbc) != ACI_SUCCESS);

#if defined(ALTI_CFG_OS_LINUX)
    /*PROJ-2616*/
    if(cmiGetLinkImpl(&(aDbc->mPtContext.mCmiPtContext)) == CMI_LINK_IMPL_IPCDA)
    {
        sLinkIPCDA = (cmnLinkPeerIPCDA*)aDbc->mLink;
        sLinkIPCDA->mMessageQ.mMessageQTimeout = aDbc->mParentEnv->mProperties.mIpcDaClientMessageQTimeout;
    }
#endif

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDbcFreeLink(ulnDbc *aDbc)
{
    if (aDbc->mLink != NULL)
    {
        // proj_2160 cm_type removal: free cmbBlock
        cmiFreeCmBlock(&(aDbc->mPtContext.mCmiPtContext));
        ACI_TEST(cmiFreeLink(aDbc->mLink) != ACI_SUCCESS);
        aDbc->mLink = NULL;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnDbcCreate.
 *
 * �Լ��� �ϴ� �� :
 *  - DBC���� ����� ûũ Ǯ �Ҵ�
 *  - DBC���� ����� �޸� �ν��Ͻ� �Ҵ�
 *  - DBC�ν��Ͻ� ����
 *  - DBC�� mObj�ʱ�ȭ
 *  - DBC�� mDiagHeader���� �� �ʱ�ȭ
 *
 * @return
 *  - ACI_SUCCESS
 *    DBC �ν��Ͻ� ���� ����
 *  - ACI_FAILURE
 *    �޸� ����
 *
 * ���� ������ �߻��ؼ� �Լ����� �������� ������,
 * �Ҵ��� ��� �޸𸮸� û���ϰ� ����������.
 */
ACI_RC ulnDbcCreate(ulnDbc **aOutputDbc)
{
    ULN_FLAG(sNeedDestroyPool);
    ULN_FLAG(sNeedDestroyMemory);
    ULN_FLAG(sNeedDestroyLock);

    uluChunkPool   *sPool = NULL;
    uluMemory      *sMemory;
    ulnDbc         *sDbc;

    acp_thr_mutex_t *sLock = NULL;

    /*
     * ûũ Ǯ ����
     */

    sPool = uluChunkPoolCreate(ULN_SIZE_OF_CHUNK_IN_DBC, ULN_NUMBER_OF_SP_IN_DBC, 1);
    ACI_TEST(sPool == NULL);
    ULN_FLAG_UP(sNeedDestroyPool);

    /*
     * �޸� �ν��Ͻ� ����
     */

    ACI_TEST(uluMemoryCreate(sPool, &sMemory) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyMemory);

    /*
     * ulnDbc �ν��Ͻ� ����
     */

    ACI_TEST(sMemory->mOp->mMalloc(sMemory, (void **)(&sDbc), ACI_SIZEOF(ulnDbc)) != ACI_SUCCESS);
    ACI_TEST(sMemory->mOp->mMarkSP(sMemory) != ACI_SUCCESS);

    /*
     * Object �ʱ�ȭ : �Ҵ�Ǿ����Ƿ� �ٷ� C2����.
     */

    ulnObjectInitialize((ulnObject *)sDbc,
                        ULN_OBJ_TYPE_DBC,
                        ULN_DESC_TYPE_NODESC,
                        ULN_S_C2,
                        sPool,
                        sMemory);

    /*
     * Lock ����ü ���� �� �ʱ�ȭ
     */

    ACI_TEST(uluLockCreate(&sLock) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyLock);

    ACI_TEST(acpThrMutexCreate(sLock, ACP_THR_MUTEX_DEFAULT) != ACP_RC_SUCCESS);

    /*
     * Diagnostic Header ����,
     */

    ACI_TEST(ulnCreateDiagHeader((ulnObject *)sDbc, NULL) != ACI_SUCCESS);

    // proj_2160 cm_type removal: set cmbBlock-ptr NULL
    cmiMakeCmBlockNull(&(sDbc->mPtContext.mCmiPtContext));

    /*
     * ������ ulnDbc ����
     */

    sDbc->mObj.mLock     = sLock;

    /* PROJ-1573 XA */
    sDbc->mXaConnection = NULL;
    sDbc->mXaEnlist     = ACP_FALSE;
    sDbc->mXaName       = NULL;

    *aOutputDbc = sDbc;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedDestroyLock)
    {
        acpThrMutexDestroy(sLock);
        uluLockDestroy(sLock);
    }

    ULN_IS_FLAG_UP(sNeedDestroyMemory)
    {
        sMemory->mOp->mDestroyMyself(sMemory);
    }

    ULN_IS_FLAG_UP(sNeedDestroyPool)
    {
        sPool->mOp->mDestroyMyself(sPool);
    }

    return ACI_FAILURE;
}

/**
 * ulnDbcDestroy.
 *
 * @param[in] aDbc
 *  �ı��� DBC �� ����Ű�� ������.
 * @return
 *  - ACI_SUCCESS
 *    ����
 *  - ACI_FAILURE
 *    ����.
 *    ȣ���ڴ� HY013 �� ����ڿ��� ��� �Ѵ�.
 */
ACI_RC ulnDbcDestroy(ulnDbc *aDbc)
{
    uluChunkPool *sPool;
    uluMemory    *sMemory;

    ACE_ASSERT(ULN_OBJ_GET_TYPE(aDbc) == ULN_OBJ_TYPE_DBC);

    sPool   = aDbc->mObj.mPool;
    sMemory = aDbc->mObj.mMemory;

    /*
     * --------------------------------------------------------------------
     * �޸� �Ҵ�(idlOS::malloc)�� �ؼ� ���� �����ϴ�
     * ULN_CONN_ATTR_TYPE_STRING Ÿ���� �����
     *
     * ���� : ulnDbcDestroy() �ÿ� �ݵ�� ������ ��� �Ѵ�.
     *
     * �Ʒ��� �Լ����� NULL �� �����ϸ� free �ϰ� alloc ���ϰԲ� �Ǿ� �ִ�.
     * --------------------------------------------------------------------
     */

    ulnDbcSetNlsLangString(aDbc, NULL, 0);  // mNlsLangString
    ulnDbcSetCurrentCatalog(aDbc, NULL, 0); // mAttrCurrentCatalog
    /* proj-1538 ipv6
     * ulnDbcSetDsnString needs aFnContext. so it's not used here */
    ulnDbcSetStringAttr(&aDbc->mDsnString, NULL, 0);
    ulnDbcSetUserName(aDbc, NULL, 0);       // mUserName
    ulnDbcSetPassword(aDbc, NULL, 0);       // mPassword
    ulnDbcSetDateFormat(aDbc, NULL, 0);     // mDateFormat
    ulnDbcSetAppInfo(aDbc, NULL, 0);        // mAppInfo
    ulnDbcSetStringAttr(&aDbc->mHostName, NULL, 0);
    ulnDbcSetTimezoneSring( aDbc, NULL, 0 );// mTimezoneString
    // PROJ-2727 add connect attr
    ulnDbcSetNlsTerriroty( aDbc, NULL, 0 ); 
    ulnNlsISOCurrency( aDbc, NULL, 0 );
    ulnNlsCurrency( aDbc, NULL, 0 );
    ulnNlsNumChar( aDbc, NULL, 0 );
    
    //fix BUG-18071
    ulnDbcSetServerPackageVer(aDbc, NULL, 0);  // mServerPackageVer
    /* ����������, ��Ʈ�� ���� ������ ǥ�� �Ӽ���. ���� ����ϰ� �Ǹ� �̰͵鵵 �Լ���
     * free �ϵ��� �� �־�� ��.
     */
    aDbc->mAttrTracefile        = NULL;
    aDbc->mAttrTranslateLib     = NULL;

    // PROJ-1579 NCHAR
    ulnDbcSetNlsCharsetString(aDbc, NULL, 0);      // mNlsCharsetString
    ulnDbcSetNlsNcharCharsetString(aDbc, NULL, 0); // mNlsNcharCharsetString

    //PROJ-1645 Failover
    ulnDbcSetAlternateServers(aDbc, NULL, 0);
    ulnDbcSetFailoverSource(aDbc, NULL, 0);  /* BUG-46599 */

    ulnFailoverFinalize(aDbc);

    /* BUG-36256 Improve property's communication */
    ulnConnAttrArrFinal(&aDbc->mUnsupportedProperties);

    /* BUG-44488 */
    ulnDbcSetSslCert(aDbc, NULL, 0);
    ulnDbcSetSslCaPath(aDbc, NULL, 0);
    ulnDbcSetSslCipher(aDbc, NULL, 0);
    ulnDbcSetSslKey(aDbc, NULL, 0);

    /* BUG-48315 */
    if (aDbc->mClientTouchNodeArr != NULL)
    {
        /* BUG-48384 UserSession(MetaDbc)�� �޸� ���� */
        if (aDbc->mShardDbcCxt.mParentDbc == NULL)
        {
            acpMemFree(aDbc->mClientTouchNodeArr);
        }
        aDbc->mClientTouchNodeArr = NULL;
    }

    /*
     * ���� mLink �� ��� �ִٸ� free �Ѵ�. ¦�� �ȸ�����, ������ġ��.
     */
    if (aDbc->mLink != NULL)
    {
        ACI_TEST(ulnDbcFreeLink(aDbc) != ACI_SUCCESS);
    }

    /*
     * DiagHeader �ı�
     */

    ACI_TEST(ulnDestroyDiagHeader(&aDbc->mObj.mDiagHeader, ULN_DIAG_HDR_DESTROY_CHUNKPOOL)
             != ACI_SUCCESS);

#ifdef COMPILE_SHARDCLI
    /* BUG-47312 ERROR RAISE ������ ��� ���� �������� �����Ѵ� */
    ulsdShardDestroy( aDbc );
#endif
    /* BUG-46092 */
    ulnShardDbcContextFinalize( aDbc );

    /*
     * BUG-15894 �Ǽ��� ���� ���� ����
     */

    aDbc->mObj.mType = ULN_OBJ_TYPE_MAX;

    /*
     * �޸�, ûũǮ �ı�
     * ��⼭ aDbc�� �����ϹǷ� aDbc���� ������� ���� ������
     * ��� ������ �����ؾ� �Ѵ�
     */

    sMemory->mOp->mDestroyMyself(sMemory);
    sPool->mOp->mDestroyMyself(sPool);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}




/**
 * ulnDbcInitialize.
 *
 * DBC �� �� �ʵ���� �ʱ�ȭ�Ѵ�.
 */
ACI_RC ulnDbcInitialize(ulnFnContext *aFnContext, ulnDbc *aDbc)
{
    acp_char_t* sNcharLiteral = NULL;
    acp_char_t* sCharValidation = NULL;

    // bug-25905: conn nls not applied to client lang module
    acp_char_t   sNlsUse[128];
    acp_char_t*  sNlsUseTemp  = NULL;
    acp_uint32_t sNlsUseLen   = 0;

    /*
     * Database ���� ���� ���ῡ ���õ� �Ӽ����� �ʱ�ȭ
     */
    aDbc->mLink                 = NULL;

    aDbc->mPortNumber           = 0;
    acpMemSet((void *)(&(aDbc->mUnixdomainFilepath)), 0, UNIX_FILE_PATH_LEN);
    acpMemSet((void *)(&(aDbc->mIpcFilepath)), 0, IPC_FILE_PATH_LEN);
    acpMemSet((void *)(&(aDbc->mIPCDAFilepath)), 0, IPC_FILE_PATH_LEN);
    aDbc->mConnType             = ULN_CONNTYPE_INVALID;
    aDbc->mCmiLinkImpl          = CMI_LINK_IMPL_INVALID;
    acpMemSet((void *)(&(aDbc->mConnectArg)), 0, ACI_SIZEOF(cmiConnectArg));

    /*
     * ----------------------------------------------------
     * �޸� �Ҵ�(idlOS::malloc)�� �ؼ� ���� �����ϴ�
     * ULN_CONN_ATTR_TYPE_STRING Ÿ���� �����
     *
     * ���� : ulnDbcDestroy() �ÿ� �ݵ�� ������ ��� �Ѵ�.
     * ----------------------------------------------------
     */

    aDbc->mDsnString            = NULL;
    aDbc->mUserName             = NULL;
    aDbc->mPassword             = NULL;
    aDbc->mDateFormat           = NULL;
    aDbc->mAppInfo              = NULL;
    aDbc->mHostName             = NULL;
    aDbc->mTimezoneString       = NULL;

    /*
     * ������ �����͸� NULL �� ���� ������
     * ulnDbcSetStringAttr() ���� �����Ⱚ�� free �Ϸ��� �õ��ϴٰ� ���� ���� �ִ�.
     */
    aDbc->mNlsLangString        = NULL;
    aDbc->mAttrCurrentCatalog   = NULL;

    //fix BUG-18971
    aDbc->mServerPackageVer     = NULL;

    /* BUG-36256 Improve property's communication */
    ulnConnAttrArrInit(&aDbc->mUnsupportedProperties);

    // bug-25905: conn nls not applied to client lang module
    // failover ���� �ʱ�ȭ �ڵ带 altibase_nls_use ���� ������
    // �ϵ��� �̰����� �Ű��.
    // why? nls_use ���� ���н� ulnDbcDestroy �������� failover
    // unavailable server list�� null�� �Ǿ� segv �߻� ����.

    //PROJ-1645 UL Failover.
    aDbc->mAlternateServers     = NULL;
    aDbc->mLoadBalance          = (acp_bool_t)gUlnConnAttrTable[ULN_CONN_ATTR_LOAD_BALANCE].mDefValue;
    aDbc->mConnectionRetryCnt   = gUlnConnAttrTable[ULN_CONN_ATTR_CONNECTION_RETRY_COUNT].mDefValue;
    aDbc->mConnectionRetryDelay = gUlnConnAttrTable[ULN_CONN_ATTR_CONNECTION_RETRY_DELAY].mDefValue;
    aDbc->mSessionFailover      = (acp_bool_t)gUlnConnAttrTable[ULN_CONN_ATTR_SESSION_FAILOVER].mDefValue;
    aDbc->mFailoverSource       = NULL; /* BUG-31390 Failover info for v$session */

    ulnFailoverInitialize(aDbc);

    // PROJ-1579 NCHAR
    if (acpEnvGet("ALTIBASE_NLS_NCHAR_LITERAL_REPLACE", &sNcharLiteral) == ACP_RC_SUCCESS)
    {
        if (sNcharLiteral[0] == '1')
        {
            aDbc->mNlsNcharLiteralReplace = ULN_NCHAR_LITERAL_REPLACE;
        }
        else
        {
            aDbc->mNlsNcharLiteralReplace = ULN_NCHAR_LITERAL_NONE;
        }
    }
    else
    {
        aDbc->mNlsNcharLiteralReplace = ULN_NCHAR_LITERAL_NONE;
    }

    // fix BUG-21790
    if (acpEnvGet("ALTIBASE_NLS_CHARACTERSET_VALIDATION", &sCharValidation) == ACP_RC_SUCCESS)
    {
        if (sCharValidation[0] == '0')
        {
            aDbc->mNlsCharactersetValidation = ULN_CHARACTERSET_VALIDATION_OFF;
        }
        else
        {
            aDbc->mNlsCharactersetValidation = ULN_CHARACTERSET_VALIDATION_ON;
        }
    }
    else
    {
        aDbc->mNlsCharactersetValidation = ULN_CHARACTERSET_VALIDATION_ON;
    }

    aDbc->mNlsCharsetString       = NULL;
    aDbc->mNlsNcharCharsetString  = NULL;
    aDbc->mCharsetLangModule      = NULL;
    aDbc->mNcharCharsetLangModule = NULL;
    aDbc->mWcharCharsetLangModule = NULL;
    aDbc->mIsSameEndian           = SQL_FALSE;

    /*
     * ����� ����������, ��Ʈ�� ���� ������ ǥ�� �Ӽ���.
     * ���� ����ϰ� �Ǹ� �̰͵鵵 ulnDbcDestroy() �Լ����� free �ϵ��� �� �־�� ��.
     */
    aDbc->mAttrTracefile        = NULL;
    aDbc->mAttrTranslateLib     = NULL;

    /*
     * ������ ���� �ʵ� �ʱ�ȭ
     */
    acpListInit(&aDbc->mStmtList);
    acpListInit(&aDbc->mDescList);

    aDbc->mStmtCount            = 0;
    aDbc->mDescCount            = 0;

    aDbc->mIsConnected          = ACP_FALSE;

    /* Altibase specific acp_uint32_t - if ( ID_UINT_MAX ) is get from server default*/
    aDbc->mAttrStackSize         = ACP_UINT32_MAX;    /* ALTIBASE_STACK_SIZE           */
    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    aDbc->mAttrDdlTimeout        = ULN_DDL_TMOUT_DEFAULT;    /* ALTIBASE_DDL_TIMEOUT          */
    aDbc->mAttrUtransTimeout     = ACP_UINT32_MAX;    /* ALTIBASE_UTRANS_TIMEOUT       */
    aDbc->mAttrFetchTimeout      = ACP_UINT32_MAX;    /* ALTIBASE_FETCH_TIMEOUT        */
    aDbc->mAttrIdleTimeout       = ACP_UINT32_MAX;    /* ALTIBASE_IDLE_TIMEOUT         */
    aDbc->mAttrOptimizerMode     = ACP_UINT32_MAX;    /* ALTIBASE_OPTIMIZER_MODE       */
    aDbc->mAttrHeaderDisplayMode = ACP_UINT32_MAX;    /* ALTIBASE_HEADER_DISPLAY_MODE  */
    aDbc->mTransactionalDDL      = SQL_UNDEF;         /* ALTIBASE_TRANSACTIONAL_DDL    */
    aDbc->mGlobalDDL             = SQL_UNDEF;         /* ALTIBASE_GLOBAL_DDL           */

    /*
     * Attribute �ʱ�ȭ
     * BUGBUG : �Ʒ� ������ �ϳ��� ��� ���Ͽ� DEFAULT ����� �����ؼ� �Ѳ����� �ֵ��� ����.
     */
    aDbc->mAttrExplainPlan       = SQL_UNDEF;        // ���� �� �𸣰�����, SQL_TRUE/FALSE �ΰͰ���

    /*
     * Altibase (Cli2) �Ӽ�
     */

    /*
     * ODBC �Ӽ�
     */
    aDbc->mAttrConnPooling      = SQL_UNDEF;
    aDbc->mAttrDisconnect       = SQL_DB_DISCONNECT;

    aDbc->mAttrAccessMode       = SQL_MODE_DEFAULT;
    aDbc->mAttrAsyncEnable      = SQL_ASYNC_ENABLE_DEFAULT;

    aDbc->mAttrAutoIPD          = SQL_FALSE;

    aDbc->mAttrAutoCommit       = SQL_UNDEF;     //Altibase use server defined mode  SQL_AUTOCOMMIT_DEFAULT;
    aDbc->mAttrConnDead         = SQL_CD_TRUE;   /* ODBC 3.5 */

    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    aDbc->mAttrQueryTimeout     = ULN_QUERY_TMOUT_DEFAULT;
    aDbc->mAttrLoginTimeout     = ULN_LOGIN_TMOUT_DEFAULT;

    ulnDbcSetConnectionTimeout(aDbc, ULN_CONN_TMOUT_DEFAULT);

    aDbc->mAttrOdbcCursors      = SQL_CUR_DEFAULT;

    /*
     * Note : Many data sources either do not support this option
     * or only can return but not set the network packet size.
     */
    aDbc->mAttrPacketSize       = ULN_PACKET_SIZE_DEFAULT;

    aDbc->mAttrQuietMode        = ACP_UINT64_LITERAL(0);
    aDbc->mAttrTrace            = SQL_OPT_TRACE_OFF;
    aDbc->mAttrTranslateOption  = 0;
    aDbc->mAttrTxnIsolation     = ULN_ISOLATION_LEVEL_DEFAULT;

    aDbc->mAttrFetchAheadRows   = 0;

    /*
     * SQL_ATTR_LONGDATA_COMPAT ��ǥ�� �Ӽ�,
     * ACP_TRUE �� ������ �Ǿ� ���� ��쿡��
     * SQL_BLOB, SQL_CLOB ���� Ÿ���� ����ڿ��� ���� �� ��
     * SQL_BINARY Ÿ������ �÷� �ش�.
     * ����Ʈ���� ACP_FALSE. ��, ���� ���ϴ� ���� ����Ʈ
     */
    aDbc->mAttrLongDataCompat   = ACP_FALSE;

    aDbc->mAttrAnsiApp          = ACP_TRUE;

    aDbc->mIsURL                 = ACP_FALSE;     /* �Ⱦ��� */

    aDbc->mMessageCallback       = NULL;

    // bug-19279 remote sysdba enable
    aDbc->mPrivilege             = ULN_PRIVILEGE_INVALID;
    aDbc->mAttrPreferIPv6        = ACP_FALSE;

    /* BUG-31144 */
    aDbc->mAttrMaxStatementsPerSession = ACP_UINT32_MAX;    /* ALTIBASE_MAX_STATEMENTS_PER_SESSION */

    /* bug-31468: adding conn-attr for trace logging */
    aDbc->mTraceLog = (acp_uint32_t)gUlnConnAttrTable[ULN_CONN_ATTR_TRACELOG].mDefValue;

    /* PROJ-2177 User Interface - Cancel */

    /** SessionID (Server side).
     *  StmtCID�� ���������� ��ȿ��(�ߺ����� �ʴ�) ���� �� �ֵ��� ����. */
    aDbc->mSessionID            = ULN_SESS_ID_NONE;

    /** ������ StmtCID�� �����ϱ� ���� seq */
    aDbc->mNextCIDSeq           = 0;

    /* PROJ-1891 Deferred Prepare */
    aDbc->mAttrDeferredPrepare  = SQL_UNDEF; /* BUG-40521 */

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    aDbc->mAttrLobCacheThreshold = ACP_UINT32_MAX;
    /* BUG-36548 Return code of client functions should be differed by ODBC version */
    aDbc->mAttrOdbcCompatibility = gUlnConnAttrTable[ULN_CONN_ATTR_ODBC_COMPATIBILITY].mDefValue;
    /* BUG-36729 Connection attribute will be added to unlock client mutex by force */
    aDbc->mAttrForceUnlock = gUlnConnAttrTable[ULN_CONN_ATTR_FORCE_UNLOCK].mDefValue;

    /* PROJ-2474 SSL/TLS */
    aDbc->mAttrSslCa = NULL;
    aDbc->mAttrSslCaPath = NULL;
    aDbc->mAttrSslCert = NULL;
    aDbc->mAttrSslCipher = NULL;
    aDbc->mAttrSslKey = NULL;
    aDbc->mAttrSslVerify = SQL_UNDEF; /* BUG-40521 */

    /* PROJ-2616 */
    aDbc->mIPCDAMicroSleepTime = 0;
    aDbc->mIPCDAULExpireCount  = 0;

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    if (ulnDbcReadEnvSockRcvBufBlockRatio(aDbc) != ACI_SUCCESS)
    {
        aDbc->mAttrSockRcvBufBlockRatio =
            gUlnConnAttrTable[ULN_CONN_ATTR_SOCK_RCVBUF_BLOCK_RATIO].mDefValue;
    }
    aDbc->mAsyncPrefetchStmt = NULL;

    aDbc->mSockBindAddr = NULL; /* BUG-44271 */

    aDbc->mAttrPDODeferProtocols = 0;  /* BUG-45286 For PDO Driver */

    /* PROJ-2681 */
    aDbc->mAttrIBLatency = ALTIBASE_IB_LATENCY_NORMAL;
    aDbc->mAttrIBConChkSpin = ALTIBASE_IB_CONCHKSPIN_DEFAULT;

    aDbc->mAttrGlobalTransactionLevel = ULN_GLOBAL_TX_NONE;

    // PROJ-2727
    aDbc->mAttributeCID                    = ULN_PROPERTY_MAX;
    aDbc->mAttributeCVal                   = 0;
    aDbc->mAttributeCStr                   = NULL;
    aDbc->mAttributeCLen                   = 0;    
    // add connect attr
    aDbc->mCommitWriteWaitMode             = 0;
    aDbc->mSTObjBufSize                    = 0;
    aDbc->mUpdateMaxLogSize                = 0;
    aDbc->mParallelDmlMode                 = 0;
    aDbc->mParallelDmlMode                 = 0;
    aDbc->mNlsNcharConvExcp                = 0;
    aDbc->mAutoRemoteExec                  = 0;
    aDbc->mTrclogDetailPredicate           = 0;
    aDbc->mOptimizerDiskIndexCostAdj       = 0;
    aDbc->mOptimizerMemoryIndexCostAdj     = 0;
    aDbc->mNlsTerritory                    = NULL;
    aDbc->mNlsISOCurrency                  = NULL;
    aDbc->mNlsCurrency                     = NULL;
    aDbc->mNlsNumChar                      = NULL;
    aDbc->mQueryRewriteEnable              = 0; 
    aDbc->mDblinkRemoteStatementAutoCommit = 0;
    aDbc->mRecyclebinEnable                = 0;
    aDbc->mUseOldSort                      = 0;
    aDbc->mArithmeticOpMode                = 0;
    aDbc->mResultCacheEnable               = 0;
    aDbc->mTopResultCacheMode              = 0;
    aDbc->mOptimizerAutoStats              = 0;
    aDbc->mOptimizerTransitivityOldRule    = 0;
    aDbc->mOptimizerPerformanceView        = 0;
    aDbc->mReplicationDDLSync              = 0; 
    aDbc->mReplicationDDLSyncTimeout       = 0;
    aDbc->mPrintOutEnable                  = 0;
    aDbc->mTrclogDetailShard               = 0;
    aDbc->mSerialExecuteMode               = 0;
    aDbc->mTrcLogDetailInformation         = 0;
    aDbc->mOptimizerDefaultTempTbsType     = 0;
    aDbc->mNormalFormMaximum               = 0;        
    aDbc->mReducePartPrepareMemory         = 0;    

    /* PROJ-2733-DistTxInfo */
    ulsdInitDistTxInfo(aDbc);

    /* TASK-7219 Non-shard DML */
    ulnDbcInitStmtExecSeqForShardTx(aDbc);

    aDbc->mShardStatementRetry = SQL_UNDEF;
    aDbc->mIndoubtFetchTimeout = ULN_TMOUT_MAX;
    aDbc->mIndoubtFetchMethod  = SQL_UNDEF;
    aDbc->mDDLLockTimeout = ULN_DDL_LOCK_TMOUT_UNDEF;

    /* BUG-48315 */
    aDbc->mClientTouchNodeArr   = NULL;
    aDbc->mClientTouchNodeCount = 0;

    ulnDbcSetShardCoordFixCtrlContext( aDbc, NULL );

    /* ***********************************************************
     * Exception ó���� �ʿ��� attributes.  (see BUG-44588 )
     * *********************************************************** */

    ACI_TEST(ulnShardDbcContextInitialize(aFnContext, aDbc) != ACI_SUCCESS);

    /* fix BUG-22353 CLI�� conn str���� NLS�� ������ ȯ�溯������ �о�;� �մϴ�. */

    /* fix BUG-25172
     * ȯ�溯���� ALTIBASE_NLS_USE�� ���ų� �߸��Ǿ� ���� ���
     * �⺻���� ASCII�� �����ϵ��� �Ѵ�.
     * �̴� ���� ������ �ӽ÷� �����Ǹ�, ���Ṯ�ڿ����� NLS�� �о�� �ٽ� �����Ѵ�.

     *bug-25905: conn nls not applied to client lang module
     * nls_use�� ���� �빮�� ��ȯ �� error ó�� �߰� */

    /* BUG-36059 [ux-isql] Need to handle empty envirionment variables gracefully at iSQL */
    if ((acpEnvGet("ALTIBASE_NLS_USE", &sNlsUseTemp) == ACP_RC_SUCCESS) &&
        (sNlsUseTemp[0] != '\0'))
    {
        acpCStrCpy(sNlsUse, ACI_SIZEOF(sNlsUse), sNlsUseTemp, acpCStrLen(sNlsUseTemp, ACP_SINT32_MAX));
    }
    else
    {
        acpCStrCpy(sNlsUse, ACI_SIZEOF(sNlsUse), "US7ASCII", 8);
    }
    sNlsUse[ACI_SIZEOF(sNlsUse) - 1] = '\0';

    sNlsUseLen = acpCStrLen(sNlsUse, ACI_SIZEOF(sNlsUse));
    acpCStrToUpper(sNlsUse, sNlsUseLen);
    ulnDbcSetNlsLangString(aDbc, sNlsUse, sNlsUseLen); /* mNlsLangString */

    ACI_TEST_RAISE(mtlModuleByName((const mtlModule **)&(aDbc->mClientCharsetLangModule),
                                   sNlsUse,
                                   sNlsUseLen)
                   != ACI_SUCCESS, NLS_NOT_FOUND);

    /* bug-26661: nls_use not applied to nls module for ut
     * nls_use ���� UT���� ����� gNlsModuleForUT���� �����Ŵ. */
    ACI_TEST_RAISE(mtlModuleByName((const mtlModule **)&gNlsModuleForUT,
                                   sNlsUse,
                                   sNlsUseLen)
                   != ACI_SUCCESS, NLS_NOT_FOUND);
    
    return ACI_SUCCESS;

    /* bug-25905: conn nls not applied to client lang module
     * nls_use ȯ�溯���� �߸� �����ϸ� mtl Module�� �� ã�� ����. */
    ACI_EXCEPTION(NLS_NOT_FOUND)
    {
        ulnError(aFnContext, ulERR_ABORT_idnSetDefaultFactoryError);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/**
 * ulnDbcAddDesc.
 *
 * Explicit �ϰ� �Ҵ�� ��ũ���͸� DBC �� mDescList �� �߰��Ѵ�.
 */
ACI_RC ulnDbcAddDesc(ulnDbc *aDbc, ulnDesc *aDesc)
{
    ACE_ASSERT(aDbc != NULL);
    ACE_ASSERT(aDesc != NULL);

    acpListAppendNode(&(aDbc->mDescList), (acp_list_t *)aDesc);
    aDbc->mDescCount++;

    aDesc->mParentObject = (ulnObject *)aDbc;

    return ACI_SUCCESS;
}

/**
 * ulnDbcRemoveDesc.
 *
 * Explicit �ϰ� �Ҵ�Ǿ��� ��ũ���͸� DBC �� mDescList ���� �����Ѵ�.
 */
ACI_RC ulnDbcRemoveDesc(ulnDbc *aDbc, ulnDesc *aDesc)
{
    ACE_ASSERT(aDbc != NULL);
    ACE_ASSERT(aDesc != NULL);

    ACI_TEST(acpListIsEmpty(&aDbc->mDescList));
    ACI_TEST(aDbc->mDescCount > 0);

    /*
     * aDesc �� ���� DESC �� �ƴ�, acp_list_t �� �ǳ��൵ ������ ���ڱ��� !
     */
    acpListDeleteNode(&(aDesc->mObj.mList));
    aDbc->mDescCount--;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnDbcGetDescCount.
 *
 * DBC �� ���� Explicit �ϰ� �Ҵ�� ��ũ������ ������ �д´�.
 */
acp_uint32_t ulnDbcGetDescCount(ulnDbc *aDbc)
{
    return aDbc->mDescCount;
}

/**
 * ulnDbcAddStmt.
 *
 * STMT �� DBC �� mStmtList �� �߰��Ѵ�.
 */
ACI_RC ulnDbcAddStmt(ulnDbc *aDbc, ulnStmt *aStmt)
{
    ACE_ASSERT(aDbc != NULL);
    ACE_ASSERT(aStmt != NULL);

    acpListAppendNode(&(aDbc->mStmtList), (acp_list_t *)aStmt);
    aDbc->mStmtCount++;

    aStmt->mParentDbc = aDbc;

    /* PROJ-2177: StmtID�� �𸦶� ���� ���� CID ���� */
    aStmt->mStmtCID = ulnDbcGetNextStmtCID(aDbc);
    ulnDbcSetUsingCIDSeq(aDbc, ULN_STMT_CID_SEQ(aStmt->mStmtCID));

    return ACI_SUCCESS;
}

/**
 * ulnDbcRemoveStmt.
 *
 * STMT �� DBC �� mStmtList �κ��� �����Ѵ�.
 */
ACI_RC ulnDbcRemoveStmt(ulnDbc *aDbc, ulnStmt *aStmt)
{
    ACE_ASSERT(aDbc != NULL);
    ACE_ASSERT(aStmt != NULL);

    ACI_TEST(acpListIsEmpty(&aDbc->mStmtList));
    ACI_TEST(aDbc->mStmtCount == 0);

    /*
     * aStmt �� ���� STMT �� �ƴ�, acp_list_t �� �ǳ��൵ ������ ���ڱ��� !
     */
    acpListDeleteNode(&(aStmt->mObj.mList));
    (aDbc->mStmtCount)--;

    /* PROJ-2177: CIDSeq ��Ȱ���� ���� flag ���� */
    ulnDbcClearUsingCIDSeq(aDbc, ULN_STMT_CID_SEQ(aStmt->mStmtCID));

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnDbcGetStmtCount.
 *
 * DBC �� ���� �ִ� STMT ���� ������ �о�´�.
 */
acp_uint32_t ulnDbcGetStmtCount(ulnDbc *aDbc)
{
    return aDbc->mStmtCount;
}

/*
 * mCmiLinkImpl �� �ѹ� ���õǸ� ���ٲ۴�.
 *
 * ulnDbcInitLinkImpl() �Լ��� ȣ���ؼ� �ʱ�ȭ���� ��� �缳���� �����ϰ� �Ѵ�.
 */
ACI_RC ulnDbcInitCmiLinkImpl(ulnDbc *aDbc)
{
    aDbc->mCmiLinkImpl = CMI_LINK_IMPL_INVALID;

    /*
     * BUGBUG : memset ?
     */
    acpMemSet(&(aDbc->mConnectArg), 0, ACI_SIZEOF(cmiConnectArg));

    return ACI_SUCCESS;
}

ACI_RC ulnDbcSetCmiLinkImpl(ulnDbc *aDbc, cmiLinkImpl aCmiLinkImpl)
{
    ACI_TEST(cmiIsSupportedLinkImpl(aCmiLinkImpl) != ACP_TRUE);

    aDbc->mCmiLinkImpl = aCmiLinkImpl;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

cmiLinkImpl  ulnDbcGetCmiLinkImpl(ulnDbc *aDbc)
{
    return aDbc->mCmiLinkImpl;
}

ACI_RC ulnDbcSetPortNumber(ulnDbc *aDbc, acp_sint32_t aPortNumber)
{
    aDbc->mPortNumber = aPortNumber;

    return ACI_SUCCESS;
}

acp_sint32_t ulnDbcGetPortNumber(ulnDbc *aDbc)
{
    return aDbc->mPortNumber;
}

/* PROJ-2474 SSL/TLS */
acp_char_t *ulnDbcGetSslCert(ulnDbc *aDbc)
{
    return aDbc->mAttrSslCert;
}

ACI_RC ulnDbcSetSslCert(ulnDbc *aDbc, 
                        acp_char_t *aCert, 
                        acp_sint32_t aCertLen)
{
    return ulnDbcSetStringAttr(&aDbc->mAttrSslCert, aCert, aCertLen);
}

acp_char_t *ulnDbcGetSslCa(ulnDbc *aDbc)
{
    return aDbc->mAttrSslCa;
}

ACI_RC ulnDbcSetSslCa(ulnDbc *aDbc, 
                      acp_char_t *aCa,
                      acp_sint32_t aCaLen)
{
    return ulnDbcSetStringAttr(&aDbc->mAttrSslCa, aCa, aCaLen);
}

acp_char_t *ulnDbcGetSslCaPath(ulnDbc *aDbc)
{
    return aDbc->mAttrSslCaPath;
}

ACI_RC ulnDbcSetSslCaPath(ulnDbc *aDbc, 
                          acp_char_t *aCaPath, 
                          acp_sint32_t aCaPathLen)
{
    return ulnDbcSetStringAttr(&aDbc->mAttrSslCaPath, aCaPath, aCaPathLen);
}

acp_char_t *ulnDbcGetSslCipher(ulnDbc *aDbc)
{
    return aDbc->mAttrSslCipher;
}

ACI_RC ulnDbcSetSslCipher(ulnDbc *aDbc, 
                          acp_char_t *aCipher, 
                          acp_sint32_t aCipherLen)
{
    return ulnDbcSetStringAttr(&aDbc->mAttrSslCipher, aCipher, aCipherLen);
}

acp_char_t *ulnDbcGetSslKey(ulnDbc *aDbc)
{
    return aDbc->mAttrSslKey;
}

ACI_RC ulnDbcSetSslKey(ulnDbc *aDbc, 
                       acp_char_t *aKey, 
                       acp_sint32_t aKeyLen)
{
    return ulnDbcSetStringAttr(&aDbc->mAttrSslKey, aKey, aKeyLen);
}

acp_bool_t ulnDbcGetSslVerify(ulnDbc *aDbc)
{
    return aDbc->mAttrSslVerify;
}

ACI_RC ulnDbcSetConnType(ulnDbc *aDbc, ulnConnType aConnType)
{
    aDbc->mConnType = aConnType;

    return ACI_SUCCESS;
}

ulnConnType ulnDbcGetConnType(ulnDbc *aDbc)
{
    return aDbc->mConnType;
}

ACI_RC ulnDbcSetShardConnType(ulnDbc *aDbc, ulnConnType aConnType)
{
    aDbc->mShardDbcCxt.mShardConnType = (ulsdShardConnType)aConnType;

    return ACI_SUCCESS;
}

ulnConnType ulnDbcGetShardConnType(ulnDbc *aDbc)
{
    return (ulnConnType)aDbc->mShardDbcCxt.mShardConnType;
}

cmiConnectArg *ulnDbcGetConnectArg(ulnDbc *aDbc)
{
    return &(aDbc->mConnectArg);
}

ACI_RC ulnDbcSetLoginTimeout(ulnDbc *aDbc, acp_uint32_t aLoginTimeout)
{
    /*
     * Note : SQL_ATTR_LOGIN_TIMEOUT �� �ʴ����̴�.
     */
    aDbc->mAttrLoginTimeout = aLoginTimeout;

    return ACI_SUCCESS;
}

acp_uint32_t ulnDbcGetLoginTimeout(ulnDbc *aDbc)
{
    /*
     * Note : SQL_ATTR_LOGIN_TIMEOUT �� �ʴ����̴�.
     */
    return aDbc->mAttrLoginTimeout;
}

ACI_RC ulnDbcSetConnectionTimeout(ulnDbc *aDbc, acp_uint32_t aConnectionTimeout)
{
    aDbc->mAttrConnTimeout = aConnectionTimeout;

    if (aConnectionTimeout == 0)
    {
        aDbc->mConnTimeoutValue = ACP_TIME_INFINITE;
    }
    else
    {
        aDbc->mConnTimeoutValue = acpTimeFromSec(aConnectionTimeout);
    }

    return ACI_SUCCESS;
}

acp_uint32_t ulnDbcGetConnectionTimeout(ulnDbc *aDbc)
{
    return aDbc->mAttrConnTimeout;
}

acp_uint8_t ulnDbcGetAttrAutoIPD(ulnDbc *aDbc)
{
    return aDbc->mAttrAutoIPD;
}

ACI_RC ulnDbcSetAttrAutoIPD(ulnDbc *aDbc, acp_uint8_t aEnable)
{
    aDbc->mAttrAutoIPD = aEnable;

    return ACI_SUCCESS;
}

ACI_RC ulnDbcSetStringAttr(acp_char_t **aAttr, acp_char_t *aStr, acp_sint32_t aStrLen)
{
    ACI_TEST(aStrLen < 0);

    if (aStr == NULL || aStrLen == 0)
    {
        if (*aAttr != NULL)
        {
            acpMemFree(*aAttr);
            *aAttr = NULL;
        }
    }
    else
    {
        ACI_TEST(ulnDbcAttrMem(aAttr, aStrLen) != ACI_SUCCESS);
        acpCStrCpy(*aAttr, aStrLen + 1, aStr, aStrLen);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDbcAttrMem(acp_char_t **aAttr, acp_sint32_t aLen)
{
    if (*aAttr != NULL)
    {
        acpMemFree(*aAttr);
        *aAttr = NULL;
    }

    if ( aLen > 0)
    {
        aLen++;
        ACI_TEST(acpMemAlloc((void**)aAttr, aLen) != ACP_RC_SUCCESS);
        acpMemSet(*aAttr, 0, aLen);
    }

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

acp_uint32_t ulnDbcGetAttrFetchAheadRows(ulnDbc *aDbc)
{
    return aDbc->mAttrFetchAheadRows;
}

void ulnDbcSetAttrFetchAheadRows(ulnDbc *aDbc, acp_uint32_t aNumberOfRow)
{
    aDbc->mAttrFetchAheadRows = aNumberOfRow;
}

// bug-19279 remote sysdba enable
void ulnDbcSetPrivilege(ulnDbc *aDbc, ulnPrivilege aVal)
{
    aDbc->mPrivilege = aVal;
}

ulnPrivilege ulnDbcGetPrivilege(ulnDbc *aDbc)
{
    return aDbc->mPrivilege;
}

acp_bool_t ulnDbcGetLongDataCompat(ulnDbc *aDbc)
{
    return aDbc->mAttrLongDataCompat;
}

void ulnDbcSetLongDataCompat(ulnDbc *aDbc, acp_bool_t aUseLongDataCompatibleMode)
{
    aDbc->mAttrLongDataCompat = aUseLongDataCompatibleMode;
}

void ulnDbcSetAnsiApp(ulnDbc *aDbc, acp_bool_t aIsAnsiApp)
{
    aDbc->mAttrAnsiApp = aIsAnsiApp;
}

/* PROJ-1573 XA */

ulnDbc * ulnDbcSwitch(ulnDbc *aDbc)
{
    ulnDbc *sDbc;
    sDbc = aDbc;
    /* BUG-18812 */
    if (sDbc == NULL)
    {
        return NULL;
    }

    if ((aDbc->mXaEnlist == ACP_TRUE) && (aDbc->mXaConnection != NULL))
    {
        sDbc = aDbc->mXaConnection->mDbc;
    }
    return sDbc;
}

/* BUG-42406 */
acp_bool_t ulnIsXaConnection(ulnDbc *aDbc)
{
    ACI_TEST(aDbc->mXaConnection == NULL);

    ACI_TEST(aDbc->mXaConnection->mDbc != aDbc);

    return ACP_TRUE;

    ACI_EXCEPTION_END;

    return ACP_FALSE;
}

/* BUG-42406 */
acp_bool_t ulnIsXaActive(ulnDbc *aDbc)
{
    ACI_TEST(ulnIsXaConnection(aDbc) == ACP_FALSE);

    ACI_TEST(ulxConnGetStatus(aDbc->mXaConnection) != ULX_XA_ACTIVE);

    return ACP_TRUE;

    ACI_EXCEPTION_END;

    return ACP_FALSE;
}

/* bug-31468: adding conn-attr for trace logging */
void  ulnDbcSetTraceLog(ulnDbc *aDbc, acp_uint32_t  aTraceLog)
{
    aDbc->mTraceLog = aTraceLog;
}

acp_uint32_t ulnDbcGetTraceLog(ulnDbc *aDbc)
{
    return aDbc->mTraceLog;
}

// PROJ-1645 UL Failover.
void ulnDbcCloseAllStatement(ulnDbc *aDbc)
{
    acp_list_node_t  *sIterator;
    ulnStmt          *sStmt;

    ulnDbcSetIsConnected(aDbc,ACP_FALSE);
    ulnDbcInitUsingCIDSeq(aDbc);
    ACP_LIST_ITERATE(&(aDbc->mStmtList),sIterator)
    {
        sStmt = (ulnStmt*) sIterator;
        ulnStmtSetPrepared(sStmt,ACP_FALSE);
        ulnStmtSetStatementID(sStmt, ULN_STMT_ID_NONE);
        ulnCursorSetServerCursorState(&(sStmt->mCursor),ULN_CURSOR_STATE_CLOSED);

        sStmt->mStmtCID = ulnDbcGetNextStmtCID(aDbc);
        ulnDbcSetUsingCIDSeq(aDbc, ULN_STMT_CID_SEQ(sStmt->mStmtCID));
    }
    ulnDbcSetIsConnected(aDbc,ACP_TRUE);
}

/* PROJ-2177 User Interface - Cancel */

/**
 * �� StmtCID(Client side StatementID)�� ��´�.
 *
 * StmtCID�� SessionID�� �־�߸� ��ȿ��(�ߺ����� �ʴ�) ���� ���� �� �����Ƿ�,
 * SessionID�� ������ StmtCID �������� �����Ѵ�.
 *
 * @param[in] aDbc   connection handle
 *
 * @return �� StmtCID. ��ȿ�� StmtCID�� ���� �� ������ 0
 */
acp_uint32_t ulnDbcGetNextStmtCID(ulnDbc *aDbc)
{
    acp_uint32_t sStmtCID = 0;
    acp_uint32_t sCurrCIDSeq;

    if (aDbc->mSessionID != 0)
    {
        /* Alloc, Free�� �ݺ��ϸ� �ߺ��� CIDSeq�� �� �� �ִ�.
         * �ߺ��� ���� ���� �ʵ��� ����������� �Ǵ��ؾ��Ѵ�.
         *
         * BUGBUG: ������ CIDSeq�� ��� ���������� ���ѹݺ��� ������.
         *         ������, �� �Լ� ȣ������ ���� ������ Ȯ���ϹǷ�
         *         ���� ���⼭ �� Ȯ�������� �ʴ´�. */
        sCurrCIDSeq = aDbc->mNextCIDSeq;
        while (ulnDbcCheckUsingCIDSeq(aDbc, sCurrCIDSeq) == ACP_TRUE)
        {
            sCurrCIDSeq = (sCurrCIDSeq + 1) % ULN_DBC_MAX_STMT;
        }
        ACE_ASSERT(sCurrCIDSeq < ULN_DBC_MAX_STMT);

        /* StmtCID�� SessionID�� ClientSeq�� �̷�����.
         * �̴� ���� ���ÿ� ���� StmtCID�� �������� �����ϱ� �����̴�. */
        sStmtCID = ULN_STMT_CID(aDbc->mSessionID, sCurrCIDSeq);
        aDbc->mNextCIDSeq = (sCurrCIDSeq + 1) % ULN_DBC_MAX_STMT;
    }

    return sStmtCID;
}

/**
 * ������� CIDSeq�� Ȯ���ϱ� ���� ��Ʈ���� �ʱ�ȭ�Ѵ�.
 *
 * @param[in] aDbc   dbc handle
 */
void ulnDbcInitUsingCIDSeq(ulnDbc *aDbc)
{
    acpMemSet(aDbc->mUsingCIDSeqBMP, 0, sizeof(aDbc->mUsingCIDSeqBMP));
}

/**
 * CIDSeq�� ��������� Ȯ���Ѵ�.
 *
 * @param[in] aDbc      dbc handle
 * @param[in] aCIDSeq   CID seq
 *
 * @return ������̸� ACP_TRUE, �ƴϸ� ACP_FALSE
 */
acp_bool_t ulnDbcCheckUsingCIDSeq(ulnDbc *aDbc, acp_uint32_t aCIDSeq)
{
    acp_uint8_t  sByteVal;
    acp_uint8_t  sBitVal;

    ACE_ASSERT(aCIDSeq < ULN_DBC_MAX_STMT);

    sByteVal = aDbc->mUsingCIDSeqBMP[aCIDSeq / 8];
    sBitVal = 0x01 & (sByteVal >> (7 - (aCIDSeq % 8)));

    return (sBitVal == 1) ? ACP_TRUE : ACP_FALSE;
}

/**
 * CIDSeq�� ��������� �����Ѵ�.
 *
 * @param[in] aDbc      dbc handle
 * @param[in] aCIDSeq   CID seq
 */
void ulnDbcSetUsingCIDSeq(ulnDbc *aDbc, acp_uint32_t aCIDSeq)
{
    ACE_ASSERT(aCIDSeq < ULN_DBC_MAX_STMT);

    aDbc->mUsingCIDSeqBMP[aCIDSeq / 8] |= (acp_uint8_t) (1 << (7 - (aCIDSeq % 8)));
}

/**
 * CIDSeq�� ������� �ƴѰ����� �����Ѵ�.
 *
 * @param[in] aDbc      dbc handle
 * @param[in] aCIDSeq   CID seq
 */
void ulnDbcClearUsingCIDSeq(ulnDbc *aDbc, acp_uint32_t aCIDSeq)
{
    ACE_ASSERT(aCIDSeq < ULN_DBC_MAX_STMT);

    aDbc->mUsingCIDSeqBMP[aCIDSeq / 8] &= ~((acp_uint8_t) (1 << (7 - (aCIDSeq % 8))));
}

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
static ACI_RC ulnDbcReadEnvSockRcvBufBlockRatio(ulnDbc *aDbc)
{
    acp_char_t   *sEnvStr = NULL;
    acp_sint64_t  sSockRcvBufBlockRatio = 0;
    acp_sint32_t  sSign = 1;

    ACE_DASSERT(aDbc != NULL);

    ACI_TEST(acpEnvGet("ALTIBASE_SOCK_RCVBUF_BLOCK_RATIO", &sEnvStr) != ACP_RC_SUCCESS);

    ACI_TEST(acpCStrToInt64(sEnvStr,
                            acpCStrLen((acp_char_t *)sEnvStr, ACP_UINT32_MAX),
                            &sSign,
                            (acp_uint64_t *)&sSockRcvBufBlockRatio,
                            10,
                            NULL) != ACP_RC_SUCCESS);

    sSockRcvBufBlockRatio *= sSign;

    ACI_TEST(sSockRcvBufBlockRatio < gUlnConnAttrTable[ULN_CONN_ATTR_SOCK_RCVBUF_BLOCK_RATIO].mMinValue);
    ACI_TEST(gUlnConnAttrTable[ULN_CONN_ATTR_SOCK_RCVBUF_BLOCK_RATIO].mMaxValue < sSockRcvBufBlockRatio);

    aDbc->mAttrSockRcvBufBlockRatio = (acp_uint32_t)sSockRcvBufBlockRatio;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDbcSetSockRcvBufBlockRatio(ulnFnContext *aFnContext,
                                     acp_uint32_t  aSockRcvBufBlockRatio)
{
    ulnDbc *sDbc = NULL;
    acp_sint32_t sSockRcvBufSize;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /* set socket receive buffer size */
    if (aSockRcvBufBlockRatio == 0)
    {
        sSockRcvBufSize = CMI_BLOCK_DEFAULT_SIZE * ULN_SOCK_RCVBUF_BLOCK_RATIO_DEFAULT;
    }
    else
    {
        sSockRcvBufSize = CMI_BLOCK_DEFAULT_SIZE * aSockRcvBufBlockRatio;
    }

    ACI_TEST_RAISE(cmiSetLinkRcvBufSize(sDbc->mLink,
                                        sSockRcvBufSize)
            != ACI_SUCCESS, LABEL_CM_ERR);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_CM_ERR)
    {
        ulnErrHandleCmError(aFnContext, NULL);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDbcSetAttrSockRcvBufBlockRatio(ulnFnContext *aFnContext,
                                         acp_uint32_t  aSockRcvBufBlockRatio)
{
    ulnDbc *sDbc = NULL;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /* set socket receive buffer size, if connected */
    if (ulnDbcIsConnected(sDbc) == ACP_TRUE)
    {
        /* don't change socket receive buffer on auto-tuning */
        if (ulnFetchIsAutoTunedSockRcvBuf(sDbc) == ACP_FALSE)
        {
            ACI_TEST(ulnDbcSetSockRcvBufBlockRatio(aFnContext, aSockRcvBufBlockRatio) != ACI_SUCCESS);
        }
        else
        {
            /* auto-tuned socket receive buffer */
        }
    }
    else
    {
        /* nothing to do */
    }

    sDbc->mAttrSockRcvBufBlockRatio = aSockRcvBufBlockRatio;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void ulnDbcShardCoordFixCtrlEnter(ulnFnContext *aFnContext, ulnShardCoordFixCtrlContext * aCtx)
{
    aCtx->mFunc(  aCtx->mSession,
                 &aCtx->mCount,
                  ULN_SHARD_COORD_FIX_CTRL_EVENT_ENTER );

    aFnContext->mShardCoordFixCtrlCtx = aCtx;
}

void ulnDbcShardCoordFixCtrlExit(ulnFnContext *aFnContext)
{
    ulnShardCoordFixCtrlContext * sCtx = aFnContext->mShardCoordFixCtrlCtx;

    sCtx->mFunc(  sCtx->mSession,
                 &sCtx->mCount,
                  ULN_SHARD_COORD_FIX_CTRL_EVENT_EXIT );

    aFnContext->mShardCoordFixCtrlCtx = NULL;
}
