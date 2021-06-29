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
 * $Id:$
 **********************************************************************/

#ifndef _O_SDB_FLUSH_MGR_H_
#define _O_SDB_FLUSH_MGR_H_ 1

#include <sdbDef.h>
#include <idu.h>
#include <idv.h>
#include <sdbFlusher.h>
#include <sdbFT.h>
#include <smuQueueMgr.h>
#include <sdsBCB.h>

// flushMgr�� �Ѽ����� �ִ�� ������ �� �ִ� �۾���
#define SDB_FLUSH_JOB_MAX          (64)
#define SDB_FLUSH_COUNT_UNLIMITED  (ID_ULONG_MAX)

typedef enum
{
    SDB_FLUSH_JOB_NOTHING = 0,
    SDB_FLUSH_JOB_REPLACEMENT_FLUSH,
    SDB_FLUSH_JOB_CHECKPOINT_FLUSH,
    SDB_FLUSH_JOB_DBOBJECT_FLUSH
} sdbFlushJobType;

typedef struct sdbLRUList sdbLRUList;
typedef struct sdbFlushList sdbFlushList;
typedef struct sdbCPListSet sdbCPListSet;

// replace flush�� ���� �ʿ��� �ڷᱸ��
typedef struct sdbReplaceFlushJobParam
{
    // replace flush�� �ؾ��� flush list
    sdbFlushList    *mFlushList;
    // replace����� �ƴ� BCB���� �ű� LRU List
    sdbLRUList      *mLRUList;
} sdbReplaceFlushJobParam;

// checkpoint flush�� ���� �ʿ��� �ڷᱸ��
// BUG-22857 �� ���Ͽ� CP List�� DirtyPage�� ������ ���
// ����� ������ �ֱ� ���ؼ� �߰� �Ǿ���
typedef struct sdbChkptFlushJobParam
{
    // Restart Recovery�ÿ� Redo�� Page ��
    // �ݴ�� ���ϸ� Buffer Pool�� ���ܵ�
    // Dirty Page ��
    ULong             mRedoPageCount;
    // Restart Recovery�ÿ� Redo�� log file ��
    UInt              mRedoLogFileCount;

    sdbCheckpointType mCheckpointType;

} sdbChkptFlushJobParam;

// DB object flush�� ���� �ʿ��� �ڷᱸ��
typedef struct sdbObjectFlushJobParam
{
    // flush �ؾ� �� BCB�����͵��� ����ִ� ť
    smuQueueMgr *mBCBQueue;
    // flush �ؾ� �� BCB�� ������ ����Ǿ��� ���ɼ��� �����Ƿ�
    // �ٽ� Ȯ���ϱ� ���� �Լ�
    sdbFiltFunc  mFiltFunc;
    // mFiltFunc�� �ݵ�� ���� �־� �ִ� ����
    void        *mFiltObj;
} sdbObjectFlushJobParam;


// flush job�� ������ �� �۾��� ó���ϱ� ����
// �ʿ��� �ڷᱸ��. ���ο����� ����Ѵ�.
typedef struct sdbFlushJobDoneNotifyParam
{
    // job�� ������ ���� ��⸦ �ؾ� �ϴµ�, �̶� �ʿ��� mutex
    iduMutex    mMutex;
    // job�� ������ ���� ��⸦ �ؾ� �ϴµ�, �̶� �ʿ��� variable
    iduCond     mCondVar;
    // job�� �������� ����.
    idBool      mJobDone;
} sdbFlushJobDoneNotifyParam;

/* PROJ-2102 Fast Secondaty Buffer */
typedef struct sdbSBufferReplaceFlushJobParam
{
    ULong   mExtentIndex;
} sdbSBufferReplaceFlushJobParam;

typedef struct sdbFlushJob
{
    // flush job type
    sdbFlushJobType             mType;
    // ��û�� flush ������ ����
    ULong                       mReqFlushCount;
    // flush�۾��� �Ϸ� �� ������ ��ٷ��� �ϴ� ��쿡 �����
    sdbFlushJobDoneNotifyParam *mJobDoneParam;

    // flush �۾��� �ʿ��� �Ķ���� ����
    union
    {
        sdbReplaceFlushJobParam       mReplaceFlush;
        sdbObjectFlushJobParam        mObjectFlush;
        sdbChkptFlushJobParam         mChkptFlush;
        /* PROJ-2102 Fast Secondary Buffer */
        sdbSBufferReplaceFlushJobParam mSBufferReplaceFlush;

    } mFlushJobParam;
} sdbFlushJob;

class sdbFlushMgr
{
public:
    static IDE_RC initialize(UInt aFlusherCount);

    static void startUpFlushers();

    static IDE_RC destroy();

    static void getJob(idvSQL       *aStatistics,
                       sdbFlushJob  *aRetJob);

    static void getDelayedFlushJob( sdbFlushJob *aColdJob,
                                sdbFlushJob *aRetJob );

    static IDE_RC initJobDoneNotifyParam(sdbFlushJobDoneNotifyParam *aParam);

    static IDE_RC waitForJobDone(sdbFlushJobDoneNotifyParam *aParam);

    static void notifyJobDone(sdbFlushJobDoneNotifyParam *aParam);

    static IDE_RC wakeUpAllFlushers();
    static IDE_RC wait4AllFlusher2Do1JobDone();

