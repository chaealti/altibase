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
#include <idsGPKI.h>
#include <cmuProperty.h>
#include <iduLZ4.h>
#include <iduCompression.h>
#include <idsRC4.h>

UChar * cmnLinkPeerGetReadBlock(UInt aChannelID);
UChar * cmnLinkPeerGetWriteBlock(UInt aChannelID);

extern cmpOpMap gCmpOpBaseMap[];
extern cmpOpMap gCmpOpDBMap[];
extern cmpOpMap gCmpOpRPMap[];
extern cmpOpMap gCmpOpDKMap[];

// BUG-19465 : CM_Buffer�� pending list�� ����
UInt     gMaxPendingList;

//BUG-21080
static pthread_once_t     gCMInitOnce  = PTHREAD_ONCE_INIT;
static PDL_thread_mutex_t gCMInitMutex;
static SInt               gCMInitCount;
/* bug-33841: ipc thread's state is wrongly displayed */
static cmiCallbackSetExecute gCMCallbackSetExecute = NULL;

/* PROJ-2681 */
extern "C" const acp_uint32_t aciVersionID;

const acp_char_t *gCmErrorFactory[] =
{
#include "E_CM_US7ASCII.c"
};

inline IDE_RC cmiIPCDACheckLinkAndWait(cmiProtocolContext *aCtx,
                                       UInt                aMicroSleepTime,
                                       UInt               *aCurLoopCount)
{
    idBool            sLinkIsClosed = ID_FALSE;
    cmnLinkPeerIPCDA *sLink         = (cmnLinkPeerIPCDA *)aCtx->mLink;

    if( ++(*aCurLoopCount) == aCtx->mIPDASpinMaxCount )
    {
        sLink->mLinkPeer.mPeerOp->mCheck(aCtx->mLink, &sLinkIsClosed);
        IDE_TEST_RAISE(sLinkIsClosed == ID_TRUE, err_disconnected);

        if (aMicroSleepTime == 0)
        {
            /* IPCDA_SLEEP_TIME�� ���� 0 �� ���,
             * thread_yield�� �����մϴ�. */
            idlOS::thr_yield();
        }
        else
        {
            acpSleepUsec(aMicroSleepTime);
        }
        (*aCurLoopCount) = 0;

        if( (aCtx->mIPDASpinMaxCount / 2) < CMI_IPCDA_SPIN_MIN_LOOP )
        {
            aCtx->mIPDASpinMaxCount = CMI_IPCDA_SPIN_MIN_LOOP;
        }
        else
        {
            aCtx->mIPDASpinMaxCount = aCtx->mIPDASpinMaxCount / 2;
        }
    }
    else
    {
        idlOS::thr_yield();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_disconnected)
    {
         IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC cmiIPCDACheckReadFlag(cmiProtocolContext *aCtx, UInt aMicroSleepTime)
{
    cmiProtocolContext *sCtx          = (cmiProtocolContext *)aCtx;
    cmbBlockIPCDA      *sReadBlock    = NULL;
    UInt                sLoopCount    = 0;

    sReadBlock = (cmbBlockIPCDA*)sCtx->mReadBlock;

    IDE_TEST(aCtx->mIsDisconnect == ID_TRUE);
    
    /* BUG-46502 atomic �Լ� ���� */
    while ( acpAtomicGet32( &sReadBlock->mRFlag) == CMB_IPCDA_SHM_DEACTIVATED )
    {
        IDE_TEST( cmiIPCDACheckLinkAndWait(aCtx,
                                           aMicroSleepTime,
                                           &sLoopCount) == IDE_FAILURE);
    }

    if( (aCtx->mIPDASpinMaxCount * 2) > CMI_IPCDA_SPIN_MAX_LOOP )
    {
        aCtx->mIPDASpinMaxCount = CMI_IPCDA_SPIN_MAX_LOOP;
    }
    else
    {
        aCtx->mIPDASpinMaxCount = aCtx->mIPDASpinMaxCount * 2;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC cmiIPCDACheckDataCount(cmiProtocolContext *aCtx,
                                     volatile UInt      *aCount,
                                     UInt                aCompValue,
                                     UInt                aMicroSleepTime)
{
    IDE_RC sRC = IDE_FAILURE;
    UInt   sLoopCount    = 0;

    IDE_TEST(aCtx->mIsDisconnect == ID_TRUE);

    IDE_TEST(aCount == NULL);

    /* BUG-46502 atomic �Լ� ���� */
    while ( (UInt)acpAtomicGet32(aCount) <= aCompValue )
    {
        sRC = cmiIPCDACheckLinkAndWait(aCtx,
                                       aMicroSleepTime,
                                       &sLoopCount);
        IDE_TEST(sRC == IDE_FAILURE);
    }

    if( (aCtx->mIPDASpinMaxCount * 2) > CMI_IPCDA_SPIN_MAX_LOOP )
    {
        aCtx->mIPDASpinMaxCount = CMI_IPCDA_SPIN_MAX_LOOP;
    }
    else
    {
        aCtx->mIPDASpinMaxCount = aCtx->mIPDASpinMaxCount * 2;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static void cmiInitializeOnce( void )
{
    IDE_ASSERT(idlOS::thread_mutex_init(&gCMInitMutex) == 0);

    gCMInitCount = 0;
}

static void cmiDump(cmiProtocolContext   *aCtx,
                    cmpHeader            *aHeader,
                    cmbBlock             *aBlock,
                    UInt                  aFromIndex,
                    UInt                  aLen);

#define CMI_CHECK_BLOCK_FOR_READ(aBlock)  ((aBlock)->mCursor < (aBlock)->mDataSize)
#define CMI_CHECK_BLOCK_FOR_WRITE(aBlock) ((aBlock)->mCursor < (aBlock)->mBlockSize)

/* BUG-38102 */
#define RC4_KEY         "7dcb6f959b9d37bf"
#define RC4_KEY_LEN     (16) /* 16 byte ( 128 bit ) */

/*
 * Packet Header�κ��� Module�� ���Ѵ�.
 */
static IDE_RC cmiGetModule(cmpHeader *aHeader, cmpModule **aModule)
{
    /*
     * Module ȹ��
     */
    IDE_TEST_RAISE(aHeader->mA5.mModuleID >= CMP_MODULE_MAX, UnknownModule);

    *aModule = gCmpModule[aHeader->mA5.mModuleID];

    return IDE_SUCCESS;

    IDE_EXCEPTION(UnknownModule);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_UNKNOWN_MODULE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ProtocolContext�� Free ������� Read Block���� ��ȯ�Ѵ�.
 */
static IDE_RC cmiFreeReadBlock(cmiProtocolContext *aProtocolContext)
{
    cmnLinkPeer *sLink;
    cmbBlock    *sBlock;
    iduListNode *sIterator;
    iduListNode *sNodeNext;

    IDE_TEST_CONT(cmiGetLinkImpl(aProtocolContext) == CMN_LINK_IMPL_IPCDA,
                  ContFreeReadBlock);

    /*
     * Protocol Context�κ��� Link ȹ��
     */
    sLink = aProtocolContext->mLink;

    /*
     * Read Block List�� Block�� ��ȯ
     */
    IDU_LIST_ITERATE_SAFE(&aProtocolContext->mReadBlockList, sIterator, sNodeNext)
    {
        sBlock = (cmbBlock *)sIterator->mObj;

        IDE_TEST(sLink->mPeerOp->mFreeBlock(sLink, sBlock) != IDE_SUCCESS);
    }


    IDE_EXCEPTION_CONT(ContFreeReadBlock);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Block�� �о�´�.
 */
static IDE_RC cmiReadBlock(cmiProtocolContext *aProtocolContext, PDL_Time_Value *aTimeout)
{
    UInt sCmSeqNo;

    IDE_TEST_RAISE(aProtocolContext->mIsDisconnect == ID_TRUE, Disconnected);

    /*
     * Link�κ��� Block�� �о��
     */
    IDE_TEST(aProtocolContext->mLink->mPeerOp->mRecv(aProtocolContext->mLink,
                                                     &aProtocolContext->mReadBlock,
                                                     &aProtocolContext->mReadHeader,
                                                     aTimeout) != IDE_SUCCESS);

    /* BUG-45184 */
    aProtocolContext->mReceiveDataSize += aProtocolContext->mReadBlock->mDataSize;
    aProtocolContext->mReceiveDataCount++;
    
    /*
     * Sequence �˻�
     */
    sCmSeqNo = CMP_HEADER_SEQ_NO(&aProtocolContext->mReadHeader);

    IDE_TEST_RAISE(sCmSeqNo != aProtocolContext->mCmSeqNo, InvalidProtocolSeqNo);

    /*
     * Next Sequence ����
     */
    if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader) == ID_TRUE)
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
    IDE_TEST(cmiGetModule(&aProtocolContext->mReadHeader,
                          &aProtocolContext->mModule) != IDE_SUCCESS);

    /*
     * ReadHeader�κ��� WriteHeader�� �ʿ��� ������ ȹ��
     */
    aProtocolContext->mWriteHeader.mA5.mModuleID        = aProtocolContext->mReadHeader.mA5.mModuleID;
    aProtocolContext->mWriteHeader.mA5.mModuleVersion   = aProtocolContext->mReadHeader.mA5.mModuleVersion;
    aProtocolContext->mWriteHeader.mA5.mSourceSessionID = aProtocolContext->mReadHeader.mA5.mTargetSessionID;
    aProtocolContext->mWriteHeader.mA5.mTargetSessionID = aProtocolContext->mReadHeader.mA5.mSourceSessionID;

    return IDE_SUCCESS;

    IDE_EXCEPTION(Disconnected);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION(InvalidProtocolSeqNo);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Block�� �����Ѵ�.
 */
static IDE_RC cmiWriteBlock(cmiProtocolContext *aProtocolContext, idBool aIsEnd, PDL_Time_Value *aTimeout = NULL)
{
    cmnLinkPeer *sLink   = aProtocolContext->mLink;
    cmbBlock    *sBlock  = aProtocolContext->mWriteBlock;
    cmpHeader   *sHeader = &aProtocolContext->mWriteHeader;
    cmbBlock    *sPendingBlock;
    iduListNode *sIterator;
    iduListNode *sNodeNext;
    idBool       sSendSuccess;
    UInt         sSendDataSize = 0;

    // bug-27250 IPC linklist can be crushed.
    PDL_Time_Value      sWaitTime;
    cmnDispatcherImpl   sImpl;

    /*
     * �������� ���̶�� Sequence ���� ����
     */
    if (aIsEnd == ID_TRUE)
    {
        CMP_HEADER_SET_PROTO_END(sHeader);

        if (aProtocolContext->mIsAddReadBlock == ID_FALSE)
        {
            if (aProtocolContext->mReadBlock != NULL)
            {
                aProtocolContext->mIsAddReadBlock = ID_TRUE;

                /*
                 * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
                 */
                IDU_LIST_ADD_LAST(&aProtocolContext->mReadBlockList,
                                  &aProtocolContext->mReadBlock->mListNode);

                aProtocolContext->mReadBlock = NULL;
                IDE_TEST(cmiFreeReadBlock(aProtocolContext) != IDE_SUCCESS);
            }
        }
    }

    /*
     * Protocol Header ���
     */
    IDE_TEST(cmpHeaderWrite(sHeader, sBlock) != IDE_SUCCESS);

    /*
     * Pending Write Block���� ����
     */
    IDU_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
    {
        sPendingBlock = (cmbBlock *)sIterator->mObj;

        sSendSuccess = ID_TRUE;

        sSendDataSize = sPendingBlock->mDataSize;
        // BUG-19465 : CM_Buffer�� pending list�� ����
        while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != IDE_SUCCESS)
        {
            sSendSuccess = ID_FALSE;

            IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);

            /* BUG-44468 [cm] codesonar warning in CM */
            if( aProtocolContext->mListLength <= gMaxPendingList )
            {
                break;
            }

            //BUG-30338 
            sImpl = cmnDispatcherImplForLinkImpl(((cmnLink*)sLink)->mImpl);
            if (aTimeout != NULL)
            {
                IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                                CMN_DIRECTION_WR, 
                                                aTimeout) != IDE_SUCCESS);
            } 
            else 
            {
                // Timeout is null.
                if (sImpl == CMI_DISPATCHER_IMPL_TCP)
                {
                    //BUG-34070
                    //In case of using TCP to communicate clients, 
                    //the write sockets should not be blocked forever.
                    sWaitTime.set(cmuProperty::getSockWriteTimeout(), 0);

                    IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                                    CMN_DIRECTION_WR, 
                                                    &sWaitTime) != IDE_SUCCESS);
                }
                else
                {
                    IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink, 
                                                    CMN_DIRECTION_WR,
                                                    NULL) != IDE_SUCCESS);
                }
            }

            sSendSuccess = ID_TRUE;
        }

        if (sSendSuccess == ID_FALSE)
        {
            break;
        }
        
        /* BUG-45184 */
        aProtocolContext->mSendDataSize += sSendDataSize;
        aProtocolContext->mSendDataCount++;
            
        aProtocolContext->mListLength--;
    }

    /*
     * Pending Write Block�� ������ ���� Block ����
     */
    if (sIterator == &aProtocolContext->mWriteBlockList)
    {
        sSendDataSize = sBlock->mDataSize;
        if (sLink->mPeerOp->mSend(sLink, sBlock) != IDE_SUCCESS)
        {
            IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);

            IDU_LIST_ADD_LAST(&aProtocolContext->mWriteBlockList, &sBlock->mListNode);
            aProtocolContext->mListLength++;
        }
        
        /* BUG-45184 */
        aProtocolContext->mSendDataSize += sSendDataSize;
        aProtocolContext->mSendDataCount++;
    }
    else
    {
        IDU_LIST_ADD_LAST(&aProtocolContext->mWriteBlockList, &sBlock->mListNode);
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
    if (aIsEnd == ID_TRUE)
    {
        aProtocolContext->mWriteHeader.mA5.mCmSeqNo = 0;

        /*
         * �������� ���̶�� ��� Block�� ���۵Ǿ�� ��
         */
        IDU_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
        {
            sPendingBlock = (cmbBlock *)sIterator->mObj;

            sSendDataSize = sPendingBlock->mDataSize;
            while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != IDE_SUCCESS)
            {
                IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);
                
                //BUG-30338
                sImpl = cmnDispatcherImplForLinkImpl(((cmnLink*)sLink)->mImpl);
                if (aTimeout != NULL)
                {
                    IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink, 
                                                    CMN_DIRECTION_WR, 
                                                    &sWaitTime) != IDE_SUCCESS);
                }
                else
                {
                    //BUG-34070
                    if (sImpl == CMI_DISPATCHER_IMPL_TCP)
                    {
                        sWaitTime.set(cmuProperty::getSockWriteTimeout(), 0);
                        IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink, 
                                                        CMN_DIRECTION_WR, 
                                                        &sWaitTime) != IDE_SUCCESS);
                    }
                    else
                    {
                        IDE_TEST(cmnDispatcherWaitLink((cmiLink *)sLink,
                                                        CMN_DIRECTION_WR, 
                                                        NULL) != IDE_SUCCESS);
                    }
                }
            }
            
            /* BUG-45184 */
            aProtocolContext->mSendDataSize += sSendDataSize;
            aProtocolContext->mSendDataCount++;
        }

        sLink->mPeerOp->mReqComplete(sLink);
    }
    else
    {
        aProtocolContext->mWriteHeader.mA5.mCmSeqNo++;
    }

    return IDE_SUCCESS;

    // bug-27250 IPC linklist can be crushed.
    // ��� ������ ���Ͽ� pending block�� ������ �����ϵ��� ����.
    // sendfail�� empty�� ���ܵ�.
    IDE_EXCEPTION(SendFail);
    {
    }
    IDE_EXCEPTION_END;
    {
        /* BUG-44468 [cm] codesonar warning in CM */
        if ( ( sBlock != NULL ) && ( aProtocolContext->mListLength > gMaxPendingList ) )
        {
            aProtocolContext->mResendBlock = ID_FALSE;
        }
        else
        {
            /* do nothing */
        }

        if ( aProtocolContext->mResendBlock == ID_TRUE )
        {
            if ( sBlock != NULL )
            {
                /*timeout error�� ��쿡 ���߿� ���� �� �ֵ��� pending block�� �����Ѵ�.*/
                IDU_LIST_ADD_LAST( &aProtocolContext->mWriteBlockList, &sBlock->mListNode );
                aProtocolContext->mListLength++;

                /*
                 * Protocol Context�� Write Block ����
                 */
                sBlock                        = NULL;
                aProtocolContext->mWriteBlock = NULL;
                if ( aIsEnd == ID_TRUE )
                {
                    aProtocolContext->mWriteHeader.mA5.mCmSeqNo = 0;
                }
                else
                {
                    aProtocolContext->mWriteHeader.mA5.mCmSeqNo++;
                } 
            }
            else
            {
                /* do nothing */
            }
        }
        else
        {
            IDU_LIST_ITERATE_SAFE(&aProtocolContext->mWriteBlockList, sIterator, sNodeNext)
            {
                sPendingBlock = (cmbBlock *)sIterator->mObj;

                IDE_ASSERT(sLink->mPeerOp->mFreeBlock(sLink, sPendingBlock) == IDE_SUCCESS);
            }

            if (sBlock != NULL)
            {
                IDE_ASSERT(sLink->mPeerOp->mFreeBlock(sLink, sBlock) == IDE_SUCCESS);
            }

            aProtocolContext->mWriteBlock               = NULL;
            aProtocolContext->mWriteHeader.mA5.mCmSeqNo = 0;
        }
    }

    return IDE_FAILURE;
}


