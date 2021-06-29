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

#ifndef _O_SDB_FLUSHER_H_
#define _O_SDB_FLUSHER_H_ 1

#include <sdbDef.h>
#include <idu.h>
#include <idv.h>
#include <idtBaseThread.h>
#include <sdbBCB.h>
#include <sdbLRUList.h>
#include <sdbFlushList.h>
#include <sdbFT.h>
#include <sdbCPListSet.h>
#include <sddDWFile.h>
#include <smuQueueMgr.h>


typedef enum
{
    SDB_CHECKPOINT_BY_CHKPT_THREAD = 0,
    SDB_CHECKPOINT_BY_FLUSH_THREAD
} sdbCheckpointType;

struct sdbFlushJob;

class sdbFlusher : public idtBaseThread
{
public:
    sdbFlusher();

    ~sdbFlusher();

    IDE_RC start();

    void run();

    IDE_RC initialize(UInt          aFlusherID,
                      UInt          aPageSize,
                      UInt          aPageCount,
                      sdbCPListSet *aCPListSet);

    IDE_RC destroy();

    void wait4OneJobDone();

    IDE_RC finish(idvSQL *aStatistics, idBool *aNotStarted);

    IDE_RC flushForReplacement(idvSQL         *aStatistics,
                               ULong           aReqFlushCount,
                               sdbFlushList   *aFlushList,
                               sdbLRUList     *aLRUList,
                               ULong          *aFlushedCount);

    IDE_RC flushForCheckpoint( idvSQL          * aStatistics,
                               ULong             aMinFlushCount,
                               ULong             aRedoDirtyPageCnt,
                               UInt              aRedoLogFileCount,
                               sdbCheckpointType aCheckpointType,
                               ULong           * aFlushedCount );

    IDE_RC flushDBObjectBCB(idvSQL       *aStatistics,
                            sdbFiltFunc   aFiltFunc,
                            void         *aFiltAgr,
                            smuQueueMgr  *aBCBQueue,
                            ULong        *aFlushedCount);

    void applyStatisticsToSystem();

    IDE_RC wakeUpOnlyIfSleeping(idBool *aWaken);

    void getMinRecoveryLSN(idvSQL *aStatistics,
                           smLSN  *aRet);

    UInt getWaitInterval(ULong aFlushedCount);

    inline sdbFlusherStat* getStat();

    inline idBool isRunning();

    void setDelayedFlushProperty( UInt aDelayedFlushListPct,
                                  UInt aDelayedFlushProtectionTimeMsec );

private:

    void addToLocalList( sdbBCB **aHead,
                         sdbBCB **aTail,
                         UInt    *aCount,
                         sdbBCB  *aBCB );

    IDE_RC writeIOB( idvSQL *aStatistics, 
                     idBool  aMoveToPrepare,
                     idBool  aMoveToSBuffer );

    IDE_RC copyToIOB( idvSQL *aStatistics,
                      sdbBCB *aBCB,
                      idBool  aMoveToPrepare,
                      idBool  aMoveToSBuffer );

    IDE_RC flushTempPage(idvSQL *aStatistics,
                         sdbBCB *aBCB,
                         idBool  aMoveToPrepare);

    IDE_RC copyTempPage(idvSQL *aStatistics,
                        sdbBCB *aBCB,
                        idBool  aMoveToPrepare,
                        idBool  aMoveToSBuffer );

    IDE_RC syncAllFile4Flush( idvSQL   * aStatistics );

     /* PROJ-2102 Secondary Buffer */
    inline idBool isSBufferServiceable( void );

    inline idBool needToUseDWBuffer( idBool aMoveToSBuffer );

    inline idBool needToMovedownSBuffer( idBool    aMoveToSBuffer,  
                                         sdbBCB  * aBCB );

    inline idBool needToSkipBCB( sdbBCB  * aBCB );

    inline idBool needToSyncTBS( idBool aMoveToSBuffer );

    inline idBool isHotBCBByFlusher( sdbBCB *aBCB );

    IDE_RC delayedFlushForReplacement( idvSQL      * aStatistics,
                                       sdbFlushJob * aNormalFlushJob,
                                       ULong       * aRetFlushedCount );
private:
    /* Data page ũ��. ����� 8K */
    UInt           mPageSize;

    /* flusher ������ ���� ����. run�ÿ� ID_TRUE�� ����  */
    idBool         mStarted;

    /* flusher �����带 ���� ��Ű�� ���ؼ� �ܺο��� ID_TRUE�� �����Ѵ�. */
    idBool         mFinish;

