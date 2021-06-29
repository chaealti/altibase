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
/******************************************************************************
 * PROJ-1568 BUFFER MANAGER RENEWAL
 ******************************************************************************/
#ifndef _O_SDB_BUFFER_POOL_H_
#define _O_SDB_BUFFER_POOL_H_ 1

#include <idl.h>
#include <idu.h>
#include <smDef.h>
#include <sdbDef.h>
#include <iduLatch.h>

#include <sdbFlushList.h>
#include <sdbLRUList.h>
#include <sdbPrepareList.h>
#include <sdbCPListSet.h>
#include <sdbBCBHash.h>
#include <sdbBCB.h>
#include <sdbBufferPoolStat.h>
#include <sdbMPRKey.h>

#include <sdsBCB.h>

#define SDB_BUFFER_POOL_INVALIDATE_WAIT_UTIME   (100000)

class sdbBufferPool
{
public:
    IDE_RC initialize(UInt aBCBCnt,
                      UInt aBucketCnt,
                      UInt aBucketCntPerLatch,
                      UInt aLRUListNum,
                      UInt aPrepareListNum,
                      UInt aFlushListNum,
                      UInt aChkptListNum);

    IDE_RC destroy();

    IDE_RC initPrepareList();
    IDE_RC initLRUList();
    IDE_RC initFlushList();

    void destroyPrepareList();
    void destroyLRUList();
    void destroyFlushList();


    inline IDE_RC resizeHashTable( UInt       aNewHashBucketCnt,
                                   UInt       aBucketCntPerLatch);

    inline void expandBufferPool( idvSQL    *aStatistics,
                                  sdbBCB    *aFirstBCB,
                                  sdbBCB    *aLastBCB,
                                  UInt       aLength );


    void distributeBCB2PrepareLsts( idvSQL    *aStatistics,
                                 sdbBCB    *aFirstBCB,
                                 sdbBCB    *aLastBCB,
                                 UInt       aLength );

    IDE_RC addBCB2PrepareLst(idvSQL *aStatistics,
                             sdbBCB *aBCB);

    void addBCBList2PrepareLst(idvSQL    *aStatistics,
                               sdbBCB    *aFirstBCB,
                               sdbBCB    *aLastBCB,
                               UInt       aLength);

    IDE_RC getVictim( idvSQL  *aStatistics,
                      UInt     aListKey,
                      sdbBCB **aReturnedBCB);

    IDE_RC createPage( idvSQL          *aStatistics,
                       scSpaceID        aSpaceID,
                       scPageID         aPageID,
                       UInt             aPageType,
                       void*            aMtx,
                       UChar**          aPagePtr);

    IDE_RC getPage( idvSQL               * aStatistics,
                    scSpaceID              aSpaceID,
                    scPageID               aPageID,
                    sdbLatchMode           aLatchMode,
                    sdbWaitMode            aWaitMode,
                    sdbPageReadMode        aReadMode,
                    void                 * aMtx,
                    UChar               ** aRetPage,
                    idBool               * aTrySuccess,
                    idBool               * aIsCorruptPage );

    IDE_RC fixPage( idvSQL              *aStatistics,
                    scSpaceID            aSpaceID,
                    scPageID             aPageID,
                    UChar              **aRetPage);

    IDE_RC fixPageWithoutIO( idvSQL              *aStatistics,
                             scSpaceID            aSpaceID,
                             scPageID             aPageID,
                             UChar              **aRetPage);

#if 0 //not used
    IDE_RC getPagePtrFromGRID( idvSQL     * aStatistics,
                               scGRID       aGRID,
                               UChar      **aPagePtr );
#endif
    void setDirtyPage( idvSQL *aStatistics,
                       void   *aBCB,
                       void   *aMtx );

    void releasePage( idvSQL * aStatistics,
                      void   * aBCB,
                      UInt     aLatchMode );

    void releasePage( idvSQL          *aStatistics,
                      void            *aBCB );

    void releasePageForRollback( idvSQL       *aStatistics,
                                 void         *aBCB,
                                 UInt          aLatchMode );

    void unfixPage( idvSQL     * aStatistics,
                    UChar      * aPagePtr);

    inline void unfixPage( idvSQL     * aStatistics,
                           sdbBCB     * aBCB);
#ifdef DEBUG
    inline idBool isPageExist( idvSQL       *aStatistics,
                               scSpaceID     aSpaceID,
                               scPageID      aPageID);
#endif
    void discardBCB( idvSQL                 *aStatistics,
                     sdbBCB                 *aBCB,
                     sdbFiltFunc             aFilter,
                     void                   *aObj,
                     idBool                  aIsWait,
                     idBool                 *aIsSuccess,
                     idBool                 *aMakeFree);


