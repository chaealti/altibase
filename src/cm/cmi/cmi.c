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

#include <aclLZ4.h>
#include <aclCompression.h>
#include <aciVersion.h>

extern cmpOpMap gCmpOpBaseMapClient[];
extern cmpOpMap gCmpOpDBMapClient[];
extern cmpOpMap gCmpOpRPMapClient[];

extern cmbShmIPCDAInfo gIPCDAShmInfo;

#if !defined(CM_DISABLE_SSL)
extern cmnOpenssl *gOpenssl; /* BUG-45235 */
#endif


acp_char_t *gCmErrorFactory[] =
{
#include "E_CM_US7ASCII.c"
};

acp_sint8_t * cmnLinkPeerGetWriteBlock(acp_uint32_t aChannelID);
acp_sint8_t * cmnLinkPeerGetReadBlock(acp_uint32_t aChannelID);

/* PROJ-2616 */
void cmiInitIPCDABuffer(cmiProtocolContext *aCtx);

/*
 * BUG-19465 : CM_Buffer�� pending list�� ����
 */
acp_uint32_t     gMaxPendingList;

/*
 * BUG-21080
 */
static acp_thr_once_t     gCMInitOnceClient  = ACP_THR_ONCE_INIT;
static acp_thr_mutex_t    gCMInitMutexClient = ACP_THR_MUTEX_INITIALIZER;
static acp_sint32_t       gCMInitCountClient;

cmbShmIPCDAChannelInfo *cmnLinkPeerGetChannelInfoIPCDA(int aChannleID);

static void cmiInitializeOnce( void )
{
    ACE_ASSERT(acpThrMutexCreate(&gCMInitMutexClient, ACP_THR_MUTEX_DEFAULT)
               == ACP_RC_SUCCESS);

    gCMInitCountClient = 0;
}

#define CMI_CHECK_BLOCK_FOR_READ(aBlock)  ((aBlock)->mCursor < (aBlock)->mDataSize)
#define CMI_CHECK_BLOCK_FOR_WRITE(aBlock) ((aBlock)->mCursor < (aBlock)->mBlockSize)

/* BUG-41330 */
#define CMI_GET_ERROR_COUNT(aErrorFactory) (ACI_SIZEOF(aErrorFactory) / ACI_SIZEOF((aErrorFactory)[0]))

/*
 * Packet Header�κ��� Module�� ���Ѵ�.
 */
static ACI_RC cmiGetModule(cmpHeader *aHeader, cmpModule **aModule)
{
    /*
     * Module ȹ��
     */
    ACI_TEST_RAISE(aHeader->mA5.mModuleID >= CMP_MODULE_MAX, UnknownModule);

    *aModule = gCmpModuleClient[aHeader->mA5.mModuleID];

    return ACI_SUCCESS;

    ACI_EXCEPTION(UnknownModule);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_UNKNOWN_MODULE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ProtocolContext�� Free ������� Read Block���� ��ȯ�Ѵ�.
 */
static ACI_RC cmiFreeReadBlock(cmiProtocolContext *aProtocolContext)
{
    cmnLinkPeer *sLink;
    cmbBlock    *sBlock;

    acp_list_node_t *sIterator;
    acp_list_node_t *sNodeNext;

    /*
     * Protocol Context�κ��� Link ȹ��
     */
    sLink = aProtocolContext->mLink;

    /*
     * Read Block List�� Block�� ��ȯ
     */

    ACP_LIST_ITERATE_SAFE(&aProtocolContext->mReadBlockList, sIterator, sNodeNext)
    {
        sBlock = (cmbBlock *)sIterator->mObj;

        ACI_TEST(sLink->mPeerOp->mFreeBlock(sLink, sBlock) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACP_INLINE acp_bool_t cmiIPCDACheckLinkAndWait(cmiProtocolContext *aCtx,
                                               acp_uint32_t        aMicroSleepTime,
                                               acp_uint32_t       *aLoopCount,
                                               acp_uint32_t       *aMaxLoopCount)
{
    acp_bool_t      sLinkConState = ACP_FALSE;

    if ((++(*aLoopCount)) == *aMaxLoopCount)
    {
        aCtx->mLink->mPeerOp->mCheck(aCtx->mLink, &sLinkConState);
        ACI_TEST(sLinkConState == ACP_TRUE);

        if (aMicroSleepTime == 0)
        {
            acpThrYield();
        }
        else
        {
            acpSleepUsec(aMicroSleepTime);
        }
        *aLoopCount = 0;

        if (((*aMaxLoopCount) / 2) < CMI_IPCDA_SPIN_MIN_LOOP)
        {
            (*aMaxLoopCount) = CMI_IPCDA_SPIN_MIN_LOOP;
        }
        else
        {
            (*aMaxLoopCount) = (*aMaxLoopCount) / 2;
        }
    }
    else
    {
        acpThrYield();
    }

    return ACP_TRUE;

    ACI_EXCEPTION_END;

    /* BUG-46390 IPCDA client�� mIsDisconnect�� �������� ���� ���Ḧ �ǹ��� */
    aCtx->mIsDisconnect = ACP_TRUE;

    return ACP_FALSE;
}

/*
 * PROJ-2616
 *
 *
 */
ACI_RC cmiIPCDACheckReadFlag(void *aCtx, void *aBlock, acp_uint32_t aMicroSleepTime, acp_uint32_t aExpireCount)
{
    cmiProtocolContext *sCtx   = (cmiProtocolContext *)aCtx;
    cmbBlockIPCDA      *sBlock = (cmbBlockIPCDA*)(aBlock!=NULL?aBlock:sCtx->mReadBlock);

    acp_uint32_t        sExpireCount    = aExpireCount * 1000;
    acp_uint32_t        sTotalLoopCount = 0; // for expired count.
    acp_uint32_t        sLoopCount      = 0;
    acp_uint32_t        sMaxLoopCount   = CMI_IPCDA_SPIN_MAX_LOOP;

    ACI_TEST_RAISE(sCtx->mIsDisconnect == ACP_TRUE, err_disconnected);

    /* BUG-46502 atomic �Լ� ���� */
    while ( acpAtomicGet32(&sBlock->mRFlag) == CMB_IPCDA_SHM_DEACTIVATED )
    {
        ACI_TEST_RAISE(((aExpireCount > 0) && ((sTotalLoopCount++) ==  sExpireCount)), err_loop_expired);

        ACI_TEST_RAISE(cmiIPCDACheckLinkAndWait(sCtx,
                                                aMicroSleepTime, 
                                                &sLoopCount, 
                                                &sMaxLoopCount) != ACP_TRUE,
                       err_disconnected);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(err_loop_expired)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_TIMED_OUT));
    }
    ACI_EXCEPTION(err_disconnected)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACP_INLINE ACI_RC cmiIPCDACheckDataCount(void                  *aCtx,
                                         volatile acp_uint32_t *aCountPtr,
                                         acp_uint32_t           aCompValue,
                                         acp_uint32_t           aExpireCount,
                                         acp_uint32_t           aMicroSleepTime)
{
    cmiProtocolContext *sCtx  = (cmiProtocolContext *)aCtx;
    acp_uint32_t        sTotalLoopCount = 0;  // for expired count.
    acp_uint32_t        sExpireCount    = aExpireCount * 1000;
    acp_uint32_t        sLoopCount      = 0;
    acp_uint32_t        sMaxLoopCount   = CMI_IPCDA_SPIN_MAX_LOOP;

    ACI_TEST(aCountPtr == NULL);

    /* BUG-46502 atomic �Լ� ���� */
    while ( aCompValue >= (acp_uint32_t)acpAtomicGet32(aCountPtr) )
    {
        ACI_TEST_RAISE(((aExpireCount > 0) && ((sTotalLoopCount++) ==  sExpireCount)), err_loop_expired);
        ACI_TEST_RAISE(cmiIPCDACheckLinkAndWait(sCtx,
                                                aMicroSleepTime, 
                                                &sLoopCount, 
                                                &sMaxLoopCount) != ACP_TRUE,
                       err_disconnected);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(err_loop_expired)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_TIMED_OUT));
    }
    ACI_EXCEPTION(err_disconnected)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void cmiLinkPeerFinalizeCliReadForIPCDA(cmiProtocolContext *aCtx)
{
    cmbBlockIPCDA *sReadBlock = (cmbBlockIPCDA*)aCtx->mReadBlock;

    if (sReadBlock != NULL)
    {
        /* BUG-46502 operationCount �ʱ�ȭ */
        acpAtomicSet32(&sReadBlock->mOperationCount, 0);

        /* BUG-46502 atomic �Լ� ���� */
        acpAtomicSet32(&sReadBlock->mRFlag, CMB_IPCDA_SHM_DEACTIVATED);
    }

    return;
}

/*
 * Block�� �о�´�.
 */
static ACI_RC cmiReadBlock(cmiProtocolContext *aProtocolContext, acp_time_t aTimeout)
{
    acp_uint32_t sCmSeqNo;

    ACI_TEST_RAISE(aProtocolContext->mIsDisconnect == ACP_TRUE, Disconnected);

    /*
     * Link�κ��� Block�� �о��
     */
    ACI_TEST(aProtocolContext->mLink->mPeerOp->mRecv(aProtocolContext->mLink,
                                                     &aProtocolContext->mReadBlock,
                                                     &aProtocolContext->mReadHeader,
                                                     aTimeout) != ACI_SUCCESS);

    /*
     * Sequence �˻�
     */
    sCmSeqNo = CMP_HEADER_SEQ_NO(&aProtocolContext->mReadHeader);

    ACI_TEST_RAISE(sCmSeqNo != aProtocolContext->mCmSeqNo, InvalidProtocolSequence);

    /*
     * Next Sequence ����
     */
    if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader) == ACP_TRUE)
    {
        aProtocolContext->mCmSeqNo = 0;
    }
    else
    {
        aProtocolContext->mCmSeqNo = sCmSeqNo + 1;
    }

    /*
     * Module ȹ��
     */
    ACI_TEST(cmiGetModule(&aProtocolContext->mReadHeader,
                          &aProtocolContext->mModule) != ACI_SUCCESS);

    /*
     * ReadHeader�κ��� WriteHeader�� �ʿ��� ������ ȹ��
     */
    aProtocolContext->mWriteHeader.mA5.mModuleID        = aProtocolContext->mReadHeader.mA5.mModuleID;
    aProtocolContext->mWriteHeader.mA5.mModuleVersion   = aProtocolContext->mReadHeader.mA5.mModuleVersion;
    aProtocolContext->mWriteHeader.mA5.mSourceSessionID = aProtocolContext->mReadHeader.mA5.mTargetSessionID;
    aProtocolContext->mWriteHeader.mA5.mTargetSessionID = aProtocolContext->mReadHeader.mA5.mSourceSessionID;

    return ACI_SUCCESS;

    ACI_EXCEPTION(Disconnected);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(InvalidProtocolSequence);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * Block�� �����Ѵ�.
 */
static ACI_RC cmiWriteBlock(cmiProtocolContext *aProtocolContext, acp_bool_t aIsEnd)
{
    cmnLinkPeer     *sLink   = aProtocolContext->mLink;
    cmbBlock        *sBlock  = aProtocolContext->mWriteBlock;
    cmpHeader       *sHeader = &aProtocolContext->mWriteHeader;
    cmbBlock        *sPendingBlock;
    acp_list_node_t *sIterator;
    acp_list_node_t *sNodeNext;
    acp_bool_t       sSendSuccess;

    /*
     * bug-27250 IPC linklist can be crushed.
     */
    acp_time_t        sWaitTime;
    cmnDispatcherImpl sImpl;

    /*
     * �������� ���̶�� Sequence ���� ����
     */
    if (aIsEnd == ACP_TRUE)
    {
        CMP_HEADER_SET_PROTO_END(sHeader);

        if (aProtocolContext->mIsAddReadBlock == ACP_FALSE)
        {
            if (aProtocolContext->mReadBlock != NULL)
            {
                aProtocolContext->mIsAddReadBlock = ACP_TRUE;

                /*
                 * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
                 */
                acpListAppendNode(&aProtocolContext->mReadBlockList,
                                    &aProtocolContext->mReadBlock->mListNode);

                aProtocolContext->mReadBlock = NULL;
                ACI_TEST(cmiFreeReadBlock(aProtocolContext) != ACI_SUCCESS);
            }
        }
    }

    /*
     * Protocol Header ���
     */
    ACI_TEST(cmpHeaderWrite(sHeader, sBlock) != ACI_SUCCESS);

    /*
     * Pending Write Block���� ����
     */
    ACP_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
    {
        sPendingBlock = (cmbBlock *)sIterator->mObj;

        sSendSuccess = ACP_TRUE;

        /*
         * BUG-19465 : CM_Buffer�� pending list�� ����
         */
        while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != ACI_SUCCESS)
        {
            sSendSuccess = ACP_FALSE;

            ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);

            /* BUG-44468 [cm] codesonar warning in CM */
            if( aProtocolContext->mListLength <= gMaxPendingList )
            {
                break;
            }

            /* bug-27250 IPC linklist can be crushed.
             * ������: all timeout NULL, ������: 1 msec for IPC
             * IPC�� ��� ���Ѵ���ϸ� �ȵȴ�.
             */
            sImpl = cmnDispatcherImplForLinkImpl(((cmnLink*)sLink)->mImpl);
            if (sImpl == CMI_DISPATCHER_IMPL_IPC)
            {
                sWaitTime = acpTimeFrom(0, 1000);
            }
            else
            {
                sWaitTime = ACP_TIME_INFINITE;
            }

            ACI_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                           CMN_DIRECTION_WR,
                                           sWaitTime) != ACI_SUCCESS);

            sSendSuccess = ACP_TRUE;
        }

        if (sSendSuccess == ACP_FALSE)
        {
            break;
        }

        aProtocolContext->mListLength--;
    }

    /*
     * Pending Write Block�� ������ ���� Block ����
     */
    if (sIterator == &aProtocolContext->mWriteBlockList)
    {
        if (sLink->mPeerOp->mSend(sLink, sBlock) != ACI_SUCCESS)
        {
            ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);

            acpListAppendNode(&aProtocolContext->mWriteBlockList, &sBlock->mListNode);
            aProtocolContext->mListLength++;
        }
    }
    else
    {
        acpListAppendNode(&aProtocolContext->mWriteBlockList, &sBlock->mListNode);
        aProtocolContext->mListLength++;
    }

    /*
     * Protocol Context�� Write Block ����
     */
    sBlock                        = NULL;
    aProtocolContext->mWriteBlock = NULL;

    /*
     * Sequence Number
     */
    if (aIsEnd == ACP_TRUE)
    {
        aProtocolContext->mWriteHeader.mA5.mCmSeqNo = 0;

        /*
         * �������� ���̶�� ��� Block�� ���۵Ǿ�� ��
         */
        ACP_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
        {
            sPendingBlock = (cmbBlock *)sIterator->mObj;

            while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != ACI_SUCCESS)
            {
                ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);
                ACI_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                               CMN_DIRECTION_WR,
                                               ACP_TIME_INFINITE) != ACI_SUCCESS);
            }
        }

        sLink->mPeerOp->mReqComplete(sLink);
    }
    else
    {
        aProtocolContext->mWriteHeader.mA5.mCmSeqNo++;
    }

    return ACI_SUCCESS;

    /*
     * bug-27250 IPC linklist can be crushed.
     * ��� ������ ���Ͽ� pending block�� ������ �����ϵ��� ����.
     * sendfail�� empty�� ���ܵ�.
     */
    ACI_EXCEPTION(SendFail);
    {
    }
    ACI_EXCEPTION_END;
    {
        ACP_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
        {
            sPendingBlock = (cmbBlock *)sIterator->mObj;

            ACE_ASSERT(sLink->mPeerOp->mFreeBlock(sLink, sPendingBlock) == ACI_SUCCESS);
        }

        if (sBlock != NULL)
        {
            ACE_ASSERT(sLink->mPeerOp->mFreeBlock(sLink, sBlock) == ACI_SUCCESS);
        }

        aProtocolContext->mWriteBlock              = NULL;
        aProtocolContext->mWriteHeader.mA5.mCmSeqNo = 0;
    }

    return ACI_FAILURE;
}


