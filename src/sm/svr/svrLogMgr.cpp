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
 
/**********************************************************************
 * Volatile Tablespace ������ ���� �α� �Ŵ���
 *
 *    �޸𸮿� �α׸� ����ϴ� �Լ��� �α׸� �д� �Լ��� �����Ѵ�.
 *    �αװ� ��ϵǴ� �޸𸮴� ��ü������ ������ �Ѵ�.
 *
 * Abstraction
 *    svrLogMgr�� log record�� ���� �д� �������̽��� �����Ѵ�.
 *    ����ڴ� ����� log�� body�� ������ writeLog�Լ��� ����
 *    �α� ���ڵ带 ����� �� �ִ�. �α� ���۴� �������� ���ӵ�
 *    �޸� �������� ������ �� �ִ�.
 *    ������ �������̽��� �����Ѵ�.
 *      - initializeStatic
 *      - destroyStatic
 *      - initEnv
 *      - destroyEnv
 *      - writeLog
 *      - readLog
 *      - getLastLSN
 *
 * Implementation
 *    �α״� ���� ũ�� �޸��� ������ ������ �ɰ��� ������ ����ȴ�.
 *    �α׸� ���� �� ���� ����� ���̱� ���� �ϳ��� �αװ� �� ������ ����
 *    �־�� �Ѵٴ� ������ �����Ѵ�.
 *    in-place update �α״� �ִ� 32K���� �� ũ�⸦ ������ ������
 *    ������ �ϳ��� 32K�� �Ǿ� �Ѵ�.
 *    
 **********************************************************************/

#include <ide.h>
#include <iduMemPool.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <svrLogMgr.h>

/* �Ʒ� ���ǵ� �ڷ� �������� svrLogMgr.cpp �ȿ����� �� �� �ְ� �ϱ� ����
 * cpp ���Ͽ� ���Ǹ� �Ѵ�. */

/*****************************************************************
 * typedef�� define ��
 *****************************************************************/
typedef struct svrLogPage
{
    svrLogPage     *mNext;
    SChar           mLogBuf[1];
} svrLogPage;

/* log�� ����
   - sub log�� ������ �ʴ� �Ϲ� �α� -> mPrev ����
   - sub log�� ������ �Ϲ� �α�      -> mPrev, mNext ����
   - sub log                         -> mNext ����  */
typedef struct svrLogRec
{
    svrLogPage     *mPageBelongTo;
    svrLogRec      *mPrevLogRec;
    svrLogRec      *mNextSubLogRec;
    UInt            mBodySize;
    SChar           mLogBody[1];
} svrLogRec;

#define SVR_LOG_PAGE_SIZE       (32768)
#define SVR_LOG_PAGE_HEAD_SIZE  (offsetof(svrLogPage, mLogBuf))
#define SVR_LOG_PAGE_BODY_SIZE  (SVR_LOG_PAGE_SIZE - SVR_LOG_PAGE_HEAD_SIZE)
#define SVR_LOG_HEAD_SIZE       (offsetof(svrLogRec, mLogBody))

/* mempool���� �޸� �Ҵ� ������ chunk �����̴�.
   �� 32�� ������ �� �Ҵ��Ѵ�.
   �ʱ⿣ 32���� �������� �Ҵ�ȴ�. �� 1M�� ����ϰ� �ȴ�. */
#define SVR_LOG_PAGE_COUNT_PER_CHUNK      (32)

/* mempool�� �ѹ� �þ��ٰ� �پ���� �ϴ� �ּ� ũ�⸦ ���Ѵ�.
   10���� chunk�� 10M�̴�.  */
#define SVR_LOG_PAGE_CHUNK_SHRINK_LIMIT   (10)
 
/*****************************************************************
 * �� ���Ͽ����� �����ϴ� private �� static instance ����
 *****************************************************************/
static iduMemPool   mLogMemPool;

/*****************************************************************
 * �� ���Ͽ����� �����ϴ� private �� static �Լ� ����
 *****************************************************************/
static IDE_RC allocNewLogPage(svrLogEnv *aEnv);

static UInt logRecSize(svrLogEnv *aEnv, UInt aLogDataSize);

static SChar* curPos(svrLogEnv *aEnv);

/* svrLogEnv.mPageOffset�� align�� ����Ǿ�� �Ѵ�.
   ���� �� �Լ��� ���� mPageOffset�� ���ŵǾ�� �Ѵ�. */