    void invalidateDirtyOfBCB( idvSQL                 *aStatistics,
                               sdbBCB                 *aBCB,
                               sdbFiltFunc             aFilter,
                               void                   *aObj,
                               idBool                  aIsWait,
                               idBool                 *aIsSuccess,
                               idBool                 *aFiltOK);

    idBool makeFreeBCB(idvSQL     *aStatistics,
                       sdbBCB     *aBCB,
                       sdbFiltFunc aFilter,
                       void       *aObj);

    idBool removeBCBFromList(idvSQL *aStatistics, sdbBCB *aBCB);

    inline void setInvalidateWaitTime(UInt  aMicroSec)
    {   mInvalidateWaitTime.set(0, aMicroSec ); };

    // sdbBCBListStat�� ���� ����Ʈ�� ���� �����ϴ� �Լ���
    inline sdbLRUList  *   getLRUList(UInt aID);
    inline sdbFlushList*   getFlushList(UInt aID);
    inline sdbPrepareList* getPrepareList(UInt aID);

    sdbLRUList     *getMinLengthLRUList(void);
    sdbPrepareList *getMinLengthPrepareList(void);
    sdbFlushList   *getMaxLengthFlushList(void);
    UInt            getFlushListTotalLength(void);
    UInt            getFlushListNormalLength(void);
    UInt            getDelayedFlushListLength(void);
    UInt            getFlushListLength( UInt aID );
    void            setMaxDelayedFlushListPct( UInt aMaxListPct );
    inline UInt     getDelayedFlushCnt4LstFromPCT( UInt aMaxListPct );

    sdbFlushList   *getDelayedFlushList( UInt aIndex );


    // multi page read���� �Լ���
    IDE_RC fetchPagesByMPR( idvSQL               * aStatistics,
                            scSpaceID              aSpaceID,
                            scPageID               aStartPID,
                            UInt                   aPageCount,
                            sdbMPRKey            * aKey );

    void releasePage( sdbMPRKey *aKey,
                      UInt       aLatchMode );

    void cleanUpKey(idvSQL    *aStatistics,
                    sdbMPRKey *aKey,
                    idBool     aCachePage);

    ////////////////////////////////////////////////
    inline IDE_RC findBCB( idvSQL       * aStatistics,
                           scSpaceID      aSpaceID,
                           scPageID       aPageID,
                           sdbBCB      ** aBCB );

    inline void applyStatisticsForSystem();

    inline sdbBufferPoolStat *getBufferPoolStat();

    inline void lockPoolXLatch(idvSQL *aStatistics);
    inline void lockPoolSLatch(idvSQL *aStatistics);
    inline void unlockPoolLatch(void);

    // BufferPool �ɹ����� get �Լ���
    inline UInt getID();
    inline UInt getPoolSize();
    inline UInt getPageSize();
    inline sdbCPListSet *getCPListSet();
    inline UInt getHashBucketCount();
    inline UInt getHashChainLatchCount();
    inline UInt getLRUListCount();
    inline UInt getPrepareListCount();
    inline UInt getFlushListCount();
    inline UInt getCheckpointListCount();

    // ��������� ������� �Լ���
    inline UInt getHashPageCount();
    inline UInt getHotListPageCount();
    inline UInt getColdListPageCount();
    inline UInt getPrepareListPageCount();
    inline UInt getFlushListPageCount();
    inline UInt getCheckpointListPageCount();

    inline void latchPage( idvSQL        * aStatistics,
                           sdbBCB        * aBCB,
                           sdbLatchMode    aLatchMode,
                           sdbWaitMode     aWaitMode,
                           idBool        * aTrySuccess );

    inline void unlatchPage( sdbBCB *aBCB );
    inline UInt getLRUSearchCount();

    inline idBool isHotBCB(sdbBCB *aBCB);
    inline void   setLRUSearchCnt(UInt aLRUSearchPCT);

    static inline idBool isFixedBCB(sdbBCB *aBCB);

    void setHotMax(idvSQL *aStatistics, UInt aHotMaxPCT);
    void setPageLSN( sdbBCB *aBCB, void *aMtx );
    void tryEscalateLatchPage( idvSQL            *aStatistics,
                               sdbBCB            *aBCB,
                               sdbPageReadMode    aReadMode,
                               idBool            *aTrySuccess );

    void setFrameInfoAfterRecoverPage( sdbBCB * aBCB,
                                       idBool   aChkOnlineTBS );
    /* PROJ-2102 Secondary Buffer */
    inline idBool isSBufferServiceable( void );
    inline void setSBufferServiceState( idBool aState );

private:
    inline void setDirtyBCB( idvSQL * aStatistics,
                             sdbBCB * aBCB );

