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

#ifndef _O_CMI_H_
#define _O_CMI_H_ 1


/*
 * cmn Defines
 */

#define CMI_DIRECTION_RD                    CMN_DIRECTION_RD
#define CMI_DIRECTION_WR                    CMN_DIRECTION_WR
#define CMI_DIRECTION_RDWR                  CMN_DIRECTION_RDWR

#define CMI_DISPATCHER_IMPL_SOCK            CMN_DISPATCHER_IMPL_SOCK
#define CMI_DISPATCHER_IMPL_TCP             CMN_DISPATCHER_IMPL_SOCK
#define CMI_DISPATCHER_IMPL_UNIX            CMN_DISPATCHER_IMPL_SOCK
#define CMI_DISPATCHER_IMPL_IPC             CMN_DISPATCHER_IMPL_IPC
#define CMI_DISPATCHER_IMPL_IPCDA           CMN_DISPATCHER_IMPL_IPCDA
#define CMI_DISPATCHER_IMPL_IB              CMN_DISPATCHER_IMPL_IB
#define CMI_DISPATCHER_IMPL_BASE            CMN_DISPATCHER_IMPL_BASE
#define CMI_DISPATCHER_IMPL_MAX             CMN_DISPATCHER_IMPL_MAX
#define CMI_DISPATCHER_IMPL_INVALID         CMN_DISPATCHER_IMPL_INVALID

#define CMI_LINK_FEATURE_SYSDBA             CMN_LINK_FEATURE_SYSDBA

#define CMI_LINK_TYPE_LISTEN                CMN_LINK_TYPE_LISTEN
#define CMI_LINK_TYPE_PEER_SERVER           CMN_LINK_TYPE_PEER_SERVER
#define CMI_LINK_TYPE_PEER_CLIENT           CMN_LINK_TYPE_PEER_CLIENT
#define CMI_LINK_TYPE_BASE                  CMN_LINK_TYPE_BASE
#define CMI_LINK_TYPE_MAX                   CMN_LINK_TYPE_MAX
#define CMI_LINK_TYPE_INVALID               CMN_LINK_TYPE_INVALID

#define CMI_LINK_IMPL_DUMMY                 CMN_LINK_IMPL_DUMMY
#define CMI_LINK_IMPL_TCP                   CMN_LINK_IMPL_TCP
#define CMI_LINK_IMPL_UNIX                  CMN_LINK_IMPL_UNIX
#define CMI_LINK_IMPL_IPC                   CMN_LINK_IMPL_IPC
#define CMI_LINK_IMPL_IPCDA                 CMN_LINK_IMPL_IPCDA
#define CMI_LINK_IMPL_BASE                  CMN_LINK_IMPL_BASE
#define CMI_LINK_IMPL_MAX                   CMN_LINK_IMPL_MAX
#define CMI_LINK_IMPL_INVALID               CMN_LINK_IMPL_INVALID
/* PROJ-2474 SSL/TLS */
#define CMI_LINK_IMPL_SSL                   CMN_LINK_IMPL_SSL
/* PROJ-2681 */
#define CMI_LINK_IMPL_IB                    CMN_LINK_IMPL_IB

#define CMI_LINK_INFO_ALL                   CMN_LINK_INFO_ALL
#define CMI_LINK_INFO_IMPL                  CMN_LINK_INFO_IMPL
#define CMI_LINK_INFO_LOCAL_ADDRESS         CMN_LINK_INFO_LOCAL_ADDRESS
#define CMI_LINK_INFO_LOCAL_IP_ADDRESS      CMN_LINK_INFO_LOCAL_IP_ADDRESS
#define CMI_LINK_INFO_LOCAL_PORT            CMN_LINK_INFO_LOCAL_PORT
#define CMI_LINK_INFO_REMOTE_ADDRESS        CMN_LINK_INFO_REMOTE_ADDRESS
#define CMI_LINK_INFO_REMOTE_IP_ADDRESS     CMN_LINK_INFO_REMOTE_IP_ADDRESS
#define CMI_LINK_INFO_REMOTE_PORT           CMN_LINK_INFO_REMOTE_PORT
#define CMI_LINK_INFO_REMOTE_SOCKADDR       CMN_LINK_INFO_REMOTE_SOCKADDR
#define CMI_LINK_INFO_UNIX_PATH             CMN_LINK_INFO_UNIX_PATH
#define CMI_LINK_INFO_IPC_KEY               CMN_LINK_INFO_IPC_KEY
#define CMI_LINK_INFO_IPCDA_KEY             CMN_LINK_INFO_IPCDA_KEY
/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
#define CMI_LINK_INFO_TCP_KERNEL_STAT       CMN_LINK_INFO_TCP_KERNEL_STAT

/* PROJ-2733-Protocol cmpDefDB.h */
#define CMI_IS_CONNECT_GROUP                CMP_IS_CONNECT_GROUP
#define CMI_IS_PROPERTY_SET_GROUP           CMP_IS_PROPERTY_SET_GROUP
#define CMI_IS_PROPERTY_GET_GROUP           CMP_IS_PROPERTY_GET_GROUP
#define CMI_IS_PARAM_DATA_IN_LIST_GROUP     CMP_IS_PARAM_DATA_IN_LIST_GROUP
#define CMI_IS_EXECUTE_GROUP                CMP_IS_EXECUTE_GROUP
#define CMI_IS_FETCH_GROUP                  CMP_IS_FETCH_GROUP
#define CMI_IS_TRANSACTION_GROUP            CMP_IS_TRANSACTION_GROUP
#define CMI_IS_SHARD_TRANSACTION_GROUP      CMP_IS_SHARD_TRANSACTION_GROUP
#define CMI_IS_SHARD_PREPARE_GROUP          CMP_IS_SHARD_PREPARE_GROUP
#define CMI_IS_SHARD_END_PENDING_GROUP      CMP_IS_SHARD_END_PENDING_GROUP
#define CMI_IS_PREPARE_GROUP                CMP_IS_PREPARE_GROUP

