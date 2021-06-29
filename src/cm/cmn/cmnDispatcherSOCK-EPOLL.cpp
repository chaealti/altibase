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

/* 
 * BUG-38951 Support to choice a type of CM dispatcher on run-time
 *
 * ����ü�� �Լ��� suffix 'Epoll'�� �߰�. (BUG-45240)
 */

#if !defined(CM_DISABLE_TCP) || !defined(CM_DISABLE_UNIX)

IDE_RC cmnDispatcherFinalizeSOCKEpoll(cmnDispatcher *aDispatcher);

typedef struct cmnDispatcherSOCKEpoll
{
    cmnDispatcher       mDispatcher;

    UInt                mPollingFdCnt;  /* polling���� Fd ���� */

    SInt                mEpollFd;       /* A handle for Epoll */
    struct epoll_event *mEvents;        /* ��ȯ�� �̺�Ʈ ����Ʈ */
    UInt                mMaxEvents;     /* ������ �̺�Ʈ �ִ�� */
} cmnDispatcherSOCKEpoll;

IDE_RC cmnDispatcherInitializeSOCKEpoll(cmnDispatcher *aDispatcher, UInt aMaxLink)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;

    sDispatcher->mEpollFd      = PDL_INVALID_HANDLE;
    sDispatcher->mEvents       = NULL;
    sDispatcher->mMaxEvents    = 0;
    sDispatcher->mPollingFdCnt = 0;

    /*
     * Since Linux 2.6.8, the size argument is unused.
     * (The kernel dynamically sizes the required data structures without needing this initial hint.)
     */
    sDispatcher->mEpollFd = idlOS::epoll_create(aMaxLink);
    IDE_TEST_RAISE(sDispatcher->mEpollFd == PDL_INVALID_HANDLE, EpollCreateError);

    /* mEvents�� ���� �޸� �Ҵ� - ��� �����ұ�? */
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_CMN,
                                     ID_SIZEOF(struct epoll_event) * aMaxLink,
                                     (void **)&(sDispatcher->mEvents),
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory);

    sDispatcher->mMaxEvents = aMaxLink;

    return IDE_SUCCESS;

    IDE_EXCEPTION(EpollCreateError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SYSTEM_CALL_ERROR, "epoll_create(aMaxLink)"));
    }
    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    cmnDispatcherFinalizeSOCKEpoll(aDispatcher);

    return IDE_FAILURE;
}

IDE_RC cmnDispatcherFinalizeSOCKEpoll(cmnDispatcher *aDispatcher)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;

    if (sDispatcher->mEpollFd != PDL_INVALID_HANDLE)
    {
        (void)idlOS::close(sDispatcher->mEpollFd);
        sDispatcher->mEpollFd = PDL_INVALID_HANDLE;
    }
    else
    {
        /* A obsolete convention */
    }

    /* BUG-41458 �Ҵ��� �޸� ���� */
    if (sDispatcher->mEvents != NULL)
    {
        (void)iduMemMgr::free(sDispatcher->mEvents);
        sDispatcher->mEvents = NULL;
    }
    else
    {
        /* A obsolete convention */
    }

    sDispatcher->mMaxEvents    = 0;
    sDispatcher->mPollingFdCnt = 0;

    return IDE_SUCCESS;
}