/*
 * Protocol�� �о�´�.
 */
static ACI_RC cmiReadProtocolInternal(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      acp_time_t          aTimeout)
{
    cmpMarshalFunction sMarshalFunction;
    acp_uint8_t        sOpID;

    /*
     * Operation ID ����
     */
    CMB_BLOCK_READ_BYTE1(aProtocolContext->mReadBlock, &sOpID);

    /*
     * ���������� ó������ �о�� �ϴ� ��Ȳ
     */
    if (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) == ACP_TRUE)
    {
        /*
         * Operation ID �˻�
         */
        ACI_TEST_RAISE(sOpID >= aProtocolContext->mModule->mOpMaxA5, InvalidOpError);

        /*
         * Protocol �ʱ�ȭ
         * fix BUG-17947.
         */
        ACI_TEST(cmiInitializeProtocol(aProtocol,
                                       aProtocolContext->mModule,
                                       sOpID) != ACI_SUCCESS);
    }
    else
    {
        /*
         * ���������� ���ӵǴ� ��� �������� OpID�� ������ �˻�
         */
        ACI_TEST_RAISE(sOpID != aProtocol->mOpID, InvalidProtocolSequence);
    }

    /*
     * Get Marshal Function
     */
    sMarshalFunction = aProtocolContext->mModule->mReadFunction[sOpID];

    /*
     * Marshal Protocol
     */
    ACI_TEST(sMarshalFunction(aProtocolContext->mReadBlock,
                              aProtocol,
                              &aProtocolContext->mMarshalState) != ACI_SUCCESS);

    /*
     * Protocol Marshal�� �Ϸ���� �ʾ����� ���� Block�� ��� �о�� �� ����
     */
    while (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) != ACP_TRUE)
    {
        /*
         * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
         */
        acpListAppendNode(&aProtocolContext->mReadBlockList,
                            &aProtocolContext->mReadBlock->mListNode);

        aProtocolContext->mReadBlock = NULL;

        /*
         * ���� Block�� �о��
         */
        ACI_TEST(cmiReadBlock(aProtocolContext, aTimeout) != ACI_SUCCESS);

        /*
         * Block���� Operation ID ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            CMB_BLOCK_READ_BYTE1(aProtocolContext->mReadBlock, &sOpID);

            ACI_TEST_RAISE(sOpID != aProtocol->mOpID, InvalidProtocolSequence);

            /*
             * Marshal Protocol
             */
            ACI_TEST(sMarshalFunction(aProtocolContext->mReadBlock,
                                      aProtocol,
                                      &aProtocolContext->mMarshalState) != ACI_SUCCESS);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(InvalidOpError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    ACI_EXCEPTION(InvalidProtocolSequence);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
        
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiInitialize( acp_uint32_t  aCmMaxPendingList )
{
    cmbPool *sPoolLocal;
    cmbPool *sPoolIPC;

    acp_uint32_t sState = 0;

    /*
     * BUG-21080
     */
    acpThrOnce(&gCMInitOnceClient, cmiInitializeOnce);

    ACE_ASSERT(acpThrMutexLock(&gCMInitMutexClient) == ACP_RC_SUCCESS);
    sState = 1;

    ACI_TEST(gCMInitCountClient < 0);

    if (gCMInitCountClient == 0)
    {
        /* BUG-41330 */
        (void)aciRegistErrorFromBuffer(ACI_E_MODULE_CM,
                                       aciVersionID,
                                       CMI_GET_ERROR_COUNT(gCmErrorFactory),
                                       (acp_char_t**)&gCmErrorFactory);

        /*
         * Shared Pool ���� �� ���
         * fix BUG-17864.
         */
        ACI_TEST(cmbPoolAlloc(&sPoolLocal, CMB_POOL_IMPL_LOCAL,CMB_BLOCK_DEFAULT_SIZE,0) != ACI_SUCCESS);
        sState = 2;
        ACI_TEST(cmbPoolSetSharedPool(sPoolLocal, CMB_POOL_IMPL_LOCAL) != ACI_SUCCESS);

        /* fix BUG-18848 */
#if !defined(CM_DISABLE_IPC)

        ACI_TEST(cmbPoolAlloc(&sPoolIPC, CMB_POOL_IMPL_IPC,CMB_BLOCK_DEFAULT_SIZE,0) != ACI_SUCCESS);
        sState = 3;
        ACI_TEST(cmbPoolSetSharedPool(sPoolIPC, CMB_POOL_IMPL_IPC) != ACI_SUCCESS);

        /* IPC Mutex �ʱ�ȭ */
        ACI_TEST(cmbShmInitializeStatic() != ACI_SUCCESS);
#else
        ACP_UNUSED(sPoolIPC);
#endif

#if !defined(CM_DISABLE_IPCDA)
        /* IPC-DA Mutex �ʱ�ȭ */
        ACI_TEST(cmbShmIPCDAInitializeStatic() != ACI_SUCCESS);
#endif
        /* cmmSession �ʱ�ȭ */
        ACI_TEST(cmmSessionInitializeStatic() != ACI_SUCCESS);

        /* cmtVariable Piece Pool �ʱ�ȭ */
        ACI_TEST(cmtVariableInitializeStatic() != ACI_SUCCESS);

        /* cmpModule �ʱ�ȭ */
        ACI_TEST(cmpModuleInitializeStatic() != ACI_SUCCESS);

        /*
         * BUG-19465 : CM_Buffer�� pending list�� ����
         */
        gMaxPendingList = aCmMaxPendingList;

#if !defined(CM_DISABLE_SSL)
        /* Initialize OpenSSL library */
        /* Loading SSL library is not mandatory as long as the user does not try to use it. 
         * Therefore, any error from the function can be ignored at this point. */
        (void)cmnOpensslInitialize(&gOpenssl);
#endif

        /* PROJ-2681 */
        (void)cmnIBInitialize();
    }

    gCMInitCountClient++;

    sState = 0;
    ACE_ASSERT(acpThrMutexUnlock(&gCMInitMutexClient) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    {
        gCMInitCountClient = -1;

        switch( sState )
        {
            case 3:
                cmbPoolFree( sPoolIPC );
            case 2:
                cmbPoolFree( sPoolLocal );
            case 1:
                acpThrMutexUnlock(&gCMInitMutexClient);
            case 0:
            default:
                break;
        }

    }

    return ACI_FAILURE;
}

ACI_RC cmiFinalize()
{
    cmbPool *sPoolLocal = NULL;
    cmbPool *sPoolIPC;

    /*
     * BUG-21080
     */
    ACE_ASSERT(acpThrMutexLock(&gCMInitMutexClient) == ACP_RC_SUCCESS);

    ACI_TEST(gCMInitCountClient < 0);

    gCMInitCountClient--;

    if (gCMInitCountClient == 0)
    {
        /*
        * cmpModule ����
        */
        ACI_TEST(cmpModuleFinalizeStatic() != ACI_SUCCESS);

        /*
        * cmtVariable Piece Pool ����
        */
        ACI_TEST(cmtVariableFinalizeStatic() != ACI_SUCCESS);

        /*
        * cmmSession ����
        */
        ACI_TEST(cmmSessionFinalizeStatic() != ACI_SUCCESS);

        /*
         * fix BUG-18848
         */
#if !defined(CM_DISABLE_IPC)

        /*
         * IPC Mutex ����
         */
        ACI_TEST(cmbShmFinalizeStatic() != ACI_SUCCESS);

        /*
         * Shared Pool ����
         */
        ACI_TEST(cmbPoolGetSharedPool(&sPoolIPC, CMB_POOL_IMPL_IPC) != ACI_SUCCESS);
        ACI_TEST(cmbPoolFree(sPoolIPC) != ACI_SUCCESS);
#else
        ACP_UNUSED(sPoolIPC);
#endif

        ACI_TEST(cmbPoolGetSharedPool(&sPoolLocal, CMB_POOL_IMPL_LOCAL) != ACI_SUCCESS);
        ACI_TEST(cmbPoolFree(sPoolLocal) != ACI_SUCCESS);

#if !defined(CM_DISABLE_SSL)
        (void)cmnOpensslDestroy(&gOpenssl);
#endif

        (void)cmnIBFinalize();
    }

    ACE_ASSERT(acpThrMutexUnlock(&gCMInitMutexClient) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    {
        gCMInitCountClient = -1;

        ACE_ASSERT(acpThrMutexUnlock(&gCMInitMutexClient) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

// BUG-39147
void cmiDestroy()
{
}

ACI_RC cmiSetCallback(acp_uint8_t aModuleID, acp_uint8_t aOpID, cmiCallbackFunction aCallbackFunction)
{
    /*
     * Module ID �˻�
     */
    ACI_TEST_RAISE((aModuleID == CMP_MODULE_BASE) ||
                   (aModuleID >= CMP_MODULE_MAX), InvalidModule);

    /*
     * Operation ID �˻�
     */
    ACI_TEST_RAISE(aOpID >= gCmpModuleClient[aModuleID]->mOpMax, InvalidOperation);

    /*
     * Callback Function ����
     */
    if (aCallbackFunction == NULL)
    {
        gCmpModuleClient[aModuleID]->mCallbackFunction[aOpID] = cmpCallbackNULL;
    }
    else
    {
        gCmpModuleClient[aModuleID]->mCallbackFunction[aOpID] = aCallbackFunction;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(InvalidModule);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_MODULE));
    }
    ACI_EXCEPTION(InvalidOperation);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_bool_t cmiIsSupportedLinkImpl(cmiLinkImpl aLinkImpl)
{
    return cmnLinkIsSupportedImpl(aLinkImpl);
}

ACI_RC cmiAllocLink(cmiLink **aLink, cmiLinkType aType, cmiLinkImpl aImpl)
{
    /*
     * Link �Ҵ�
     */
    ACI_TEST(cmnLinkAlloc(aLink, aType, aImpl) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiFreeLink(cmiLink *aLink)
{
    /*
     * Link ����
     */
    ACI_TEST(cmnLinkFree(aLink) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiCloseLink(cmiLink *aLink)
{
    /*
     * Link Close
     */
    ACI_TEST(aLink->mOp->mClose(aLink) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiWaitLink(cmiLink *aLink, acp_time_t  aTimeout)
{
    return cmnDispatcherWaitLink(aLink, CMI_DIRECTION_RD, aTimeout);
}

ACI_RC cmiListenLink(cmiLink *aLink, cmiListenArg *aListenArg)
{
    cmnLinkListen *sLink = (cmnLinkListen *)aLink;

    /*
     * Listen Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_LISTEN);

    /*
     * listen
     */
    ACI_TEST(sLink->mListenOp->mListen(sLink, aListenArg) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiAcceptLink(cmiLink *aLinkListen, cmiLink **aLinkPeer)
{
    cmnLinkListen *sLinkListen = (cmnLinkListen *)aLinkListen;
    cmnLinkPeer   *sLinkPeer   = NULL;

    /*
     * Listen Type �˻�
     */
    ACE_ASSERT(aLinkListen->mType == CMN_LINK_TYPE_LISTEN);

    /*
     * accept
     */
    ACI_TEST(sLinkListen->mListenOp->mAccept(sLinkListen, &sLinkPeer) != ACI_SUCCESS);

    /*
     * accept�� Link ��ȯ
     */
    *aLinkPeer = (cmiLink *)sLinkPeer;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiGetLinkInfo(cmiLink *aLink, acp_char_t *aBuf, acp_uint32_t aBufLen, cmiLinkInfoKey aKey)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Get Info
     */
    return sLink->mPeerOp->mGetInfo(sLink, aBuf, aBufLen, aKey);
}

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
ACI_RC cmiGetLinkSndBufSize(cmiLink *aLink, acp_sint32_t *aSndBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mGetSndBufSize != NULL)
    {
        ACI_TEST(sLink->mPeerOp->mGetSndBufSize(sLink, aSndBufSize) != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSetLinkSndBufSize(cmiLink *aLink, acp_sint32_t aSndBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mSetSndBufSize != NULL)
    {
        ACI_TEST(sLink->mPeerOp->mSetSndBufSize(sLink, aSndBufSize) != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiGetLinkRcvBufSize(cmiLink *aLink, acp_sint32_t *aRcvBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mGetRcvBufSize != NULL)
    {
        ACI_TEST(sLink->mPeerOp->mGetRcvBufSize(sLink, aRcvBufSize) != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSetLinkRcvBufSize(cmiLink *aLink, acp_sint32_t aRcvBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mSetRcvBufSize != NULL)
    {
        ACI_TEST(sLink->mPeerOp->mSetRcvBufSize(sLink, aRcvBufSize) != ACI_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiCheckLink(cmiLink *aLink, acp_bool_t *aIsClosed)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Connection Closed �˻�
     */
    return sLink->mPeerOp->mCheck(sLink, aIsClosed);
}

ACI_RC cmiShutdownLink(cmiLink *aLink, cmiDirection aDirection)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    ACE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * bug-28277 ipc: server stop failed when idle clis exist
     * server stop�ÿ��� shutdown_mode_force �ѱ⵵�� ��.
     */
    ACI_TEST(sLink->mPeerOp->mShutdown(sLink, aDirection,
                                       CMN_SHUTDOWN_MODE_NORMAL)
             != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiAddSession(cmiSession         *aSession,
                     void               *aOwner,
                     acp_uint8_t         aModuleID,
                     cmiProtocolContext *aProtocolContext)
{
    ACP_UNUSED(aProtocolContext);
    /*
     * �Ķ���� ���� �˻�
     */
    ACE_ASSERT(aModuleID > CMP_MODULE_BASE);

    ACI_TEST_RAISE(aModuleID >= CMP_MODULE_MAX, UnknownModule);

    /*
     * Session �߰�
     */
    ACI_TEST(cmmSessionAdd(aSession) != ACI_SUCCESS);

    /*
     * Session �ʱ�ȭ
     */
    aSession->mOwner           = aOwner;
    aSession->mBaseVersion     = CMP_VER_BASE_NONE;

    aSession->mLink           = NULL;
    aSession->mCounterpartID  = 0;
    aSession->mModuleID       = aModuleID;
    aSession->mModuleVersion  = CMP_VER_BASE_NONE;

    return ACI_SUCCESS;

    ACI_EXCEPTION(UnknownModule);
    {
         ACI_SET(aciSetErrorCode(cmERR_ABORT_UNKNOWN_MODULE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiRemoveSession(cmiSession *aSession)
{
    /*
     * Session�� Session ID�� 0�̸� ��ϵ��� ���� Session
     */
    ACI_TEST_RAISE(aSession->mSessionID == 0, SessionNotAdded);

    /*
     * Session ����
     */
    ACI_TEST(cmmSessionRemove(aSession) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(SessionNotAdded);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SESSION_NOT_ADDED));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSetLinkForSession(cmiSession *aSession, cmiLink *aLink)
{
    if (aLink != NULL)
    {
        /*
         * Session�� Link�� �̹� ��ϵ� ���¿��� ���ο� Link�� ������ �� ����
         */
        ACI_TEST_RAISE(aSession->mLink != NULL, LinkAlreadyRegistered);

        /*
         * Link�� Peer Type���� �˻�
         */
        /*
         * BUG-28119 for RP PBT
         */
        ACI_TEST_RAISE((aLink->mType != CMN_LINK_TYPE_PEER_CLIENT) &&
                       (aLink->mType != CMN_LINK_TYPE_PEER_SERVER), InvalidLinkType);
    }

    /*
     * Session�� Link ����
     */
    aSession->mLink = (cmnLinkPeer *)aLink;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LinkAlreadyRegistered);
    {
         ACI_SET(aciSetErrorCode(cmERR_ABORT_LINK_ALREADY_REGISTERED));
    }
    ACI_EXCEPTION(InvalidLinkType);
    {
         ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_LINK_TYPE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiGetLinkForSession(cmiSession *aSession, cmiLink **aLink)
{
    /*
     * Session�� Link ��ȯ
     */
    *aLink = (cmiLink *)aSession->mLink;

    return ACI_SUCCESS;
}

/**************************************************************
 * PROJ-2616
 * cmiIPCDAInitReadHandShakeResult
 *
 * - Callback Result Process for IPCDA-Handshake.
 *
 * aCtx         [in]      - cmiProtocolContext
 * aOrReadBlock [in]      - Real-Shared Memory which data is written.
 * aTmpBlock    [in]      - Temporary cmBlockInfo
 * aReadDataCount[in/out] - ���� �������� ��ġ
 **************************************************************/
acp_bool_t cmiIPCDAInitReadHandShakeResult(cmiProtocolContext  *aCtx,
                                           cmbBlockIPCDA      **aOrReadBlock,
                                           cmbBlock            *aTmpBlock,
                                           acp_uint32_t        *aReadDataCount)
{
    cmbBlockIPCDA    *sOrBlock          = NULL;
    cmnLinkPeerIPCDA *sCmnLinkPeerIPCDA = (cmnLinkPeerIPCDA*)aCtx->mLink;
    acp_char_t        sMessageQBuf[1];
    struct timespec   sMessageQWaitTime;

    *aReadDataCount = 0;

    sOrBlock = (cmbBlockIPCDA*)aCtx->mReadBlock;
    /* BUG-46390 */
    *aOrReadBlock = sOrBlock;
    aTmpBlock->mCursor      = CMP_HEADER_SIZE;
    aTmpBlock->mData        = &sOrBlock->mData;
    aTmpBlock->mBlockSize   = sOrBlock->mBlock.mBlockSize;
    aTmpBlock->mIsEncrypted = sOrBlock->mBlock.mIsEncrypted;
    aCtx->mReadBlock        = aTmpBlock;

    /* PROJ-2616 ipc-da message queue */
#if defined(ALTI_CFG_OS_LINUX)
    if( sCmnLinkPeerIPCDA->mMessageQ.mUseMessageQ == ACP_TRUE )
    {
        clock_gettime(CLOCK_REALTIME, &sMessageQWaitTime);
        sMessageQWaitTime.tv_sec += sCmnLinkPeerIPCDA->mMessageQ.mMessageQTimeout;

        acp_sint32_t mq_timeResult =  mq_timedreceive(sCmnLinkPeerIPCDA->mMessageQ.mMessageQID,
                                                      sMessageQBuf,
                                                      1,
                                                      NULL,
                                                      &sMessageQWaitTime);

        ACI_TEST_RAISE((mq_timeResult < 0), HandShakeErrorTimeOut);
    }
#endif

    ACI_TEST(cmiIPCDACheckReadFlag(aCtx, sOrBlock, 0, 0) == ACI_FAILURE);

    ACI_TEST(cmiIPCDACheckDataCount((void*)aCtx,
                                    &sOrBlock->mOperationCount,
                                    *aReadDataCount,
                                    0,
                                    0) != ACI_SUCCESS);

    (*aReadDataCount)++;

    return ACP_TRUE;

    ACI_EXCEPTION(HandShakeErrorTimeOut)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_TIMED_OUT));
    }
    ACI_EXCEPTION_END;

    return ACP_FALSE;
}

acp_bool_t cmiIPCDAHandShakeResultCallback(cmiProtocolContext *aCtx,
                                           cmbBlockIPCDA      *aOrReadBlock,
                                           acp_uint32_t       *aReadDataCount)
{
    ACI_TEST(cmiIPCDACheckDataCount((void*)aCtx,
                                    &aOrReadBlock->mOperationCount,
                                    *aReadDataCount,
                                    0,
                                    0) == ACI_FAILURE);

    (*aReadDataCount)++; /* BUG-46390 ������ ���� */
    CMI_SKIP_READ_BLOCK(aCtx, 1);    /* IPCDALastOpEnded skip */
    
    aCtx->mReadBlock = (cmbBlock*)aOrReadBlock;
    cmiLinkPeerFinalizeCliReadForIPCDA(aCtx);

    return ACP_TRUE;

    ACI_EXCEPTION_END;

    /* BUG-46390 */
    aCtx->mReadBlock = (cmbBlock*)aOrReadBlock;
    cmiLinkPeerFinalizeCliReadForIPCDA(aCtx);
    
    return ACP_FALSE;
}

//===========================================================
// proj_2160 cm_type removal
// �Լ��� 2���� ���� ����:
// cmiConnect            : DB �������ݿ�
// cmiConnectWithoutData : RP �������ݿ� (DB_Handshake ���� ����)
// RP������ DB_Handshake�� ó���ϱⰡ ��ư�,
// (BASE ���������� �������鼭 opcode ���� DB �������� ��ȿ����),
// �� ���ص� ������ ���ٰ� �����Ǿ� ���� �ʵ��� �Ѵ�
// ���� �Լ��� ����ϴ� ����  if-else ó�� ���� ���ٰ� �����Ͽ���
//===========================================================
ACI_RC cmiConnect(cmiProtocolContext *aCtx, cmiConnectArg *aConnectArg, acp_time_t aTimeout, acp_sint32_t aOption)
{
    acp_bool_t                 sConnectFlag      = ACP_FALSE;

    acp_uint8_t                sModuleID;      /* 1: DB, 2: RP */
    acp_uint8_t                sMajorVersion;  /* CM major ver of client */
    acp_uint8_t                sMinorVersion;  /* CM minor ver of client */
    acp_uint8_t                sPatchVersion;  /* CM patch ver of client */
    acp_uint8_t                sLastOpID;      /* PROJ-2733-Protocol */

    acp_uint8_t                sOpID;
    acp_uint32_t               sErrIndex;
    acp_uint32_t               sErrCode;
    acp_uint16_t               sErrMsgLen;
    acp_char_t                 sErrMsg[ACI_MAX_ERROR_MSG_LEN]; // 2048
    cmbBlockIPCDA             *sOrBlock = NULL;
    cmbBlock                   sTmpBlock;
    acp_uint32_t               sCurReadOperationCount = 0;
    
    ACP_UNUSED( sModuleID );
    ACP_UNUSED( sMajorVersion );
    ACP_UNUSED( sMinorVersion );
    ACP_UNUSED( sPatchVersion );

    ACI_TEST_RAISE(aCtx->mModule->mModuleID != CMP_MODULE_DB,
                   InvalidModuleError);

    /* Link Connect */
    ACI_TEST(aCtx->mLink->mPeerOp->mConnect(aCtx->mLink,
                                            aConnectArg,
                                            aTimeout,
                                            aOption) != ACI_SUCCESS);
    sConnectFlag = ACP_TRUE;
    // STF�� ��� ������ ���⼭ �ٽ� �ʱ�ȭ���������
    aCtx->mWriteHeader.mA7.mCmSeqNo = 0; // send seq
    aCtx->mCmSeqNo = 0;                  // recv seq

    ACI_TEST( aCtx->mLink->mPeerOp->mHandshake(aCtx->mLink) != ACI_SUCCESS);

    aCtx->mIsDisconnect = ACP_FALSE;
    
    /*PROJ-2616*/
    if (cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA)
    {
        cmiInitIPCDABuffer(aCtx);
    }

    /*OpCode + ModuleID + Major_Version + Minor_Version + Patch_Version + dummy*/
    CMI_WRITE_CHECK(aCtx, 6); 

    CMI_WR1(aCtx, CMP_OP_DB_Handshake);
    CMI_WR1(aCtx, aCtx->mModule->mModuleID); // DB: 1, RP: 2
    CMI_WR1(aCtx, CM_MAJOR_VERSION);
    CMI_WR1(aCtx, CM_MINOR_VERSION);
    CMI_WR1(aCtx, CM_PATCH_VERSION);
    CMI_WR1(aCtx, CMP_OP_DB_MAX - 1);  /* PROJ-2733-Protocol */

    if (cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA)
    {
        cmiIPCDAIncDataCount(aCtx);
        /* Finalize to write data block. */
        (void)cmiFinalizeSendBufferForIPCDA((void*)aCtx);
    }
    else
    {
        ACI_TEST( cmiSend(aCtx, ACP_TRUE) != ACI_SUCCESS);
    }

    //fix BUG-17942 
    // cmiRecvNext() ��ſ� cmiRecv()�� ȣ���Ѵ�
    // DB_HandshakeResult�� ���� callback�� �������� ����
    //fix BUG-38128 (ACI_TEST_RAISE -> ACI_TEST)
    if (cmiGetLinkImpl(aCtx) != CMN_LINK_IMPL_IPCDA)
    {
        ACI_TEST(cmiRecvNext(aCtx, aTimeout) != ACI_SUCCESS);
    }
    else
    {
        ACI_TEST(cmiIPCDAInitReadHandShakeResult(aCtx,
                                                 &sOrBlock,
                                                 &sTmpBlock,
                                                 &sCurReadOperationCount) != ACP_TRUE);
    }

    CMI_RD1(aCtx, sOpID);

    /* PROJ-2733-Protocol */
    switch (sOpID)
    {
        case CMP_OP_DB_HandshakeResult:
            CMI_RD1(aCtx, sModuleID);
            CMI_RD1(aCtx, sMajorVersion);
            CMI_RD1(aCtx, sMinorVersion);
            CMI_RD1(aCtx, sPatchVersion);
            CMI_RD1(aCtx, sLastOpID);

            if (sLastOpID != 0)
            {
                aCtx->mProtocol.mServerLastOpID = sLastOpID;
            }
            break;

        case CMP_OP_DB_ErrorV3Result:
        case CMP_OP_DB_ErrorResult:
            ACI_RAISE(HandshakeError);
            break;

        default:
            ACI_RAISE(InvalidProtocolSeqError);
            break;
    }

    if (cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA)
    {
        ACI_TEST(cmiIPCDAHandShakeResultCallback(aCtx,
                                                 sOrBlock, 
                                                 &sCurReadOperationCount) != ACP_TRUE);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(HandshakeError);
    {
        CMI_SKIP_READ_BLOCK(aCtx, 1); /* skip error op ID */
        /* BUG-44556  Handshake �����߿� �߻��� ������ �������� �ؼ��� �߸��Ǿ����ϴ�.*/
        CMI_RD4(aCtx, &sErrIndex);
        CMI_RD4(aCtx, &sErrCode);
        CMI_RD2(aCtx, &sErrMsgLen);
        if (sErrMsgLen >= ACI_MAX_ERROR_MSG_LEN)
        {
            sErrMsgLen = ACI_MAX_ERROR_MSG_LEN - 1;
        }
        CMI_RCP(aCtx, sErrMsg, sErrMsgLen);
        sErrMsg[sErrMsgLen] = '\0';
        ACI_SET(aciSetErrorCodeAndMsg(sErrCode, sErrMsg));

        /* PROJ-2733-Protocol */
        switch (sOpID)
        {
            case CMP_OP_DB_ErrorV3Result:
                CMI_SKIP_READ_BLOCK(aCtx, 8);
                break;

            case CMP_OP_DB_ErrorResult:
            default:
                break;
        }
    }
    ACI_EXCEPTION(InvalidModuleError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_MODULE));
    }
    ACI_EXCEPTION(InvalidProtocolSeqError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    ACI_EXCEPTION_END;
    {
        /*
         * BUG-24170 [CM] cmiConnect ���� ��, cmiConnect ������ close �ؾ� �մϴ�
         */
        if(sConnectFlag == ACP_TRUE)
        {
            (void)aCtx->mLink->mPeerOp->mShutdown(aCtx->mLink,
                                                  CMI_DIRECTION_RDWR,
                                                  CMN_SHUTDOWN_MODE_NORMAL);

            (void)aCtx->mLink->mLink.mOp->mClose(&aCtx->mLink->mLink);
        }
    }

    /* PROJ-2616 */
    aCtx->mIsDisconnect = ACP_TRUE;
    if (sOrBlock != NULL)
    {
        aCtx->mReadBlock = (cmbBlock*)sOrBlock;
    }

    return ACI_FAILURE;
}

// RP �������ݿ� (DB_Handshake ���� ����)
ACI_RC cmiConnectWithoutData( cmiProtocolContext * aCtx,
                              cmiConnectArg * aConnectArg,
                              acp_time_t aTimeout,
                              acp_sint32_t aOption )
{
    acp_bool_t sConnectFlag = ACP_FALSE;

    /* Link Connect */
    ACI_TEST(aCtx->mLink->mPeerOp->mConnect(aCtx->mLink,
                                            aConnectArg,
                                            aTimeout,
                                            aOption) != ACI_SUCCESS);
    sConnectFlag = ACP_TRUE;

    ACI_TEST(aCtx->mLink->mPeerOp->mHandshake(aCtx->mLink));

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    /*
     * BUG-24170 [CM] cmiConnect ���� ��, cmiConnect ������ close �ؾ� �մϴ�
     */
    if(sConnectFlag == ACP_TRUE)
    {
        (void)aCtx->mLink->mPeerOp->mShutdown(aCtx->mLink,
                                              CMI_DIRECTION_RDWR,
                                              CMN_SHUTDOWN_MODE_NORMAL);

        (void)aCtx->mLink->mLink.mOp->mClose(&aCtx->mLink->mLink);
    }

    return ACI_FAILURE;
}

ACI_RC cmiInitializeProtocol(cmiProtocol *aProtocol, cmpModule*  aModule, acp_uint8_t aOperationID)
{
    /*
     * fix BUG-17947.
     */
    /*
     * Operation ID ����
     */
    aProtocol->mOpID = aOperationID;
    aProtocol->mServerLastOpID = 0;  /* PROJ-2733-Protocol Unused at A5 */

    /*
     * Protocol Finalize �Լ� ����
     */
    aProtocol->mFinalizeFunction = (void *)aModule->mArgFinalizeFunction[aOperationID];

    /*
     * Protocol �ʱ�ȭ
     */
    if (aModule->mArgInitializeFunction[aOperationID] != cmpArgNULL)
    {
        ACI_TEST_RAISE((aModule->mArgInitializeFunction[aOperationID])(aProtocol) != ACI_SUCCESS,
                       InitializeFail);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(InitializeFail);
    {
        aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*fix BUG-30041 cmiReadProtocol���� mFinalization ���ʱ�ȭ �Ǳ�����
 �����ϴ� case�� cmiFinalization���� ����������˴ϴ�.*/
void  cmiInitializeProtocolNullFinalization(cmiProtocol *aProtocol)
{
    aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
}

ACI_RC cmiFinalizeProtocol(cmiProtocol *aProtocol)
{
    if (aProtocol->mFinalizeFunction != (void *)cmpArgNULL)
    {
        ACI_TEST(((cmpArgFunction)aProtocol->mFinalizeFunction)(aProtocol) != ACI_SUCCESS);

        aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void cmiSetProtocolContextLink(cmiProtocolContext *aProtocolContext, cmiLink *aLink)
{
    /*
     * Protocol Context�� Link ����
     */
    aProtocolContext->mLink = (cmnLinkPeer *)aLink;
}

ACI_RC cmiReadProtocolAndCallback(cmiProtocolContext      *aProtocolContext,
                                  void                    *aUserContext,
                                  acp_time_t               aTimeout)
{
    cmpCallbackFunction  sCallbackFunction;
    ACI_RC               sRet = ACI_SUCCESS;
    cmnLinkPeer          *sLink;

    /*
     * �о�� Block�� �ϳ��� ������ �о��
     */
    if (aProtocolContext->mReadBlock == NULL)
    {
        ACI_TEST(cmiReadBlock(aProtocolContext, aTimeout) != ACI_SUCCESS);
    }

    while (1)
    {
        /*
         * Protocol ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            ACI_TEST(cmiReadProtocolInternal(aProtocolContext,
                                             &aProtocolContext->mProtocol,
                                             aTimeout) != ACI_SUCCESS);

            /*
             * Callback Function ȹ��
             */
            sCallbackFunction = aProtocolContext->mModule->mCallbackFunction[aProtocolContext->mProtocol.mOpID];

            /*
             * Callback ȣ��
             */
            sRet = sCallbackFunction(aProtocolContext,
                                     &aProtocolContext->mProtocol,
                                     aProtocolContext->mOwner,
                                     aUserContext);

            /*
             * Protocol Finalize
             */
            ACI_TEST(cmiFinalizeProtocol(&aProtocolContext->mProtocol) != ACI_SUCCESS);

            /*
             * Free Block List�� �޸� Block ����
             */
            ACI_TEST(cmiFreeReadBlock(aProtocolContext) != ACI_SUCCESS);

            /*
             * Callback ��� Ȯ��
             */
            if (sRet != ACI_SUCCESS)
            {
                break;
            }

            if (aProtocolContext->mIsAddReadBlock == ACP_TRUE)
            {
                ACI_RAISE(ReadBlockEnd);
            }
        }
        else
        {
            if (aProtocolContext->mIsAddReadBlock == ACP_FALSE)
            {
                /*
                 * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
                 */
                acpListAppendNode(&aProtocolContext->mReadBlockList,
                                  &aProtocolContext->mReadBlock->mListNode);
            }

            ACI_EXCEPTION_CONT(ReadBlockEnd);
            {
                aProtocolContext->mReadBlock = NULL;
                aProtocolContext->mIsAddReadBlock = ACP_FALSE;

                if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader) == ACP_TRUE)
                {
                    ACI_TEST(cmiFreeReadBlock(aProtocolContext) != ACI_SUCCESS);
                    /*
                     * Protocol Sequence �Ϸ�
                     */

                    sLink = aProtocolContext->mLink;
                    sLink->mPeerOp->mResComplete(sLink);
                    break;
                }
                else
                {
                    /*
                     * ���� Block�� �о��
                     */
                    ACI_TEST(cmiReadBlock(aProtocolContext, aTimeout) != ACI_SUCCESS);
                }

            }
        }
    }

    return sRet;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiReadProtocol(cmiProtocolContext *aProtocolContext,
                       cmiProtocol        *aProtocol,
                       acp_time_t          aTimeout,
                       acp_bool_t         *aIsEnd)
{
    cmpCallbackFunction sCallbackFunction;
    ACI_RC              sRet;
    cmnLinkPeer         *sLink;

    /*
     * ���� Read Protocol�� ���������� �Ϸ�Ǿ��� ���
     */
    if (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) == ACP_TRUE)
    {
        /*
         * Read Block ��ȯ
         */
        ACI_TEST(cmiFreeReadBlock(aProtocolContext) != ACI_SUCCESS);

        sLink = aProtocolContext->mLink;
        sLink->mPeerOp->mResComplete(sLink);

        /*
         * Protocol Finalize �Լ� �ʱ�ȭ
         *
         * cmiReadProtocol �Լ��� cmiFinalizeProtocolȣ���� å���� �������̾ ����
         */
        aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
    }

    /*
     * �о�� Block�� �ϳ��� ������ �о��
     */
    if (aProtocolContext->mReadBlock == NULL)
    {
        ACI_TEST(cmiReadBlock(aProtocolContext, aTimeout) != ACI_SUCCESS);
    }

    while (1)
    {
        /*
         * Protocol ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            ACI_TEST(cmiReadProtocolInternal(aProtocolContext,
                                             aProtocol,
                                             aTimeout) != ACI_SUCCESS);

            /*
             * BASE Module�̸� Callback
             */
            if (aProtocolContext->mReadHeader.mA5.mModuleID == CMP_MODULE_BASE)
            {
                /*
                 * Callback Function ȹ��
                 */
                sCallbackFunction = aProtocolContext->mModule->mCallbackFunction[aProtocol->mOpID];

                /*
                 * Callback
                 */
                sRet = sCallbackFunction(aProtocolContext,
                                         aProtocol,
                                         aProtocolContext->mOwner,
                                         NULL);

                /*
                 * Protocol Finalize
                 */
                ACI_TEST(cmiFinalizeProtocol(aProtocol) != ACI_SUCCESS);

                ACI_TEST(sRet != ACI_SUCCESS);

                /*
                 * BUG-18846
                 */
                if (aProtocolContext->mIsAddReadBlock == ACP_TRUE)
                {
                    ACI_RAISE(ReadBlockEnd);
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            /*
             * BUG-18846
             */
            if (aProtocolContext->mIsAddReadBlock == ACP_FALSE)
            {
                acpListAppendNode(&aProtocolContext->mReadBlockList,
                                  &aProtocolContext->mReadBlock->mListNode);
            }

            ACI_EXCEPTION_CONT(ReadBlockEnd);
            {
                aProtocolContext->mReadBlock      = NULL;
                aProtocolContext->mIsAddReadBlock = ACP_FALSE;
            }

            ACI_TEST(cmiReadBlock(aProtocolContext, aTimeout) != ACI_SUCCESS);
        }
    }

    /*
     * ���� Read Block�� ������ �о�����
     */
    if (!CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
    {
        /*
         * BUG-18846
         */
        if (aProtocolContext->mIsAddReadBlock == ACP_FALSE)
        {
            acpListAppendNode(&aProtocolContext->mReadBlockList,
                              &aProtocolContext->mReadBlock->mListNode);

        }

        aProtocolContext->mReadBlock      = NULL;
        aProtocolContext->mIsAddReadBlock = ACP_FALSE;

        if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader))
        {
            *aIsEnd = ACP_TRUE;
        }
    }
    else
    {
        *aIsEnd = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC cmiWriteProtocol(cmiProtocolContext *aProtocolContext, cmiProtocol *aProtocol)
{
    cmpMarshalState     sMarshalState;
    cmpMarshalFunction  sMarshalFunction;

    /*
     * Write Block�� �Ҵ�Ǿ����� ������ �Ҵ�
     */
    if (aProtocolContext->mWriteBlock == NULL)
    {
        ACI_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                               &aProtocolContext->mWriteBlock)
                 != ACI_SUCCESS);
    }

    /*
     * Marshal State �ʱ�ȭ
     */
    CMP_MARSHAL_STATE_INITIALIZE(sMarshalState);

    /*
     * Module ȹ��
     */
    if (aProtocolContext->mModule == NULL)
    {
        ACI_TEST(cmiGetModule(&aProtocolContext->mWriteHeader,
                              &aProtocolContext->mModule) != ACI_SUCCESS);
    }

    /*
     * Operation ID �˻�
     */
    ACI_TEST_RAISE(aProtocol->mOpID >= aProtocolContext->mModule->mOpMaxA5, InvalidOpError);

    /*
     * Marshal Function ȹ��
     */
    sMarshalFunction = aProtocolContext->mModule->mWriteFunction[aProtocol->mOpID];

    /*
     * Marshal Loop
     */
    while (1)
    {
        /*
         * Operation ID�� ����ϰ� Marshal
         */
        if (CMI_CHECK_BLOCK_FOR_WRITE(aProtocolContext->mWriteBlock))
        {
            CMB_BLOCK_WRITE_BYTE1(aProtocolContext->mWriteBlock, aProtocol->mOpID);

            ACI_TEST(sMarshalFunction(aProtocolContext->mWriteBlock,
                                      aProtocol,
                                      &sMarshalState) != ACI_SUCCESS);

            /*
             * �������� ���Ⱑ �Ϸ�Ǿ����� Loop ����
             */
            if (CMP_MARSHAL_STATE_IS_COMPLETE(sMarshalState) == ACP_TRUE)
            {
                break;
            }
        }

        /*
         * ����
         */
        ACI_TEST(cmiWriteBlock(aProtocolContext, ACP_FALSE) != ACI_SUCCESS);

        /*
         * ���ο� Block �Ҵ�
         */
        ACI_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                               &aProtocolContext->mWriteBlock)
                 != ACI_SUCCESS);
    }

    /*
     * Protocol Finalize
     */
    ACI_TEST(cmiFinalizeProtocol(aProtocol) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(InvalidOpError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    ACI_EXCEPTION_END;
    {
        ACE_ASSERT(cmiFinalizeProtocol(aProtocol) == ACI_SUCCESS);
    }

    return ACI_FAILURE;
}

ACI_RC cmiFlushProtocol(cmiProtocolContext *aProtocolContext, acp_bool_t aIsEnd)
{
    if (aProtocolContext->mWriteBlock != NULL)
    {
        /*
         * Write Block�� �Ҵ�Ǿ� ������ ����
         */
        ACI_TEST(cmiWriteBlock(aProtocolContext, aIsEnd) != ACI_SUCCESS);
    }
    else
    {
        if ((aIsEnd == ACP_TRUE) &&
            (aProtocolContext->mWriteHeader.mA5.mCmSeqNo != 0) &&
            (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mWriteHeader) == ACP_FALSE))
        {
            /*
             * Sequence End�� ���۵��� �ʾ����� �� Write Block�� �Ҵ��Ͽ� ����
             */
            ACI_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                                   &aProtocolContext->mWriteBlock)
                     != ACI_SUCCESS);

            ACI_TEST(cmiWriteBlock(aProtocolContext, ACP_TRUE) != ACI_SUCCESS);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_bool_t cmiCheckInVariable(cmiProtocolContext *aProtocolContext, acp_uint32_t aInVariableSize)
{
    acp_uint32_t sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        /*
         * mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
         * ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
         * ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
         * cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
         */
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    /*
     * TYPE(1) + SIZE(4) + DATA(x) + DELIMETER(1)
     */
    return ((sCurSize + 5 + aInVariableSize + 1 + ACI_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ACP_TRUE : ACP_FALSE;
}

acp_bool_t cmiCheckInBinary(cmiProtocolContext *aProtocolContext, acp_uint32_t aInBinarySize)
{
    acp_uint32_t sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    /*
     * TYPE(1) + SIZE(4) + DATA(x) + DELIMETER(1)
     */
    return ((sCurSize + 5 + aInBinarySize + 1 + ACI_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ACP_TRUE : ACP_FALSE;
}

acp_bool_t cmiCheckInBit(cmiProtocolContext *aProtocolContext, acp_uint32_t aInBitSize)
{
    acp_uint32_t sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        /*
         * mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
         * ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
         * ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
         * cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
         */
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    /*
     * TYPE(1) + PRECISION(4) + SIZE(4) + DATA(x)
     */
    return ((sCurSize + 9 + aInBitSize + ACI_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ACP_TRUE : ACP_FALSE;
}

acp_bool_t cmiCheckInNibble(cmiProtocolContext *aProtocolContext, acp_uint32_t aInNibbleSize)
{
    acp_uint32_t sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        /*
         * mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
         * ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
         * ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
         * cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
         */
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    /*
     * TYPE(1) + PRECISION(4) + SIZE(4) + DATA(x)
     */
    return ((sCurSize + 9 + aInNibbleSize + ACI_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ACP_TRUE : ACP_FALSE;
}

acp_uint8_t *cmiGetOpName( acp_uint32_t aModuleID, acp_uint32_t aOpID )
{
    acp_uint8_t *sOpName = NULL;

    ACE_DASSERT( aModuleID < CMP_MODULE_MAX );

    switch( aModuleID )
    {
        case CMP_MODULE_BASE:
            ACE_DASSERT( aOpID < CMP_OP_BASE_MAX );
            sOpName = (acp_uint8_t*)gCmpOpBaseMapClient[aOpID].mCmpOpName;
            break;

        case CMP_MODULE_DB:
            ACE_DASSERT( aOpID < CMP_OP_DB_MAX );
            sOpName = (acp_uint8_t*)gCmpOpDBMapClient[aOpID].mCmpOpName;
            break;

        case CMP_MODULE_RP:
            ACE_DASSERT( aOpID < CMP_OP_RP_MAX );
            sOpName = (acp_uint8_t*)gCmpOpRPMapClient[aOpID].mCmpOpName;
            break;

        default:
            ACE_ASSERT(0);
            break;
    }

    return sOpName;
}

cmiLinkImpl cmiGetLinkImpl(cmiProtocolContext *aProtocolContext)
{
    return (cmiLinkImpl)(aProtocolContext->mLink->mLink.mImpl);
}

/***********************************************************
 * proj_2160 cm_type removal
 * cmbBlock ������ 2���� NULL�� �����
 * cmiAllocCmBlock�� ȣ���ϱ� ���� �� �Լ��� �ݵ�� ����
 * ȣ���ؼ� cmbBlock �Ҵ��� ����� �ǵ��� �ؾ� �Ѵ�.
***********************************************************/
ACI_RC cmiMakeCmBlockNull(cmiProtocolContext *aCtx)
{
    aCtx->mLink = NULL;
    aCtx->mReadBlock = NULL;
    aCtx->mWriteBlock = NULL;
    return ACI_SUCCESS;
}


/***********************************************************
 * proj_2160 cm_type removal
 **********************************************************
 *  cmiAllocCmBlock:
 * 1. �� �Լ��� ������ cmiAddSession() ��
 *  cmiInitializeProtocolContext() �� ��ü�ϴ� �Լ��̴�
 * 2. �� �Լ��� cmiProtocolContext�� �ʱ�ȭ�ϰ�
 *  2���� cmbBlock(recv, send)�� �Ҵ��Ѵ�.
 * 3. ������ ���� ȣ���ϱ� ���� cmbBlock �����Ͱ� NULL��
 *  �ʱ�ȭ�Ǿ� �־�� �Ѵٴ� ���̴�(cmiMakeCmBlockNull)
 * 4. A5 client�� �����ϴ� ��쿡�� ���������� �� �Լ��� 
 *  ����ϴµ� ������ ����(A7���� A5�� ��ȯ��)
 **********************************************************
 *  �������:
 * 1. �������� �ۼ��Ÿ��� cmbBlock�� �Ҵ�/������ �Ǿ��µ�,
 *  A7���ʹ� �ѹ��� �Ҵ��� �� ������ ���涧����
 *  ��� �����ǵ��� ����Ǿ���.
 * 2. cmiProtocolContext�� �ѹ��� �ʱ�ȭ�� �ؾ��Ѵ�. ������
 *  ��Ŷ�Ϸù�ȣ�� ���ǳ����� ��� �����ؾ� �ϱ� �����̴�
 * 3. cmmSession ����ü�� ���̻� ������� �ʴ´�
 **********************************************************
 *  �����(�Լ� ȣ�� ����):
 *  1. cmiMakeCmBlockNull(ctx);   : cmbBlock ������ NULL ����
 *  2. cmiAllocLink(&link);       : Link ����ü �Ҵ�
 *  3. cmiAllocCmBlock(ctx, link);: cmbBlock 2�� �Ҵ�
 *  4.  connected   ...           : ���� ����
 *  5.  send/recv   ...           : cmbBlock�� ���� �ۼ���
 *  6.  disconnected ..           : ���� ����
 *  7. cmiFreeCmBlock(ctx);       : cmbBlock 2�� ����
 *  8. cmiFreeLink(link);         : Link ����ü ����
***********************************************************/
ACI_RC cmiAllocCmBlock(cmiProtocolContext* aCtx,
                       acp_uint8_t         aModuleID,
                       cmiLink*            aLink,
                       void*               aOwner)
{
    cmbPool*   sPool;

    // cmnLink has to be prepared
    ACI_TEST(aLink == NULL);
    // memory allocation allowed only once
    ACI_TEST(aCtx->mReadBlock != NULL);

    aCtx->mModule  = gCmpModuleClient[aModuleID];
    aCtx->mLink    = (cmnLinkPeer*)aLink;
    aCtx->mOwner   = aOwner;
    aCtx->mIsAddReadBlock = ACP_FALSE;
    aCtx->mSession = NULL; // deprecated

    /* PROJ-2616 */
    aCtx->mWriteBlock                               = NULL;
    aCtx->mReadBlock                                = NULL;
    aCtx->mSimpleQueryFetchIPCDAReadBlock.mData     = NULL;

#if defined(__CSURF__)      
    /* BUG-44539 
       ���ʿ��� false alarm�� �����ϱ����ؼ� ȣ���� ����� �˻��Ѵ�.*/
    ACI_TEST_RAISE( (cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA)  && 
                    (aModuleID == CMI_PROTOCOL_MODULE(DB)), ContAllockBlock );
#else 
    /* PROJ-2616 */
    ACI_TEST_RAISE(cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA,
                   ContAllockBlock);
#endif

    // allocate readBlock, writeBlock statically.
    sPool = aCtx->mLink->mPool;
    ACI_TEST(sPool->mOp->mAllocBlock(
            sPool, &aCtx->mReadBlock) != ACI_SUCCESS);
    ACI_TEST(sPool->mOp->mAllocBlock(
            sPool, &aCtx->mWriteBlock) != ACI_SUCCESS);
    aCtx->mWriteBlock->mDataSize = CMP_HEADER_SIZE;
    aCtx->mWriteBlock->mCursor   = CMP_HEADER_SIZE;

    cmpHeaderInitialize(&aCtx->mWriteHeader);
    CMP_MARSHAL_STATE_INITIALIZE(aCtx->mMarshalState);
    aCtx->mProtocol.mFinalizeFunction = (void *)cmpArgNULL;

    acpListInit(&aCtx->mReadBlockList);
    acpListInit(&aCtx->mWriteBlockList);

    ACI_EXCEPTION_CONT(ContAllockBlock);

    aCtx->mListLength   = 0;
    aCtx->mCmSeqNo      = 0; // started from 0
    aCtx->mIsDisconnect = ACP_FALSE;
    aCtx->mSessionCloseNeeded  = ACP_FALSE;

    cmiDisableCompress( aCtx );
    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
    /* ���� ���� ����� default ���� LZO */ 
    cmiSetDecompressType( aCtx, CMI_COMPRESS_LZO );

    aCtx->mProtocol.mServerLastOpID = CMP_OP_DB_LAST_OP_ID_OF_VER_7_1;  /* PROJ-2733-Protocol */
    
    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

/***********************************************************
 * �� �Լ��� ������ ������Ŀ��� �޸� �ݳ��� ����
 * �ݵ�� ȣ��Ǿ�� �Ѵ�
 * ���ο����� A7�� A5 ������ ���ÿ� ó���ϵ��� �Ǿ� �ִ�
***********************************************************/
ACI_RC cmiFreeCmBlock(cmiProtocolContext* aCtx)
{
    cmbPool*  sPool;

    ACI_TEST(aCtx->mLink == NULL);

    ACI_TEST_RAISE(cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA,
                   ContFreeCmBlock);

    if (aCtx->mLink->mLink.mPacketType != CMP_PACKET_TYPE_A5)
    {
        sPool = aCtx->mLink->mPool;

        /* BUG-44547 
         * mWriteBlock and mReadBlock can be null 
         * if AllocLink has failed with a same DBC handle multiple times.*/
        if( aCtx->mWriteBlock != NULL )
        {
            // timeout���� ������ ������ �����޽����� ������
            // ���� �����Ͱ� ���� cmBlock�� ���� �ִ�. �̸� ����
            if (aCtx->mWriteBlock->mCursor > CMP_HEADER_SIZE)
            {
                (void)cmiSend(aCtx, ACP_TRUE);
            }

            ACI_TEST(sPool->mOp->mFreeBlock(sPool, aCtx->mWriteBlock)
                     != ACI_SUCCESS);
            aCtx->mWriteBlock = NULL;
        }

        if( aCtx->mReadBlock != NULL ) 
        {
            ACI_TEST(sPool->mOp->mFreeBlock(sPool, aCtx->mReadBlock)
                     != ACI_SUCCESS);
            aCtx->mReadBlock = NULL;
        }
    }
    else
    {
        if (aCtx->mReadBlock != NULL)
        {
            acpListAppendNode(&aCtx->mReadBlockList,
                              &aCtx->mReadBlock->mListNode);
        }

        aCtx->mReadBlock = NULL;
        ACI_TEST_RAISE(cmiFlushProtocol(aCtx, ACP_TRUE)
                       != ACI_SUCCESS, FlushFail);
        ACI_TEST(cmiFinalizeProtocol(&aCtx->mProtocol)
                 != ACI_SUCCESS);

        /*
         * Free All Read Blocks
         */
        ACI_TEST(cmiFreeReadBlock(aCtx) != ACI_SUCCESS);
    }

    ACI_EXCEPTION_CONT(ContFreeCmBlock);

    return ACI_SUCCESS;

    ACI_EXCEPTION(FlushFail);
    {
        ACE_ASSERT(cmiFinalizeProtocol(&aCtx->mProtocol) == ACI_SUCCESS);
        if (cmiGetLinkImpl(aCtx) != CMN_LINK_IMPL_IPCDA)
        {
            ACE_ASSERT(cmiFreeReadBlock(aCtx) == ACI_SUCCESS);
        }
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * CM Block except CM Packet header is compressed by using aclCompress. then
 * Original CM Block is replaced with compressed one.
 */
/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
static ACI_RC cmiCompressLZOCmBlock( cmiProtocolContext * aCtx,
                                     cmbBlock           * aBlock )
{
    acp_uint8_t sOutBuffer[
        IDU_COMPRESSION_MAX_OUTSIZE( CMB_BLOCK_DEFAULT_SIZE ) ] = { 0, };
    acp_uint8_t sWorkMemory[ IDU_COMPRESSION_WORK_SIZE ] = { 0, };
    acp_uint32_t sCompressSize = 0;

    ACI_TEST_RAISE( aclCompress( aBlock->mData + CMP_HEADER_SIZE,
                                 aBlock->mCursor - CMP_HEADER_SIZE,
                                 sOutBuffer,
                                 sizeof( sOutBuffer ),
                                 &sCompressSize,
                                 sWorkMemory )
                    != ACP_RC_SUCCESS, COMPRESS_ERROR );

    aBlock->mCursor = CMP_HEADER_SIZE;
    CMI_WCP( aCtx, sOutBuffer, sCompressSize );
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( COMPRESS_ERROR )
    {
        ACI_SET( aciSetErrorCode( cmERR_ABORT_COMPRESS_DATA_ERROR ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC cmiCompressLZ4CmBlock( cmiProtocolContext * aCtx,
                                     cmbBlock           * aBlock )
{
    acp_uint8_t  sOutBuffer[ ACL_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE ) ];
    acp_sint32_t sCompressSize = 0;
    acp_sint32_t sMaxCompressSize = ACL_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE );

    /* BUG-47078 Apply LZ4 Algorithm */
    sCompressSize = aclLZ4_compress_fast( (acp_char_t*)aBlock->mData + CMP_HEADER_SIZE,
                                          (acp_char_t*)sOutBuffer,
                                          aBlock->mCursor - CMP_HEADER_SIZE,
                                          sMaxCompressSize,
                                          aCtx->mCompressLevel );
    ACI_TEST_RAISE( ( sCompressSize <= 0 ) ||
                    ( sCompressSize > sMaxCompressSize),
                    COMPRESS_ERROR );

    aBlock->mCursor = CMP_HEADER_SIZE;
    CMI_WCP( aCtx, sOutBuffer, sCompressSize );
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( COMPRESS_ERROR )
    {
        ACI_SET( aciSetErrorCode( cmERR_ABORT_COMPRESS_DATA_ERROR ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * CM Block except CM Packet header is decompressed by using aclCompress. then
 * Original CM Block is replaced with decompressed one.
 */
/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
static ACI_RC cmiDecompressLZOCmBlock( cmbBlock           * aBlock,
                                       acp_uint32_t         aDataLength )
{
    acp_uint8_t sOutBuffer[ CMB_BLOCK_DEFAULT_SIZE ] = { 0, };
    acp_uint32_t sDecompressSize = 0;

    ACI_TEST_RAISE( aclDecompress( aBlock->mData + CMP_HEADER_SIZE,
                                   aDataLength,
                                   sOutBuffer,
                                   sizeof( sOutBuffer ),
                                   &sDecompressSize )
                    != ACP_RC_SUCCESS, DECOMPRESS_ERROR );

    memcpy( aBlock->mData + CMP_HEADER_SIZE, sOutBuffer, sDecompressSize );
    aBlock->mDataSize = sDecompressSize + CMP_HEADER_SIZE;

    return ACI_SUCCESS;

    ACI_EXCEPTION( DECOMPRESS_ERROR )
    {
        ACI_SET( aciSetErrorCode( cmERR_ABORT_DECOMPRESS_DATA_ERROR ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC cmiCompressCmBlock( cmpHeader          * aHeader,
                                  cmiProtocolContext * aCtx,
                                  cmbBlock           * aBlock )
{
    switch ( aCtx->mCompressType )
    {
        case CMI_COMPRESS_NONE :
            CMP_HEADER_FLAG_CLR_COMPRESS( aHeader );
            break;
            
        case CMI_COMPRESS_LZO :
            ACI_TEST( cmiCompressLZ4CmBlock( aCtx, aBlock ) != ACI_SUCCESS );
            CMP_HEADER_FLAG_SET_COMPRESS( aHeader );
            break;
            
        case CMI_COMPRESS_LZ4 :
            ACI_TEST( cmiCompressLZOCmBlock( aCtx, aBlock ) != ACI_SUCCESS );
            CMP_HEADER_FLAG_SET_COMPRESS( aHeader );
            break; 
            
        default :
            ACE_DASSERT( 0 );            
            ACI_RAISE( InvalidCompressType );
            break;             
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( InvalidCompressType )
    {
        ACI_SET( aciSetErrorCode( CM_TRC_COMPRESS_TYPE_ERROR, aCtx->mCompressType ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC cmiDecompressLZ4CmBlock( cmbBlock           * aBlock,
                                       acp_uint32_t         aDataLength )
{
    acp_uint8_t  sOutBuffer[ ACL_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE ) ];
    acp_sint32_t sDecompressSize = 0;
    acp_sint32_t sMaxDecompressSize = ACL_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE );

    /* BUG-47078 Apply LZ4 Algorithm */
    sDecompressSize = aclLZ4_decompress_safe( (acp_char_t*)aBlock->mData + CMP_HEADER_SIZE,
                                              (acp_char_t*)sOutBuffer,
                                              aDataLength,
                                              sMaxDecompressSize );
    ACI_TEST_RAISE( ( sDecompressSize <= 0 ) ||
                    ( sDecompressSize > sMaxDecompressSize ),
                    DECOMPRESS_ERROR );

    acpMemCpy( aBlock->mData + CMP_HEADER_SIZE,
               sOutBuffer,
               sDecompressSize );
    aBlock->mDataSize = sDecompressSize + CMP_HEADER_SIZE;

    return ACI_SUCCESS;

    ACI_EXCEPTION( DECOMPRESS_ERROR )
    {
        ACI_SET( aciSetErrorCode( cmERR_ABORT_DECOMPRESS_DATA_ERROR ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC cmiDecompressCmBlock( cmiProtocolContext  *aCtx,
                                    acp_uint32_t         aDataLength )
{
    switch ( aCtx->mDecompressType )
    {   
        case CMI_COMPRESS_NONE :
            break;
            
        case CMI_COMPRESS_LZO :
            ACI_TEST( cmiDecompressLZOCmBlock( aCtx->mReadBlock,
                                               aDataLength )
                      != ACI_SUCCESS );
            break;
            
        case CMI_COMPRESS_LZ4 :
            ACI_TEST( cmiDecompressLZ4CmBlock( aCtx->mReadBlock,
                                               aDataLength )
                      != ACI_SUCCESS );
            break;
            
        default :
            ACE_DASSERT( 0 );
            ACI_RAISE( InvalidDecompressType );
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( InvalidDecompressType )
    {
        ACI_SET( aciSetErrorCode( CM_TRC_DECOMPRESS_TYPE_ERROR, aCtx->mDecompressType ) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;    
}

/*************************************************************
 * proj_2160 cm_type removal
 * 1. �� �Լ��� A7 �̻� �����̴�
 * 2. ��Ŷ ���Ź� �ش� �������ݿ� �����ϴ� �ݹ��Լ���
 *  �ڵ� ȣ���ϱ� ���� ���Ǵ� �Լ��̴�
 * 3. ��Ŷ �Ѱ��� �о���� �� ��Ŷ�� �������� ���������� ���
 *  ���� �� �����Ƿ� �ݺ������� �ݺ������� �ݹ��� ȣ���Ѵ�
 * 4. �ݺ����� ������ ������ ��Ŷ �����͸� ���� �� ���� ����̴�
 * 5. ���� ��Ŷ�� ������ ���(ū ��������)�� ���⼭ ó������ ������
 *  �ش� �������� �ݹ�ȿ��� �ݺ����� ����Ͽ� �˾Ƽ� ó���Ѵ�
**************************************************************
 * 6. �׷� �������� (���� ��Ŷ�ε� ��Ŷ���� �ϼ��� ���¸� ����)��
 *  ��� �ݹ� ���ο��� �ݺ������� ó���ϱⰡ ����� ������
 *  ���⼭ goto�� ����Ͽ� Ư��ó�� �Ѵ� (ex) �޽��� ��������)
 * 7. A5 client�� �����ϸ� handshakeProtocol�� ȣ���ϰ� �Ǹ�
 *  �ش� �ݹ�ȿ��� A5 ������ �ָ鼭 A5�� ��ȯ�� �ǰ� �ȴ�
 * 8. ��Ŷ���� ���ǳ� �����Ϸù�ȣ(���Žø��� 1�� ����)�� �ο��Ͽ�
 *  �߸��ǰų� �ߺ��� ��Ŷ�� ���ŵǴ� ���� ���´�
 *  (����� A5������ ���� ��Ŷ�� ���ؼ��� �Ϸù�ȣ�� �ο��߾���)
 * 9. RP(ALA ����) ��⵵ �� �Լ��� ���� ����Ѵ�. �ٸ� RP�� ���
 *  �ݹ鱸���� �ƴϱ� ������ ��Ŷ ������ �ٷ� �Լ��� ����������
 * 10. CMI_DUMP: ������ ����� �뵵�� �־�ξ���. �ܼ��� ����
 *  �������� �̸��� ��Ŷ���� ������ ����Ѵ�. ���Ŀ� alter system
 *  ���� ��Ŷ������ �Ҽ� �ֵ��� �����ϴ� �͵� ���� �� ����
 * 11. �� �Լ��� A5�� cmiReadBlock + cmiReadProtocolAndCallback
 *  �� ��ü�Ѵ�
*************************************************************/
/* #define CMI_DUMP 1 */
ACI_RC cmiRecv(cmiProtocolContext* aCtx,
               void*           aUserContext,
               acp_time_t      aTimeout)
{
    cmpCallbackFunction  sCallbackFunction;
    cmpHeader*           sHeader = &(aCtx->mReadHeader);
    acp_uint32_t         sCmSeqNo;
    acp_uint8_t          sOpID;
    ACI_RC               sRet = ACI_SUCCESS;

beginToRecv:
    aCtx->mReadBlock->mDataSize  = 0;
    aCtx->mReadBlock->mCursor   = 0;

    ACI_TEST_RAISE(aCtx->mIsDisconnect == ACP_TRUE, Disconnected);
    ACI_TEST(aCtx->mLink->mPeerOp->mRecv(aCtx->mLink,
                                         &aCtx->mReadBlock,
                                         sHeader,
                                         aTimeout) != ACI_SUCCESS);

    // ALA�� ��� ����� ��ġ �Ǽ���,
    // A5 RP�κ���  handshake ��Ŷ�� ������ �� �ִ�.
    // �׷� ���, ������� �ٷ� ����ó���Ѵ�.
    if (aCtx->mLink->mLink.mPacketType == CMP_PACKET_TYPE_A5)
    {
        ACI_RAISE(MarshalErr);
    }

    if ( CMP_HEADER_FLAG_COMPRESS_IS_SET( sHeader ) == ACP_TRUE )
    {
        /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
        ACI_TEST( cmiDecompressCmBlock( aCtx, 
                                        sHeader->mA7.mPayloadLength )
                  != ACI_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    sCmSeqNo = CMP_HEADER_SEQ_NO(sHeader);
#ifdef CMI_DUMP
    printf("[%5d] recv [%5d]\n", sCmSeqNo, sHeader->mA7.mPayloadLength);
#endif

    // ��� ��Ŷ�� ���ǳ��ּ� �����Ϸù�ȣ�� ���´�.
    // ����: 0 ~ 0x7fffffff, �ִ밪�� �ٴٸ��� 0���� �ٽ� ���۵ȴ�
    ACI_TEST_RAISE(sCmSeqNo != aCtx->mCmSeqNo, InvalidProtocolSeqNo);
    if (aCtx->mCmSeqNo == CMP_HEADER_MAX_SEQ_NO)
    {
        aCtx->mCmSeqNo = 0;
    }
    else
    {
        aCtx->mCmSeqNo++;
    }

    // RP(ALA) ��⿡���� callback�� ������� �ʴ´�.
    // ����, RP�� ��� callback ȣ����� �ٷ� return �Ѵ�.
    if (aCtx->mModule->mModuleID == CMP_MODULE_RP)
    {
        ACI_RAISE(CmiRecvReturn);
    }

    while (1)
    {
        CMI_RD1(aCtx, sOpID);
        ACI_TEST_RAISE(sOpID >= aCtx->mModule->mOpMax, InvalidOpError);  /* BUG-45804 */
#ifdef CMI_DUMP
        printf("%s\n", gCmpOpDBMapClient[sOpID].mCmpOpName);
#endif

        aCtx->mSessionCloseNeeded  = ACP_FALSE;
        aCtx->mProtocol.mOpID      = sOpID;  /* PROJ-2733-Protocol */
        sCallbackFunction = aCtx->mModule->mCallbackFunction[sOpID];

        sRet = sCallbackFunction(aCtx,
                                 &aCtx->mProtocol,
                                 aCtx->mOwner,
                                 aUserContext);

        // dequeue�� ��� IDE_CM_STOP�� ��ȯ�� �� �ִ�.
        ACI_TEST_RAISE(sRet != ACI_SUCCESS, CmiRecvReturn);

        // ������ ��Ŷ�� ���� ��� �������� ó���� ���� ���
        if (aCtx->mReadBlock->mCursor == aCtx->mReadBlock->mDataSize)
        {
            break;
        }
        // �������� �ؼ��� �߸��Ǿ� cursor�� ��Ŷ�� �Ѿ ���
        else if (aCtx->mReadBlock->mCursor > aCtx->mReadBlock->mDataSize)
        {
            ACI_RAISE(MarshalErr);
        }

        ACI_TEST_RAISE(aCtx->mIsDisconnect == ACP_TRUE, Disconnected);
    }

    // special �������� ó��(Message, LobPut protocol)
    // msg�� lobput ���������� ��� �������� group���� ������ ������
    // (��Ŷ���� ���� opID�� ������ ��Ŷ����� ���� flag�� 0�̴�)
    // �̵��� ������ �����ϴ��� ������ �ѹ��� ����۽��ؾ� �Ѵ�.
    if (CMP_HEADER_PROTO_END_IS_SET(sHeader) == ACP_FALSE)
    {
        goto beginToRecv;
    }

    ACI_EXCEPTION_CONT(CmiRecvReturn);

    return sRet;

    ACI_EXCEPTION(Disconnected)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(InvalidProtocolSeqNo)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    ACI_EXCEPTION(MarshalErr)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_MARSHAL_ERROR));
    }
    ACI_EXCEPTION(InvalidOpError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**************************************************************
 * PROJ-2616
 *
 * IPCDA ��忡�� ���� �޸𸮷κ��� �����͸� �о� ���� �Լ�.
 *
 * aCtx            [in] - cmiProtocolContext
 * aUserContext    [in] - UserContext(fnContext)
 * aTimeout        [in] - timeout count values
 * aMicroSleepTime [in] - Sleep time. (micro seconds.)
 **************************************************************/
ACI_RC cmiRecvIPCDA(cmiProtocolContext *aCtx,
                    void               *aUserContext,
                    acp_time_t          aTimeout,
                    acp_uint32_t        aMicroSleepTime)
{
    cmpCallbackFunction  sCallbackFunction;
    acp_uint8_t          sOpID;
    ACI_RC               sRet = ACI_SUCCESS;

    cmbBlockIPCDA  *sOrgBlock              = NULL;
    cmbBlock        sTmpBlock;
    acp_uint32_t    sCurReadOperationCount = 0;

    cmnLinkPeerIPCDA        *sCmnLinkPeerIPCDA = (cmnLinkPeerIPCDA*)aCtx->mLink;
    cmnLinkDescIPCDA        *sDesc             = &sCmnLinkPeerIPCDA->mDesc;
    cmbShmIPCDAChannelInfo  *sChannelInfo      = NULL;

    acp_char_t        sMessageQBuf[1];
    struct timespec   sMessageQWaitTime;
    
    ACI_TEST_RAISE(aCtx->mIsDisconnect == ACP_TRUE, Disconnected);

    /* BUG-45713 */
    /* check ghost-connection in shutdown state() : PR-4096 */
    sChannelInfo = cmbShmIPCDAGetChannelInfo(gIPCDAShmInfo.mShmBuffer, sDesc->mChannelID);
    ACI_TEST_RAISE(sDesc->mTicketNum != sChannelInfo->mTicketNum, GhostConnection);

#if defined(ALTI_CFG_OS_LINUX)
    if( sCmnLinkPeerIPCDA->mMessageQ.mUseMessageQ == ACP_TRUE )
    {
        clock_gettime(CLOCK_REALTIME, &sMessageQWaitTime);
        sMessageQWaitTime.tv_sec += sCmnLinkPeerIPCDA->mMessageQ.mMessageQTimeout;

        ACI_TEST_RAISE((mq_timedreceive( sCmnLinkPeerIPCDA->mMessageQ.mMessageQID,
                                         sMessageQBuf,
                                         1,
                                         NULL,
                                         &sMessageQWaitTime) < 0), err_messageq_timeout);
    }
    else
    {
        /* Do nothing. */
    }
#endif

    sOrgBlock = (cmbBlockIPCDA*)aCtx->mReadBlock;
    sTmpBlock.mBlockSize = sOrgBlock->mBlock.mBlockSize;
    sTmpBlock.mCursor = CMP_HEADER_SIZE;
    sTmpBlock.mDataSize = sOrgBlock->mBlock.mDataSize;
    sTmpBlock.mIsEncrypted = sOrgBlock->mBlock.mIsEncrypted;
    sTmpBlock.mData = &sOrgBlock->mData;
    
    ACI_TEST_RAISE(cmiIPCDACheckReadFlag(aCtx,
                                         NULL,
                                         aMicroSleepTime,
                                         aTimeout) != ACI_SUCCESS, Disconnected);
    aCtx->mReadBlock = &sTmpBlock;

    while (1)
    {
        ACI_TEST_RAISE(cmiIPCDACheckDataCount((void*)aCtx,
                                              &sOrgBlock->mOperationCount,
                                              sCurReadOperationCount,
                                              aTimeout,
                                              aMicroSleepTime) != ACI_SUCCESS, err_cmi_lockcount);

        sTmpBlock.mDataSize = sOrgBlock->mBlock.mDataSize;

        CMI_RD1(aCtx, sOpID);
        ACI_TEST_RAISE( sOpID >= aCtx->mModule->mOpMax, InvalidOpError);

        aCtx->mProtocol.mOpID = sOpID;  /* PROJ-2733-Protocol */

        if (sOpID == CMP_OP_DB_IPCDALastOpEnded)
        {
            sCurReadOperationCount++;
            break;
        }
        else
        {
            sCallbackFunction = aCtx->mModule->mCallbackFunction[sOpID];
            sRet = sCallbackFunction(aCtx,
                                     &aCtx->mProtocol,
                                     aCtx->mOwner,
                                     aUserContext);
            sCurReadOperationCount++;
            ACI_TEST(sRet != ACI_SUCCESS);
        }
    }

    /* BUG-44468 [cm] codesonar warning in CM */
    aCtx->mReadBlock                  = (cmbBlock*)sOrgBlock;

    cmiLinkPeerFinalizeCliReadForIPCDA(aCtx);

    return sRet;

    ACI_EXCEPTION(err_messageq_timeout)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_TIMED_OUT));
    }
    ACI_EXCEPTION(err_cmi_lockcount)
    {
        aCtx->mIsDisconnect = ACP_TRUE;
    }
    ACI_EXCEPTION(Disconnected);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(InvalidOpError)
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    ACI_EXCEPTION(GhostConnection);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION_END;

    if (sOrgBlock != NULL)
    {
        aCtx->mReadBlock = (cmbBlock*)sOrgBlock;
    }

    cmiLinkPeerFinalizeCliReadForIPCDA(aCtx);

    return ACI_FAILURE;
}

/*************************************************************
 * proj_2160 cm_type removal
 * 1. �� �Լ��� �ݹ� �ȿ��� ���� ��Ŷ�� ���������� �����ϴ� ��쿡
 *  ����ϱ� ���� ���������.
 * 2. cmiRecv()���� �������� �ݹ��� ȣ���ϴ� �ݺ����� ����
*************************************************************/
ACI_RC cmiRecvNext(cmiProtocolContext* aCtx, acp_time_t aTimeout)
{
    cmpHeader*           sHeader = &(aCtx->mReadHeader);
    acp_uint32_t         sCmSeqNo;

    aCtx->mReadBlock->mDataSize  = 0;
    aCtx->mReadBlock->mCursor    = 0;

    ACI_TEST_RAISE(aCtx->mIsDisconnect == ACP_TRUE, Disconnected);
    ACI_TEST(aCtx->mLink->mPeerOp->mRecv(aCtx->mLink,
                                         &aCtx->mReadBlock,
                                         sHeader,
                                         aTimeout) != ACI_SUCCESS);

    sCmSeqNo = CMP_HEADER_SEQ_NO(sHeader);
#ifdef CMI_DUMP
    printf("[%5d] recv [%5d]\n", sCmSeqNo, sHeader->mA7.mPayloadLength);
#endif

    // ��� ��Ŷ�� ���ǳ��ּ� �����Ϸù�ȣ�� ���´�.
    // ����: 0 ~ 0x7fffffff, �ִ밪�� �ٴٸ��� 0���� �ٽ� ���۵ȴ�
    ACI_TEST_RAISE(sCmSeqNo != aCtx->mCmSeqNo, InvalidProtocolSeqNo);
    if (aCtx->mCmSeqNo == CMP_HEADER_MAX_SEQ_NO)
    {
        aCtx->mCmSeqNo = 0;
    }
    else
    {
        aCtx->mCmSeqNo++;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(Disconnected);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    ACI_EXCEPTION(InvalidProtocolSeqNo);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

/********************************************************
 * PROJ-2616
 * cmiInitIPCDABuffer
 *
 * �������� ���ؽ�Ʈ���� cmiBlock�� �ʱ�ȭ �Ѵ�.
 *
 * aCtx[in] - cmiProtocolContext
 ********************************************************/
void cmiInitIPCDABuffer(cmiProtocolContext *aCtx)
{
    cmbBlockIPCDA    *sBlockIPCDA = NULL;
    cmnLinkPeerIPCDA *sLinkDA = NULL;

    aCtx->mLink->mLink.mPacketType = CMP_PACKET_TYPE_A7;
    sLinkDA = (cmnLinkPeerIPCDA *)aCtx->mLink;

    if (aCtx->mWriteBlock == NULL)
    {
        aCtx->mWriteBlock = (cmbBlock*)cmnLinkPeerGetWriteBlock(sLinkDA->mDesc.mChannelID);
        aCtx->mWriteBlock->mDataSize = CMP_HEADER_SIZE;
        aCtx->mWriteBlock->mCursor   = CMP_HEADER_SIZE;
        aCtx->mWriteHeader.mA7.mHeaderSign = CMP_HEADER_SIGN_A7;
    }

    if (aCtx->mReadBlock == NULL)
    {
        aCtx->mReadBlock = (cmbBlock*)cmnLinkPeerGetReadBlock(sLinkDA->mDesc.mChannelID);
        aCtx->mReadHeader.mA7.mHeaderSign = CMP_HEADER_SIGN_A7;
    }

    if (aCtx->mSimpleQueryFetchIPCDAReadBlock.mData == NULL)
    {
        aCtx->mSimpleQueryFetchIPCDAReadBlock.mData = cmbShmIPCDAGetClientReadDataBlock(gIPCDAShmInfo.mShmBufferForSimpleQuery,
                                                                                        0,
                                                                                        sLinkDA->mDesc.mChannelID);
    }

    sBlockIPCDA =(cmbBlockIPCDA *)aCtx->mWriteBlock;
    sBlockIPCDA->mBlock.mData = &sBlockIPCDA->mData;
}

/*************************************************************
 * proj_2160 cm_type removal
 * 1. �� �Լ��� A5�� cmiWriteBlock�� ��ü�Ѵ�
 * 2. �� �Լ������� ��Ŷ ����� ����� ��Ŷ�� �۽��Ѵ� 
 * 3. A5�� ���������� pendingList�� �����Ѵ�. ������
 *  Altibase������ �񵿱� ����� ����(ex) client�� �۽��ϰ� �ִ�
 *  �����ε�, ���������� ������ �ٷ� �����ϸ� �� ��Ŷ�� �ѱ�� ���
 *  �ٷ� �۽��� �Ǿ������� �Ǿ� ����) �ѵ�, �� �� ���Ϲ��۰�
 *  �� ���� ������ ��� ���۸��� ���� �ʰ� ���� ����ϰ� �Ǹ�
 *  ���θ� ��Ÿ�� ��ٸ��� ��Ȳ�� ���������� �ִ�
*************************************************************/
ACI_RC cmiSend(cmiProtocolContext *aCtx, acp_bool_t aIsEnd)
{
    cmnLinkPeer     *sLink   = aCtx->mLink;
    cmbBlock        *sBlock  = aCtx->mWriteBlock;
    cmpHeader       *sHeader = &aCtx->mWriteHeader;
    cmbPool         *sPool   = aCtx->mLink->mPool;

    cmbBlock        *sPendingBlock;
    acp_list_node_t *sIterator;
    acp_list_node_t *sNodeNext;
    acp_bool_t       sSendSuccess;
    acp_bool_t       sNeedToSave;
    cmbBlock        *sNewBlock;

    /* bug-27250 IPC linklist can be crushed */
    acp_time_t        sWaitTime;
    cmnDispatcherImpl sImpl;
    acp_uint32_t      sCmSeqNo;

    ACI_TEST_RAISE(sBlock->mCursor == CMP_HEADER_SIZE, noDataToSend);

    if (aIsEnd == ACP_TRUE)
    {
        CMP_HEADER_SET_PROTO_END(sHeader);
    }
    else
    {
        CMP_HEADER_CLR_PROTO_END(sHeader);
    }
    
    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
    ACI_TEST( cmiCompressCmBlock( sHeader, 
                                  aCtx, 
                                  sBlock )
              != ACI_SUCCESS );

    sBlock->mDataSize = sBlock->mCursor;
    ACI_TEST(cmpHeaderWrite(sHeader, sBlock) != ACI_SUCCESS);

    // Pending Write Block���� ���� (send previous packets)
    ACP_LIST_ITERATE_SAFE(&aCtx->mWriteBlockList, sIterator, sNodeNext)
    {
        sPendingBlock = (cmbBlock *)sIterator->mObj;

        sSendSuccess = ACP_TRUE;

        // BUG-19465 : CM_Buffer�� pending list�� ����
        while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != ACI_SUCCESS)
        {
            sSendSuccess = ACP_FALSE;

            ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);

            /* BUG-44468 [cm] codesonar warning in CM */
            if( aCtx->mListLength < gMaxPendingList )
            {
                break;
            }

            /* bug-27250 IPC linklist can be crushed.
             * ������: all timeout NULL, ������: 1 msec for IPC
             * IPC�� ��� ���Ѵ���ϸ� �ȵȴ�.
             */
            sImpl = cmnDispatcherImplForLinkImpl(((cmnLink*)sLink)->mImpl);
            if (sImpl == CMI_DISPATCHER_IMPL_IPC)
            {
                sWaitTime = acpTimeFrom(0, 1000);
            }
            else
            {
                sWaitTime = ACP_TIME_INFINITE;
            }

            ACI_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                           CMN_DIRECTION_WR,
                                           sWaitTime) != ACI_SUCCESS);

            sSendSuccess = ACP_TRUE;
        }

        if (sSendSuccess == ACP_FALSE)
        {
            break;
        }

        ACI_TEST(sPool->mOp->mFreeBlock(sPool, sPendingBlock) != ACI_SUCCESS);
        aCtx->mListLength--;
    }

    // send current block if there is no pendng block
    if (sIterator == &aCtx->mWriteBlockList)
    {
        if (sLink->mPeerOp->mSend(sLink, sBlock) != ACI_SUCCESS)
        {
            ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);
            sNeedToSave = ACP_TRUE;
        }
        else
        {
            sNeedToSave = ACP_FALSE;
        }
    }
    else
    {
        sNeedToSave = ACP_TRUE;
    }

    // ���� block�� pendingList �ǵڿ� ������ �д�
    if (sNeedToSave == ACP_TRUE)
    {
        sNewBlock = NULL;
        ACI_TEST(sPool->mOp->mAllocBlock(sPool, &sNewBlock) != ACI_SUCCESS);
        sNewBlock->mDataSize = sNewBlock->mCursor = CMP_HEADER_SIZE;
        acpListAppendNode(&aCtx->mWriteBlockList, &sBlock->mListNode);
        aCtx->mWriteBlock = sNewBlock;
        sBlock            = sNewBlock;
        aCtx->mListLength++;
    }

    // ��� ��Ŷ�� ���ǳ��ּ� �����Ϸù�ȣ�� ���´�.
    // ����: 0 ~ 0x7fffffff, �ִ밪�� �ٴٸ��� 0���� �ٽ� ���۵ȴ�
    sCmSeqNo = CMP_HEADER_SEQ_NO(sHeader);
    if (sCmSeqNo == CMP_HEADER_MAX_SEQ_NO)
    {
        sHeader->mA7.mCmSeqNo = 0;
    }
    else
    {
        sHeader->mA7.mCmSeqNo = sCmSeqNo + 1;
    }

    if (aIsEnd == ACP_TRUE)
    {
        // �������� ���̶�� ��� Block�� ���۵Ǿ�� ��
        ACP_LIST_ITERATE_SAFE(&aCtx->mWriteBlockList, sIterator, sNodeNext)
        {
            sPendingBlock = (cmbBlock *)sIterator->mObj;

            while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != ACI_SUCCESS)
            {
                ACI_TEST_RAISE(aciIsRetry() != ACI_SUCCESS, SendFail);
                ACI_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                               CMN_DIRECTION_WR,
                                               ACP_TIME_INFINITE) != ACI_SUCCESS);
            }

            ACI_TEST(sPool->mOp->mFreeBlock(sPool, sPendingBlock) != ACI_SUCCESS);
            aCtx->mListLength--;
        }

        // for IPC
        sLink->mPeerOp->mReqComplete(sLink);
    }

noDataToSend:
    sBlock->mDataSize = sBlock->mCursor = CMP_HEADER_SIZE;
    return ACI_SUCCESS;

    ACI_EXCEPTION(SendFail);
    {
    }
    ACI_EXCEPTION_END;

    sBlock->mDataSize = sBlock->mCursor = CMP_HEADER_SIZE;
    return ACI_FAILURE;
}

ACI_RC cmiCheckAndFlush( cmiProtocolContext * aProtocolContext,
                         acp_uint32_t aLen,
                         acp_bool_t aIsEnd )
{
    if ( aProtocolContext->mWriteBlock->mCursor + aLen >
         aProtocolContext->mWriteBlock->mBlockSize )
    {
        ACI_TEST( cmiSend( aProtocolContext, aIsEnd ) != ACI_SUCCESS );
    }
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSplitRead( cmiProtocolContext *aCtx,
                     acp_uint64_t        aReadSize,
                     acp_uint8_t        *aBuffer,
                     acp_time_t          aTimeout )
{
    acp_uint8_t   *sBuffer      = aBuffer;
    acp_uint64_t   sReadSize    = aReadSize;
    acp_uint32_t   sRemainSize  = aCtx->mReadBlock->mDataSize -
                                  aCtx->mReadBlock->mCursor;
    acp_uint32_t   sCopySize;

    while( sReadSize > sRemainSize )
    {
        sCopySize = ACP_MIN( sReadSize, sRemainSize );

        CMI_RCP ( aCtx, sBuffer, sCopySize );
        ACI_TEST( cmiRecvNext( aCtx, aTimeout ) != ACI_SUCCESS );

        sRemainSize = aCtx->mReadBlock->mDataSize - CMP_HEADER_SIZE;
        sBuffer    += sCopySize;
        sReadSize  -= sCopySize;
    }
    CMI_RCP( aCtx, sBuffer, sReadSize );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSplitSkipRead( cmiProtocolContext *aCtx,
                         acp_uint64_t        aReadSize,
                         acp_time_t          aTimeout )
{
    acp_uint64_t sReadSize    = aReadSize;
    acp_uint32_t sRemainSize  = 0;
    acp_uint64_t sCopySize    = 0;

    if (cmiGetLinkImpl(aCtx) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST(aCtx->mReadBlock->mDataSize < (aCtx->mReadBlock->mCursor + aReadSize));
    }
    else
    {
        sRemainSize = aCtx->mReadBlock->mDataSize - aCtx->mReadBlock->mCursor;
        while( sReadSize > sRemainSize )
        {
            ACI_TEST( cmiRecvNext( aCtx, aTimeout ) != ACI_SUCCESS );

            sCopySize = ACP_MIN( sReadSize, sRemainSize );

            sRemainSize = aCtx->mReadBlock->mDataSize - CMP_HEADER_SIZE;
            sReadSize  -= sCopySize;
        }
    }
    aCtx->mReadBlock->mCursor += sReadSize;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmiSplitWrite( cmiProtocolContext *aCtx,
                      acp_uint64_t        aWriteSize,
                      acp_uint8_t        *aBuffer )
{
    acp_uint8_t  *sBuffer      = aBuffer;
    acp_uint64_t  sWriteSize   = aWriteSize;
    acp_uint32_t  sRemainSize  = 0;
    acp_uint32_t  sCopySize    = 0;

    if (cmiGetLinkImpl(aCtx) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST( aCtx->mWriteBlock->mCursor + aWriteSize >= CMB_BLOCK_DEFAULT_SIZE );
    }
    else
    {
        sRemainSize = aCtx->mWriteBlock->mBlockSize - aCtx->mWriteBlock->mCursor;
        while( sWriteSize > sRemainSize )
        {
            sCopySize = ACP_MIN( sWriteSize, sRemainSize );

            CMI_WCP ( aCtx, sBuffer, sCopySize );
            ACI_TEST( cmiSend ( aCtx, ACP_FALSE ) != ACI_SUCCESS );

            sRemainSize  = CMB_BLOCK_DEFAULT_SIZE - CMP_HEADER_SIZE;
            sBuffer     += sCopySize;
            sWriteSize  -= sCopySize;
        }
    }
    CMI_WCP( aCtx, sBuffer, sWriteSize );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
/*
 *
 */ 
void cmiEnableCompress( cmiProtocolContext * aCtx,
                        acp_uint32_t         aLevel,
                        cmiCompressType      aCompressType )
{
    aCtx->mCompressLevel    = aLevel;
    aCtx->mCompressType     = aCompressType;
}

/*
 *
 */ 
void cmiDisableCompress( cmiProtocolContext * aCtx )
{
    aCtx->mCompressLevel    = 0;
    aCtx->mCompressType     = CMI_COMPRESS_NONE;
}

/*
 *
 */ 
void cmiSetDecompressType( cmiProtocolContext * aCtx,
                           cmiCompressType      aDecompressType )
{
    aCtx->mDecompressType = aDecompressType;
}