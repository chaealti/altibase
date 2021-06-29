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

#include <cmAll.h>

#if !defined(CM_DISABLE_IPCDA)

typedef struct cmnLinkListenIPCDA
{
    cmnLinkListen  mLinkListen;
    PDL_SOCKET     mHandle;
    UInt           mDispatchInfo;
} cmnLinkListenIPCDA;

IDE_RC cmnLinkListenInitializeIPCDA(cmnLink *aLink)
{
    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /*
     * Handle �ʱ�ȭ
     */
    sLink->mHandle = PDL_INVALID_SOCKET;

    return IDE_SUCCESS;
}

IDE_RC cmnLinkListenFinalizeIPCDA(cmnLink *aLink)
{
    IDE_TEST(aLink->mOp->mClose(aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmnLinkListenCloseIPCDA(cmnLink *aLink)
{
    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /*
     * socket�� ���������� ����
     */
    if (sLink->mHandle != PDL_INVALID_SOCKET)
    {
        idlOS::closesocket(sLink->mHandle);
        sLink->mHandle = PDL_INVALID_SOCKET;
    }

    return IDE_SUCCESS;
}

IDE_RC cmnLinkListenGetHandleIPCDA(cmnLink *aLink, void *aHandle)
{
    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /*
     * socket�� ������
     */
    *(PDL_SOCKET *)aHandle = sLink->mHandle;


    return IDE_SUCCESS;
}

IDE_RC cmnLinkListenGetDispatchInfoIPCDA(cmnLink * aLink, void * aDispatchInfo)
{
    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /*
     * DispatcherInfo�� ������
     */
    *(UInt *)aDispatchInfo = sLink->mDispatchInfo;

    return IDE_SUCCESS;
}

IDE_RC cmnLinkListenSetDispatchInfoIPCDA(cmnLink * aLink, void * aDispatchInfo)
{
    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /*
     * DispatchInfo�� ����
     */
    sLink->mDispatchInfo = *(UInt *)aDispatchInfo;

    return IDE_SUCCESS;
}

IDE_RC cmnLinkListenListenIPCDA(cmnLinkListen *aLink, cmnLinkListenArg *aListenArg)
{
    UInt                  sChannelCount;
    SInt                  sOption;
    UInt                  sPathLen;
    struct sockaddr_un    sAddr;

    cmnLinkListenIPCDA *sLink = (cmnLinkListenIPCDA *)aLink;

    /* --------------------------------------------------
     * UNIX ���� �ʱ�ȭ
     * ------------------------------------------------*/

    /*
     * socket�� �̹� �����ִ��� �˻�
     */
    IDE_TEST_RAISE(sLink->mHandle != PDL_INVALID_SOCKET, SocketAlreadyOpened);

    /*
     * IPC ���� �̸� �˻�
     */
    sPathLen = idlOS::strlen(aListenArg->mIPCDA.mFilePath);

    IDE_TEST_RAISE(ID_SIZEOF(sAddr.sun_path) <= sPathLen, IpcPathTruncated);

    /*
     * socket ����
     */
    sLink->mHandle = idlOS::socket(AF_UNIX, SOCK_STREAM, 0);

    IDE_TEST_RAISE(sLink->mHandle == PDL_INVALID_SOCKET, SocketError);

    /*
     * SO_REUSEADDR ����
     */
    sOption = 1;

    if (idlOS::setsockopt(sLink->mHandle,
                          SOL_SOCKET,
                          SO_REUSEADDR,
                          (SChar *)&sOption,
                          ID_SIZEOF(sOption)) < 0)
    {
        ideLog::log(IDE_SERVER_0, CM_TRC_SOCKET_REUSEADDR_FAIL, errno);
    }

    /*
     * IPC ���� �̸� ����
     */
    idlOS::snprintf(sAddr.sun_path,
                    ID_SIZEOF(sAddr.sun_path),
                    "%s",
                    aListenArg->mIPCDA.mFilePath);

    /*
     * IPC ���� ����
     */
    idlOS::unlink(sAddr.sun_path);

    /*
     * bind
     */
    sAddr.sun_family = AF_UNIX;

    IDE_TEST_RAISE(idlOS::bind(sLink->mHandle,
                               (struct sockaddr *)&sAddr,
                               ID_SIZEOF(sAddr)) < 0,
                   BindError);

    /*
     * listen
     */
    sChannelCount = aListenArg->mIPCDA.mMaxListen;
    IDE_TEST_RAISE(idlOS::listen(sLink->mHandle, sChannelCount) < 0, ListenError);

    /*
     * Shared Memory ����
     */

    IDE_TEST(cmbShmIPCDACreate(sChannelCount) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(IpcPathTruncated);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_UNIX_PATH_TRUNCATED));
    }
    IDE_EXCEPTION(SocketAlreadyOpened);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SOCKET_ALREADY_OPENED));
    }
    IDE_EXCEPTION(SocketError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SOCKET_OPEN_ERROR, errno));
    }
    IDE_EXCEPTION(BindError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_BIND_ERROR, errno));
    }
    IDE_EXCEPTION(ListenError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_LISTEN_ERROR));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnLinkListenAcceptIPCDA(cmnLinkListen *aLink, cmnLinkPeer **aLinkPeer)
{
    cmnLinkListenIPCDA   *sLink     = (cmnLinkListenIPCDA *)aLink;
    cmnLinkPeer        *sLinkPeer = NULL;
    cmnLinkDescIPCDA     *sDesc;
    SInt                sAddrLen;
    cmnLinkDescTCP     sTmpDesc;

    /*
     * ���ο� Link�� �Ҵ�
     */
    /* BUG-29957
     * cmnLinkAlloc ���н� Connect�� ��û�� Socket�� �ӽ÷� accept ����� �Ѵ�.
     */
    IDE_TEST_RAISE(cmnLinkAlloc((cmnLink **)&sLinkPeer,
                                CMN_LINK_TYPE_PEER_SERVER,
                                CMN_LINK_IMPL_IPCDA) != IDE_SUCCESS, LinkError);

    /*
     * Desc ȹ��
     */
    IDE_TEST_RAISE(sLinkPeer->mPeerOp->mGetDesc(sLinkPeer, &sDesc) != IDE_SUCCESS, LinkError);

    /* TASK-3873 5.3.3 Release Static Analysis Code-sonar */
    /* Code-Sonar�� Function  Pointer�� follow���� ���ؼ�..
       assert�� �־����ϴ� */
    IDE_ASSERT( sDesc != NULL);

    /*
     * accept
     */
    sAddrLen = ID_SIZEOF(sDesc->mAddr);

    sDesc->mHandle = idlOS::accept(sLink->mHandle,
                                   (struct sockaddr *)&(sDesc->mAddr),
                                   &sAddrLen);

    IDE_TEST_RAISE(sDesc->mHandle == PDL_INVALID_SOCKET, AcceptError);

    /*
     * Link�� ������
     */
    *aLinkPeer = sLinkPeer;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(LinkError);
    {
      /* BUG-29957 */
      sAddrLen = ID_SIZEOF(sTmpDesc.mAddr);

      sTmpDesc.mHandle = idlOS::accept(sLink->mHandle,
                                       (struct sockaddr *)&(sTmpDesc.mAddr),
                                       &sAddrLen);

      if (sTmpDesc.mHandle != PDL_INVALID_SOCKET)
      {
        idlOS::closesocket(sTmpDesc.mHandle);
      }
    }
    IDE_EXCEPTION(AcceptError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_ACCEPT_ERROR));
    }
    IDE_EXCEPTION_END;
    {
        if (sLinkPeer != NULL)
        {
            IDE_ASSERT(cmnLinkFree((cmnLink *)sLinkPeer) == IDE_SUCCESS);
        }

        *aLinkPeer = NULL;
    }

    return IDE_FAILURE;
}


