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


typedef struct cmnLinkAllocInfo
{
    ACI_RC       (*mMap)(cmnLink *aLink);
    acp_uint32_t (*mSize)();
} cmnLinkAllocInfo;

static cmnLinkAllocInfo gCmnLinkAllocInfoClient[CMN_LINK_IMPL_MAX][CMN_LINK_TYPE_MAX] =
{
    /* DUMMY : client no use */
    {
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
    },

    /* TCP */
    {
#if defined(CM_DISABLE_TCP)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { cmnLinkListenMapTCP, cmnLinkListenSizeTCP },
        { cmnLinkPeerMapTCP,   cmnLinkPeerSizeTCP   },
        { cmnLinkPeerMapTCP,   cmnLinkPeerSizeTCP   },
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
        { NULL,                    NULL                     },
        { NULL,                    NULL                     },
        { cmnLinkPeerClientMapIPC, cmnLinkPeerClientSizeIPC },
#endif
    },

    /* SSL */
    {
#if defined(CM_DISABLE_SSL)
        { NULL, NULL },
        { NULL, NULL }, 
        { NULL, NULL }, 
#else
        { NULL, NULL }, 
        { cmnLinkPeerMapSSL, cmnLinkPeerSizeSSL },
        { cmnLinkPeerMapSSL, cmnLinkPeerSizeSSL },
#endif
    },

    /* IPCDA */
    {
#if defined(CM_DISABLE_IPCDA)
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
#else
        { NULL,                    NULL                     },
        { NULL,                    NULL                     },
        { cmnLinkPeerClientMapIPCDA, cmnLinkPeerClientSizeIPCDA },
#endif
    },

    /* IB : PROJ-2681 */
    {
        { cmnLinkListenMapIB, cmnLinkListenSizeIB },
        { cmnLinkPeerMapIB,   cmnLinkPeerSizeIB   },
        { cmnLinkPeerMapIB,   cmnLinkPeerSizeIB   },
    }
};

static acp_uint32_t gCmnLinkFeatureClient[CMN_LINK_IMPL_MAX][CMN_LINK_TYPE_MAX] =
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
    { 0, 0, 0 },

    /* IPCDA */
    { 0, 0, 0 },

    /* IB : PROJ-2681 */
    { 0, CMN_LINK_FEATURE_SYSDBA, 0 },
};


acp_bool_t cmnLinkIsSupportedImpl(cmnLinkImpl aImpl)
{
    acp_bool_t sIsSupported = ACP_FALSE;

    switch (aImpl)
    {
#if !defined(CM_DISABLE_TCP)
        case CMN_LINK_IMPL_TCP:
#endif
#if !defined(CM_DISABLE_UNIX)
        case CMN_LINK_IMPL_UNIX:
#endif
#if !defined(CM_DISABLE_IPC)
        case CMN_LINK_IMPL_IPC:
#endif
#if !defined(CM_DISABLE_SSL)
        case CMN_LINK_IMPL_SSL:
#endif
#if !defined(CM_DISABLE_IPCDA)
        case CMN_LINK_IMPL_IPCDA:
#endif
        /* PROJ-2681 */    
        case CMN_LINK_IMPL_IB:
            sIsSupported = ACP_TRUE;
            break;

        default:
            break;
    }

    return sIsSupported;
}

ACI_RC cmnLinkAlloc(cmnLink **aLink, cmnLinkType aType, cmnLinkImpl aImpl)
{
    acp_uint32_t      sImpl = aImpl;
    cmnLinkAllocInfo *sAllocInfo;
    cmnLink          *sLink = NULL;

    /*
     * �����ϴ� Impl���� �˻�
     */
    ACI_TEST_RAISE(cmnLinkIsSupportedImpl(aImpl) != ACP_TRUE, UnsupportedLinkImpl);

    /*
     * AllocInfo ȹ��
     */
    sAllocInfo = &gCmnLinkAllocInfoClient[sImpl][aType];

    ACE_ASSERT(sAllocInfo->mMap  != NULL);
    ACE_ASSERT(sAllocInfo->mSize != NULL);

    /*
     * �޸� �Ҵ�
     */
    ACI_TEST(acpMemAlloc((void **)&sLink, sAllocInfo->mSize()) != ACP_RC_SUCCESS);

    /*
     * ��� �ʱ�ȭ
     */
    sLink->mType    = aType;
    sLink->mImpl    = aImpl;
    sLink->mFeature = gCmnLinkFeatureClient[aImpl][aType];
    /* proj_2160 cm_type removal */
    sLink->mPacketType = CMP_PACKET_TYPE_UNKNOWN;

    acpListInitObj(&sLink->mDispatchListNode, sLink);
    acpListInitObj(&sLink->mReadyListNode, sLink);

    /*
     * �Լ� ������ ����
     */
    ACI_TEST_RAISE(sAllocInfo->mMap(sLink) != ACI_SUCCESS, InitializeFail);

    /*
     * �ʱ�ȭ
     */
    ACI_TEST_RAISE(sLink->mOp->mInitialize(sLink) != ACI_SUCCESS, InitializeFail);

    *aLink = sLink;

    return ACI_SUCCESS;

    ACI_EXCEPTION(UnsupportedLinkImpl);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_UNSUPPORTED_LINK_IMPL));
    }
    ACI_EXCEPTION(InitializeFail);
    {
        acpMemFree(sLink);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmnLinkFree(cmnLink *aLink)
{
    /*
     * Dispatcher�� ��ϵǾ� �ִ� Link���� �˻�
     */
    ACE_ASSERT(acpListIsEmpty(&aLink->mDispatchListNode) == ACP_TRUE);

    /*
     * ����
     */
    ACI_TEST(aLink->mOp->mFinalize(aLink) != ACI_SUCCESS);

    /*
     * �޸� ����
     */
    acpMemFree(aLink);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
