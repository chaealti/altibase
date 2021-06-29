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
 * $Id: sdbBufferMgr.h 83116 2018-05-29 05:52:23Z jiwon.kim $
 **********************************************************************/

#ifndef _O_SDB_BUFFER_MGR_H_
#define _O_SDB_BUFFER_MGR_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdbDef.h>
#include <sdbBCB.h>
#include <iduLatch.h>

#include <sdbBufferPool.h>
#include <sdbFlushMgr.h>
#include <sdbBufferArea.h>

// pinning�� ���� ���̴� �ڷᱸ��
typedef struct sdbPageID
{
    /* space ID */
    scSpaceID mSpaceID;
    /* page ID */
    scPageID  mPageID;
}  sdbPageID;

typedef struct sdbPinningEnv
{
    // �ڽ��� ������ pageID�� ���� ����� ������ �ִ�.
    sdbPageID  *mAccessHistory;
    // mAccessHisty�� ����� �ִ� pageID����
    UInt        mAccessHistoryCount;
    // ���� ������ pageID�� ����� ��ġ
    UInt        mAccessInsPos;

    // �ڽ��� pinning�ϰ� �ִ� pageID�� ���� ������ ������.
    sdbPageID  *mPinningList;
    // �ڽ��� pinning�ϰ� �ִ� BCB�����͸� ������ �ִ�.
    sdbBCB    **mPinningBCBList;
    // �ִ��� pinning�� �� �ִ� BCB����
    UInt        mPinningCount;
    // ���� BCB�� pinning�� ��ġ
    UInt        mPinningInsPos;
} sdbPinningEnv;

// sdbFiltFunc�� aFiltAgr�� ���̴� �ڷᱸ��
typedef struct sdbBCBRange
{
    // mSpaceID�� ���ϸ鼭
    // mStartPID ���� ũ�ų� ����,
    // mEndPID���� �۰ų� ���� ��� pid�� ������ ���ϵ����Ѵ�.
    scSpaceID   mSpaceID;
    scPageID    mStartPID;
    scPageID    mEndPID;
}sdbBCBRange;

typedef struct sdbDiscardPageObj
{
    /* discard ���� �˻� �Լ� */
    sdbFiltFunc              mFilter;
    /* ������� */
    idvSQL                  *mStatistics;
    /* ������ ����Ǯ */
    sdbBufferPool           *mBufferPool;
    /* mFilter���࿡ �ʿ��� ȯ�� */
    void                    *mEnv;
    /* BCB�� ��Ƶδ� ť */
    smuQueueMgr              mQueue;
}sdbDiscardPageObj;


typedef struct sdbPageOutObj
{
    /* page out ���� �˻� �Լ� */
    sdbFiltFunc              mFilter;
    /* mFilter���࿡ �ʿ��� ȯ�� */
    void                    *mEnv;
    /* flush�� �ؾ��ϴ� BCB���� ��Ƶ� ť */
    smuQueueMgr              mQueueForFlush;
    /* make free�� �ؾ��ϴ� BCB���� ��Ƶ� ť */
    smuQueueMgr              mQueueForMakeFree;
}sdbPageOutObj;

typedef struct sdbFlushObj
{
    /* flush ���� �˻� �Լ� */
    sdbFiltFunc              mFilter;
    /* mFilter���࿡ �ʿ��� ȯ�� */
    void                    *mEnv;
    /* BCB�� ��Ƶδ� ť */
    smuQueueMgr              mQueue;
}sdbFlushObj;

/* BUG-21793: [es50-x32] natc���� clean�ϰ� server start�ϴ� ��� ���� ����մϴ�.
 *
 * inline ���� getPage, fixPage�Լ����� ó���ϸ� compiler ������ ������ �������մϴ�.
 * �Ͽ� �� �Լ����� ��� ���� �Լ��� ó���߽��ϴ�.
 *
 * ����: getPage, fixPage�Լ����� inlineó������ ������. linux 32bit release���� ������
 * �����մϴ�.
 * */
class sdbBufferMgr
{
public:
    static IDE_RC initialize();

    static IDE_RC destroy();

    static IDE_RC resize( idvSQL *aStatistics,
                          ULong   aAskedSize,
                          void   *aTrans,
                          ULong  *aNewSize);

    static IDE_RC expand( idvSQL *aStatistics,
                          ULong   aAddedSize,
                          void   *aTrans,
                          ULong  *aExpandedSize);