static void initOffset(svrLogEnv *aEnv);

static void updateOffset(svrLogEnv *aEnv, UInt aIncOffset);

static svrLogRec* getLastSubLogRec(svrLogRec *aLogRec);

/*****************************************************************
 * svrLogMgr �������̽� �Լ��� ����
 *****************************************************************/

/*****************************************************************
 * Description:
 *    volatile logging�� �ϱ� ���ؼ� �� �Լ��� ȣ���ؾ� �Ѵ�.
 *    volatile tablespace�� ���ٸ� �� �Լ��� �θ��� ���� �޸�
 *    ������ ���̴�.
 *    �� �Լ��� ȣ���ϸ� ����� destory()�� ȣ���ؼ� �޸𸮸�
 *    �����ؾ� �Ѵ�.
 *****************************************************************/
IDE_RC svrLogMgr::initializeStatic()
{
    /* mLogMemPool�� �ʱ�ȭ�Ѵ�. */
    IDE_TEST(mLogMemPool.initialize(IDU_MEM_SM_SVR,
                                    (SChar*)"Volatile log memory pool",
                                    ID_SCALABILITY_SYS,
                                    SVR_LOG_PAGE_SIZE,
                                    SVR_LOG_PAGE_COUNT_PER_CHUNK,
                                    SVR_LOG_PAGE_CHUNK_SHRINK_LIMIT,	/* ChunkLimit */
                                    ID_TRUE,							/* UseMutex */
                                    IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                    ID_FALSE,							/* ForcePooling */
                                    ID_TRUE,							/* GarbageCollection */
                                    ID_TRUE,                            /* HWCacheLine */
                                    IDU_MEMPOOL_TYPE_LEGACY             /* mempool type*/) 
             != IDE_SUCCESS);			

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svrLogMgr::destroyStatic()
{
    IDE_TEST(mLogMemPool.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Desciption:
 *   volatile log�� ����ϱ� ���ؼ��� svrLogEnv �ڷᱸ���� �ʿ��ϴ�.
 *   initEnv()�� ���ؼ� svrLogEnv�� �ʱ�ȭ�Ѵ�.
 *   initEnv������ svrLogEnv�� memPool�� �ʱ�ȭ�ϰ� �� �������� �ϳ�
 *   �Ҵ��ϸ� offset�� �ʱ�ȭ�Ѵ�.
 *****************************************************************/
IDE_RC svrLogMgr::initEnv(svrLogEnv *aEnv, idBool aAlignForce)
{
    /* svrLogEnv�� �� �ɹ��� �ʱ�ȭ�Ѵ�. */
    aEnv->mHeadPage = NULL;
    aEnv->mCurrentPage = NULL;
    aEnv->mPageOffset = SVR_LOG_PAGE_SIZE;
    aEnv->mLastLogRec = SVR_LSN_BEFORE_FIRST;
    aEnv->mLastSubLogRec = SVR_LSN_BEFORE_FIRST;
    aEnv->mAlignForce = aAlignForce;
    aEnv->mAllocPageCount = 0;
    aEnv->mFirstLSN = SVR_LSN_BEFORE_FIRST;

    return IDE_SUCCESS;
}

/*****************************************************************
 * Desciption:
 *   initEnv���� �ʱ�ȭ�ߴ� �ڿ����� destroyEnv()�� ���ؼ� �����Ѵ�.
 *
 * Imeplementation:
 *    �̶����� alloc�� log page���� ��ȸ�ϸ鼭 ��� free���ش�.
 *****************************************************************/
IDE_RC svrLogMgr::destroyEnv(svrLogEnv *aEnv)
{
    svrLogPage  *sPrevPage;
    svrLogPage  *sCurPage;
    UInt         sPageCount = 0;

    sPrevPage = aEnv->mHeadPage;

    while (sPrevPage != NULL)
    {
        sCurPage = sPrevPage->mNext;

        IDE_TEST(mLogMemPool.memfree(sPrevPage) != IDE_SUCCESS);

        sPrevPage = sCurPage;

        sPageCount++;
    }

    IDE_ASSERT(aEnv->mAllocPageCount == sPageCount);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Description:
 *   �α׸� ����ϱ� ���� �Ҵ��� �޸��� �� ũ�⸦ ���Ѵ�.
 *****************************************************************/
UInt svrLogMgr::getAllocMemSize(svrLogEnv *aEnv)
{
    return aEnv->mAllocPageCount * SVR_LOG_PAGE_SIZE;
}

/*****************************************************************
 * Description:
 *   log record�� �α� ���ۿ� ����Ѵ�.
 *   log record�� sub log�� ���� �� �ִ�.
 *   - aEnv�� initEnv()�� ���ؼ� �ʱ�ȭ�� �����̾�� �ϸ�,
 *   - aLogData�� �α� ������ ����Ű�� �ִ� �������̸�,
 *   - aLogSize�� �α� ������ �����̴�.
 *
 * Implementation:
 *   ���� �α� ���� �������� üũ�ؼ� �α׸� ����� ������ �ִ���
 *   ���� �˻��Ѵ�. ���� ���� �������� ���� �� ������
 *   ���ο� �������� �Ҵ��Ѵ�.
 *   mLastLogRec�� �����ϰ� mLastSubLogRec�� NULL�� �����Ѵ�.
 *****************************************************************/
IDE_RC svrLogMgr::writeLog(svrLogEnv *aEnv,
                           svrLog    *aLogData,
                           UInt       aLogSize)
{
    svrLogRec sLogRec;

    IDE_ASSERT(logRecSize(aEnv, aLogSize) <= SVR_LOG_PAGE_BODY_SIZE);

    /* ���� �������� �α׸� ����� �� �ִ��� �˻��Ѵ�.
       ����� �� ������ ���ο� page�� �Ҵ�޴´�. */
    if (aEnv->mPageOffset + logRecSize(aEnv, aLogSize) >
        SVR_LOG_PAGE_SIZE)
    {
        IDE_TEST_RAISE(allocNewLogPage(aEnv) != IDE_SUCCESS,
                       cannot_alloc_logbuffer);
    }

    /* �α� ��� ���� */
    sLogRec.mPageBelongTo = aEnv->mCurrentPage;
    sLogRec.mPrevLogRec = aEnv->mLastLogRec;
    sLogRec.mNextSubLogRec = SVR_LSN_BEFORE_FIRST;
    sLogRec.mBodySize = aLogSize;

    /* ������ log record ������ ���� */
    aEnv->mLastLogRec = (svrLogRec*)(curPos(aEnv));
    aEnv->mLastSubLogRec = SVR_LSN_BEFORE_FIRST;

    /* log head ��� */
    idlOS::memcpy(curPos(aEnv),
                  &sLogRec,
                  SVR_LOG_HEAD_SIZE);
    updateOffset(aEnv, SVR_LOG_HEAD_SIZE);

    /* log body ��� */
    idlOS::memcpy(curPos(aEnv),
                  aLogData,
                  aLogSize);
    updateOffset(aEnv, aLogSize);

    /* mFirstLSN ���� */
    if (aEnv->mFirstLSN == SVR_LSN_BEFORE_FIRST)
    {
        aEnv->mFirstLSN = aEnv->mLastLogRec;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(cannot_alloc_logbuffer)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CannotAllocLogBufferMemory));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Description:
 *   sub log record�� �α� ���ۿ� ����Ѵ�.
 *   - aEnv�� initEnv()�� ���ؼ� �ʱ�ȭ�� �����̾�� �ϸ�,
 *   - aLogData�� �α� ������ ����Ű�� �ִ� �������̸�,
 *   - aLogSize�� �α� ������ �����̴�.
 *
 * Implementation:
 *   ���� �α� ���� �������� üũ�ؼ� �α׸� ����� ������ �ִ���
 *   ���� �˻��Ѵ�. ���� ���� �������� ���� �� ������
 *   ���ο� �������� �Ҵ��Ѵ�.
 *   mLastLogRec�� �������� �ʰ� mLastSubLogRec�� �����Ѵ�.
 *****************************************************************/
IDE_RC svrLogMgr::writeSubLog(svrLogEnv *aEnv,
                              svrLog    *aLogData, 
                              UInt       aLogSize)
{
    svrLogRec sSubLogRec;

    IDE_ASSERT(logRecSize(aEnv, aLogSize) <= SVR_LOG_PAGE_BODY_SIZE);

    /* ���� �������� �α׸� ����� �� �ִ��� �˻��Ѵ�.
       ����� �� ������ ���ο� page�� �Ҵ�޴´�. */
    if (aEnv->mPageOffset + logRecSize(aEnv, aLogSize) >
        SVR_LOG_PAGE_SIZE)
    {
        IDE_TEST_RAISE(allocNewLogPage(aEnv) != IDE_SUCCESS,
                       cannot_alloc_logbuffer);
    }

    /* �α� ��� ���� */
    sSubLogRec.mPageBelongTo = aEnv->mCurrentPage;
    sSubLogRec.mPrevLogRec = SVR_LSN_BEFORE_FIRST;
    sSubLogRec.mNextSubLogRec = SVR_LSN_BEFORE_FIRST;
    sSubLogRec.mBodySize = aLogSize;

    /* ���� �α�(mLastLogRec�̰ų� mLastSubLogRec)�� mNextSubLogRec��
       �����Ѵ�. */
    if (aEnv->mLastSubLogRec == SVR_LSN_BEFORE_FIRST)
    {
        /* mLastSubLogRec�� SVR_LSN_BEFORE_FIRST���
           ���� sub log�� ��ϵ��� �ʾҴ�.
           �� ��� last log�� mNextSubLogRec�� �����ؾ� �Ѵ�. */
        IDE_ASSERT(aEnv->mLastLogRec != SVR_LSN_BEFORE_FIRST);

        aEnv->mLastLogRec->mNextSubLogRec = (svrLogRec*)curPos(aEnv);
    }
    else
    {
        aEnv->mLastSubLogRec->mNextSubLogRec = (svrLogRec*)curPos(aEnv);
    }

    /* aEnv�� mLastSubLogRec�� �����Ѵ�.
       mLastLogRec�� �������� �ʴ´�. */
    aEnv->mLastSubLogRec = (svrLogRec*)curPos(aEnv);

    /* log head ��� */
    idlOS::memcpy(curPos(aEnv),
                  &sSubLogRec,
                  SVR_LOG_HEAD_SIZE);
    updateOffset(aEnv, SVR_LOG_HEAD_SIZE);

    /* log body ��� */
    idlOS::memcpy(curPos(aEnv),
                  aLogData,
                  aLogSize);
    updateOffset(aEnv, aLogSize);

    return IDE_SUCCESS;

    IDE_EXCEPTION(cannot_alloc_logbuffer)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CannotAllocLogBufferMemory));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Description:
 *    �̶����� ����� �α� ���ڵ��� ������ �α� ���ڵ��� LSN�� ��´�.
 *****************************************************************/
svrLSN svrLogMgr::getLastLSN(svrLogEnv *aEnv)
{
    return aEnv->mLastLogRec;
}

idBool svrLogMgr::isOnceUpdated(svrLogEnv *aEnv)
{
    return ((aEnv->mLastLogRec != SVR_LSN_BEFORE_FIRST) ? ID_TRUE : ID_FALSE);
}

/*****************************************************************
 * Description:
 *    aLSNToRead�� ����Ű�� �α� ���ڵ带 �о� aBufToLoadAt��
 *    ����Ѵ�.
 *    undo �� ������ ���� �α� ���ڵ��� LSN�� aUndoNextLSN��
 *    ��ȯ�Ѵ�.
 *****************************************************************/
IDE_RC svrLogMgr::readLogCopy(svrLogEnv *aEnv,
                              svrLSN     aLSNToRead,
                              svrLog    *aBufToLoadAt,
                              svrLSN    *aUndoNextLSN,
                              svrLSN    *aNextSubLSN)
{
    svrLogRec *sLogRec = aLSNToRead;
    UInt       sBodySize;

    IDE_ASSERT(sLogRec != SVR_LSN_BEFORE_FIRST);

    /* ���� undo�� log record�� lsn�� �����Ѵ�. */
    idlOS::memcpy(aUndoNextLSN, &sLogRec->mPrevLogRec, ID_SIZEOF(svrLSN*));

    /* ������ ���� sub log record�� ������ �����Ѵ�. */
    idlOS::memcpy(aNextSubLSN, &sLogRec->mNextSubLogRec, ID_SIZEOF(svrLSN*));

    /* �α� �����͸� ���ۿ� �����Ѵ�. */
    if (aEnv->mAlignForce == ID_TRUE)
    {
        idlOS::memcpy(aBufToLoadAt, 
                      (SChar*)(vULong)idlOS::align8((ULong)(vULong)sLogRec->mLogBody),
                      sLogRec->mBodySize);
    }
    else
    {
        idlOS::memcpy(&sBodySize, &sLogRec->mBodySize, ID_SIZEOF(UInt));
        idlOS::memcpy(aBufToLoadAt, sLogRec->mLogBody, sBodySize);
    }

    return IDE_SUCCESS;
}

/*****************************************************************
 * Description:
 *    �αװ� ��ϵ� ��ġ�� �����Ѵ�. align�� �ȵǾ� �ֱ� ������
 *    primitive type �ɽ����ϸ� �ȵȴ�.
 *****************************************************************/
IDE_RC svrLogMgr::readLog(svrLogEnv *aEnv,
                          svrLSN     aLSNToRead,
                          svrLog   **aLogData,
                          svrLSN    *aUndoNextLSN,
                          svrLSN    *aNextSubLSN)
{
    svrLogRec *sLogRec = aLSNToRead;

    IDE_ASSERT(sLogRec != SVR_LSN_BEFORE_FIRST);

    /* ���� �α� ��ġ�� �����Ѵ�. */
    if (aEnv->mAlignForce == ID_TRUE)
    {
        *aLogData = (svrLog *)(vULong)
            idlOS::align8((ULong)(vULong)sLogRec->mLogBody);
    }
    else
    {
        /* mAlignForce�� flase�� ��쿣 �� �Լ��� ȣ���� ������
           �α׸� memcpy�� �� �о�� �Ѵ�. */
        *aLogData = (svrLog *)sLogRec->mLogBody;
    }

    /* ���� undo�� log record�� lsn�� �����Ѵ�. */
    idlOS::memcpy(aUndoNextLSN, &sLogRec->mPrevLogRec, ID_SIZEOF(svrLSN*));

    /* ������ ���� sub log record�� ������ �����Ѵ�. */
    idlOS::memcpy(aNextSubLSN, &sLogRec->mNextSubLogRec, ID_SIZEOF(svrLSN*));

    return IDE_SUCCESS;
}

IDE_RC svrLogMgr::removeLogHereafter(svrLogEnv *aEnv,
                                     svrLSN     aThisLSN)
{
    svrLogPage *sPrevPage;
    svrLogPage *sCurPage;
    idBool      sAlignForce;
    svrLogRec  *sLastPosLogRec;

    if (aThisLSN == SVR_LSN_BEFORE_FIRST)
    {
        /* total rollback�� aThisLSN�� SVR_LSN_BEFORE_FIRST��
           �Ѿ�´�. �� ��� aEnv�� ���� �ʱ�ȭ�ؾ� �Ѵ�. */
        sAlignForce = aEnv->mAlignForce;
        destroyEnv(aEnv);
        initEnv(aEnv, sAlignForce);
    }
    else
    {
        /* aEnv�� �� ������� �����Ѵ�. */
        aEnv->mLastLogRec = aThisLSN;
        aEnv->mLastSubLogRec = getLastSubLogRec(aEnv->mLastLogRec);

        if (aEnv->mLastSubLogRec == SVR_LSN_BEFORE_FIRST)
        {
            sLastPosLogRec = aEnv->mLastLogRec;
        }
        else
        {
            sLastPosLogRec = aEnv->mLastSubLogRec;
        }

        aEnv->mCurrentPage = sLastPosLogRec->mPageBelongTo;
        aEnv->mPageOffset = (SChar*)sLastPosLogRec - (SChar*)aEnv->mCurrentPage;

        updateOffset(aEnv, SVR_LOG_HEAD_SIZE);
        updateOffset(aEnv, sLastPosLogRec->mBodySize);

        /* mCurrentPage ������ ���������� ��� �����Ѵ�. */
        sPrevPage = aEnv->mCurrentPage->mNext;
        while (sPrevPage != NULL)
        {
            sCurPage = sPrevPage->mNext;

            IDE_TEST(mLogMemPool.memfree(sPrevPage) != IDE_SUCCESS);

            sPrevPage = sCurPage;

            aEnv->mAllocPageCount--;
        }

        /* BUG-18018 : svrLogMgrTest�� �����մϴ�.
         *
         * aEnv->mCurrentPage���� �������� Free���Ŀ�
         * aEnv->mCurrentPage�� Tail�� �Ǵµ� Tail��
         * mNext�� Null���� �ʾƼ� ���Ŀ� �� link�� ����
         * Free�ϴ� Logic���� �׽��ϴ�. */
        aEnv->mCurrentPage->mNext = NULL;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Description:
 *   ���ο� �α� ���� �������� �Ҵ��Ѵ�.
 *****************************************************************/
IDE_RC allocNewLogPage(svrLogEnv *aEnv)
{
    svrLogPage *sNewPage;

    /* svrLogMgr_allocNewLogPage_alloc_NewPage.tc */
    IDU_FIT_POINT("svrLogMgr::allocNewLogPage::alloc::NewPage");
    IDE_TEST(mLogMemPool.alloc((void**)&sNewPage)
             != IDE_SUCCESS);

    sNewPage->mNext = NULL;

    if (aEnv->mCurrentPage != NULL)
    {
        aEnv->mCurrentPage->mNext = sNewPage;
    }
    else
    {
        /* mCurrentPage�� NULL�̸� ó�� alloc�ϴ� ���̱� ������
           next�� ������ �ʿ䰡 ����. */
        aEnv->mHeadPage = sNewPage;
    }

    aEnv->mCurrentPage = sNewPage;

    initOffset(aEnv);

    aEnv->mAllocPageCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*****************************************************************
 * Description:
 *    �α� ������ ũ�⸦ �Է¹޾� ���� �α� ���ڵ尡 
 *    �α� ���ۿ� ����� �� �����ϴ� ũ�⸦ ���Ѵ�.
 *    �� �Լ��� align�� ����Ѵ�.
 *****************************************************************/
UInt logRecSize(svrLogEnv *aEnv, UInt aLogDataSize)
{
    if (aEnv->mAlignForce == ID_TRUE)
    {
        return idlOS::align8((SInt)SVR_LOG_HEAD_SIZE) + 
               idlOS::align8(aLogDataSize);
    }

    return SVR_LOG_HEAD_SIZE + aLogDataSize;
}

/*****************************************************************
 * Description:
 *    �α׸� ����� ��ġ�� ����Ű�� mPageOffset�� �ʱ�ȭ�Ѵ�.
 *    aEnv->mAlignForce�� �����Ͽ� align ���θ� �Ǵ��Ѵ�.
 *****************************************************************/
void initOffset(svrLogEnv *aEnv)
{
    if (aEnv->mAlignForce == ID_TRUE)
    {
        aEnv->mPageOffset = idlOS::align8((SInt)SVR_LOG_PAGE_HEAD_SIZE);
    }
    else
    {
        aEnv->mPageOffset = SVR_LOG_PAGE_HEAD_SIZE;
    }
}

/*****************************************************************
 * Description:
 *    �α׸� ����� ��ġ�� ����Ű�� mPageOffset�� �����Ѵ�.
 *    �̶� align�� ����Ͽ� �����ؾ� �Ѵ�.
 *****************************************************************/
void updateOffset(svrLogEnv *aEnv, UInt aIncOffset)
{
    if (aEnv->mAlignForce == ID_TRUE)
    {
        aEnv->mPageOffset += idlOS::align8(aIncOffset);
    }
    else
    {
        aEnv->mPageOffset += aIncOffset;
    }
}

/*****************************************************************
 * Description:
 *    aEnv�� ������ �ִ� ���� ���ڵ� ��ġ�� ���´�.
 *
 * Implementation:
 *    mCurrentPage�� svrLogPage* Ÿ���̱� ������ SChar*�� ĳ���� ��
 *    offset�� ���ؾ� �Ѵ�.
 *****************************************************************/
SChar* curPos(svrLogEnv *aEnv)
{
    return ((SChar*)aEnv->mCurrentPage) + aEnv->mPageOffset;
}

/*****************************************************************
 * Description:
 *    �־��� log record�� ���� �� log record�� ������ sub log
 *    record�� ���Ѵ�.
 *****************************************************************/
svrLogRec* getLastSubLogRec(svrLogRec *aLogRec)
{
    svrLogRec *sLastSubLogRec = SVR_LSN_BEFORE_FIRST;

    if (aLogRec->mNextSubLogRec != SVR_LSN_BEFORE_FIRST)
    {
        sLastSubLogRec = aLogRec->mNextSubLogRec;
        while (sLastSubLogRec->mNextSubLogRec != SVR_LSN_BEFORE_FIRST)
        {
            sLastSubLogRec = sLastSubLogRec->mNextSubLogRec;
        }
    }

    return sLastSubLogRec;
}