IDE_RC cmnDispatcherAddLinkSOCKEpoll(cmnDispatcher *aDispatcher, cmnLink *aLink)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;
    PDL_SOCKET              sHandle     = PDL_INVALID_SOCKET;
    struct epoll_event      sEvent;

    /* Link�� socket ȹ�� */
    IDE_TEST(aLink->mOp->mGetHandle(aLink, &sHandle) != IDE_SUCCESS);

    /* socket �߰� */
    sEvent.events   = EPOLLIN;
    sEvent.data.ptr = (void *)aLink;
    IDE_TEST_RAISE(idlOS::epoll_ctl(sDispatcher->mEpollFd,
                                    EPOLL_CTL_ADD,
                                    sHandle,
                                    &sEvent) < 0, EpollCtlError);

    sDispatcher->mPollingFdCnt++;

    /* Dispatcher�� Link list�� �߰� */
    IDE_TEST(cmnDispatcherAddLink(aDispatcher, aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(EpollCtlError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SYSTEM_CALL_ERROR, "epoll_ctl(add)"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnDispatcherRemoveLinkSOCKEpoll(cmnDispatcher *aDispatcher, cmnLink *aLink)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;
    PDL_SOCKET              sHandle     = PDL_INVALID_SOCKET;
    struct epoll_event      sEvent;

    /* Link�� socket ȹ�� */
    IDE_TEST(aLink->mOp->mGetHandle(aLink, &sHandle) != IDE_SUCCESS); 

    /* 2.6.9 ���� ������ ȣȯ�� ������ ���� sEvent ���� */
    sEvent.events   = EPOLLIN;
    sEvent.data.ptr = (void *)aLink;
    IDE_TEST_RAISE(idlOS::epoll_ctl(sDispatcher->mEpollFd,
                                    EPOLL_CTL_DEL,
                                    sHandle,
                                    &sEvent) < 0, EpollCtlError);

    sDispatcher->mPollingFdCnt--;

    /* Dispatcher�� Link list�� ���� */
    IDE_TEST(cmnDispatcherRemoveLink(aDispatcher, aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(EpollCtlError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SYSTEM_CALL_ERROR, "epoll_ctl(del)"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnDispatcherRemoveAllLinksSOCKEpoll(cmnDispatcher *aDispatcher)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;

    IDE_TEST(cmnDispatcherRemoveAllLinks(aDispatcher) != IDE_SUCCESS);

    (void)idlOS::close(sDispatcher->mEpollFd);

    sDispatcher->mEpollFd = idlOS::epoll_create(sDispatcher->mMaxEvents);
    IDE_TEST_RAISE(sDispatcher->mEpollFd == PDL_INVALID_HANDLE, EpollCreateError);

    return IDE_SUCCESS;

    IDE_EXCEPTION(EpollCreateError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SYSTEM_CALL_ERROR, "epoll_create(mMaxEvents)"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnDispatcherDetectSOCKEpoll(cmnDispatcher  *aDispatcher,
                                    iduList        *aReadyList,
                                    UInt           *aReadyCount,
                                    PDL_Time_Value *aTimeout)
{
    cmnDispatcherSOCKEpoll *sDispatcher = (cmnDispatcherSOCKEpoll *)aDispatcher;
    cmnLink                *sLink       = NULL;
    SInt                    sResult     = 0;
    SInt                    i           = 0;

    /* PROJ-2108 Dedicated thread mode which uses less CPU */
    idBool                 sIsDedicatedMode = ID_FALSE;

    IDU_LIST_INIT(aReadyList);

    /*
     * BUG-39068 Verify the poll() system call in case
     *           the service threads are the dedicated mode
     */
    if (aTimeout != NULL)
    {
        if (aTimeout->sec() == DEDICATED_THREAD_MODE_TIMEOUT_FLAG)
        {

            /* 
             * PROJ-2108 Dedicated thread mode which uses less CPU 
             * Set dedicated mode flag if
             * aTimeout->sec == magic number(765432) for infinite select()
             */
            sIsDedicatedMode = ID_TRUE;
        }
    }

    if (sIsDedicatedMode == ID_TRUE)
    {
        if (sDispatcher->mPollingFdCnt > 0)
        {
            sResult = idlOS::epoll_wait( sDispatcher->mEpollFd,
                                         sDispatcher->mEvents,
                                         sDispatcher->mMaxEvents,
                                         NULL );
        }
        else
        {
            /* 
             * Client�� ������ ����̸� DedicatedMode������ epoll_wait()�� ȣ���� �ʿ䰡 ����.
             * DedicatedMode������ �ش� Servicethread�� cond_wait ���·� ���� �����̴�.
             */
        }
    }
    else
    {
        sResult = idlOS::epoll_wait( sDispatcher->mEpollFd,
                                     sDispatcher->mEvents,
                                     sDispatcher->mMaxEvents,
                                     aTimeout );
    }

    IDE_TEST_RAISE(sResult < 0, EpollWaitError);

    /* Ready Link �˻� */
    for (i = 0; i < sResult; i++)
    {
        IDE_ASSERT(sDispatcher->mEvents[i].data.ptr != NULL);

        sLink = (cmnLink *)sDispatcher->mEvents[i].data.ptr;
        IDU_LIST_ADD_LAST(aReadyList, &sLink->mReadyListNode);
    }

    /* ReadyCount ���� */
    if (aReadyCount != NULL)
    {
        *aReadyCount = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(EpollWaitError);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SYSTEM_CALL_ERROR, "epoll_wait()", errno));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


struct cmnDispatcherOP gCmnDispatcherOpSOCKEpoll =
{
    (SChar *)"SOCK-EPOLL",

    cmnDispatcherInitializeSOCKEpoll,
    cmnDispatcherFinalizeSOCKEpoll,

    cmnDispatcherAddLinkSOCKEpoll,
    cmnDispatcherRemoveLinkSOCKEpoll,
    cmnDispatcherRemoveAllLinksSOCKEpoll,

    cmnDispatcherDetectSOCKEpoll
};


IDE_RC cmnDispatcherMapSOCKEpoll(cmnDispatcher *aDispatcher)
{
    /* �Լ� ������ ���� */
    aDispatcher->mOp = &gCmnDispatcherOpSOCKEpoll;

    return IDE_SUCCESS;
}

UInt cmnDispatcherSizeSOCKEpoll()
{
    return ID_SIZEOF(cmnDispatcherSOCKEpoll);
}

IDE_RC cmnDispatcherWaitLinkSOCKEpoll(cmnLink        * /*aLink*/,
                                      cmnDirection     /*aDirection*/,
                                      PDL_Time_Value * /*aTimeout*/)
{
    /* 
     * Epoll�� 1ȸ�� FD�� �����ϴ� �뵵�δ� �ý��� �� ����� �� ũ��.
     * Poll�� ��õ�Ѵ�. (epoll_create() -> epoll_ctl() -> close()) 
     */
    IDE_ASSERT(0);

    return IDE_FAILURE;
}

SInt cmnDispatcherCheckHandleSOCKEpoll(PDL_SOCKET       /*aHandle*/,
                                       PDL_Time_Value * /*aTimeout*/)
{
    /* 
     * Epoll�� 1ȸ�� FD�� �����ϴ� �뵵�δ� �ý��� �� ����� �� ũ��.
     * Poll�� ��õ�Ѵ�. (epoll_create() -> epoll_ctl() -> close()) 
     */
    IDE_ASSERT(0);

    return -1;
}

#endif  /* !defined(CM_DISABLE_TCP) || !defined(CM_DISABLE_UNIX) */