    static void addBCB(idvSQL    *aStatistics,
                       sdbBCB    *aFirstBCB,
                       sdbBCB    *aLastBCB,
                       UInt       aLength);

    static IDE_RC createPage( idvSQL          *aStatistics,
                              scSpaceID        aSpaceID,
                              scPageID         aPageID,
                              UInt             aPageType,
                              void*            aMtx,
                              UChar**          aPagePtr);

#if 0 //not used.
//    static IDE_RC getPageWithMRU( idvSQL             *aStatistics,
//                                  scSpaceID           aSpaceID,
//                                  scPageID            aPageID,
//                                  sdbLatchMode        aLatchMode,
//                                  sdbWaitMode         aWaitMode,
//                                  void               *aMtx,
//                                  UChar             **aRetPage,
//                                  idBool             *aTrySuccess = NULL);
#endif


    static IDE_RC getPageByPID( idvSQL               * aStatistics,
                                scSpaceID              aSpaceID,
                                scPageID               aPageID,
                                sdbLatchMode           aLatchMode,
                                sdbWaitMode            aWaitMode,
                                sdbPageReadMode        aReadMode,
                                void                 * aMtx,
                                UChar               ** aRetPage,
                                idBool               * aTrySuccess,
                                idBool               * aIsCorruptPage );

    static IDE_RC getPage( idvSQL             *aStatistics,
                           scSpaceID           aSpaceID,
                           scPageID            aPageID,
                           sdbLatchMode        aLatchMode,
                           sdbWaitMode         aWaitMode,
                           sdbPageReadMode     aReadMode,
                           UChar             **aRetPage,
                           idBool             *aTrySuccess = NULL);

    static IDE_RC getPageByRID( idvSQL             *aStatistics,
                                scSpaceID           aSpaceID,
                                sdRID               aPageRID,
                                sdbLatchMode        aLatchMode,
                                sdbWaitMode         aWaitMode,
                                void               *aMtx,
                                UChar             **aRetPage,
                                idBool             *aTrySuccess = NULL);

    static IDE_RC getPage( idvSQL               * aStatistics,
                           scSpaceID              aSpaceID,
                           scPageID               aPageID,
                           sdbLatchMode           aLatchMode,
                           sdbWaitMode            aWaitMode,
                           void                 * aMtx,
                           UChar               ** aRetPage,
                           idBool               * aTrySuccess,
                           sdbBufferingPolicy     aBufferingPolicy );

    static IDE_RC getPage(idvSQL               *aStatistics,
                          void                 *aPinningEnv,
                          scSpaceID             aSpaceID,
                          scPageID              aPageID,
                          sdbLatchMode          aLatchMode,
                          sdbWaitMode           aWaitMode,
                          void                 *aMtx,
                          UChar               **aRetPage,
                          idBool               *aTrySuccess );

    static IDE_RC getPageBySID( idvSQL             *aStatistics,
                                scSpaceID           aSpaceID,
                                sdSID               aPageSID,
                                sdbLatchMode        aLatchMode,
                                sdbWaitMode         aWaitMode,
                                void               *aMtx,
                                UChar             **aRetPage,
                                idBool             *aTrySuccess = NULL);

    static IDE_RC getPageByGRID( idvSQL             *aStatistics,
                                 scGRID              aGRID,
                                 sdbLatchMode        aLatchMode,
                                 sdbWaitMode         aWaitMode,
                                 void               *aMtx,
                                 UChar             **aRetPage,
                                 idBool             *aTrySuccess = NULL);

    static IDE_RC getPageBySID( idvSQL             *aStatistics,
                                scSpaceID           aSpaceID,
                                sdSID               aPageSID,
                                sdbLatchMode        aLatchMode,
                                sdbWaitMode         aWaitMode,
                                void               *aMtx,
                                UChar             **aRetPage,
                                idBool             *aTrySuccess,
                                idBool             *aSkipFetch /* BUG-43844 */ );

    static IDE_RC getPageByGRID( idvSQL             *aStatistics,
                                 scGRID              aGRID,
                                 sdbLatchMode        aLatchMode,
                                 sdbWaitMode         aWaitMode,
                                 void               *aMtx,
                                 UChar             **aRetPage,
                                 idBool             *aTrySuccess,
                                 idBool             *aSkipFetch /* BUG-43844 */ );

    static IDE_RC setDirtyPage( void *aBCB,
                                void *aMtx );

