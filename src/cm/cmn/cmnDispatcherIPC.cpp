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

#if !defined(CM_DISABLE_IPC)


typedef struct cmnDispatcherIPC
{
    cmnDispatcher  mDispatcher;

    PDL_SOCKET     mMaxHandle;

    fd_set         mFdSet;
} cmnDispatcherIPC;


IDE_RC cmnDispatcherInitializeIPC(cmnDispatcher *aDispatcher, UInt /*aMaxLink*/)
{
    cmnDispatcherIPC *sDispatcher = (cmnDispatcherIPC *)aDispatcher;

    /*
     * ��� �ʱ�ȭ
     */
    sDispatcher->mMaxHandle  = PDL_INVALID_SOCKET;

    /*
     * fdset �ʱ�ȭ
     */
    FD_ZERO(&sDispatcher->mFdSet);

    return IDE_SUCCESS;
}

IDE_RC cmnDispatcherFinalizeIPC(cmnDispatcher * /*aDispatcher*/)
{
    return IDE_SUCCESS;
}

IDE_RC cmnDispatcherAddLinkIPC(cmnDispatcher *aDispatcher, cmnLink *aLink)
{
    cmnDispatcherIPC  *sDispatcher = (cmnDispatcherIPC *)aDispatcher;
    PDL_SOCKET         sHandle;

    /*
     * Dispatcher���� ����� �� �ִ� Link Impl���� �˻� (PROJ-2681)
     */
    IDE_TEST_RAISE(cmiDispatcherImplForLink(aLink) != sDispatcher->mDispatcher.mImpl, InvalidLinkImpl);

    /*
     * Dispatcher�� Link List�� �߰�
     */
    IDE_TEST(cmnDispatcherAddLink(aDispatcher, aLink) != IDE_SUCCESS);

    /*
     * Link�� socket ȹ��
     */
    IDE_TEST(aLink->mOp->mGetHandle(aLink, &sHandle) != IDE_SUCCESS);

    /*
     * MaxHandle ����
     */
    if ((sDispatcher->mMaxHandle == PDL_INVALID_SOCKET) ||
        (sDispatcher->mMaxHandle < sHandle))
    {
        sDispatcher->mMaxHandle = sHandle;
    }

    /*
     * FdSet�� socket ����
     */
    FD_SET(sHandle, &sDispatcher->mFdSet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidLinkImpl)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_LINK_IMPL));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnDispatcherRemoveLinkIPC(cmnDispatcher */*aDispatcher*/, cmnLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer*)aLink;

    // bug-28277 ipc: server stop failed when idle clis exist
    // server stop�ÿ��� shutdown_mode_force �ѱ⵵�� ��.
    IDE_TEST(sLink->mPeerOp->mShutdown(sLink, CMN_DIRECTION_RDWR,
                                       CMN_SHUTDOWN_MODE_NORMAL)
             != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmnDispatcherRemoveAllLinksIPC(cmnDispatcher * aDispatcher)
{
    cmnDispatcherIPC *sDispatcher = (cmnDispatcherIPC *)aDispatcher;

    IDE_TEST(cmnDispatcherRemoveAllLinks(aDispatcher) != IDE_SUCCESS);

    sDispatcher->mMaxHandle  = PDL_INVALID_SOCKET;

    FD_ZERO(&sDispatcher->mFdSet);


    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmnDispatcherSelectIPC(cmnDispatcher  *aDispatcher,
                              iduList        *aReadyList,
                              UInt           *aReadyCount,
                              PDL_Time_Value *aTimeout)
{
    cmnDispatcherIPC  *sDispatcher = (cmnDispatcherIPC *)aDispatcher;
    iduListNode       *sIterator;
    cmnLink           *sLink;
    PDL_SOCKET         sHandle;
    SInt               sResult;

    IDU_LIST_INIT(aReadyList);

    sResult = idlOS::select(sDispatcher->mMaxHandle + 1,
                            &sDispatcher->mFdSet,
                            NULL,
                            NULL,
                            aTimeout);
    IDE_TEST_RAISE(sResult < 0, SelectError);

    /*
     * Ready Count ����
     */
    if (aReadyCount != NULL)
    {
        *aReadyCount = sResult;
    }

    /*
     * Ready Link �˻�
     */
    IDU_LIST_ITERATE(&aDispatcher->mLinkList, sIterator)
    {
        sLink = (cmnLink *)sIterator->mObj;

        /*
         * Link�� socket�� ȹ��
         */
        IDE_TEST(sLink->mOp->mGetHandle(sLink, &sHandle) != IDE_SUCCESS);

        /*
         * ready �˻�
         */
        if (FD_ISSET(sHandle, &sDispatcher->mFdSet))
        {
            IDU_LIST_ADD_LAST(aReadyList, &sLink->mReadyListNode);
        }
        else
        {
            FD_SET(sHandle, &sDispatcher->mFdSet);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(SelectError);
    {
        /* BUG-47714 ���� �޼����� sock number �߰� */
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SELECT_ERROR, sDispatcher->mMaxHandle));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

struct cmnDispatcherOP gCmnDispatcherOpIPC =
{
    (SChar *)"IPC",

    cmnDispatcherInitializeIPC,
    cmnDispatcherFinalizeIPC,

    cmnDispatcherAddLinkIPC,
    cmnDispatcherRemoveLinkIPC,
    cmnDispatcherRemoveAllLinksIPC,

    cmnDispatcherSelectIPC
};


IDE_RC cmnDispatcherMapIPC(cmnDispatcher *aDispatcher)
{
    /*
     * �Լ� ������ ����
     */

    aDispatcher->mOp = &gCmnDispatcherOpIPC;

    return IDE_SUCCESS;
}

UInt cmnDispatcherSizeIPC()
{
    return ID_SIZEOF(cmnDispatcherIPC);
}

IDE_RC cmnDispatcherWaitLinkIPC(cmnLink *        aLink,
                                cmnDirection     aDirection,
                                PDL_Time_Value * aTimeout)
{
    IDE_RC sRet = IDE_SUCCESS;

    cmnLinkPeer *sLink = (cmnLinkPeer*)aLink;
    PDL_Time_Value  sSleepTime;

    // bug-27250 free Buf list can be crushed when client killed
    if (aDirection == CMN_DIRECTION_WR)
    {
        // receiver�� �۽� ��� ��ȣ�� �ٶ����� ���� ���
        // cmiWriteBlock���� protocol end packet �۽Ž�
        // pending block�� �ִ� ��� �� �ڵ� ����
        if (aTimeout == NULL)
        {
            // server
            if (idlOS::strcmp(sLink->mLink.mOp->mName, "IPC-PEER-SERVER") == 0)
            {
                // cmnLinkPeerIPC (defined in cmnLinkPeerIPC.cpp)
                // ����ü�� ���� ������ �� ���,�ѹ��� ȣ��ó��.
                sRet = cmnLinkPeerWaitSendServerIPC(aLink);
            }
            // client
            else
            {
                sRet = cmnLinkPeerWaitSendClientIPC(aLink);
            }

        }
        // cmiWriteBlock���� �۽� ��� list�� �Ѿ ��� ����.
        else
        {
            sSleepTime.set(0, 1000); // wait 1 msec
            idlOS::sleep(sSleepTime);
        }
    }
    return sRet;
}


#endif