    IDE_RC readPage( idvSQL               *aStatistics,
                     scSpaceID             aSpaceID,
                     scPageID              aPageID,
                     sdbPageReadMode       aReadMode,
                     idBool               *aHit,
                     sdbBCB              **aReplacedBCB,
                     idBool               *aIsCorruptPage );

    void getVictimFromPrepareList(idvSQL  *aStatistics,
                                  UInt     aListKey,
                                  sdbBCB **aResidentBCB);

    void getVictimFromLRUList( idvSQL  *aStatistics,
                               UInt     aListKey,
                               sdbBCB **aResidentBCB);

    IDE_RC readPageFromDisk( idvSQL               *aStatistics,
                             sdbBCB               *aBCB,
                             idBool               *aIsCorruptPage );

    void setFrameInfoAfterReadPage( sdbBCB *aBCB,
                                    idBool  aChkOnlineTBS );

    IDE_RC fixPageInBuffer( idvSQL             *aStatistics,
                            scSpaceID           aSpaceID,
                            scPageID            aPageID,
                            sdbPageReadMode     aReadMode,
                            UChar             **aRetPage,
                            idBool             *aHit,
                            sdbBCB            **aFixedBCB,
                            idBool             *aIsCorruptPage);


    // multi page read ���� �Լ���
    IDE_RC copyToFrame( idvSQL                 *aStatistics,
                        scSpaceID               aSpaceID,
                        sdbReadUnit            *aReadUnit,
                        UChar                  *aPagePtr );

    IDE_RC fetchMultiPagesNormal( idvSQL                * aStatistics,
                                  sdbMPRKey             * aKey );

    IDE_RC fetchMultiPagesAtOnce( idvSQL              * aStatistics,
                                  scPageID              aStartPID,
                                  UInt                  aPageCount,
                                  sdbMPRKey           * aKey );

    IDE_RC fetchSinglePage( idvSQL                 * aStatistics,
                            scPageID                 aPageID,
                            sdbMPRKey              * aKey );

    idBool analyzeCostToReadAtOnce(sdbMPRKey *aKey);

    inline UInt getHotCnt4LstFromPCT( UInt aHotMaxPCT );
    inline UInt getPrepareListIdx( UInt aListKey );
    inline UInt getLRUListIdx( UInt aListKey );
    inline UInt getFlushListIdx( UInt aListKey );
    inline UInt genListKey( UInt aPageID, idvSQL *aStatistics );

    inline void fixAndUnlockHashAndTouch( idvSQL                   *aStatistics,
                                          sdbBCB                   *aBCB,
                                          sdbHashChainsLatchHandle *aHandle );
#ifdef DEBUG    
    IDE_RC validate( sdbBCB *aBCB );
#endif
public:
    /* �  BCB�� touch count�� mHotTouchCnt�� ���ų� Ŭ ��,
     * �� BCB�� hot���� �Ǵ��Ѵ�.  */
    UInt              mHotTouchCnt;

private:
    /* bufferPool�� �ٷ�� ������ ũ��.. ����� 8K */
    UInt              mPageSize;

    /* ���� sdbLRUList�� �迭 ���·� ������ �ִ�.*/
    sdbLRUList       *mLRUList;
    /* LRU list�� ���� */
    UInt              mLRUListCnt;

    /* ���� sdbPrepareList�� �迭 ���·� ������ �ִ�.  */
    sdbPrepareList   *mPrepareList;
    /* prepare list ���� */
    UInt              mPrepareListCnt;

    /* ���� sdbFlushList�� �迭 ���·� ������ �ִ�. */
    sdbFlushList     *mFlushList;

    /* PROJ-2669
     * �ֱٿ� touch �� Dirty BCB �� �����ϴ� Delayed Flush List */
    sdbFlushList     *mDelayedFlushList;

    /* flush list�� ���� */
    UInt              mFlushListCnt;

    /* ��ü������ ����ȭ �Ǿ��ִ� sdbCPListSet�ϳ��� ���� */
    sdbCPListSet      mCPListSet;

    /* �ܺο��� Ʈ������� �ѹ��� BCB�� ã�� ���� �ְ� BCB���� hash table ��
     * �ִ�. */
    sdbBCBHash        mHashTable;

    /* Dirty BCB�� clean���� �����ϴ� ������ �ִ�.
     * �̶�, InIOB�� redirty������ ��쿡 BCB�� ���°� ���ϱ� ��ٷ��� �ϴµ�,
     * �󸶸�ŭ ��ٸ��� ���� */
    PDL_Time_Value    mInvalidateWaitTime;