#ifndef SO_NONE
#define SO_NONE 0
#endif

/* PROJ-2616 */
#define CMI_IPCDA_SPIN_MAX_LOOP       100000
#define CMI_IPCDA_SPIN_MIN_LOOP        1

#if defined(ALTI_CFG_OS_LINUX)
/* Maximum size of a whole message queue */
#define CMI_IPCDA_MESSAGEQ_MAX_MESSAGE   10
/* Size of a message queue segment */
#define CMI_IPCDA_MESSAGEQ_MESSAGE_SIZE  1
/* server�� ������ ���۵Ǿ����� message queue�� �˸���. */
#define CMI_IPCDA_MESSAGEQ_NOTIFY    1
#endif

/* BUG-44705 ���� �߻��� client�� �����ϴ� ���� ���������� ������
 * IPCDA������ buffer����� ������ �ֱ� ������ client�� ��û�� ���� ���� ó����
 * ������ �߻��� ��츦 ����� ���� �޼����� ������ size�� �̸� Ȯ���ؾ� �Ѵ�.
 * CMP_OP_DB_ErrorResult + error_msg_max_size + CMP_OP_DB_IPCDALastOpEnded*/

/* PROJ-2733-Protocol CMP_OP_DB_ErrorV3Result�� SCN �߰��� ���� 8byte ����
                      ErrorResult, ErrorResultV3, ErrorResultV?(Future)�� �ִ� �����
                      �����Ǿ�� �Ѵ�. üũ �������� ErrorResult�� ��Ȯ�� ������ �𸣱� ���� */
#define CMI_IPCDA_REMAIN_PROTOCOL_SIZE (12 + MAX_ERROR_MSG_LEN + 8 + 1)

/*
 * cmn Types
 */
typedef struct cmnDispatcher     cmiDispatcher;
typedef struct cmnLink           cmiLink;
typedef union  cmnLinkListenArg  cmiListenArg;
typedef union  cmnLinkConnectArg cmiConnectArg;
typedef        cmnDirection      cmiDirection;
typedef        cmnLinkType       cmiLinkType;
typedef        cmnLinkImpl       cmiLinkImpl;
typedef        cmnDispatcherImpl cmiDispatcherImpl;
typedef        cmnLinkInfoKey    cmiLinkInfoKey;

/*
 * cmm Types
 */

typedef struct cmmSession cmiSession;

/*
 * cmp Types
 */

typedef struct cmpProtocol cmiProtocol;

typedef cmpCallbackFunction cmiCallbackFunction;

/* bug-33841: ipc thread's state is wrongly displayed */
typedef IDE_RC (*cmiCallbackSetExecute)(void* ,void*);

/*
 * cmp protocol macros
 */

#define CMI_PROTOCOL_MODULE(aModule)         CMP_MODULE_ ## aModule
#define CMI_PROTOCOL_OPERATION(aModule, aOp) CMP_OP_ ## aModule ## _ ## aOp
#define CMI_PROTOCOL(aModule, aOp)           CMI_PROTOCOL_MODULE(aModule), CMI_PROTOCOL_OPERATION(aModule, aOp)

#define CMI_PROTOCOL_GET_ARG(aProtocol, aModule, aOp) &((aProtocol).mArg.m ## aModule.m ## aOp)

