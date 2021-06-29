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
 * $Id: sdsDef.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer
 **********************************************************************/

#ifndef  _O_SDS_DEF_H_
#define  _O_SDS_DEF_H_  1



#define SDS_META_MAX_CHUNK_PAGE_COUNT           (2048)

#define SDS_SECONDARY_BUFFER_FILE_NAME_PREFIX   "sbuffer"

/***********************************************************************
 * Property State
 **********************************************************************/
typedef enum
{
    SDS_PROPERTY_OFF,
    SDS_PROPERTY_ON 
}sdsSBufferPropertyState;

/***********************************************************************
 * Servie Stage
 **********************************************************************/
typedef enum
{
    SDS_UNSERVICEABLE,
    SDS_IDENTIFIABLE,
    SDS_SERICEABLE 
}sdsSBufferServieStage;

/***********************************************************************
 * Cache Type
 **********************************************************************/
typedef enum
{
    SDS_FLUSH_PAGE_ALL,
    SDS_FLUSH_PAGE_DIRTY,
    SDS_FLUSH_PAGE_CLEAN
} sdsSBufferType;

/***********************************************************************
 * sdsFlusherStat
 **********************************************************************/
typedef enum
{
    SDS_FLUSHJOB_NONE,
    SDS_FLUSHJOB_REPLACEMENT,
    SDS_FLUSHJOB_CHECKPOINT,
    SDS_FLUSHJOB_OBJECT
}sdsFlushjob;

typedef enum
{   
    SDS_IO_DONE,
    SDS_IO_IN_PROGRESS
}sdsIOState;

typedef enum
{   
    SDS_FLUSHER_FINISH,
    SDS_FLUSHER_STARTED
}sdsFlusherState;

typedef struct sdsFlusherStatData
{
    UInt  mID;
    UInt  mAlive;
    UInt  mCurrentJob; // 0: sleep, 1: replacement flush, 2: checkpoint flush
    UInt  mIOing;      // 0: memory, 1: IO
    /* ���� IOB�� ��� �ִ� �������� �� */
    UInt  mINIOBCount;
    /* replacement job�� �Ϸ��� Ƚ�� */
    ULong mReplacementFlushJobs;
    /* replacement job�� ���� ������ ���� pages */
    ULong mReplacementFlushPages;
    /* replacement job�� skip �� ���� pages  */
    ULong mReplacementSkipPages;
    ULong mCheckpointFlushJobs;
    ULong mCheckpointFlushPages;
    ULong mCheckpointSkipPages;
    ULong mObjectFlushJobs;
    ULong mObjectFlushPages;
    ULong mObjectSkipPages;
    UInt  mLastSleepSec;
    ULong mWakeUpsByTimeout;
    ULong mWakeUpsBySignal;
    ULong mTotalSleepSec;
    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance. */
    ULong mTotalFlushPages;
    /* USec*/
    ULong mTotalDWTime;
    /* USec*/
    ULong mTotalWriteTime;
    /* USec*/
    ULong mTotalSyncTime;
    
    ULong mTotalFlushTempPages;
    /* USec*/
    ULong mTotalTempWriteTime;
} sdsFlusherStatData;

/***********************************************************************
 * sdsBufferMgrStat
 **********************************************************************/
typedef struct sdsBufferMgrStatData
{
    UInt      mBufferPages;

    UInt      mHashBucketCount;
    UInt      mHashChainLatchCount;
    UInt      mCheckpointListCount;
    /* Hash Table�� �ɸ� �������� �� */
    UInt      mHashPages;
    /* �������� �ִ� ������ �� */
    UInt      mMovedoweIngPages;
    /* Flush ��� �������� �� */
    UInt      mMovedownDonePages;
    /* Flush ���� �������� �� */     
    UInt      mFlushIngPages;
    /* Flush �Ϸ��� �������� �� */
    UInt      mFlushDonePages;
    /* checkpoint list �� ��  */
    UInt      mCheckpointListPages;
    /* secondary Buffer�� �������� ��û�� Ƚ�� */
    ULong     mGetPages;
    /* secondary Buffer���� �������� �о Ƚ�� */
    ULong     mReadPages;
    /* single page  Read�� Read�� �ð� */
    ULong     mReadTime;
    /* single page  Read�� Checksum �� �ð� */
    ULong     mReadChecksumTime;
    /* secondary Buffer�� �������� ���� Ƚ��*/
    ULong     mWritePages;
    /* single page  Write �ð� */
    ULong     mWriteTime;
    /* single page  Checksum �ð� */
    ULong     mWriteChecksumTime;
    /* */
    SDouble   mHitRatio;
    /* victim�� ã�� ���� flusher�� ��ٸ� Ƚ�� */
    ULong     mVictimWaits;
    /* Fullscan Read�� chechsum �ð� */
    ULong     mMPRChecksumTime;
    /* Fullscan Read�� Read�� �ð� */
    ULong     mMPRReadTime;
    /* Fullscan Read�� Read�� Page ����  */
    ULong     mMPRReadPages;
} sdsBufferMgrStatData;

#endif
