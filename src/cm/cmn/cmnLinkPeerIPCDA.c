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
#include <stdlib.h>

#if !defined(CM_DISABLE_IPCDA)

/*
 * IPCDA Channel Information
 */
extern cmbShmIPCDAInfo    gIPCDAShmInfo;

extern acp_thr_mutex_t    gIPCDAMutex;

extern acp_uint32_t       gIPCDASimpleQueryDataBlockSize;

/* IPCDA Handshake Message */
#define IPCDA_HELLO_MSG          "IPCDA_HELLO"
#define IPCDA_READY_MSG          "IPCDA_READY"
#define IPCDA_MSG_PREFIX_CNT     (4)
#define IPCDA_MSG_CNT            (11)
#define IPCDA_PID_PREFIX_CNT     (5)
#define IPCDA_PID_CNT            (5)
#define IPCDA_PROTO_CNT          IPCDA_MSG_PREFIX_CNT + IPCDA_MSG_CNT + IPCDA_PID_PREFIX_CNT + IPCDA_PID_CNT

acp_uint8_t * cmnLinkPeerGetReadBlock(acp_uint32_t aChannelID)
{
    return cmbShmIPCDAGetDefaultServerBuffer(gIPCDAShmInfo.mShmBuffer,
                                             gIPCDAShmInfo.mMaxChannelCount,
                                             aChannelID);
}

acp_uint8_t * cmnLinkPeerGetWriteBlock(acp_uint32_t aChannelID)
{
    return cmbShmIPCDAGetDefaultClientBuffer(gIPCDAShmInfo.mShmBuffer,
                                             gIPCDAShmInfo.mMaxChannelCount,
                                             aChannelID);
}