    // buffer pool�� �����ϴ� ��ü BCB����
    UInt              mBCBCnt;

    /* victim�� LRUList���� ������, Ž���� BCB���� */
    UInt              mLRUSearchCnt;

    /* Secondary Buffer �� ��� �������� ���� */
    idBool            mIsSBufferServiceable;

    /* Buffer Pool ���� ������� */
    sdbBufferPoolStat mStatistics;
};

/****************************************************************
 * Description:
 *  ����Ʈ�� �������̹Ƿ�, ����Ʈ �� �ϳ��� �����ؾ� �Ѵ�. �̶�,
 *  aListKey�� �̿��� �ϳ��� ������ �ش�.
 *  aListKey�� ���� �����ϰ� ����Ʈ�� �й��ϱ� ���� ����� �ܼ��� mod ������
 *  �̿��Ѵ�.
 *
 *  aListKey    - [IN]  ���� ����Ʈ�� �ϳ��� �����ϱ� ���� ����
 ****************************************************************/
inline UInt sdbBufferPool:: getPrepareListIdx( UInt aListKey )
{
    return aListKey % mPrepareListCnt;
}

inline UInt sdbBufferPool:: getLRUListIdx( UInt aListKey )
{
    return aListKey % mLRUListCnt;
}

inline UInt sdbBufferPool::getFlushListIdx( UInt aListKey )
{
    return  aListKey % mFlushListCnt;
}


/****************************************************************
 * Description:
 *  ������ BCB�� �����ϰ� �ִٸ�, ID_TRUE�� �����Ѵ�.
 * Implemenation:
 *  BCB�� mFixCnt�� 1�̻��̸� true�� ����.
 *  fixCount���� Ư�� hashTable���ü��� ���� ������ �ִ�.
 *  fixCount�� 1�̻��� ��� hashTable���� ����� �����ؼ��� �ȵȴ�.
 *  ���� ������, ���� fixCount�� ���� ���� ������ ����� hashTable���� ���ŵ���
 *  ������ ������ �� �ִ�.
 *
 *  aBCB        - [IN]  BCB
 ****************************************************************/
inline idBool  sdbBufferPool::isFixedBCB( sdbBCB *aBCB )
{
    if ( aBCB->mFixCnt > 0 )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}


/****************************************************************
 * Description :
 *  fixPage - unfixPage�� ������ ȣ��ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  fixPage BCB
 ****************************************************************/
inline void sdbBufferPool::unfixPage( idvSQL     * aStatistics,
                                      sdbBCB     * aBCB )
{
    aBCB->lockBCBMutex( aStatistics);
    aBCB->decFixCnt();
    aBCB->unlockBCBMutex();
    return;
}

/****************************************************************
 * Description :
 *  aBCB�� hash�� �����Ҷ�, �� BCB�� fix��Ű��, touch count��
 *  �����ϰ�, hashLatch�� Ǭ��.
 *  aBCB�� hit�Ͽ����� ȣ��Ǵ� ������ ������ �ζ��� ���� ���
 *  ���� �� �Լ�
 *
 *  ����! �ݵ�� �ش� hash ��ġ�� ��� ������, aBCB�� �ݵ��
 *  �ؽÿ� �����Ҷ�, ���Լ��� ȣ���Ѵ�. �� �Լ������� hash ��ġ�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 *  aHandle     - [IN]  unlock�� ���� �ʿ��� hash handle
 ****************************************************************/

inline void sdbBufferPool::fixAndUnlockHashAndTouch(
                                    idvSQL                    *aStatistics,
                                    sdbBCB                    *aBCB,
                                    sdbHashChainsLatchHandle  *aHandle )
{
    aBCB->lockBCBMutex( aStatistics );
    /* fix count ������ �ݵ�� hash latch�� ��� �ִ� ���¿��� �Ѵ�.
     * �׷��� ������ fix count���� ���� hash���� ���ŵ� �� �ִ�.
     * ���� BCBMutex�� ��� �Ѵ�. hash S latch�� ���� �ΰ� �̻���
     * Ʈ������� ���ÿ� fix�� ������ �� �ֱ� �����̴�. */
    aBCB->incFixCnt();

    /*�ϴ� fix�� �س����� ���� hash���� ���� ���� �ʱ� ������
     *hash ��ġ�� Ǭ��.*/
    mHashTable.unlockHashChainsLatch( aHandle );
    aBCB->updateTouchCnt();
    aBCB->unlockBCBMutex();
}

