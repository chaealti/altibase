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

#ifndef  _O_SDB_BUFFER_POOL_STAT_H_
#define  _O_SDB_BUFFER_POOL_STAT_H_ 1

#include <idl.h>
#include <idu.h>
#include <idv.h>
#include <smDef.h>
#include <sdbDef.h>
#include <sdpDef.h>
#include <sdbBCB.h>

typedef struct sdbBufferPoolStatData
{
    UInt      mID;
    UInt      mPoolSize;
    UInt      mPageSize;
    UInt      mHashBucketCount;
    UInt      mHashChainLatchCount;
    UInt      mLRUListCount;
    UInt      mPrepareListCount;
    UInt      mFlushListCount;
    UInt      mCheckpointListCount;
    UInt      mVictimSearchCount;
    UInt      mHashPages;
    UInt      mHotListPages;
    UInt      mColdListPages;
    UInt      mPrepareListPages;
    UInt      mFlushListPages;
    UInt      mCheckpointListPages;
    ULong     mFixPages;
    ULong     mGetPages;
    ULong     mReadPages;
    ULong     mCreatePages;
    SDouble   mHitRatio;
    /* LRU hot���� hit�� Ƚ�� */
    ULong     mHotHits;
    
    /* LRU Cold���� hit�� Ƚ�� */
    ULong     mColdHits;
    
    /* prepare List���� hit�� Ƚ�� */
    ULong     mPrepareHits;
    
    /* flush List���� hit�� Ƚ�� */
    ULong     mFlushHits;
    
    /* Delayed Flush List���� hit�� Ƚ�� */
    ULong     mDelayedFlushHits;

    /* prepare, lru, flush list�� ������ hit�� Ƚ��  */
    ULong     mOtherHits;
    
    /* prepare���� victim�� ã�� Ƚ��*/
    ULong     mPrepareVictims;
    
    /* LRU���� victim�� ã�� Ƚ�� */
    ULong     mLRUVictims;
    
    /* LRU���� victim�� ã�� ���� Ƚ�� */
    ULong     mVictimFails;
    
    /* victim�� ã�� ���� flusher�� ��ٸ� Ƚ�� */
    ULong     mVictimWaits;
    
    /* prepareList���� ����ϴٰ� BCB�� ���� Ƚ�� */
    ULong     mPrepareAgainVictims;
    
    /* prepareList���� ����ϴٰ� BCB�� ���� ���� Ƚ�� */
    ULong     mVictimSearchWarps;
    
    /* LRU list search Ƚ�� */
    ULong     mLRUSearchs;
    
    /* LRU List���� victim�� ã����, ���� �ѹ��� ��� BCB�� search�ϴ���
     * ��� */
    UInt      mLRUSearchsAvg;
    
    /* LRU search ���� LRU Hot���� BCB�� ���� Ƚ�� */
    ULong     mLRUToHots;
    
    /* LRU search ���� LRU Mid�� BCB�� ���� Ƚ�� */
    ULong     mLRUToColds;
    
    /* LRU search ���� FlushList�� BCB�� ���� Ƚ�� */
    ULong     mLRUToFlushs;

    /* LRU Hot�� ������ Ƚ�� */
    ULong     mHotInsertions;
    
    /* LRU Cold�� ������ Ƚ�� */
    ULong     mColdInsertions;

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance. */
    /* �Ϲ� Read�� checksum ��� �ð� */
    ULong     mNormalCalcChecksumTime;

    /* �Ϲ� Read�� Storage�κ��� Read�� �ð� */
    ULong     mNormalReadTime;

    /* �Ϲ� Read�� Read�� Page ����  */
    ULong     mNormalReadPageCount;

    /* Fullscan Read�� checksum ��� �ð� */
    ULong     mMPRCalcChecksumTime;

    /* Fullscan Read�� Storage�κ��� Read�� �ð� */
    ULong     mMPRReadTime;

    /* Fullscan Read�� Read�� Page ����  */
    ULong     mMPRReadPageCount;
} sdbBufferPoolStatData;

class sdbBufferPool;

