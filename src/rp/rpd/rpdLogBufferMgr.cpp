/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: rpdLogBufferMgr.cpp 19967 2007-01-18 03:09:58Z lswhh $
 **********************************************************************/

#include <rpxSender.h>
#include <rpdLogBufferMgr.h>

IDE_RC rpdLogBufferMgr::initialize(UInt aSize, UInt aMaxSenderCnt)
{
    UInt      sIdx;
    smiLogHdr sLogHead;

    /*Buffer Info*/
    mWritableFlag = RP_OFF;
    mBufSize      = aSize;
    mEndPtr       = mBufSize - 1;
    mWrotePtr     = mEndPtr;
    mWritePtr     = getWritePtr(mWrotePtr);
    /*�ƹ��͵� �������� �ʴٸ� mWrotePtr�� mMinReadPtr�� ����*/
    mMinReadPtr   = mWrotePtr;

    mMinSN     = SM_SN_NULL;
    mMaxSN     = SM_SN_NULL;

    mMaxSenderCnt    = aMaxSenderCnt;
    mAccessInfoCnt   = 0;
    mAccessInfoList  = NULL;
    mBasePtr         = NULL;

    IDE_TEST_RAISE(mBufInfoMutex.initialize((SChar *)"RP_LOGBUFER_BUFINFO_MUTEX",
                                            IDU_MUTEX_KIND_POSIX,
                                            IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    IDU_FIT_POINT_RAISE( "rpdLogBufferMgr::initialize::calloc::AccessInfoList",
                          ERR_MEMORY_ALLOC_ACCESS_INFO_LIST );
    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_RP_RPD,
                                     mMaxSenderCnt,
                                     ID_SIZEOF(AccessingInfo),
                                     (void**)&mAccessInfoList,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_ACCESS_INFO_LIST);

    for(sIdx = 0; sIdx < mMaxSenderCnt; sIdx++)
    {
        mAccessInfoList[sIdx].mReadPtr = RP_BUF_NULL_PTR;
        mAccessInfoList[sIdx].mNextPtr = RP_BUF_NULL_PTR;
    }

    IDU_FIT_POINT_RAISE( "rpdLogBufferMgr::initialize::malloc::Buf",
                          ERR_BUF_ALLOC );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                     mBufSize,
                                     (void **)&mBasePtr,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_BUF_ALLOC);

    smiLogRec::initializeDummyLog(&mBufferEndLog);
    smiLogRec::getLogHdr(&mBufferEndLog, &sLogHead);
    mBufferEndLogSize = smiLogRec::getLogSizeFromLogHdr(&sLogHead);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MUTEX_INIT);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_FATAL_ThrMutexInit));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Mutex initialization error");
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_ACCESS_INFO_LIST);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdLogBufferMgr::initialize",
                                "mAccessInfoList"));
    }
    IDE_EXCEPTION(ERR_BUF_ALLOC);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_LOGBUFFER_ALLOC));
    }
    IDE_EXCEPTION_END;
    IDE_ERRLOG(IDE_RP_0);
    IDE_PUSH();

    (void)mBufInfoMutex.destroy();

    if(mAccessInfoList != NULL)
    {
        (void)iduMemMgr::free(mAccessInfoList);
        mAccessInfoList = NULL;
    }
    if(mBasePtr != NULL)
    {
        (void)iduMemMgr::free(mBasePtr);
        mBasePtr = NULL;
    }

    IDE_POP();
    return IDE_FAILURE;
}

void rpdLogBufferMgr::destroy()
{
    if(mBufInfoMutex.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mAccessInfoList != NULL)
    {
        (void)iduMemMgr::free(mAccessInfoList);
        mAccessInfoList = NULL;
    }

    if(mBasePtr != NULL)
    {
        (void)iduMemMgr::free(mBasePtr);
        mBasePtr = NULL;
    }

    return;
}

void rpdLogBufferMgr::finalize()
{
}