    /* �÷��� �ĺ��� */
    UInt           mFlusherID;

    /* ���� IOB�� ����� page���� */
    UInt           mIOBPos;

    /* IOB�� �ִ�� ���� �� �ִ� ������ ���� */
    UInt           mIOBPageCount;

    /* �� mIOB�� �޸� ���� �ּҰ��� ������ �ִ� �迭.
     * ���� 3��° mIOB�� �ּҰ��� �����Ѵٸ�, mIOBPtr[3]�̷� ������ ���� */
    UChar        **mIOBPtr;

    /* mIOB�� ����� �� frame�� BCB�� �������ϰ� �ִ� array */
    sdbBCB       **mIOBBCBArray;

    /* mIOBSpace�� 8K align�� �޸� ����.
     * ���� mIOB�� �����Ҷ��� �̰����� �����Ѵ�. */
    UChar         *mIOB;

    /* ���� IOB �޸� ����. 8K align�Ǿ� ���� �ʰ�
     * ���� �ʿ��� �纸�� 8K�� ũ��. ���� �޸� �Ҵ�� �������� ���δ�. */
    UChar         *mIOBSpace;

    /* mMinRecoveryLSN ������ ���� ���ü��� �����ϱ� ���� ���ؽ� */
    iduMutex       mMinRecoveryLSNMutex;

    /* mIOB�� ����� page�� recoveryLSN�� ���� ���� ��  */
    smLSN          mMinRecoveryLSN;

    /* mIOB�� ����� page�� pageLSN���� ���� ū��. WAL�� ��Ű�� ���� page flush����
     * log flush�� ���� ���� */
    smLSN          mMaxPageLSN;

    /* �ϳ��� �۾��� ����ģ flusher�� ����� �� �ð��� ������ �ش�. */
    UInt           mWaitTime;

    /* �÷��� �ϴ�  buffer pool�� ���� checkpoint List set*/
    sdbCPListSet  *mCPListSet;

    /* �÷��� �ϴ�  buffer pool */
    sdbBufferPool *mPool;

    // ��� ������ ���� private session �� statistics ����
    idvSQL         mStatistics;
    idvSession     mCurrSess;
    idvSession     mOldSess;

    // ������ ��� ���� �ɹ�
    iduCond        mCondVar;
    iduMutex       mRunningMutex;

    // DWFile ��ü�� flusher���� ������ �ִ´�.
    sddDWFile      mDWFile;

    // ��������� �����ϴ� ��ü
    sdbFlusherStat mStat;

    // WritePage�� Sync�� TBSID��
    sdbSyncFileInfo * mArrSyncFileInfo;

    // Secondary Buffer �� ��� �����Ѱ�.
    idBool         mServiceable;

    /* PROJ-2669
     * Delayed Flush ����� on/off �ϸ�
     * 0�� �ƴѰ�� Delayed Flush List �� �ִ� ����(percent)�� �ǹ��Ѵ� */
    UInt           mDelayedFlushListPct;

    /* PROJ-2669
     * Flush ��� BCB�� 
     * Touch �� �� �ð��� ���� �ð�(DELAYED_FLUSH_PROTECTION_TIME_MSEC) ������ ���
     * Delyaed Flush List �� �Ű����� Flush ������ �������� �̷��.  */
    ULong          mDelayedFlushProtectionTimeUsec;
};

sdbFlusherStat* sdbFlusher::getStat()
{
    return &mStat;
}

idBool sdbFlusher::isRunning()
{
    if ((mStarted == ID_TRUE) && (mFinish == ID_FALSE))
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

/******************************************************************************
 * Description :
 *   Flush list �� ���Ե� BCB�� ������� HOT BCB ���θ� �Ǵ��Ѵ�.
 *   ����: ���� �ð� - ������ BCB Touch �ð� <= HOT_FLUSH_PROTECTION_TIME_MSEC
 *
 *  aBCB         - [IN]  BCB Pointer
 *  aCurrentTime - [IN]  Current Time
 ******************************************************************************/
inline idBool sdbFlusher::isHotBCBByFlusher( sdbBCB *aBCB )
{
    idvTime sCurrentTime;
    ULong   sTime;

    IDV_TIME_GET( &sCurrentTime );

    sTime = IDV_TIME_DIFF_MICRO( &aBCB->mLastTouchCheckTime, &sCurrentTime );

    return  (idBool)( sTime < mDelayedFlushProtectionTimeUsec );
}
#endif // _O_SDB_FLUSHER_H_

