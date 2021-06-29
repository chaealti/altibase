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


typedef struct cmnLinkAllocInfo
{
    IDE_RC (*mMap)(cmnLink *aLink);
    UInt   (*mSize)();
} cmnLinkAllocInfo;

static cmnLinkAllocInfo gCmnLinkAllocInfo[CMN_LINK_IMPL_MAX][CMN_LINK_TYPE_MAX] =
{
    /* DUMMY */
    {
        { NULL, NULL },
        { cmnLinkPeerServerMapDUMMY,  cmnLinkPeerServerSizeDUMMY },
        { NULL, NULL },
    },

    /* TCP */
    {
#if defined(CM_DISABLE_TCP)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapTCP,  cmnLinkListenSizeTCP  },
        { cmnLinkPeerMapTCP,    cmnLinkPeerSizeTCP    },
        { cmnLinkPeerMapTCP,    cmnLinkPeerSizeTCP    },
#endif
    },

    /* UNIX */
    {
#if defined(CM_DISABLE_UNIX)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapUNIX, cmnLinkListenSizeUNIX },
        { cmnLinkPeerMapUNIX,   cmnLinkPeerSizeUNIX   },
        { cmnLinkPeerMapUNIX,   cmnLinkPeerSizeUNIX   },
#endif
    },

    /* IPC */
    {
#if defined(CM_DISABLE_IPC)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapIPC,      cmnLinkListenSizeIPC     },
        { cmnLinkPeerServerMapIPC,  cmnLinkPeerServerSizeIPC },
        { cmnLinkPeerClientMapIPC,  cmnLinkPeerClientSizeIPC },
#endif
    },

    /* PROJ-2474 SSL/TLS */
    {
#if defined(CM_DISABLE_SSL)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapSSL, cmnLinkListenSizeSSL },
        { cmnLinkPeerMapSSL,   cmnLinkPeerSizeSSL   },
        { cmnLinkPeerMapSSL,   cmnLinkPeerSizeSSL   },
#endif
    },

    {
#if defined(CM_DISABLE_IPCDA)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapIPCDA, cmnLinkListenSizeIPCDA },
        { cmnLinkPeerServerMapIPCDA,  cmnLinkPeerServerSizeIPCDA },
        { cmnLinkPeerClientMapIPCDA,  cmnLinkPeerClientSizeIPCDA },
#endif
    },

    /* PROJ-2681 */
    {
        { cmnLinkListenMapIB, cmnLinkListenSizeIB },
        { cmnLinkPeerMapIB,   cmnLinkPeerSizeIB   },
        { cmnLinkPeerMapIB,   cmnLinkPeerSizeIB   },
    },
};

static UInt gCmnLinkFeature[CMN_LINK_IMPL_MAX][CMN_LINK_TYPE_MAX] =
{
    // bug-19279 remote sysdba enable + sys can kill session
    // �� ���� ���Ǵ� �κ�:
    // mmtServiceThread::connectProtocol -> mmcTask::authenticate()
    // ���� �Լ����� client�� sysdba�� ������ ��� ���� task�� link��
    // CMN_LINK_FEATURE_SYSDBA Ư���� �����߸� ������ ���ȴ�.
    // ���� ����:
    // task ������ �������� tcp link�� ��� sysdbaƯ���� �����µ�
    // ���� sysdba  ������ ����ϱ� ���� ��� tcp link�� ����
    // sysdbaƯ���� �ο��Ѵ�
    // => �ᱹ ���� task�� link�� ���� sysdba Ư�� �ʵ尡 ���ǹ��� ����.
    // listen,   server,    client

    /* DUMMY */
    { 0, 0, 0 },

    /* TCP */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },

    /* UNIX */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },

    /* IPC */
    /* TASK-5894 Permit sysdba via IPC */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },

    /* PROJ-2474 SSL/TLS */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },

    /* IPCDA */
    { 0, 0, 0 },

    /* IB, PROJ-2681 */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },
};