struct cmnLinkOP gCmnLinkListenOpIPCDA =
{
    "IPCDA-LISTEN",

    cmnLinkListenInitializeIPCDA,
    cmnLinkListenFinalizeIPCDA,

    cmnLinkListenCloseIPCDA,

    cmnLinkListenGetHandleIPCDA,

    cmnLinkListenGetDispatchInfoIPCDA,
    cmnLinkListenSetDispatchInfoIPCDA
};

struct cmnLinkListenOP gCmnLinkListenListenOpIPCDA =
{
    cmnLinkListenListenIPCDA,
    cmnLinkListenAcceptIPCDA
};


IDE_RC cmnLinkListenMapIPCDA(cmnLink *aLink)
{
    cmnLinkListen *sLink = (cmnLinkListen *)aLink;

    /*
     * Link �˻�
     */

    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_LISTEN);
    IDE_ASSERT(aLink->mImpl == CMN_LINK_IMPL_IPCDA);

    /*
     * �Լ� ������ ����
     */

    aLink->mOp       = &gCmnLinkListenOpIPCDA;
    sLink->mListenOp = &gCmnLinkListenListenOpIPCDA;

    return IDE_SUCCESS;
}

UInt cmnLinkListenSizeIPCDA()
{
    return ID_SIZEOF(cmnLinkListenIPCDA);
}


#endif