IDE_RC rpdLogBufferMgr::copyToRPLogBuf(idvSQL * aStatistics,
                                       UInt     aSize,
                                       SChar*   aLogPtr,
                                       smLSN    aLSN)
{
    smSN        sTmpSN    = SM_SN_NULL;
    SChar*      sCopyStartPtr = 0;
    smiLogRec   sLog;
    smiLogHdr   sLogHead;
    UInt        sWrotePtr = ID_UINT_MAX;
    flagSwitch  sWritableFlag = RP_OFF;
    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_S_COPY_LOG_TO_REPLBUFFER);

    if(aStatistics != NULL)
    {
        RP_OPTIMIZE_TIME_BEGIN(aStatistics, IDV_OPTM_INDEX_RP_S_COPY_LOG_TO_REPLBUFFER)
    }

   /* Service Thread�� ������ ������ ������ ��쿡 mWritableFlag��
    * RP_OFF�� Set�ϸ� �� ������ ���縦 �� �� ����.
    * �׷��� �ʰ� ON�� ��� ���縦 �õ��Ѵ�
    * mWritableFlag�� Service Thread���� �����ϱ� ������
    * Lock�� �����ʰ� Ȯ���� �� �ִ�.
    * (Service Thread�� ���� ����ȭ�� Log File Lock���� ��������)*/
    if(mWritableFlag == RP_OFF)
    {
        /*�� ��� Accessing Info List Count�� Ȯ���Ͽ� �ٽ� ON���� ���� �������� Ȯ�θ� �Ѵ�.*/
        IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
        if(mAccessInfoCnt == 0)
        {
            mMinSN      = SM_SN_NULL;
            mWrotePtr   = mEndPtr;
            mMinReadPtr = mWrotePtr;
            IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
            mWritePtr   = getWritePtr(mWrotePtr);
            sWritableFlag = RP_ON;
        }
        else
        {
            IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
            /*do nothing, retain previous values */
            sWritableFlag = RP_OFF;
        }
    }
    else
    {
        sWritableFlag = RP_ON;
    }

    if(sWritableFlag == RP_ON)
    {
        smiLogRec::getLogHdr(aLogPtr, &sLogHead);
        sTmpSN = smiLogRec::getSNFromLogHdr(&sLogHead);

        // PROJ-1705 �Ķ���� �߰�
        sLog.initialize(NULL,
                        NULL,
                        RPU_REPLICATION_POOL_ELEMENT_SIZE);

       /* Replication�� �ʿ���ϴ� �α����� Ȯ���ϰ� �ʿ���ϴ�
        * �α��� ��� RP Log Buf�� copy�Ѵ�.*/
        if ( sLog.needReplicationByType( &sLogHead,
                                         aLogPtr,
                                         &aLSN ) == ID_TRUE )
        {
            IDE_TEST_RAISE(((aSize + mBufferEndLogSize) > mBufSize), ERR_OVERFLOW_RPBUF);

            /*if not enough space, write buffer end Log */
            if((mWritePtr + aSize + mBufferEndLogSize) > mBufSize)
            {
                /*mWrotePtr�� mEndPtr�� �����*/
                IDE_TEST(writeBufferEndLogToRPBuf() != IDE_SUCCESS);
            }

            /* write�� �޸𸮰� Sender�� ���� ������ �ּҸ� overwrite�Ѵ�.
             * sWritePtr <= mMinReadPtr < sWritePtr + aSize */
            if((mWritePtr <= mMinReadPtr) &&
               (mMinReadPtr < mWritePtr + aSize))
            {
                updateMinReadPtr();                  //mMinReadPtr�� �ٽ� ���
                if((mWritePtr <= mMinReadPtr) &&
                   (mMinReadPtr < mWritePtr + aSize))//���̻� ������ ������ ����
                {
                    IDE_RAISE(ERR_NO_SPACE);
                }
            }
            /*Log�� Buf�� Copy�Ѵ�.*/
            sCopyStartPtr = (SChar*)(mBasePtr + mWritePtr);
            idlOS::memcpy(sCopyStartPtr, aLogPtr, aSize);

            /* BUG-22098 Buffer�� ������ �Ŀ� mWrotePtr�� �����ؾ� �Ѵ�. */
            IDL_MEM_BARRIER;

            /*sWirtePtr�� �����ϱ� ������ aSize -1�� ��*/
            sWrotePtr = mWritePtr + (aSize - 1);
        }
        else
        {
            /*retain previous wroteptr value*/
            sWrotePtr = mWrotePtr;
        }

        IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
        mWrotePtr = sWrotePtr;
        if(mMinSN == SM_SN_NULL)
        {
            mMinSN = sTmpSN;
        }
        mMaxSN = sTmpSN;
        SM_GET_LSN(mMaxLSN, aLSN);//���� �α��� aLSN�� ���� mMaxLSN�� copy
        mWritableFlag = RP_ON;
        IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
        mWritePtr = getWritePtr(mWrotePtr);
    }
    else
    {
        /*nothing to do*/
    }
    if(aStatistics != NULL)
    {
        RP_OPTIMIZE_TIME_END(aStatistics, IDV_OPTM_INDEX_RP_S_COPY_LOG_TO_REPLBUFFER);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OVERFLOW_RPBUF);
    {
        ideLog::log(IDE_RP_4, RP_TRC_LB_OVERFLOW_RPBUF);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_OVERFLOW));
    }
    IDE_EXCEPTION(ERR_NO_SPACE);
    {
        ideLog::log(IDE_RP_4, RP_TRC_LB_NO_SPACE);
        IDE_SET(ideSetErrorCode(rpERR_IGNORE_RP_NO_SPACE));
    }

    IDE_EXCEPTION_END;

    if(aStatistics != NULL)
    {
        RP_OPTIMIZE_TIME_END(aStatistics, IDV_OPTM_INDEX_RP_S_COPY_LOG_TO_REPLBUFFER);
    }
    if(mWritableFlag == RP_ON)
    {
        IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
        mWritableFlag = RP_OFF;
        IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
    }
    return IDE_FAILURE;
}