/****************************************************************
 * Description :
 *  aBCB�� mState�� �����ϰ�, ��, dirty��Ű��, üũ����Ʈ ����Ʈ��
 *  �����մϴ�.
 *
 *  ����! ���Լ��� �ݵ�� ������ x��ġ�� ��� ȣ��˴ϴ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 ****************************************************************/
inline void sdbBufferPool::setDirtyBCB( idvSQL *aStatistics,
                                        sdbBCB *aBCB )
{
    /*
     * aBCB->mState�� Ȯ���ϰ� �����ϴµ� ���ü��� ������ �ֱ� ���ؼ�
     * BCBMutex�� ����ϴ�.
     * �� �Լ��� setDirtyPage�� �Ҷ� ȣ��˴ϴ�. �׷��Ƿ�, fix�Ǿ� ������
     * victim���� �������� �ʱ� ������,
     * getVictim�� �ϴ� Ʈ����ǰ� ���ü� ������ �߻����� �ʽ��ϴ�.
     *
     * flusher���� ������ ��ũ I/O�� �� ���Ŀ�, BCB���� ���¸� INIOB����
     * clean���� �����ϴµ�, �̶��� ���ü� ������ �ذ��ϱ� ���ؼ� BCBMutex��
     * ����ϴ�.
     *
     * flusher�Ǵ� �ٸ� setDirty������� ���ü��� ������ x ��ġ�� �մϴ�.
     **/
    aBCB->lockBCBMutex( aStatistics );
    if ( aBCB->mState == SDB_BCB_CLEAN )
    {
        aBCB->mState = SDB_BCB_DIRTY;
    }
    else
    {
        /* �� �Լ��� x��ġ�� ��� ���� ���°� CLEAN �Ǵ� INIOB ���� �϶�
         * ��� �ɴϴ�. �� ���� ���¸� �����ų ������ �ִ� flusher��
         * �ٸ� setDirty������� x��ġ�� ��� �Ǳ� ������,
         * CLEAN ���°� �ƴ϶��, INIOB�������� ������ �� �ֽ��ϴ�. */
        // �÷��ſ� ���� INIOB���·� ����ȴ�.
        IDE_ASSERT( aBCB->mState == SDB_BCB_INIOB);
        aBCB->mState = SDB_BCB_REDIRTY;
    }
    aBCB->unlockBCBMutex();

    // recoverLSN�� init�̶� ���� no logging���� ���ŵ� temp page�̴�.
    // �� ��쿣 checkpoint list�� ���� �ʴ´�.
    if ( !SM_IS_LSN_INIT(aBCB->mRecoveryLSN) )
    {
        mCPListSet.add( aStatistics, aBCB );
    }
    return ;
}

/****************************************************************
 * Description :
 *  hash tableũ�� ����.. ���� �ؾ� �Ұ�, resizeHashTable�߰���
 *  ����� hashTable�� �����ϸ� �ȵȴ�.
 *
 * aNewHashBucketCnt    - [IN] ���� ���� hashBucketCnt
 * aBucketCntPerLatch   - [IN] ���� ���� aBucketCntPerLatch
 ****************************************************************/
inline IDE_RC sdbBufferPool::resizeHashTable( UInt aNewHashBucketCnt,
                                              UInt aBucketCntPerLatch )
{
    return mHashTable.resize( aNewHashBucketCnt,
                              aBucketCntPerLatch );
}

inline void sdbBufferPool::applyStatisticsForSystem()
{
    mStatistics.applyStatisticsForSystem();
}

inline sdbBufferPoolStat *sdbBufferPool::getBufferPoolStat()
{
    return &mStatistics;
}

inline UInt sdbBufferPool::getPoolSize()
{
    return mBCBCnt;
}

inline UInt sdbBufferPool::getPageSize()
{
    return mPageSize;
}

inline sdbCPListSet *sdbBufferPool::getCPListSet()
{
    return &mCPListSet;
}

inline UInt sdbBufferPool::getHashBucketCount()
{
    return mHashTable.getBucketCount();
}

inline UInt sdbBufferPool::getHashChainLatchCount()
{
    return mHashTable.getLatchCount();
}

inline UInt sdbBufferPool::getLRUListCount()
{
    return mLRUListCnt;
}

inline UInt sdbBufferPool::getPrepareListCount()
{
    return mPrepareListCnt;
}

inline UInt sdbBufferPool::getFlushListCount()
{
    return mFlushListCnt;
}

inline UInt sdbBufferPool::getCheckpointListCount()
{
    return mCPListSet.getListCount();
}

inline UInt sdbBufferPool::getHashPageCount()
{
    return mHashTable.getBCBCount();
}