/*
 * Protocol�� �о�´�.
 */
static IDE_RC cmiReadProtocolInternal(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      PDL_Time_Value     *aTimeout)
{
    cmpMarshalFunction sMarshalFunction;
    UChar              sOpID;

    /*
     * Operation ID ����
     */
    CMB_BLOCK_READ_BYTE1(aProtocolContext->mReadBlock, &sOpID);

    /*
     * ���������� ó������ �о�� �ϴ� ��Ȳ
     */
    if (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) == ID_TRUE)
    {
        /*
         * Operation ID �˻�
         */
        IDE_TEST_RAISE(sOpID >= aProtocolContext->mModule->mOpMaxA5, InvalidOpError);

        /*
         * Protocol �ʱ�ȭ
         */
        //fix BUG-17947.
        IDE_TEST(cmiInitializeProtocol(aProtocol,
                                       aProtocolContext->mModule,
                                       sOpID) != IDE_SUCCESS);
    }
    else
    {
        /*
         * ���������� ���ӵǴ� ��� �������� OpID�� ������ �˻�
         */
        IDE_TEST_RAISE(sOpID != aProtocol->mOpID, InvalidProtocolSeqNo);
    }

    /*
     * Get Marshal Function
     */
    sMarshalFunction = aProtocolContext->mModule->mReadFunction[sOpID];

    /*
     * Marshal Protocol
     */
    IDE_TEST(sMarshalFunction(aProtocolContext->mReadBlock,
                              aProtocol,
                              &aProtocolContext->mMarshalState) != IDE_SUCCESS);

    /*
     * Protocol Marshal�� �Ϸ���� �ʾ����� ���� Block�� ��� �о�� �� ����
     */
    while (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) != ID_TRUE)
    {
        /*
         * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
         */
        IDU_LIST_ADD_LAST(&aProtocolContext->mReadBlockList,
                          &aProtocolContext->mReadBlock->mListNode);

        aProtocolContext->mReadBlock = NULL;

        /*
         * ���� Block�� �о��
         */
        IDE_TEST(cmiReadBlock(aProtocolContext, aTimeout) != IDE_SUCCESS);

        /*
         * Block���� Operation ID ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            CMB_BLOCK_READ_BYTE1(aProtocolContext->mReadBlock, &sOpID);

            IDE_TEST_RAISE(sOpID != aProtocol->mOpID, InvalidProtocolSeqNo);

            /*
             * Marshal Protocol
             */
            IDE_TEST(sMarshalFunction(aProtocolContext->mReadBlock,
                                      aProtocol,
                                      &aProtocolContext->mMarshalState) != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidOpError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    IDE_EXCEPTION(InvalidProtocolSeqNo);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC cmiDispatcherWaitLink( cmiLink             * aLink,
                                     cmiDirection          aDirection,
                                     PDL_Time_Value      * aTimeout )
{
    cmnDispatcherImpl   sImpl;
    PDL_Time_Value      sWaitTime;
    PDL_Time_Value    * sWaitTimePtr = NULL;

    // ������: all timeout NULL, ������: 1 msec for IPC
    // IPC�� ��� ���Ѵ���ϸ� �ȵȴ�.
    sImpl = cmnDispatcherImplForLinkImpl( aLink->mImpl );
    if ( aTimeout != NULL )
    {
        sWaitTimePtr = aTimeout;
    }
    else
    {
        if ( sImpl == CMI_DISPATCHER_IMPL_IPC )
        {
            sWaitTime.set( 0, 1000 );
            sWaitTimePtr = &sWaitTime;
        }
        else
        {
            sWaitTimePtr = NULL;
        }
    }

    IDE_TEST( cmnDispatcherWaitLink( (cmiLink *)aLink,
                                     aDirection,
                                     sWaitTimePtr ) 
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiInitialize( UInt   aCmMaxPendingList )
{
    cmbPool *sPoolLocal;

    // Property loading 
    IDE_TEST(cmuProperty::load() != IDE_SUCCESS);

    //BUG-21080   
    idlOS::pthread_once(&gCMInitOnce, cmiInitializeOnce);

    IDE_ASSERT(idlOS::thread_mutex_lock(&gCMInitMutex) == 0);

    IDE_TEST(gCMInitCount < 0);

    if (gCMInitCount == 0)
    {         
        
        /*
        * Shared Pool ���� �� ���
        */
        //fix BUG-17864.
        IDE_TEST(cmbPoolAlloc(&sPoolLocal, CMB_POOL_IMPL_LOCAL,CMB_BLOCK_DEFAULT_SIZE,0) != IDE_SUCCESS);
        IDE_TEST(cmbPoolSetSharedPool(sPoolLocal, CMB_POOL_IMPL_LOCAL) != IDE_SUCCESS);
    
    // fix BUG-18848
    #if !defined(CM_DISABLE_IPC)
        cmbPool *sPoolIPC;
    
        IDE_TEST(cmbPoolAlloc(&sPoolIPC, CMB_POOL_IMPL_IPC,CMB_BLOCK_DEFAULT_SIZE,0) != IDE_SUCCESS);
        IDE_TEST(cmbPoolSetSharedPool(sPoolIPC, CMB_POOL_IMPL_IPC) != IDE_SUCCESS);
    
        /*
        * IPC Mutex �ʱ�ȭ
        */
        IDE_TEST(cmbShmInitializeStatic() != IDE_SUCCESS);
    #endif

    #if !defined(CM_DISABLE_IPCDA)
        /*
         * IPC Mutex �ʱ�ȭ
         */
        IDE_TEST(cmbShmIPCDAInitializeStatic() != IDE_SUCCESS);
    #endif

        /*
        * cmmSession �ʱ�ȭ
        */
        IDE_TEST(cmmSessionInitializeStatic() != IDE_SUCCESS);
    
        /*
        * cmtVariable Piece Pool �ʱ�ȭ
        */
        IDE_TEST(cmtVariableInitializeStatic() != IDE_SUCCESS);
    
        /*
        * cmpModule �ʱ�ȭ
        */
        IDE_TEST(cmpModuleInitializeStatic() != IDE_SUCCESS);
    
        /*
        * DB Protocol ������� �ʱ�ȭ
        */
        idlOS::memset( gDBProtocolStat, 0x00, ID_SIZEOF(ULong) * CMP_OP_DB_MAX );
    
        // BUG-19465 : CM_Buffer�� pending list�� ����
        gMaxPendingList = aCmMaxPendingList;

        /* BUG-38951 Support to choice a type of CM dispatcher on run-time */
        IDE_TEST(cmnDispatcherInitialize() != IDE_SUCCESS);
    }
    
    gCMInitCount++;

    /* PROJ-2681 */
    (void)aciRegistErrorFromBuffer(ACI_E_MODULE_CM,
                                   aciVersionID,
                                   ID_SIZEOF(gCmErrorFactory) / ID_SIZEOF(gCmErrorFactory[0]), /* count */
                                   (acp_char_t **)&gCmErrorFactory);

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCMInitMutex) == 0);
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        gCMInitCount = -1;

        IDE_ASSERT(idlOS::thread_mutex_unlock(&gCMInitMutex) == 0);
          
    }         

    return IDE_FAILURE;
}

IDE_RC cmiFinalize()
{
    cmbPool *sPoolLocal;

    //BUG-21080 
    IDE_ASSERT(idlOS::thread_mutex_lock(&gCMInitMutex) == 0);

    IDE_TEST(gCMInitCount < 0);

    gCMInitCount--;

    if (gCMInitCount == 0)
    {
        /*
        * cmpModule ����
        */
        IDE_TEST(cmpModuleFinalizeStatic() != IDE_SUCCESS);
    
        /*
        * cmtVariable Piece Pool ����
        */
        IDE_TEST(cmtVariableFinalizeStatic() != IDE_SUCCESS);
    
        /*
        * cmmSession ����
        */
        IDE_TEST(cmmSessionFinalizeStatic() != IDE_SUCCESS);
    
    // fix BUG-18848
    #if !defined(CM_DISABLE_IPC)
        cmbPool *sPoolIPC;
    
        /* IPC Mutex ���� */
        IDE_TEST(cmbShmFinalizeStatic() != IDE_SUCCESS);
    
        /* Shared Pool ���� */
        IDE_TEST(cmbPoolGetSharedPool(&sPoolIPC, CMB_POOL_IMPL_IPC) != IDE_SUCCESS);
        IDE_TEST(cmbPoolFree(sPoolIPC) != IDE_SUCCESS);

        /* Shared Memory ���� */
        IDE_TEST(cmbShmDestroy() != IDE_SUCCESS);
    #endif

    #if !defined(CM_DISABLE_IPCDA)
        /* IPCDA Mutex ���� */
        IDE_TEST(cmbShmIPCDAFinalizeStatic() != IDE_SUCCESS);

        /* Shared Memory ���� */
         IDE_TEST(cmbShmIPCDADestroy() != IDE_SUCCESS);
    #endif

        IDE_TEST(cmbPoolGetSharedPool(&sPoolLocal, CMB_POOL_IMPL_LOCAL) != IDE_SUCCESS);
        IDE_TEST(cmbPoolFree(sPoolLocal) != IDE_SUCCESS);
    }

    IDE_ASSERT(idlOS::thread_mutex_unlock(&gCMInitMutex) == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        gCMInitCount = -1;

        IDE_ASSERT(idlOS::thread_mutex_unlock(&gCMInitMutex) == 0);
    }

    return IDE_FAILURE;
}

IDE_RC cmiSetCallback(UChar aModuleID, UChar aOpID, cmiCallbackFunction aCallbackFunction)
{
    /*
     * Module ID �˻�
     */
    IDE_TEST_RAISE((aModuleID == CMP_MODULE_BASE) ||
                   (aModuleID >= CMP_MODULE_MAX), InvalidModule);

    /*
     * Operation ID �˻�
     */
    IDE_TEST_RAISE(aOpID >= gCmpModule[aModuleID]->mOpMax, InvalidOperation);

    /*
     * Callback Function ����
     */
    if (aCallbackFunction == NULL)
    {
        gCmpModule[aModuleID]->mCallbackFunction[aOpID] = cmpCallbackNULL;
    }
    else
    {
        gCmpModule[aModuleID]->mCallbackFunction[aOpID] = aCallbackFunction;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidModule);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_MODULE));
    }
    IDE_EXCEPTION(InvalidOperation);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool cmiIsSupportedLinkImpl(cmiLinkImpl aLinkImpl)
{
    return cmnLinkIsSupportedImpl(aLinkImpl);
}

idBool cmiIsSupportedDispatcherImpl(cmiDispatcherImpl aDispatcherImpl)
{
    return cmnDispatcherIsSupportedImpl(aDispatcherImpl);
}