#define CMI_PROTOCOL_INITIALIZE(aProtocol, aArg, aModule, aOp)                  \
    do                                                                          \
    {                                                                           \
        IDE_TEST(cmiInitializeProtocol(&(aProtocol),                            \
                                       &(gCmpModule ## aModule),                \
                                       CMI_PROTOCOL_OPERATION(aModule, aOp))    \
                 != IDE_SUCCESS);                                               \
                                                                                \
        aArg = CMI_PROTOCOL_GET_ARG(aProtocol, aModule, aOp);                   \
    } while (0)

/* BUG-44124 ipcda ��� ��� �� hang - iloader �÷��� ���� ���̺� */
typedef enum cmiWriteCheckState
{
    CMI_WRITE_CHECK_ACTIVATED   = 1,
    CMI_WRITE_CHECK_DEACTIVATED = 2
} cmiWriteCheckState;

/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ����  */
typedef enum cmiCompressType
{
    CMI_COMPRESS_NONE       = 0,
    CMI_COMPRESS_LZO        = 1,
    CMI_COMPRESS_LZ4        = 2,
    CMI_COMPRESS_MAX        
} cmiCompressType;

/*
 * cmiProtocolContext
 */

typedef struct cmiProtocolContext
{
    struct cmnLinkPeer           *mLink;
    struct cmmSession            *mSession;
    struct cmpModule             *mModule;

    struct cmbBlock              *mReadBlock;
    struct cmbBlock              *mWriteBlock;
    idBool                        mIsAddReadBlock;

    cmpHeader                     mReadHeader;
    cmpHeader                     mWriteHeader;

    cmpMarshalState               mMarshalState;
    cmpProtocol                   mProtocol;

    iduList                       mReadBlockList;
    iduList                       mWriteBlockList;
    // BUG-19465 : CM_Buffer�� pending list�� ����
    UInt                          mListLength; /* BUG-44468 [cm] codesonar warning in CM */

    UInt                          mCmSeqNo;

    idBool                        mIsDisconnect;

    /* proj_2160 cm_type removal */
    void                         *mOwner;
    idBool                        mSessionCloseNeeded;

    /* BUG-47078 */
    UInt                          mCompressLevel;
    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ����  */
    /* send�� ������ ���� �ʾ� CompressType�� NONE�̾ ��밡 ������ �ؼ� ���� ��� */ 
    /* recv�ÿ��� � Ÿ������ ������ Ǯ��� ���� �����Ǿ� �־�� �ϱ� ������ */
    /* CompressType�� DecompressType�� ������ ����մϴ�. */
    cmiCompressType               mCompressType;
    cmiCompressType               mDecompressType;
    
    /* BUG-38102 */
    idBool                        mEncryptFlag;

    idBool                        mResendBlock;

    /* PROJ-2616 */
    cmbBlockSimpleQueryFetchIPCDA mSimpleQueryFetchIPCDAWriteBlock;
    acp_uint32_t                  mIPDASpinMaxCount;
    
    /* BUG-45184 network ���� size�� count Ȯ�� (RP View���� Ȯ�ο뵵) */
    ULong               mSendDataSize;
    ULong               mReceiveDataSize;    
    ULong               mSendDataCount;
    ULong               mReceiveDataCount;
} cmiProtocolContext;

/*
 * Initialization
 */

// BUG-19465 : CM_Buffer�� pending list�� ����
IDE_RC cmiInitialize( UInt aCmMaxPendingList = ID_UINT_MAX );
IDE_RC cmiFinalize();

IDE_RC cmiSetCallback(UChar aModuleID, UChar aOpID, cmiCallbackFunction aCallbackFunction);

/*
 * Check Network Implementation Support
 */

idBool cmiIsSupportedLinkImpl(cmiLinkImpl aLinkImpl);
idBool cmiIsSupportedDispatcherImpl(cmiDispatcherImpl aDispatcherImpl);

/*
 * Link
 */

IDE_RC cmiAllocLink(cmiLink **aLink, cmiLinkType aType, cmiLinkImpl aImpl);
IDE_RC cmiFreeLink(cmiLink *aLink);

IDE_RC cmiCloseLink(cmiLink *aLink);

IDE_RC cmiWaitLink(cmiLink *aLink, PDL_Time_Value *aTimeout);

UInt   cmiGetLinkFeature(cmiLink *aLink);

/*
 * Listen Link
 */

IDE_RC cmiListenLink(cmiLink *aLink, cmiListenArg *aListenArg);
IDE_RC cmiAcceptLink(cmiLink *aLinkListen, cmiLink **aLinkPeer);

/*
 * Peer Link
 */

/* PROJ - 1607 */
IDE_RC cmiAllocChannel(cmiLink *aLink, SInt *aChannelID);

IDE_RC cmiHandshake(cmiLink *aLink);

IDE_RC cmiSetLinkBlockingMode(cmiLink *aLink, idBool aBlockingMode);

/* 
 * BUG-38716 
 * [rp-sender] It needs a property to give sending timeout to replication sender.
 */
idBool cmiIsLinkBlockingMode( cmiLink * aLink );

IDE_RC cmiGetLinkInfo(cmiLink *aLink, SChar *aBuf, UInt aBufLen, cmiLinkInfoKey aKey);

/* PROJ-2474 SSL/TLS */
IDE_RC cmiGetPeerCertIssuer(cmnLink *aLink, 
                            SChar *aBuf, 
                            UInt aBufLen);
IDE_RC cmiGetPeerCertSubject(cmnLink *aLink, 
                             SChar *aBuf, 
                             UInt aBufLen);
IDE_RC cmiGetSslCipherInfo(cmnLink *aLink, 
                           SChar *aBuf, 
                           UInt aBufLen);
IDE_RC cmiSslInitialize( void );
IDE_RC cmiSslFinalize( void );

/* PROJ-2681 */
IDE_RC cmiIBInitialize(void);
IDE_RC cmiIBFinalize(void);

IDE_RC cmiCheckLink(cmiLink *aLink, idBool *aIsClosed);

idBool cmiLinkHasPendingRequest(cmiLink *aLink);

idvSession *cmiGetLinkStatistics(cmiLink *aLink);
void        cmiSetLinkStatistics(cmiLink *aLink, idvSession *aStatistics);

void  *cmiGetLinkUserPtr(cmiLink *aLink);
void   cmiSetLinkUserPtr(cmiLink *aLink, void *aUserPtr);

IDE_RC cmiShutdownLink(cmiLink *aLink, cmiDirection aDirection);
// bug-28227: ipc: server stop failed when idle cli exists
IDE_RC cmiShutdownLinkForce(cmiLink *aLink);

/*
 * Dispatcher
 */

cmiDispatcherImpl cmiDispatcherImplForLinkImpl(cmiLinkImpl aLinkImpl);
cmiDispatcherImpl cmiDispatcherImplForLink(cmiLink *aLink);

IDE_RC cmiAllocDispatcher(cmiDispatcher **aDispatcher, cmiDispatcherImpl aImpl, UInt aMaxLink);
IDE_RC cmiFreeDispatcher(cmiDispatcher *aDispatcher);

IDE_RC cmiAddLinkToDispatcher(cmiDispatcher *aDispatcher, cmiLink *aLink);
IDE_RC cmiRemoveLinkFromDispatcher(cmiDispatcher *aDispatcher, cmiLink *aLink);
IDE_RC cmiRemoveAllLinksFromDispatcher(cmiDispatcher *aDispatcher);

IDE_RC cmiSelectDispatcher(cmiDispatcher *aDispatcher, iduList *aReadyList, UInt *aReadyCount, PDL_Time_Value *aTimeout);

/*
 * Session
 */

IDE_RC cmiAddSession(cmiSession *aSession, void *aOwner, UChar aModuleID, cmiProtocolContext *aProtocolContext);
IDE_RC cmiRemoveSession(cmiSession *aSession);

IDE_RC cmiSetLinkForSession(cmiSession *aSession, cmiLink *aLink);
IDE_RC cmiGetLinkForSession(cmiSession *aSession, cmiLink **aLink);

IDE_RC cmiSetOwnerForSession(cmiSession *aSession, void *aOwner);
IDE_RC cmiGetOwnerForSession(cmiSession *aSession, void **aOwner);
IDE_RC cmiGetOwnerForProtocolContext( cmiProtocolContext *aCtx, void **aOwner );
IDE_RC cmiGetLinkForProtocolContext( cmiProtocolContext *aCtx, cmiLink **aLink );
IDE_RC cmiSetOwnerForProtocolContext( cmiProtocolContext *aCtx, void *aOwner );

IDE_RC cmiConnect(cmiProtocolContext *aCtx, cmiConnectArg *aConnectArg, PDL_Time_Value *aTimeout, SInt aOption);
IDE_RC cmiConnectWithoutData(cmiProtocolContext *aCtx, cmiConnectArg *aConnectArg, PDL_Time_Value *aTimeout, SInt aOption);

/*
 * Protocol Initialize
 */
//fix BUG-17947.
IDE_RC cmiInitializeProtocol(cmiProtocol *aProtocol,cmpModule*  aModule, UChar aOperationID);
/*fix BUG-30041 cmiReadProtocol���� mFinalization ���ʱ�ȭ �Ǳ�����
 �����ϴ� case�� cmiFinalization���� ����������˴ϴ�.*/
void  cmiInitializeProtocolNullFinalization(cmiProtocol *aProtocol);

IDE_RC cmiFinalizeProtocol(cmiProtocol *aProtocol);

/*
 * Protocol Context
 */

/* PROJ-2296 for only replication */
/* BUG-38716 Add aIsEnableResendBlock */
IDE_RC cmiInitializeProtocolContext( cmiProtocolContext * aCtx,
                                     UChar                aModuleID,
                                     cmiLink            * aLink,
                                     idBool               aResendBlock = ID_FALSE );
IDE_RC cmiFinalizeProtocolContext( cmiProtocolContext   * aProtocolContext );
    
void   cmiSetProtocolContextLink(cmiProtocolContext *aProtocolContext, cmiLink *aLink);

/* PROJ-2296 for only replication */
void cmiProtocolContextCopyA5Header( cmiProtocolContext * aCtx,
                                     cmpHeader * aHeader );

idBool cmiProtocolContextHasPendingRequest(cmiProtocolContext *aProtocolContext);

/*
 * Read Protocol and Callback
 */

IDE_RC cmiReadProtocolAndCallback(cmiProtocolContext   *aProtocolContext, 
                                  void                 *aUserContext, 
                                  PDL_Time_Value       *aTimeout,
                                  void                 *aTask);

/*
 * Read One Protocol from Protocol Context
 */

IDE_RC cmiReadProtocol(cmiProtocolContext *aProtocolContext, cmiProtocol *aProtocol, PDL_Time_Value *aTimeout, idBool *aIsEnd);

/*
 * Write One Protocol to Protocol Context
 */

IDE_RC cmiWriteProtocol(cmiProtocolContext *aProtocolContext, cmiProtocol *aProtocol, PDL_Time_Value *aTimeout = NULL);

/*
 * Flush Written Protocol
 */

IDE_RC cmiFlushProtocol(cmiProtocolContext *aProtocolContext, idBool aIsEnd, PDL_Time_Value *aTimeout = NULL);

/* BUG-38716 [rp-sender] It needs a property to give sending timeout to replication sender.*/
idBool cmiIsResendBlock( cmiProtocolContext   * aProtocolContext );


// fix BUG-17715
IDE_RC cmiCheckFetch(cmiProtocolContext *aProtocolContext, UInt aRecordSize);

idBool cmiCheckInVariable(cmiProtocolContext *aProtocolContext, UInt aInVariableSize);
idBool cmiCheckInBinary(cmiProtocolContext *aProtocolContext, UInt aInBinarySize);
idBool cmiCheckInBit(cmiProtocolContext *aProtocolContext, UInt aInBitSize);
idBool cmiCheckInNibble(cmiProtocolContext *aProtocolContext, UInt aInNibbleSize);

UChar *cmiGetOpName( UInt aModuleID, UInt aOpID );

cmiLinkImpl cmiGetLinkImpl(cmiProtocolContext *aProtocolContext);

// PROJ-1752 LIST PROTOCOL
UInt cmiGetBindColumnInfoStructSize( void );    
UInt cmiGetMaxInTypeHeaderSize();

// bug-19279 remote sysdba enable + sys can kill session
IDE_RC cmiCheckRemoteAccess(cmiLink* aLink, idBool* aIsRemote);

// BUG-30835 Considerations on IPv6 link-local address followed by zone index (RFC 4007)
idBool cmiIsValidIPFormat(SChar * aIP);


/***********************************************************
 * proj_2160 cm_type removal
 *  new marshaling interface used in MM/CLI for A7 or higher
 * CMI_RD: recv: read  mReadBlock --> buffer
 * CMI_WR: send: write buffer     --> mWriteBlock
***********************************************************/

#ifdef ENDIAN_IS_BIG_ENDIAN

#define CM_ENDIAN_ASSIGN1(dst, src) \
    (dst) = (src)

#define CM_ENDIAN_ASSIGN2(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 1); \
    } while (0)