inline UInt sdbBufferPool::getHotListPageCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < mLRUListCnt; i++)
    {
        sRet += mLRUList[i].getHotLength();
    }
    return sRet;
}

inline UInt sdbBufferPool::getColdListPageCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < mLRUListCnt; i++)
    {
        sRet += mLRUList[i].getColdLength();
    }
    return sRet;
}

inline UInt sdbBufferPool::getPrepareListPageCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < mPrepareListCnt; i++)
    {
        sRet += mPrepareList[i].getLength();
    }
    return sRet;
}

inline UInt sdbBufferPool::getFlushListPageCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < mFlushListCnt; i++)
    {
        /* PROJ-2669 Normal + Delayed Flush List */
        sRet += getFlushListLength( i );
    }
    return sRet;
}

inline UInt sdbBufferPool::getCheckpointListPageCount()
{
    return mCPListSet.getTotalBCBCnt();
}

inline sdbLRUList* sdbBufferPool::getLRUList(UInt aID)
{
    IDE_DASSERT(aID < mLRUListCnt);
    return &mLRUList[aID];
}

inline sdbFlushList* sdbBufferPool::getFlushList(UInt aID)
{
    IDE_DASSERT(aID < mFlushListCnt);
    return &mFlushList[aID];
}

inline sdbPrepareList* sdbBufferPool::getPrepareList(UInt aID)
{
    IDE_DASSERT(aID < mPrepareListCnt);
    return &mPrepareList[aID];
}

inline UInt sdbBufferPool::getHotCnt4LstFromPCT(UInt aHotMaxPCT)
{
    IDE_DASSERT( mBCBCnt > 0 );
    IDE_DASSERT( mLRUListCnt > 0 );

    if (mBCBCnt <= SDB_SMALL_BUFFER_SIZE)
    {
        return 1;
    }
    else
    {
        return (mBCBCnt * aHotMaxPCT ) / (mLRUListCnt * 100);
    }
}

inline void  sdbBufferPool::setLRUSearchCnt(UInt aLRUSearchPCT)
{
    IDE_DASSERT( mBCBCnt > 0 );
    IDE_DASSERT( mLRUListCnt > 0 );
    mLRUSearchCnt = (mBCBCnt * aLRUSearchPCT ) / mLRUListCnt / 100;

    if (mLRUSearchCnt == 0)
    {
        mLRUSearchCnt = 1;
    }
}

/***********************************************************************
 * Description :
 *  ���� ����Ʈ�� �ϳ��� �����Ҷ� ���̴� list key�� �����Ѵ�.  ����� pageID��
 *  aStatistics�� ��� ����Ѵ�.
 *
 *  aPageID     - [IN]  page ID
 *  aStatistics - [IN]  �������
 ***********************************************************************/
inline UInt sdbBufferPool::genListKey( UInt   aPageID,
                                       idvSQL *aStatistics )
{
    return aPageID + ( (UInt)(vULong)aStatistics >> 4);
}

#ifdef DEBUG
/****************************************************************
 * Description :
 *  ���ۿ� �ش� BCB�� �����ϴ��� ���θ� �������ִ� �Լ�
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 ****************************************************************/
inline idBool sdbBufferPool::isPageExist( idvSQL       *aStatistics,
                                          scSpaceID     aSpaceID,
                                          scPageID      aPageID )
{
    sdbBCB *sBCB;

    // ������ BCB�����翩�η� DASSERT �ϴϱ� ����� ID_FALSE�� �ø�.
    (void)findBCB( aStatistics, aSpaceID, aPageID, &sBCB );
    
    if ( sBCB == NULL )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}
#endif

/****************************************************************
 * Description :
 *  PROJ-2669
 *  Delayed Flush List �� �ִ� ���̸� �����Ѵ�.
 *
 *  aMaxListPct - [IN]  Delayed Flush List �� �ִ� ����(Percent)
 ****************************************************************/
inline UInt sdbBufferPool::getDelayedFlushCnt4LstFromPCT( UInt aMaxListPct )
{
    IDE_DASSERT( mBCBCnt > 0 );
    IDE_DASSERT( mFlushListCnt > 0 );

    if ( mBCBCnt <= SDB_SMALL_BUFFER_SIZE )
    {
        return 1;
    }
    else
    {
        return (UInt)( ( (ULong)mBCBCnt * aMaxListPct ) / ( mFlushListCnt * 100 ) );
    }
}