ACI_RC cmnLinkPeerInitializeClientIPCDA(cmnLink *aLink)
{
    cmnLinkPeerIPCDA *sLink = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA *sDesc = &sLink->mDesc;

    /* ��� �ʱ�ȭ*/
    sDesc->mConnectFlag   = ACP_FALSE;
    sDesc->mHandShakeFlag = ACP_FALSE;

    sDesc->mSock.mHandle  = CMN_INVALID_SOCKET_HANDLE;
    sDesc->mChannelID     = -1;

    sDesc->mSemChannelKey = -1;
    sDesc->mSemChannelID  = -1;

    acpMemSet(&sDesc->mSemInfo, 0, ACI_SIZEOF(struct semid_ds));

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerFinalizeIPCDA(cmnLink *aLink)
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
ACI_RC cmnLinkPeerCloseClientIPCDA(cmnLink *aLink)
{
    cmnLinkPeerIPCDA       *sLink        = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA       *sDesc        = &sLink->mDesc;
    cmbShmIPCDAChannelInfo *sChannelInfo = NULL;
    acp_sint32_t            sRc          = 0;;

    if(sDesc->mConnectFlag == ACP_TRUE)
    {
        if (sDesc->mHandShakeFlag == ACP_TRUE)
        {
            sChannelInfo = cmbShmIPCDAGetChannelInfo( gIPCDAShmInfo.mShmBuffer,
                                                      sDesc->mChannelID );

            /* bug-29324 channel not closed when a client alive after disconn
             * before: timestamp�� ���Ͽ� ���� ��츸 ����ó���� ������.
             * after :
             * 1. ����ó���� �� �ʿ��� �����̹Ƿ� timestamp �񱳸� ������.
             * 2. �����ȣ �۽� ����(mOpSignCliExit)�� ���� �������� �ǵ��� ����
             */

            /* BUG-32398 Ÿ�ӽ�����(Ƽ�Ϲ�ȣ) �񱳺κ� ���� */
            if (sDesc->mTicketNum == sChannelInfo->mTicketNum)
            {
                while(1)
                {
                    sRc = semop(sDesc->mSemChannelID, sDesc->mOpSignCliExit, 1);

                    if (sRc != 0)
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

ACI_RC cmnLinkPeerGetSockIPCDA(cmnLink *aLink, void **aHandle)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aHandle);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerGetDispatchInfoIPCDA(cmnLink * aLink, void * aDispatchInfo)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aDispatchInfo);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSetDispatchInfoIPCDA(cmnLink * aLink, void * aDispatchInfo)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aDispatchInfo);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSetBlockingModeIPCDA(cmnLinkPeer *aLink, acp_bool_t aBlockingMode)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlockingMode);

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerGetInfoIPCDA(cmnLinkPeer * aLink, acp_char_t *aBuf, acp_uint32_t aBufLen, cmiLinkInfoKey aKey)
{
    acp_sint32_t sRet;

    ACP_UNUSED(aLink);

    switch (aKey)
    {
        case CMN_LINK_INFO_ALL:
        case CMN_LINK_INFO_IMPL:
        case CMN_LINK_INFO_IPCDA_KEY:
            sRet = acpSnprintf(aBuf, aBufLen, "IPCDA");

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

ACI_RC cmnLinkPeerGetDescIPCDA(cmnLinkPeer *aLink, void *aDesc)
{
    cmnLinkPeerIPCDA *sLink = (cmnLinkPeerIPCDA *)aLink;

    /* Desc�� ������ */
    *(cmnLinkDescIPCDA **)aDesc = &sLink->mDesc;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerConnectIPCDA(cmnLinkPeer       *aLink,
                             cmnLinkConnectArg *aConnectArg,
                             acp_time_t         aTimeout,
                             acp_sint32_t       aOption)
{
    cmnLinkPeerIPCDA    *sLink = (cmnLinkPeerIPCDA *)aLink;
    acp_rc_t           sRet;

    ACP_UNUSED(aOption);

    /*
     * socket ����
     */
    sRet = acpSockOpen(&sLink->mDesc.mSock,
                       ACP_AF_UNIX,
                       ACP_SOCK_STREAM,
                       0);
    ACI_TEST_RAISE(sRet != ACP_RC_SUCCESS, SocketError);

    /*
     * IPC �ּ� ����
     */

    sLink->mDesc.mAddr.sun_family = AF_UNIX;
    sRet = acpSnprintf(sLink->mDesc.mAddr.sun_path,
                       ACI_SIZEOF(sLink->mDesc.mAddr.sun_path),
                       "%s",
                       aConnectArg->mIPCDA.mFilePath);

    /*
     *  IPC �����̸� ���� �˻�
     */

    ACI_TEST_RAISE(ACP_RC_IS_ETRUNC(sRet), IPCDAPathTruncated);
    ACI_TEST(ACP_RC_NOT_SUCCESS(sRet));

    /*
     * connect
     */

    sRet = acpSockTimedConnect(&sLink->mDesc.mSock,
                                       (acp_sock_addr_t *)(&sLink->mDesc.mAddr),
                                       ACI_SIZEOF(sLink->mDesc.mAddr),
                                       aTimeout);
    ACI_TEST_RAISE(sRet != ACP_RC_SUCCESS, ConnectError);

    sLink->mDesc.mConnectFlag = ACP_TRUE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(SocketError);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SOCKET_OPEN_ERROR, sRet));
    }
    ACI_EXCEPTION(IPCDAPathTruncated);
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

ACI_RC cmnLinkPeerAllocChannelClientIPCDA(cmnLinkPeer *aLink, acp_sint32_t *aChannelID)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aChannelID);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerSetOperationIPCDA(cmnLinkDescIPCDA *aDesc)
{
    /*
     * =====================================================
     * bug-28340 rename semop name for readability
     * �������� ���� semaphore op �������� ������ ���� ����
     * =====================================================
     * IPCDA_SEM_SERVER_DETECT  > IPCDA_SEM_CHECK_SVR_EXIT (0)
     * server_detect_init     > InitSvrExit  : ������ ��� ����
     * server_detect_try      > CheckSvrExit : ������ �׾����� Ȯ��
     * server_detect_release  > SignSvrExit  : ������ �����ȣ ����
     * =====================================================
     * IPCDA_SEM_CLIENT_DETECT  > IPCDA_SEM_CHECK_CLI_EXIT (1)
     * client_detect_init     > InitCliExit  : cli�� ��� ����
     * client_detect_try      > CheckCliExit : cli�� �׾����� Ȯ��
     * client_detect_hold     > WaitCliExit  : cli ���ᶧ���� ���
     * client_detect_release  > SignCliExit  : cli�� �����ȣ ����
     * =====================================================
     */

    /*
     * =====================================================
     * IPCDA_SEM_CHECK_SVR_EXIT (0)
     */
    aDesc->mOpInitSvrExit[0].sem_num = IPCDA_SEM_CHECK_SVR_EXIT;
    aDesc->mOpInitSvrExit[0].sem_op  = -1;
    aDesc->mOpInitSvrExit[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckSvrExit[0].sem_num = IPCDA_SEM_CHECK_SVR_EXIT;
    aDesc->mOpCheckSvrExit[0].sem_op  = -1;
    aDesc->mOpCheckSvrExit[0].sem_flg = IPC_NOWAIT;
    aDesc->mOpCheckSvrExit[1].sem_num = IPCDA_SEM_CHECK_SVR_EXIT;
    aDesc->mOpCheckSvrExit[1].sem_op  = 1;
    aDesc->mOpCheckSvrExit[1].sem_flg = 0;

    aDesc->mOpSignSvrExit[0].sem_num = IPCDA_SEM_CHECK_SVR_EXIT;
    aDesc->mOpSignSvrExit[0].sem_op  = 1;
    aDesc->mOpSignSvrExit[0].sem_flg = 0;

    /*
     * =====================================================
     * IPCDA_SEM_CHECK_CLI_EXIT (1)
     */
    aDesc->mOpInitCliExit[0].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpInitCliExit[0].sem_op  = -1;
    aDesc->mOpInitCliExit[0].sem_flg = SEM_UNDO;

    aDesc->mOpCheckCliExit[0].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpCheckCliExit[0].sem_op  = -1;
    aDesc->mOpCheckCliExit[0].sem_flg = IPC_NOWAIT;
    aDesc->mOpCheckCliExit[1].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpCheckCliExit[1].sem_op  =  1;
    aDesc->mOpCheckCliExit[1].sem_flg = 0;

    aDesc->mOpWaitCliExit[0].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpWaitCliExit[0].sem_op  = -1;
    aDesc->mOpWaitCliExit[0].sem_flg = 0;
    aDesc->mOpWaitCliExit[1].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpWaitCliExit[1].sem_op  =  1;
    aDesc->mOpWaitCliExit[1].sem_flg = 0;

    aDesc->mOpSignCliExit[0].sem_num = IPCDA_SEM_CHECK_CLI_EXIT;
    aDesc->mOpSignCliExit[0].sem_op  = 1;
    aDesc->mOpSignCliExit[0].sem_flg = 0;

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerHandshakeClientIPCDA(cmnLinkPeer *aLink)
{
    cmnLinkPeerIPCDA *sLink = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA *sDesc = &sLink->mDesc;

    acp_sint32_t       rc;
    acp_size_t         sCount;
    acp_sint32_t       sFlag = 0666;
    acp_sint32_t       sSemCount = 0;
    acp_char_t         sProtoBuffer[32];
    acp_bool_t         sLocked = ACP_FALSE;
    acp_uint32_t       sPID = 0;
    acp_size_t         sDataLength;

    cmnLinkDescIPCDAMsg     sIpcMsg;
    cmbShmIPCDAChannelInfo *sChannelInfo = NULL;

    // BUG-32625 32bit Client IPC Connection Error
    union semun {
        struct semid_ds *buf;
    } sArg;

    /* send hello message */
    sPID = acpProcGetSelfID();

    acpSnprintf(sProtoBuffer, 32, "MSG=%s;PID=%d;", IPCDA_HELLO_MSG, sPID);
    sDataLength = acpCStrLen(sProtoBuffer, strlen(sProtoBuffer));

    ACI_TEST(acpSockSend(&sDesc->mSock,
                         sProtoBuffer,
                         sDataLength,//IPC_PROTO_CNT,
                         &sCount,
                         0) != ACP_RC_SUCCESS);
    ACI_TEST( sCount != sDataLength);

    /*
     * receive ipc protocol message
     */

    ACI_TEST(acpSockRecv(&sDesc->mSock,
                         &sIpcMsg,
                         ACI_SIZEOF(cmnLinkDescIPCDAMsg),
                         &sCount,
                         0) != ACP_RC_SUCCESS);

    ACI_TEST(sCount != ACI_SIZEOF(cmnLinkDescIPCDAMsg));

    ACI_TEST_RAISE(sIpcMsg.mChannelID < 0, NoChannelError);

    /*
     * CAUTION: don't move
     * if do, it causes network deadlock in MT_threaded Clinets
     */
    ACI_TEST(acpThrMutexLock(&gIPCDAMutex) != ACP_RC_SUCCESS);

    sLocked = ACP_TRUE;

    sDesc->mChannelID = sIpcMsg.mChannelID;

    if (gIPCDAShmInfo.mMaxChannelCount != sIpcMsg.mMaxChannelCount)
    {
        gIPCDAShmInfo.mMaxChannelCount = sIpcMsg.mMaxChannelCount;
    }

    if (gIPCDAShmInfo.mMaxBufferCount != sIpcMsg.mMaxBufferCount)
    {
        gIPCDAShmInfo.mMaxBufferCount = sIpcMsg.mMaxBufferCount;
    }

    if (gIPCDAShmInfo.mShmKey != sIpcMsg.mShmKey)
    {
        gIPCDAShmInfo.mShmKey         = sIpcMsg.mShmKey;
        sLink->mDesc.mShmKey = sIpcMsg.mShmKey;

        gIPCDAShmInfo.mMaxChannelCount  = sIpcMsg.mMaxChannelCount;
        gIPCDAShmInfo.mUsedChannelCount = 0;

        gIPCDAShmInfo.mShmID = -1;
        cmbShmIPCDADetach();
    }

    if (gIPCDAShmInfo.mShmKeyForSimpleQuery != sIpcMsg.mShmKeyForSimpleQuery)
    {
        gIPCDAShmInfo.mShmKeyForSimpleQuery = sIpcMsg.mShmKeyForSimpleQuery;
        gIPCDAShmInfo.mShmIDForSimpleQuery  = -1;
        cmbShmIPCDADetach();
    }

    gIPCDASimpleQueryDataBlockSize = sIpcMsg.mDataBlockSize;

    /*
     * attach shared memory
     */
    ACI_TEST(cmbShmIPCDAAttach() != ACI_SUCCESS);

    /*
     * attach channel semaphores
     */

    if (sDesc->mSemChannelKey != sIpcMsg.mSemChannelKey)
    {
        sDesc->mSemChannelKey = sIpcMsg.mSemChannelKey;
        sSemCount             = IPCDA_SEM_CNT_PER_CHANNEL;
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

    cmnLinkPeerSetOperationIPCDA(sDesc);


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
     * send ready msg
     */
    acpMemCpy(sProtoBuffer, IPCDA_READY_MSG, IPCDA_MSG_CNT);

    ACI_TEST(acpSockSend(&sDesc->mSock,
                         sProtoBuffer,
                         IPCDA_MSG_CNT,
                         &sCount,
                         0) != ACP_RC_SUCCESS);

    ACI_TEST( sCount != IPCDA_MSG_CNT);

    gIPCDAShmInfo.mClientCount++;

    sChannelInfo = cmbShmIPCDAGetChannelInfo(gIPCDAShmInfo.mShmBuffer, sDesc->mChannelID);
    sDesc->mTicketNum = sChannelInfo->mTicketNum;

    /*
     * BUG-25420 [CodeSonar] Lock, Unlock ���� �ڵ鸵 ������ ���� Double Unlock
     * unlock �� �ϱ����� �������ؾ� Double Unlock �� ������ �ִ�.
     */
    sLocked = ACP_FALSE;
    rc = acpThrMutexUnlock(&gIPCDAMutex);
    if (ACP_RC_NOT_SUCCESS(rc))
    {
        gIPCDAShmInfo.mClientCount--;
        ACI_RAISE(ACI_EXCEPTION_END_LABEL);
    }

    sDesc->mHandShakeFlag = ACP_TRUE;

    /*
     * fix BUG-18833
     */
    if (sDesc->mSock.mHandle != CMN_INVALID_SOCKET_HANDLE)
    {
        ACI_TEST(acpSockClose(&sLink->mDesc.mSock) != ACP_RC_SUCCESS);
        sDesc->mSock.mHandle = CMN_INVALID_SOCKET_HANDLE;
    }

#if defined(ALTI_CFG_OS_LINUX)
    /* PROJ-2616 message queue */
    sLink->mMessageQ.mUseMessageQ = sIpcMsg.mUseMessageQ;
    if( sLink->mMessageQ.mUseMessageQ == ACP_TRUE )
    {
        
        sLink->mMessageQ.mAttr.mq_maxmsg  = CMI_IPCDA_MESSAGEQ_MAX_MESSAGE;
        sLink->mMessageQ.mAttr.mq_msgsize = CMI_IPCDA_MESSAGEQ_MESSAGE_SIZE;

        acpMemSet(sLink->mMessageQ.mName, 0, CMN_IPCDA_MESSAGEQ_NAME_SIZE);
        acpSnprintf(sLink->mMessageQ.mName, ACI_SIZEOF(sLink->mMessageQ.mName),
                    "/temp.%d", sDesc->mChannelID+1);

        sLink->mMessageQ.mMessageQID = mq_open( sLink->mMessageQ.mName, O_RDONLY | O_CREAT, 0666, &(sLink->mMessageQ.mAttr) );
        ACI_TEST( sLink->mMessageQ.mMessageQID == -1 );
    }
#endif

    return ACI_SUCCESS;

    ACI_EXCEPTION(NoChannelError);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CMN_ERR_FULL_IPC_CHANNEL))
    }
    ACI_EXCEPTION_END;

    if (sLocked == ACP_TRUE)
    {
        ACE_ASSERT(acpThrMutexUnlock(&gIPCDAMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

ACI_RC cmnLinkPeerSetOptionsIPCDA(cmnLinkPeer *aLink, acp_sint32_t aOption)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aOption);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerShutdownClientIPCDA(cmnLinkPeer    *aLink,
                                      cmnDirection    aDirection,
                                      cmnShutdownMode aMode)
{
    cmnLinkPeerIPCDA       *sLink        = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA       *sDesc        = &sLink->mDesc;
    cmbShmIPCDAChannelInfo *sChannelInfo = NULL;
    acp_sint32_t            sRc          = 0;

    ACP_UNUSED(aDirection);
    ACP_UNUSED(aMode);

    if (sDesc->mHandShakeFlag == ACP_TRUE)
    {
        /* clinet�� close flag�� on : ��, detect sem = 1 */
        sChannelInfo = cmbShmIPCDAGetChannelInfo( gIPCDAShmInfo.mShmBuffer, sDesc->mChannelID );

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
                sRc = semop(sDesc->mSemChannelID,
                            sDesc->mOpSignCliExit,
                            1);

                if (sRc != 0)
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

ACI_RC cmnLinkPeerAllocBlockClientIPCDA(cmnLinkPeer *aLink, cmbBlock **aBlock)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlock);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerFreeBlockClientIPCDA(cmnLinkPeer *aLink, cmbBlock *aBlock)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlock);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerRecvClientIPCDA(cmnLinkPeer    *aLink,
                                  cmbBlock      **aBlock,
                                  cmpHeader      *aHeader,
                                  acp_time_t      aTimeout)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlock);
    ACP_UNUSED(aHeader);
    ACP_UNUSED(aTimeout);

    /* not used */

    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerReqCompleteIPCDA(cmnLinkPeer * aLink)
{
    ACP_UNUSED(aLink);
    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerResCompleteIPCDA(cmnLinkPeer * aLink)
{
    ACP_UNUSED(aLink);
    return ACI_SUCCESS;
}

ACI_RC cmnLinkPeerCheckClientIPCDA(cmnLinkPeer * aLink, acp_bool_t * aIsClosed)
{
    cmnLinkPeerIPCDA *sLink = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA *sDesc = &sLink->mDesc;

    acp_sint32_t rc;

    /* BUG-45976 */
    while (1)
    {
        rc = semop(sDesc->mSemChannelID, sDesc->mOpCheckSvrExit, 2);
        if (rc == 0)
        {
            ACI_RAISE(ConnectionClosed);
        }
        else if ((rc == -1) && (errno == EAGAIN))
        {
            break;
        }
        else
        {
            ACI_TEST_RAISE(errno == EIDRM, ConnectionClosed);
            ACI_TEST_RAISE(errno != EINTR, SemOpError);
        }
    }

    *aIsClosed = ACP_FALSE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ConnectionClosed)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(SemOpError)
    {
        ACI_SET(aciSetErrorCode(cmERR_FATAL_CMN_SEM_OP));
    }
    ACI_EXCEPTION_END;

    *aIsClosed = ACP_TRUE;

    return ACI_FAILURE;
}

acp_bool_t cmnLinkPeerHasPendingRequestIPCDA(cmnLinkPeer * aLink)
{
    ACP_UNUSED(aLink);

    return ACP_FALSE;
}

ACI_RC cmnLinkPeerSendClientIPCDA(cmnLinkPeer *aLink, cmbBlock *aBlock)
{
    ACP_UNUSED(aLink);
    ACP_UNUSED(aBlock);

    /* This will be not used in ipc_da. */

    return ACI_SUCCESS;
}

struct cmnLinkOP gCltCmnLinkPeerClientOpIPCDA =
{
    "IPCDA-PEER-CLIENT",

    cmnLinkPeerInitializeClientIPCDA,
    cmnLinkPeerFinalizeIPCDA,
    
    cmnLinkPeerCloseClientIPCDA,
    
    cmnLinkPeerGetSockIPCDA,
    
    cmnLinkPeerGetDispatchInfoIPCDA,
    cmnLinkPeerSetDispatchInfoIPCDA
};

struct cmnLinkPeerOP gCltCmnLinkPeerPeerClientOpIPCDA =
{
    cmnLinkPeerSetBlockingModeIPCDA,

    cmnLinkPeerGetInfoIPCDA,
    cmnLinkPeerGetDescIPCDA,
    
    cmnLinkPeerConnectIPCDA,
    cmnLinkPeerSetOptionsIPCDA,
    
    cmnLinkPeerAllocChannelClientIPCDA,
    cmnLinkPeerHandshakeClientIPCDA,
    
    cmnLinkPeerShutdownClientIPCDA,

    cmnLinkPeerRecvClientIPCDA,
    cmnLinkPeerSendClientIPCDA,

    cmnLinkPeerReqCompleteIPCDA,
    cmnLinkPeerResCompleteIPCDA,

    cmnLinkPeerCheckClientIPCDA,
    cmnLinkPeerHasPendingRequestIPCDA,

    cmnLinkPeerAllocBlockClientIPCDA,
    cmnLinkPeerFreeBlockClientIPCDA,

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


ACI_RC cmnLinkPeerClientMapIPCDA(cmnLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /* Link �˻� */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);
    ACE_ASSERT(aLink->mImpl == CMN_LINK_IMPL_IPCDA);

    /* �Լ� ������ ���� */
    aLink->mOp     = &gCltCmnLinkPeerClientOpIPCDA;
    sLink->mPeerOp = &gCltCmnLinkPeerPeerClientOpIPCDA;

    /* ��� �ʱ�ȭ */
    sLink->mUserPtr    = NULL;

    return ACI_SUCCESS;
}

acp_uint32_t cmnLinkPeerClientSizeIPCDA()
{
    return ACI_SIZEOF(cmnLinkPeerIPCDA);
}


/*
 * bug-27250 free Buf list can be crushed when client killed.
 * cmiWriteBlock���� protocol end packet �۽Ž�
 * pending block�� �ִ� ��� �� �ڵ� ����
 */
ACI_RC cmnLinkPeerWaitSendClientIPCDA(cmnLink* aLink)
{

    cmnLinkPeerIPCDA *sLink = (cmnLinkPeerIPCDA *)aLink;
    cmnLinkDescIPCDA *sDesc = &sLink->mDesc;
    acp_sint32_t      rc    = 0;

    /*
     * disconnect check
     */
    rc = semop(sDesc->mSemChannelID,
               sDesc->mOpCheckSvrExit, 2);
    ACI_TEST_RAISE(rc == 0, ConnectionClosed);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ConnectionClosed);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
#endif
