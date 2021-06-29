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

#include <cmAllClient.h>

#if !defined(CM_DISABLE_IPC)

/*
 * IPC Channel Information
 */
extern acp_thr_mutex_t    gIpcMutex;
extern cmbShmInfo         gIpcShmInfo;
extern acp_sint32_t       gIpcClientCount;
extern acp_sint8_t       *gIpcShmBuffer;
extern acp_key_t          gIpcShmKey;
extern acp_sint32_t       gIpcShmID;
extern acp_key_t         *gIpcSemChannelKey;
extern acp_sint32_t      *gIpcSemChannelID;

/*
 * IPC Handshake Message
 */
#define IPC_HELLO_MSG    "IPC_HELLO"
#define IPC_READY_MSG    "IPC_READY"
#define IPC_PROTO_CNT    (9)

typedef struct cmnLinkPeerIPC
{
    cmnLinkPeer    mLinkPeer;
    cmnLinkDescIPC mDesc;
    acp_sint8_t*   mLastWriteBuffer; /* Ŭ���̾�Ʈ ������ ����� ������ write�� ������ ���� �ּ� */
    acp_sint8_t*   mPrevWriteBuffer; /* ���������� write�� shared memory ���� �����ּ� */
    acp_sint8_t*   mPrevReadBuffer;  /* ���������� read�� shared memory ���� �����ּ� */
} cmnLinkPeerIPC;