/* �� page type���� ������ ���� �����ϱ� ���� �ʿ��� �ڷᱸ�� */
typedef struct sdbPageTypeStatData
{
    /* idvOwner */
    UInt      mOwner;
    UInt      mPageType;
    ULong     mFixPages;
    ULong     mGetPages;
    ULong     mReadPages;
    ULong     mCreatePages;
    
    /* LRU list�� ���� Victim�� ã�� ȸ�� */
    ULong     mVictimPagesFromLRU;     

    /* Prepare list�� ���� Victim�� ã�� ȸ�� */
    ULong     mVictimPagesFromPrepare; 

    /* LRU list���� Victim�� ã�� ���� Skip�� �������� ���� */
    ULong     mSkipPagesFromLRU;
    
    /* Prepare list���� Victim�� ã�� ���� Skip�� �������� ���� */
    ULong     mSkipPagesFromPrepare;
    
    SDouble   mHitRatio;

    /* LRU Hot���� hit�� Ƚ�� */
    ULong     mHotHits;

    /* LRU Cold���� hit�� Ƚ�� */
    ULong     mColdHits;

    /* prepare list���� hit�� Ƚ�� */
    ULong     mPrepareHits;

    /* flush list���� hit�� Ƚ�� */
    ULong     mFlushHits;

    /* Delayed Flush List���� hit�� Ƚ�� */
    ULong     mDelayedFlushHits;

    /* list�� ���� hit�� Ƚ�� */
    ULong     mOtherHits;
} sdbPageTypeStatData;

class sdbBufferPoolStat
{
public:
    IDE_RC initialize(sdbBufferPool *aBufferPool);
    IDE_RC destroy();

    void   updateBufferPoolStat();
    // ������ Ÿ�Ժ� ���� ����
    void applyFixPages( idvSQL   * aStatistics,
                        scSpaceID  aSpaceID,
                        scPageID   aPageID,
                        UInt       aPageType );

    void applyGetPages( idvSQL    * aStatistics,
                        scSpaceID   aSpaceID,
                        scPageID    aPageID,
                        UInt        aPageType );

    void applyReadPages( idvSQL    *aStatistics,
                         scSpaceID  aSpaceID,
                         scPageID   aPageID,
                         UInt       aPageType );

    void applyCreatePages(idvSQL   * aStatistics,
                          scSpaceID  aSpaceID,
                          scPageID   aPageID,
                          UInt       aPageType);

    inline void applyHits(idvSQL *aStatistics, UInt aPageType, UInt aListType);
    
    inline void applyBeforeMultiReadPages(idvSQL *aStatistics);
    inline void applyAfterMultiReadPages(idvSQL *aStatistics);
    inline void applyBeforeSingleReadPage(idvSQL *aStatistics);
    inline void applyAfterSingleReadPage(idvSQL *aStatistics);

    void applyMultiReadPages( idvSQL    * aStatistics,
                              scSpaceID   aSpaceID,
                              scPageID    aPageID,
                              UInt        aPageType);

    void applyVictimPagesFromLRU( idvSQL   * aStatistics,
                                  scSpaceID  aSpaceID,
                                  scPageID   aPageID,
                                  UInt       aPageType );

    void applyVictimPagesFromPrepare( idvSQL    * aStatistics,
                                      scSpaceID   aSpaceID,
                                      scPageID    aPageID,
                                      UInt        aPageType );

    void applySkipPagesFromLRU( idvSQL   * aStatistics,
                                scSpaceID  aSpaceID,
                                scPageID   aPageID,
                                UInt       aPageType );

    void applySkipPagesFromPrepare(idvSQL   * aStatistics,
                                   scSpaceID  aSpaceID,
                                   scPageID   aPageID,
                                   UInt       aPageType);


    // find victim ��� ���� ����
    inline void applyPrepareVictims();
    inline void applyLRUVictims();
    inline void applyPrepareAgainVictims();
    inline void applyVictimWaits();
    inline void applyVictimSearchWarps();

    // victim search ��� ���� ����
    inline void applyVictimSearchs();
    inline void applyVictimSearchsToHot();
    inline void applyVictimSearchsToCold();
    inline void applyVictimSearchsToFlush();