#define CM_ENDIAN_ASSIGN4(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 1); \
        *((UChar *)(dst) + 2) = *((UChar *)(src) + 2); \
        *((UChar *)(dst) + 3) = *((UChar *)(src) + 3); \
    } while (0)

#define CM_ENDIAN_ASSIGN8(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 1); \
        *((UChar *)(dst) + 2) = *((UChar *)(src) + 2); \
        *((UChar *)(dst) + 3) = *((UChar *)(src) + 3); \
        *((UChar *)(dst) + 4) = *((UChar *)(src) + 4); \
        *((UChar *)(dst) + 5) = *((UChar *)(src) + 5); \
        *((UChar *)(dst) + 6) = *((UChar *)(src) + 6); \
        *((UChar *)(dst) + 7) = *((UChar *)(src) + 7); \
    } while (0)

#else /* LITTLE_ENDIAN */

#define CM_ENDIAN_ASSIGN1(dst, src) \
    (dst) = (src)

#define CM_ENDIAN_ASSIGN2(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 1); \
    } while (0)

#define CM_ENDIAN_ASSIGN4(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 3) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 2) = *((UChar *)(src) + 1); \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 2); \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 3); \
    } while (0)

#define CM_ENDIAN_ASSIGN8(dst, src)                    \
    do                                                 \
    {                                                  \
        *((UChar *)(dst) + 7) = *((UChar *)(src) + 0); \
        *((UChar *)(dst) + 6) = *((UChar *)(src) + 1); \
        *((UChar *)(dst) + 5) = *((UChar *)(src) + 2); \
        *((UChar *)(dst) + 4) = *((UChar *)(src) + 3); \
        *((UChar *)(dst) + 3) = *((UChar *)(src) + 4); \
        *((UChar *)(dst) + 2) = *((UChar *)(src) + 5); \
        *((UChar *)(dst) + 1) = *((UChar *)(src) + 6); \
        *((UChar *)(dst) + 0) = *((UChar *)(src) + 7); \
    } while (0)