/*sender�� �а� �ִ� �޸� ������ Write���� ���ϸ�
 *IDE_FAILURE�� ��ȯ�Ѵ�
 */
IDE_RC rpdLogBufferMgr::writeBufferEndLogToRPBuf()
{
    SChar* sWriteStartPtr = NULL;
    UInt   sRemainBufSize = mEndPtr - mWrotePtr;
    /* write�� �޸𸮰� Sender�� �а� �ִ� �ּҸ� overwrite�Ѵ�.
     * mWritePtr <= mMinReadPtr <= mEndPtr
     */

    if((mWritePtr <= mMinReadPtr) && (mMinReadPtr <= mEndPtr))
    {
        updateMinReadPtr(); //cache�Ǿ��ִ� �����̹Ƿ� �ٽ� ���

        if((mWritePtr <= mMinReadPtr) && (mMinReadPtr <= mEndPtr))
        {
            IDE_RAISE(ERR_NO_SPACE); //�޸𸮰� ������
        }
    }
    //�ƹ��α׵� �Ⱦ����� ���¿��� buffer end�� ���� �� ����
    IDE_DASSERT(mMaxSN != SM_SN_NULL);

    /*write: buffer end log�� �������� �αװ� �ƴϰ� RP Buf���� �ӽ÷� ���� ���̴�.
     *�� �α״� ���� �������� �������� ���� SN�� ���� �����Ƿ� ���� �ֱٿ� ó���� SN�� Set�Ѵ�.
     *�� SN�� min ptr update�� ����Ѵ�.*/
    smiLogRec::setSNOfDummyLog(&mBufferEndLog, mMaxSN);
    smiLogRec::setLogSizeOfDummyLog(&mBufferEndLog, sRemainBufSize);
    sWriteStartPtr = (SChar*)(mBasePtr + mWritePtr);
    idlOS::memcpy(sWriteStartPtr, &mBufferEndLog, mBufferEndLogSize);

    /* BUG-32475 Buffer�� ������ �Ŀ� mWrotePtr�� �����ؾ� �Ѵ�. */
    IDL_MEM_BARRIER;

    IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
    mWrotePtr = mEndPtr;
    IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
    mWritePtr = getWritePtr(mWrotePtr);
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_SPACE);
    {
        ideLog::log(IDE_RP_4, RP_TRC_LB_NO_SPACE);
        IDE_SET(ideSetErrorCode(rpERR_IGNORE_RP_NO_SPACE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void rpdLogBufferMgr::updateMinReadPtr()
{
    UInt sIdx;
    UInt sCnt    = 0;
    smSN sMinSN  = SM_SN_NULL;
    UInt sMinReadPtr  = mBufSize;
    UInt sReadPtr = RP_BUF_NULL_PTR;
    UInt sDistance    = ID_UINT_MAX;
    UInt sMinDistance = ID_UINT_MAX;
    void* sLogPtr = NULL;
    smiLogHdr sLogHead;

    IDE_DASSERT(mAccessInfoCnt <= mMaxSenderCnt);
    /*Sender�� ���� �� ������ mutex�� ��ƾ���*/
    IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);

    if(mAccessInfoCnt == 0)
    {
        //���� ����Ʈ�� �������� Sender�� �����Ƿ� �ٷ� ������
        IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
        return;
    }
    /**************************************************************************
     *                               |-----write�ؾ��ϴ� ��-------|
     *     Sender1                                    Sender2
     * |---Readptr----------------WritePtr------------ReadPtr------------| Buf
     **************************************************************************/
    /*writePtr���� ���� �Ÿ��� ����� readptr(�ּ� read ptr) ���ϱ�*/
    for(sIdx = 0; (sCnt < mAccessInfoCnt) && (sIdx < mMaxSenderCnt); sIdx++)
    {
        /*sender�� �α׸� �д� �߿� read ptr�� RP_BUF_NULL_PTR�� ���� �� ����,
         *Buf�� ���ö� Ư�������� �����ϰ� ���� �� RP_BUF_NULL_PTR�� �ʱ�ȭ �Ѵ�.*/
        sReadPtr = mAccessInfoList[sIdx].mReadPtr;
        if( sReadPtr != RP_BUF_NULL_PTR )
        {
            sCnt++;
            if( sReadPtr > mWrotePtr )
            {
                sDistance = sReadPtr - mWrotePtr;
            }
            else
            {
                sDistance = mBufSize - (mWrotePtr - sReadPtr);
            }

            if(sDistance < sMinDistance)
            {
                sMinDistance = sDistance;
                IDU_FIT_POINT( "1.BUG-34115@rpdLogBufferMgr::updateMinReadPtr" );
                sMinReadPtr  = sReadPtr;
            }
        }
    }
    if(sMinReadPtr != mMinReadPtr) //������ �ʿ� ����
    {
        if(sMinReadPtr == mWrotePtr)//���۰� ����ִ� ������
        {
            sMinSN = SM_SN_NULL;
        }
        else //�ּ� SN�� ���ۿ��� ����
        {
            sLogPtr = mBasePtr + getReadPtr(sMinReadPtr);
            smiLogRec::getLogHdr(sLogPtr, &sLogHead);
            if(isBufferEndLog(&sLogHead) == ID_TRUE)
            {
                if(mEndPtr == mWrotePtr)
                {
                    sMinSN = SM_SN_NULL;
                }
                else /* ������ �������� ������ ���, ������ ó���� ���� �αװ� �ִ�. */
                {
                    sLogPtr = mBasePtr;
                    smiLogRec::getLogHdr(sLogPtr, &sLogHead);
                    sMinSN = smiLogRec::getSNFromLogHdr(&sLogHead);
                }
            }
            else
            {
                sMinSN = smiLogRec::getSNFromLogHdr(&sLogHead);
            }
        }

        IDE_DASSERT(sMinReadPtr != mBufSize);

        if( mMinSN != SM_SN_NULL )
        {
            if( mMinSN > sMinSN )
            {
                /*abnormal situation no update MinReadPtr*/
                ideLog::log(IDE_RP_0,
                            "Replication Log Buffer Error: sMinSN=%"ID_UINT64_FMT", "
                            "sMinReadPtr=%"ID_UINT32_FMT,
                            sMinSN, sMinReadPtr );
                printBufferInfo();
            }
            else
            {
                mMinSN      = sMinSN;
                mMinReadPtr = sMinReadPtr;
            }
        }
        else
        {
            mMinSN      = sMinSN;
            mMinReadPtr = sMinReadPtr;
        }
    }
    IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);

    return;

    IDE_EXCEPTION_END;

    return;
}

/***********************************************************************
 * ���ۿ��� �α׸� ��ȯ�Ѵ�. call by sender
 * Sender�� ���۸��� �а� �մ� ��쿡 Sender ID�� �̿��� �α׸� ��ȯ�Ѵ�.
 * [IN]  aSndrID: ȣ���ϴ� Sender�� ID(AccessInfoList�� Index)
 * [OUT] aLogHead: �˻��� �α��� align�� �α� head
 * [OUT] aSN: �˻��� �α��� SN, Ȥ�� �˻��� �αװ� ���� ��� ���� �ֱ� SN
 *            �ƹ��͵� �� ���� ��� SM_SN_NULL�� ��ȯ
 * [OUT] aLSN: ������ �α��� LSN, aIsLeave�� TRUE�� ��� ������ �α��� LSN
 *             �׷��� ���� ���  INIT LSN�� ����
 * [OUT] aIsLeave: �� �̻� ���� �αװ� ����, ��带 ��ȯ�ؾ� �ϴ� ��� ID_TRUE
 * RETURN VALUE : �˻��� �α��� ������, ���� ��� NULL
 ************************************************************************/
IDE_RC rpdLogBufferMgr::readFromRPLogBuf(UInt       aSndrID,
                                        smiLogHdr* aLogHead,
                                        void**     aLogPtr,
                                        smSN*      aSN,
                                        smLSN*     aLSN,
                                        idBool*    aIsLeave)
{
    /*wrotePtr�� ���� �ּ� ���� ������ �� �д� ����
     *���� �� �о��ٰ� Ȯ�εǰ� OFF�̸� ���� ��带 ����*/
    SChar*  sLogPtr = NULL;

    /*���ݱ��� ���� ������ ptr*/
    UInt sNextPtr = RP_BUF_NULL_PTR;
    UInt sNewReadPtr = RP_BUF_NULL_PTR;

    IDE_DASSERT(aSndrID < mMaxSenderCnt);

    *aLogPtr  = NULL;
    *aSN      = SM_SN_NULL;
    *aIsLeave = ID_FALSE;

    sNextPtr = mAccessInfoList[aSndrID].mNextPtr;

    if(mWrotePtr == sNextPtr) /*���� ������ ��� �� �о���*/
    {
        /*RP_OFF�� ��� ��������*/
        if(mWritableFlag == RP_OFF)
        {
            IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
            /* mWrotePtr == sReadPtr�� �ƴ� ��� service�� write������,
             * ó�� if(mWrotePtr == sReadPtr)���� lock�� ���� �ʰ�
             * mWrotePtr�� �о ���� ���� �о��� ������ �����
             * ���� �� �ִ�. �׷��� �ٽ� �ѹ� Ȯ���ϰ� ����
             * �ٸ��ٸ� �ٽ� �õ��� �� �ֵ��� *aSN�� SM_SN_NULL��
             * �����Ͽ� �ش�*/
            if((mWrotePtr == sNextPtr) && (mWritableFlag == RP_OFF))
            {
                *aSN = mMaxSN;
                SM_GET_LSN(*aLSN, mMaxLSN);
                *aIsLeave = ID_TRUE;
            }
            IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
        }
        else /*RP_ON�� ��� ���� ������ �αװ� ���� ��Ȳ��*/
        {
            IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);

            if((mWrotePtr == sNextPtr) && (mWritableFlag == RP_ON))
            {
                *aSN = mMaxSN;
            }
            /* *aIsLeave = ID_FALSE; */
            IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
            mAccessInfoList[aSndrID].mReadPtr = mAccessInfoList[aSndrID].mNextPtr;
        }
    }
    else /*�о�� �ϴ� ptr�� ����*/
    {
        IDL_MEM_BARRIER;
        sNewReadPtr = getReadPtr(sNextPtr);
        IDE_DASSERT(sNewReadPtr < mBufSize);
        sLogPtr = mBasePtr + sNewReadPtr;
        *aLogPtr = sLogPtr;
        smiLogRec::getLogHdr(sLogPtr, (smiLogHdr*)aLogHead);
        *aSN = smiLogRec::getSNFromLogHdr((smiLogHdr*)aLogHead);
        mAccessInfoList[aSndrID].mNextPtr = sNewReadPtr +
            smiLogRec::getLogSizeFromLogHdr(aLogHead) - 1;
        mAccessInfoList[aSndrID].mReadPtr = sNextPtr;

        /* BUG-32633 Buffer End �α׸� �ǳ� �ڴ�. (�ִ� 1ȸ ����) */
        if ( isBufferEndLog( aLogHead ) == ID_TRUE )
        {
            IDE_TEST( readFromRPLogBuf( aSndrID,
                                        aLogHead,
                                        aLogPtr,
                                        aSN,
                                        aLSN,
                                        aIsLeave )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void rpdLogBufferMgr::getSN(smSN* aMinSN, smSN* aMaxSN)
{
    IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
    *aMinSN = mMinSN;
    *aMaxSN = mMaxSN;
    IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
}

/***********************************************************************
 * buffer���� ���� �������� �׽�Ʈ �ϰ� ��带 �����Ѵ�. call by sender
 * ���� ������ ��쿡 AccessInfoList���� �� ������ ã�� �ش� ������
 * index��ȣ�� ID�� ��ȯ�� �ش�.
 * [IN]     aNeedSN: Sender�� �ʿ���ϴ� �α��� SN
 * [IN/OUT] aMinSN/aMaxSN: Sender�� �˰��ִ�(ĳ����) ������ �ּ�/�ִ� SN�� �Ѱ��ָ�
 *                  �ʿ��� ��� ������ ���۰� �����ִ� �ּ�/�ִ� SN�� �ٽ� �Ѱ��ش�.
 * [OUT] aSndrID : ������ ������ ��� Sender���� �����ִ� ���ۿ����� ID
 *                 ������ ������ ��� RP_BUF_NULL_ID�� ��ȯ�Ѵ�.
 * [OUT] aIsEnter: ������ ������ ��� ID_TRUE, �׷��� ������ ID_FALSE
 ************************************************************************/
idBool rpdLogBufferMgr::tryEnter(smSN    aNeedSN,
                                 smSN*   aMinSN,
                                 smSN*   aMaxSN,
                                 UInt*   aSndrID )
{
    UInt      sSndrID  = RP_BUF_NULL_ID;
    idBool    sIsEnter = ID_FALSE;

    IDE_DASSERT(aNeedSN != SM_SN_NULL);
    IDE_DASSERT(aMinSN != NULL);
    IDE_DASSERT(aMaxSN != NULL);
    IDE_DASSERT(aSndrID != NULL);

    if((*aMinSN <= aNeedSN) && (aNeedSN <= *aMaxSN))
    {
        if(mWritableFlag == RP_ON)
        {
            IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);

            if((mMinSN != SM_SN_NULL) &&
               (mMaxSN != SM_SN_NULL) &&
               (mWritableFlag == RP_ON))
            {
                *aMinSN = mMinSN;
                *aMaxSN = mMaxSN;
                if((mMinSN <= aNeedSN) && (aNeedSN <= mMaxSN))
                {
                    //���� ������
                    for(sSndrID = 0; sSndrID < mMaxSenderCnt; sSndrID++)
                    {
                        if(mAccessInfoList[sSndrID].mReadPtr == RP_BUF_NULL_PTR)
                        {
                            mAccessInfoList[sSndrID].mReadPtr = mMinReadPtr;
                            mAccessInfoList[sSndrID].mNextPtr = mMinReadPtr;
                            mAccessInfoCnt++;
                            sIsEnter = ID_TRUE;
                            break;
                        }
                    }
                }
            }
            IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
            if(sSndrID >= mMaxSenderCnt)
            {
                sSndrID = RP_BUF_NULL_ID;
                sIsEnter = ID_FALSE;
            }
        }
        else // RP_OFF
        {
            *aMinSN = 0;
            *aMaxSN = SM_SN_MAX;
            sSndrID = RP_BUF_NULL_ID;
            sIsEnter = ID_FALSE;
        }
    }
    *aSndrID = sSndrID;

    return sIsEnter;
}

void rpdLogBufferMgr::leave(UInt aSndrID)
{
    if(aSndrID < mMaxSenderCnt)
    {
        IDE_ASSERT(mBufInfoMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
        mAccessInfoList[aSndrID].mReadPtr = RP_BUF_NULL_PTR;
        mAccessInfoList[aSndrID].mNextPtr = RP_BUF_NULL_PTR;
        mAccessInfoCnt--;
        IDE_ASSERT(mBufInfoMutex.unlock() == IDE_SUCCESS);
    }
}
void rpdLogBufferMgr::printBufferInfo()
{
    UInt sIdx = 0;

    for( sIdx = 0; sIdx < mMaxSenderCnt; sIdx++ )
    {
        if( mAccessInfoList[sIdx].mReadPtr != RP_BUF_NULL_PTR )
        {
            ideLog::log(IDE_RP_0,"Sender ID:%"ID_UINT32_FMT", "
                        "mReadPtr:%"ID_UINT32_FMT", "
                        "mNextPtr:%"ID_UINT32_FMT,
                        sIdx,
                        mAccessInfoList[sIdx].mReadPtr,
                        mAccessInfoList[sIdx].mNextPtr);
        }
        else
        {
            /*do nothing*/
        }
    }
    ideLog::log(IDE_RP_0,
                "mMinSN=%"ID_UINT64_FMT", "
                "mMinReadPtr=%"ID_UINT32_FMT", "
                "mWrotePtr=%"ID_UINT32_FMT,
                mMinSN, mMinReadPtr, mWrotePtr);
}