    static void setDirtyPageToBCB( idvSQL * aStatistics,
                                   UChar  * aPagePtr );
    
    static IDE_RC releasePage( void            *aBCB,
                               UInt             aLatchMode,
                               void            *aMtx );

    static IDE_RC releasePage( idvSQL * aStatistics,
                               UChar  * aPagePtr );

    static IDE_RC releasePageForRollback( void * aBCB,
                                          UInt   aLatchMode,
                                          void * aMtx );

    static IDE_RC fixPageByPID( idvSQL            * aStatistics,
                                scSpaceID           aSpaceID,
                                scPageID            aPageID,
                                UChar            ** aRetPage,
                                idBool            * aTrySuccess );
#if 0
    static IDE_RC fixPageByGRID( idvSQL            * aStatistics,
                                 scGRID              aGRID,
                                 UChar            ** aRetPage,
                                 idBool            * aTrySuccess = NULL );
#endif
    static IDE_RC fixPageByRID( idvSQL            * aStatistics,
                                scSpaceID           aSpaceID,
                                sdRID               aPageRID,
                                UChar            ** aRetPage,
                                idBool            * aTrySuccess = NULL );

    static IDE_RC fixPageByPID( idvSQL              *aStatistics,
                                scSpaceID            aSpaceID,
                                scPageID             aPageID,
                                UChar              **aRetPage );

    static inline IDE_RC fixPageWithoutIO( idvSQL             *aStatistics,
                                           scSpaceID           aSpaceID,
                                           scPageID            aPageID,
                                           UChar             **aRetPage );

    static inline IDE_RC unfixPage( idvSQL     * aStatistics,
                                    UChar      * aPagePtr);

    static inline void unfixPage( idvSQL     * aStatistics,
                                  sdbBCB     * aBCB);

    static inline sdbBufferPool *getPool() { return &mBufferPool; };
    static inline sdbBufferArea *getArea() { return &mBufferArea; };

    static IDE_RC removePageInBuffer( idvSQL     *aStatistics,
                                      scSpaceID   aSpaceID,
                                      scPageID    aPageID );

    static inline void latchPage( idvSQL        *aStatistics,
                                  UChar         *aPagePtr,
                                  sdbLatchMode   aLatchMode,
                                  sdbWaitMode    aWaitMode,
                                  idBool        *aTrySuccess );

    static inline void unlatchPage ( UChar *aPage );
#ifdef DEBUG
    static inline idBool isPageExist( idvSQL       *aStatistics,
                                      scSpaceID     aSpaceID,
                                      scPageID      aPageID );
#endif
    static void setDiskAgerStatResetFlag(idBool  aValue);

    static inline UInt   getPageCount();

    static void getMinRecoveryLSN( idvSQL   *aStatistics,
                                   smLSN    *aLSN );

    static sdbBCB* getBCBFromPagePtr( UChar * aPage);

#if 0 // not used
    static IDE_RC getPagePtrFromGRID( idvSQL     *aStatistics,
                                      scGRID      aGRID,
                                      UChar     **aPagePtr);
#endif
    static void applyStatisticsForSystem();

    static inline sdbBufferPoolStat *getBufferPoolStat();

    /////////////////////////////////////////////////////////////////////////////

#if 0 //not used
//    static IDE_RC createPinningEnv(void **aEnv);

//    static IDE_RC destroyPinningEnv(idvSQL *aStatistics, void *aEnv);
#endif

    static void discardWaitModeFromQueue( idvSQL      *aStatistics,
                                          sdbFiltFunc  aFilter,
                                          void        *aFiltAgr,
                                          smuQueueMgr *aQueue );

    static IDE_RC discardPages( idvSQL        *aStatistics,
                                sdbFiltFunc    aFilter,
                                void          *aFiltAgr);

    static IDE_RC discardPagesInRange( idvSQL    * aStatistics,
                                       scSpaceID   aSpaceID,
                                       scPageID    aStartPID,
                                       scPageID    aEndPID );

    static IDE_RC discardAllPages( idvSQL *aStatistics);

    static IDE_RC flushPages( idvSQL      *aStatistics,
                              sdbFiltFunc  aFilter,
                              void        *aFiltAgr );

    static IDE_RC flushPagesInRange( idvSQL   *aStatistics,
                                     scSpaceID aSpaceID,
                                     scPageID  aStartPID,
                                     scPageID  aEndPID );


    static IDE_RC pageOutAll( idvSQL *aStatistics );