#endif /* ENDIAN_IS_BIG_ENDIAN */

static inline void cmEndianAssign2( UShort* aDst, UShort* aSrc)
{
    CM_ENDIAN_ASSIGN2(aDst, aSrc);
}

static inline void cmEndianAssign4( UInt* aDst,   UInt* aSrc)
{
    CM_ENDIAN_ASSIGN4(aDst, aSrc);
}

static inline void cmEndianAssign8( ULong* aDst,  ULong* aSrc)
{
    CM_ENDIAN_ASSIGN8(aDst, aSrc);
}

/* PROJ-2677 peek ���� Ŀ���� �ű��� �ʰ� �о� ���⸸ �Ѵ�. */
#define CMI_PEEK1(aCtx, aDst, aPos)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(aDst) == 1 );                                                          \
        CM_ENDIAN_ASSIGN1((aDst), *((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor + aPos)); \
    } while (0)

#define CMI_PEEK2(aCtx, aDst, aPos)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 2 );                                                         \
        cmEndianAssign2((aDst), (UShort *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor + aPos)); \
    } while (0)

#define CMI_PEEK4(aCtx, aDst, aPos)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 4 );                                                         \
        cmEndianAssign4((aDst), (UInt *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor + aPos)); \
    } while (0)

#define CMI_PEEK8(aCtx, aDst, aPos)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 8 );                                                         \
        cmEndianAssign8((aDst), (ULong *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor + aPos)); \
    } while (0)

/* read */
/* ���� ���� ����Ÿ�� �а�, ���� ���ؼ� ���ȴ�. Align �� ������ �ʰ� 1����Ʈ�� �����Ѵ�. */
#define CMI_RD1(aCtx, aDst)                                                                    \
    do                                                                                         \
    {                                                                                          \
        IDE_DASSERT( ID_SIZEOF(aDst) == 1 );                                                   \
        CM_ENDIAN_ASSIGN1((aDst), *((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor)); \
        (aCtx)->mReadBlock->mCursor += 1;                                                      \
    } while (0)

#define CMI_RD2(aCtx, aDst)                                                                           \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 2 );                                                         \
        cmEndianAssign2((aDst), (UShort *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor)); \
        (aCtx)->mReadBlock->mCursor += 2;                                                             \
    } while (0)

#define CMI_RD4(aCtx, aDst)                                                                         \
    do                                                                                              \
    {                                                                                               \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 4 );                                                       \
        cmEndianAssign4((aDst), (UInt *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor)); \
        (aCtx)->mReadBlock->mCursor += 4;                                                           \
    } while (0)