/****************************************************************
 * Description :
 *  �ش��ϴ� BCB�� ���ۿ� �����ϴ� ��� BCB�� �����Ѵ�.
 *  �׷��� �ʴ� ��쿣 NULL�� �����Ѵ�.
 *  ����! ���ϵǴ� BCB�� ���ؼ� � ��ġ�� ���� ���� �ʱ� ������,
 *  BCB�� ���� ���ü��� �ܺο��� �����ؾ� �Ѵ�.
 *  ��, ���� ���� BCB�� ������ �񵿱������� ���� �� �ְ�, ���� ���� �Ұ����ϴ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 *  aBCB        - [OUT] find�ؼ� �߰��� BCB����.
 ****************************************************************/
inline IDE_RC sdbBufferPool::findBCB( idvSQL       * aStatistics,
                                      scSpaceID      aSpaceID,
                                      scPageID       aPageID,
                                      sdbBCB      ** aBCB )
{
    sdbHashChainsLatchHandle    * sHandle = NULL;

    IDE_DASSERT( aBCB != NULL );
    IDE_DASSERT( aSpaceID <= SC_MAX_SPACE_COUNT - 1);

    *aBCB = NULL;

    sHandle = mHashTable.lockHashChainsSLatch( aStatistics,
                                               aSpaceID,
                                               aPageID );

    IDE_TEST( mHashTable.findBCB( aSpaceID,
                                  aPageID,
                                  (void**)aBCB )
              != IDE_SUCCESS );

    mHashTable.unlockHashChainsLatch( sHandle );
    sHandle = NULL;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if ( sHandle != NULL )
    {
        mHashTable.unlockHashChainsLatch( sHandle );
        sHandle = NULL;
    }
    else
    {
        /* nothing to do */
    }
    
    return IDE_FAILURE;
}

inline UInt sdbBufferPool::getLRUSearchCount()
{
    return mLRUSearchCnt;
}

/****************************************************************
 * Description :
 *  BCB�� touch count�� ���� hot���� ���θ� �Ǵ��Ѵ�.
 *  �̶�, ����ڰ� ���� property�� ���ؼ� hot���� ���θ� �Ǵ��Ѵ�.
 *  touch count�� �������� �ʰ� �׻� �����ϴ� ���̸�,
 *  BCB�� �����ϴ� ��쿡 �����Ѵ�.
 *  ���������� �����ϴ� ��쿡�� ������Ű�� �ʴ´�.
 ****************************************************************/