    static IDE_RC pageOut(  idvSQL            *aStatistics,
                            sdbFiltFunc        aFilter,
                            void              *aFiltAgr);

    static IDE_RC pageOutTBS( idvSQL          *aStatkstics,
                              scSpaceID        aSpaceID);

    static IDE_RC pageOutInRange( idvSQL    *aStatistics,
                                  scSpaceID  aSpaceID,
                                  scPageID   aStartPID,
                                  scPageID   aEndPID);

    static IDE_RC discardNoWaitModeFunc( sdbBCB    *aBCB,
                                         void      *aObj);

    static IDE_RC makePageOutTargetQueueFunc( sdbBCB *aBCB,
                                              void   *aObj);

    static IDE_RC makeFlushTargetQueueFunc( sdbBCB *aBCB,
                                            void   *aObj);

    static IDE_RC flushDirtyPagesInCPList(idvSQL *aStatistics, idBool aFlushAll);

    static IDE_RC flushDirtyPagesInCPListByCheckpoint(idvSQL *aStatistics, idBool aFlushAll);

    static inline void setPageLSN( void     *aBCB,
                                   void     *aMtx );

    static inline void lockBufferMgrMutex(idvSQL  *aStatistics);
    static inline void unlockBufferMgrMutex();

    static inline UInt getPageFixCnt( UChar * aPage);

    static void   setPageTypeToBCB( UChar*  aPagePtr, UInt aPageType );

    static void   tryEscalateLatchPage( idvSQL            *aStatistics,
                                        UChar             *aPagePtr,
                                        sdbPageReadMode    aReadMode,
                                        idBool            *aTrySuccess );

    static inline sdbBufferPool* getBufferPool();
    
    static inline void setSBufferUnserviceable( void );

    static inline void setSBufferServiceable( void );

    static IDE_RC gatherStatFromEachBCB();

    // BUG-45598
    static inline idBool isPageReadError( UChar *aPagePtr );

private:
    // buffer pinning�� ���� �Լ��� /////////////////////////////////////////////
    static sdbBCB* findPinnedPage(sdbPinningEnv *aEnv,
                                  scSpaceID      aSpaceID,
                                  scPageID       aPageID);

    static void addToHistory(sdbPinningEnv *aEnv,
                             sdbBCB        *aBCB,
                             idBool        *aBCBPinned,
                             sdbBCB       **aBCBToUnpin);
    /////////////////////////////////////////////////////////////////////////////

    /* [BUG-20861] ���� hash resize�� �ϱ� ���ؼ� �ٸ� Ʈ����ǵ餷�� ��� ����
       ���� ���ϰ� �ؾ� �մϴ�. */
    static void blockAllApprochBufHash( void     *aTrans,
                                        idBool   *aSuccess );

    static void unblockAllApprochBufHash();


    static idBool filterAllBCBs( void   *aBCB,
                                 void   */*aObj*/);

    static idBool filterTBSBCBs( void   *aBCB,
                                 void   *aObj);

    static idBool filterBCBRange( void   *aBCB,
                                  void   *aObj);

    static idBool filterTheBCB( void   *aBCB,
                                void   *aObj);
    

private:
    /* ���� �޸𸮸� �����ϴ� ��� */
    static sdbBufferArea mBufferArea;
    /* BCB ���� �� ���� ��ü ��å ���  */
    static sdbBufferPool mBufferPool;
    /* ����ڰ� alter system ���� ���� ���ü� ���� */
    static iduMutex      mBufferMgrMutex;
    /* ���� �Ŵ����� �����ϴ�  PAGE���� (BCB����) */
    static UInt          mPageCount;
};


inline IDE_RC sdbBufferMgr::fixPageWithoutIO( idvSQL             * aStatistics,
                                              scSpaceID            aSpaceID,
                                              scPageID             aPageID,
                                              UChar             ** aRetPage )
{
    IDE_RC rc;

    IDV_SQL_OPTIME_BEGIN( aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE );

    rc = mBufferPool.fixPageWithoutIO( aStatistics,
                                       aSpaceID,
                                       aPageID,
                                       aRetPage );

    IDV_SQL_OPTIME_END( aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE );

    return rc;
}


/****************************************************************
 * Description :
 *  ���� sdbBufferPool�� ���� �Լ��� ȣ���ϴ� ������ ��Ȱ�� �Ѵ�.
 *  �ڼ��� ������ sdbBufferPool�� ���� �Լ��� ����
 ****************************************************************/