#define CMI_RD8(aCtx, aDst)                                                                          \
    do                                                                                               \
    {                                                                                                \
        IDE_DASSERT( ID_SIZEOF(*aDst) == 8 );                                                        \
        cmEndianAssign8((aDst), (ULong *)((aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor)); \
        (aCtx)->mReadBlock->mCursor += 8;                                                            \
    } while (0)

#define CMI_RCP(aCtx, aDst, aLen)                                                                 \
    do                                                                                            \
    {                                                                                             \
        if (aLen > 0)                                                                             \
        {                                                                                         \
            idlOS::memcpy((aDst), (aCtx)->mReadBlock->mData + (aCtx)->mReadBlock->mCursor, aLen); \
            (aCtx)->mReadBlock->mCursor += aLen;                                                  \
        }                                                                                         \
    } while (0)

/* write */
#define CMI_WOP(aCtx, aOp)                                                                      \
    do                                                                                          \
    {                                                                                           \
        CMP_DB_PROTOCOL_STAT_ADD((aOp), (1));                                                   \
        CM_ENDIAN_ASSIGN1(*((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor), (aOp)); \
        (aCtx)->mWriteBlock->mCursor += 1;                                                      \
    } while (0)

#define CMI_WR1(aCtx, aSrc)                                                                      \
    do                                                                                           \
    {                                                                                            \
        /* �ι�° ���ڿ� ���ڻ���� �Ѿ���� ��찡 �־ UCHAR_MAX�˻縦 �Ѵ�.*/               \
        IDE_DASSERT( (ID_SIZEOF(aSrc) == 1) || (aSrc <= UCHAR_MAX) );                            \
        CM_ENDIAN_ASSIGN1(*((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor), (aSrc)); \
        (aCtx)->mWriteBlock->mCursor += 1;                                                       \
    } while (0)

#define CMI_WR2(aCtx, aSrc)                                                                             \
    do                                                                                                  \
    {                                                                                                   \
        IDE_DASSERT( ID_SIZEOF(*aSrc) == 2 );                                                           \
        cmEndianAssign2((UShort *)((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor), (aSrc)); \
        (aCtx)->mWriteBlock->mCursor += 2;                                                              \
    } while (0)

#define CMI_WR4(aCtx, aSrc)                                                                           \
    do                                                                                                \
    {                                                                                                 \
        IDE_DASSERT( ID_SIZEOF(*aSrc) == 4 );                                                         \
        cmEndianAssign4((UInt *)((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor), (aSrc)); \
        (aCtx)->mWriteBlock->mCursor += 4;                                                            \
    } while (0)

#define CMI_WR8(aCtx, aSrc)                                                                            \
    do                                                                                                 \
    {                                                                                                  \
        IDE_DASSERT( ID_SIZEOF(*aSrc) == 8 );                                                          \
        cmEndianAssign8((ULong *)((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor), (aSrc)); \
        (aCtx)->mWriteBlock->mCursor += 8;                                                             \
    } while (0)

#define CMI_WCP(aCtx, aSrc, aLen)                                                                     \
    do                                                                                                \
    {                                                                                                 \
        if ((aLen) > 0)                                                                               \
        {                                                                                             \
            IDE_TEST((aCtx)->mWriteBlock->mCursor + (aLen) > (aCtx)->mWriteBlock->mBlockSize);        \
            idlOS::memcpy((aCtx)->mWriteBlock->mData + (aCtx)->mWriteBlock->mCursor, (aSrc), (aLen)); \
            (aCtx)->mWriteBlock->mCursor += (aLen);                                                   \
        }                                                                                             \
    } while (0)

/* BUGBUG
 * aCtx->mWriteBlock->mData�� align�� �̹� ������ �ִٸ�
 * aCtx->mWriteBlock->mCursor = (vULong)idlOS::align8((vULong)aCtx->mWriteBlock->mCursor)�� ������ �����Ҽ� �ִ�.
 * ���� Ȯ���Ͽ� ���� */
#define CMI_WRITE_ALIGN8(aCtx)                                                   \
    ((aCtx)->mWriteBlock->mCursor) +=                                            \
        (vULong)idlOS::align8((vULong)((aCtx)->mWriteBlock->mData +              \
        (aCtx)->mWriteBlock->mCursor)) -                                         \
        (vULong)((aCtx)->mWriteBlock->mData +                                    \
        (aCtx)->mWriteBlock->mCursor)

/* BUG-44705 CMI_WRITE_CHECK
 * aLen �� ���̸�ŭ�� ����Ÿ�� 1���� ����� ������ �ִ��� üũ�Ѵ�.
 * ���������� ���ҵ��� �ʴ� ������ �����ϱ� ���ؼ� ���ȴ�.
 * IPCDA�� ��� �����޼����� ������ �ּ����� ������ �����*/
#define CMI_WRITE_CHECK(aCtx, aLen)                                                       \
    do                                                                                    \
    {                                                                                     \
        if (aCtx->mLink->mLink.mImpl != CMN_LINK_IMPL_IPCDA)                              \
        {                                                                                 \
            IDE_TEST((aLen) > CMB_BLOCK_DEFAULT_SIZE - CMP_HEADER_SIZE);                  \
            if ((aCtx)->mWriteBlock->mCursor + (aLen) > CMB_BLOCK_DEFAULT_SIZE)           \
            {                                                                             \
                IDE_TEST(cmiSend((aCtx), ID_FALSE) != IDE_SUCCESS);                       \
            }                                                                             \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            IDE_TEST(((aCtx)->mWriteBlock->mCursor + (aLen)) >                            \
            (CMB_BLOCK_DEFAULT_SIZE - CMI_IPCDA_REMAIN_PROTOCOL_SIZE));                   \
        }                                                                                 \
    } while (0)

/* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
/* CMI_WRITE_CHECK_WITH_IPCDA
 * aLen �� ���̸�ŭ�� ����Ÿ�� 1���� ����� ������ �ִ��� üũ�Ѵ�.
 * ���������� ���ҵ��� �ʴ� ������ �����ϱ� ���ؼ� ���ȴ�.
 * IPCDA�ϰ�� size�� �ٸ���찡 �����ϱ� ������ �߰� ���ڸ� �޾Ƽ� ó���Ѵ�.
 * IPCDA�� ��� �����޼����� ������ �ּ����� ������ �����*/
#define CMI_WRITE_CHECK_WITH_IPCDA(aCtx, aLen, aLenIPCDA)                                 \
    do                                                                                    \
    {                                                                                     \
        if (aCtx->mLink->mLink.mImpl != CMN_LINK_IMPL_IPCDA)                              \
        {                                                                                 \
            IDE_TEST((aLen) > CMB_BLOCK_DEFAULT_SIZE - CMP_HEADER_SIZE);                  \
            if ((aCtx)->mWriteBlock->mCursor + (aLen) > CMB_BLOCK_DEFAULT_SIZE)           \
            {                                                                             \
                IDE_TEST(cmiSend((aCtx), ID_FALSE) != IDE_SUCCESS);                       \
            }                                                                             \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            IDE_TEST(((aCtx)->mWriteBlock->mCursor + (aLenIPCDA)) >                       \
            (CMB_BLOCK_DEFAULT_SIZE - CMI_IPCDA_REMAIN_PROTOCOL_SIZE));                   \
        }                                                                                 \
    } while (0)


/* PROJ-2733-Protocol IPCDA�� CMI_WRITE_CHECK*���� �����޽��� ������ Ȯ���ϹǷ�
                      AnswerErrorResult���� ����� CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR�� �߰� */
#define CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aCtx, aLen, aLenIPCDA)                       \
    do                                                                                    \
    {                                                                                     \
        if (aCtx->mLink->mLink.mImpl != CMN_LINK_IMPL_IPCDA)                              \
        {                                                                                 \
            IDE_TEST((aLen) > CMB_BLOCK_DEFAULT_SIZE - CMP_HEADER_SIZE);                  \
            if ((aCtx)->mWriteBlock->mCursor + (aLen) > CMB_BLOCK_DEFAULT_SIZE)           \
            {                                                                             \
                IDE_TEST(cmiSend((aCtx), ID_FALSE) != IDE_SUCCESS);                       \
            }                                                                             \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            IDE_TEST(((aCtx)->mWriteBlock->mCursor + (aLenIPCDA)) >=                      \
                      (aCtx)->mWriteBlock->mBlockSize)                                    \
        }                                                                                 \
    } while (0)

#define CMI_IS_READ_ALL(aCtx) \
        (((aCtx)->mReadBlock->mDataSize == (aCtx)->mReadBlock->mCursor) ? ID_TRUE : ID_FALSE)

#define CMI_REMAIN_SPACE_IN_WRITE_BLOCK(aCtx) \
        ((aCtx)->mWriteBlock->mBlockSize - (aCtx)->mWriteBlock->mCursor)

#define CMI_REMAIN_DATA_IN_READ_BLOCK(aCtx) \
        ((aCtx)->mReadBlock->mDataSize - (aCtx)->mReadBlock->mCursor)

#define CMI_SKIP_READ_BLOCK(aCtx, aByte) \
        (((aCtx)->mReadBlock->mCursor) += (aByte))

#define CMI_SKIP_WRITE_BLOCK(aCtx, aByte) \
        (((aCtx)->mWriteBlock->mCursor) += (aByte))

#define CMI_SET_CURSOR(aCtx , aPos) \
        (((aCtx)->mWriteBlock->mCursor) = (aPos))

/* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
#define CMI_GET_CURSOR(aCtx) \
        ((aCtx)->mWriteBlock->mCursor)

#define CMI_GET_CURRENT_POSITION_IN_READ_BLOCK( aCtx ) \
        (UChar *)(((aCtx)->mReadBlock->mData) + ((aCtx)->mReadBlock->mCursor))

/*********************************************************/
// proj_2160 cm_type removal
// cmpHeaderRead is put here because of a compile error
IDE_RC cmpHeaderInitialize(cmpHeader *aHeader);
IDE_RC cmpHeaderInitializeForA5( cmpHeader *aHeader );

IDE_RC cmpHeaderRead(cmnLinkPeer *aLink, cmpHeader *aHeader, cmbBlock *aBlock);
IDE_RC cmpHeaderWrite(cmpHeader *aHeader, cmbBlock *aBlock);

IDE_RC cmiMakeCmBlockNull(cmiProtocolContext *aCtx);
IDE_RC cmiAllocCmBlock(cmiProtocolContext* aCtx,
                       UChar               aModuleID,
                       cmiLink*            aLink,
                       void*               aOwner,
                       idBool              aResendBlock = ID_FALSE);
IDE_RC cmiAllocCmBlockForA5( cmiProtocolContext* aCtx,
                             UChar               aModuleID,
                             cmiLink*            aLink,
                             void*               aOwner,
                             idBool              aResendBlock = ID_FALSE );
/* 
 * PROJ-2704 ����ȭ DDL ���� ������ ���� �غ� 
 * cmiProtocolContext�� cmBlock�� �Ҵ� �ߴ��� Ȯ���ϴ� �Լ� �߰� */
inline idBool cmiIsAllocCmBlock( cmiProtocolContext* aCtx )
{
    return ( aCtx->mReadBlock != NULL ) ? ID_TRUE : ID_FALSE;
}
IDE_RC cmiFreeCmBlock(cmiProtocolContext* aCtx);
IDE_RC cmiRecv(cmiProtocolContext* aCtx,
               void*           aUserContext,
               PDL_Time_Value* aTimeout,
               void*           aTask); /* bug-33841 */
IDE_RC cmiRecvIPCDA(cmiProtocolContext *aCtx,
                    void               *aUserContext,
                    UInt                aMicroSleepTime);
#if defined(ALTI_CFG_OS_LINUX)
IDE_RC cmiMessageQNotify(cmnLinkPeerIPCDA *aLink);
#endif
IDE_RC cmiRecvNext(cmiProtocolContext* aCtx, PDL_Time_Value* aTimeout);
IDE_RC cmiSend( cmiProtocolContext  * aCtx, 
                idBool                aIsEnd, 
                PDL_Time_Value      * aTimeout = NULL );

IDE_RC cmiLinkPeerFinalizeSvrForIPCDA( cmiProtocolContext  *aCtx );

/* 
 * BUG-38716 [rp-sender] It needs a property to give sending timeout to replication sender.
 */
IDE_RC cmiFlushPendingBlock( cmiProtocolContext * aCtx,
                             PDL_Time_Value     * aTimeout );

/*********************************************************/
/* mmtServiceThread can't access directly to cmnLinkPeer.
 * so it needs this function. */
static inline cmpPacketType cmiGetPacketType(cmiProtocolContext* aCtx)
{
    return aCtx->mLink->mLink.mPacketType;
}

IDE_RC cmiCheckAndFlush( cmiProtocolContext     * aProtocolContext, 
                         UInt                     aLen, 
                         idBool                   aIsEnd,
                         PDL_Time_Value         * aTimeout = NULL );

IDE_RC cmiSplitRead( cmiProtocolContext *aCtx,
                     ULong               aReadSize,
                     UChar              *aBuffer,
                     PDL_Time_Value     *aTimeout );

IDE_RC cmiSplitSkipRead( cmiProtocolContext *aCtx,
                         ULong               aReadSize,
                         PDL_Time_Value     *aTimeout );

IDE_RC cmiSplitWrite( cmiProtocolContext *aCtx,
                      ULong               aWriteSize,
                      UChar              *aBuffer );

/* TASK-5894 Permit sysdba via IPC */
IDE_RC cmiPermitConnection(cmiLink *aLink,
                           idBool   aHasSysdba,
                           idBool   aIsSysdba);

cmnLinkImpl cmiGetLinkImpl(cmiLink *aLink);

IDE_RC cmiSetCallbackSetExecute(cmiCallbackSetExecute aCallback);

/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ����  */
void cmiEnableCompress( cmiProtocolContext * aCtx, 
                        UInt                 aLevel,
                        cmiCompressType      aCompressType );
void cmiDisableCompress( cmiProtocolContext * aCtx );
void cmiSetDecompressType( cmiProtocolContext * aCtx,
                           cmiCompressType      aDecompressType );

/* BUG-38102 */
void cmiEnableEncrypt( cmiProtocolContext * aCtx );
void cmiDisableEncrypt( cmiProtocolContext * aCtx );

void cmiLinkSetPacketTypeA5( cmiLink *aLink );

/**************************************************
 * PROJ-2616
 * IPCDA�� cmiSplitRead�� ������� �ʴ´�.
 *
 * aCtx[in]      - cmiProtocolContext
 * aReadSize[in] - acp_uint64_t ���� ������ ������
 * aBuffer1[in]  - acp_uint8_t  �������� ����� �ּҸ� �����Ѵ�.
 * aBuffer2[in]  - acp_uint8_t  �����Ͱ� copy�ȴ�.
 **************************************************/
inline IDE_RC cmiSplitReadIPCDA( cmiProtocolContext  *aCtx,
                                 ULong                aReadSize,
                                 UChar              **aBuffer1,
                                 UChar               *aBuffer2)
{
    UInt sEndPos = aCtx->mReadBlock->mCursor + aReadSize;

    IDE_TEST( sEndPos >= CMB_BLOCK_DEFAULT_SIZE);

    if (aBuffer2 != NULL)
    {
        CMI_RCP( aCtx, aBuffer2, aReadSize );
    }
    else
    {
        *aBuffer1 = (UChar*)(aCtx->mReadBlock->mData + aCtx->mReadBlock->mCursor);
        aCtx->mReadBlock->mCursor += aReadSize;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * Increase packet-data-count
 *
 * aCtx [in] - cmiProtocolContext
 **************************************************************/
inline void cmiIPCDAIncDataCount(cmiProtocolContext * aCtx)
{
    cmbBlockIPCDA *sWriteBlock = (cmbBlockIPCDA*)aCtx->mWriteBlock;
    /*BUG-44275 "IPCDA select test ���� fetch �̻�"
     * mDataSize, mOperationCount ���� �����ϱ� ���Ͽ�
     * mem barrier ��ġ ���� */
    sWriteBlock->mBlock.mDataSize = sWriteBlock->mBlock.mCursor;
    /* BUG-46502 atomic �Լ� ���� atomic�� mem_barrier�� ���ԵǾ� �ִ�.*/
    acpAtomicInc32(&(sWriteBlock->mOperationCount));
}
#endif