    // hot, cold ���� ���� ���� ����
    inline void applyHotInsertions();
    inline void applyColdInsertions();

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance. */
    inline void applyReadByNormal( ULong  aChecksumTime,
                                   ULong  aReadTime,
                                   ULong  aReadPageCount );
    inline void applyReadByMPR( ULong  aChecksumTime,
                                ULong  aReadTime,
                                ULong  aReadPageCount );

    inline void applyVictimWaitTimeMsec( ULong aMSec );

    void clearAll();

    void applyStatisticsForSystem();

    IDE_RC buildRecord(void                *aHeader,
                       iduFixedTableMemory *aMemory);

    IDE_RC buildPageInfoRecord(void                *aHeader,
                               iduFixedTableMemory *aMemory);


    SDouble getSingleReadPerf() /* USec����*/
    {
        SDouble sRet = 0;
        if( mPoolStat.mNormalReadTime == 0 )
        {
            sRet = 0;
        }
        else
        {
            sRet = (SDouble)mPoolStat.mNormalReadTime /
                   (SDouble)mPoolStat.mNormalReadPageCount;
        }
        return sRet;
    }
    SDouble getMultiReadPerf() /* USec����*/
    {
        SDouble sRet = 0;
        if( mPoolStat.mMPRReadTime == 0 )
        {
            sRet = 0;
        }
        else
        {
            sRet = (SDouble)mPoolStat.mMPRReadTime /
                   (SDouble)mPoolStat.mMPRReadPageCount;
        }
        return sRet;
    }

private:
    inline SDouble getHitRatio( ULong aGetFixPages, ULong aGetPages );

private:
    /* ��������� ������ buffer pool */
    sdbBufferPool         *mPool;
    
    /* buffer pool ��ü �ڷᱸ�� */
    sdbBufferPoolStatData  mPoolStat;
    
    /* mPageTypeStat[ PageType ][ owner ].mHitRatio�� ���� ���� */
    sdbPageTypeStatData  (*mPageTypeStat)[IDV_OWNER_MAX];
    
    /* page type����*/
    UInt                   mPageTypeCount;
};

void sdbBufferPoolStat::applyHits(idvSQL *aStatistics,
                                  UInt    aPageType,
                                  UInt    aListType)
{
    //IDE_DASSERT( aPageType < mPageTypeCount);
    //TODO: 1568
    idvOwner sOwner;

    if( aStatistics == NULL )
    {
        sOwner = IDV_OWNER_UNKNOWN;
    }
    else
    {
        sOwner = aStatistics->mOwner;
    }

    if( aPageType >= mPageTypeCount )
    {
        /* aPageType�� ���� �ʱ�ȭ ���� ���� ��찡 �ִ�.
         * ��, hash Table�� �����ϹǷ�, hit�� �Ͽ��� �� �Լ��� ȣ���Ͽ���������,
         * replace�Ǵ� MPR�� ��츦 �����غ���,
         * �̵��� ���� hash���̺� ������ �ϰ� �� ���Ŀ�,
         * ������ �����͸� �о� ���� ������(���� �����͸� �о� ���� ��������
         * � type���� �� �� ����)
         * pageType�� ����� �����Ǿ� ���� ���� �� �ִ�.
         * BUGBUG: �̰�� undefinedPageType���� ���� ������ ����,
         * ���� ��������� ��Ƶ� ���� �� �ʹ�.
         */
        return;
    }
    
    switch (aListType)
    {
        case SDB_BCB_LRU_HOT:
            mPageTypeStat[aPageType][sOwner].mHotHits++;
            break;
        case SDB_BCB_LRU_COLD:
            mPageTypeStat[aPageType][sOwner].mColdHits++;
            break;
        case SDB_BCB_PREPARE_LIST:
            mPageTypeStat[aPageType][sOwner].mPrepareHits++;
            break;
        case SDB_BCB_FLUSH_LIST:
            mPageTypeStat[aPageType][sOwner].mFlushHits++;
            break;
        case SDB_BCB_DELAYED_FLUSH_LIST:
            mPageTypeStat[aPageType][sOwner].mDelayedFlushHits++;
            break;
        default:
            mPageTypeStat[aPageType][sOwner].mOtherHits++;
            break;
    }
}