inline idBool sdbBufferPool::isHotBCB(sdbBCB  *aBCB)
{
    if( aBCB->mTouchCnt >= mHotTouchCnt )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/****************************************************************
 * Description :
 *  �ܺη� ���� BCB����Ʈ�� �޾� �ڽ��� ������ prepareList �� ���� �޾��ش�.
 *
 *  aStatistics - [IN]  �������
 *  aFirstBCB   - [IN]  BCB����Ʈ�� head
 *  aLastBCB    - [IN]  BCB����Ʈ�� tail
 *  aLength     - [IN]  �����ϴ� BCB����Ʈ�� ����
 ****************************************************************/
inline void sdbBufferPool::expandBufferPool( idvSQL    *aStatistics,
                                             sdbBCB    *aFirstBCB,
                                             sdbBCB    *aLastBCB,
                                             UInt       aLength )
{
    IDE_ASSERT( mBCBCnt + aLength > mBCBCnt);

    mBCBCnt += aLength;
    // ����Ǯ�� BCB������ ���� �Ͽ����Ƿ�, �� LRUList�� HotMax�� ���� ��Ų��.
    setHotMax( aStatistics, smuProperty::getHotListPct());

    distributeBCB2PrepareLsts( aStatistics, aFirstBCB, aLastBCB, aLength);

    return;
}

inline void sdbBufferPool::unlatchPage( sdbBCB *aBCB )
{
    aBCB->unlockPageLatch();
}

/***********************************************************************
 * Descrition:
 *  RedoMgr�� ���� Page�� �����Ǿ�����, �ش� Page�� Frame�� ������
 *  �ʿ��� ������ �����Ѵ�.
 *
 *  aBCB            - [IN]  BCB
 *  aChkOnlineTBS   - [IN] TBS Online ���θ� Ȯ���ؼ� SMO NO ���� (Yes/No)
 **********************************************************************/
inline void sdbBufferPool::setFrameInfoAfterRecoverPage(
                                            sdbBCB  * aBCB,
                                            idBool    aChkOnlineTBS )
{
    setFrameInfoAfterReadPage( aBCB, aChkOnlineTBS );
}

/* PROJ-2102 Secondary Buffer */
/****************************************************************
 * Description : Secondary Buffer�� ��� �������� Ȯ�� 
 ****************************************************************/
inline idBool sdbBufferPool::isSBufferServiceable( void )
{
    return mIsSBufferServiceable;
}

/****************************************************************
 * Description : Secondary Buffer�� ������ Ȯ��  
 ****************************************************************/
inline void sdbBufferPool::setSBufferServiceState( idBool aState )
{
    mIsSBufferServiceable = aState;
}

/****************************************************************
 * Description:
 *  aBCB�� ������ ��ġ�� �Ǵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 *  aLatchMode  - [IN]  ��ġ ���, shared, exclusive, nolatch�� �ִ�.
 *  aWaitMode   - [IN]  ������� ���� ����
 *  aSuccess    - [OUT] lock�� ���������� ��Ҵ��� ���θ� �����Ѵ�.
 ****************************************************************/
inline void sdbBufferPool::latchPage( idvSQL        * aStatistics,
                                      sdbBCB        * aBCB,
                                      sdbLatchMode    aLatchMode,
                                      sdbWaitMode     aWaitMode,
                                      idBool        * aSuccess )
{
    idBool sSuccess = ID_FALSE;
    IDE_DASSERT( isFixedBCB( aBCB ) == ID_TRUE );

    if ( aLatchMode == SDB_NO_LATCH )
    {
        /* �ƹ��� no latch�� dirty read�� �Ѵٰ� �ص�, ���� ��ũ����
         * read I/O���� ���۸� ������ �ʵȴ�. �׷��� ������ read I/O��
         * �������� ��� �̰��� ������ ���� ����Ѵ�.
         * read I/O���� Ʈ������� ������ x��ġ �Ӹ� �ƴ϶� mReadIOMutex��
         * ��⶧���� no_latch�� �����ϴ� Ʈ������� ��쿡�� mReadIOMutex
         * ���� ����ϰ� �ȴ�.*/
        /* ��ǻ� mReadyToRead �� mReadIOMutex �� �� �ϳ��� �־ �ȴ�.
         * ������, mReadyToRead�� ���ٸ� �Ź� mReadIOMutex�� ��ƾ� �ϹǷ�
         * ����� ���,
         * mReadIOMutex�� ���ٸ�, ���� �ڽ��� �о�� �ϴ��� �˾ƺ��� ���ؼ�
         * loop�� ���ƾ� �ϴ� �� �ۿ� ����.
         * �׷��� ������ �Ѵ� ���� �Ѵ�.*/
        if ( aBCB->mReadyToRead == ID_FALSE )
        {
            IDE_ASSERT ( aBCB->mReadIOMutex.lock( aStatistics ) == IDE_SUCCESS );
            IDE_ASSERT ( aBCB->mReadIOMutex.unlock() == IDE_SUCCESS );
        }
        else
        {
            /* Read Page�� ���� mReadyToRead�� Frame�� �����ϴ� ������ ������ �ڹٲ�� �ִµ�
             * read�ϴ� Ʈ������� ReadIoMutex�� latch�� ������ ����������, No-latch��
             * latchPage�ϴ� Ʈ������� ������ �ڹٲ�� �־ mReadyToRead�� TRUE��� �ϴ���
             * mFrame�� ����� �ǵ��� �� �ֵ��� Barrier�� ���־�� �Ѵ�.
             */
            IDL_MEM_BARRIER;
        }

        /* mReadIOMutex ��Ҵ� Ǯ������, ���� BCB�� ���� read I/O�� �ƴ���
         * �����Ҽ� �����Ƿ� ������ �д´�. �ֳĸ� fix�� �߱⶧���̴�. */
        sSuccess = ID_TRUE;
    }
    else
    {
        if ( aWaitMode == SDB_WAIT_NORMAL )
        {
            if ( aLatchMode == SDB_S_LATCH )
            {
                aBCB->lockPageSLatch( aStatistics );
            }
            else
            {
                aBCB->lockPageXLatch( aStatistics );
            }
            sSuccess = ID_TRUE;
        }
        else
        {
            /*no wait.. */
            if ( aLatchMode == SDB_S_LATCH )
            {
                aBCB->tryLockPageSLatch( &sSuccess );
            }
            else
            {
                IDE_DASSERT( aLatchMode == SDB_X_LATCH );
                aBCB->tryLockPageXLatch( &sSuccess );
            }
        }
    }

    /* BUG-28423 - [SM] sdbBufferPool::latchPage���� ASSERT�� ������
     * �����մϴ�. */
    IDE_ASSERT ( ( sSuccess == ID_FALSE ) || ( aBCB->mReadyToRead == ID_TRUE ) );

    if ( aSuccess != NULL )
    {
        *aSuccess = sSuccess;
    }
}

#endif  //_O_SDB_BUFFER_POOL_H_