IDE_RC cmiAllocLink(cmiLink **aLink, cmiLinkType aType, cmiLinkImpl aImpl)
{
    /*
     * Link �Ҵ�
     */
    IDE_TEST(cmnLinkAlloc(aLink, aType, aImpl) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiFreeLink(cmiLink *aLink)
{
    /*
     * Link ����
     */
    IDE_TEST(cmnLinkFree(aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiCloseLink(cmiLink *aLink)
{
    /*
     * Link Close
     */
    IDE_TEST(aLink->mOp->mClose(aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiWaitLink(cmiLink *aLink, PDL_Time_Value *aTimeout)
{
    return cmnDispatcherWaitLink(aLink, CMI_DIRECTION_RD, aTimeout);
}

UInt cmiGetLinkFeature(cmiLink *aLink)
{
    return aLink->mFeature;
}

IDE_RC cmiListenLink(cmiLink *aLink, cmiListenArg *aListenArg)
{
    cmnLinkListen *sLink = (cmnLinkListen *)aLink;

    /*
     * Listen Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_LISTEN);

    /*
     * listen
     */
    IDE_TEST(sLink->mListenOp->mListen(sLink, aListenArg) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiAcceptLink(cmiLink *aLinkListen, cmiLink **aLinkPeer)
{
    cmnLinkListen *sLinkListen = (cmnLinkListen *)aLinkListen;
    cmnLinkPeer   *sLinkPeer   = NULL;

    /*
     * Listen Type �˻�
     */
    IDE_ASSERT(aLinkListen->mType == CMN_LINK_TYPE_LISTEN);

    /*
     * accept
     */
    IDE_TEST(sLinkListen->mListenOp->mAccept(sLinkListen, &sLinkPeer) != IDE_SUCCESS);

    /*
     * accept�� Link ��ȯ
     */
    *aLinkPeer = (cmiLink *)sLinkPeer;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiAllocChannel(cmiLink *aLink, SInt *aChannelID)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER);

    /*
     * Alloc Channel for IPC
     */
    IDE_TEST(sLink->mPeerOp->mAllocChannel(sLink, aChannelID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiHandshake(cmiLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Handshake
     */
    IDE_TEST(sLink->mPeerOp->mHandshake(sLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSetLinkBlockingMode(cmiLink *aLink, idBool aBlockingMode)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Set Blocking Mode
     */
    IDE_TEST(sLink->mPeerOp->mSetBlockingMode(sLink, aBlockingMode) != IDE_SUCCESS);

    if (aBlockingMode == ID_FALSE)
    {
        aLink->mFeature |= CMN_LINK_FLAG_NONBLOCK;
    }
    else
    {
        aLink->mFeature &= ~CMN_LINK_FLAG_NONBLOCK;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool cmiIsLinkBlockingMode( cmiLink * aLink )
{
    idBool  sIsBlockingMode = ID_TRUE;

    if ( ( aLink->mFeature & CMN_LINK_FLAG_NONBLOCK ) != CMN_LINK_FLAG_NONBLOCK )
    {
        sIsBlockingMode = ID_TRUE;
    }
    else
    {
        sIsBlockingMode = ID_FALSE;
    }

    return sIsBlockingMode;
    
}

IDE_RC cmiGetLinkInfo(cmiLink *aLink, SChar *aBuf, UInt aBufLen, cmiLinkInfoKey aKey)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Get Info
     */
    return sLink->mPeerOp->mGetInfo(sLink, aBuf, aBufLen, aKey);
}

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
IDE_RC cmiGetLinkSndBufSize(cmiLink *aLink, SInt *aSndBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mGetSndBufSize != NULL)
    {
        IDE_TEST(sLink->mPeerOp->mGetSndBufSize(sLink, aSndBufSize) != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSetLinkSndBufSize(cmiLink *aLink, SInt aSndBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mSetSndBufSize != NULL)
    {
        IDE_TEST(sLink->mPeerOp->mSetSndBufSize(sLink, aSndBufSize) != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiGetLinkRcvBufSize(cmiLink *aLink, SInt *aRcvBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mGetRcvBufSize != NULL)
    {
        IDE_TEST(sLink->mPeerOp->mGetRcvBufSize(sLink, aRcvBufSize) != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSetLinkRcvBufSize(cmiLink *aLink, SInt aRcvBufSize)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    if (sLink->mPeerOp->mSetRcvBufSize != NULL)
    {
        IDE_TEST(sLink->mPeerOp->mSetRcvBufSize(sLink, aRcvBufSize) != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiCheckLink(cmiLink *aLink, idBool *aIsClosed)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Connection Closed �˻�
     */
    return sLink->mPeerOp->mCheck(sLink, aIsClosed);
}

/* PROJ-2474 SSL/TLS */
IDE_RC cmiGetPeerCertIssuer(cmnLink *aLink,
                            SChar *aBuf,
                            UInt aBufLen)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    IDE_ASSERT( (aLink->mType == CMN_LINK_TYPE_PEER_SERVER) ||
                (aLink->mType == CMN_LINK_TYPE_PEER_CLIENT) );

    if ( aLink->mImpl == CMN_LINK_IMPL_SSL )
    {
        return sLink->mPeerOp->mGetSslPeerCertIssuer(sLink, aBuf, aBufLen);
    }
    else
    {
        aBuf[0] = '\0';
        aBufLen = 0;
    }

    return IDE_SUCCESS;

}

IDE_RC cmiGetPeerCertSubject(cmnLink *aLink,
                             SChar *aBuf,
                             UInt aBufLen)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    IDE_ASSERT( (aLink->mType == CMN_LINK_TYPE_PEER_SERVER) ||
                (aLink->mType == CMN_LINK_TYPE_PEER_CLIENT) );


    if ( aLink->mImpl == CMN_LINK_IMPL_SSL )
    {
        return sLink->mPeerOp->mGetSslPeerCertSubject(sLink, aBuf, aBufLen);
    }
    else
    {
        aBuf[0] = '\0';
        aBufLen = 0;
    }

    return IDE_SUCCESS;
}

IDE_RC cmiGetSslCipherInfo(cmnLink *aLink,
                           SChar *aBuf,
                           UInt aBufLen)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    IDE_ASSERT( (aLink->mType == CMN_LINK_TYPE_PEER_SERVER) ||
                (aLink->mType == CMN_LINK_TYPE_PEER_CLIENT) );

    if ( aLink->mImpl == CMN_LINK_IMPL_SSL )
    {
        return sLink->mPeerOp->mGetSslCipher(sLink, aBuf, aBufLen);
    }
    else
    {
        aBuf[0] = '\0';
        aBufLen = 0;
    }

    return IDE_SUCCESS;
}

/* only called in db-link */
idBool cmiLinkHasPendingRequest(cmiLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    /*
     * Pending Request ���� ���� ����
     */
    return sLink->mPeerOp->mHasPendingRequest(sLink);
}

idvSession *cmiGetLinkStatistics(cmiLink *aLink)
{
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    return cmnLinkPeerGetStatistics((cmnLinkPeer *)aLink);
}

void cmiSetLinkStatistics(cmiLink *aLink, idvSession *aStatistics)
{
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    cmnLinkPeerSetStatistics((cmnLinkPeer *)aLink, aStatistics);
}

void *cmiGetLinkUserPtr(cmiLink *aLink)
{
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    return cmnLinkPeerGetUserPtr((cmnLinkPeer *)aLink);
}

void cmiSetLinkUserPtr(cmiLink *aLink, void *aUserPtr)
{
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    cmnLinkPeerSetUserPtr((cmnLinkPeer *)aLink, aUserPtr);
}

IDE_RC cmiShutdownLink(cmiLink *aLink, cmiDirection aDirection)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER ||
               aLink->mType == CMN_LINK_TYPE_PEER_CLIENT);

    // bug-28277 ipc: server stop failed when idle clis exist
    // server stop�ÿ��� shutdown_mode_force �ѱ⵵�� ��.
    IDE_TEST(sLink->mPeerOp->mShutdown(sLink, aDirection,
                                       CMN_SHUTDOWN_MODE_NORMAL)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// bug-28227: ipc: server stop failed when idle cli exists
// server stop�� mmtSessionManager::shutdown() ���� �����Լ� ȣ��.
// IPC�� ���ؼ��� shutdown_mode_force�� �����Ͽ� shutdown ȣ��.
IDE_RC cmiShutdownLinkForce(cmiLink *aLink)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    if (((aLink->mImpl == CMN_LINK_IMPL_IPC) ||
         (aLink->mImpl == CMN_LINK_IMPL_IPCDA)) &&
        (aLink->mType == CMN_LINK_TYPE_PEER_SERVER))
    {
        (void)sLink->mPeerOp->mShutdown(sLink, CMI_DIRECTION_RD,
                                        CMN_SHUTDOWN_MODE_FORCE);
    }
    return IDE_SUCCESS;
}

cmiDispatcherImpl cmiDispatcherImplForLinkImpl(cmiLinkImpl aLinkImpl)
{
    return cmnDispatcherImplForLinkImpl(aLinkImpl);
}

cmiDispatcherImpl cmiDispatcherImplForLink(cmiLink *aLink)
{
    return cmnDispatcherImplForLinkImpl(aLink->mImpl);
}

IDE_RC cmiAllocDispatcher(cmiDispatcher **aDispatcher, cmiDispatcherImpl aImpl, UInt aMaxLink)
{
    /*
     * Dispatcher �Ҵ�
     */
    IDE_TEST(cmnDispatcherAlloc(aDispatcher, aImpl, aMaxLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiFreeDispatcher(cmiDispatcher *aDispatcher)
{
    /*
     * Dispatcher ����
     */
    IDE_TEST(cmnDispatcherFree(aDispatcher) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiAddLinkToDispatcher(cmiDispatcher *aDispatcher, cmiLink *aLink)
{
    /*
     * Dispatcher�� Link �߰�
     */
    IDE_TEST(aDispatcher->mOp->mAddLink(aDispatcher, aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiRemoveLinkFromDispatcher(cmiDispatcher *aDispatcher, cmiLink *aLink)
{
    /*
     * Dispatcher�� Link ����
     */
    IDE_TEST(aDispatcher->mOp->mRemoveLink(aDispatcher, aLink) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiRemoveAllLinksFromDispatcher(cmiDispatcher *aDispatcher)
{
    /*
     * Dispatcher���� ��� Link ����
     */
    IDE_TEST(aDispatcher->mOp->mRemoveAllLinks(aDispatcher) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSelectDispatcher(cmiDispatcher  *aDispatcher,
                           iduList        *aReadyList,
                           UInt           *aReadyCount,
                           PDL_Time_Value *aTimeout)
{
    /*
     * select
     */
    IDE_TEST(aDispatcher->mOp->mSelect(aDispatcher,
                                       aReadyList,
                                       aReadyCount,
                                       aTimeout) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiAddSession(cmiSession         *aSession,
                     void               *aOwner,
                     UChar               aModuleID,
                     cmiProtocolContext */* aProtocolContext */)
{
    /*
     * �Ķ���� ���� �˻�
     */
    IDE_ASSERT(aModuleID > CMP_MODULE_BASE);

    IDE_TEST_RAISE(aModuleID >= CMP_MODULE_MAX, UnknownModule);

    /*
     * Session �߰�
     */
    IDE_TEST(cmmSessionAdd(aSession) != IDE_SUCCESS);

    /*
     * Session �ʱ�ȭ
     */
    aSession->mOwner       = aOwner;
    aSession->mBaseVersion = CMP_VER_BASE_NONE;

    aSession->mLink           = NULL;
    aSession->mCounterpartID  = 0;
    aSession->mModuleID       = aModuleID;
    aSession->mModuleVersion  = CMP_VER_BASE_NONE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(UnknownModule);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_UNKNOWN_MODULE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiRemoveSession(cmiSession *aSession)
{
    /*
     * Session�� Session ID�� 0�̸� ��ϵ��� ���� Session
     */
    IDE_TEST_RAISE(aSession->mSessionID == 0, SessionNotAdded);

    /*
     * Session ����
     */
    IDE_TEST(cmmSessionRemove(aSession) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SessionNotAdded);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SESSION_NOT_ADDED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSetLinkForSession(cmiSession *aSession, cmiLink *aLink)
{
    if (aLink != NULL)
    {
        /*
         * Session�� Link�� �̹� ��ϵ� ���¿��� ���ο� Link�� ������ �� ����
         */
        IDE_TEST_RAISE(aSession->mLink != NULL, LinkAlreadyRegistered);

        /*
         * Link�� Peer Type���� �˻�
         */
        //BUG-28119 for RP PBT
        IDE_TEST_RAISE((aLink->mType != CMN_LINK_TYPE_PEER_CLIENT) && 
                       (aLink->mType != CMN_LINK_TYPE_PEER_SERVER), InvalidLinkType);
    }

    /*
     * Session�� Link ����
     */
    aSession->mLink = (cmnLinkPeer *)aLink;

    return IDE_SUCCESS;

    IDE_EXCEPTION(LinkAlreadyRegistered);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_LINK_ALREADY_REGISTERED));
    }
    IDE_EXCEPTION(InvalidLinkType);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_LINK_TYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiGetLinkForSession(cmiSession *aSession, cmiLink **aLink)
{
    /*
     * Session�� Link ��ȯ
     */
    *aLink = (cmiLink *)aSession->mLink;

    return IDE_SUCCESS;
}

IDE_RC cmiSetOwnerForSession(cmiSession *aSession, void *aOwner)
{
    /*
     * Session�� Owner ����
     */
    aSession->mOwner = aOwner;

    return IDE_SUCCESS;
}

IDE_RC cmiGetOwnerForSession(cmiSession *aSession, void **aOwner)
{
    /*
     * Session�� Owner ��ȯ
     */
    *aOwner = aSession->mOwner;

    return IDE_SUCCESS;
}

IDE_RC cmiSetOwnerForProtocolContext( cmiProtocolContext *aCtx, void *aOwner )
{
    /*
     * ProtocolContext�� Owner ����
     */
    aCtx->mOwner = aOwner;

    return IDE_SUCCESS;
}

IDE_RC cmiGetOwnerForProtocolContext( cmiProtocolContext *aCtx, void **aOwner )
{
    /*
     * ProtocolContext�� Owner ��ȯ
     */
    *aOwner = aCtx->mOwner;

    return IDE_SUCCESS;
}

IDE_RC cmiGetLinkForProtocolContext( cmiProtocolContext *aCtx, cmiLink **aLink )
{
    /*
     * ProtocolContext�� Link ��ȯ
     */
    *aLink = (cmiLink *)(aCtx->mLink);

    return IDE_SUCCESS;
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
IDE_RC cmiConnect(cmiProtocolContext *aCtx, cmiConnectArg *aConnectArg, PDL_Time_Value *aTimeout, SInt aOption)
{
    idBool                     sConnectFlag      = ID_FALSE;
    UChar                      sOpID;

    UInt                       sErrIndex;
    UInt                       sErrCode;
    UShort                     sErrMsgLen;
    SChar                      sErrMsg[MAX_ERROR_MSG_LEN]; // 2048

    IDE_TEST_RAISE(aCtx->mModule->mModuleID != CMP_MODULE_DB,
                   InvalidModuleError);
    
    IDU_FIT_POINT( "cmiConnect::Connect" );

    /* Link Connect */
    IDE_TEST(aCtx->mLink->mPeerOp->mConnect(aCtx->mLink,
                                            aConnectArg,
                                            aTimeout,
                                            aOption) != IDE_SUCCESS);
    sConnectFlag = ID_TRUE;
    // STF�� ��� ������ ���⼭ �ٽ� �ʱ�ȭ���������
    aCtx->mWriteHeader.mA7.mCmSeqNo = 0; // send seq
    aCtx->mCmSeqNo = 0;                  // recv seq

    IDE_TEST(aCtx->mLink->mPeerOp->mHandshake(aCtx->mLink) != IDE_SUCCESS);

    CMI_WR1(aCtx, CMP_OP_DB_Handshake);
    CMI_WR1(aCtx, aCtx->mModule->mModuleID); // DB: 1, RP: 2
    CMI_WR1(aCtx, CM_MAJOR_VERSION);
    CMI_WR1(aCtx, CM_MINOR_VERSION);
    CMI_WR1(aCtx, CM_PATCH_VERSION);
    CMI_WR1(aCtx, 0);
    IDE_TEST( cmiSend(aCtx, ID_TRUE) != IDE_SUCCESS);

    //fix BUG-17942
    // cmiRecvNext() ��ſ� cmiRecv()�� ȣ���Ѵ�
    // DB_HandshakeResult�� ���� callback�� �������� ����
    IDE_TEST(cmiRecvNext(aCtx, aTimeout) != IDE_SUCCESS);
    CMI_RD1(aCtx, sOpID);
    if (sOpID != CMP_OP_DB_HandshakeResult)
    {
        if (sOpID == CMP_OP_DB_ErrorResult)
        {
            IDE_RAISE(HandshakeError);
        }
        else
        {
            IDE_RAISE(InvalidProtocolSeqError);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(HandshakeError);
    {
        CMI_SKIP_READ_BLOCK(aCtx, 1); /* skip error op ID */
        /* BUG-44556  Handshake �����߿� �߻��� ������ �������� �ؼ��� �߸��Ǿ����ϴ�.*/
        CMI_RD4(aCtx, &sErrIndex);
        CMI_RD4(aCtx, &sErrCode);
        CMI_RD2(aCtx, &sErrMsgLen);
        if (sErrMsgLen >= MAX_ERROR_MSG_LEN)
        {
            sErrMsgLen = MAX_ERROR_MSG_LEN - 1;
        }
        CMI_RCP(aCtx, sErrMsg, sErrMsgLen);
        sErrMsg[sErrMsgLen] = '\0';
        IDE_SET(ideSetErrorCodeAndMsg(sErrCode, sErrMsg));
    }
    IDE_EXCEPTION(InvalidModuleError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_MODULE));
    }
    IDE_EXCEPTION(InvalidProtocolSeqError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));
    }
    IDE_EXCEPTION_END;
    {
        IDE_PUSH();
        // BUG-24170 [CM] cmiConnect ���� ��, cmiConnect ������ close �ؾ� �մϴ�
        if(sConnectFlag == ID_TRUE)
        {
            (void)aCtx->mLink->mPeerOp->mShutdown(aCtx->mLink,
                                                  CMI_DIRECTION_RDWR,
                                                  CMN_SHUTDOWN_MODE_NORMAL);

            (void)aCtx->mLink->mLink.mOp->mClose(&aCtx->mLink->mLink);
        }
        IDE_POP();
    }

    return IDE_FAILURE;
}

// RP �������ݿ� (DB_Handshake ���� ����)
IDE_RC cmiConnectWithoutData( cmiProtocolContext * aCtx,
                              cmiConnectArg * aConnectArg,
                              PDL_Time_Value * aTimeout,
                              SInt aOption )
{
    idBool sConnectFlag = ID_FALSE;

    IDU_FIT_POINT( "cmiConnectWithoutData::Connect" );
    
    /* Link Connect */
    IDE_TEST(aCtx->mLink->mPeerOp->mConnect(aCtx->mLink,
                                            aConnectArg,
                                            aTimeout,
                                            aOption) != IDE_SUCCESS);
    sConnectFlag = ID_TRUE;

    IDE_TEST(aCtx->mLink->mPeerOp->mHandshake(aCtx->mLink));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    // BUG-24170 [CM] cmiConnect ���� ��, cmiConnect ������ close �ؾ� �մϴ�
    if(sConnectFlag == ID_TRUE)
    {
        (void)aCtx->mLink->mPeerOp->mShutdown(aCtx->mLink,
                                              CMI_DIRECTION_RDWR,
                                              CMN_SHUTDOWN_MODE_NORMAL);

        (void)aCtx->mLink->mLink.mOp->mClose(&aCtx->mLink->mLink);
    }
    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC cmiInitializeProtocol(cmiProtocol *aProtocol, cmpModule*  aModule, UChar aOperationID)
{
    //fix BUG-17947.
    /*
     * Operation ID ����
     */
    aProtocol->mOpID = aOperationID;
    aProtocol->mClientLastOpID = 0;  /* PROJ-2733-Protocol Unused at A5 */

    /*
     * Protocol Finalize �Լ� ����
     */
    aProtocol->mFinalizeFunction = (void *)aModule->mArgFinalizeFunction[aOperationID];

    /*
     * Protocol �ʱ�ȭ
     */
    if (aModule->mArgInitializeFunction[aOperationID] != cmpArgNULL)
    {
        IDE_TEST_RAISE((aModule->mArgInitializeFunction[aOperationID])(aProtocol) != IDE_SUCCESS,
                       InitializeFail);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InitializeFail);
    {
        aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*fix BUG-30041 cmiReadProtocol���� mFinalization ���ʱ�ȭ �Ǳ�����
 �����ϴ� case�� cmiFinalization���� ����������˴ϴ�.*/
void  cmiInitializeProtocolNullFinalization(cmiProtocol *aProtocol)
{
    aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
}

IDE_RC cmiFinalizeProtocol(cmiProtocol *aProtocol)
{
    if (aProtocol->mFinalizeFunction != (void *)cmpArgNULL)
    {
        IDE_TEST(((cmpArgFunction)aProtocol->mFinalizeFunction)(aProtocol) != IDE_SUCCESS);

        aProtocol->mFinalizeFunction = (void *)cmpArgNULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-2296
 *
 * ����ȭ �������� ȣȯ�� ���� �߰���. ������ ���� ������
 * cmiProtocolContext�� �� �Լ��� �����.
 */ 
IDE_RC cmiInitializeProtocolContext( cmiProtocolContext * aCtx,
                                     UChar                aModuleID,
                                     cmiLink            * aLink,
                                     idBool               aResendBlock )
{
    aCtx->mModule         = gCmpModule[ aModuleID ];
    aCtx->mLink           = (cmnLinkPeer*)aLink;
    aCtx->mOwner          = NULL;
    aCtx->mIsAddReadBlock = ID_FALSE;
    aCtx->mSession        = NULL; // deprecated

    aCtx->mReadBlock      = NULL;
    aCtx->mWriteBlock     = NULL;
    /*PROJ-2616*/
    aCtx->mSimpleQueryFetchIPCDAWriteBlock.mData = NULL;

    cmpHeaderInitializeForA5( &aCtx->mWriteHeader );

    aCtx->mWriteHeader.mA5.mModuleID = aModuleID;
    
    CMP_MARSHAL_STATE_INITIALIZE( aCtx->mMarshalState );
    
    aCtx->mProtocol.mFinalizeFunction = (void *)cmpArgNULL;

    IDU_LIST_INIT( &aCtx->mReadBlockList );
    IDU_LIST_INIT( &aCtx->mWriteBlockList );

    aCtx->mListLength             = 0;
    aCtx->mCmSeqNo                = 0;
    aCtx->mIsDisconnect           = ID_FALSE;
    aCtx->mSessionCloseNeeded     = ID_FALSE;
    /*PROJ-2616*/
    aCtx->mIPDASpinMaxCount       = CMI_IPCDA_SPIN_MIN_LOOP;

    cmiDisableCompress( aCtx );
    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
    /* ���� ���� ����� default ���� LZO */ 
    cmiSetDecompressType( aCtx, CMI_COMPRESS_LZO );

    /* BUG-38102 */
    cmiDisableEncrypt( aCtx );

    /* 
     * BUG-38716 
     * [rp-sender] It needs a property to give sending timeout to replication sender. 
     */
    aCtx->mResendBlock = aResendBlock;

    aCtx->mSendDataSize           = 0;
    aCtx->mReceiveDataSize        = 0;
    aCtx->mSendDataCount          = 0;
    aCtx->mReceiveDataCount       = 0;

    aCtx->mProtocol.mClientLastOpID = 0;  /* PROJ-2733-Protocol Unused at RP  */
    
    return IDE_SUCCESS;
}

/*
 * PROJ-2296
 *
 * cmiInitializeProtocolContext()�� ������� Protocol Context�� �����Ѵ�.
 */
IDE_RC cmiFinalizeProtocolContext( cmiProtocolContext   * aProtocolContext )
{
    IDE_TEST_CONT(cmiGetLinkImpl(aProtocolContext) == CMN_LINK_IMPL_IPCDA,
                  ContFinPtcCtx);

    return cmiFreeCmBlock( aProtocolContext );
    
    IDE_EXCEPTION_CONT(ContFinPtcCtx);
    
    return IDE_SUCCESS;
}


void cmiSetProtocolContextLink(cmiProtocolContext *aProtocolContext, cmiLink *aLink)
{
    /*
     * Protocol Context�� Link ����
     */
    aProtocolContext->mLink = (cmnLinkPeer *)aLink;
}

/*
 * PROJ-2296 
 */ 
void cmiProtocolContextCopyA5Header( cmiProtocolContext * aCtx,
                                     cmpHeader          * aHeader )
{
    aCtx->mWriteHeader.mA5.mHeaderSign      = CMP_HEADER_SIGN_A5;
    aCtx->mWriteHeader.mA5.mModuleID        = aHeader->mA5.mModuleID;
    aCtx->mWriteHeader.mA5.mModuleVersion   = aHeader->mA5.mModuleVersion;
    aCtx->mWriteHeader.mA5.mSourceSessionID = aHeader->mA5.mTargetSessionID;
    aCtx->mWriteHeader.mA5.mTargetSessionID = aHeader->mA5.mSourceSessionID;
}


idBool cmiProtocolContextHasPendingRequest(cmiProtocolContext *aCtx)
{
    idBool sRet = ID_FALSE;

    /* proj_2160 cm_type removal: A7 uses static mReadBlock */
    if (aCtx->mLink->mLink.mPacketType != CMP_PACKET_TYPE_A5)
    {
        sRet = aCtx->mLink->mPeerOp->mHasPendingRequest(aCtx->mLink);
    }
    else
    {
        if (aCtx->mReadBlock != NULL)
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = aCtx->mLink->mPeerOp->mHasPendingRequest(aCtx->mLink);
        }
    }
    return sRet;
}

IDE_RC cmiReadProtocolAndCallback(cmiProtocolContext      *aProtocolContext,
                                  void                    *aUserContext,
                                  PDL_Time_Value          *aTimeout,
                                  void                    *aTask)
{
    cmpCallbackFunction  sCallbackFunction;
    IDE_RC               sRet = IDE_SUCCESS;
    cmnLinkPeer         *sLink;

    /*
     * �о�� Block�� �ϳ��� ������ �о��
     */
    if (aProtocolContext->mReadBlock == NULL)
    {
        IDE_TEST(cmiReadBlock(aProtocolContext, aTimeout) != IDE_SUCCESS);

        /* bug-33841: ipc thread's state is wrongly displayed.
           IPC�� ��� ��Ŷ �����Ŀ� execute ���·� ���� */
        (void) gCMCallbackSetExecute(aUserContext, aTask);
    }

    while (1)
    {
        /*
         * Protocol ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            IDE_TEST(cmiReadProtocolInternal(aProtocolContext,
                                             &aProtocolContext->mProtocol,
                                             aTimeout) != IDE_SUCCESS);

            /*
             * Callback Function ȹ��
             */
            // proj_2160 cm_type removal: call mCallbackFunctionA5
            sCallbackFunction = aProtocolContext->mModule->mCallbackFunctionA5[aProtocolContext->mProtocol.mOpID];

            /*
             * Callback ȣ��
             */
            sRet = sCallbackFunction(aProtocolContext,
                                     &aProtocolContext->mProtocol,
                                     aProtocolContext->mOwner,
                                     aUserContext);
            
            /*
             * PROJ-1697 Performance view for Protocols
             */
            if( aProtocolContext->mModule->mModuleID == CMP_MODULE_DB )
            {
                CMP_DB_PROTOCOL_STAT_ADD( aProtocolContext->mProtocol.mOpID, 1 );
            }

            /*
             * Protocol Finalize
             */
            IDE_TEST(cmiFinalizeProtocol(&aProtocolContext->mProtocol) != IDE_SUCCESS);

            /*
             * Free Block List�� �޸� Block ����
             */
            IDE_TEST(cmiFreeReadBlock(aProtocolContext) != IDE_SUCCESS);

            /*
             * Callback ��� Ȯ��
             */
            if (sRet != IDE_SUCCESS)
            {
                break;
            }

            if (aProtocolContext->mIsAddReadBlock == ID_TRUE)
            {
                IDE_RAISE(ReadBlockEnd);
            }
        }
        else
        {
            if (aProtocolContext->mIsAddReadBlock == ID_FALSE)
            {
                /*
                 * ���� Block�� Free ��⸦ ���� Read Block List�� �߰�
                 */
                IDU_LIST_ADD_LAST(&aProtocolContext->mReadBlockList,
                                  &aProtocolContext->mReadBlock->mListNode);
            }

            IDE_EXCEPTION_CONT(ReadBlockEnd);
            {
                aProtocolContext->mReadBlock = NULL;
                aProtocolContext->mIsAddReadBlock = ID_FALSE;

                if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader) == ID_TRUE)
                {
                    IDE_TEST(cmiFreeReadBlock(aProtocolContext) != IDE_SUCCESS);
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
                    IDE_TEST(cmiReadBlock(aProtocolContext, aTimeout) != IDE_SUCCESS);
                }

            }
        }
    }

    return sRet;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiReadProtocol(cmiProtocolContext *aProtocolContext,
                       cmiProtocol        *aProtocol,
                       PDL_Time_Value     *aTimeout,
                       idBool             *aIsEnd)
{
    cmpCallbackFunction sCallbackFunction;
    IDE_RC              sRet;
    cmnLinkPeer        *sLink;

    /*
     * ���� Read Protocol�� ���������� �Ϸ�Ǿ��� ���
     */
    if (CMP_MARSHAL_STATE_IS_COMPLETE(aProtocolContext->mMarshalState) == ID_TRUE)
    {
        /*
         * Read Block ��ȯ
         */
        IDE_TEST(cmiFreeReadBlock(aProtocolContext) != IDE_SUCCESS);

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
        IDE_TEST(cmiReadBlock(aProtocolContext, aTimeout) != IDE_SUCCESS);
    }

    while (1)
    {
        /*
         * Protocol ����
         */
        if (CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
        {
            IDE_TEST(cmiReadProtocolInternal(aProtocolContext,
                                             aProtocol,
                                             aTimeout) != IDE_SUCCESS);

            /*
             * BASE Module�̸� Callback
             */
            if (aProtocolContext->mReadHeader.mA5.mModuleID == CMP_MODULE_BASE)
            {
                /*
                 * Callback Function ȹ��
                 */
                // proj_2160 cm_type removal: call mCallbackFunctionA5
                sCallbackFunction = aProtocolContext->mModule->mCallbackFunctionA5[aProtocol->mOpID];

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
                IDE_TEST(cmiFinalizeProtocol(aProtocol) != IDE_SUCCESS);

                IDE_TEST(sRet != IDE_SUCCESS);

                // BUG-18846
                if (aProtocolContext->mIsAddReadBlock == ID_TRUE)
                {
                    IDE_RAISE(ReadBlockEnd);
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            // BUG-18846
            if (aProtocolContext->mIsAddReadBlock == ID_FALSE)
            {
                IDU_LIST_ADD_LAST(&aProtocolContext->mReadBlockList,
                                  &aProtocolContext->mReadBlock->mListNode);
            }

            IDE_EXCEPTION_CONT(ReadBlockEnd);
            {
                aProtocolContext->mReadBlock      = NULL;
                aProtocolContext->mIsAddReadBlock = ID_FALSE;
            }

            IDE_TEST(cmiReadBlock(aProtocolContext, aTimeout) != IDE_SUCCESS);
        }
    }

    /*
     * ���� Read Block�� ������ �о�����
     */
    if (!CMI_CHECK_BLOCK_FOR_READ(aProtocolContext->mReadBlock))
    {
        // BUG-18846
        if (aProtocolContext->mIsAddReadBlock == ID_FALSE)
        {
            IDU_LIST_ADD_LAST(&aProtocolContext->mReadBlockList,
                              &aProtocolContext->mReadBlock->mListNode);

        }

        aProtocolContext->mReadBlock      = NULL;
        aProtocolContext->mIsAddReadBlock = ID_FALSE;

        if (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mReadHeader))
        {
            *aIsEnd = ID_TRUE;
        }
    }
    else
    {
        *aIsEnd = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC cmiWriteProtocol(cmiProtocolContext *aProtocolContext, cmiProtocol *aProtocol, PDL_Time_Value *aTimeout)
{
    cmpMarshalState     sMarshalState;
    cmpMarshalFunction  sMarshalFunction;

    /*
     * Write Block�� �Ҵ�Ǿ����� ������ �Ҵ�
     */
    if (aProtocolContext->mWriteBlock == NULL)
    {
        IDE_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                               &aProtocolContext->mWriteBlock)
                 != IDE_SUCCESS);
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
        IDE_TEST(cmiGetModule(&aProtocolContext->mWriteHeader,
                              &aProtocolContext->mModule) != IDE_SUCCESS);
    }

    /*
     * Operation ID �˻�
     */
    IDE_TEST_RAISE(aProtocol->mOpID >= aProtocolContext->mModule->mOpMaxA5, InvalidOpError);

    /*
     * Marshal Function ȹ��
     */
    sMarshalFunction = aProtocolContext->mModule->mWriteFunction[aProtocol->mOpID];

    /*
     * PROJ-1697 Performance view for Protocols
     */
    if( aProtocolContext->mModule->mModuleID == CMP_MODULE_DB )
    {
        CMP_DB_PROTOCOL_STAT_ADD( aProtocol->mOpID, 1 );
    }
    
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

            IDE_TEST(sMarshalFunction(aProtocolContext->mWriteBlock,
                                      aProtocol,
                                      &sMarshalState) != IDE_SUCCESS);

            /*
             * �������� ���Ⱑ �Ϸ�Ǿ����� Loop ����
             */
            if (CMP_MARSHAL_STATE_IS_COMPLETE(sMarshalState) == ID_TRUE)
            {
                break;
            }
        }

        /*
         * ����
         */
        if ( cmiWriteBlock(aProtocolContext, ID_FALSE, aTimeout) != IDE_SUCCESS )
        {
            IDE_TEST( aProtocolContext->mResendBlock != ID_TRUE );
        }
        else
        {
            /* do nothing */
        }

        /*
         * ���ο� Block �Ҵ�
         */
        IDE_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                               &aProtocolContext->mWriteBlock)
                 != IDE_SUCCESS);
    }

    /*
     * Protocol Finalize
     */
    IDE_TEST(cmiFinalizeProtocol(aProtocol) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidOpError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    IDE_EXCEPTION_END;
    {
        IDE_ASSERT(cmiFinalizeProtocol(aProtocol) == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

IDE_RC cmiFlushProtocol(cmiProtocolContext *aProtocolContext, idBool aIsEnd, PDL_Time_Value *aTimeout)
{
    /*PROJ-2616*/
    IDE_TEST_CONT((cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_DUMMY) ||
                  (cmiGetLinkImpl(aProtocolContext) == CMN_LINK_IMPL_IPCDA),
                  LABEL_SKIP_FLUSH);
                  
    if (aProtocolContext->mWriteBlock != NULL)
    {
        /*
         * Write Block�� �Ҵ�Ǿ� ������ ����
         */
        IDE_TEST(cmiWriteBlock(aProtocolContext, aIsEnd, aTimeout) != IDE_SUCCESS);
    }
    else
    {
        if ((aIsEnd == ID_TRUE) &&
            (aProtocolContext->mWriteHeader.mA5.mCmSeqNo != 0) &&
            (CMP_HEADER_PROTO_END_IS_SET(&aProtocolContext->mWriteHeader) == ID_FALSE))
        {
            /*
             * Sequence End�� ���۵��� �ʾ����� �� Write Block�� �Ҵ��Ͽ� ����
             */
            IDE_TEST(aProtocolContext->mLink->mPeerOp->mAllocBlock(aProtocolContext->mLink,
                                                                   &aProtocolContext->mWriteBlock)
                     != IDE_SUCCESS);

            IDE_TEST(cmiWriteBlock(aProtocolContext, ID_TRUE, aTimeout) != IDE_SUCCESS);
        }
    }

    IDE_EXCEPTION_CONT(LABEL_SKIP_FLUSH);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// fix BUG-17715
// ���� ��� ���ۿ� ���ڵ尡 �� �� �ִ��� �˻��Ѵ�.
IDE_RC cmiCheckFetch(cmiProtocolContext *aProtocolContext, UInt aRecordSize)
{
    // ������ Ÿ�Ը��� �ΰ����� ������ �߰��� ����
    // char, varchar�� ��� cmtAny�� ���� ���� ������ ����.
    // ���� ����� ���� ���� �ΰ� ������ ���� ���� ����Ѵ�.

    // OPCODE(1) + STMTID(4) + RSTID(2) + ROWNO(2) + COLNO(2) + TYPEID(1) + OFFSET(4) + SIZE(2) + END(1) + DATA(x)

    return ((aProtocolContext->mWriteBlock->mCursor + 19 + aRecordSize) < CMB_BLOCK_DEFAULT_SIZE) ?
        IDE_SUCCESS : IDE_FAILURE;
}

idBool cmiCheckInVariable(cmiProtocolContext *aProtocolContext, UInt aInVariableSize)
{
    UInt sCurSize;
    
    if( aProtocolContext->mWriteBlock == NULL )
    {
        // mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
        // ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
        // ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
        // cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }
    
    // TYPE(1) + SIZE(4) + DATA(x) + DELIMETER(1)
    return ((sCurSize + 5 + aInVariableSize + 1 + ID_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ID_TRUE : ID_FALSE;
}

idBool cmiCheckInBinary(cmiProtocolContext *aProtocolContext, UInt aInBinarySize)
{
    UInt sCurSize;
    
    if( aProtocolContext->mWriteBlock == NULL )
    {
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }
    
    // TYPE(1) + SIZE(4) + DATA(x) + DELIMETER(1)
    return ((sCurSize + 5 + aInBinarySize + 1 + ID_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ID_TRUE : ID_FALSE;
}

idBool cmiCheckInBit(cmiProtocolContext *aProtocolContext, UInt aInBitSize)
{
    UInt sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        // mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
        // ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
        // ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
        // cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    // TYPE(1) + PRECISION(4) + SIZE(4) + DATA(x)
    return ((sCurSize + 9 + aInBitSize + ID_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ID_TRUE : ID_FALSE;
}

idBool cmiCheckInNibble(cmiProtocolContext *aProtocolContext, UInt aInNibbleSize)
{
    UInt sCurSize;

    if( aProtocolContext->mWriteBlock == NULL )
    {
        // mWriteBlock�� null�� ���� ���� �ƹ��͵� ä������ ���� �����̱� ������
        // ä������ �������ݿ� ���� sCurSize�� �޶����� �ִ�.
        // ����, sCurSize�� ������ �ִ밪(CMP_HEADER_SIZE)���� �����Ѵ�.
        // cmtInVariable�� CM ��ü�� ���� Ÿ���̱� ������ ������� ��...
        sCurSize = CMP_HEADER_SIZE;
    }
    else
    {
        sCurSize = aProtocolContext->mWriteBlock->mCursor;
    }

    // TYPE(1) + PRECISION(4) + SIZE(4) + DATA(x)
    return ((sCurSize + 9 + aInNibbleSize + ID_SIZEOF(cmpProtocol)) < CMB_BLOCK_DEFAULT_SIZE) ?
        ID_TRUE : ID_FALSE;
}

// IN Ÿ���߿� ���� ū ����� ���� ���� IN_NIBBLE�̳� IN_BIT�̴�.
UInt cmiGetMaxInTypeHeaderSize()
{
    // TYPE(1) + PRECISION(4) + SIZE(4) + DATA(x)
    return 9;
}

UChar *cmiGetOpName( UInt aModuleID, UInt aOpID )
{
    UChar *sOpName = NULL;

    IDE_DASSERT( aModuleID < CMP_MODULE_MAX );

    switch( aModuleID )
    {
        case CMP_MODULE_BASE:
            IDE_DASSERT( aOpID < CMP_OP_BASE_MAX );
            sOpName = (UChar*)gCmpOpBaseMap[aOpID].mCmpOpName;
            break;

        case CMP_MODULE_DB:
            IDE_DASSERT( aOpID < CMP_OP_DB_MAX );
            sOpName = (UChar*)gCmpOpDBMap[aOpID].mCmpOpName;
            break;

        case CMP_MODULE_RP:
            IDE_DASSERT( aOpID < CMP_OP_RP_MAX );
            sOpName = (UChar*)gCmpOpRPMap[aOpID].mCmpOpName;
            break;

        case CMP_MODULE_DK:
            IDE_DASSERT( aOpID < CMP_OP_DK_MAX );
            sOpName = (UChar*)gCmpOpDKMap[aOpID].mCmpOpName;
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return sOpName;
}

cmiLinkImpl cmiGetLinkImpl(cmiProtocolContext *aProtocolContext)
{
    return (cmiLinkImpl)(aProtocolContext->mLink->mLink.mImpl);
}

/**
 * cmpCollectionDBBindColumnInfoGetResult�� �ִ� ũ�⸦ ��´�.
 *
 * cmtAny�� �̸� ���� �ִ� 50 ������ ������ ����.
 *
 * @return cmpCollectionDBBindColumnInfoGetResult�� �ִ� ũ��
 */
UInt cmiGetBindColumnInfoStructSize( void )
{
    return ID_SIZEOF(UInt)  /* mDataType */
         + ID_SIZEOF(UInt)  /* mLanguage */
         + ID_SIZEOF(UChar) /* mArguments */
         + ID_SIZEOF(SInt)  /* mPrecision */
         + ID_SIZEOF(SInt)  /* mScale */
         + ID_SIZEOF(UChar) /* mFlags */
         + (56 * 7);        /* cmtAny(1+4+50+1=56) * 7 */
}

// bug-19279 remote sysdba enable + sys can kill session
// client�� ���ݿ��� ���������� true
// local���� ���������� false ��ȯ
// tcp ����̰� IP�� 127.0.0.1�� �ƴ� ��쿡 �������� �����Ѵ�
// ����: local�̶� 127.0.0.l�� �ƴ� �ּҶ�� �������� ����.
// remote sysdba �� ������� ���θ� ������ �� ���
IDE_RC cmiCheckRemoteAccess(cmiLink* aLink, idBool* aIsRemote)
{
    struct sockaddr*         sAddrCommon = NULL ;
    struct sockaddr_storage  sAddr;
    struct sockaddr_in*      sAddr4 = NULL;
    struct sockaddr_in6*     sAddr6 = NULL;
    UInt                     sAddrInt = 0;
    UInt*                    sUIntPtr = NULL;

    *aIsRemote = ID_FALSE;
    /* BUG-44530 SSL���� ALTIBASE_SOCK_BIND_ADDR ���� */
    if ((aLink->mImpl == CMN_LINK_IMPL_TCP) ||
        (aLink->mImpl == CMN_LINK_IMPL_SSL) ||
        (aLink->mImpl == CMN_LINK_IMPL_IB))
    {
        /* proj-1538 ipv6 */
        idlOS::memset(&sAddr, 0x00, ID_SIZEOF(sAddr));
        IDE_TEST(cmiGetLinkInfo(aLink, (SChar *)&sAddr, ID_SIZEOF(sAddr),
                                CMI_LINK_INFO_REMOTE_SOCKADDR)
                 != IDE_SUCCESS);

        /* bug-30541: ipv6 code review bug.
         * use sockaddr.sa_family instead of sockaddr_storage.ss_family.
         * because the name is different on AIX 5.3 tl1 as __ss_family
         */
        sAddrCommon = (struct sockaddr*)&sAddr;
        /* ipv4 */
        if (sAddrCommon->sa_family == AF_INET)
        {
            sAddr4 = (struct sockaddr_in*)&sAddr;
            sAddrInt = *((UInt*)&sAddr4->sin_addr); // network byte order
            if (sAddrInt != htonl(INADDR_LOOPBACK))
            {
                *aIsRemote = ID_TRUE; /* remote */
            }
        }
        /* ipv6 including v4mapped */
        else
        {
            sAddr6 = (struct sockaddr_in6*)&sAddr;
            /* if v4mapped-ipv6, extract ipv4 (4bytes)
               ex) ::ffff:127.0.0.1 => 127.0.0.1 */
            if (idlOS::in6_is_addr_v4mapped(&(sAddr6->sin6_addr)))
            {
                /* sin6_addr: 16bytes => 4 UInts */
                sUIntPtr = (UInt*)&(sAddr6->sin6_addr);
                sAddrInt = *(sUIntPtr + 3);
                if (sAddrInt != htonl(INADDR_LOOPBACK))
                {
                    *aIsRemote = ID_TRUE; /* remote */
                }
            }
            /* pure ipv6 addr */
            else
            {
                if (idlOS::in6_is_addr_loopback(&sAddr6->sin6_addr) != ID_TRUE)
                {
                    *aIsRemote = ID_TRUE; /* remote */
                }
            }
        }
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

// fix BUG-30835
idBool cmiIsValidIPFormat(SChar * aIP)
{
    struct  sockaddr sSockAddr;
    SChar *          sCurrentPos = NULL;
    SChar            sIP[IDL_IP_ADDR_MAX_LEN + 1] = {0,};
    idBool           sRes = ID_FALSE;
    UInt             sIPaddrLen;

    if (aIP != NULL)
    {
        /* trim zone index because inet_pton does not support rfc4007 zone index */
        sIPaddrLen = IDL_MIN((IDL_IP_ADDR_MAX_LEN), idlOS::strlen(aIP));
        
        idlOS::strncpy(sIP, aIP, sIPaddrLen);
                
        for(sCurrentPos = sIP; *sCurrentPos != '\0'; sCurrentPos++)
        {
            if(*sCurrentPos == '%')
            {
                *sCurrentPos = '\0';
                break;
            }
        }
        
        /* inet_pton : success is not zero  */
        if( (PDL_OS::inet_pton(AF_INET, sIP, &sSockAddr) > 0) || 
            (PDL_OS::inet_pton(AF_INET6, sIP, &sSockAddr) > 0) )
        {
            sRes = ID_TRUE;
        }
        else
        {
            sRes = ID_FALSE;
        }
    }
    
    return sRes;
}

/***********************************************************
 * proj_2160 cm_type removal
 * cmbBlock ������ 2���� NULL�� �����
 * cmiAllocCmBlock�� ȣ���ϱ� ���� �� �Լ��� �ݵ�� ����
 * ȣ���ؼ� cmbBlock �Ҵ��� ����� �ǵ��� �ؾ� �Ѵ�.
***********************************************************/
IDE_RC cmiMakeCmBlockNull(cmiProtocolContext *aCtx)
{
    aCtx->mLink = NULL;
    aCtx->mReadBlock = NULL;
    aCtx->mWriteBlock = NULL;
    return IDE_SUCCESS;
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
IDE_RC cmiAllocCmBlock(cmiProtocolContext* aCtx,
                       UChar               aModuleID,
                       cmiLink*            aLink,
                       void*               aOwner,
                       idBool              aResendBlock)
{
    cmbPool*   sPool;

    // cmnLink has to be prepared
    IDE_TEST(aLink == NULL);
    // memory allocation allowed only once
    IDE_TEST(aCtx->mReadBlock != NULL);

    aCtx->mModule  = gCmpModule[aModuleID];
    aCtx->mLink    = (cmnLinkPeer*)aLink;
    aCtx->mOwner   = aOwner;
    aCtx->mIsAddReadBlock = ID_FALSE;
    aCtx->mSession = NULL; // deprecated
    
    /*PROJ-2616*/
    IDE_TEST_CONT(cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA,
                  ContAllocCmBlock);

    // allocate readBlock, writeBlock statically.
    sPool = aCtx->mLink->mPool;
    IDE_TEST(sPool->mOp->mAllocBlock(
            sPool, &aCtx->mReadBlock) != IDE_SUCCESS);
    IDE_TEST(sPool->mOp->mAllocBlock(
            sPool, &aCtx->mWriteBlock) != IDE_SUCCESS);
    aCtx->mWriteBlock->mDataSize = CMP_HEADER_SIZE;
    aCtx->mWriteBlock->mCursor   = CMP_HEADER_SIZE;

    cmpHeaderInitialize(&aCtx->mWriteHeader);
    CMP_MARSHAL_STATE_INITIALIZE(aCtx->mMarshalState);
    aCtx->mProtocol.mFinalizeFunction = (void *)cmpArgNULL;

    IDU_LIST_INIT(&aCtx->mReadBlockList);
    IDU_LIST_INIT(&aCtx->mWriteBlockList);

    /*PROJ-2616*/
    IDE_EXCEPTION_CONT(ContAllocCmBlock);

    aCtx->mListLength             = 0;
    aCtx->mCmSeqNo                = 0; // started from 0
    aCtx->mIsDisconnect           = ID_FALSE;
    aCtx->mSessionCloseNeeded     = ID_FALSE;
    aCtx->mIPDASpinMaxCount       = CMI_IPCDA_SPIN_MIN_LOOP; /*PROJ-2616*/
    
    /* BUG-45184 */
    aCtx->mSendDataSize           = 0;
    aCtx->mReceiveDataSize        = 0;
    aCtx->mSendDataCount          = 0;
    aCtx->mReceiveDataCount       = 0;
    
    cmiDisableCompress( aCtx );
    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
    /* ���� ���� ����� default ���� LZO */ 
    cmiSetDecompressType( aCtx, CMI_COMPRESS_LZO );
    
    /* BUG-38102*/
    cmiDisableEncrypt( aCtx );

    aCtx->mResendBlock = aResendBlock;

    aCtx->mProtocol.mClientLastOpID = CMP_OP_DB_LAST_OP_ID_OF_VER_7_1;  /* PROJ-2733-Protocol */
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmiAllocCmBlockForA5( cmiProtocolContext* aCtx,
                             UChar               aModuleID,
                             cmiLink*            aLink,
                             void*               aOwner,
                             idBool              aResendBlock )
 {
     cmbPool*   sPool = NULL;

     // cmnLink has to be prepared
     IDE_TEST(aLink == NULL);
     // memory allocation allowed only once
     IDE_TEST(aCtx->mReadBlock != NULL);

     aCtx->mModule  = gCmpModule[aModuleID];
     aCtx->mLink    = (cmnLinkPeer*)aLink;
     aCtx->mOwner   = aOwner;
     aCtx->mIsAddReadBlock = ID_FALSE;
     aCtx->mSession = NULL; // deprecated

     // allocate readBlock, writeBlock statically.
     sPool = aCtx->mLink->mPool;
     IDE_TEST(sPool->mOp->mAllocBlock(
             sPool, &aCtx->mReadBlock) != IDE_SUCCESS);

     IDE_TEST(sPool->mOp->mAllocBlock(
             sPool, &aCtx->mWriteBlock) != IDE_SUCCESS);

     aCtx->mWriteBlock->mDataSize = CMP_HEADER_SIZE;
     aCtx->mWriteBlock->mCursor   = CMP_HEADER_SIZE;

     cmpHeaderInitializeForA5(&aCtx->mWriteHeader);
     aCtx->mWriteHeader.mA5.mModuleID = aModuleID;

     // initialize for readheader
     cmpHeaderInitializeForA5(&aCtx->mReadHeader);
     aCtx->mReadHeader.mA5.mModuleID = aModuleID;

     CMP_MARSHAL_STATE_INITIALIZE(aCtx->mMarshalState);
     aCtx->mProtocol.mFinalizeFunction = (void *)cmpArgNULL;

     IDU_LIST_INIT(&aCtx->mReadBlockList);
     IDU_LIST_INIT(&aCtx->mWriteBlockList);

     aCtx->mListLength   = 0;
     aCtx->mCmSeqNo      = 0; // started from 0
     aCtx->mIsDisconnect = ID_FALSE;
     aCtx->mSessionCloseNeeded  = ID_FALSE;
     
     /* BUG-45184 */
     aCtx->mSendDataSize           = 0;
     aCtx->mReceiveDataSize        = 0;
     aCtx->mSendDataCount          = 0;
     aCtx->mReceiveDataCount       = 0;
     
     cmiDisableCompress( aCtx );
     /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
     /* ���� ���� ����� default ���� LZO */ 
     cmiSetDecompressType( aCtx, CMI_COMPRESS_LZO );

     /* BUG-38102*/
     cmiDisableEncrypt( aCtx );

     aCtx->mResendBlock = aResendBlock;

     aCtx->mProtocol.mClientLastOpID = 0;  /* PROJ-2733-Protocol Unused at A5 */

     return IDE_SUCCESS;
     IDE_EXCEPTION_END;
     return IDE_FAILURE;
 }

/***********************************************************
 * �� �Լ��� ������ ������Ŀ��� �޸� �ݳ��� ����
 * �ݵ�� ȣ��Ǿ�� �Ѵ�
 * ���ο����� A7�� A5 ������ ���ÿ� ó���ϵ��� �Ǿ� �ִ�
***********************************************************/
IDE_RC cmiFreeCmBlock(cmiProtocolContext* aCtx)
{
    cmbPool*  sPool;

    iduListNode     * sIterator = NULL;
    iduListNode     * sNodeNext = NULL;
    cmbBlock        * sPendingBlock = NULL;

    /*PROJ-2616*/
    IDE_TEST_CONT(cmiGetLinkImpl(aCtx) == CMN_LINK_IMPL_IPCDA,
                  ContFreeCmBlock);

    IDE_TEST(aCtx->mLink == NULL);

    if (aCtx->mLink->mLink.mPacketType != CMP_PACKET_TYPE_A5)
    {
        sPool = aCtx->mLink->mPool;
        IDE_ASSERT(aCtx->mReadBlock != NULL && aCtx->mWriteBlock != NULL);

        // timeout���� ������ ������ �����޽����� ������
        // ���� �����Ͱ� ���� cmBlock�� ���� �ִ�. �̸� ����
        if (aCtx->mWriteBlock->mCursor > CMP_HEADER_SIZE)
        {
            (void)cmiSend(aCtx, ID_TRUE);
        }

        IDE_TEST(sPool->mOp->mFreeBlock(sPool, aCtx->mReadBlock)
                 != IDE_SUCCESS);
        aCtx->mReadBlock = NULL;

        IDE_TEST(sPool->mOp->mFreeBlock(sPool, aCtx->mWriteBlock)
                 != IDE_SUCCESS);
        aCtx->mWriteBlock = NULL;
    }
    else
    {
        if (aCtx->mReadBlock != NULL)
        {
            IDU_LIST_ADD_LAST(&aCtx->mReadBlockList,
                              &aCtx->mReadBlock->mListNode);
        }

        aCtx->mReadBlock = NULL;
        IDE_TEST_RAISE(cmiFlushProtocol(aCtx, ID_TRUE)
                       != IDE_SUCCESS, FlushFail);
        IDE_TEST(cmiFinalizeProtocol(&aCtx->mProtocol)
                 != IDE_SUCCESS);

        /*
         * Free All Read Blocks
         */
        IDE_TEST(cmiFreeReadBlock(aCtx) != IDE_SUCCESS);
    }

    /* BUG-46608 mWriteBlockList �� �Ҵ�Ǿ� �ִ� Block �� ������ ���� �Ѵ�. */
    IDU_LIST_ITERATE_SAFE(&aCtx->mWriteBlockList, sIterator, sNodeNext)
    {
        sPool = aCtx->mLink->mPool;

        sPendingBlock = (cmbBlock *)sIterator->mObj;
        IDE_TEST( sPool->mOp->mFreeBlock(sPool, sPendingBlock) != IDE_SUCCESS );
        aCtx->mListLength--;
    }
    
    IDE_EXCEPTION_CONT(ContFreeCmBlock);

    return IDE_SUCCESS;

    IDE_EXCEPTION(FlushFail);
    {
        IDE_PUSH();

        IDE_ASSERT(cmiFinalizeProtocol(&aCtx->mProtocol) == IDE_SUCCESS);

        IDE_ASSERT(cmiFreeReadBlock(aCtx) == IDE_SUCCESS);

        IDE_POP();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * CM ���������� A5 ������ Handshake�� ó���Ѵ�.
 */ 
static IDE_RC cmiHandshakeA5( cmiProtocolContext * aCtx )
{
    UChar                 sOpID;
    cmpArgBASEHandshake * sResultA5 = NULL;
    cmiProtocol           sProtocol;
    SInt                  sStage = 0;
    
    /* PROJ-2563 */
    if ( aCtx->mReadHeader.mA5.mModuleID == CMP_MODULE_RP )
    {
        /* do nothing */
        /* ������ A5�������ݷ� ������ �õ��ϴ� Ŭ���̾�Ʈ�� ���� ��,
         * UL�ΰ�� ACK�� �����ϸ�, UL���� �ٸ� ����� ��� ������ ���´�.
         * a631���� A5�� �̿��� RP Ŭ���̾�Ʈ�� ������ �ν��ϹǷ�,
         * RPŬ���̾�Ʈ�� ������ �� ������ �νľʰ� �ϸ�, ���� ACK�� �������� �ʴ� ������ �����Ѵ�.
         */
    }
    else
    {
        IDE_TEST_RAISE( aCtx->mReadHeader.mA5.mModuleID != CMP_MODULE_BASE,
                        INVALID_MODULE );
        
        CMI_RD1( aCtx, sOpID );
        IDE_TEST_RAISE( sOpID != CMP_OP_BASE_Handshake, INVALID_PROTOCOL );
            
        CMI_SKIP_READ_BLOCK(aCtx,3); //cmpArgBASEHandshake
    
        /* initialize writeBlock's header using recved header */
        aCtx->mWriteHeader.mA5.mHeaderSign      = CMP_HEADER_SIGN_A5;
        aCtx->mWriteHeader.mA5.mModuleID        = aCtx->mReadHeader.mA5.mModuleID;
        aCtx->mWriteHeader.mA5.mModuleVersion   = aCtx->mReadHeader.mA5.mModuleVersion;
        aCtx->mWriteHeader.mA5.mSourceSessionID = aCtx->mReadHeader.mA5.mTargetSessionID;
        aCtx->mWriteHeader.mA5.mTargetSessionID = aCtx->mReadHeader.mA5.mSourceSessionID;
    
        /* mModule has to be set properly for cmiWriteProtocol */
        aCtx->mModule = gCmpModule[ CMP_MODULE_BASE ];
        CMI_PROTOCOL_INITIALIZE( sProtocol, sResultA5, BASE, Handshake );
        sStage = 1;

        sResultA5->mBaseVersion   = CMP_VER_BASE_MAX - 1;
        sResultA5->mModuleID      = CMP_MODULE_DB;
        sResultA5->mModuleVersion = CM_PATCH_VERSION;

        IDE_TEST( cmiWriteProtocol( aCtx, &sProtocol ) != IDE_SUCCESS );

        IDE_TEST( cmiFlushProtocol( aCtx, ID_TRUE ) != IDE_SUCCESS );
    
        sStage = 0;
        IDE_TEST( cmiFinalizeProtocol( &sProtocol ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_MODULE )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_INVALID_MODULE ) );
    }
    IDE_EXCEPTION( INVALID_PROTOCOL )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1:
            (void)cmiFinalizeProtocol( &sProtocol );
        default:
            break;
    }

    IDE_POP();
    
    return IDE_FAILURE;
}

/*
 * CM Block except CM Packet header is compressed by using iduCompression. then
 * Original CM Block is replaced with compressed one.
 */
/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
static IDE_RC cmiCompressLZOCmBlock( cmiProtocolContext * aCtx,
                                     cmbBlock           * aBlock )
{
    UChar sOutBuffer[ IDU_COMPRESSION_MAX_OUTSIZE( CMB_BLOCK_DEFAULT_SIZE ) ] = { 0, };
    ULong sWorkMemory[ IDU_COMPRESSION_WORK_SIZE / ID_SIZEOF(ULong) ] = { 0, };
    UInt sCompressSize = 0;

    IDE_TEST_RAISE(
        iduCompression::compress( aBlock->mData + CMP_HEADER_SIZE,
                                  aBlock->mCursor - CMP_HEADER_SIZE,
                                  sOutBuffer,
                                  sizeof( sOutBuffer ),
                                  &sCompressSize,
                                  sWorkMemory )
        != IDE_SUCCESS, COMPRESS_ERROR );

    aBlock->mCursor = CMP_HEADER_SIZE;
    CMI_WCP( aCtx, sOutBuffer, sCompressSize );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( COMPRESS_ERROR )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_COMPRESS_DATA_ERROR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC cmiCompressLZ4CmBlock( cmiProtocolContext * aCtx,
                                     cmbBlock           * aBlock )
{
    SInt  sCompressSize = 0;
    UChar sCompressBuffer[ IDU_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE ) ];
    SInt  sMaxCompressSize = IDU_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE );

    /* BUG-47078 Apply LZ4 Algorithm */
    sCompressSize = iduLZ4_compress( (SChar*)aBlock->mData + CMP_HEADER_SIZE,
                                     (SChar*)sCompressBuffer,
                                     aBlock->mCursor - CMP_HEADER_SIZE,
                                     sMaxCompressSize,
                                     aCtx->mCompressLevel );

    IDU_FIT_POINT_RAISE( "cmiCompressCmBlock::COMPRESS_ERROR",
                         COMPRESS_ERROR );

    IDE_TEST_RAISE( ( sCompressSize <= 0 ) ||
                    ( sCompressSize > sMaxCompressSize ),
                    COMPRESS_ERROR );

    aBlock->mCursor = CMP_HEADER_SIZE;
    CMI_WCP( aCtx, sCompressBuffer, sCompressSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION( COMPRESS_ERROR )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_COMPRESS_DATA_ERROR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC cmiCompressCmBlock( cmpHeader          * aHeader,
                                  cmiProtocolContext * aCtx,
                                  cmbBlock           * aBlock )
{
    switch ( aCtx->mCompressType )
    {
        case CMI_COMPRESS_NONE :
            break;
            
        case CMI_COMPRESS_LZO :
            if ( cmiCompressLZOCmBlock( aCtx, aBlock ) == IDE_SUCCESS )
            {
                CMP_HEADER_FLAG_SET_COMPRESS( aHeader );        
            }
            else
            {
                /* BUG-47020 ���࿡ �����ϴ��� ������ ��� �� �� ���� �α׷� ������. 
                 * ��Ŷ ������ ����ȭ������ �ϹǷ� altibase_rp.log �� ������ش�. */
                 IDE_ERRLOG( IDE_RP_0 );
            }
            break;
            
        case CMI_COMPRESS_LZ4 :
            if ( cmiCompressLZ4CmBlock( aCtx, aBlock ) == IDE_SUCCESS )
            {
                CMP_HEADER_FLAG_SET_COMPRESS( aHeader );        
            }
            else
            {
                /* BUG-47020 ���࿡ �����ϴ��� ������ ��� �� �� ���� �α׷� ������. 
                 * ��Ŷ ������ ����ȭ������ �ϹǷ� altibase_rp.log �� ������ش�. */
                 IDE_ERRLOG( IDE_RP_0 );
            }
            break; 
            
        default :
            IDE_DASSERT( 0 );
            IDE_RAISE( InvalidCompressType );
            break;             
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( InvalidCompressType )
    {
        ideLog::log( IDE_RP_0, CM_TRC_COMPRESS_TYPE_ERROR, aCtx->mCompressType );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * CM Block except CM Packet header is decompressed by using iduCompression.
 * then Original CM Block is replaced with decompressed one.
 */
/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
static IDE_RC cmiDecompressLZOCmBlock( cmbBlock           * aBlock,
                                       UInt                 aDataLength )
{
    UChar sOutBuffer[ CMB_BLOCK_DEFAULT_SIZE ] = { 0, };
    UInt sDecompressSize = 0;

    IDE_TEST_RAISE(
        iduCompression::decompress( aBlock->mData + CMP_HEADER_SIZE,
                                    aDataLength,
                                    sOutBuffer,
                                    sizeof( sOutBuffer ),
                                    &sDecompressSize )
        != IDE_SUCCESS, DECOMPRESS_ERROR );

    idlOS::memcpy( aBlock->mData + CMP_HEADER_SIZE,
                   sOutBuffer,
                   sDecompressSize );
    aBlock->mDataSize = sDecompressSize + CMP_HEADER_SIZE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( DECOMPRESS_ERROR )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_DECOMPRESS_DATA_ERROR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC cmiDecompressLZ4CmBlock( cmbBlock           * aBlock,
                                       UInt                 aDataLength )
{
    SInt  sDecompressSize = 0;
    UChar sDecompressBuffer[ IDU_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE ) ];
    SInt  sMaxDecompressSize = IDU_LZ4_COMPRESSBOUND( CMB_BLOCK_DEFAULT_SIZE );

    /* BUG-47078 Apply LZ4 Algorithm */
    sDecompressSize = iduLZ4_decompress( (SChar*)aBlock->mData + CMP_HEADER_SIZE,
                                         (SChar*)sDecompressBuffer,
                                         aDataLength,
                                         sMaxDecompressSize );
    IDE_TEST_RAISE( ( sDecompressSize <= 0 ) ||
                    ( sDecompressSize > sMaxDecompressSize ),
                    DECOMPRESS_ERROR );

    idlOS::memcpy( aBlock->mData + CMP_HEADER_SIZE,
                   sDecompressBuffer,
                   sDecompressSize );
    aBlock->mDataSize = sDecompressSize + CMP_HEADER_SIZE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( DECOMPRESS_ERROR )
    {
        IDE_SET( ideSetErrorCode( cmERR_ABORT_DECOMPRESS_DATA_ERROR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC cmiDecompressCmBlock( cmiProtocolContext * aCtx,
                                    UInt                 aDataLength )
{
    switch ( aCtx->mDecompressType )
    {
        case CMI_COMPRESS_NONE :
            break;
            
        case CMI_COMPRESS_LZO :
            IDE_TEST( cmiDecompressLZOCmBlock( aCtx->mReadBlock,
                                               aDataLength )
                      != IDE_SUCCESS );
            break;
            
        case CMI_COMPRESS_LZ4 :
            IDE_TEST( cmiDecompressLZ4CmBlock( aCtx->mReadBlock,
                                               aDataLength )
                      != IDE_SUCCESS );
            break;
            
        default :
            IDE_DASSERT( 0 );
            IDE_RAISE( InvalidDecompressType );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( InvalidDecompressType )
    {
        ideLog::log( IDE_RP_0, CM_TRC_DECOMPRESS_TYPE_ERROR, aCtx->mDecompressType );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/*
 * CM Block except CM Packet header is encrypted by using RC4. 
 * Then original CM Block is replaced with encrypted one.
 */
static IDE_RC cmiEncryptCmBlock( cmbBlock   * aBlock )
{
    UChar      *sSrcData     = NULL;
    UChar       sEncryptedData[ CMB_BLOCK_DEFAULT_SIZE ] = {0, };
    idsRC4      sRC4;
    RC4Context  sRC4Ctx;

    IDE_TEST_CONT( aBlock->mCursor == CMP_HEADER_SIZE, noDataToEncrypt );

    sRC4.setKey( &sRC4Ctx, (UChar *)RC4_KEY, RC4_KEY_LEN );

    sSrcData = (UChar *)aBlock->mData + CMP_HEADER_SIZE;

    sRC4.convert( &sRC4Ctx, 
                  (const UChar *)sSrcData, 
                  aBlock->mCursor - CMP_HEADER_SIZE, 
                  sEncryptedData );    
        
    idlOS::memcpy( aBlock->mData + CMP_HEADER_SIZE,
                   sEncryptedData,
                   aBlock->mCursor - CMP_HEADER_SIZE );

    IDE_EXCEPTION_CONT( noDataToEncrypt );
    
    return IDE_SUCCESS;
}

/*
 * CM Block except CM Packet header is decrypted by using RC4.
 * Then original CM Block is replaced with decrypted one.
 */
static IDE_RC cmiDecryptCmBlock( cmbBlock   * aBlock, 
                                 UInt         aDataLength )
{
    UChar       sDecryptedData[ CMB_BLOCK_DEFAULT_SIZE ] = {0, };
    idsRC4      sRC4;
    RC4Context  sRC4Ctx;

    sRC4.setKey( &sRC4Ctx, (UChar *)RC4_KEY, RC4_KEY_LEN );

    sRC4.convert( &sRC4Ctx,
                  aBlock->mData + CMP_HEADER_SIZE,
                  aDataLength,
                  sDecryptedData );

    idlOS::memcpy( aBlock->mData + CMP_HEADER_SIZE,
                   sDecryptedData,
                   aDataLength );

    aBlock->mDataSize = aDataLength + CMP_HEADER_SIZE;

    return IDE_SUCCESS;
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
// #define CMI_DUMP 1
IDE_RC cmiRecv(cmiProtocolContext* aCtx,
               void*           aUserContext,
               PDL_Time_Value* aTimeout,
               void*           aTask)
{
    cmpCallbackFunction  sCallbackFunction;
    cmpHeader*           sHeader = &(aCtx->mReadHeader);
    UInt                 sCmSeqNo;
    UChar                sOpID;
    IDE_RC               sRet = IDE_SUCCESS;

beginToRecv:
    aCtx->mReadBlock->mDataSize  = 0;
    aCtx->mReadBlock->mCursor   = 0;

    IDU_FIT_POINT_RAISE( "cmiRecv::Server::Disconnected", Disconnected ); 
    IDU_FIT_POINT( "cmiRecv::Server::Recv" ); 
    
    IDE_TEST_RAISE(aCtx->mIsDisconnect == ID_TRUE, Disconnected);
    IDE_TEST(aCtx->mLink->mPeerOp->mRecv(aCtx->mLink,
                                         &aCtx->mReadBlock,
                                         sHeader, 
                                         aTimeout) != IDE_SUCCESS);
    
    /* BUG-45184 */
    aCtx->mReceiveDataSize += aCtx->mReadBlock->mDataSize;
    aCtx->mReceiveDataCount++;
    
    /* bug-33841: ipc thread's state is wrongly displayed.
       IPC�� ��� ��Ŷ �����Ŀ� execute ���·� ����.
       RP, DK ����� ��� SetExecute�� ���� ���� */
    if (aCtx->mModule->mModuleID == CMP_MODULE_DB)
    {
        (void) gCMCallbackSetExecute(aUserContext, aTask);
    }

    // �� if���� A5 client�� ������ ��쿡 ���� ���� �ѹ��� ����ȴ�
    // call A7's DB handshake directly.
    if ( cmiGetPacketType( aCtx ) == CMP_PACKET_TYPE_A5 )
    {
        IDE_TEST( cmiHandshakeA5( aCtx ) != IDE_SUCCESS );

        aCtx->mIsAddReadBlock = ID_FALSE; // this is needed.

        IDE_RAISE( CmiRecvReturn );
    }

    /* BUG-38102 */
    if ( CMP_HEADER_FLAG_ENCRYPT_IS_SET( sHeader ) == ID_TRUE )
    {
        IDE_TEST( cmiDecryptCmBlock( aCtx->mReadBlock, 
                                     sHeader->mA7.mPayloadLength )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    if ( CMP_HEADER_FLAG_COMPRESS_IS_SET( sHeader ) == ID_TRUE )
    {
        /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
        IDE_TEST( cmiDecompressCmBlock( aCtx,
                                        sHeader->mA7.mPayloadLength )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    sCmSeqNo = CMP_HEADER_SEQ_NO(sHeader);
#ifdef CMI_DUMP
    ideLog::logLine(IDE_CM_4, "[%5d] recv [%5d]",
                    sCmSeqNo, sHeader->mA7.mPayloadLength);
#endif

    /* BUG-41909 Add dump CM block when a packet error occurs */
    IDU_FIT_POINT_RAISE( "cmiRecv::Server::InvalidProtocolSeqNo", InvalidProtocolSeqNo );

    // ��� ��Ŷ�� ���ǳ��ּ� �����Ϸù�ȣ�� ���´�.
    // ����: 0 ~ 0x7fffffff, �ִ밪�� �ٴٸ��� 0���� �ٽ� ���۵ȴ�
    IDE_TEST_RAISE(sCmSeqNo != aCtx->mCmSeqNo, InvalidProtocolSeqNo);
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
    if ( ( aCtx->mModule->mModuleID == CMP_MODULE_RP ) ||
         ( aCtx->mModule->mModuleID == CMP_MODULE_DK ) )
    {
        IDE_RAISE(CmiRecvReturn);
    }

    while (1)
    {
        CMI_RD1(aCtx, sOpID);
        IDE_TEST_RAISE(sOpID >= aCtx->mModule->mOpMax, InvalidOpError);  /* BUG-45804 */
#ifdef CMI_DUMP
        ideLog::logLine(IDE_CM_4, "%s", gCmpOpDBMap[sOpID].mCmpOpName);
#endif

        aCtx->mSessionCloseNeeded  = ID_FALSE;
        /* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
        aCtx->mProtocol.mOpID      = sOpID;
        sCallbackFunction = aCtx->mModule->mCallbackFunction[sOpID];
        
        sRet = sCallbackFunction(aCtx,
                                 &aCtx->mProtocol,
                                 aCtx->mOwner,
                                 aUserContext);

        if (aCtx->mModule->mModuleID == CMP_MODULE_DB)
        {
            CMP_DB_PROTOCOL_STAT_ADD(sOpID, 1);
        }
        // dequeue�� ��� IDE_CM_STOP�� ��ȯ�� �� �ִ�.
        IDE_TEST_RAISE(sRet != IDE_SUCCESS, CmiRecvReturn);

        /* BUG-41909 Add dump CM block when a packet error occurs */
        IDU_FIT_POINT_RAISE( "cmiRecv::Server::MarshalErr", MarshalErr );

        // ������ ��Ŷ�� ���� ��� �������� ó���� ���� ���
        if (aCtx->mReadBlock->mCursor == aCtx->mReadBlock->mDataSize)
        {
            break;
        }
        // �������� �ؼ��� �߸��Ǿ� cursor�� ��Ŷ�� �Ѿ ���
        else if (aCtx->mReadBlock->mCursor > aCtx->mReadBlock->mDataSize)
        {
            IDE_RAISE(MarshalErr);
        }

        IDE_TEST_RAISE(aCtx->mIsDisconnect == ID_TRUE, Disconnected);
    }

    // special �������� ó��(Message, LobPut protocol)
    // msg�� lobput ���������� ��� �������� group���� ������ ������
    // (��Ŷ���� ���� opID�� ������ ��Ŷ����� ���� flag�� 0�̴�)
    // �̵��� ������ �����ϴ��� ������ �ѹ��� ����۽��ؾ� �Ѵ�.
    if (CMP_HEADER_PROTO_END_IS_SET(sHeader) == ID_FALSE)
    {
        goto beginToRecv;
    }

    IDE_EXCEPTION_CONT(CmiRecvReturn);

    return sRet; // IDE_SUCCESS, IDE_FAILURE, IDE_CM_STOP

    IDE_EXCEPTION(Disconnected)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION(InvalidProtocolSeqNo)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));

        cmiDump(aCtx, sHeader, aCtx->mReadBlock, 0, aCtx->mReadBlock->mDataSize);
    }
    IDE_EXCEPTION(MarshalErr)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_MARSHAL_ERROR));

        cmiDump(aCtx, sHeader, aCtx->mReadBlock, 0, aCtx->mReadBlock->mDataSize);
    }
    IDE_EXCEPTION(InvalidOpError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************
 * PROJ-2616
 * cmiRecvIPCDA
 *
 * cmiRecv for IPCDA.
 *
 * aCtx[in]           - cmiProtocolContext
 * aUserContext[in]   - UserContext
 * aMicroSleepTime[in] - Sleep time in spinlock
 ****************************************************/
IDE_RC cmiRecvIPCDA(cmiProtocolContext *aCtx,
                    void               *aUserContext,
                    UInt                aMicroSleepTime)
{
    cmpCallbackFunction  sCallbackFunction;
    UChar                sOpID;
    IDE_RC               sRet = IDE_SUCCESS;

    cmnLinkPeerIPCDA    *sLinkIPCDA     = NULL;
    cmbBlockIPCDA       *sOrgBlock = NULL;
    cmbBlock             sTmpBlock;

    UInt                 sCurReadOperationCount     = 0;

    sLinkIPCDA = (cmnLinkPeerIPCDA *)aCtx->mLink;

    sOrgBlock              = (cmbBlockIPCDA *)aCtx->mReadBlock;

    IDE_TEST_RAISE(aCtx->mIsDisconnect == ID_TRUE, Disconnected);

    sTmpBlock.mBlockSize   = sOrgBlock->mBlock.mBlockSize;
    sTmpBlock.mCursor      = CMP_HEADER_SIZE;
    sTmpBlock.mDataSize    = sOrgBlock->mBlock.mDataSize;
    sTmpBlock.mIsEncrypted = sOrgBlock->mBlock.mIsEncrypted;
    sTmpBlock.mData        = &sOrgBlock->mData;

    IDE_TEST_RAISE(cmiIPCDACheckReadFlag(aCtx, aMicroSleepTime) == IDE_FAILURE, Disconnected);

    aCtx->mReadBlock = &sTmpBlock;

    while(1)
    {
        IDE_TEST_RAISE(cmiIPCDACheckDataCount(aCtx,
                                              &sOrgBlock->mOperationCount,
                                              sCurReadOperationCount,
                                              aMicroSleepTime) == IDE_FAILURE, Disconnected);

        sTmpBlock.mDataSize = sOrgBlock->mBlock.mDataSize;

        CMI_RD1(aCtx, sOpID);

        IDE_TEST_RAISE(sOpID >= aCtx->mModule->mOpMax, InvalidOpError);

        /* Check end-of protocol */
        /* BUG-46502 */
        if ( sOpID == CMP_OP_DB_IPCDALastOpEnded )
        {
            break;
        }

        if (sCurReadOperationCount++ == 0)
        {
            IDE_TEST_RAISE(cmnLinkPeerInitSvrWriteIPCDA((void*)aCtx) == IDE_FAILURE, Disconnected);
        }

        /* Callback Function ȹ�� */
        aCtx->mSessionCloseNeeded  = ID_FALSE;
        /* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
        aCtx->mProtocol.mOpID      = sOpID;

#if defined(ALTI_CFG_OS_LINUX)
        sLinkIPCDA->mMessageQ.mNeedToNotify = ID_TRUE;
#endif

        /* Callback ȣ�� */
        sCallbackFunction = aCtx->mModule->mCallbackFunction[sOpID];
        sRet = sCallbackFunction(aCtx,
                                 &aCtx->mProtocol,
                                 aCtx->mOwner,
                                 aUserContext);
        /* PROJ-1697 Performance view for Protocols */
        CMP_DB_PROTOCOL_STAT_ADD( aCtx->mProtocol.mOpID, 1 );

        /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
        /* BUG-46502 */
        if ( sRet == IDE_CM_STOP )
        {
            break;
        }
        IDE_TEST(sRet == IDE_FAILURE);
    }

    aCtx->mReadBlock = (cmbBlock *)sOrgBlock;

    return sRet; // IDE_SUCCESS, IDE_FAILURE, IDE_CM_STOP

    IDE_EXCEPTION(Disconnected);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION(InvalidOpError)
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_OPERATION));
    }
    IDE_EXCEPTION_END;

    /* BUG-44468 [cm] codesonar warning in CM */
    aCtx->mReadBlock = (cmbBlock *)sOrgBlock;

    return IDE_FAILURE;
}

#if defined(ALTI_CFG_OS_LINUX)
/* PROJ-2616 Send message through MessageQ.*/
IDE_RC cmiMessageQNotify(cmnLinkPeerIPCDA *aLink)
{
    char sSendBuf[1];
    struct timespec   sMessageQWaitTime;

    IDE_TEST_CONT((cmuProperty::isIPCDAMessageQ() != ID_TRUE), ContMQNOtify);

    sSendBuf[0] = CMI_IPCDA_MESSAGEQ_NOTIFY;

    clock_gettime(CLOCK_REALTIME, &sMessageQWaitTime);
    sMessageQWaitTime.tv_sec += cmuProperty::getIPCDAMessageQTimeout();
        
    IDE_TEST( mq_timedsend( aLink->mMessageQ.mId,
                            sSendBuf,
                            aLink->mMessageQ.mAttr.mq_msgsize,
                            0,
                            &sMessageQWaitTime) != 0);

    IDE_EXCEPTION_CONT(ContMQNOtify);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

/*************************************************************
 * proj_2160 cm_type removal
 * 1. �� �Լ��� �ݹ� �ȿ��� ���� ��Ŷ�� ���������� �����ϴ� ��쿡
 *  ����ϱ� ���� ���������.
 * 2. cmiRecv()���� �������� �ݹ��� ȣ���ϴ� �ݺ����� ����
*************************************************************/
IDE_RC cmiRecvNext(cmiProtocolContext* aCtx, PDL_Time_Value* aTimeout)
{
    cmpHeader*           sHeader = &(aCtx->mReadHeader);
    UInt                 sCmSeqNo;

    aCtx->mReadBlock->mDataSize  = 0;
    aCtx->mReadBlock->mCursor    = 0;

    IDE_TEST_RAISE(aCtx->mIsDisconnect == ID_TRUE, Disconnected);
    IDE_TEST(aCtx->mLink->mPeerOp->mRecv(aCtx->mLink,
                                         &aCtx->mReadBlock,
                                         sHeader, 
                                         aTimeout) != IDE_SUCCESS);
    
    /* BUG-45184 */
    aCtx->mReceiveDataSize += aCtx->mReadBlock->mDataSize;
    aCtx->mReceiveDataCount++;
    
    sCmSeqNo = CMP_HEADER_SEQ_NO(sHeader);
#ifdef CMI_DUMP
    ideLog::logLine(IDE_CM_4, "[%5d] recv [%5d]",
                    sCmSeqNo, sHeader->mA7.mPayloadLength);
#endif

    /* BUG-41909 Add dump CM block when a packet error occurs */
    IDU_FIT_POINT_RAISE( "cmiRecvNext::Server::InvalidProtocolSeqNo", InvalidProtocolSeqNo );

    // ��� ��Ŷ�� ���ǳ��ּ� �����Ϸù�ȣ�� ���´�.
    // ����: 0 ~ 0x7fffffff, �ִ밪�� �ٴٸ��� 0���� �ٽ� ���۵ȴ�
    IDE_TEST_RAISE(sCmSeqNo != aCtx->mCmSeqNo, InvalidProtocolSeqNo);
    if (aCtx->mCmSeqNo == CMP_HEADER_MAX_SEQ_NO)
    {
        aCtx->mCmSeqNo = 0;
    }
    else
    {
        aCtx->mCmSeqNo++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(Disconnected);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION(InvalidProtocolSeqNo);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_PROTOCOL_SEQUENCE));

        cmiDump(aCtx, sHeader, aCtx->mReadBlock, 0, aCtx->mReadBlock->mDataSize);
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
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
IDE_RC cmiSend( cmiProtocolContext  * aCtx, 
                idBool                aIsEnd, 
                PDL_Time_Value      * aTimeout )
{
    cmnLinkPeer *sLink   = aCtx->mLink;
    cmbBlock    *sBlock  = aCtx->mWriteBlock;
    cmpHeader   *sHeader = &aCtx->mWriteHeader;
    cmbPool     *sPool   = aCtx->mLink->mPool;

    cmbBlock    *sPendingBlock;
    iduListNode *sIterator;
    iduListNode *sNodeNext;
    idBool       sSendSuccess;
    idBool       sNeedToSave = ID_FALSE;
    cmbBlock    *sNewBlock;

    UInt         sCmSeqNo = 0;
    UInt         sSendDataSize = 0;

    IDE_TEST_CONT((sLink->mLink.mImpl == CMN_LINK_IMPL_IPCDA), noDataToSend);
    IDE_TEST_CONT(sBlock->mCursor == CMP_HEADER_SIZE, noDataToSend);

    if (aIsEnd == ID_TRUE)
    {
        CMP_HEADER_SET_PROTO_END(sHeader);
    }
    else
    {
        CMP_HEADER_CLR_PROTO_END(sHeader);
    }

    CMP_HEADER_FLAG_CLR_COMPRESS( sHeader );

    /* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
    IDE_TEST( cmiCompressCmBlock( sHeader, 
                                  aCtx,
                                  sBlock ) 
              != IDE_SUCCESS );

    /* BUG-38102 */
    if ( aCtx->mEncryptFlag == ID_TRUE )
    {
        IDE_TEST( cmiEncryptCmBlock( sBlock ) != IDE_SUCCESS );

        CMP_HEADER_FLAG_SET_ENCRYPT( sHeader );        
    }
    else
    {
        CMP_HEADER_FLAG_CLR_ENCRYPT( sHeader );
    }

    sBlock->mDataSize = sBlock->mCursor;
    IDE_TEST(cmpHeaderWrite(sHeader, sBlock) != IDE_SUCCESS);
    sNeedToSave = ID_TRUE;
    
    IDU_FIT_POINT_RAISE( "cmiSend::Server::ideIsRetry", SendFail );

    // Pending Write Block���� ���� (send previous packets)
    IDU_LIST_ITERATE_SAFE(&aCtx->mWriteBlockList, sIterator, sNodeNext)
    {
        sPendingBlock = (cmbBlock *)sIterator->mObj;

        sSendSuccess = ID_TRUE;

        sSendDataSize = sPendingBlock->mDataSize;
        // BUG-19465 : CM_Buffer�� pending list�� ����
        while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != IDE_SUCCESS)
        {
            sSendSuccess = ID_FALSE;

            IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);

            /* BUG-44468 [cm] codesonar warning in CM */
            if( aCtx->mListLength < gMaxPendingList )
            {
                break;
            }

            // bug-27250 IPC linklist can be crushed.
            
            IDE_TEST( cmiDispatcherWaitLink( (cmiLink*)sLink,
                                             CMN_DIRECTION_WR,
                                             aTimeout )
                      != IDE_SUCCESS );

            sSendSuccess = ID_TRUE;
        }

        if (sSendSuccess == ID_FALSE)
        {
            break;
        }

        /* BUG-45184 */
        aCtx->mSendDataSize += sSendDataSize;
        aCtx->mSendDataCount++;

        IDE_TEST(sPool->mOp->mFreeBlock(sPool, sPendingBlock) != IDE_SUCCESS);
        aCtx->mListLength--;
    }

    // send current block if there is no pendng block
    if (sIterator == &aCtx->mWriteBlockList)
    {
        sSendDataSize = sBlock->mDataSize;
        if (sLink->mPeerOp->mSend(sLink, sBlock) != IDE_SUCCESS)
        {
            IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);
            sNeedToSave = ID_TRUE;
        }
        else
        {
            sNeedToSave = ID_FALSE;
            
            /* BUG-45184 */
            aCtx->mSendDataSize += sSendDataSize;
            aCtx->mSendDataCount++;
        }
    }
    else
    {
        sNeedToSave = ID_TRUE;
    }

    // ���� block�� pendingList �ǵڿ� ������ �д�
    if (sNeedToSave == ID_TRUE)
    {
        sNewBlock = NULL;
        IDE_TEST(sPool->mOp->mAllocBlock(sPool, &sNewBlock) != IDE_SUCCESS);
        sNewBlock->mDataSize = sNewBlock->mCursor = CMP_HEADER_SIZE;
        IDU_LIST_ADD_LAST(&aCtx->mWriteBlockList, &sBlock->mListNode);
        aCtx->mWriteBlock = sNewBlock;
        sBlock            = sNewBlock;
        aCtx->mListLength++;
        sNeedToSave = ID_FALSE;
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

    if (aIsEnd == ID_TRUE)
    {
        // �������� ���̶�� ��� Block�� ���۵Ǿ�� ��
        IDU_LIST_ITERATE_SAFE(&aCtx->mWriteBlockList, sIterator, sNodeNext)
        {
            sPendingBlock = (cmbBlock *)sIterator->mObj;

            sSendDataSize = sPendingBlock->mDataSize;
            while (sLink->mPeerOp->mSend(sLink, sPendingBlock) != IDE_SUCCESS)
            {
                IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, SendFail);

                IDE_TEST( cmiDispatcherWaitLink( (cmiLink*)sLink,
                                                 CMN_DIRECTION_WR,
                                                 aTimeout )
                          != IDE_SUCCESS );
            }
            
            /* BUG-45184 */
            aCtx->mSendDataSize += sSendDataSize;
            aCtx->mSendDataCount++;

            IDE_TEST(sPool->mOp->mFreeBlock(sPool, sPendingBlock) != IDE_SUCCESS);
            aCtx->mListLength--;
        }

        // for IPC
        sLink->mPeerOp->mReqComplete(sLink);
    }

    IDE_EXCEPTION_CONT(noDataToSend);
    sBlock->mDataSize = sBlock->mCursor = CMP_HEADER_SIZE;
    return IDE_SUCCESS;

    IDE_EXCEPTION(SendFail);
    {
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if( ( sNeedToSave == ID_TRUE ) &&
        ( aCtx->mListLength >= gMaxPendingList ) ) /* BUG-44468 [cm] codesonar warning in CM */
    {
        aCtx->mResendBlock = ID_FALSE;
    }
    else
    {
        /* do nothing */
    }

    /*
     *  BUG-38716 
     *  [rp-sender] It needs a property to give sending timeout to replication sender. 
     */
    if ( ( sNeedToSave == ID_TRUE ) && 
         ( aCtx->mResendBlock == ID_TRUE ) ) 
    {
        sNewBlock = NULL;
        (void)sPool->mOp->mAllocBlock(sPool, &sNewBlock);
        sNewBlock->mDataSize = sNewBlock->mCursor = CMP_HEADER_SIZE;
        IDU_LIST_ADD_LAST(&aCtx->mWriteBlockList, &sBlock->mListNode);
        aCtx->mWriteBlock = sNewBlock;
        sBlock            = sNewBlock;
        aCtx->mListLength++;
    }
    else
    {
        /* do nohting */
    }

    IDE_POP();

    sBlock->mDataSize = sBlock->mCursor = CMP_HEADER_SIZE;
    return IDE_FAILURE;
}

/*************************************************************
 * BUG-46163 IPCDA Send
 * IPCDA���� Server -> Client�� ���� Protocol ������
 * �Ϸ� �Ǿ����� ȣ���Ѵ�.
*************************************************************/
IDE_RC cmiLinkPeerFinalizeSvrForIPCDA( cmiProtocolContext  * aCtx )
{
    cmnLinkPeerIPCDA  *sLinkIPCDA = NULL;

    sLinkIPCDA = (cmnLinkPeerIPCDA *)aCtx->mLink;

    IDE_TEST_RAISE(aCtx->mIsDisconnect == ID_TRUE, Disconnected);

    /* BUG-46163 �̹� �����ʹ� shard memory�� copy�Ǿ� �����Ƿ� �Ϸ� ��ȣ�� ������. */
#if defined(ALTI_CFG_OS_LINUX)
    /* message queue */
    if( sLinkIPCDA->mMessageQ.mNeedToNotify == ID_TRUE )
    {
        sLinkIPCDA->mMessageQ.mNeedToNotify = ID_FALSE;
        IDE_TEST(cmiMessageQNotify(sLinkIPCDA) != IDE_SUCCESS);
    }
#endif

    /* BUG-46502 */
    if (aCtx->mWriteBlock != NULL)
    {
        /* Write marking for end-of-protocol. */
        CMI_WOP(aCtx, CMP_OP_DB_IPCDALastOpEnded);
        cmiIPCDAIncDataCount(aCtx);
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mWriteBlock)->mWFlag), CMB_IPCDA_SHM_DEACTIVATED );
    }

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    if (aCtx->mReadBlock != NULL)
    {
        /* BUG-46502 operationCount �ʱ�ȭ */
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mReadBlock)->mOperationCount), 0 );

        /* BUG-46502 atomic �Լ� ���� */
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mReadBlock)->mRFlag), CMB_IPCDA_SHM_DEACTIVATED );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(Disconnected);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_CONNECTION_CLOSED));
    }
    IDE_EXCEPTION_END;

    /* BUG-46502 */
    if (aCtx->mWriteBlock != NULL)
    {
        /* Write marking for end-of-protocol. */
        CMI_WOP(aCtx, CMP_OP_DB_IPCDALastOpEnded);
        cmiIPCDAIncDataCount(aCtx);
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mWriteBlock)->mWFlag), CMB_IPCDA_SHM_DEACTIVATED);
    }

    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    if (aCtx->mReadBlock != NULL)
    {
        /* BUG-46502 operationCount �ʱ�ȭ */
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mReadBlock)->mOperationCount), 0 );

        /* BUG-46502 atomic �Լ� ���� */
        acpAtomicSet32( &(((cmbBlockIPCDA *)aCtx->mReadBlock)->mRFlag), CMB_IPCDA_SHM_DEACTIVATED );
    }
    
    return IDE_FAILURE;
}

/*
 *  BUG-38716 
 *  [rp-sender] It needs a property to give sending timeout to replication sender. 
 *
 *  Pending Block �� ����Ǿ� �ִ� Block ���� ���� �մϴ�.
 *  ���� RP ������ ���
 */
IDE_RC cmiFlushPendingBlock( cmiProtocolContext * aCtx,
                             PDL_Time_Value     * aTimeout )
{
    cmnLinkPeer     * sLink = aCtx->mLink;
    cmbPool         * sPool = aCtx->mLink->mPool;

    cmbBlock        * sPendingBlock = NULL;
    iduListNode     * sIterator = NULL;
    iduListNode     * sNodeNext = NULL;
    UInt              sSendDataSize = 0;

    IDU_LIST_ITERATE_SAFE( &aCtx->mWriteBlockList, sIterator, sNodeNext )
    {
        sPendingBlock = (cmbBlock *)sIterator->mObj;

        sSendDataSize = sPendingBlock->mDataSize;
        while ( sLink->mPeerOp->mSend( sLink, sPendingBlock ) != IDE_SUCCESS )
        {
            IDE_TEST( ideIsRetry() != IDE_SUCCESS );

            IDE_TEST( cmnDispatcherWaitLink( (cmiLink *)sLink,
                                             CMN_DIRECTION_WR,
                                             aTimeout ) 
                      != IDE_SUCCESS );
        }
        
        /* BUG-45184 */
        aCtx->mSendDataSize += sSendDataSize;
        aCtx->mSendDataCount++;
        
        IDE_TEST( sPool->mOp->mFreeBlock( sPool, sPendingBlock ) != IDE_SUCCESS );
        aCtx->mListLength--;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC cmiCheckAndFlush( cmiProtocolContext * aProtocolContext,
                         UInt aLen,
                         idBool aIsEnd,
                         PDL_Time_Value * aTimeout )
{
    if ( aProtocolContext->mWriteBlock->mCursor + aLen >
         aProtocolContext->mWriteBlock->mBlockSize )
    {
        IDE_TEST( cmiSend( aProtocolContext, 
                           aIsEnd, 
                           aTimeout ) 
                  != IDE_SUCCESS );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSplitRead( cmiProtocolContext *aCtx,
                     ULong               aReadSize,
                     UChar              *aBuffer,
                     PDL_Time_Value     *aTimeout )
{
    UChar *sBuffer      = aBuffer;
    ULong  sReadSize    = aReadSize;
    UInt   sRemainSize  = aCtx->mReadBlock->mDataSize -
                          aCtx->mReadBlock->mCursor;
    UInt   sCopySize;

    while( sReadSize > sRemainSize )
    {
        sCopySize = IDL_MIN( sReadSize, sRemainSize );

        CMI_RCP ( aCtx, sBuffer, sCopySize );
        IDE_TEST( cmiRecvNext( aCtx, aTimeout ) != IDE_SUCCESS );

        sRemainSize = aCtx->mReadBlock->mDataSize - CMP_HEADER_SIZE;
        sBuffer    += sCopySize;
        sReadSize  -= sCopySize;
    }
    CMI_RCP( aCtx, sBuffer, sReadSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSplitSkipRead( cmiProtocolContext *aCtx,
                         ULong               aReadSize,
                         PDL_Time_Value     *aTimeout )
{
    ULong  sReadSize    = aReadSize;
    UInt   sRemainSize  = 0;
    UInt   sCopySize    = 0;

    if (cmiGetLinkImpl(aCtx) == CMI_LINK_IMPL_IPCDA)
    {
        IDE_TEST( aCtx->mReadBlock->mCursor + aReadSize >= CMB_BLOCK_DEFAULT_SIZE);
    }
    else
    {
        sRemainSize = aCtx->mReadBlock->mDataSize - aCtx->mReadBlock->mCursor;
        while( sReadSize > sRemainSize )
        {
            IDE_TEST( cmiRecvNext( aCtx, aTimeout ) != IDE_SUCCESS );

            sCopySize = IDL_MIN( sReadSize, sRemainSize );

            sRemainSize = aCtx->mReadBlock->mDataSize - CMP_HEADER_SIZE;
            sReadSize  -= sCopySize;
        }
    }
    aCtx->mReadBlock->mCursor += sReadSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiSplitWrite( cmiProtocolContext *aCtx,
                      ULong               aWriteSize,
                      UChar              *aBuffer )
{
    UChar *sBuffer      = aBuffer;
    ULong  sWriteSize   = aWriteSize;
    UInt   sRemainSize  = 0;
    UInt   sCopySize    = 0;

    if(cmiGetLinkImpl(aCtx) == CMI_LINK_IMPL_IPCDA)
    {
        IDE_TEST( aCtx->mWriteBlock->mCursor + aWriteSize >= CMB_BLOCK_DEFAULT_SIZE);
    }
    else
    {
        sRemainSize  = aCtx->mWriteBlock->mBlockSize - aCtx->mWriteBlock->mCursor;
        while( sWriteSize > sRemainSize )
        {
            sCopySize = IDL_MIN( sWriteSize, sRemainSize );

            CMI_WCP ( aCtx, sBuffer, sCopySize );
            IDE_TEST( cmiSend ( aCtx, ID_FALSE ) != IDE_SUCCESS );

            sRemainSize  = CMB_BLOCK_DEFAULT_SIZE - CMP_HEADER_SIZE;
            sBuffer     += sCopySize;
            sWriteSize  -= sCopySize;
        }
    }
    CMI_WCP( aCtx, sBuffer, sWriteSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-5894 Permit sysdba via IPC */
IDE_RC cmiPermitConnection(cmiLink *aLink,
                           idBool   aHasSysdba,
                           idBool   aIsSysdba)
{
    cmnLinkPeer *sLink = (cmnLinkPeer *)aLink;

    /*
     * Peer Type �˻�
     */
    IDE_ASSERT(aLink->mType == CMN_LINK_TYPE_PEER_SERVER);

    /* IPC������ mPermitConnection�� üũ�ϸ� �ȴ� */
    if (sLink->mPeerOp->mPermitConnection != NULL)
    {
        IDE_TEST(sLink->mPeerOp->mPermitConnection(sLink,
                                                   aHasSysdba,
                                                   aIsSysdba) != IDE_SUCCESS);
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

cmnLinkImpl cmiGetLinkImpl(cmiLink *aLink)
{
    return aLink->mImpl;
}

/* bug-33841: ipc thread's state is wrongly displayed
   mmtThreadManager���� mmtServiceThread::setExecuteCallback�� ��� */
IDE_RC cmiSetCallbackSetExecute(cmiCallbackSetExecute aCallback)
{
    gCMCallbackSetExecute = aCallback;
    return IDE_SUCCESS;
}

/* BUG-48871 ���� ��� LZ4�� ���� �� ����ȭ ���� ȣȯ�� ���� */
/*
 *
 */ 
void cmiEnableCompress( cmiProtocolContext * aCtx,
                        UInt                 aLevel,
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

/* 
 * BUG-38102: Add a replication functionality for XLog encryption 
 */ 
void cmiEnableEncrypt( cmiProtocolContext * aCtx )
{
    aCtx->mEncryptFlag = ID_TRUE;
}

/*
 * BUG-38102: Add a replication functionality for XLog encryption
 */ 
void cmiDisableEncrypt( cmiProtocolContext * aCtx )
{
    aCtx->mEncryptFlag = ID_FALSE;    
}

idBool cmiIsResendBlock( cmiProtocolContext * aProtocolContext )
{
    return aProtocolContext->mResendBlock;
}

void cmiLinkSetPacketTypeA5( cmiLink *aLink )
{
    IDE_DASSERT( aLink != NULL );

    ((cmnLink*)aLink)->mPacketType = CMP_PACKET_TYPE_A5;
}

/**
 *  cmiDump
 *
 *  Ctx, Packet�� ���� �߻��� ������ ������ �����Ѵ�.
 *
 *  @aCtx       : cmiProtocolContext
 *  @aHeader    : CM Packet Header
 *  @aBlock     : CM Packet Block
 *  @aFromIndex : ����� ���� Index
 *  @aLen       : ����� Length
 */
static void cmiDump(cmiProtocolContext   *aCtx,
                    cmpHeader            *aHeader,
                    cmbBlock             *aBlock,
                    UInt                  aFromIndex,
                    UInt                  aLen)
{
    /* BUG-41909 Add dump CM block when a packet error occurs */
    SChar *sHexBuf      = NULL;
    UInt   sHexBufIndex = 0;
    /* 
     * One Line Size = 16 * 3 + 11 = 59
     * Line Count    = 2^15(CMB_BLOCK_DEFAULT_SIZE) / 2^4 = 2^11
     * Needed Buffer = 59 * 2^11 = 120832byte
     *
     * 32KB ��Ŷ ��ü�� ����ϱ� ���ؼ��� 120KB�� �ʿ��ϴ�.
     * CTX, HEADER� ����ϱ� ������ 128KB ������ ���� ����ϴ�.
     * 1byte�� HEX ������ ����ϱ� ���ؼ� �뷫 4byte�� �ʿ��� ���̴�.
     */
    UInt   sHexBufSize  = CMB_BLOCK_DEFAULT_SIZE * 4;
    UInt   sToIndex     = aFromIndex + aLen - 1;
    UInt   sAddr        = 0;
    UInt   i;

    IDE_TEST_CONT( (aCtx == NULL) || (aHeader == NULL) || (aBlock == NULL), NO_NEED_WORK);

    /*
     * cmiProtocolContext�� HexBuffer�� �޸� Session���� 128KB ���۸� ������ �Ѵ�.
     * Packet error�� ���� �幮 ��Ȳ�̹Ƿ� ���� ����� �� �� �Ҵ��ؼ� ����.
     */
    IDU_FIT_POINT_RAISE( "cmiDump::Server::MemAllocError", MemAllocError );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_CMI,
                                     sHexBufSize,
                                     (void **)&sHexBuf,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, MemAllocError);

    /* Ctx */
    sHexBufIndex += idlOS::snprintf(&sHexBuf[sHexBufIndex], sHexBufSize - sHexBufIndex,
                                    CM_TRC_DUMP_CTX,
                                    aCtx->mCmSeqNo);

    /* Header */
    sHexBufIndex += idlOS::snprintf(&sHexBuf[sHexBufIndex], sHexBufSize - sHexBufIndex,
                                    CM_TRC_DUMP_PACKET_HEADER,
                                    aHeader->mA7.mHeaderSign,
                                    aHeader->mA7.mPayloadLength,
                                    CMP_HEADER_SEQ_NO(aHeader),
                                    aBlock->mDataSize);

    /* MSG ���Ͽ��� %02X�� �νĸ��ϴ� ���װ� �ִ�. %x�� �ν��Ѵ�. */
    sHexBufIndex += idlOS::snprintf(&sHexBuf[sHexBufIndex], sHexBufSize - sHexBufIndex,
                                    "# BLOCK HEADER  = "
                                    "%02X %02X %02X%02X %02X%02X%02X%02X "
                                    "%02X%02X %02X%02X %02X%02X%02X%02X\n",
                                    aBlock->mData[0],
                                    aBlock->mData[1],
                                    aBlock->mData[2],
                                    aBlock->mData[3],
                                    aBlock->mData[4],
                                    aBlock->mData[5],
                                    aBlock->mData[6],
                                    aBlock->mData[7],
                                    aBlock->mData[8],
                                    aBlock->mData[9],
                                    aBlock->mData[10],
                                    aBlock->mData[11],
                                    aBlock->mData[12],
                                    aBlock->mData[13],
                                    aBlock->mData[14],
                                    aBlock->mData[15]);

    IDE_TEST_CONT( (aFromIndex >= CMB_BLOCK_DEFAULT_SIZE) ||
                   (aLen       >  CMB_BLOCK_DEFAULT_SIZE) ||
                   (sToIndex   >= CMB_BLOCK_DEFAULT_SIZE),
                   NO_NEED_WORK );

    /* Payload */
    sHexBufIndex += idlOS::snprintf(&sHexBuf[sHexBufIndex], sHexBufSize - sHexBufIndex,
                                    CM_TRC_DUMP_PACKET_PAYLOAD,
                                    aFromIndex,
                                    sToIndex);

    for (i = aFromIndex, sAddr = 0; i <= sToIndex; i++)
    {
        /* �� ���ο� HEX�� 16���� ����Ѵ�. */
        if (sAddr % 16 == 0)
        {
            sHexBufIndex += idlOS::snprintf(&sHexBuf[sHexBufIndex], sHexBufSize - sHexBufIndex, "\n%08X: ", sAddr);
        }
        else
        {
            /* Nothing */
        }
        sAddr += 1;

        sHexBuf[sHexBufIndex++] = "0123456789ABCDEF"[aBlock->mData[i] >> 4 & 0xF];
        sHexBuf[sHexBufIndex++] = "0123456789ABCDEF"[aBlock->mData[i]      & 0xF];
        sHexBuf[sHexBufIndex++] = ' ';
    }

    IDE_EXCEPTION_CONT(NO_NEED_WORK);

    if (sHexBufIndex > 0)
    {
        sHexBuf[sHexBufIndex] = '\0';
        ideLog::log(IDE_CM_1, "%s", sHexBuf);
    }
    else
    {
        /* Nothing */
    }

    /* �޸� �Ҵ� ���н� TRC Log�� ����ϰ� �Ѿ�� */
    IDE_EXCEPTION(MemAllocError)
    {
        ideLog::log(IDE_CM_0, CM_TRC_MEM_ALLOC_ERROR, errno, sHexBufSize);
    }
    IDE_EXCEPTION_END;

    if (sHexBuf != NULL)
    {
        IDE_ASSERT(iduMemMgr::free(sHexBuf) == IDE_SUCCESS)
    }

    return;
}

/* PROJ-2474 SSL/TLS, BUG-44488 */
IDE_RC cmiSslInitialize( void )
{
#if !defined(CM_DISABLE_SSL)
    return cmnOpenssl::initialize();
#endif
    return IDE_SUCCESS;
}

IDE_RC cmiSslFinalize( void )
{
#if !defined(CM_DISABLE_SSL)
    return cmnOpenssl::destroy();
#endif
    return IDE_SUCCESS;
}

/* PROJ-2681 */
IDE_RC cmiIBInitialize(void)
{
    IDE_TEST_RAISE(cmnIBInitialize() != ACI_SUCCESS, InitFailed);

    ideLog::log(IDE_SERVER_0, CM_TRC_IB_INITIALIZED);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InitFailed)
    {
        IDE_SET(ideSetErrorCodeAndMsg(aciGetErrorCode(), aciGetErrorMsg(aciGetErrorCode())));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmiIBFinalize(void)
{
    ACI_TEST(cmnIBFinalize() != ACI_SUCCESS);

    return IDE_SUCCESS;

    ACI_EXCEPTION_END;

    return IDE_FAILURE;
}

