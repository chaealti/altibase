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

#ifndef _O_SDS_FLUSHER_H_
#define _O_SDS_FLUSHER_H_ 1

#include <idu.h>
#include <idv.h>
#include <idtBaseThread.h>

#include <sdbDef.h>
#include <sdbLRUList.h>
#include <sdbFlushList.h>
#include <sdbFT.h>
#include <sddDWFile.h>
#include <smuQueueMgr.h>

#include <sdsBCB.h>
#include <sdsFlusherStat.h>

class sdsFlusher : public idtBaseThread
{
public:
    sdsFlusher();

    ~sdsFlusher();

    IDE_RC start();

    void run ( void );

    IDE_RC initialize( UInt  aFlusherID,
                       UInt  aPageSize, /* SD_PAGE_SIZE*/
                       UInt  aPageCount );

    IDE_RC destroy( void );

    void wait4OneJobDone( void );

    IDE_RC finish( idvSQL *aStatistics, idBool *aNotStarted );

    IDE_RC flushForReplacement( idvSQL   * aStatistics,
                                ULong      aReqFlushCount,
                                ULong      aIndex,
                                ULong    * aFlushedCount );

    IDE_RC flushForCheckpoint( idvSQL   * aStatistics,
                               ULong      aMinFlushCount,
                               ULong      aRedoDirtyPageCnt,
                               UInt       aRedoLogFileCount,
                               ULong    * aFlushedCount);

    IDE_RC flushForObject( idvSQL       * aStatistics,
                           sdsFiltFunc    aFiltFunc,
                           void         * aFiltAgr,
                           smuQueueMgr  * aBCBQueue,
                           ULong        * aFlushedCount);

    IDE_RC wakeUpSleeping( idBool *aWaken );

    void getMinRecoveryLSN( idvSQL *aStatistics,
                            smLSN  *aRet );

    UInt getWaitInterval( ULong aFlushedCount );

    inline sdsFlusherStat *getStat( void );

    inline idBool isRunning( void );

private:

    IDE_RC writeIOB( idvSQL *aStatistics );

    IDE_RC copyToIOB( idvSQL *aStatistics,   sdsBCB *aBCB );

    IDE_RC flushTempPage( idvSQL *aStatistics,
                          sdsBCB *aBCB );

    IDE_RC syncAllFile4Flush( idvSQL * aStatistics,
                              UInt     aSyncArrCnt );

private:
    /* Data page ũ��. ����� 8K */
    UInt            mPageSize;

    /* flusher ������ ���� ����. run�ÿ� ID_TRUE�� ����  */
    idBool          mStarted;

    /* flusher �����带 ���� ��Ű�� ���ؼ� �ܺο��� ID_TRUE�� �����Ѵ�. */
    idBool          mFinish;

    /* �÷��� �ĺ��� */
    UInt            mFlusherID;

    /* ���� IOB�� ����� page���� */
    UInt            mIOBPos;

    /* IOB�� �ִ�� ���� �� �ִ� ������ ���� */
    UInt            mIOBPageCount;

    /* �� mIOB�� �޸� ���� �ּҰ��� ������ �ִ� �迭.
     * ���� 3��° mIOB�� �ּҰ��� �����Ѵٸ�, mIOBPtr[3]�̷� ������ ���� */
    UChar        ** mIOBPtr;

    /* mIOB�� ����� �� frame�� BCB�� �������ϰ� �ִ� array */
    sdsBCB       ** mIOBBCBArray;

    /* mIOBSpace�� 8K align�� �޸� ����.
     * ���� mIOB�� �����Ҷ��� �̰����� �����Ѵ�. */
    UChar         * mIOB;

    /* ���� IOB �޸� ����. 8K align�Ǿ� ���� �ʰ�
     * ���� �ʿ��� �纸�� 8K�� ũ��. ���� �޸� �Ҵ�� �������� ���δ�. */
    UChar         * mIOBSpace;

    /* mMinRecoveryLSN ������ ���� ���ü��� �����ϱ� ���� ���ؽ� */
    iduMutex        mMinRecoveryLSNMutex;

    /* mIOB�� ����� page�� recoveryLSN�� ���� ���� ��  */
    smLSN           mMinRecoveryLSN;

    /* mIOB�� ����� page�� pageLSN���� ���� ū��. WAL�� ��Ű�� ���� page flush����
     * log flush�� ���� ���� */
    smLSN           mMaxPageLSN;

    /* �ϳ��� �۾��� ����ģ flusher�� ����� �� �ð��� ������ �ش�. */
    UInt            mWaitTime;

    /* �÷��� �ϴ�  buffer pool�� ���� checkpoint List set*/
    sdbCPListSet  * mCPListSet;

    // ��� ������ ���� private session �� statistics ����
    idvSQL          mStatistics;
    idvSession      mCurrSess;

    // ������ ��� ���� �ɹ�
    iduCond         mCondVar;
    iduMutex        mRunningMutex;

    // DWFile ��ü�� flusher���� ������ �ִ´�.
    sddDWFile       mDWFile;

    sdsMeta       * mMeta;

    sdsFile       * mSBufferFile;

    sdsBufferArea * mBufferArea;
    // ��������� �����ϴ� ��ü
    sdsFlusherStat mFlusherStat;

    // WritePage�� Sync�� TBSID��
    sdbSyncFileInfo * mFileInfoArray;
};

sdsFlusherStat* sdsFlusher::getStat( void )
{
    return &mFlusherStat;
}

idBool sdsFlusher::isRunning( void )
{
    if( (mStarted == ID_TRUE) && (mFinish == ID_FALSE) )
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}
#endif // _O_SDS_FLUSHER_H_