    static void getMinRecoveryLSN(idvSQL *aStatistics,
                                  smLSN  *aRet);
#if 0 //not used.  
    static void addReqFlushJob(idvSQL         *aStatistics,
                               sdbFlushList   *aFlushList,
                               sdbLRUList     *aLRUList,
                               idBool         *aAdded);
#endif
    static void addReqCPFlushJob(
        idvSQL                        * aStatistics,
        ULong                           aMinFlushCount,
        ULong                           aRedoPageCount,
        UInt                            aRedoLogFileCount,
        sdbFlushJobDoneNotifyParam    * aJobDoneParam,
        idBool                        * aAdded);

    static void addReqDBObjectFlushJob(
                                    idvSQL                     * aStatistics,
                                    smuQueueMgr                * aBCBQueue,
                                    sdbFiltFunc                  aFiltFunc,
                                    void                       * aFiltObj,
                                    sdbFlushJobDoneNotifyParam * aJobDoneParam,
                                    idBool                     * aAdded);

    static IDE_RC turnOffFlusher(idvSQL *aStatistics, UInt aFlusherID);

    static IDE_RC turnOnFlusher(UInt aFlusherID);

    static IDE_RC turnOnAllflushers();

    static IDE_RC flushDirtyPagesInCPList( idvSQL * aStatistics,
                                           idBool   aFlushAll,
                                           idBool   aIsChkptThread );

    static IDE_RC flushObjectDirtyPages(idvSQL       *aStatistics,
                                        smuQueueMgr  *aBCBQueue,
                                        sdbFiltFunc   aFiltFunc,
                                        void         *aFiltObj);
    static idBool isBusyCondition();

    static idBool isCond4ReplaceFlush();

    static ULong getLowFlushLstLen4FF();

    static ULong getLowPrepareLstLen4FF();

    static ULong getHighFlushLstLen4FF();

    static inline UInt getReqJobCount();

    static inline void updateLastFlushedTime();

    static inline sdbFlusherStat* getFlusherStat(UInt aFlusherID);

    static inline UInt getFlusherCount();

    static UInt getActiveFlusherCount();

    static void setDelayedFlushProperty( UInt aDelayedFlushListPct,
                                         UInt aDelayedFlushProtectionTimeMsec );

private:
    static inline void initJob(sdbFlushJob *aJob);

    static inline void incPos(UInt *aPos);

    static void getReplaceFlushJob(sdbFlushJob *aRetJob);

    static void getCPFlushJob(idvSQL *aStatistics, sdbFlushJob *aRetJob);

    static void getReqJob(idvSQL       *aStatistics,
                          sdbFlushJob  *aRetJob);

private:
    // Job�� ����ϰ� �����ö� ����ϴ� mutex
    // mReqJobMutex�� Ʈ����� �����尡 req job�� ����Ҷ�,
    // �׸��� flusher�� getJob�� �� ���ȴ�. ���� Ʈ����� �������
    // flusher�� ���� ������ �߻��� �� �ִ�.
    // ���� �ּ����� mutex ������ �����ؾ� �Ѵ�.
    static iduMutex      mReqJobMutex;

    // Job�� ����Ҷ� ����ϴ� �ڷᱸ��
    static sdbFlushJob   mReqJobQueue[SDB_FLUSH_JOB_MAX];

    // Job�� ����Ҷ� ����ϴ� ����,
    // mReqJobQueue[mReqJobAddPos++] = job
    static UInt          mReqJobAddPos;

    // Job�� ���� �ö� ����ϴ� ����,
    // job = mReqJobQueue[mReqJobGetPos++]
    static UInt          mReqJobGetPos;

    // BUG-26476
    // checkpoint ����� flusher control�� ���� mutex
    static iduLatch   mFCLatch; // flusher control latch

    // flusher�� �迭���·� ������ �ִ�.
    static sdbFlusher   *mFlushers;

    // sdbFlushMgr�� �ִ�� ���� �� �ִ� flusher����
    static UInt          mFlusherCount;

    // �������� flush�� �ð�
    static idvTime       mLastFlushedTime;
    
    // flush Mgr�� �۾��ؾ��� buffer pool�� ���� �ִ� checkpoint list
    static sdbCPListSet *mCPListSet;
};

void sdbFlushMgr::initJob(sdbFlushJob *aJob)
{
    aJob->mType          = SDB_FLUSH_JOB_NOTHING;
    aJob->mReqFlushCount = 0;
    aJob->mJobDoneParam  = NULL;
}

/***************************************************************************
 *  description:
 *      ���� jobQueue������ position�� ������Ų��.
 *      mReqJobQueue�� ũ�Ⱑ �����Ǿ� �ֱ� ������, SDB_FLUSH_JOB_MAX��
 *      �ʰ��ϴ� ��쿣 0���� �ȴ�.
 ***************************************************************************/
void sdbFlushMgr::incPos(UInt *aPos)
{
    UInt sPos = *aPos;

    IDE_ASSERT(sPos < SDB_FLUSH_JOB_MAX);

    sPos += 1;
    if (sPos == SDB_FLUSH_JOB_MAX)
    {
        sPos = 0;
    }
    *aPos = sPos;
}

UInt sdbFlushMgr::getFlusherCount()
{
    return mFlusherCount;
}

sdbFlusherStat* sdbFlushMgr::getFlusherStat(UInt aFlusherID)
{
    return mFlushers[aFlusherID].getStat();
}

UInt sdbFlushMgr::getReqJobCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < SDB_FLUSH_JOB_MAX; i++)
    {
        if ( mReqJobQueue[mReqJobAddPos].mType != SDB_FLUSH_JOB_NOTHING )
        {
            sRet++;
        }
    }
    return sRet;
}

void sdbFlushMgr::updateLastFlushedTime()
{
    IDV_TIME_GET(&mLastFlushedTime);
}

#endif // _O_SDB_FLUSH_MGR_H_