idBool cmnLinkIsSupportedImpl(cmnLinkImpl aImpl)
{
    idBool sIsSupported = ID_FALSE;

    switch (aImpl)
    {
        case CMN_LINK_IMPL_DUMMY:
#if !defined(CM_DISABLE_TCP)
        case CMN_LINK_IMPL_TCP:
#endif
#if !defined(CM_DISABLE_UNIX)
        case CMN_LINK_IMPL_UNIX:
#endif
#if !defined(CM_DISABLE_IPC)
        case CMN_LINK_IMPL_IPC:
#endif
/* PROJ-2474 SSL/TLS */
#if !defined(CM_DISABLE_SSL)
        case CMN_LINK_IMPL_SSL:
#endif
#if !defined(CM_DISABLE_IPCDA)
        case CMN_LINK_IMPL_IPCDA:
#endif
        /* PROJ-2681 */
        case CMN_LINK_IMPL_IB:
            sIsSupported = ID_TRUE;
            break;

        default:
            break;
    }

    return sIsSupported;
}

IDE_RC cmnLinkAlloc(cmnLink **aLink, cmnLinkType aType, cmnLinkImpl aImpl)
{
    UInt sImpl = aImpl;
    cmnLinkAllocInfo *sAllocInfo;

    /*
     * �����ϴ� Impl���� �˻�
     */
    IDE_TEST_RAISE(cmnLinkIsSupportedImpl(aImpl) != ID_TRUE, UnsupportedLinkImpl);

    /*
     * AllocInfo ȹ��
     */
    sAllocInfo = &gCmnLinkAllocInfo[sImpl][aType];

    IDE_ASSERT(sAllocInfo->mMap  != NULL);
    IDE_ASSERT(sAllocInfo->mSize != NULL);

    IDU_FIT_POINT_RAISE( "cmnLink::cmnLinkAlloc::malloc::Link",
                          InsufficientMemory );

    /*
     * �޸� �Ҵ�
     */
     
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_CMN,
                                     sAllocInfo->mSize(),
                                     (void **)aLink,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );

    /*
     * ��� �ʱ�ȭ
     */
    (*aLink)->mType    = aType;
    (*aLink)->mImpl    = aImpl;
    (*aLink)->mFeature = gCmnLinkFeature[aImpl][aType];
    /* proj_2160 cm_type removal */
    /* this is why mPacketType exists in cmnLink, not in cmnLinkPeer */
    (*aLink)->mPacketType = CMP_PACKET_TYPE_UNKNOWN;

    IDU_LIST_INIT_OBJ(&(*aLink)->mDispatchListNode, *aLink);
    IDU_LIST_INIT_OBJ(&(*aLink)->mReadyListNode, *aLink);

    /*
     * �Լ� ������ ����
     */
    IDE_TEST_RAISE(sAllocInfo->mMap(*aLink) != IDE_SUCCESS, InitializeFail);

    /*
     * �ʱ�ȭ
     */
    IDE_TEST_RAISE((*aLink)->mOp->mInitialize(*aLink) != IDE_SUCCESS, InitializeFail);

    return IDE_SUCCESS;

    IDE_EXCEPTION(UnsupportedLinkImpl);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_UNSUPPORTED_LINK_IMPL));
    }
    IDE_EXCEPTION(InitializeFail);
    {
        IDE_ASSERT(iduMemMgr::free(*aLink) == IDE_SUCCESS);
        *aLink = NULL;
    }
    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmnLinkFree(cmnLink *aLink)
{
    /*
     * Dispatcher�� ��ϵǾ� �ִ� Link���� �˻�
     */
    IDE_ASSERT(IDU_LIST_IS_EMPTY(&aLink->mDispatchListNode) == ID_TRUE);

    /*
     * ����
     */
    IDE_TEST(aLink->mOp->mFinalize(aLink) != IDE_SUCCESS);

    /*
     * �޸� ����
     */
    IDE_TEST(iduMemMgr::free(aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