void sdbBufferPoolStat::applyBeforeMultiReadPages(idvSQL *aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_DB_FILE_MULTI_PAGE_READ,
                    0, /* WaitParam1 */
                    0, /* WaitParam2 */
                    0  /* WaitParam3 */ );

    IDV_BEGIN_WAIT_EVENT(aStatistics, &sWeArgs );
}

void sdbBufferPoolStat::applyAfterMultiReadPages(idvSQL *aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_DB_FILE_MULTI_PAGE_READ,
                    0, /* WaitParam1 */
                    0, /* WaitParam2 */
                    0  /* WaitParam3 */ );

    IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
}

void sdbBufferPoolStat::applyBeforeSingleReadPage(idvSQL *aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_READ,
                    0, /* WaitParam1 */
                    0, /* WaitParam2 */
                    0  /* WaitParam3 */ );

    IDV_BEGIN_WAIT_EVENT(aStatistics, &sWeArgs );
}

void sdbBufferPoolStat::applyAfterSingleReadPage(idvSQL *aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_DB_FILE_SINGLE_PAGE_READ,
                    0, /* WaitParam1 */
                    0, /* WaitParam2 */
                    0  /* WaitParam3 */ );

    IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
}



void sdbBufferPoolStat::applyPrepareVictims()
{
    mPoolStat.mPrepareVictims++;
}

void sdbBufferPoolStat::applyLRUVictims()
{
    mPoolStat.mLRUVictims++;
}

void sdbBufferPoolStat::applyPrepareAgainVictims()
{
    mPoolStat.mVictimFails++;
    mPoolStat.mPrepareAgainVictims++;
}

void sdbBufferPoolStat::applyVictimWaits()
{
    mPoolStat.mVictimWaits++;
}

void sdbBufferPoolStat::applyVictimSearchWarps()
{
    mPoolStat.mVictimFails++;
    mPoolStat.mVictimSearchWarps++;
}

void sdbBufferPoolStat::applyVictimSearchs()
{
    mPoolStat.mLRUSearchs++;
}

void sdbBufferPoolStat::applyVictimSearchsToHot()
{
    mPoolStat.mLRUToHots++;
}

void sdbBufferPoolStat::applyVictimSearchsToCold()
{
    mPoolStat.mLRUToColds++;
}

void sdbBufferPoolStat::applyVictimSearchsToFlush()
{
    mPoolStat.mLRUToFlushs++;
}

void sdbBufferPoolStat::applyHotInsertions()
{
    mPoolStat.mHotInsertions++;
}

void sdbBufferPoolStat::applyColdInsertions()
{
    mPoolStat.mColdInsertions++;
}

void sdbBufferPoolStat::applyReadByNormal( ULong  aChecksumTime,
                                           ULong  aReadTime,
                                           ULong  aReadPageCount )
{
    mPoolStat.mNormalCalcChecksumTime += aChecksumTime;
    mPoolStat.mNormalReadTime         += aReadTime;
    mPoolStat.mNormalReadPageCount    += aReadPageCount;
}

void sdbBufferPoolStat::applyReadByMPR( ULong  aChecksumTime,
                                        ULong  aReadTime,
                                        ULong  aReadPageCount )
{
    mPoolStat.mMPRCalcChecksumTime += aChecksumTime;
    mPoolStat.mMPRReadTime         += aReadTime;
    mPoolStat.mMPRReadPageCount    += aReadPageCount;
}

/* BUG-21307: VS6.0���� Compile Error�߻�.
 *
 * ULong�� double�� casting�� win32���� ���� �߻� */
inline SDouble sdbBufferPoolStat::getHitRatio( ULong aGetFixPages,
                                               ULong aReadPages )
{
    SDouble sGetFixPages = UINT64_TO_DOUBLE( aGetFixPages );
    SDouble sMissGetPages = sGetFixPages - UINT64_TO_DOUBLE( aReadPages );

    return sMissGetPages / sGetFixPages * 100.0;
}

#endif//   _O_SDB_BUFFER_POOL_STAT_H_
