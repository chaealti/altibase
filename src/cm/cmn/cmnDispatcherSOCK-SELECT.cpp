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
 * ����ü�� �Լ��� suffix 'Select'�� �߰�.
 */

#if !defined(CM_DISABLE_TCP) || !defined(CM_DISABLE_UNIX)

typedef struct cmnDispatcherSOCKSelect
{
    cmnDispatcher mDispatcher;

    PDL_SOCKET    mMaxHandle;

    fd_set        mFdSet;
} cmnDispatcherSOCKSelect;


IDE_RC cmnDispatcherInitializeSOCKSelect(cmnDispatcher *aDispatcher, UInt /*aMaxLink*/)
{
    cmnDispatcherSOCKSelect *sDispatcher = (cmnDispatcherSOCKSelect *)aDispatcher;

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

IDE_RC cmnDispatcherFinalizeSOCKSelect(cmnDispatcher * /*aDispatcher*/)
{

    return IDE_SUCCESS;
}

IDE_RC cmnDispatcherAddLinkSOCKSelect(cmnDispatcher *aDispatcher, cmnLink *aLink)
{
    cmnDispatcherSOCKSelect *sDispatcher = (cmnDispatcherSOCKSelect *)aDispatcher;
    PDL_SOCKET               sHandle;

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

IDE_RC cmnDispatcherRemoveLinkSOCKSelect(cmnDispatcher *aDispatcher, cmnLink *aLink)
{
    cmnDispatcherSOCKSelect *sDispatcher = (cmnDispatcherSOCKSelect *)aDispatcher;
    PDL_SOCKET               sHandle;

    /*
     * Link�� socket ȹ��
     */
    IDE_TEST(aLink->mOp->mGetHandle(aLink, &sHandle) != IDE_SUCCESS);

    /*
     * Dispatcher�� Link List���� ����
     */
    IDE_TEST(cmnDispatcherRemoveLink(aDispatcher, aLink) != IDE_SUCCESS);

    /*
     * FdSet���� socket ����
     */
    FD_CLR(sHandle, &sDispatcher->mFdSet);

    /*
     * MaxHandle ���
     */
    if (sDispatcher->mMaxHandle == sHandle)
    {
        sDispatcher->mMaxHandle = PDL_INVALID_SOCKET;

        if (aDispatcher->mLinkCount > 0)
        {
            do
            {
                sHandle--;

                if (FD_ISSET(sHandle, &sDispatcher->mFdSet))
                {
                    sDispatcher->mMaxHandle = sHandle;

                    break;
                }
            } while (sHandle != 0);

            IDE_ASSERT(sDispatcher->mMaxHandle != PDL_INVALID_SOCKET);
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmnDispatcherRemoveAllLinksSOCKSelect(cmnDispatcher *aDispatcher)
{
    cmnDispatcherSOCKSelect *sDispatcher = (cmnDispatcherSOCKSelect *)aDispatcher;

    IDE_TEST(cmnDispatcherRemoveAllLinks(aDispatcher) != IDE_SUCCESS);

    sDispatcher->mMaxHandle  = PDL_INVALID_SOCKET;

    FD_ZERO(&sDispatcher->mFdSet);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmnDispatcherDetectSOCKSelect(cmnDispatcher  *aDispatcher,
                                     iduList        *aReadyList,
                                     UInt           *aReadyCount,
                                     PDL_Time_Value *aTimeout)
{
    cmnDispatcherSOCKSelect *sDispatcher = (cmnDispatcherSOCKSelect *)aDispatcher;
    iduListNode             *sIterator;
    cmnLink                 *sLink;
    PDL_SOCKET               sHandle;
    SInt                     sResult = 0;
    IDU_LIST_INIT(aReadyList);

    /* PROJ-2108 Dedicated thread mode which uses less CPU */
    idBool             sIsDedicatedMode = ID_FALSE;

    if ( aTimeout != NULL )
    {
        if ( aTimeout->sec() == DEDICATED_THREAD_MODE_TIMEOUT_FLAG )
        {

            /* PROJ-2108 Dedicated thread mode which uses less CPU 
             * Set dedicated mode flag if
             * aTimeout->sec == magic number(765432) for infinite select()
             */
            sIsDedicatedMode = ID_TRUE;
        }
    }

    if (sDispatcher->mMaxHandle == PDL_INVALID_SOCKET)
    {
        if ( sIsDedicatedMode == ID_FALSE )
        {
            /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
               fd set null, max handle�� 0���� �Ѱܼ�
               select system call cost�� �����̶� �Ʋ�����.
             */
            sResult = idlOS::select(0, NULL, NULL, NULL, aTimeout);
        }
    }
    else
    {
        if ( sIsDedicatedMode == ID_FALSE )
        {
            sResult = idlOS::select(sDispatcher->mMaxHandle + 1,
                                    &sDispatcher->mFdSet,
                                    NULL,
                                    NULL,
                                    aTimeout);
        }
        else
        {
            sResult = idlOS::select(sDispatcher->mMaxHandle + 1,
                                    &sDispatcher->mFdSet,
                                    NULL,
                                    NULL,
                                    NULL);
        }
    }
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


struct cmnDispatcherOP gCmnDispatcherOpSOCKSelect =
{
    (SChar *)"SOCK-SELECT",

    cmnDispatcherInitializeSOCKSelect,
    cmnDispatcherFinalizeSOCKSelect,

    cmnDispatcherAddLinkSOCKSelect,
    cmnDispatcherRemoveLinkSOCKSelect,
    cmnDispatcherRemoveAllLinksSOCKSelect,

    cmnDispatcherDetectSOCKSelect
};


IDE_RC cmnDispatcherMapSOCKSelect(cmnDispatcher *aDispatcher)
{
    /*
     * �Լ� ������ ����
     */
    aDispatcher->mOp = &gCmnDispatcherOpSOCKSelect;

    return IDE_SUCCESS;
}

UInt cmnDispatcherSizeSOCKSelect()
{
    return ID_SIZEOF(cmnDispatcherSOCKSelect);
}

IDE_RC cmnDispatcherWaitLinkSOCKSelect(cmnLink *aLink,
                                       cmnDirection aDirection,
                                       PDL_Time_Value *aTimeout)
{
    PDL_SOCKET sHandle;
    SInt       sResult;
    fd_set     sFdSet;
    fd_set     sFdSet2;

    /*
     * Link�� socket ȹ��
     */
    IDE_TEST(aLink->mOp->mGetHandle(aLink, &sHandle) != IDE_SUCCESS);

    /*
     * fdset ����
     */
    FD_ZERO(&sFdSet);
    FD_SET(sHandle, &sFdSet);

    /*
     * select ����
     */
    switch (aDirection)
    {
        case CMN_DIRECTION_RD:
            sResult = idlOS::select(sHandle + 1, &sFdSet, NULL, NULL, aTimeout);
            break;
        case CMN_DIRECTION_WR:
            sResult = idlOS::select(sHandle + 1, NULL, &sFdSet, NULL, aTimeout);
            break;
        case CMN_DIRECTION_RDWR:
            idlOS::memcpy(&sFdSet2, &sFdSet, ID_SIZEOF(fd_set));
            sResult = idlOS::select(sHandle + 1, &sFdSet, &sFdSet2, NULL, aTimeout);
            break;
        default:
            sResult = idlOS::select(sHandle + 1, NULL, NULL, NULL, aTimeout);
            break;
    }

    IDE_TEST_RAISE(sResult < 0, SelectError);
    IDE_TEST_RAISE(sResult == 0, TimedOut);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SelectError);
    {
        /* BUG-47714 ���� �޼����� sock number �߰� */
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SELECT_ERROR, sHandle));
    }
    IDE_EXCEPTION(TimedOut);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_TIMED_OUT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-45240 */
SInt cmnDispatcherCheckHandleSOCKSelect(PDL_SOCKET      aHandle,
                                        PDL_Time_Value *aTimeout)
{
    SInt   sResult = -1;
    fd_set sFdSet;

    /* fdset ���� */
    FD_ZERO(&sFdSet);
    FD_SET(aHandle, &sFdSet);

    /* select ���� */
    sResult = idlOS::select(aHandle + 1, &sFdSet, NULL, NULL, aTimeout);

    return sResult;
}

#endif  /* !defined(CM_DISABLE_TCP) || !defined(CM_DISABLE_UNIX) */