ACI_RC cmnLinkPeerInitializeClientIPC(cmnLink *aLink)
{
    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;

    /*
     * ��� �ʱ�ȭ
     */

    sLink->mLastWriteBuffer = NULL;
    sLink->mPrevWriteBuffer = NULL;
    sLink->mPrevReadBuffer  = NULL;

    sDesc->mConnectFlag   = ACP_FALSE;
    sDesc->mHandShakeFlag = ACP_FALSE;

    sDesc->mSock.mHandle  = CMN_INVALID_SOCKET_HANDLE;
    sDesc->mChannelID     = -1;

    sDesc->mSemChannelKey = -1;
    sDesc->mSemChannelID  = -1;

    acpMemSet(&sDesc->mSemInfo, 0, ACI_SIZEOF(struct semid_ds));

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerFinalizeIPC(cmnLink *aLink)
{
    ACI_TEST(aLink->mOp->mClose(aLink) != ACI_SUCCESS);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

/*
 * !!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Ŭ���̾�Ʈ�� ���Ḧ ��ٸ� �� *�����* mutex�� ��Ƽ���
 * �ȵȴ�. mutex�� ��� ���Ḧ ��ٸ� ��� �ý��� ��ü��
 * ������ �� �ִ�.
 * PR-4407
 */

ACI_RC cmnLinkPeerCloseClientIPC(cmnLink *aLink)
{
    cmnLinkPeerIPC    *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC    *sDesc = &sLink->mDesc;
    cmbShmChannelInfo *sChannelInfo;
    acp_sint32_t       rc;
    struct sembuf      sOp;

    /*
     * BUG-34140
     * A IPC channels may be hanged when try to the IPC connecting the server which is starting.
     */
    sOp.sem_num = IPC_SEM_SEND_TO_SVR;
    sOp.sem_op  = 100;
    sOp.sem_flg = 0;

    if(sDesc->mConnectFlag == ACP_TRUE)
    {
        if (sDesc->mHandShakeFlag == ACP_TRUE)
        {
            sChannelInfo = cmbShmGetChannelInfo(gIpcShmBuffer, sDesc->mChannelID);

            /* bug-29324 channel not closed when a client alive after disconn
             * before: timestamp�� ���Ͽ� ���� ��츸 ����ó���� ������.
             * after :
             * 1. ����ó���� �� �ʿ��� �����̹Ƿ� timestamp �񱳸� ������.
             * 2. �����ȣ �۽� ����(mOpSignCliExit)�� ���� �������� �ǵ��� ����
             */

            /* BUG-32398 Ÿ�ӽ�����(Ƽ�Ϲ�ȣ) �񱳺κ� ���� */
            if (sDesc->mTicketNum == sChannelInfo->mTicketNum)
            {
                /*
                 * bug-27250 free Buf list can be crushed.
                 * wake up server sending next packet of big data
                 */
                while(1)
                {
                    rc = semop(sDesc->mSemChannelID, sDesc->mOpSignSendMoreToCli, 1);

                    if (rc != 0)
                    {
                        ACI_TEST_RAISE(errno != EINTR, SemOpError);
                    }
                    else
                    {
                        break;
                    }
                }

                while(1)
                {
                    /*
                     * BUG-34140
                     * A IPC channels may be hanged when try to the IPC connecting the server which is starting.
                     */
                    // rc = semop(sDesc->mSemChannelID, sDesc->mOpSignSendToSvr, 1);
                    rc = semop( sDesc->mSemChannelID, &sOp, 1 );

                    if (rc != 0)
                    {
                        ACI_TEST_RAISE(errno != EINTR, SemOpError);
                    }
                    else
                    {
                        break;
                    }
                }

                while(1)
                {
                    rc = semop(sDesc->mSemChannelID, sDesc->mOpSignCliExit, 1);

                    if (rc != 0)
                    {
                        ACI_TEST_RAISE(errno != EINTR, SemOpError);
                    }
                    else
                    {
                        break;
                    }
                }
            }  /* if (sDesc->mTicketNum == sChannelInfo->mTicketNum) */
        }
    }

    /*
     * socket�� ���������� ����
     */

    if (sLink->mDesc.mSock.mHandle != CMN_INVALID_SOCKET_HANDLE)
    {
        ACI_TEST(acpSockClose(&sLink->mDesc.mSock) != ACP_RC_SUCCESS);
        sLink->mDesc.mSock.mHandle = CMN_INVALID_SOCKET_HANDLE;
    }

    sLink->mDesc.mConnectFlag = ACP_FALSE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerGetSockIPC(cmnLink *aLink, void **aHandle)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aHandle);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerGetDispatchInfoIPC(cmnLink * aLink, void * aDispatchInfo)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aDispatchInfo);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSetDispatchInfoIPC(cmnLink * aLink, void * aDispatchInfo)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aDispatchInfo);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSetBlockingModeIPC(cmnLinkPeer * aLink, acp_bool_t aBlockingMode)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlockingMode);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerGetInfoIPC(cmnLinkPeer * aLink, acp_char_t *aBuf, acp_uint32_t aBufLen, cmiLinkInfoKey aKey)
{
    acp_sint32_t sRet;

    ACP_UNUSED(aLink);

    switch (aKey)
    {
        case CMN_LINK_INFO_ALL:
        case CMN_LINK_INFO_IMPL:
        case CMN_LINK_INFO_IPC_KEY:
            sRet = acpSnprintf(aBuf, aBufLen, "IPC");

            ACI_TEST_RAISE(ACP_RC_IS_ETRUNC(sRet), StringTruncated);
            ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRet), StringOutputError);

            break;

        default:
            ACI_RAISE(UnsupportedLinkInfoKey);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(StringOutputError);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_STRING_OUTPUT_ERROR));
    }
    ACI_EXCEPTION(StringTruncated);
    {
        ACI_SET(aciSetErrorCode(cmERR_IGNORE_STRING_TRUNCATED));
    }
    ACI_EXCEPTION(UnsupportedLinkInfoKey);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_UNSUPPORTED_LINK_INFO_KEY));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerGetDescIPC(cmnLinkPeer *aLink, void *aDesc)
{
    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;

    /*
     *  Desc�� ������
     */
    *(cmnLinkDescIPC **)aDesc = &sLink->mDesc;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerConnectIPC(cmnLinkPeer       *aLink,
                             cmnLinkConnectArg *aConnectArg,
                             acp_time_t         aTimeout,
                             acp_sint32_t       aOption)
{
    cmnLinkPeerIPC    *sLink = (cmnLinkPeerIPC *)aLink;
    acp_rc_t           sRet;

    ACP_UNUSED(aOption);

    /*
     * socket ����
     */

    sRet = acpSockOpen(&sLink->mDesc.mSock,
                       ACP_AF_UNIX,
                       ACP_SOCK_STREAM,
                       0);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRet), SocketError);

    /*
     * IPC �ּ� ����
     */

    sLink->mDesc.mAddr.sun_family = AF_UNIX;
    sRet = acpSnprintf(sLink->mDesc.mAddr.sun_path,
                       ACI_SIZEOF(sLink->mDesc.mAddr.sun_path),
                       "%s",
                       aConnectArg->mIPC.mFilePath);

    /*
     *  IPC �����̸� ���� �˻�
     */

    ACI_TEST_RAISE(ACP_RC_IS_ETRUNC(sRet), IpcPathTruncated);
    ACI_TEST(ACP_RC_NOT_SUCCESS(sRet));

    /*
     * connect
     */

    sRet = acpSockTimedConnect(&sLink->mDesc.mSock,
                               (acp_sock_addr_t *)(&sLink->mDesc.mAddr),
                               ACI_SIZEOF(sLink->mDesc.mAddr),
                               aTimeout);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRet), ConnectError);

    sLink->mDesc.mConnectFlag = ACP_TRUE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(SocketError);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SOCKET_OPEN_ERROR, sRet));
    }
    ACI_EXCEPTION(IpcPathTruncated);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_UNIX_PATH_TRUNCATED));
    }
    ACI_EXCEPTION(ConnectError);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECT_ERROR, sRet));
    }
    ACI_EXCEPTION_END;

    if (sLink->mDesc.mSock.mHandle != CMN_INVALID_SOCKET_HANDLE)
    {
        (void)acpSockClose(&sLink->mDesc.mSock);
        sLink->mDesc.mSock.mHandle = CMN_INVALID_SOCKET_HANDLE;
    }

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerAllocChannelClientIPC(cmnLinkPeer *aLink, acp_sint32_t *aChannelID)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aChannelID);

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerSetOperation(cmnLinkDescIPC *aDesc)
{
    /*
     * =====================================================
     * bug-28340 rename semop name for readability
     * �������� ���� semaphore op �������� ������ ���� ����
     * =====================================================
     * IPC_SEM_SERVER_DETECT  > IPC_SEM_CHECK_SVR_EXIT (0)
     * server_detect_init     > InitSvrExit  : ������ ��� ����
     * server_detect_try      > CheckSvrExit : ������ �׾����� Ȯ��
     * server_detect_release  > SignSvrExit  : ������ �����ȣ ����
     * =====================================================
     * IPC_SEM_CLIENT_DETECT  > IPC_SEM_CHECK_CLI_EXIT (1)
     * client_detect_init     > InitCliExit  : cli�� ��� ����
     * client_detect_try      > CheckCliExit : cli�� �׾����� Ȯ��
     * client_detect_hold     > WaitCliExit  : cli ���ᶧ���� ���
     * client_detect_release  > SignCliExit  : cli�� �����ȣ ����
     * =====================================================
     * IPC_SEM_SERVER_CHANNEL > IPC_SEM_SEND_TO_SVR (2)
     * server_channel_init    > InitSendToSvr :cli�� ��� ����
     * server_channel_hold    > WaitSendToSvr :������ ���Ŵ��
     * server_channel_release > SignSendToSvr :cli�� �����������
     * =====================================================
     * IPC_SEM_CLIENT_CHANNLE > IPC_SEM_SEND_TO_CLI (3)
     * client_channel_init    > InitSendToCli :������ ��� ����
     * client_channel_hold    > WaitSendToCli :cli�� ���Ŵ��
     * client_channel_release > SignSendToCli :������ cli�������
     * =====================================================
     * IPC_SEM_CLI_SENDMORE  > IPC_SEM_SENDMORE_TO_SVR (4)
     * cli_sendmore_init     > InitSendMoreToSvr  :������ ��� ����
     * cli_sendmore_try      > CheckSendMoreToSvr :cli�� �۽Žõ�
     * cli_sendmore_hold     > WaitSendMoreToSvr  :cli�� �۽Ŵ��
     * cli_sendmore_release  > SignSendMoreToSvr  :������ �۽����
     * =====================================================
     * IPC_SEM_SVR_SENDMORE  > IPC_SEM_SENDMORE_TO_CLI (5)
     * svr_sendmore_init     > InitSendMoreToCli  :cli�� ��� ����
     * svr_sendmore_try      > CheckSendMoreToCli :������ �۽Žõ�
     * svr_sendmore_hold     > WaitSendMoreToCli  :������ �۽Ŵ��
     * svr_sendmore_release  > SignSendMoreToCli  :cli�� �۽����
     * =====================================================
     */

    /*
     * =====================================================
     * IPC_SEM_CHECK_SVR_EXIT (0)
     */
    aDesc->mOpInitSvrExit[0].sem_num = IPC_SEM_CHECK_SVR_EXIT;
    aDesc->mOpInitSvrExit[0].sem_op  = -1;
    aDesc->mOpInitSvrExit[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckSvrExit[0].sem_num = IPC_SEM_CHECK_SVR_EXIT;
    aDesc->mOpCheckSvrExit[0].sem_op  = -1;
    aDesc->mOpCheckSvrExit[0].sem_flg = IPC_NOWAIT;
    aDesc->mOpCheckSvrExit[1].sem_num = IPC_SEM_CHECK_SVR_EXIT;
    aDesc->mOpCheckSvrExit[1].sem_op  = 1;
    aDesc->mOpCheckSvrExit[1].sem_flg = 0;

    aDesc->mOpSignSvrExit[0].sem_num = IPC_SEM_CHECK_SVR_EXIT;
    aDesc->mOpSignSvrExit[0].sem_op  = 1;
    aDesc->mOpSignSvrExit[0].sem_flg = 0;

    /*
     * =====================================================
     * IPC_SEM_CHECK_CLI_EXIT (1)
     */
    aDesc->mOpInitCliExit[0].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpInitCliExit[0].sem_op  = -1;
    aDesc->mOpInitCliExit[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckCliExit[0].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpCheckCliExit[0].sem_op  = -1;
    aDesc->mOpCheckCliExit[0].sem_flg = IPC_NOWAIT;
    aDesc->mOpCheckCliExit[1].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpCheckCliExit[1].sem_op  =  1;
    aDesc->mOpCheckCliExit[1].sem_flg = 0;

    aDesc->mOpWaitCliExit[0].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpWaitCliExit[0].sem_op  = -1;
    aDesc->mOpWaitCliExit[0].sem_flg = 0;
    aDesc->mOpWaitCliExit[1].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpWaitCliExit[1].sem_op  =  1;
    aDesc->mOpWaitCliExit[1].sem_flg = 0;

    aDesc->mOpSignCliExit[0].sem_num = IPC_SEM_CHECK_CLI_EXIT;
    aDesc->mOpSignCliExit[0].sem_op  = 1;
    aDesc->mOpSignCliExit[0].sem_flg = 0;

    /* =====================================================
     * IPC_SEM_SEND_TO_SVR (2)
     */
    aDesc->mOpInitSendToSvr[0].sem_num = IPC_SEM_SEND_TO_SVR;
    aDesc->mOpInitSendToSvr[0].sem_op  = (-1 * gIpcShmInfo.mMaxBufferCount);
    aDesc->mOpInitSendToSvr[0].sem_flg = SEM_UNDO;

    aDesc->mOpWaitSendToSvr[0].sem_num = IPC_SEM_SEND_TO_SVR;
    aDesc->mOpWaitSendToSvr[0].sem_op  = -1;
    aDesc->mOpWaitSendToSvr[0].sem_flg = 0;

    aDesc->mOpSignSendToSvr[0].sem_num = IPC_SEM_SEND_TO_SVR;
    aDesc->mOpSignSendToSvr[0].sem_op  = 1;
    aDesc->mOpSignSendToSvr[0].sem_flg = 0;

    /* =====================================================
     * IPC_SEM_SEND_TO_CLI (3)
     */
    aDesc->mOpInitSendToCli[0].sem_num = IPC_SEM_SEND_TO_CLI;
    aDesc->mOpInitSendToCli[0].sem_op  = (-1 * gIpcShmInfo.mMaxBufferCount);
    aDesc->mOpInitSendToCli[0].sem_flg = SEM_UNDO;

    aDesc->mOpWaitSendToCli[0].sem_num = IPC_SEM_SEND_TO_CLI;
    aDesc->mOpWaitSendToCli[0].sem_op  = -1;
    aDesc->mOpWaitSendToCli[0].sem_flg = 0;

    aDesc->mOpSignSendToCli[0].sem_num = IPC_SEM_SEND_TO_CLI;
    aDesc->mOpSignSendToCli[0].sem_op  = 1;
    aDesc->mOpSignSendToCli[0].sem_flg = 0;

    /*
     * bug-27250 free Buf list can be crushed when client killed
     * ū �������ݿ� ���� ���� ��Ŷ �۽� ��ȣ ����� semaphore
     * =====================================================
     * IPC_SEM_SENDMORE_TO_SVR (4)
     */
    aDesc->mOpInitSendMoreToSvr[0].sem_num = IPC_SEM_SENDMORE_TO_SVR;
    aDesc->mOpInitSendMoreToSvr[0].sem_op  = (-1 * gIpcShmInfo.mMaxBufferCount);
    aDesc->mOpInitSendMoreToSvr[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckSendMoreToSvr[0].sem_num = IPC_SEM_SENDMORE_TO_SVR;
    aDesc->mOpCheckSendMoreToSvr[0].sem_op  = -1;
    aDesc->mOpCheckSendMoreToSvr[0].sem_flg = IPC_NOWAIT;

    /*
     * cmnDispatcherWaitLink ���� ���Ѵ�� �ϱ����� ���.
     */
    aDesc->mOpWaitSendMoreToSvr[0].sem_num = IPC_SEM_SENDMORE_TO_SVR;
    aDesc->mOpWaitSendMoreToSvr[0].sem_op  = -1;
    aDesc->mOpWaitSendMoreToSvr[0].sem_flg = 0;
    aDesc->mOpWaitSendMoreToSvr[1].sem_num = IPC_SEM_SENDMORE_TO_SVR;
    aDesc->mOpWaitSendMoreToSvr[1].sem_op  = 1;
    aDesc->mOpWaitSendMoreToSvr[1].sem_flg = 0;

    aDesc->mOpSignSendMoreToSvr[0].sem_num = IPC_SEM_SENDMORE_TO_SVR;
    aDesc->mOpSignSendMoreToSvr[0].sem_op  = 1;
    aDesc->mOpSignSendMoreToSvr[0].sem_flg = 0;

    /*
     * =====================================================
     * IPC_SEM_SENDMORE_TO_CLI (5)
     */
    aDesc->mOpInitSendMoreToCli[0].sem_num = IPC_SEM_SENDMORE_TO_CLI;
    aDesc->mOpInitSendMoreToCli[0].sem_op  = (-1 * gIpcShmInfo.mMaxBufferCount);
    aDesc->mOpInitSendMoreToCli[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckSendMoreToCli[0].sem_num = IPC_SEM_SENDMORE_TO_CLI;
    aDesc->mOpCheckSendMoreToCli[0].sem_op  = -1;
    aDesc->mOpCheckSendMoreToCli[0].sem_flg = IPC_NOWAIT;

    /*
     * cmnDispatcherWaitLink ���� ���Ѵ�� �ϱ����� ���.
     */
    aDesc->mOpWaitSendMoreToCli[0].sem_num = IPC_SEM_SENDMORE_TO_CLI;
    aDesc->mOpWaitSendMoreToCli[0].sem_op  = -1;
    aDesc->mOpWaitSendMoreToCli[0].sem_flg = 0;
    aDesc->mOpWaitSendMoreToCli[1].sem_num = IPC_SEM_SENDMORE_TO_CLI;
    aDesc->mOpWaitSendMoreToCli[1].sem_op  = 1;
    aDesc->mOpWaitSendMoreToCli[1].sem_flg = 0;

    aDesc->mOpSignSendMoreToCli[0].sem_num = IPC_SEM_SENDMORE_TO_CLI;
    aDesc->mOpSignSendMoreToCli[0].sem_op  = 1;
    aDesc->mOpSignSendMoreToCli[0].sem_flg = 0;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSyncClientIPC(cmnLinkDescIPC *aDesc)
{
    acp_sint32_t rc;

    /*
     * �����ϱ� ���� ����ȭ
     */
    while(1)
    {
        rc = semop(aDesc->mSemChannelID, aDesc->mOpWaitSendToCli, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST_RAISE(errno != EINTR, SemOpError);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerHandshakeClientIPC(cmnLinkPeer *aLink)
{
    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;

    acp_sint32_t       rc;
    acp_size_t         sCount = 0;
    acp_sint32_t       sFlag = 0666;
    acp_sint32_t       sSemCount = 0;
    acp_sint8_t        sProtoBuffer[20];
    acp_bool_t         sLocked = ACP_FALSE;
    cmnLinkDescIPCMsg  sIpcMsg;
    cmbShmChannelInfo *sChannelInfo;
    // BUG-32625 32bit Client IPC Connection Error
    union semun {
        struct semid_ds *buf;
    } sArg;

    /*
     * send hello message
     */

    acpMemCpy(sProtoBuffer, IPC_HELLO_MSG, IPC_PROTO_CNT);

    ACI_TEST(acpSockSend(&sDesc->mSock,
                         sProtoBuffer,
                         IPC_PROTO_CNT,
                         &sCount,
                         0) != ACP_RC_SUCCESS);
    ACI_TEST( sCount != IPC_PROTO_CNT);

    /*
     * receive ipc protocol message
     */

    ACI_TEST(acpSockRecv(&sDesc->mSock,
                         &sIpcMsg,
                         ACI_SIZEOF(cmnLinkDescIPCMsg),
                         &sCount,
                         0) != ACP_RC_SUCCESS);

    ACI_TEST(sCount != ACI_SIZEOF(cmnLinkDescIPCMsg));

    ACI_TEST_RAISE(sIpcMsg.mChannelID < 0, NoChannelError);

    /*
     * CAUTION: don't move
     * if do, it causes network deadlock in MT_threaded Clinets
     */

    ACI_TEST(acpThrMutexLock(&gIpcMutex) != ACP_RC_SUCCESS);

    sLocked = ACP_TRUE;

    sDesc->mChannelID = sIpcMsg.mChannelID;

    if (gIpcShmInfo.mMaxChannelCount != sIpcMsg.mMaxChannelCount)
    {
        gIpcShmInfo.mMaxChannelCount = sIpcMsg.mMaxChannelCount;
    }

    if (gIpcShmInfo.mMaxBufferCount != sIpcMsg.mMaxBufferCount)
    {
        gIpcShmInfo.mMaxBufferCount = sIpcMsg.mMaxBufferCount;
    }

    if (gIpcShmKey != sIpcMsg.mShmKey)
    {
        gIpcShmKey           = sIpcMsg.mShmKey;
        sLink->mDesc.mShmKey = sIpcMsg.mShmKey;

        gIpcShmInfo.mMaxChannelCount  = sIpcMsg.mMaxChannelCount;
        gIpcShmInfo.mUsedChannelCount = 0;

        gIpcShmID = -1;
        ACI_TEST(cmbShmDetach() != ACI_SUCCESS);
    }

    /*
     * attach shared memory
     */
    ACI_TEST(cmbShmAttach() != ACI_SUCCESS);

    /*
     * attach channel semaphores
     */

    if (sDesc->mSemChannelKey != sIpcMsg.mSemChannelKey)
    {
        sDesc->mSemChannelKey = sIpcMsg.mSemChannelKey;
        sSemCount             = IPC_SEMCNT_PER_CHANNEL;
        sDesc->mSemChannelID  = -1;
    }

    if (sDesc->mSemChannelID == -1)
    {
        /*
         * BUGBUG
         */
        sDesc->mSemChannelID = semget(sDesc->mSemChannelKey, sSemCount, sFlag);
        ACI_TEST(sLink->mDesc.mSemChannelID < 0);
    }

    cmnLinkPeerSetOperation(sDesc);

    // BUG-32625 32bit Client IPC Connection Error
    sArg.buf = &sDesc->mSemInfo;
    ACI_TEST(semctl(sDesc->mSemChannelID, 0, IPC_STAT, sArg) != 0);

    /*
     * set client semaphore to detect client dead
     */

    while(1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpInitCliExit, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST(errno != EINTR);
        }
    }

    /*
     * hold server channel to prevent server's starting
     */

    while(1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpInitSendToSvr, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST(errno != EINTR);
        }
    }

    /*
     * bug-27250 free Buf list can be crushed when client killed
     * hold server send for big protocol
     */
    while(1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpInitSendMoreToCli, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST(errno != EINTR);
        }
    }

    /*
     * send ready msg
     */

    acpMemCpy(sProtoBuffer, IPC_READY_MSG, IPC_PROTO_CNT);

    ACI_TEST(acpSockSend(&sDesc->mSock,
                         sProtoBuffer,
                         IPC_PROTO_CNT,
                         &sCount,
                         0) != ACP_RC_SUCCESS);

    ACI_TEST( sCount != IPC_PROTO_CNT);

    gIpcClientCount++;

    sChannelInfo = cmbShmGetChannelInfo(gIpcShmBuffer, sDesc->mChannelID);
    sDesc->mTicketNum = sChannelInfo->mTicketNum;

    /*
     * BUG-25420 [CodeSonar] Lock, Unlock ���� �ڵ鸵 ������ ���� Double Unlock
     * unlock �� �ϱ����� �������ؾ� Double Unlock �� ������ �ִ�.
     */
    sLocked = ACP_FALSE;
    rc = acpThrMutexUnlock(&gIpcMutex);
    if (ACP_RC_NOT_SUCCESS(rc))
    {
        gIpcClientCount--;
        ACI_RAISE(ACI_EXCEPTION_END_LABEL);
    }

    /*
     * Sync after Establish Connection
     */

    ACI_TEST(cmnLinkPeerSyncClientIPC(sDesc) != ACI_SUCCESS);

    sDesc->mHandShakeFlag = ACP_TRUE;

    /*
     * fix BUG-18833
     */
    if (sDesc->mSock.mHandle != CMN_INVALID_SOCKET_HANDLE)
    {
        ACI_TEST(acpSockClose(&sLink->mDesc.mSock) != ACP_RC_SUCCESS);
        sDesc->mSock.mHandle = CMN_INVALID_SOCKET_HANDLE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(NoChannelError);
    {
        /* 
         * TASK-5894 Permit sysdba via IPC
         *
         * cm-ipc ������ �������� ���� �Ͱ� IPC ä���� ���� ���� �����Ѵ�.
         */
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CMN_ERR_FULL_IPC_CHANNEL));
    }
    ACI_EXCEPTION_END;

    if (sLocked == ACP_TRUE)
    {
        ACE_ASSERT(acpThrMutexUnlock(&gIpcMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerSetOptionsIPC(cmnLinkPeer * aLink, acp_sint32_t aOption)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aOption);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerShutdownClientIPC(cmnLinkPeer    *aLink,
                                    cmnDirection    aDirection,
                                    cmnShutdownMode aMode)
{
    cmnLinkPeerIPC    *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC    *sDesc = &sLink->mDesc;
    cmbShmChannelInfo *sChannelInfo;
    acp_sint32_t       rc;

    ACP_UNUSED(aDirection);
    ACP_UNUSED(aMode);

    if (sDesc->mHandShakeFlag == ACP_TRUE)
    {
        /*
         * clinet�� close flag�� on : ��, detect sem = 1
         */

        sChannelInfo = cmbShmGetChannelInfo(gIpcShmBuffer, sDesc->mChannelID);

        /* BUG-32398 Ÿ�ӽ�����(Ƽ�Ϲ�ȣ) �񱳺κ� ���� */
        if (sDesc->mTicketNum == sChannelInfo->mTicketNum)
        {
            /*
             * Client�� Connection�� ��ȿ�� ��쿡�� close ������ �����ض�.
             * bug-27162: ipc server,client hang
             * ���� ����
             * why: server read���� client ���� �����κ� ����
             * before:
             * release server read waiting -> mark client exited
             * after:
             * mark client exited -> release server read waiting
             */

            while(1)
            {
                rc = semop(sDesc->mSemChannelID,
                           sDesc->mOpSignCliExit,
                           1);

                if (rc != 0)
                {
                    ACI_TEST_RAISE(errno != EINTR, SemOpError);
                }
                else
                {
                    break;
                }
            }

            /*
             * bug-27250 free Buf list can be crushed when client killed.
             * wake up server sending next packet of big data
             */
            while(1)
            {
                rc = semop(sDesc->mSemChannelID,
                           sDesc->mOpSignSendMoreToCli,
                           1);
                if (rc != 0)
                {
                    ACI_TEST_RAISE(errno != EINTR, SemOpError);
                }
                else
                {
                    break;
                }
            }

            while(1)
            {
                rc = semop(sDesc->mSemChannelID,
                           sDesc->mOpSignSendToSvr,
                           1);

                if (rc != 0)
                {
                    ACI_TEST_RAISE(errno != EINTR, SemOpError);
                }
                else
                {
                    break;
                }
            }
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerAllocBlockClientIPC(cmnLinkPeer *aLink, cmbBlock **aBlock)
{
    /*
     * bug-27250 free Buf list can be crushed when client killed
     * modify: to be same as tcp
     * ������: ���� �޸� block�� �Ҵ�
     * ������:  mAllocBlock���� �Ϲ� block buffer�� �Ҵ�
     */
    cmbBlock *sBlock;

    ACI_TEST(aLink->mPool->mOp->mAllocBlock(aLink->mPool, &sBlock) != ACI_SUCCESS);

    /*
     * Write Block �ʱ�ȭ
     */
    sBlock->mDataSize    = CMP_HEADER_SIZE;
    sBlock->mCursor      = CMP_HEADER_SIZE;

    /*
     *  Write Block�� ������
     */
    *aBlock = sBlock;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerFreeBlockClientIPC(cmnLinkPeer *aLink, cmbBlock *aBlock)
{
    /*
     * bug-27250 free Buf list can be crushed when client killed
     * ���� �޸� linked list �����ϴ� �κ� ����
     * Block ����
     */
    ACI_TEST(aLink->mPool->mOp->mFreeBlock(aLink->mPool, aBlock) != ACI_SUCCESS);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerRecvClientIPC(cmnLinkPeer    *aLink,
                                cmbBlock      **aBlock,
                                cmpHeader      *aHeader,
                                acp_time_t      aTimeout)
{
    /*
     * semaphore�� mutex�� �ƴϴ�!!!!
     *
     * lock�� ȹ��, ��ȯ�ϴ� ������ �ƴ϶�
     * ����� �� �ִ� �ڿ��� ������ �Ǵ��Ѵ�.
     *
     * write : ������ ���� +1
     * read  : -1 �� �б�
     */

    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;

    acp_sint32_t       rc;
    cmbBlock          *sBlock = *aBlock; /* proj_2160 cm_type removal */
    acp_sint8_t       *sDataBlock;
    cmpHeader          sHeader;
    cmbShmChannelInfo *sChannelInfo;

    ACP_UNUSED(aTimeout);

    ACI_TEST(sDesc->mConnectFlag != ACP_TRUE || sDesc->mHandShakeFlag != ACP_TRUE);

    /*
     * check ghost-connection in shutdown state() : PR-4096
     */
    sChannelInfo = cmbShmGetChannelInfo(gIpcShmBuffer, sDesc->mChannelID);
    ACI_TEST_RAISE(sDesc->mTicketNum != sChannelInfo->mTicketNum, GhostConnection);

    while(1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpWaitSendToCli, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST_RAISE(EINTR != errno, SemOpError);
        }
    }

    /*
     * disconnect check
     */

    rc = semop(sDesc->mSemChannelID, sDesc->mOpCheckSvrExit, 2);
    ACI_TEST_RAISE(rc == 0, ConnectionClosed);


    /*
     * ======================================================
     * bug-27250 free Buf list can be crushed when client killed
     * ������: ���� �Ǵ� ������ block ����.
     * ������: ������ ������ ��� block ����.
     */

    sDataBlock = cmbShmGetDefaultClientBuffer(gIpcShmBuffer,
                                              gIpcShmInfo.mMaxChannelCount,
                                              sDesc->mChannelID);

    /*
     * ����� ���� �о� data size�� ���� ������ �ش� size ��ŭ
     * buffer�� ������ �д�. �ٸ� buffer�� �����ϴ� ������,
     * ���� marshal �ܰ迡�� ���簡 �ƴ� �����͸� ����ϱ� ����.
     */

    /*
     * read header
     */
    acpMemCpy(sBlock->mData, sDataBlock, CMP_HEADER_SIZE);
    sBlock->mDataSize    = CMP_HEADER_SIZE;
    /*
     * Protocol Header �ؼ�
     */
    ACI_TEST(cmpHeaderRead(aLink, &sHeader, sBlock) != ACI_SUCCESS);

    /*
     * read payload
     */
    acpMemCpy(sBlock->mData + CMP_HEADER_SIZE,
              sDataBlock + CMP_HEADER_SIZE,
              sHeader.mA7.mPayloadLength);
    sBlock->mDataSize = CMP_HEADER_SIZE + sHeader.mA7.mPayloadLength;

    /*
     * ======================================================
     * bug-27250 free Buf list can be crushed when client killed.
     * �������� data�� Ŀ�� ���� ��Ŷ�� ������ �����ϴ� ���,
     * ���� ��Ŷ�� ������ �غ� �Ǿ� ������ ������ �˷��־�
     * ����ȭ�� ��Ų��.(�ݵ�� block ���� ���Ŀ� Ǯ���־�� ��)
     * need to recv next packet.
     * seq end�� �ƴ϶�� ���� ������ ���� ��Ŷ�� �� ������ �ǹ�
     */
    if (CMP_HEADER_PROTO_END_IS_SET(&sHeader) == ACP_FALSE)
    {
        /*
         * ū �������� ��Ŷ �۽� ��ȣ ����� semaphore ���
         */
        while(1)
        {
            rc = semop(sDesc->mSemChannelID, sDesc->mOpSignSendMoreToCli, 1);
            if (rc == 0)
            {
                break;
            }
            else
            {
                ACI_TEST_RAISE(EINTR != errno, SemOpError);
            }
        }
    }

    *aHeader = sHeader;

    return ACI_SUCCESS;

    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    ACI_EXCEPTION(GhostConnection);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(ConnectionClosed);
    {
        (void)semop(sDesc->mSemChannelID, sDesc->mOpSignSvrExit, 1);
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerReqCompleteIPC(cmnLinkPeer * aLink)
{
    /*
     * write complete
     */

    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;

    sLink->mPrevWriteBuffer = NULL;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerResCompleteIPC(cmnLinkPeer * aLink)
{
    /*
     * read complete
     */

    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;

    sLink->mPrevReadBuffer = NULL;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerCheckClientIPC(cmnLinkPeer * aLink, acp_bool_t * aIsClosed)
{
    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;

    acp_sint32_t    rc;

    rc = semop(sDesc->mSemChannelID, sDesc->mOpCheckSvrExit, 2);
    ACI_TEST_RAISE(rc == 0, ConnectionClosed);

    *aIsClosed = ACP_FALSE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ConnectionClosed);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }

    ACI_EXCEPTION_END;

    *aIsClosed = ACP_TRUE;

    return ACI_FAILURE;
}

acp_bool_t cmnLinkPeerHasPendingRequestIPC(cmnLinkPeer * aLink)
{
    ACP_UNUSED(aLink);

    return ACP_FALSE;
}

ACI_RC cmnLinkPeerSendClientIPC(cmnLinkPeer *aLink, cmbBlock *aBlock)
{
    /*
     * semaphore�� mutex�� �ƴϴ�!!!!
     *
     * lock�� ȹ��, ��ȯ�ϴ� ������ �ƴ϶�
     * ����� �� �ִ� �ڿ��� ������ �Ǵ��Ѵ�.
     *
     * write : ������ ���� +1
     * read  : -1 �� �б�
     */

    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;

    acp_sint32_t       rc;
    cmbShmChannelInfo *sChannelInfo;

    cmpHeader          sHeader;
    acp_sint8_t       *sDataBlock;

    ACI_TEST((sDesc->mConnectFlag != ACP_TRUE) ||
             (sDesc->mHandShakeFlag != ACP_TRUE));

    /*
     * check ghost-connection in shutdown state() : PR-4096
     */
    sChannelInfo = cmbShmGetChannelInfo(gIpcShmBuffer, sDesc->mChannelID);
    ACI_TEST_RAISE(sDesc->mTicketNum != sChannelInfo->mTicketNum, GhostConnection);

    /*
     * =======================================================
     * bug-27250 free Buf list can be crushed when client killed
     * server�� ���� ��Ŷ�� ������ �غ� �Ǿ����� Ȯ��(try)
     * �غ� �ȵƴٸ� WirteBlockList�� �߰��Ǿ� ���߿� retry.
     */
    if (sLink->mPrevWriteBuffer != NULL)
    {
        /*
         * ū �������� ��Ŷ �۽� ��ȣ ����� semaphore ���
         */
        while(1)
        {
            rc = semop(sDesc->mSemChannelID,
                       sDesc->mOpCheckSendMoreToSvr, 1);
            if (rc == 0)
            {
                break;
            }
            else
            {
                // lock try�� �����ϸ� goto retry
                ACI_TEST_RAISE(EAGAIN == errno, Retry);
                ACI_TEST_RAISE(EINTR != errno, SemOpError);
            }
        }
    }

    /*
     * bug-27250 free Buf list can be crushed when client killed.
     * block �����͸� ���� �޸� ��� ���۷� �������� �۽��Ѵ�.
     * ��� ������ datasize�� �˱� ���� �ʿ��ϴ�.
     * �������� marshal �ܰ迡�� ���� �޸𸮸� ���� �����ϴ� ����
     * ���� ���� ���縦 �Ѵ�.
     */

    ACI_TEST(cmpHeaderRead(aLink, &sHeader, aBlock) != ACI_SUCCESS);
    aBlock->mCursor = 0;

    sDataBlock = cmbShmGetDefaultServerBuffer(gIpcShmBuffer,
            gIpcShmInfo.mMaxChannelCount,
            sDesc->mChannelID);
    acpMemCpy(sDataBlock, aBlock->mData, aBlock->mDataSize);
    aBlock->mCursor = aBlock->mDataSize;
    /*
     * ======================================================
     */

    while(1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpSignSendToSvr, 1);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST_RAISE(EINTR != errno, SemOpError);
        }
    }

    /*
     * =======================================================
     * bug-27250 free Buf list can be crushed when client killed
     * �������� data�� Ŀ�� ���� ��Ŷ�� ������ �۽��ϴ� ���,
     * �ϴ� mark�� �صд�.(���� ȣ��� ������ �ڵ带 Ÿ�� ��)
     * seq end�� �ƴ϶�� ���� �۽��� ���� ��Ŷ�� �� ������ �ǹ�
     */
    if (CMP_HEADER_PROTO_END_IS_SET(&sHeader) == ACP_FALSE)
    {
        sLink->mPrevWriteBuffer = sDataBlock;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(GhostConnection);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    /*
     * bug-27250 free Buf list can be crushed when client killed
     */
    ACI_EXCEPTION(Retry);
    {
        ACI_SET(aciSetErrorCode(cmERR_RETRY_SOCKET_OPERATION_WOULD_BLOCK));
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

struct cmnLinkOP gCmnLinkPeerClientOpIPC =
{
    "IPC-PEER-CLIENT",

    cmnLinkPeerInitializeClientIPC,
    cmnLinkPeerFinalizeIPC,

    cmnLinkPeerCloseClientIPC,

    cmnLinkPeerGetSockIPC,

    cmnLinkPeerGetDispatchInfoIPC,
    cmnLinkPeerSetDispatchInfoIPC
};

struct cmnLinkPeerOP gCmnLinkPeerPeerClientOpIPC =
{
    cmnLinkPeerSetBlockingModeIPC,

    cmnLinkPeerGetInfoIPC,
    cmnLinkPeerGetDescIPC,

    cmnLinkPeerConnectIPC,
    cmnLinkPeerSetOptionsIPC,

    cmnLinkPeerAllocChannelClientIPC,
    cmnLinkPeerHandshakeClientIPC,

    cmnLinkPeerShutdownClientIPC,

    cmnLinkPeerRecvClientIPC,
    cmnLinkPeerSendClientIPC,

    cmnLinkPeerReqCompleteIPC,
    cmnLinkPeerResCompleteIPC,

    cmnLinkPeerCheckClientIPC,
    cmnLinkPeerHasPendingRequestIPC,

    cmnLinkPeerAllocBlockClientIPC,
    cmnLinkPeerFreeBlockClientIPC,

    /* TASK-5894 Permit sysdba via IPC */
    NULL,

    /* PROJ-2474 SSL/TLS */
    NULL,
    NULL,
    NULL,

    /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
    NULL,
    NULL,
    NULL,
    NULL
};


ACI_RC cmnLinkPeerClientMapIPC(cmnLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Link �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);
    ACE_ASSERT(aLink->mImpl == CMN_LINK_IMPL_IPC);

    /*
     *  BUGBUG: IPC Pool ȹ��
     */
    ACI_TEST(cmbPoolGetSharedPool(&sLink->mPool, CMB_POOL_IMPL_IPC) != ACI_SUCCESS);

    /*
     * �Լ� ������ ����
     */
    aLink->mOp     = &gCmnLinkPeerClientOpIPC;
    sLink->mPeerOp = &gCmnLinkPeerPeerClientOpIPC;

    /*
     * ��� �ʱ�ȭ
     */
    sLink->mUserPtr    = NULL;

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

acp_uint32_t cmnLinkPeerClientSizeIPC()
{
    return ACI_SIZEOF(cmnLinkPeerIPC);
}

/*
 * bug-27250 free Buf list can be crushed when client killed.
 * cmiWriteBlock���� protocol end packet �۽Ž�
 * pending block�� �ִ� ��� �� �ڵ� ����
 */
ACI_RC cmnLinkPeerWaitSendClientIPC(cmnLink* aLink)
{

    cmnLinkPeerIPC *sLink = (cmnLinkPeerIPC *)aLink;
    cmnLinkDescIPC *sDesc = &sLink->mDesc;
    acp_sint32_t            rc;

    /*
     * receiver�� �۽� ��� ��ȣ�� �ٶ����� ���� ���
     */
    while(1)
    {
        rc = semop(sDesc->mSemChannelID,
                   sDesc->mOpWaitSendMoreToSvr, 2);
        if (rc == 0)
        {
            break;
        }
        else
        {
            ACI_TEST_RAISE(EIDRM == errno, SemRemoved);
            ACI_TEST_RAISE(EINTR != errno, SemOpError);
        }
    }

    /*
     * disconnect check
     */
    rc = semop(sDesc->mSemChannelID,
               sDesc->mOpCheckSvrExit, 2);
    ACI_TEST_RAISE(rc == 0, ConnectionClosed);

    return ACI_SUCCESS;

    ACI_EXCEPTION(SemRemoved);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(SemOpError);
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    ACI_EXCEPTION(ConnectionClosed);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

#endif