inline IDE_RC sdbBufferMgr::unfixPage( idvSQL     * aStatistics,
                                       UChar      * aPagePtr)
{
    mBufferPool.unfixPage( aStatistics, aPagePtr );

    return IDE_SUCCESS;
}

inline void sdbBufferMgr::unfixPage( idvSQL     * aStatistics,
                                     sdbBCB*      aBCB)
{
    mBufferPool.unfixPage( aStatistics, aBCB );
}

#ifdef DEBUG
inline idBool sdbBufferMgr::isPageExist( idvSQL       * aStatistics,
                                         scSpaceID      aSpaceID,
                                         scPageID       aPageID )
{
    
    return mBufferPool.isPageExist( aStatistics, 
                                    aSpaceID, 
                                    aPageID );
}
#endif

inline UInt sdbBufferMgr::getPageCount()
{
    return mPageCount;
}

inline sdbBufferPoolStat *sdbBufferMgr::getBufferPoolStat()
{
    return mBufferPool.getBufferPoolStat();
}

/****************************************************************
 * Description :
 *  �� �Լ��� �ַ� ����ڰ� smuProperty�� ���� sdbBufferManager����
 *  ���� ���� �ٲ� �� ����Ѵ�. ��, ����ڵ� ���� ���ü� ��� ���� ����ϰ�,
 *  ���ο����� ������� �ʴ´�.
 ****************************************************************/
inline void sdbBufferMgr::lockBufferMgrMutex( idvSQL  *aStatistics )
{
    IDE_ASSERT( mBufferMgrMutex.lock(aStatistics)
                == IDE_SUCCESS );
}

inline void sdbBufferMgr::unlockBufferMgrMutex()
{
    IDE_ASSERT( mBufferMgrMutex.unlock()
                == IDE_SUCCESS );
}

inline UInt sdbBufferMgr::getPageFixCnt( UChar * aPage)
{
    sdbBCB * sBCB = getBCBFromPagePtr( aPage );
    return sBCB->mFixCnt;
}

inline void sdbBufferMgr::setPageLSN( void     *aBCB,
                                      void     *aMtx )
{
    mBufferPool.setPageLSN( (sdbBCB*)aBCB, aMtx );
}

inline void sdbBufferMgr::latchPage( idvSQL        *aStatistics,
                                     UChar         *aPagePtr,
                                     sdbLatchMode   aLatchMode,
                                     sdbWaitMode    aWaitMode,
                                     idBool        *aTrySuccess )
{
    sdbBCB *sBCB = getBCBFromPagePtr( aPagePtr );

    mBufferPool.latchPage( aStatistics,
                           sBCB,
                           aLatchMode,
                           aWaitMode,
                           aTrySuccess );

}


inline void sdbBufferMgr::tryEscalateLatchPage( idvSQL            *aStatistics,
                                                UChar             *aPagePtr,
                                                sdbPageReadMode    aReadMode,
                                                idBool            *aTrySuccess )
{
    sdbBCB *sBCB = getBCBFromPagePtr( aPagePtr );

    mBufferPool.tryEscalateLatchPage( aStatistics,
                                      sBCB,
                                      aReadMode,
                                      aTrySuccess );
}

inline void sdbBufferMgr::unlatchPage( UChar *aPagePtr )
{
    sdbBCB *sBCB = getBCBFromPagePtr( aPagePtr );

    mBufferPool.unlatchPage( sBCB );
}

inline sdbBufferPool* sdbBufferMgr::getBufferPool()
{
    return &mBufferPool;
}

// BUG-45598
inline idBool sdbBufferMgr::isPageReadError( UChar *aPagePtr )
{
    sdbBCB *sBCB = getBCBFromPagePtr( aPagePtr );

    return sBCB->mPageReadError;
}

/* PROJ-2102 Secondary Buffer */
/****************************************************************
 * Description : Secondary Buffer�� ��� ���� 
 ****************************************************************/
inline void sdbBufferMgr::setSBufferUnserviceable( void )
{
     mBufferPool.setSBufferServiceState( ID_FALSE );
}

/****************************************************************
 * Description : Secondary Buffer�� ��� ����
 ****************************************************************/
inline void sdbBufferMgr::setSBufferServiceable( void )
{
     mBufferPool.setSBufferServiceState( ID_TRUE );
}


#endif //_O_SDB_BUFFER_MGR_H_
